// Orbe Cabine : chaîne vocale par piste. C++ pur (sans JUCE).
// Valeurs des presets = celles de cabine.html (objet FX) ; la chaîne reproduit le graphe WebAudio du prototype.
#pragma once
#include <algorithm>
#include <cmath>
#include <vector>
#include "../../OneKnob/OneKnob.h"

namespace cab {

using onek::Biquad;
constexpr float kPi = 3.14159265358979f;

struct Params {
    float hp = 20, mud = 0, pres = 0, air = 0, lp = 20000, sat = 0;   // Hz, dB, dB, dB, Hz (20000 = coupé), drive
    float thr = 0, ratio = 1, atk = 6, rel = 120, mk = 0;             // dB, :1, ms, ms, dB
    float rev = 0, dly = 0, dbl = 0, out = 0;                          // envois 0..1, doubleur 0..1, sortie dB
};
struct Preset { const char* name; Params p; };

inline Params mk(float hp, float mud, float pres, float air, float lp, float sat, float thr, float ratio, float atkMs, float relMs, float mkDb, float rev, float dly, float dbl) {
    Params p; p.hp = hp; p.mud = mud; p.pres = pres; p.air = air; p.lp = lp; p.sat = sat; p.thr = thr; p.ratio = ratio; p.atk = atkMs; p.rel = relMs; p.mk = mkDb; p.rev = rev; p.dly = dly; p.dbl = dbl; return p;
}
constexpr int kPresets = 11, kDefaultPreset = 2;
inline const Preset* presets() {
    static const Preset t[kPresets] = {
        {"Beat",         mk(20, 0, 0, 0, 20000, 0, 0, 1, 6, 120, 0, 0, 0, 0)},
        {"Brut",         mk(20, 0, 0, 0, 20000, 0, 0, 1, 6, 120, 0, 0, 0, 0)},
        {"Voix lead",    mk(90, -3, 2.5f, 2, 20000, 0, -22, 3.5f, 6, 120, 3, .14f, .1f, 0)},
        {"Backs",        mk(140, -4, 1, 3, 20000, 0, -26, 4, 8, 150, 3, .28f, .05f, 0)},
        {"Ad-libs",      mk(220, -6, 4, 1, 20000, 0, -28, 6, 4, 100, 4, .2f, .32f, 0)},
        {"Voix doubl\xC3\xA9""e", mk(100, -3, 2.5f, 2.5f, 20000, 0, -22, 3.5f, 6, 120, 3, .12f, .06f, .5f)},
        {"A\xC3\xA9rienne", mk(140, -4, 1.5f, 5, 20000, 0, -24, 3, 10, 200, 3, .5f, .22f, .25f)},
        {"\xC3\x89""cho rap", mk(120, -3, 3, 1.5f, 20000, 0, -24, 4, 5, 120, 3, .1f, .55f, 0)},
        {"Satur\xC3\xA9""e", mk(120, -2, 3.5f, 1, 20000, 5, -26, 5, 4, 100, 2, .12f, .12f, 0)},
        {"T\xC3\xA9l\xC3\xA9phone", mk(500, 0, 4, 0, 3200, 3, -24, 5, 4, 100, 4, .06f, .18f, 0)},
        {"Lo-fi",        mk(180, -2, 1, -4, 5200, 2, -24, 4, 8, 150, 3, .25f, .1f, 0)},
    };
    return t;
}

inline void peaking(Biquad& b, float sr, float f, float db, float q) {
    const float A = std::pow(10.f, db / 40.f), w = 2 * kPi * f / sr, c = std::cos(w), al = std::sin(w) / (2 * q);
    b.set(1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
}

// Sur-échantillonnage 2× par FIR linéaire (33 coefficients, Kaiser) : latence exacte de 16 échantillons de base.
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
    // x → deux échantillons à 2×
    void upsample(float x, float& a, float& b) {
        up[ui] = x; float e = 0, o = 0;
        for (int k = 0; 2 * k < N; ++k) e += h[2 * k] * up[(ui - k + N) % N];
        for (int k = 0; 2 * k + 1 < N; ++k) o += h[2 * k + 1] * up[(ui - k + N) % N];
        ui = (ui + 1) % N; a = 2 * e; b = 2 * o;
    }
    // deux échantillons à 2× → un échantillon de base
    float downsample(float a, float b) {
        dn[di] = a; di = (di + 1) % N; dn[di] = b; di = (di + 1) % N;
        float y = 0;
        for (int i = 0; i < N; ++i) y += h[i] * dn[(di - 2 - i + 2 * N) % N];  // phase paire : retard total exactement 16
        return y;
    }
};

// pente 1 en zéro : aucun gain en petits signaux (l'ancienne forme tanh(kx)/tanh(k) ajoutait jusqu'à +20 dB à sat 10)
inline float satShape(float x, float k) { return std::tanh(x * k) / k; }

class Engine {
public:
    void prepare(double sampleRate, int) {
        sr = (float)sampleRate;
        for (int c = 0; c < 2; ++c) { hp[c].reset(); mud[c].reset(); pres[c].reset(); air[c].reset(); lp[c].reset(); os[c].reset(); }
        comp.prepare(sr); comp.env = 0; comp.gr = 0;
        rhp.reset(); rlp.reset(); rhp.highpass(sr, 300.f); rlp.lowpass(sr, 8000.f); dlp[0].reset(); dlp[1].reset();
        for (auto& b : dlp) b.lowpass(sr, 3000.f, 0.707f);
        rev.init(sampleRate); rev.setDecay(1.8f);
        dly[0].init((int)(2.05 * sampleRate)); dly[1].init((int)(2.05 * sampleRate));
        for (auto& d : dd) d.init((int)(0.1 * sampleRate));
        lfo[0] = lfo[1] = 0.f; dlyT = 0.f;
        gMk = gOut = gRev = gDly = gDbl = -1.f; last = Params{}; lastValid = false; outPk = 0.f; grNow = 0.f;
        sc = 1.f - std::exp(-1.f / (0.02f * sr));
        lvC = 1.f - std::exp(-1.f / (0.3f * sr)); pPre = pSat = 0.f; satS = -1.f;
    }
    int latency() const { return Os2::kLatency; }
    void setBpm(double b) { bpm = (float)(b > 20.0 ? b : 120.0); }
    void set(const Params& p) { P = p; }
    float outPeak() const { return outPk; }
    float outMeanSquare() const { return outMs; }
    float gainReduction() const { return grNow; }

    // Entrée mono (inR nul) ou stéréo ; sortie stéréo ; peut travailler en place.
    void process(const float* inL, const float* inR, float* outL, float* outR, int n) {
        updateCoefs();
        const float dTarget = std::min(1.9f, 0.75f * 60.f / bpm) * sr;
        if (dlyT <= 0.f) dlyT = dTarget;
        const float tMk = std::pow(10.f, P.mk / 20.f), tOut = std::pow(10.f, P.out / 20.f);
        if (gMk < 0) { gMk = tMk; gOut = tOut; gRev = P.rev; gDly = P.dly; gDbl = P.dbl; }
        if (satS < 0.f) satS = P.sat;
        const float pw[2][2] = {{0.9930f, 0.1175f}, {0.1175f, 0.9930f}};  // pans -0.85 / +0.85, loi en cosinus
        float pk = 0.f, gmax = 0.f, ss = 0.f;
        for (int i = 0; i < n; ++i) {
            const float xl = inL[i], xr = inR ? inR[i] : xl;
            float ul[2], ur[2];
            float l = air[0].process(pres[0].process(mud[0].process(hp[0].process(xl))));
            float r = air[1].process(pres[1].process(mud[1].process(hp[1].process(xr))));
            if (lpOn) { l = lp[0].process(l); r = lp[1].process(r); }
            // saturation tanh sur-échantillonnée 2× (le FIR tourne toujours : latence constante)
            // SATURATION lissée (20 ms) puis compensation de niveau (rapport des puissances avant/après sur 300 ms, borné à [1, 1 + sat]) :
            // le potard change la couleur, pas le volume
            satS += (P.sat - satS) * sc;
            os[0].upsample(l, ul[0], ul[1]); os[1].upsample(r, ur[0], ur[1]);
            const bool satOn = satS > 0.01f;
            if (satOn) for (int k = 0; k < 2; ++k) { ul[k] = satShape(ul[k], satS); ur[k] = satShape(ur[k], satS); }
            float a = os[0].downsample(ul[0], ul[1]), b = os[1].downsample(ur[0], ur[1]);
            pPre += (l * l + r * r - pPre) * lvC; pSat += (a * a + b * b - pSat) * lvC;
            if (satOn) { const float c = std::clamp(std::sqrt((pPre + 1e-12f) / (pSat + 1e-12f)), 1.f, 1.f + satS); a *= c; b *= c; }
            const float g = comp.gain(a, b);
            gmax = std::max(gmax, comp.gr);
            gMk += (tMk - gMk) * sc; gOut += (tOut - gOut) * sc; gRev += (P.rev - gRev) * sc; gDly += (P.dly - gDly) * sc; gDbl += (P.dbl - gDbl) * sc;
            a *= g * gMk * gOut; b *= g * gMk * gOut;
            const float m = 0.5f * (a + b);
            // doubleur : 2 voix modulées, panoramiquées
            dd[0].push(m); dd[1].push(m);
            float dl = 0.f, dr = 0.f;
            for (int v = 0; v < 2; ++v) {
                lfo[v] += (v ? 0.47f : 0.31f) / sr; if (lfo[v] >= 1.f) lfo[v] -= 1.f;
                const float d = ((v ? 0.021f : 0.013f) + 0.0025f * std::sin(2 * kPi * lfo[v])) * sr;
                const float s = dd[v].read(d) * gDbl;
                dl += s * pw[v][0]; dr += s * pw[v][1];
            }
            // reverb (FDN de Voile) : HP 300 Hz, LP 8 kHz avant l'entrée
            float rL = 0.f, rR = 0.f;
            if (gRev > 1e-4f || revTail > 0) {
                rev.process(rlp.process(rhp.process(m * gRev)), 0.35f, rL, rR);
                revTail = std::fabs(rL) + std::fabs(rR) > 1e-6f ? 1 : 0;
            }
            // delay croche pointée sync tempo : feedback .32, LP 3 kHz dans la boucle
            dlyT += (dTarget - dlyT) * 0.0005f;
            const float D = std::max(2.f, dlyT);
            float yl = dlp[0].process(dly[0].read(D - 1.f)), yr = dlp[1].process(dly[1].read(D - 1.f));
            dly[0].push(a * gDly + 0.32f * yl); dly[1].push(b * gDly + 0.32f * yr);
            float oL = a + dl + 0.9f * rL + 0.8f * yl, oR = b + dr + 0.9f * rR + 0.8f * yr;
            if (!std::isfinite(oL) || !std::isfinite(oR)) { oL = oR = 0.f; reset(); }
            outL[i] = oL; outR[i] = oR;
            pk = std::max(pk, std::max(std::fabs(oL), std::fabs(oR))); ss += 0.5f * (oL * oL + oR * oR);
        }
        outPk = pk; grNow = gmax; outMs = n > 0 ? ss / (float)n : 0.f;
    }

private:
    void reset() {
        for (int c = 0; c < 2; ++c) { hp[c].reset(); mud[c].reset(); pres[c].reset(); air[c].reset(); lp[c].reset(); os[c].reset(); dlp[c].reset(); dly[c].init((int)(2.05 * sr)); }
        rev.init(sr); rev.setDecay(1.8f); rhp.reset(); rlp.reset(); comp.env = 0;
    }
    void updateCoefs() {
        const Params& p = P;
        lpOn = p.lp < 19900.f;
        if (!lastValid || p.hp != last.hp) for (auto& f : hp) f.highpass(sr, std::min(p.hp, 0.45f * sr), 0.7071f);
        if (!lastValid || p.mud != last.mud) for (auto& f : mud) peaking(f, sr, 300.f, p.mud, 1.f);
        if (!lastValid || p.pres != last.pres) for (auto& f : pres) peaking(f, sr, 4000.f, p.pres, 0.9f);
        if (!lastValid || p.air != last.air) for (auto& f : air) f.highShelf(sr, std::min(10000.f, 0.45f * sr), p.air);
        if (!lastValid || p.lp != last.lp) for (auto& f : lp) f.lowpass(sr, std::min(p.lp, 0.45f * sr), 0.9f);
        comp.thr = p.thr; comp.ratio = std::max(1.f, p.ratio); comp.makeup = 0.f;
        comp.aC = std::exp(-1.f / (std::max(0.1f, p.atk) * 1e-3f * sr)); comp.rC = std::exp(-1.f / (std::max(5.f, p.rel) * 1e-3f * sr));
        last = p; lastValid = true;
    }
    float sr = 48000.f, bpm = 120.f, sc = 0.001f, dlyT = 0.f, lfo[2] = {0, 0}, gMk = -1, gOut = 1, gRev = 0, gDly = 0, gDbl = 0, outPk = 0, outMs = 0, grNow = 0;
    float lvC = 1e-4f, pPre = 0.f, pSat = 0.f, satS = -1.f;   // compensation de niveau et lissage de la saturation
    int revTail = 0;
    bool lpOn = false, lastValid = false;
    Params P, last;
    Biquad hp[2], mud[2], pres[2], air[2], lp[2], dlp[2], rhp, rlp;
    Os2 os[2];
    onek::Comp comp;
    voile::Reverb rev;
    voile::Delay dly[2], dd[2];
};

// Contournement à latence compensée : sortie = entrée retardée de `latency` échantillons (exact).
struct BypassDelay {
    voile::Delay l, r;
    void init(int latency) { lat = latency; l.init(latency + 8); r.init(latency + 8); }
    void process(float* a, float* b, int n, bool stereoIn) {
        for (int i = 0; i < n; ++i) {
            const float x0 = a[i], x1 = stereoIn ? b[i] : x0;
            l.push(x0); r.push(x1); a[i] = l.read((float)lat); b[i] = r.read((float)lat);
        }
    }
    int lat = 0;
};

}  // namespace cab
