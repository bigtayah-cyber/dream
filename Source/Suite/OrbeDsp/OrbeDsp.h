// Briques DSP partagées des effets Orbe (Denoiser, EQ, DeEsser). C++ pur, sans JUCE, header-only.
#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <complex>
#include <cstring>
#include <vector>
#include "../../OneKnob/OneKnob.h"

namespace odsp {

constexpr double kPi = 3.14159265358979323846;
inline float db2lin(float d) { return std::pow(10.f, d / 20.f); }
inline float lin2db(float x) { return 20.f * std::log10(x + 1e-12f); }

// FFT complexe radix-2 en place (sans mise à l'échelle).
struct Fft {
    int n = 0;
    std::vector<float> cs, sn;
    std::vector<int> rev;
    void init(int N) {
        n = N; cs.resize((size_t)N / 2); sn.resize((size_t)N / 2); rev.assign((size_t)N, 0);
        int lg = 0; while ((1 << lg) < N) ++lg;
        for (int i = 0; i < N; ++i) { int r = 0; for (int b = 0; b < lg; ++b) if (i >> b & 1) r |= 1 << (lg - 1 - b); rev[(size_t)i] = r; }
        for (int k = 0; k < N / 2; ++k) { cs[(size_t)k] = (float)std::cos(2 * kPi * k / N); sn[(size_t)k] = (float)std::sin(2 * kPi * k / N); }
    }
    void run(float* re, float* im, bool inverse) const {
        for (int i = 0; i < n; ++i) { const int j = rev[(size_t)i]; if (j > i) { std::swap(re[i], re[j]); std::swap(im[i], im[j]); } }
        for (int len = 2; len <= n; len <<= 1) {
            const int half = len / 2, step = n / len;
            for (int i = 0; i < n; i += len)
                for (int j = 0; j < half; ++j) {
                    const float wr = cs[(size_t)(j * step)], wi = inverse ? sn[(size_t)(j * step)] : -sn[(size_t)(j * step)];
                    const int a = i + j, b = a + half;
                    const float tr = re[b] * wr - im[b] * wi, ti = re[b] * wi + im[b] * wr;
                    re[b] = re[a] - tr; im[b] = im[a] - ti; re[a] += tr; im[a] += ti;
                }
        }
    }
};

// Biquad RBJ en double : coefficients + réponse théorique + filtre TDF-II.
enum class Kind { Peak, LowShelf, HighShelf, LowCut, HighCut, Notch, BandPass };
struct Coef {
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double mag(double f, double sr) const {
        const double w = 2 * kPi * f / sr;
        const std::complex<double> z1(std::cos(w), -std::sin(w)), z2(std::cos(2 * w), -std::sin(2 * w));
        return std::abs((b0 + b1 * z1 + b2 * z2) / (1.0 + a1 * z1 + a2 * z2));
    }
};
inline Coef design(Kind k, double sr, double f, double gDb, double q) {
    f = std::clamp(f, 5.0, sr * 0.49); q = std::max(q, 0.05);
    const double w = 2 * kPi * f / sr, c = std::cos(w), s = std::sin(w), al = s / (2 * q), A = std::pow(10.0, gDb / 40.0), bt = 2 * std::sqrt(A) * al;
    double b0, b1, b2, a0, a1, a2;
    switch (k) {
        case Kind::Peak:      b0 = 1 + al * A; b1 = -2 * c; b2 = 1 - al * A; a0 = 1 + al / A; a1 = -2 * c; a2 = 1 - al / A; break;
        case Kind::LowShelf:  b0 = A * ((A + 1) - (A - 1) * c + bt); b1 = 2 * A * ((A - 1) - (A + 1) * c); b2 = A * ((A + 1) - (A - 1) * c - bt);
                              a0 = (A + 1) + (A - 1) * c + bt; a1 = -2 * ((A - 1) + (A + 1) * c); a2 = (A + 1) + (A - 1) * c - bt; break;
        case Kind::HighShelf: b0 = A * ((A + 1) + (A - 1) * c + bt); b1 = -2 * A * ((A - 1) + (A + 1) * c); b2 = A * ((A + 1) + (A - 1) * c - bt);
                              a0 = (A + 1) - (A - 1) * c + bt; a1 = 2 * ((A - 1) - (A + 1) * c); a2 = (A + 1) - (A - 1) * c - bt; break;
        case Kind::LowCut:    b0 = (1 + c) / 2; b1 = -(1 + c); b2 = b0; a0 = 1 + al; a1 = -2 * c; a2 = 1 - al; break;
        case Kind::HighCut:   b0 = (1 - c) / 2; b1 = 1 - c; b2 = b0; a0 = 1 + al; a1 = -2 * c; a2 = 1 - al; break;
        case Kind::Notch:     b0 = 1; b1 = -2 * c; b2 = 1; a0 = 1 + al; a1 = -2 * c; a2 = 1 - al; break;
        default:              b0 = al; b1 = 0; b2 = -al; a0 = 1 + al; a1 = -2 * c; a2 = 1 - al; break;
    }
    return {b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0};
}
struct Bq {
    Coef c; double z1 = 0, z2 = 0;
    void reset() { z1 = z2 = 0; }
    float process(float x) { const double y = c.b0 * x + z1; z1 = c.b1 * x - c.a1 * y + z2; z2 = c.b2 * x - c.a2 * y; return (float)y; }
};

// Retard entier exact (contournement à latence compensée).
struct IntDelay {
    std::vector<float> b; int w = 0, d = 0;
    void init(int delay) { d = delay; b.assign((size_t)d + 1, 0.f); w = 0; }
    float tick(float x) {
        if (d == 0) return x;
        b[(size_t)w] = x; if (++w > d) w = 0;
        return b[(size_t)w];
    }
};
struct BypassDelay {
    IntDelay l, r;
    void init(int latency) { l.init(latency); r.init(latency); }
    void process(float* a, float* b, int n, bool stereoIn) {
        for (int i = 0; i < n; ++i) { const float x0 = a[i], x1 = stereoIn ? b[i] : x0; a[i] = l.tick(x0); b[i] = r.tick(x1); }
    }
};

// Analyseur de spectre : le fil audio pousse des échantillons, l'interface calcule la FFT (Hann 2048, lissage de chute).
struct Analyzer {
    static constexpr int N = 2048, kBins = N / 2 + 1, kRing = 8192;
    std::vector<float> ring = std::vector<float>(kRing, 0.f);
    std::atomic<int> wpos{0};
    std::array<float, kBins> db;
    Fft fft; std::vector<float> win, re, im;
    Analyzer() { db.fill(-120.f); fft.init(N); win.resize(N); re.resize(N); im.resize(N); for (int i = 0; i < N; ++i) win[(size_t)i] = 0.5f - 0.5f * std::cos(2 * (float)kPi * (float)i / (float)N); }
    void push(const float* x, int n) {
        int w = wpos.load(std::memory_order_relaxed);
        for (int i = 0; i < n; ++i) ring[(size_t)((w + i) & (kRing - 1))] = x[i];
        wpos.store(w + n, std::memory_order_release);
    }
    // fall : chute max en dB par appel
    void compute(float fall = 2.5f) {
        const int w = wpos.load(std::memory_order_acquire);
        for (int i = 0; i < N; ++i) { re[(size_t)i] = ring[(size_t)((w - N + i) & (kRing - 1))] * win[(size_t)i]; im[(size_t)i] = 0.f; }
        fft.run(re.data(), im.data(), false);
        for (int k = 0; k < kBins; ++k) {
            const float m = std::sqrt(re[(size_t)k] * re[(size_t)k] + im[(size_t)k] * im[(size_t)k]) * 4.f / (float)N;  // sinus pleine échelle = 0 dBFS
            const float d = lin2db(m);
            db[(size_t)k] = d > db[(size_t)k] ? d : std::max(d, db[(size_t)k] - fall);
        }
    }
};

}  // namespace odsp
