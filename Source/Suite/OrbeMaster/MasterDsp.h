// Orbe Master : chaîne de mastering stéréo. C++ pur (sans JUCE).
// Chaîne et valeurs par défaut = section « master » de mix.html (EQF, P0, applyP), avec un VRAI limiteur
// look-ahead à plafond true-peak (interpolation 4×) à la place du compresseur + écrêtage du prototype.
#pragma once
#include <algorithm>
#include <cmath>
#include <complex>
#include <vector>
#include "../../OneKnob/OneKnob.h"

namespace mas {

using onek::Biquad;
constexpr float kPi = 3.14159265358979f;
constexpr int kBands = 7;

struct Band { int type; float f, q; const char* name; const char* tag; };   // 0 shelf grave, 1 cloche, 2 shelf aigu
inline const Band* bands() {
    static const Band b[kBands] = {{0, 45, .7f, "Sub", "45"}, {1, 120, .9f, "Basses", "120"}, {1, 350, .9f, "Bas-m\xC3\xA9" "diums", "350"}, {1, 1000, .8f, "M\xC3\xA9" "diums", "1k"},
                                   {1, 3000, .9f, "Haut-m\xC3\xA9" "diums", "3k"}, {1, 5000, 1.f, "Pr\xC3\xA9sence", "5k"}, {2, 10000, .7f, "Air", "10k"}};
    return b;
}

struct Params {
    bool eqOn = true; float eq[kBands] = {0, 0, 0, 0, 0, 0, 0};
    bool compOn = true; float cAmt = .25f, cRatio = 2.f, cAtt = 30.f, cGain = 0.f;
    bool satOn = false; float drive = .25f, satMix = .3f;
    bool stOn = true, mono = true; float width = 1.f, monoHz = 120.f;
    bool limOn = true; float lGain = 0.f, ceil = -1.f, lRel = 90.f;
};

inline void setBand(Biquad& b, int i, float sr, float db) {
    const auto& d = bands()[i]; const float f = std::min(d.f, 0.45f * sr);
    if (d.type == 0) b.lowShelf(sr, f, db);
    else if (d.type == 2) b.highShelf(sr, f, db);
    else { const float A = std::pow(10.f, db / 40.f), w = 2 * kPi * f / sr, c = std::cos(w), al = std::sin(w) / (2 * d.q); b.set(1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A); }
}
// Réponse en dB de l'égaliseur (pour la courbe de l'interface).
inline float eqResponseDb(const float* gains, float f, float sr = 48000.f) {
    double acc = 0;
    for (int i = 0; i < kBands; ++i) {
        Biquad b; setBand(b, i, sr, gains[i]);
        const double w = 2 * kPi * f / sr; const std::complex<double> z1 = std::polar(1.0, -w), z2 = std::polar(1.0, -2 * w);
        const auto n = (double)b.b0 + (double)b.b1 * z1 + (double)b.b2 * z2, d = 1.0 + (double)b.a1 * z1 + (double)b.a2 * z2;
        acc += 20 * std::log10(std::abs(n / d) + 1e-12);
    }
    return (float)acc;
}

// Sur-échantillonnage 2× par FIR linéaire (33 coefficients, Kaiser) : latence exacte de 16 échantillons.
struct Os2 {
    static constexpr int N = 33, kLatency = 16;
    float h[N] = {}, up[N] = {}, dn[N] = {};
    int ui = 0, di = 0;
    Os2() {
        auto i0 = [](double x) { double s = 1, t = 1; for (int k = 1; k < 30; ++k) { t *= (x / (2 * k)) * (x / (2 * k)); s += t; } return s; };
        double sum = 0;
        for (int i = 0; i < N; ++i) {
            const double m = i - 16.0, s = m == 0 ? 0.5 : std::sin(kPi * 0.5 * m) / (kPi * m);
            const double r = m / 16.0, w = i0(5.0 * std::sqrt(std::max(0.0, 1 - r * r))) / i0(5.0);
            h[i] = (float)(s * w); sum += h[i];
        }
        for (auto& v : h) v = (float)(v / sum);
    }
    void reset() { for (int i = 0; i < N; ++i) up[i] = dn[i] = 0; ui = di = 0; }
    void upsample(float x, float& a, float& b) {
        up[ui] = x; float e = 0, o = 0;
        for (int k = 0; 2 * k < N; ++k) e += h[2 * k] * up[(ui - k + N) % N];
        for (int k = 0; 2 * k + 1 < N; ++k) o += h[2 * k + 1] * up[(ui - k + N) % N];
        ui = (ui + 1) % N; a = 2 * e; b = 2 * o;
    }
    float downsample(float a, float b) {
        dn[di] = a; di = (di + 1) % N; dn[di] = b; di = (di + 1) % N;
        float y = 0;
        for (int i = 0; i < N; ++i) y += h[i] * dn[(di - 2 - i + 2 * N) % N];
        return y;
    }
};

// Estimation de crête vraie : 3 points intermédiaires entre deux échantillons (4×), 24 coefficients par phase.
struct TruePeak {
    static constexpr int T = 24, kLag = T / 2;   // la crête du segment [c, c+1] est connue kLag échantillons après c
    float co[3][T] = {};
    float x[2][32] = {}; int w = 0; float prevIp[2] = {0, 0};
    TruePeak() {
        auto i0 = [](double v) { double s = 1, t = 1; for (int k = 1; k < 30; ++k) { t *= (v / (2 * k)) * (v / (2 * k)); s += t; } return s; };
        for (int p = 0; p < 3; ++p) {
            const double f = (p + 1) / 4.0; double sum = 0;
            for (int j = 0; j < T; ++j) {
                const double d = (j - (T / 2 - 1)) - f, s = std::sin(kPi * d) / (kPi * d), r = d / (T / 2.0);
                co[p][j] = (float)(s * i0(7.0 * std::sqrt(std::max(0.0, 1 - r * r))) / i0(7.0)); sum += co[p][j];
            }
            for (int j = 0; j < T; ++j) co[p][j] = (float)(co[p][j] / sum);
        }
    }
    void reset() { for (auto& c : x) for (auto& v : c) v = 0; w = 0; prevIp[0] = prevIp[1] = 0; }
    // Entrée : échantillon courant (deux voies). Sortie : crête vraie de l'échantillon c = n - kLag (voies liées).
    // Les 3 points intermédiaires (4×) sont affinés par une parabole autour du maximum local (erreur < 0,05 dB).
    float push(float a, float b) {
        const float in[2] = {a, b}; float best = 0.f;
        for (int ch = 0; ch < 2; ++ch) {
            x[ch][w & 31] = in[ch];
            float v[5] = {x[ch][(w - kLag) & 31], 0, 0, 0, x[ch][(w - kLag + 1) & 31]};
            for (int p = 0; p < 3; ++p) { float s = 0.f; for (int j = 0; j < T; ++j) s += co[p][j] * x[ch][(w - (T - 1) + j) & 31]; v[p + 1] = s; }
            float ip = 0.f;
            for (int k = 0; k < 5; ++k) ip = std::max(ip, std::fabs(v[k]));
            for (int k = 1; k < 4; ++k) {
                const float sg = v[k] < 0 ? -1.f : 1.f, lo = sg * v[k - 1], mid = sg * v[k], hi = sg * v[k + 1], den = 2 * mid - lo - hi;
                if (mid >= lo && mid >= hi && den > 1e-9f) ip = std::max(ip, std::min(mid * 1.1f, mid + (hi - lo) * (hi - lo) / (8 * den)));
            }
            best = std::max(best, std::max(std::fabs(v[0]), std::max(ip, prevIp[ch])));
            prevIp[ch] = ip;
        }
        ++w; return best;
    }
};

// Limiteur look-ahead : minimum glissant de la réduction requise, lissage par moyenne glissante (atteint la valeur
// exacte au moment du pic), relâchement exponentiel. Le signal audio est retardé de latency() échantillons.
class Limiter {
public:
    void prepare(float sr_) {
        sr = sr_; W = std::max(8, (int)std::lround(0.003f * sr));
        D = W - 1 + TruePeak::kLag;
        const int n = nextPow2(D + 2);
        mask = n - 1; bufL.assign((size_t)n, 0.f); bufR = bufL;
        dq.assign((size_t)(W + 2), {1.f, 0}); head = tail = 0; cnt = 0;
        box.assign((size_t)W, 1.f); bi = 0; boxSum = (double)W; g = 1.f; idx = 0; w = 0; tp.reset();
        gIn = 1.f; gInT = 1.f; sc = 1.f - std::exp(-1.f / (0.02f * sr));
    }
    int latency() const { return D; }
    void set(bool on, float gainDb, float ceilDb, float relMs) {
        active = on; gInT = on ? std::pow(10.f, gainDb / 20.f) : 1.f;
        thr = on ? std::pow(10.f, (ceilDb - 0.1f) / 20.f) : 1e9f;   // 0,1 dB de marge pour l'erreur d'interpolation
        hard = on ? std::pow(10.f, ceilDb / 20.f) : 1e9f;
        rc = 1.f - std::exp(-1.f / (std::max(5.f, relMs) * 1e-3f * sr));
    }
    float minGain() const { return gMin; }
    void resetMeter() { gMin = 1.f; }
    void process(float& l, float& r) {
        gIn += (gInT - gIn) * sc;
        const float xl = l * gIn, xr = r * gIn;
        bufL[(size_t)(w & mask)] = xl; bufR[(size_t)(w & mask)] = xr;
        const float pk = tp.push(xl, xr);
        const float req = pk > thr ? thr / pk : 1.f;
        // minimum glissant (file monotone)
        while (cnt > 0 && dq[(size_t)((tail - 1 + (int)dq.size()) % (int)dq.size())].v >= req) { tail = (tail - 1 + (int)dq.size()) % (int)dq.size(); --cnt; }
        dq[(size_t)tail] = {req, idx}; tail = (tail + 1) % (int)dq.size(); ++cnt;
        while (dq[(size_t)head].i <= idx - W) { head = (head + 1) % (int)dq.size(); --cnt; }
        const float m = dq[(size_t)head].v;
        boxSum += (double)m - (double)box[(size_t)bi]; box[(size_t)bi] = m; bi = (bi + 1) % W; ++idx;
        const float bx = (float)(boxSum / W);
        g = bx < g ? bx : g + (bx - g) * rc;
        gMin = std::min(gMin, g);
        const size_t o = (size_t)((w - D) & mask);
        l = std::clamp(bufL[o] * g, -hard, hard); r = std::clamp(bufR[o] * g, -hard, hard);
        ++w;
    }
private:
    struct E { float v; int i; };
    static int nextPow2(int n) { int p = 1; while (p < n) p <<= 1; return p; }
    float sr = 48000.f, thr = 1e9f, hard = 1e9f, rc = 0.001f, g = 1.f, gMin = 1.f, gIn = 1.f, gInT = 1.f, sc = 0.001f;
    int W = 144, D = 149, mask = 255, head = 0, tail = 0, cnt = 0, bi = 0, idx = 0, w = 0;
    double boxSum = 0; bool active = true;
    std::vector<float> bufL, bufR, box; std::vector<E> dq;
    TruePeak tp;
};

// Loudness momentané (EBU R128 / BS.1770) : pondération K puis fenêtre de 400 ms (4 blocs de 100 ms).
struct Lufs {
    Biquad s1[2], s2[2]; double acc = 0; int n = 0, blk = 0, bl = 4800; double ring[4] = {0, 0, 0, 0}; int filled = 0; float value = -70.f;
    void prepare(float sr) {
        const double f1 = 1681.974450955533, G = 3.999843853973347, Q1 = 0.7071752369554196, K = std::tan(kPi * f1 / sr), Vh = std::pow(10.0, G / 20), Vb = std::pow(Vh, 0.4996667741545416);
        const double a0 = 1 + K / Q1 + K * K;
        const float b0 = (float)((Vh + Vb * K / Q1 + K * K) / a0), b1 = (float)(2 * (K * K - Vh) / a0), b2 = (float)((Vh - Vb * K / Q1 + K * K) / a0), a1 = (float)(2 * (K * K - 1) / a0), a2 = (float)((1 - K / Q1 + K * K) / a0);
        const double f2 = 38.13547087602444, Q2 = 0.5003270373238773, K2 = std::tan(kPi * f2 / sr), c0 = 1 + K2 / Q2 + K2 * K2;
        const float d1 = (float)(2 * (K2 * K2 - 1) / c0), d2 = (float)((1 - K2 / Q2 + K2 * K2) / c0);
        for (int c = 0; c < 2; ++c) { s1[c].reset(); s2[c].reset(); s1[c].set(b0, b1, b2, 1, a1, a2); s2[c].set(1, -2, 1, 1, d1, d2); }
        acc = 0; n = 0; filled = 0; bl = (int)(0.1f * sr); for (auto& r : ring) r = 0; value = -70.f;
    }
    void push(float l, float r) {
        const float a = s2[0].process(s1[0].process(l)), b = s2[1].process(s1[1].process(r));
        acc += (double)a * a + (double)b * b;
        if (++n >= bl) {
            ring[blk] = acc / n; blk = (blk + 1) & 3; filled = std::min(4, filled + 1); acc = 0; n = 0;
            double s = 0; for (int i = 0; i < filled; ++i) s += ring[i];
            const double p = s / filled; value = p > 1e-10 ? (float)(-0.691 + 10 * std::log10(p)) : -70.f;
        }
    }
};

class Engine {
public:
    void prepare(double sampleRate, int) {
        sr = (float)sampleRate;
        for (int c = 0; c < 2; ++c) { for (auto& b : eq[c]) b.reset(); os[c].reset(); side.reset(); }
        for (int i = 0; i < 2; ++i) { eqLast[i] = -99.f; }
        for (auto& v : eqLast2) v = -99.f;
        comp.prepare(sr); dryD[0].init(Os2::kLatency + 4); dryD[1].init(Os2::kLatency + 4);
        lim.prepare(sr); kIn.prepare(sr); kOut.prepare(sr);
        sc = 1.f - std::exp(-1.f / (0.02f * sr)); mixS = 0.f; wid = 1.f; coefValid = false; side.highpass(sr, 10.f, 0.7071f); sideF = 10.f;
        inPk = outPk = 0.f; grComp = 0.f;
    }
    int latency() const { return Os2::kLatency + lim.latency(); }
    void set(const Params& p) { P = p; }
    float inPeak() const { return inPk; }
    float outPeak() const { return outPk; }
    float compReduction() const { return grComp; }
    float limReduction() const { return -20.f * std::log10(lim.minGain() + 1e-9f); }
    float lufsIn() const { return kIn.value; }
    float lufsOut() const { return kOut.value; }

    // Traitement en place, deux voies.
    void process(float* l, float* r, int n) {
        update();
        lim.resetMeter();
        const float tMix = P.satOn ? P.satMix : 0.f, tW = P.stOn ? P.width : 1.f, k = 1.f + P.drive * 9.f, ik = 1.f / std::tanh(k);
        float ip = 0.f, op = 0.f, gm = 0.f;
        for (int i = 0; i < n; ++i) {
            float a = l[i], b = r[i];
            ip = std::max(ip, std::max(std::fabs(a), std::fabs(b)));
            kIn.push(a, b);
            for (int j = 0; j < kBands; ++j) { a = eq[0][j].process(a); b = eq[1][j].process(b); }
            const float g = comp.gain(a, b); gm = std::max(gm, comp.gr);
            a *= g; b *= g;
            // saturation tanh 2× avec mélange (dry*(1-0,6 mix) + wet*mix, comme le prototype)
            float ua[2], ub[2];
            os[0].upsample(a, ua[0], ua[1]); os[1].upsample(b, ub[0], ub[1]);
            for (int q = 0; q < 2; ++q) { ua[q] = std::tanh(k * ua[q]) * ik; ub[q] = std::tanh(k * ub[q]) * ik; }
            const float wa = os[0].downsample(ua[0], ua[1]), wb = os[1].downsample(ub[0], ub[1]);
            dryD[0].push(a); dryD[1].push(b);
            const float da = dryD[0].read((float)Os2::kLatency), db = dryD[1].read((float)Os2::kLatency);
            mixS += (tMix - mixS) * sc; wid += (tW - wid) * sc;
            a = da * (1.f - 0.6f * mixS) + wa * mixS; b = db * (1.f - 0.6f * mixS) + wb * mixS;
            // M/S : largeur, côté filtré passe-haut (basses en mono)
            const float m = 0.5f * (a + b), s = side.process(0.5f * (a - b) * wid);
            a = m + s; b = m - s;
            lim.process(a, b);
            l[i] = a; r[i] = b;
            kOut.push(a, b);
            op = std::max(op, std::max(std::fabs(a), std::fabs(b)));
        }
        inPk = ip; outPk = op; grComp = gm;
    }

private:
    void update() {
        for (int j = 0; j < kBands; ++j) {
            const float db = P.eqOn ? P.eq[j] : 0.f;
            if (db != eqLast2[j]) { for (auto& c : eq) setBand(c[j], j, sr, db); eqLast2[j] = db; }
        }
        comp.thr = P.compOn ? -4.f - P.cAmt * 26.f : 0.f; comp.ratio = P.compOn ? std::max(1.f, P.cRatio) : 1.f; comp.makeup = P.compOn ? P.cGain : 0.f;
        comp.aC = std::exp(-1.f / (std::max(0.5f, P.cAtt) * 1e-3f * sr)); comp.rC = std::exp(-1.f / (0.2f * sr));
        const float f = (P.stOn && P.mono) ? std::clamp(P.monoHz, 20.f, 400.f) : 10.f;
        if (f != sideF) { side.highpass(sr, f, 0.7071f); sideF = f; }
        lim.set(P.limOn, P.lGain, P.ceil, P.lRel);
    }
    float sr = 48000.f, sc = 0.001f, mixS = 0, wid = 1, sideF = 10, inPk = 0, outPk = 0, grComp = 0, eqLast[2], eqLast2[kBands];
    bool coefValid = false;
    Params P;
    Biquad eq[2][kBands], side;
    onek::Comp comp;
    Os2 os[2];
    voile::Delay dryD[2];
    Limiter lim;
    Lufs kIn, kOut;
};

// Contournement à latence compensée : sortie = entrée retardée de `latency` échantillons (exact).
struct BypassDelay {
    voile::Delay l, r; int lat = 0;
    void init(int latency) { lat = latency; l.init(latency + 8); r.init(latency + 8); }
    void process(float* a, float* b, int n, bool stereoIn) {
        for (int i = 0; i < n; ++i) { const float x0 = a[i], x1 = stereoIn ? b[i] : x0; l.push(x0); r.push(x1); a[i] = l.read((float)lat); b[i] = r.read((float)lat); }
    }
};

}  // namespace mas
