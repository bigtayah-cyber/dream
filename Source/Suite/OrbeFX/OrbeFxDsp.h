// Orbe FX : chaîne d'effets audio, DSP original (aucun code tiers, aucune GPL), sans JUCE, sans allocation après prepare().
//   Filtre (SVF à intégrateurs trapézoïdaux, équations publiées de Zavalishin / Simper) -> Saturation (tanh, écrêtage dur ou asymétrique,
//   anti-repliement par primitive : ADAA du 1er ordre) -> Chorus / Ensemble (3 voix, ligne à retard interpolée Hermite) -> Delay stéréo
//   (calé sur le tempo, ping-pong, boucle filtrée et bornée) -> Réverbe (réseau à retards rebouclés : 8 lignes, matrice de Householder,
//   diffuseurs d'entrée, légère modulation) -> bascule Couleur (inclinaison spectrale) -> Mix et Sortie.
// Latence nulle : aucun tampon de prévision. (L'ADAA décale la branche saturée d'une demi-échantillon : non compensé, non audible.)
// Les macros n'écrivent aucun paramètre : elles s'ajoutent aux réglages des modules à l'intérieur de set() (l'automation reste lisible).
#pragma once
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <vector>

namespace ofx {

constexpr float kPi = 3.14159265358979323846f;
inline float db2lin(float d) { return std::pow(10.f, d * 0.05f); }
inline float clamp01(float x) { return x < 0.f ? 0.f : (x > 1.f ? 1.f : x); }
inline float clampf(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }
inline bool finiteF(float x) { std::uint32_t u; std::memcpy(&u, &x, sizeof u); return (u & 0x7f800000u) != 0x7f800000u; }

// Lissage d'un paramètre (un pôle, constante de temps ~15 ms).
struct Sm {
    float v = 0.f, t = 0.f, c = 0.002f;
    void setTime(float sr, float sec) { c = 1.f - std::exp(-1.f / std::max(1.f, sec * sr)); }
    void snap(float x) { v = t = x; }
    float next() { v += (t - v) * c; return v; }
};

// Ligne à retard circulaire (taille puissance de 2), lecture fractionnaire Hermite 4 points.
// read(d) : d = 0 donne le dernier échantillon écrit ; d >= 1 pour interpoler.
struct Ring {
    std::vector<float> b; int mask = 0, w = 0;
    void alloc(int minLen) { int n = 8; while (n < minLen + 8) n <<= 1; b.assign((size_t)n, 0.f); mask = n - 1; w = 0; }
    void clear() { std::fill(b.begin(), b.end(), 0.f); w = 0; }
    void push(float x) { b[(size_t)w] = x; w = (w + 1) & mask; }
    float at(int k) const { return b[(size_t)((w - 1 - k) & mask)]; }
    float read(float d) const {
        const int i = (int)d; const float f = d - (float)i;
        const float a = at(i - 1), bb = at(i), c = at(i + 1), dd = at(i + 2);
        return bb + 0.5f * f * (c - a + f * (2.f * a - 5.f * bb + 4.f * c - dd + f * (3.f * (bb - c) + dd - a)));
    }
};

struct Params {
    bool fOn = true; int fMode = 0; float fCut = 20000.f, fRes = 0.2f, fEnv = 0.f;
    bool sOn = true; int sType = 0; float sDrive = 0.f, sTone = 1.f, sMix = 0.f;
    bool cOn = true; float cRate = 0.6f, cDepth = 0.5f, cSpread = 0.8f, cMix = 0.f;
    bool dOn = true; int dTime = 3, dPing = 0; float dFb = 0.35f, dTone = 0.6f, dMix = 0.f;
    bool rOn = true; float rSize = 0.5f, rDecay = 2.2f, rPre = 15.f, rDamp = 0.5f, rMix = 0.f;
    float mCol = 0.f, mGrain = 0.f, mMove = 0.f, mSpace = 0.f;   // macros : Couleur -1..1, Grain / Mouvement / Espace 0..1
    float mix = 1.f, gainDb = 0.f;
    double bpm = 120.0;
};
// Durées du delay en temps (noires) : 1/16, 1/8, 1/8 pointée, 1/4, 1/4 pointée, 1/2, 1/2 pointée.
inline float delayBeats(int i) { static const float b[7] = {0.25f, 0.5f, 0.75f, 1.f, 1.5f, 2.f, 3.f}; return b[std::max(0, std::min(6, i))]; }

class Engine {
public:
    void prepare(double sampleRate) {
        sr = (float)sampleRate;
        const int chorusLen = (int)(0.04f * sr) + 16;
        for (auto& r : chor) r.alloc(chorusLen);
        const int dlyLen = (int)(3.05f * sr) + 16;
        for (auto& r : dly) r.alloc(dlyLen);
        const float k = sr / 48000.f;
        for (int i = 0; i < N; ++i) { line[i].alloc((int)(kBase[i] * 1.7f * k) + 64); lenNow[i] = kBase[i] * 1.f * k; }
        pre.alloc((int)(0.13f * sr) + 8);
        static const int apL[4] = {142, 107, 379, 277};
        for (int i = 0; i < 4; ++i) { apLen[i] = std::max(2, (int)(apL[i] * k)); ap[i].alloc(apLen[i] + 4); }
        for (auto* s : {&smCut, &smRes, &smFilt, &smDrive, &smSMix, &smSTone, &smCMix, &smCDepth, &smDMix, &smDFb, &smDTone, &smDTime, &smRMix, &smTiltL, &smTiltH, &smMix, &smGain})
            s->setTime(sr, 0.015f);
        smDTime.setTime(sr, 0.12f);   // glissement du retard : effet « bande » plutôt qu'un saut
        reset();
        set(Params{}); snapAll();
    }
    void reset() {
        for (auto& s : svf) s = {};
        env = 0.f; for (auto& v : satX1) v = 0.f; for (auto& v : satTone) v = 0.f; for (auto& v : satDc) v = 0.f; for (auto& v : satDcIn) v = 0.f;
        for (auto& r : chor) r.clear(); for (auto& r : dly) r.clear();
        for (auto& v : dlyLp) v = 0.f; for (auto& v : dlyHp) v = 0.f; for (auto& v : dlyHpIn) v = 0.f;
        for (auto& r : line) r.clear(); for (auto& v : lineLp) v = 0.f; pre.clear(); for (auto& a : ap) a.clear();
        for (auto& v : tiltLp) v = 0.f; lfo = 0.f; for (int i = 0; i < N; ++i) rph[i] = (float)i / (float)N;
    }

    // Réglages du prochain bloc : macros appliquées ici, cibles de lissage mises à jour.
    void set(const Params& pr) {
        P = pr;
        const float g = clamp01(P.mGrain), m = clamp01(P.mMove), s = clamp01(P.mSpace), c = clampf(P.mCol, -1.f, 1.f);
        // filtre
        float cut = clampf(P.fCut, 20.f, 20000.f), env = clamp01(P.fEnv + 0.5f * m);
        envAmt = env;
        smCut.t = std::log2(std::min(cut, 0.45f * sr));
        const float q = 0.6f + clamp01(P.fRes) * clamp01(P.fRes) * 14.f;
        smRes.t = 1.f / q;
        fComp = 1.f / (1.f + 0.04f * (q - 0.6f));
        const bool open = P.fMode == 0 && cut >= 19900.f && env <= 0.001f;
        smFilt.t = (P.fOn && !open) ? 1.f : 0.f;
        // saturation
        const float drive = clampf(P.sDrive + 18.f * g, 0.f, 36.f);
        sGain = db2lin(drive); sType = P.sType;
        sComp = 0.6f / shape(sGain * 0.6f, sType);
        smDrive.t = sGain;
        smSMix.t = P.sOn ? clamp01(P.sMix + g) : 0.f;
        smSTone.t = P.sTone;
        // chorus
        smCMix.t = P.cOn ? clamp01(P.cMix + 0.6f * m) : 0.f;
        smCDepth.t = clamp01(P.cDepth + 0.4f * m);
        cRate = clampf(P.cRate, 0.02f, 8.f); cSpread = clamp01(P.cSpread);
        // delay
        float dSamples = (float)(60.0 / std::max(20.0, std::min(300.0, P.bpm))) * delayBeats(P.dTime) * sr;
        dSamples = clampf(dSamples, 16.f, 3.f * sr);
        smDTime.t = dSamples;
        smDFb.t = clampf(P.dFb, 0.f, 0.95f);
        smDTone.t = P.dTone;
        smDMix.t = P.dOn ? clamp01(P.dMix + 0.45f * s) : 0.f;
        dPing = P.dPing;
        // réverbe
        smRMix.t = P.rOn ? clamp01(P.rMix + 0.6f * s) : 0.f;
        rSizeK = 0.6f + clamp01(P.rSize) * 1.0f;
        rt60 = clampf(P.rDecay * (1.f + 0.6f * s), 0.2f, 20.f);
        rPreSamples = clampf(P.rPre, 0.f, 120.f) * 0.001f * sr;
        rDampA = 1.f - std::exp(-2.f * kPi * (14000.f * std::pow(0.1f, clamp01(P.rDamp))) / sr);
        // inclinaison Couleur
        smTiltL.t = db2lin(-c * 9.f); smTiltH.t = db2lin(c * 9.f);
        smMix.t = clamp01(P.mix); smGain.t = db2lin(P.gainDb);
        // gain de boucle de la réverbe selon la longueur courante des lignes
        for (int i = 0; i < N; ++i) { lineG[i] = std::pow(10.f, -3.f * (lenNow[i] / sr) / rt60); }
    }
    void snapAll() {
        for (auto* s : {&smCut, &smRes, &smFilt, &smDrive, &smSMix, &smSTone, &smCMix, &smCDepth, &smDMix, &smDFb, &smDTone, &smDTime, &smRMix, &smTiltL, &smTiltH, &smMix, &smGain}) s->snap(s->t);
        const float k = sr / 48000.f;
        for (int i = 0; i < N; ++i) lenNow[i] = kBase[i] * rSizeK * k;
        for (int i = 0; i < N; ++i) lineG[i] = std::pow(10.f, -3.f * (lenNow[i] / sr) / rt60);
    }

    // Traite `n` échantillons stéréo sur place (l et r valides, finis).
    void process(float* l, float* r, int n) {
        const float k48 = sr / 48000.f;
        const float envAtt = 1.f - std::exp(-1.f / (0.004f * sr)), envRel = 1.f - std::exp(-1.f / (0.12f * sr));
        const float tiltA = 1.f - std::exp(-2.f * kPi * 900.f / sr);
        const float lfoInc = cRate / sr;
        const float hpA = 1.f - std::exp(-2.f * kPi * 90.f / sr), dcA = 1.f - std::exp(-2.f * kPi * 12.f / sr);
        const float lenSlew = 1.f - std::exp(-1.f / (0.08f * sr));
        for (int i = 0; i < n; ++i) {
            const float dryL = l[i], dryR = r[i];
            float xl = dryL, xr = dryR;
            // ---- filtre
            const float mono = 0.5f * (std::fabs(xl) + std::fabs(xr));
            env += (mono > env ? envAtt : envRel) * (mono - env);
            const float fcl = std::exp2(smCut.next() + envAmt * clamp01(env * 4.f) * 4.f);
            const float fc = clampf(fcl, 20.f, 0.45f * sr);
            const float kk = smRes.next(), gg = std::tan(kPi * fc / sr);
            const float a1 = 1.f / (1.f + gg * (gg + kk)), a2 = gg * a1, a3 = gg * a2;
            const float fw = smFilt.next();
            { float* xs[2] = {&xl, &xr};
              for (int c = 0; c < 2; ++c) {
                  const float x = *xs[c]; Svf& s = svf[c];
                  const float v3 = x - s.s2, v1 = a1 * s.s1 + a2 * v3, v2 = s.s2 + a2 * s.s1 + a3 * v3;
                  s.s1 = 2.f * v1 - s.s1; s.s2 = 2.f * v2 - s.s2;
                  const float y = (P.fMode == 0 ? v2 : P.fMode == 1 ? kk * v1 : x - kk * v1 - v2) * fComp;
                  *xs[c] = x + (y - x) * fw;
              } }
            // ---- saturation (en parallèle selon smSMix ; état toujours chaud)
            const float sm = smSMix.next(), gs = smDrive.next(), tn = smSTone.next();
            const float toneA = tn > 0.995f ? 1.f : 1.f - std::exp(-2.f * kPi * (800.f * std::pow(25.f, tn)) / sr);
            { float* xs[2] = {&xl, &xr};
              for (int c = 0; c < 2; ++c) {
                  const float x = *xs[c];
                  float u = gs * x; if (sType == 2) u += kBias;
                  float y = adaa(u, satX1[c], sType); satX1[c] = u;
                  if (sType == 2) { y -= std::tanh(kBias); }
                  y *= sComp;
                  if (sType == 2) { const float o = y - satDcIn[c] + (1.f - dcA) * satDc[c]; satDcIn[c] = y; satDc[c] = o; y = o; }   // retire le décalage continu
                  satTone[c] += toneA * (y - satTone[c]);
                  *xs[c] = x + (satTone[c] - x) * sm;
              } }
            // ---- chorus / ensemble
            const float cm = smCMix.next(), cd = smCDepth.next();
            lfo += lfoInc; if (lfo >= 1.f) lfo -= 1.f;
            { float wet[2];
              chor[0].push(xl); chor[1].push(xr);
              for (int c = 0; c < 2; ++c) {
                  float acc = 0.f;
                  const float ph0 = lfo + (c ? 0.5f * cSpread : 0.f);
                  for (int v = 0; v < 3; ++v) {
                      float ph = ph0 + (float)v * (1.f / 3.f); ph -= std::floor(ph);
                      const float ms = 9.f + 2.f * (float)v + cd * 5.5f * std::sin(2.f * kPi * ph);   // 9 ms +- 5,5 ms au plus
                      acc += chor[c].read(ms * 0.001f * sr);
                  }
                  wet[c] = acc * (1.f / 3.f) * 1.05f;
              }
              xl += (wet[0] - xl) * cm; xr += (wet[1] - xr) * cm; }
            // ---- delay
            const float dm = smDMix.next(), fb = smDFb.next(), dtone = smDTone.next();
            const float dT = smDTime.next();
            const float dLpA = 1.f - std::exp(-2.f * kPi * (700.f * std::pow(20.f, dtone)) / sr);
            { const float yl = dly[0].read(dT - 1.f), yr = dly[1].read(dT - 1.f);
              float f[2] = {yl, yr};
              for (int c = 0; c < 2; ++c) {   // boucle : passe-bas (Ton), passe-haut léger, limiteur doux
                  dlyLp[c] += dLpA * (f[c] - dlyLp[c]);
                  float y = dlyLp[c];
                  const float h = y - dlyHpIn[c] + (1.f - hpA) * dlyHp[c]; dlyHpIn[c] = y; dlyHp[c] = h;
                  f[c] = std::tanh(h * fb);
              }
              if (dPing == 0) { dly[0].push(xl + f[0]); dly[1].push(xr + f[1]); }
              else { dly[0].push(0.5f * (xl + xr) + f[1]); dly[1].push(f[0]); }
              xl += dm * yl; xr += dm * yr; }
            // ---- réverbe
            const float rm = smRMix.next();
            { const float inMono = 0.5f * (xl + xr);
              pre.push(inMono);
              float d = pre.read(std::max(1.f, rPreSamples));
              static const float apG[4] = {0.7f, 0.7f, 0.6f, 0.6f};
              for (int a = 0; a < 4; ++a) { const float z = ap[a].at(apLen[a] - 1); const float v = d - apG[a] * z; ap[a].push(v); d = z + apG[a] * v; }
              float y[N], s = 0.f;
              for (int j = 0; j < N; ++j) {
                  lenNow[j] += (kBase[j] * rSizeK * k48 - lenNow[j]) * lenSlew;
                  rph[j] += kRate[j] / sr; if (rph[j] >= 1.f) rph[j] -= 1.f;
                  const float md = lenNow[j] + 5.f * k48 * std::sin(2.f * kPi * rph[j]);
                  float v = line[j].read(std::max(2.f, md - 1.f)) * lineG[j];
                  lineLp[j] += rDampA * (v - lineLp[j]);
                  y[j] = lineLp[j]; s += y[j];
              }
              const float hh = s * (2.f / (float)N);
              for (int j = 0; j < N; ++j) line[j].push(y[j] - hh + d * kInSign[j] * 0.5f);
              float oL = 0.f, oR = 0.f;
              for (int j = 0; j < N; ++j) { oL += kOutL[j] * y[j]; oR += kOutR[j] * y[j]; }
              xl += rm * oL * 0.5f; xr += rm * oR * 0.5f; }
            // ---- couleur (inclinaison) : x = lp + hp, gains par bande ; 1 = transparent
            { const float gl = smTiltL.next(), gh = smTiltH.next();
              float* xs[2] = {&xl, &xr};
              for (int c = 0; c < 2; ++c) { tiltLp[c] += tiltA * (*xs[c] - tiltLp[c]); *xs[c] = tiltLp[c] * gl + (*xs[c] - tiltLp[c]) * gh; } }
            // ---- mix et sortie
            const float mx = smMix.next(), go = smGain.next();
            xl = (dryL + (xl - dryL) * mx) * go; xr = (dryR + (xr - dryR) * mx) * go;
            l[i] = finiteF(xl) ? xl : 0.f; r[i] = finiteF(xr) ? xr : 0.f;
        }
        // un NaN dans une mémoire récursive empoisonnerait la suite : on remet tout à zéro si cela arrive
        if (!finiteF(env) || !finiteF(svf[0].s1) || !finiteF(svf[0].s2) || !finiteF(svf[1].s1) || !finiteF(svf[1].s2) || !finiteF(dlyLp[0]) || !finiteF(dlyLp[1]) || !finiteF(lineLp[0]) || !finiteF(lineLp[7])) reset();
    }

private:
    static constexpr int N = 8;
    static constexpr float kBias = 0.35f;
    static inline const float kBase[N] = {1117.f, 1327.f, 1543.f, 1747.f, 1931.f, 2113.f, 2339.f, 2531.f};   // premiers entre eux, à 48 kHz
    static inline const float kRate[N] = {0.11f, 0.13f, 0.17f, 0.19f, 0.23f, 0.29f, 0.31f, 0.37f};
    static inline const float kInSign[N] = {1.f, -1.f, 1.f, 1.f, -1.f, 1.f, -1.f, -1.f};
    static inline const float kOutL[N] = {1.f, 1.f, -1.f, -1.f, 1.f, 1.f, -1.f, -1.f};
    static inline const float kOutR[N] = {1.f, -1.f, 1.f, -1.f, -1.f, 1.f, -1.f, 1.f};

    static float shape(float x, int type) {
        if (type == 1) return clampf(x, -1.f, 1.f);
        if (type == 2) return std::tanh(x + kBias) - std::tanh(kBias);
        return std::tanh(x);
    }
    // primitive de la non-linéarité : tanh -> ln cosh ; écrêtage dur -> x^2/2 puis |x| - 1/2
    static float prim(float u, int type) {
        const float a = std::fabs(u);
        if (type == 1) return a <= 1.f ? 0.5f * u * u : a - 0.5f;
        return a + std::log1p(std::exp(-2.f * a)) - 0.69314718f;   // ln cosh(u), stable pour les grands |u|
    }
    // anti-repliement du 1er ordre : (F(u) - F(u1)) / (u - u1)
    static float adaa(float u, float u1, int type) {
        const float d = u - u1;
        if (std::fabs(d) < 1e-4f) return type == 1 ? clampf(0.5f * (u + u1), -1.f, 1.f) : std::tanh(0.5f * (u + u1));
        return (prim(u, type) - prim(u1, type)) / d;
    }

    struct Svf { float s1 = 0.f, s2 = 0.f; };
    float sr = 48000.f;
    Params P;
    Svf svf[2]; float env = 0.f, envAmt = 0.f, fComp = 1.f;
    float satX1[2] = {}, satTone[2] = {}, satDc[2] = {}, satDcIn[2] = {}, sGain = 1.f, sComp = 1.f; int sType = 0;
    Ring chor[2]; float lfo = 0.f, cRate = 0.6f, cSpread = 0.8f;
    Ring dly[2]; float dlyLp[2] = {}, dlyHp[2] = {}, dlyHpIn[2] = {}; int dPing = 0;
    Ring line[N]; float lenNow[N] = {}, lineG[N] = {}, lineLp[N] = {}, rph[N] = {}; Ring pre; Ring ap[4]; int apLen[4] = {2, 2, 2, 2};
    float rSizeK = 1.f, rt60 = 2.f, rPreSamples = 0.f, rDampA = 0.5f;
    float tiltLp[2] = {};
    Sm smCut, smRes, smFilt, smDrive, smSMix, smSTone, smCMix, smCDepth, smDMix, smDFb, smDTone, smDTime, smRMix, smTiltL, smTiltH, smMix, smGain;
};

}  // namespace ofx
