// Briques communes de la chaîne actuelle des plugins un-potard (C++17 pur, sans JUCE, sans allocation dans process).
//  - Curve        : courbe de macro à points d'arrêt, interpolation de Hermite monotone (Fritsch-Carlson) : plus aucun int(...) sur la course.
//  - Bq           : biquad RBJ (coefficients calculés en double), formes « différence » exactes à 0.
//  - AdaaTanh / AdaaTriode : non-linéarités sans mémoire avec antidérivée (ADAA d'ordre 1), sans latence.
//  - VoiceDetector: détecteur de voix à seuil ADAPTATIF (plancher de bruit suivi par minima) + auto-entrée figée hors voix.
//  - PauseGate    : porte de pause : hors voix, le bruit de fond ne sort jamais plus fort qu'il n'est entré.
//  - PeakLimiter  : limiteur de crête de sécurité, gain exactement 1 sous -1 dBFS.
//  - KeyTracker   : suivi de tonalité (profils de Krumhansl, verrouillage avec confiance), porté tel quel du moteur voile::Engine.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <utility>
#include "../Dsp/Saturation.h"

namespace onek {

constexpr float kPi = 3.14159265358979f;
inline float dbToLin(float d) { return std::exp(d * 0.11512925464970228f); }
inline float linToDb(float x) { return 8.685889638065035f * std::log(std::max(x, 1e-9f)); }
inline float clamp01(float v) { return v < 0.f ? 0.f : v > 1.f ? 1.f : v; }
inline float smoothstep(float a, float b, float x) { const float t = clamp01((x - a) / (b - a)); return t * t * (3.f - 2.f * t); }

// ------------------------------------------------------------------ courbe de macro
struct Curve {
    static constexpr int kMax = 8;
    int n = 0;
    float x[kMax] = {}, y[kMax] = {}, m[kMax] = {};
    Curve(std::initializer_list<std::pair<float, float>> pts) {
        for (const auto& p : pts) if (n < kMax) { x[n] = p.first; y[n] = p.second; ++n; }
        float d[kMax] = {};
        for (int i = 0; i + 1 < n; ++i) d[i] = (y[i + 1] - y[i]) / std::max(1e-6f, x[i + 1] - x[i]);
        for (int i = 0; i < n; ++i) {
            if (i == 0) m[i] = n > 1 ? d[0] : 0.f;
            else if (i == n - 1) m[i] = d[n - 2];
            else m[i] = (d[i - 1] * d[i] <= 0.f) ? 0.f : 0.5f * (d[i - 1] + d[i]);
        }
        for (int i = 0; i + 1 < n; ++i) {   // Fritsch-Carlson : aucune bosse entre deux points (monotone par morceaux)
            if (d[i] == 0.f) { m[i] = m[i + 1] = 0.f; continue; }
            const float a = m[i] / d[i], b = m[i + 1] / d[i], s = a * a + b * b;
            if (s > 9.f) { const float t = 3.f / std::sqrt(s); m[i] = t * a * d[i]; m[i + 1] = t * b * d[i]; }
        }
    }
    float operator()(float k) const {
        if (n == 0) return 0.f;
        if (k <= x[0]) return y[0];
        if (k >= x[n - 1]) return y[n - 1];
        int i = 0; while (i + 2 < n && k > x[i + 1]) ++i;
        const float h = x[i + 1] - x[i], t = (k - x[i]) / h, t2 = t * t, t3 = t2 * t;
        return (2 * t3 - 3 * t2 + 1) * y[i] + (t3 - 2 * t2 + t) * h * m[i] + (-2 * t3 + 3 * t2) * y[i + 1] + (t3 - t2) * h * m[i + 1];
    }
};

// ------------------------------------------------------------------ biquad
struct Bq {
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void reset() { z1 = z2 = 0; }
    float run(float x) { const float y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
    void set(double nb0, double nb1, double nb2, double na0, double na1, double na2) { b0 = (float)(nb0 / na0); b1 = (float)(nb1 / na0); b2 = (float)(nb2 / na0); a1 = (float)(na1 / na0); a2 = (float)(na2 / na0); }
    static double w0(double sr, double f) { return 2.0 * 3.14159265358979323846 * std::min(f, 0.45 * sr) / sr; }
    void lp(double sr, double f, double q = 0.7071) { const double w = w0(sr, f), c = std::cos(w), a = std::sin(w) / (2 * q); set((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + a, -2 * c, 1 - a); }
    void hp(double sr, double f, double q = 0.7071) { const double w = w0(sr, f), c = std::cos(w), a = std::sin(w) / (2 * q); set((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + a, -2 * c, 1 - a); }
    void bp(double sr, double f, double q) { const double w = w0(sr, f), c = std::cos(w), a = std::sin(w) / (2 * q); set(a, 0, -a, 1 + a, -2 * c, 1 - a); }   // crête 0 dB
    void peak(double sr, double f, double q, double db) {
        const double A = std::pow(10.0, db / 40.0), w = w0(sr, f), c = std::cos(w), a = std::sin(w) / (2 * q);
        set(1 + a * A, -2 * c, 1 - a * A, 1 + a / A, -2 * c, 1 - a / A);
    }
    void lowShelf(double sr, double f, double db) {
        const double A = std::pow(10.0, db / 40.0), w = w0(sr, f), c = std::cos(w), s = std::sin(w), al = s / 2 * std::sqrt(2.0), t = 2 * std::sqrt(A) * al;
        set(A * ((A + 1) - (A - 1) * c + t), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - t), (A + 1) + (A - 1) * c + t, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - t);
    }
    void highShelf(double sr, double f, double db) {
        const double A = std::pow(10.0, db / 40.0), w = w0(sr, f), c = std::cos(w), s = std::sin(w), al = s / 2 * std::sqrt(2.0), t = 2 * std::sqrt(A) * al;
        set(A * ((A + 1) + (A - 1) * c + t), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - t), (A + 1) - (A - 1) * c + t, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - t);
    }
};

// Plateau à gain variable sans marche : recalculé seulement quand le gain bouge de plus de 0,02 dB ; 0 dB = sortie = entrée (sauté).
struct Shelf {
    Bq f; float cur = 0.f, sr = 48000.f, hz = 1000.f; bool low = false;
    void prepare(double s, float freq, bool isLow) { sr = (float)s; hz = freq; low = isLow; cur = 0.f; f = Bq{}; }
    void reset() { f.reset(); }
    void setDb(float db) {
        if (std::fabs(db - cur) < 0.02f && !(db == 0.f && cur != 0.f)) return;
        cur = db; if (low) f.lowShelf(sr, hz, db); else f.highShelf(sr, hz, db);
    }
    float run(float x) { return cur == 0.f ? (f.reset(), x) : f.run(x); }
};

// ------------------------------------------------------------------ non-linéarités ADAA (ordre 1, sans latence)
inline double lnCoshD(double x) {
    x = std::fabs(x);
    if (x < 0.5) { const double x2 = x * x; return x2 * (0.5 - x2 * (1.0 / 12.0 - x2 * (1.0 / 45.0 - x2 * (17.0 / 2520.0)))); }
    return x + std::log1p(std::exp(-2.0 * x)) - 0.6931471805599453;
}
// tanh(x + b) - tanh(b) : biais b pour la 2e harmonique.
struct AdaaTanh {
    double xp = 0, Fp = 0, b = 0, tb = 0;
    void reset() { xp = 0; Fp = lnCoshD(b); }
    void setBias(double nb) { if (nb != b) { b = nb; tb = std::tanh(b); Fp = lnCoshD(xp + b) - tb * xp; } }
    float run(float xf) {
        const double x = xf, F = lnCoshD(x + b) - tb * x, dx = x - xp;
        const double y = std::fabs(dx) > 1e-5 ? (F - Fp) / dx : std::tanh(0.5 * (x + xp) + b) - tb;
        xp = x; Fp = F;
        return (float)y;
    }
};
// Triode 12AX7 (table de Koren de Dsp/Saturation.h, pente 1 au repos, harmoniques paires).
struct AdaaTriode {
    double xp = 0, Fp = 0;
    void reset() { xp = 0; Fp = dsp::sat::TriodeTable::get().prim(0.0); }
    float run(float xf) {
        const auto& T = dsp::sat::TriodeTable::get();
        const double x = xf, F = T.prim(x), dx = x - xp;
        const double y = std::fabs(dx) > 1e-5 ? (F - Fp) / dx : T.eval(0.5 * (x + xp));
        xp = x; Fp = F;
        return (float)y;
    }
};

// ------------------------------------------------------------------ détecteur de voix + auto-entrée
// Mesure par tranche (<= 32 échantillons). Plancher de bruit : suit les minima (descente rapide, montée lente), donc le seuil
// « voix » (plancher + 12 dB, au moins -70 dBFS) s'adapte à la pièce. L'auto-entrée vise -18 dBFS RMS sur la voix seulement :
// gain borné à ±15 dB, vitesse <= 6 dB/s, et FIGÉ hors voix (le bruit entre les phrases ne fait plus monter le gain).
struct VoiceDetector {
    float sr = 48000.f, ms = 0.f, msC = 0.f, nf = -60.f, lvP = 0.0158f, gDb = 0.f, gLin = 1.f, gStep = 0.f;
    float holdS = 0.f, voiceT = 0.f, eDb = -120.f;
    bool active = false, open = false;
    static constexpr float kTarget = -18.f;
    void prepare(double s) { sr = (float)s; msC = 1.f - std::exp(-1.f / (0.020f * sr)); reset(); }
    void reset() { ms = 0.f; nf = -60.f; lvP = dbToLin(kTarget) * dbToLin(kTarget); gDb = 0.f; gLin = 1.f; gStep = 0.f; holdS = 0.f; voiceT = 0.f; eDb = -120.f; active = open = false; }
    void accumulate(float x) { ms += (x * x - ms) * msC; }
    // fin de tranche de n échantillons : décisions et nouvelle rampe de gain pour la tranche suivante
    void update(int n, bool autoOn) {
        const float dt = (float)n / sr;
        eDb = 10.f * std::log10(ms + 1e-12f);
        if (eDb < nf) nf += (eDb - nf) * (1.f - std::exp(-dt / 0.2f));
        else nf += (eDb < nf + 15.f ? 3.f : 0.3f) * dt;   // remonte vite vers un bruit proche, très lentement sous la voix
        nf = std::clamp(std::min(nf, std::max(eDb, -100.f)), -100.f, -20.f);
        open = eDb > std::max(nf + 12.f, -70.f);
        if (open) holdS = 0.15f; else holdS = std::max(0.f, holdS - dt);
        active = open || holdS > 0.f;
        if (open) {
            voiceT += dt;
            const float tau = voiceT < 0.6f ? 0.3f : 2.f;
            lvP += (ms - lvP) * (1.f - std::exp(-dt / tau));
        }
        float tgt = autoOn ? std::clamp(kTarget - 10.f * std::log10(lvP + 1e-12f), -15.f, 15.f) : 0.f;
        if (autoOn && !open) tgt = gDb;                          // figé hors voix
        const float maxStep = 6.f * dt;
        const float nd = gDb + std::clamp(tgt - gDb, -maxStep, maxStep);
        const float nl = dbToLin(nd);
        gStep = (nl - gLin) / (float)std::max(1, n);
        gDb = nd;
    }
    float nextGain() { gLin += gStep; return gLin; }   // rampe linéaire (aucune marche)
    void settle() { gLin = dbToLin(gDb); gStep = 0.f; }
};

// ------------------------------------------------------------------ porte de pause
// Hors voix, ramène le chemin traité au niveau du bruit d'entrée (jamais d'amplification du bruit de fond) ; sur la voix : gain 1.
struct PauseGate {
    float pin = 0.f, pout = 0.f, g = 1.f, cP = 0.f, cOpen = 0.f, cClose = 0.f;
    void prepare(double sr) { cP = 1.f - std::exp(-1.f / (0.08f * (float)sr)); cOpen = 1.f - std::exp(-1.f / (0.003f * (float)sr)); cClose = 1.f - std::exp(-1.f / (0.06f * (float)sr)); reset(); }
    void reset() { pin = pout = 0.f; g = 1.f; }
    // in : signal d'entrée (même échelle que out), out : chemin traité avant la porte
    float step(float in, float out, bool voice) {
        if (voice) { g += (1.f - g) * cOpen; return g; }
        pin += (in * in - pin) * cP; pout += (out * out - pout) * cP;
        const float t = pout > pin && pout > 1e-14f ? std::sqrt(pin / pout) : 1.f;
        g += (t - g) * (t < g ? cClose : cOpen);
        return g;
    }
};

// ------------------------------------------------------------------ limiteur de crête de sécurité
struct PeakLimiter {
    float g = 1.f, rel = 0.f, thr = 0.891250938f;   // -1 dBFS
    void prepare(double sr) { rel = 1.f - std::exp(-1.f / (0.08f * (float)sr)); g = 1.f; }
    void reset() { g = 1.f; }
    void run(float& l, float& r) {
        const float a = std::max(std::fabs(l), std::fabs(r));
        if (a * g > thr) g = thr / a;
        else if (g < 1.f) { g += (1.f - g) * rel; if (g > 0.99999f) g = 1.f; if (a * g > thr) g = thr / a; }
        if (g < 1.f) { l *= g; r *= g; }
    }
};

// ------------------------------------------------------------------ tonalité
inline bool allowedNote(int midi, int key, int scale) {
    if (scale == 0) return true;
    static const bool maj[12] = {1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1};
    static const bool mnr[12] = {1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 0};
    return (scale == 1 ? maj : mnr)[((midi - key) % 12 + 12) % 12];
}
inline int maskOf(int key, int sc) { int m = 0; for (int i = 0; i < 12; ++i) if (allowedNote(i, key, sc)) m |= 1 << i; return m; }
inline int nearestAllowed(float midi, int key, int scale) {
    const int base = (int)std::lround(midi); int best = base; float bd = 1e9f;
    for (int o = -2; o <= 2; ++o) { const int n = base + o; if (!allowedNote(n, key, scale)) continue; const float d = std::fabs(midi - (float)n); if (d < bd) { bd = d; best = n; } }
    return best;
}

// Suivi de tonalité : copie de la logique fiable de voile::Engine (notes STABLES seulement, mémoire ~8 s, verrouillage si
// matière, variété, accord avec la gamme et marge nette ; hystérésis ; mémoire courte qui suspend la correction si ça ne colle plus).
struct KeyTracker {
    bool autoKey = true, autoScale = true; int key = 0, scale = 1;
    float hist[12] = {}, histWeight = 0.f, fastHist[12] = {}, fastW = 0.f, stC = 0.f, stS = 0.f, stW = 0.f, tuneOff = 0.f;
    int keyTick = 0, effKey = 0, effScale = 1, confirm = 0, switchCnt = 0, lostCnt = 0, candK = -1, candS = -1;
    bool haveKey = false, stale = false;
    void reset() {
        for (auto& v : hist) v = 0.f;
        for (auto& v : fastHist) v = 0.f;
        histWeight = fastW = stC = stS = stW = tuneOff = 0.f; keyTick = confirm = switchCnt = lostCnt = 0; candK = candS = -1;
        haveKey = false; stale = false; apply();
    }
    // keyChoice 0 = Auto, 1..12 = C..B ; scaleChoice 0 = Auto, 1 = Majeur, 2 = Mineur
    void select(int keyChoice, int scaleChoice) {
        const bool ak = keyChoice == 0, as = scaleChoice == 0; const int k = ak ? key : keyChoice - 1, s = as ? scale : scaleChoice;
        const bool changed = ak != autoKey || as != autoScale || (!ak && k != key) || (!as && s != scale);
        autoKey = ak; autoScale = as; key = k; scale = s;
        if (changed) { haveKey = false; confirm = switchCnt = lostCnt = 0; candK = candS = -1; }
        apply();
    }
    bool detecting() const { return autoKey || autoScale; }
    int status() const { return detecting() ? (haveKey ? 1 : 2) : 0; }
    bool usable() const { return haveKey && !stale; }
    void apply() {
        if (!detecting()) { effKey = key; effScale = scale; haveKey = true; }
        else if (!haveKey) { effKey = autoKey ? effKey : key; if (!autoScale) effScale = scale; }
    }
    // hauteur stable (médiane de 3 trames, MIDI) -> hauteur d'identification (calage global de la prise retiré)
    float feedStable(float m) {
        const float a = 2.f * kPi * m;
        stC = stC * 0.9995f + std::cos(a); stS = stS * 0.9995f + std::sin(a); stW = stW * 0.9995f + 1.f;
        const float R = std::sqrt(stC * stC + stS * stS) / std::max(1.f, stW);
        tuneOff = (stW > 150.f && R > 0.6f) ? std::clamp(std::atan2(stS, stC) / (2.f * kPi), -0.4f, 0.4f) : 0.f;
        const float idM = m - tuneOff;
        if (detecting()) {
            track(idM);
            for (auto& v : fastHist) v *= 0.996f;
            fastHist[((int)std::lround(idM) % 12 + 12) % 12] += 1.f; fastW = fastW * 0.996f + 1.f;
            if (haveKey && fastW >= 60.f) {
                float in = 0; const int mk = maskOf(effKey, effScale);
                for (int i = 0; i < 12; ++i) if (mk & (1 << i)) in += fastHist[i];
                const float fit = in / fastW;
                if (fit < 0.7f) stale = true; else if (fit >= 0.8f) stale = false;
            } else if (!haveKey) stale = false;
        } else stale = false;
        return idM;
    }
private:
    float corrKey(const float* prof, int k) const {
        float mh = 0, mp = 0; for (int i = 0; i < 12; ++i) { mh += hist[i]; mp += prof[i]; } mh /= 12; mp /= 12;
        float num = 0, dh = 0, dp = 0;
        for (int i = 0; i < 12; ++i) { const float a = hist[(i + k) % 12] - mh, b = prof[i] - mp; num += a * b; dh += a * a; dp += b * b; }
        return num / (std::sqrt(dh * dp) + 1e-9f);
    }
    float fitOf(int mask) const { float in = 0, tot = 0; for (int i = 0; i < 12; ++i) { tot += hist[i]; if (mask & (1 << i)) in += hist[i]; } return tot > 0.f ? in / tot : 0.f; }
    void track(float idMidi) {
        for (auto& v : hist) v *= 0.999f;
        hist[((int)std::lround(idMidi) % 12 + 12) % 12] += 1.f; histWeight = histWeight * 0.999f + 1.f;
        if (++keyTick < 96) return;
        keyTick = 0;
        static const float maj[12] = {6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f, 2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f};
        static const float mnr[12] = {6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f, 2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f};
        struct C { int k, sc; float c; int m; } cand[24]; int nc = 0;
        for (int k = 0; k < 12; ++k) {
            if (!autoKey && k != key) continue;
            for (int sc = 1; sc <= 2; ++sc) { if (!autoScale && sc != scale) continue; cand[nc++] = {k, sc, corrKey(sc == 1 ? maj : mnr, k), maskOf(k, sc)}; }
        }
        if (nc == 0) return;
        int bi = 0; for (int i = 1; i < nc; ++i) if (cand[i].c > cand[bi].c) bi = i;
        float second = -2.f; for (int i = 0; i < nc; ++i) if (cand[i].m != cand[bi].m && cand[i].c > second) second = cand[i].c;
        if (second < -1.5f) second = cand[bi].c - 1.f;
        float tot = 0; for (int i = 0; i < 12; ++i) tot += hist[i];
        int npc = 0; for (int i = 0; i < 12; ++i) if (hist[i] >= 0.03f * tot) ++npc;
        const bool reliable = histWeight >= 220.f && npc >= 4 && cand[bi].c >= 0.6f && fitOf(cand[bi].m) >= 0.9f && cand[bi].c - second >= 0.05f;
        if (!haveKey) {
            if (reliable && cand[bi].k == candK && cand[bi].sc == candS) ++confirm; else confirm = reliable ? 1 : 0;
            candK = reliable ? cand[bi].k : -1; candS = reliable ? cand[bi].sc : -1;
            if (confirm >= 2) { effKey = cand[bi].k; effScale = cand[bi].sc; haveKey = true; confirm = switchCnt = lostCnt = 0; }
            return;
        }
        float cur = -2.f; const int curMask = maskOf(effKey, effScale);
        for (int i = 0; i < nc; ++i) if (cand[i].k == effKey && cand[i].sc == effScale) cur = cand[i].c;
        if (cur < -1.5f) cur = corrKey(effScale == 1 ? maj : mnr, effKey);
        if (reliable && cand[bi].m != curMask && cand[bi].c > cur + 0.08f) {
            if (cand[bi].k == candK && cand[bi].sc == candS) ++switchCnt; else switchCnt = 1;
            candK = cand[bi].k; candS = cand[bi].sc;
            if (switchCnt >= 3) { effKey = candK; effScale = candS; switchCnt = 0; lostCnt = 0; }
        } else { switchCnt = 0; candK = candS = -1; }
        if (fitOf(curMask) < 0.75f) { if (++lostCnt >= 6) { haveKey = false; lostCnt = 0; confirm = 0; for (auto& v : hist) v *= 0.25f; histWeight *= 0.25f; } } else lostCnt = 0;
    }
};

}  // namespace onek
