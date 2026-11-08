// Orbe Sampler : moteur DSP partagé par Orbe Pads et Orbe Sample. C++17 pur, sans JUCE (testable hors-ligne).
// Analyse (FFT, transitoires, tempo, tonalité), rééchantillonnage, WSOLA, échantillon de démonstration.
// Algorithmes portés à l'identique des prototypes web (pads.html / sample.html).
#pragma once
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include "../../OneKnob/OneKnob.h"  // Biquad ; inclut Engine.h (Reverb, PingPong)

namespace osamp {

constexpr double kPi = 3.14159265358979323846;
using Chs = std::vector<std::vector<float>>;

// Échantillon stéréo en mémoire (un son mono est dupliqué).
struct Sample {
    std::vector<float> ch[2];
    double sr = 44100.0;
    int size() const { return (int)ch[0].size(); }
    double dur() const { return size() / sr; }
};

inline void fft(double* re, double* im, int n) {
    for (int i = 1, j = 0; i < n; ++i) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) { std::swap(re[i], re[j]); std::swap(im[i], im[j]); }
    }
    for (int len = 2; len <= n; len <<= 1) {
        const double ang = -2 * kPi / len, wr = std::cos(ang), wi = std::sin(ang);
        for (int i = 0; i < n; i += len) {
            double cr = 1, ci = 0;
            for (int j = 0; j < len / 2; ++j) {
                const int a = i + j, b = a + len / 2;
                const double tr = re[b] * cr - im[b] * ci, ti = re[b] * ci + im[b] * cr;
                re[b] = re[a] - tr; im[b] = im[a] - ti; re[a] += tr; im[a] += ti;
                const double nr = cr * wr - ci * wi; ci = cr * wi + ci * wr; cr = nr;
            }
        }
    }
}
inline std::vector<double> hann(int n) {
    std::vector<double> w((size_t)n);
    for (int i = 0; i < n; ++i) w[(size_t)i] = 0.5 - 0.5 * std::cos(2 * kPi * i / (n - 1));
    return w;
}
inline std::vector<float> mono(const Sample& s, double from, double to) {
    const int a = std::max(0, (int)std::floor(from * s.sr)), b = std::min(s.size(), (int)std::floor(to * s.sr)), n = std::max(0, b - a);
    std::vector<float> o((size_t)n);
    for (int i = 0; i < n; ++i) o[(size_t)i] = 0.5f * (s.ch[0][(size_t)(a + i)] + s.ch[1][(size_t)(a + i)]);
    return o;
}
inline std::vector<float> mono(const Sample& s) { return mono(s, 0, s.dur()); }

// ---------------- tempo : flux spectral + autocorrélation + prior log-gaussien centré sur 118 BPM ----------------
struct Tempo { double bpm = 0, conf = 0, phase = 0; };

inline Tempo tempo(const Sample& s) {
    const double sr = s.sr, dur = s.dur(), maxS = std::min(dur, 120.0), st = std::max(0.0, (dur - maxS) / 2);
    const auto x = mono(s, st, st + maxS);
    const int N = 1024, hop = 512;
    const auto win = hann(N);
    const int frames = std::max(0, ((int)x.size() - N) / hop);
    if (frames < 32) return {};
    std::vector<float> env((size_t)frames);
    std::vector<double> re((size_t)N), im((size_t)N);
    const int maxBin = std::min(N / 2, (int)std::lround(8000.0 / (sr / N)));
    std::vector<double> prev((size_t)maxBin, 0.0), cur((size_t)maxBin, 0.0);
    for (int f = 0; f < frames; ++f) {
        const int o = f * hop;
        for (int i = 0; i < N; ++i) { re[(size_t)i] = x[(size_t)(o + i)] * win[(size_t)i]; im[(size_t)i] = 0; }
        fft(re.data(), im.data(), N);
        double fl = 0;
        for (int k = 1; k < maxBin; ++k) {
            const double m = std::log1p(100 * std::hypot(re[(size_t)k], im[(size_t)k])), d = m - prev[(size_t)k];
            if (d > 0) fl += d;
            cur[(size_t)k] = m;
        }
        env[(size_t)f] = (float)fl;
        std::swap(prev, cur);
    }
    const double fps = sr / hop;
    const int w = (int)std::lround(fps * 0.4);
    std::vector<float> od((size_t)frames);
    double acc = 0;
    for (int i = 0; i < frames; ++i) {
        acc += env[(size_t)i];
        if (i >= w) acc -= env[(size_t)(i - w)];
        const double mean = acc / std::min(i + 1, w);
        od[(size_t)i] = (float)std::max(0.0, env[(size_t)i] - mean);
    }
    auto lagOf = [&](double bpm) { return 60 * fps / bpm; };
    const int lmin = (int)std::floor(lagOf(210)), lmax = (int)std::ceil(lagOf(55));
    std::vector<double> ac((size_t)lmax * 2 + 3, 0.0);
    for (int L = lmin; L <= std::min(lmax * 2, frames - 1); ++L) {
        double sum = 0;
        for (int i = 0; i + L < frames; ++i) sum += (double)od[(size_t)i] * od[(size_t)(i + L)];
        ac[(size_t)L] = sum / (frames - L);
    }
    auto A = [&](int i) { return (i >= 0 && i < (int)ac.size()) ? ac[(size_t)i] : 0.0; };
    auto prior = [](double bb) { const double q = std::log2(bb / 118) / 0.85; return std::exp(-0.5 * q * q); };
    double best = 0, sum = 0, sum2 = 0, cnt = 0;
    int bestL = lmin;
    for (int L = lmin; L <= lmax; ++L) {
        const double bpm = 60 * fps / L, sc = (A(L) + 0.5 * A(2 * L) + 0.25 * A((int)std::lround(L / 2.0))) * prior(bpm);
        sum += sc; sum2 += sc * sc; cnt += 1;
        if (sc > best) { best = sc; bestL = L; }
    }
    const double y0 = A(bestL - 1), y1 = A(bestL), y2 = A(bestL + 1), den = y0 - 2 * y1 + y2, dl = den != 0 ? 0.5 * (y0 - y2) / den : 0;
    double bpm = 60 * fps / (bestL + std::max(-0.5, std::min(0.5, dl)));
    const double mean = sum / cnt, sd = std::sqrt(std::max(1e-12, sum2 / cnt - mean * mean));
    const double conf = std::max(0.0, std::min(1.0, ((best - mean) / sd - 1) / 4));
    auto O = [&](int i) { return (i >= 0 && i < frames) ? (double)od[(size_t)i] : 0.0; };
    struct Comb { double s, p; };
    auto comb = [&](double bb) {
        const double P = 60 * fps / bb;
        Comb r{-1, 0};
        for (double p = 0; p < P; p += 0.5) {
            double sc = 0, n = 0;
            for (double k = p; k < frames; k += P) { const int i = (int)std::lround(k); sc += O(i) + 0.5 * (O(i - 1) + O(i + 1)); n += 1; }
            sc /= std::max(1.0, n);
            if (sc > r.s) r = {sc, p};
        }
        return r;
    };
    struct Cand { double b, s; };
    std::vector<Cand> cands;
    for (double v : {bpm, bpm * 2, bpm / 2}) if (v >= 55 && v <= 210) cands.push_back({v, comb(v).s * prior(v)});
    std::stable_sort(cands.begin(), cands.end(), [](const Cand& a, const Cand& b) { return a.s > b.s; });
    if (!cands.empty()) bpm = cands[0].b;
    double fb = bpm, fs = -1;
    for (int i = 0; i <= 60; ++i) { const double v = bpm + (-1.5 + 0.05 * i), c = comb(v).s; if (c > fs) { fs = c; fb = v; } }
    bpm = std::fabs(fb - std::round(fb)) < 0.2 ? std::round(fb) : std::round(fb * 10) / 10;
    return {bpm, conf, st + comb(bpm).p / fps};
}

// ---------------- tonalité : chroma pondéré par les partiels + profils de Krumhansl ----------------
struct Key { int pc = -1; bool minor = false; double conf = 0; };

inline double corr12(const double* a, const double* b) {
    double ma = 0, mb = 0, n = 0, da = 0, db = 0;
    for (int i = 0; i < 12; ++i) { ma += a[i]; mb += b[i]; }
    ma /= 12; mb /= 12;
    for (int i = 0; i < 12; ++i) { n += (a[i] - ma) * (b[i] - mb); da += (a[i] - ma) * (a[i] - ma); db += (b[i] - mb) * (b[i] - mb); }
    return n / std::sqrt(da * db > 0 ? da * db : 1);
}

inline Key detectKey(const Sample& s) {
    static const double KMAJ[12] = {6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88};
    static const double KMIN[12] = {6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17};
    const double LO = 50, HI = 2000, PW = 0.5, sr = s.sr, dur = s.dur(), maxS = std::min(dur, 90.0), st = std::max(0.0, (dur - maxS) / 2);
    const auto x = mono(s, st, st + maxS);
    const int N = 8192, hop = 4096, frames = std::max(1, ((int)x.size() - N) / hop);
    if ((int)x.size() < N) return {};
    const auto win = hann(N);
    std::vector<double> re((size_t)N), im((size_t)N), mag((size_t)N / 2), chroma(12, 0.0);
    const int kLo = (int)std::ceil(LO * N / sr), kHi = std::min(N / 2 - 2, (int)std::floor(HI * N / sr));
    struct Pk { double f, m; };
    for (int f = 0; f < frames; ++f) {
        const int o = f * hop;
        double en = 0;
        for (int i = 0; i < N; ++i) { const double v = (o + i < (int)x.size() ? x[(size_t)(o + i)] : 0.f) * win[(size_t)i]; re[(size_t)i] = v; im[(size_t)i] = 0; en += v * v; }
        if (en < 1e-6) continue;
        fft(re.data(), im.data(), N);
        double mx = 0;
        for (int k = 1; k < N / 2; ++k) { mag[(size_t)k] = std::hypot(re[(size_t)k], im[(size_t)k]); if (k >= kLo && k <= kHi && mag[(size_t)k] > mx) mx = mag[(size_t)k]; }
        std::vector<Pk> pk;
        for (int k = kLo; k <= kHi; ++k)
            if (mag[(size_t)k] > mx * 0.04 && mag[(size_t)k] >= mag[(size_t)k - 1] && mag[(size_t)k] > mag[(size_t)k + 1]) {
                const double a = mag[(size_t)k - 1], b = mag[(size_t)k], c = mag[(size_t)k + 1], dd = 0.5 * (a - c) / ((a - 2 * b + c) != 0 ? (a - 2 * b + c) : 1);
                pk.push_back({(k + dd) * sr / N, b});
            }
        double c[12] = {};
        for (size_t pi = 0; pi < pk.size(); ++pi) {
            const auto& p = pk[pi];
            double w = 1;
            for (int h = 2; h <= 5; ++h) {
                const double f0 = p.f / h;
                if (f0 < LO * 0.7) break;
                bool hit = false;
                for (size_t qi = 0; qi < pk.size(); ++qi) if (qi != pi && std::fabs(pk[qi].f - f0) / f0 < 0.03 && pk[qi].m > p.m * 0.25) { hit = true; break; }
                if (hit) { w = 0.15; break; }
            }
            const double m = 12 * std::log2(p.f / 440) + 69, r = std::round(m);
            if (std::fabs(m - r) > 0.4) continue;
            c[(((int)r % 12) + 12) % 12] += std::pow(p.m, PW) * w * (p.f < 200 ? 1.3 : 1.0);
        }
        double cm = 0;
        for (double v : c) cm = std::max(cm, v);
        if (cm <= 0) cm = 1;
        for (int i = 0; i < 12; ++i) chroma[(size_t)i] += c[i] / cm;
    }
    struct R { int pc; bool minor; double r; };
    std::vector<R> rank;
    for (int t = 0; t < 12; ++t) {
        double rot[12];
        for (int i = 0; i < 12; ++i) rot[i] = chroma[(size_t)((i + t) % 12)];
        rank.push_back({t, false, corr12(rot, KMAJ)});
        rank.push_back({t, true, corr12(rot, KMIN)});
    }
    std::stable_sort(rank.begin(), rank.end(), [](const R& a, const R& b) { return a.r > b.r; });
    return {rank[0].pc, rank[0].minor, std::max(0.0, std::min(1.0, (rank[0].r - rank[1].r) * 6 + (rank[0].r - 0.5)))};
}

inline std::string camelot(int pc, bool minor) {
    return std::to_string(minor ? ((7 * ((pc + 3) % 12) + 7) % 12) + 1 : ((7 * pc + 7) % 12) + 1) + (minor ? "A" : "B");
}
inline const char* noteLetter(int pc) {
    static const char* n[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    return n[((pc % 12) + 12) % 12];
}

// ---------------- transitoires (secondes) ----------------
inline std::vector<double> onsets(const std::vector<float>& x, double sr, float sens, int maxN) {
    const int hop = 256, n = (int)x.size() / hop;
    if (n < 4) return {0.0};
    std::vector<float> e((size_t)n), od((size_t)n, 0.f);
    for (int i = 0; i < n; ++i) { double s = 0; for (int j = i * hop; j < (i + 1) * hop; ++j) s += (double)x[(size_t)j] * x[(size_t)j]; e[(size_t)i] = (float)std::sqrt(s / hop); }
    for (int i = 1; i < n; ++i) od[(size_t)i] = (float)(std::max(0.0, std::log(e[(size_t)i] + 1e-4) - std::log(e[(size_t)i - 1] + 1e-4)) * std::sqrt(e[(size_t)i]));
    double m = 0, s2 = 0, emax = 0;
    for (int i = 0; i < n; ++i) { m += od[(size_t)i]; s2 += (double)od[(size_t)i] * od[(size_t)i]; emax = std::max(emax, (double)e[(size_t)i]); }
    m /= n;
    const double sd = std::sqrt(std::max(0.0, s2 / n - m * m)), thr = m + (2.6 - 2.3 * sens) * sd;
    const int gap = (int)std::lround(0.07 * sr / hop);
    struct P { int i; float v; };
    std::vector<P> pk;
    for (int i = 2; i < n - 1; ++i)
        if (od[(size_t)i] > thr && od[(size_t)i] >= od[(size_t)i - 1] && od[(size_t)i] > od[(size_t)i + 1] && e[(size_t)i] > emax * 0.03) {
            if (!pk.empty() && i - pk.back().i < gap) { if (od[(size_t)i] > pk.back().v) pk.back() = {i, od[(size_t)i]}; }
            else pk.push_back({i, od[(size_t)i]});
        }
    std::stable_sort(pk.begin(), pk.end(), [](const P& a, const P& b) { return a.v > b.v; });
    if ((int)pk.size() > maxN - 1) pk.resize((size_t)maxN - 1);
    std::sort(pk.begin(), pk.end(), [](const P& a, const P& b) { return a.i < b.i; });
    std::vector<double> sel;
    for (auto& p : pk) sel.push_back(std::max(0.0, (p.i - 1) * (double)hop / sr - 0.003));
    if (sel.empty() || sel[0] > 0.03) sel.insert(sel.begin(), 0.0); else sel[0] = 0.0;
    if ((int)sel.size() > maxN) sel.resize((size_t)maxN);
    return sel;
}

// ---------------- rééchantillonnage Hermite : vitesse et hauteur bougent ensemble ----------------
inline float hermite(const float* x, int n, double p) {
    const int i1 = (int)std::floor(p);
    const float t = (float)(p - i1);
    auto g = [&](int i) { return x[std::max(0, std::min(n - 1, i))]; };
    const float x0 = g(i1 - 1), x1 = g(i1), x2 = g(i1 + 1), x3 = g(i1 + 2);
    const float c1 = 0.5f * (x2 - x0), c2 = x0 - 2.5f * x1 + 2 * x2 - 0.5f * x3, c3 = 0.5f * (x3 - x0) + 1.5f * (x1 - x2);
    return ((c3 * t + c2) * t + c1) * t + x1;
}
inline Chs resample(const Chs& in, double r) {
    if (std::fabs(r - 1) < 1e-4 || in.empty() || in[0].empty()) return in;
    const int len = (int)in[0].size(), n = std::max(1, (int)std::floor(len / r));
    Chs out;
    for (auto& x : in) { std::vector<float> y((size_t)n); for (int i = 0; i < n; ++i) y[(size_t)i] = hermite(x.data(), len, i * r); out.push_back(std::move(y)); }
    return out;
}

// ---------------- WSOLA : la durée change, la hauteur reste (N=2048, Hs=512) ----------------
inline Chs stretch(const Chs& chs, double factor, const std::atomic<bool>* cancel = nullptr) {
    if (std::fabs(factor - 1) < 1e-3 || chs.empty() || chs[0].empty()) return chs;
    const int N = 2048, Hs = 512, tol = 256, len = (int)chs[0].size(), outLen = (int)std::ceil(len * factor);
    const double Ha = Hs / factor;
    std::vector<float> win((size_t)N);
    for (int i = 0; i < N; ++i) win[(size_t)i] = (float)(0.5 - 0.5 * std::cos(2 * kPi * i / N));
    std::vector<float> m((size_t)len, 0.f);
    for (auto& c : chs) for (int i = 0; i < len; ++i) m[(size_t)i] += c[(size_t)i] / (float)chs.size();
    Chs o(chs.size(), std::vector<float>((size_t)outLen + N, 0.f));
    std::vector<float> ws((size_t)outLen + N, 0.f);
    int prev = 0, k = 0;
    for (int so = 0; so < outLen; so += Hs, ++k) {
        if (cancel && (k & 63) == 0 && cancel->load()) break;
        const int ideal = (int)std::lround(k * Ha);
        int pos = ideal;
        if (k > 0) {
            const int nat = prev + Hs, lo = std::max(0, ideal - tol), hi = std::min(len - N, ideal + tol);
            float best = -INFINITY;
            for (int p = lo; p <= hi; p += 2) {
                float sc = 0;
                for (int j = 0; j < 1024; j += 4) sc += m[(size_t)(p + j)] * (nat + j < len ? m[(size_t)(nat + j)] : 0.f);
                if (sc > best) { best = sc; pos = p; }
            }
        }
        pos = std::max(0, std::min(pos, std::max(0, len - 1)));
        const int cnt = std::min(N, len - pos);
        for (size_t c = 0; c < chs.size(); ++c) for (int j = 0; j < cnt; ++j) o[c][(size_t)(so + j)] += chs[c][(size_t)(pos + j)] * win[(size_t)j];
        for (int j = 0; j < N; ++j) ws[(size_t)(so + j)] += win[(size_t)j];
        prev = pos;
    }
    Chs r;
    for (auto& y : o) {
        std::vector<float> z((size_t)outLen);
        for (int i = 0; i < outLen; ++i) z[(size_t)i] = ws[(size_t)i] > 0.1f ? y[(size_t)i] / ws[(size_t)i] : y[(size_t)i];
        r.push_back(std::move(z));
    }
    return r;
}

// ---------------- craquements de vinyle (graine fixe 11 : toujours identiques) ----------------
inline void addCrackle(std::vector<float>& L, std::vector<float>& R, float amt, double sr) {
    uint64_t seed = 11;
    auto rnd = [&]() { seed = (seed * 16807ull) % 2147483647ull; return (double)seed / 2147483647.0; };
    for (auto* d : {&L, &R}) {
        double hiss = 0;
        const int n = (int)d->size();
        for (int i = 0; i < n; ++i) {
            hiss = hiss * 0.7 + (rnd() * 2 - 1) * 0.3;
            (*d)[(size_t)i] += (float)(hiss * 0.012 * amt);
            if (rnd() < amt * 6 / sr * 60) {
                const double a = (rnd() * 0.5 + 0.2) * amt;
                const int Ln = 8 + (int)std::floor(rnd() * 40);
                for (int j = 0; j < Ln && i + j < n; ++j) (*d)[(size_t)(i + j)] += (float)(a * (rnd() * 2 - 1) * (1.0 - (double)j / Ln));
            }
        }
    }
}

inline float quantize(float x, int bits) { const float q = std::ldexp(1.f, bits - 1); return std::round(x * q) / q; }

// ---------------- échantillon de démonstration généré en code : boom-bap à 92 BPM, deux mesures ----------------
inline std::shared_ptr<Sample> demoSample(double sr = 44100.0, double bpm = 92.0) {
    auto s = std::make_shared<Sample>();
    s->sr = sr;
    const double beat = 60.0 / bpm;
    const int n = (int)(8 * beat * sr);
    std::vector<float> L((size_t)n, 0.f), R((size_t)n, 0.f);
    uint32_t rs = 12345;
    auto noise = [&]() { rs = rs * 1664525u + 1013904223u; return (float)(rs >> 8) / 8388608.f - 1.f; };
    auto add = [&](double t, int i, float l, float r) { const int k = (int)(t * sr) + i; if (k >= 0 && k < n) { L[(size_t)k] += l; R[(size_t)k] += r; } };
    for (int b = 0; b < 8; ++b) {  // charleston
        float lp = 0;
        for (int half = 0; half < 2; ++half) {
            const double t0 = (b + half * 0.5) * beat; const float vel = half ? 0.12f : 0.2f;
            for (int i = 0; i < (int)(0.05 * sr); ++i) { const float v = noise(); const float hp = v - lp; lp += 0.35f * (v - lp); const float e = vel * hp * std::exp(-(float)i / (0.012f * (float)sr)); add(t0, i, e * 0.9f, e * 1.1f); }
        }
    }
    for (double kb : {0.0, 2.5, 4.0, 5.5, 6.5}) {  // grosse caisse
        double ph = 0;
        for (int i = 0; i < (int)(0.4 * sr); ++i) { const double t = i / sr, f = 45 + 90 * std::exp(-t * 28); ph += 2 * kPi * f / sr; const float e = 0.9f * (float)std::sin(ph) * std::exp(-(float)t * 9.f); add(kb * beat, i, e, e); }
    }
    for (int sb : {1, 3, 5, 7}) {  // caisse claire
        float lp = 0;
        for (int i = 0; i < (int)(0.3 * sr); ++i) { const double t = i / sr; const float v = noise(); lp += 0.5f * (v - lp); const float e = (0.35f * (v - lp) * std::exp(-(float)t * 18.f) + 0.3f * (float)std::sin(2 * kPi * 190 * t) * std::exp(-(float)t * 30.f)); add(sb * beat, i, e, e); }
    }
    struct Chord { double at, len; int root; int n[4]; };
    const Chord ch[] = {{0, 2.4, 45, {57, 60, 64, 67}}, {2.5, 1.5, 45, {57, 60, 64, 67}}, {4, 2.4, 41, {53, 57, 60, 64}}, {6.5, 1.5, 41, {53, 57, 60, 64}}};
    for (auto& c : ch) {
        for (int k = 0; k < 4; ++k) {  // piano électrique à modulation de fréquence
            const double f = 440.0 * std::pow(2.0, (c.n[k] - 69) / 12.0);
            double p1 = 0, p2 = 0;
            for (int i = 0; i < (int)((c.len + 0.6) * beat * sr); ++i) {
                const double t = i / sr; p1 += 2 * kPi * f / sr; p2 += 2 * kPi * f * 14 / sr;
                const float env = std::min(1.f, (float)(t * 400)) * std::exp(-(float)t * 2.2f), tine = std::exp(-(float)t * 9.f);
                const float v = 0.07f * env * (float)std::sin(p1 + 0.9 * std::exp(-t * 3.0) * std::sin(p1) + 0.35 * tine * std::sin(p2));
                add(c.at * beat, i, v * (0.8f + 0.1f * k), v * (1.1f - 0.1f * k));
            }
        }
        double ph = 0; const double f = 440.0 * std::pow(2.0, (c.root - 69) / 12.0);  // basse
        for (int i = 0; i < (int)(c.len * beat * sr); ++i) { const double t = i / sr; ph += 2 * kPi * f / sr; const float e = 0.28f * (float)std::sin(ph) * std::min(1.f, (float)(t * 150)) * std::exp(-(float)t * 1.1f); add(c.at * beat, i, e, e); }
    }
    float pk = 0;
    for (int i = 0; i < n; ++i) pk = std::max(pk, std::max(std::fabs(L[(size_t)i]), std::fabs(R[(size_t)i])));
    const float g = pk > 0 ? 0.85f / pk : 1.f;
    for (int i = 0; i < n; ++i) { L[(size_t)i] *= g; R[(size_t)i] *= g; }
    s->ch[0] = std::move(L); s->ch[1] = std::move(R);
    return s;
}

// Écrête doucement au-delà de 0,9 : le pic reste borné sous 1.
inline float softClip(float x) {
    const float a = std::fabs(x);
    return a < 0.9f ? x : std::copysign(0.9f + 0.1f * std::tanh((a - 0.9f) * 10.f), x);
}

}  // namespace osamp
