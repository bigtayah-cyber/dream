// Orbe Master : chaîne de mastering stéréo. C++ pur (sans JUCE).
// Chaîne et valeurs par défaut = section « master » de mix.html (EQF, P0, applyP), avec un VRAI limiteur
// look-ahead à plafond true-peak (interpolation 4×) à la place du compresseur + écrêtage du prototype.
#pragma once
#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <atomic>
#include <vector>
#include "../../OneKnob/OneKnob.h"
#include "../../Dsp/TruePeak.h"

#ifndef HOLD_MS_DEFAULT
#define HOLD_MS_DEFAULT 30.f
#endif
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
    int target = 0;           // lot E : cible LUFS court terme (0 coupée, 1 : -14, 2 : -11, 3 : -9) ; ajuste le gain d'entrée du limiteur, lentement
    bool match = false;       // écoute à niveau égal : atténue la sortie pour qu'elle ait le LUFS court terme de l'entrée (jamais d'amplification)
    bool delta = false;       // n'écoute que ce que la chaîne a changé (sortie - entrée retardée de la latence)
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

using TruePeak = dsp::TruePeak;   // détecteur unique, partagé avec Orbe Mix (Dsp/TruePeak.h)

// Limiteur look-ahead : minimum glissant de la réduction requise, lissage par moyenne glissante (atteint la valeur
// exacte au moment du pic), puis MAINTIEN (minimum glissant sur kHoldMs) et relâchement exponentiel. Le maintien couvre une
// période entière des graves (40 ms = 25 Hz) : le gain ne suit plus l'onde d'une 808 (avant : DHT 1,16 % à 40 Hz, 8 dB de GR).
// Le signal audio est retardé de latency() échantillons.
class Limiter {
public:
    void prepare(float sr_) {
        sr = sr_; W = std::max(8, (int)std::lround(0.003f * sr));
        D = W - 1 + TruePeak::kLag;
        const int n = nextPow2(D + 2);
        mask = n - 1; bufL.assign((size_t)n, 0.f); bufR = bufL;
        dq.assign((size_t)(W + 2), {1.f, 0}); head = tail = 0; cnt = 0;
        H = std::max(W, (int)std::lround(kHoldMs * 1e-3f * sr)); hq.assign((size_t)(H + 2), {1.f, 0}); hHead = hTail = 0; hCnt = 0; hIdx = 0;
        box.assign((size_t)W, 1.f); bi = 0; boxSum = (double)W; g = 1.f; idx = 0; w = 0; tp.reset();
        gIn = 1.f; gInT = 1.f; sc = 1.f - std::exp(-1.f / (0.02f * sr));
    }
    int latency() const { return D; }
    void set(bool on, float gainDb, float ceilDb, float relMs) {
        active = on; gInT = on ? std::pow(10.f, gainDb / 20.f) : 1.f;
        thr = on ? std::pow(10.f, (ceilDb - 0.15f) / 20.f) : 1e9f;   // 0,15 dB de marge : l'estimateur sous-estime d'au plus 0,04 dB (sinus) et 0,3 dB (bruit blanc pleine bande)
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
        // maintien : minimum de bx sur les H derniers échantillons (file monotone), puis relâchement depuis ce plancher
        const int hn = (int)hq.size();
        while (hCnt > 0 && hq[(size_t)((hTail - 1 + hn) % hn)].v >= bx) { hTail = (hTail - 1 + hn) % hn; --hCnt; }
        hq[(size_t)hTail] = {bx, hIdx}; hTail = (hTail + 1) % hn; ++hCnt;
        while (hq[(size_t)hHead].i <= hIdx - H) { hHead = (hHead + 1) % hn; --hCnt; }
        const float hold = hq[(size_t)hHead].v; ++hIdx;
        g = hold < g ? hold : g + (hold - g) * rc;
        gMin = std::min(gMin, g);
        const size_t o = (size_t)((w - D) & mask);
        l = std::clamp(bufL[o] * g, -hard, hard); r = std::clamp(bufR[o] * g, -hard, hard);
        ++w;
    }
private:
    struct E { float v; int i; };
    static int nextPow2(int n) { int p = 1; while (p < n) p <<= 1; return p; }
    static constexpr float kHoldMs = HOLD_MS_DEFAULT;
    float sr = 48000.f, thr = 1e9f, hard = 1e9f, rc = 0.001f, g = 1.f, gMin = 1.f, gIn = 1.f, gInT = 1.f, sc = 0.001f;
    int W = 144, D = 149, mask = 255, head = 0, tail = 0, cnt = 0, bi = 0, idx = 0, w = 0;
    int H = 1920, hHead = 0, hTail = 0, hCnt = 0, hIdx = 0;
    std::vector<E> hq;
    double boxSum = 0; bool active = true;
    std::vector<float> bufL, bufR, box; std::vector<E> dq;
    TruePeak tp;
};

// Loudness (EBU R128 / BS.1770-4) : pondération K, blocs de 100 ms.
//  - momentané : fenêtre de 400 ms (4 blocs) ;
//  - court terme : fenêtre de 3 s (30 blocs ; sur les 3 premières secondes, la moyenne de ce qui est arrivé) ;
//  - intégré : blocs de 400 ms au pas de 100 ms (recouvrement 75 %), porte absolue -70 LUFS puis porte relative -10 LU,
//    accumulés dans un histogramme au centième de LU (alloué dans prepare, aucune allocation dans le thread audio).
struct Lufs {
    Biquad s1[2], s2[2]; double acc = 0; int n = 0, blk = 0, bl = 4800; double ring[30] = {}; int filled = 0;
    float value = -70.f, shortTerm = -70.f, integrated = -70.f; int skip = 0;
    std::vector<double> hSum; std::vector<std::uint32_t> hCnt;
    static constexpr int kBins = 8001;
    void prepare(float sr) {
        const double f1 = 1681.974450955533, G = 3.999843853973347, Q1 = 0.7071752369554196, K = std::tan(kPi * f1 / sr), Vh = std::pow(10.0, G / 20), Vb = std::pow(Vh, 0.4996667741545416);
        const double a0 = 1 + K / Q1 + K * K;
        const float b0 = (float)((Vh + Vb * K / Q1 + K * K) / a0), b1 = (float)(2 * (K * K - Vh) / a0), b2 = (float)((Vh - Vb * K / Q1 + K * K) / a0), a1 = (float)(2 * (K * K - 1) / a0), a2 = (float)((1 - K / Q1 + K * K) / a0);
        const double f2 = 38.13547087602444, Q2 = 0.5003270373238773, K2 = std::tan(kPi * f2 / sr), c0 = 1 + K2 / Q2 + K2 * K2;
        const float d1 = (float)(2 * (K2 * K2 - 1) / c0), d2 = (float)((1 - K2 / Q2 + K2 * K2) / c0);
        for (int c = 0; c < 2; ++c) { s1[c].reset(); s2[c].reset(); s1[c].set(b0, b1, b2, 1, a1, a2); s2[c].set(1, -2, 1, 1, d1, d2); }
        bl = std::max(1, (int)(0.1f * sr));
        hSum.assign(kBins, 0.0); hCnt.assign(kBins, 0u);
        resetIntegrated(); value = shortTerm = -70.f;
    }
    // Remise à zéro de l'intégré seul (appelée depuis le thread audio : voir Engine::requestLufsReset).
    void resetIntegrated(int skipSamples = 0) {
        skip = skipSamples;   // sortie : les `latence` premiers échantillons sont encore l'ancien passage
        std::fill(hSum.begin(), hSum.end(), 0.0); std::fill(hCnt.begin(), hCnt.end(), 0u); integrated = -70.f;
        acc = 0; n = 0; blk = 0; filled = 0; for (auto& r : ring) r = 0;   // les fenêtres repartent aussi : rien de l'ancien passage ne contamine le nouveau
    }
    static float lk(double p) { return p > 1e-10 ? (float)(-0.691 + 10 * std::log10(p)) : -70.f; }
    void push(float l, float r) {
        if (skip > 0) { --skip; return; }
        const float a = s2[0].process(s1[0].process(l)), b = s2[1].process(s1[1].process(r));
        acc += (double)a * a + (double)b * b;
        if (++n >= bl) {
            ring[blk] = acc / n; blk = (blk + 1) % 30; filled = std::min(30, filled + 1); acc = 0; n = 0;
            auto mean = [&](int k) { double s = 0; for (int j = 1; j <= k; ++j) s += ring[(blk - j + 60) % 30]; return s / k; };
            const double pm = mean(std::min(4, filled)), ps = mean(filled);
            value = lk(pm); shortTerm = lk(ps);
            if (filled >= 4 && value > -70.f) {   // porte absolue -70 LUFS
                const int bin = std::clamp((int)std::floor((value + 70.0) * 100.0), 0, kBins - 1);
                hSum[(size_t)bin] += pm; hCnt[(size_t)bin]++;
                double S = 0; std::uint64_t C = 0;
                for (int i = 0; i < kBins; ++i) { S += hSum[(size_t)i]; C += hCnt[(size_t)i]; }
                const double rel = lk(S / (double)C) - 10.0, relBin = (rel + 70.0) * 100.0;   // porte relative : -10 LU sous la moyenne
                double S2 = 0; std::uint64_t C2 = 0;
                for (int i = (int)std::max(0.0, std::floor(relBin)); i < kBins; ++i) if (i + 0.5 > relBin) { S2 += hSum[(size_t)i]; C2 += hCnt[(size_t)i]; }
                integrated = C2 ? lk(S2 / (double)C2) : -70.f;
            }
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
        sc = 1.f - std::exp(-1.f / (0.02f * sr)); mixS = 0.f; lvC = 1.f - std::exp(-1.f / (0.3f * sr)); pPre = pSat = 0.f; wid = 1.f; coefValid = false; side.highpass(sr, 10.f, 0.7071f); sideF = 10.f;
        inPk = outPk = 0.f; grComp = 0.f; autoG = 0.f; mG = 1.f; for (auto& d : dlyIn) d.init(latency() + 8);
    }
    int latency() const { return Os2::kLatency + lim.latency(); }
    void set(const Params& p) { P = p; }
    float inPeak() const { return inPk; }
    float outPeak() const { return outPk; }
    float compReduction() const { return grComp; }
    float limReduction() const { return -20.f * std::log10(lim.minGain() + 1e-9f); }
    float lufsIn() const { return kIn.value; }
    float lufsOut() const { return kOut.value; }
    float lufsShortIn() const { return kIn.shortTerm; }
    float lufsShortOut() const { return kOut.shortTerm; }
    float lufsIntegratedIn() const { return kIn.integrated; }
    float lufsIntegratedOut() const { return kOut.integrated; }
    void requestLufsReset() { lufsReset.store(true, std::memory_order_release); }   // tout thread ; exécuté au prochain bloc

    // Traitement en place, deux voies.
    void process(float* l, float* r, int n) {
        update();
        lim.resetMeter();
        if (lufsReset.exchange(false, std::memory_order_acq_rel)) { kIn.resetIntegrated(); kOut.resetIntegrated(latency()); }
        const float tMix = P.satOn ? P.satMix : 0.f, tW = P.stOn ? P.width : 1.f, k = 1.f + P.drive * 9.f, ik = 1.f / k;
        float ip = 0.f, op = 0.f, gm = 0.f;
        // cible LUFS : intégrateur lent (0,15 dB/s par dB d'écart, au plus 1 dB/s) sur le gain d'entrée du limiteur ; l'écart est mesuré sur le court terme de la sortie
        {
            const float dt = (float)n / sr;
            if (P.target > 0 && P.limOn && kOut.shortTerm > -60.f && kIn.shortTerm > -60.f) {
                static const float tl[4] = {0.f, -14.f, -11.f, -9.f};
                const float err = tl[std::clamp(P.target, 0, 3)] - kOut.shortTerm;
                autoG += std::clamp(0.15f * err, -1.f, 1.f) * dt;
            } else if (P.target == 0) autoG += std::clamp(-autoG, -6.f * dt, 6.f * dt);
            autoG = std::clamp(autoG, -12.f, 12.f);
            const float tot = std::clamp(P.lGain + autoG, -3.f, 24.f); autoG = tot - P.lGain;
            lim.set(P.limOn, tot, P.ceil, P.lRel);
        }
        float mTgt = 1.f;
        if (P.match && kIn.shortTerm > -60.f && kOut.shortTerm > -60.f) mTgt = std::pow(10.f, std::min(0.f, std::clamp(kIn.shortTerm - kOut.shortTerm, -18.f, 18.f)) / 20.f);
        const float mC = 1.f - std::exp(-1.f / (0.5f * sr));
        for (int i = 0; i < n; ++i) {
            float a = l[i], b = r[i];
            const float x0 = a, x1 = b;
            dlyIn[0].push(x0); dlyIn[1].push(x1);
            ip = std::max(ip, std::max(std::fabs(a), std::fabs(b)));
            kIn.push(a, b);
            for (int j = 0; j < kBands; ++j) { a = eq[0][j].process(a); b = eq[1][j].process(b); }
            const float g = comp.gain(a, b); gm = std::max(gm, comp.gr);
            a *= g; b *= g;
            // saturation tanh 2×, pente 1 en zéro (tanh(kx)/k : aucun gain en petits signaux), puis compensation de niveau
            // (rapport des puissances avant/après sur 300 ms, borné à [1, 1 + k]) : DRIVE change la couleur, pas le volume.
            // Mélange à gain unité dry*(1-mix) + wet*mix (l'ancien dry*(1-0,6 mix) du prototype ajoutait jusqu'à +20 dB).
            float ua[2], ub[2];
            os[0].upsample(a, ua[0], ua[1]); os[1].upsample(b, ub[0], ub[1]);
            for (int q = 0; q < 2; ++q) { ua[q] = std::tanh(k * ua[q]) * ik; ub[q] = std::tanh(k * ub[q]) * ik; }
            float wa = os[0].downsample(ua[0], ua[1]), wb = os[1].downsample(ub[0], ub[1]);
            pPre += (a * a + b * b - pPre) * lvC; pSat += (wa * wa + wb * wb - pSat) * lvC;
            const float comp = std::clamp(std::sqrt((pPre + 1e-12f) / (pSat + 1e-12f)), 1.f, 1.f + k);
            wa *= comp; wb *= comp;
            dryD[0].push(a); dryD[1].push(b);
            const float da = dryD[0].read((float)Os2::kLatency), db = dryD[1].read((float)Os2::kLatency);
            mixS += (tMix - mixS) * sc; wid += (tW - wid) * sc;
            a = da * (1.f - mixS) + wa * mixS; b = db * (1.f - mixS) + wb * mixS;
            // M/S : largeur, côté filtré passe-haut (basses en mono)
            const float m = 0.5f * (a + b), s = side.process(0.5f * (a - b) * wid);
            a = m + s; b = m - s;
            lim.process(a, b);
            kOut.push(a, b);
            if (P.match || mG != 1.f) { mG += (mTgt - mG) * mC; if (std::fabs(mG - 1.f) < 1e-6f && mTgt == 1.f) mG = 1.f; a *= mG; b *= mG; }
            if (P.delta) { a -= dlyIn[0].read((float)latency()); b -= dlyIn[1].read((float)latency()); }
            l[i] = a; r[i] = b;
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
        lim.set(P.limOn, P.lGain + autoG, P.ceil, P.lRel);
    }
    float lvC = 1e-4f, pPre = 0.f, pSat = 0.f;   // compensation de niveau de la saturation
    float sr = 48000.f, sc = 0.001f, mixS = 0, wid = 1, sideF = 10, inPk = 0, outPk = 0, grComp = 0, eqLast[2], eqLast2[kBands];
    bool coefValid = false;
    Params P;
    Biquad eq[2][kBands], side;
    onek::Comp comp;
    Os2 os[2];
    voile::Delay dryD[2];
    Limiter lim;
    Lufs kIn, kOut;
    float autoG = 0.f, mG = 1.f; voile::Delay dlyIn[2];
    std::atomic<bool> lufsReset{false};
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
