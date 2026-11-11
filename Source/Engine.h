// Voile : moteur audio. C++17 pur, sans JUCE (testable hors-ligne).
// Chaîne : correction de hauteur dure -> saturation/bitcrush -> doubles -> delay ping-pong + reverb.
// Briques partagées : décaleur dsp::VoiceShifter (Dsp/Splicer.h, formants préservés), doubles dsp::AdtVoice, reverb FDN 8 lignes.
#pragma once
#include <algorithm>
#include <cmath>
#include <vector>
#include "Dsp/Splicer.h"

namespace voile {

constexpr float kPi = 3.14159265358979323846f;

struct Params {
    float tension = 0.7f, mordant = 0.3f, brume = 0.3f, halo = 0.3f;
    float mix = 1.f, outDb = 0.f;
    int key = 0;    // 0 = C ... 11 = B
    int scale = 1;  // 0 chromatique, 1 majeur, 2 mineur
    bool autoKey = false;    // détecte la tonique depuis la voix (sinon `key` est imposée)
    bool autoScale = false;  // détecte majeur/mineur (sinon `scale` est imposée)
    // Mode sûr (plugins un-potard) : aucune correction vers une note tant que la tonalité n'est pas fiable,
    // correction seulement sur une hauteur stable, zone morte, note « accidentelle » respectée en auto.
    bool safe = false;
    float deadCents = 0.f;   // zone morte autour de la note (cents)
    float retuneMs = -1.f;   // constante de temps de la correction ; < 0 : dérivée de `tension`
    bool formants = true;    // décaleur à formants préservés (sinon le timbre suit la hauteur)
    // Options « v2 » de Voile (désactivées par défaut : les autres plugins et les anciens projets gardent exactement l'ancien son).
    bool mordComp = false;     // Mordant compensé en niveau : le traité garde le niveau de l'entrée (gain lent, ~400 ms)
    bool haloStereo = false;   // Halo réellement stéréo : un double par côté, graves (< 250 Hz) laissés au centre
};

inline bool allowedNote(int midi, int key, int scale) {
    if (scale == 0) return true;
    static const bool maj[12] = {1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1};
    static const bool mnr[12] = {1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 0};
    const int pc = ((midi - key) % 12 + 12) % 12;
    return (scale == 1 ? maj : mnr)[pc];
}

inline int nearestAllowed(float midi, int key, int scale) {
    const int base = (int)std::lround(midi);
    int best = base;
    float bd = 1e9f;
    for (int o = -2; o <= 2; ++o) {
        const int n = base + o;
        if (!allowedNote(n, key, scale)) continue;
        const float d = std::fabs(midi - (float)n);
        if (d < bd) { bd = d; best = n; }
    }
    return best;
}

// Ligne à retard circulaire, lecture interpolée. read(d) = d échantillons derrière le plus récent.
struct Delay {
    std::vector<float> b;
    int w = 0;
    void init(int n) { b.assign((size_t)n + 2, 0.f); w = 0; }
    void push(float x) { b[(size_t)w] = x; if (++w >= (int)b.size()) w = 0; }
    float read(float d) const {
        const int n = (int)b.size();
        float p = (float)(w - 1) - d;
        if (p < 0.f) p += (float)n;
        int i0 = (int)p;
        const float f = std::min(1.f, std::max(0.f, p - (float)i0));   // fraction prise AVANT le repli : si p arrondi vaut n, f = 0 (et non n : pic x n)
        if (i0 >= n) i0 -= n;
        if (i0 < 0) i0 = 0;
        int i1 = i0 + 1;
        if (i1 >= n) i1 = 0;
        return b[(size_t)i0] + (b[(size_t)i1] - b[(size_t)i0]) * f;
    }
    // Lecture interpolée en cubique (Catmull-Rom) : moins d'atténuation des aigus que la lecture linéaire. d >= 1 exigé.
    float readCubic(float d) const {
        const int n = (int)b.size();
        if (d < 1.f) d = 1.f;
        float p = (float)(w - 1) - d;
        while (p < 0.f) p += (float)n;
        int i = (int)p;
        const float f = p - (float)i;
        auto at = [&](int k) { k %= n; return b[(size_t)k]; };
        const float y0 = at(i - 1 + n), y1 = at(i), y2 = at(i + 1), y3 = at(i + 2);
        const float c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * f + c2) * f + c1) * f + y1;
    }
};

// ---------------------------------------------------------------- suréchantillonnage
// Demi-bande FIR à phase linéaire (fenêtre de Kaiser) de N = 2M+1 coefficients, M impair : tous les coefficients aux
// décalages pairs non nuls sont nuls. Montée : 1 échantillon -> 2 (branche « centre » + branche FIR) ; descente : 2 -> 1.
struct HalfbandTaps {
    int M = 0;
    std::vector<float> t;   // t[i] = 2*h[2i-M], i = 0..M (branche FIR) ; le centre pèse 2*h[0] = 1
    static double bessel0(double x) {
        double s = 1.0, term = 1.0;
        for (int k = 1; k < 60; ++k) { term *= (x / (2.0 * k)) * (x / (2.0 * k)); s += term; if (term < 1e-16 * s) break; }
        return s;
    }
    void design(int m, double beta) {
        M = m;
        std::vector<double> h((size_t)(2 * M + 1), 0.0);
        const double pi = 3.14159265358979323846, i0b = bessel0(beta);
        double odd = 0.0;
        for (int k = -M; k <= M; ++k) {
            if (k == 0 || (k % 2) == 0) continue;
            const double w = bessel0(beta * std::sqrt(std::max(0.0, 1.0 - (double)k * k / ((double)M * M)))) / i0b;
            h[(size_t)(k + M)] = std::sin(pi * k / 2.0) / (pi * k) * w;
            odd += h[(size_t)(k + M)];
        }
        const double sc = 0.5 / odd;   // gain continu exactement 1 (centre 0,5 + coefficients impairs 0,5)
        t.assign((size_t)M + 1, 0.f);
        for (int i = 0; i <= M; ++i) t[(size_t)i] = (float)(2.0 * h[(size_t)(2 * i - M + M)] * sc);
    }
};

struct HbUp {
    const HalfbandTaps* tp = nullptr;
    std::vector<float> h;
    int idx = 0, H = 1;
    void init(const HalfbandTaps* p) { tp = p; H = p->M + 1; h.assign((size_t)(2 * H), 0.f); idx = 0; }
    // e = échantillon pair (branche FIR), o = échantillon impair (centre), dans l'ordre du temps
    void process(float in, float& e, float& o) {
        if (--idx < 0) idx = H - 1;
        h[(size_t)idx] = h[(size_t)(idx + H)] = in;
        const float* x = &h[(size_t)idx];
        const float* t = tp->t.data();
        float a0 = 0.f, a1 = 0.f;
        const int M = tp->M;
        for (int i = 0; i + 1 <= M; i += 2) { a0 += t[i] * x[i]; a1 += t[i + 1] * x[i + 1]; }
        e = a0 + a1;
        o = x[(M - 1) / 2];
    }
};
struct HbDown {
    const HalfbandTaps* tp = nullptr;
    std::vector<float> a, b;
    int ia = 0, ib = 0, H = 1, Hb = 1;
    void init(const HalfbandTaps* p) { tp = p; H = p->M + 1; Hb = (p->M + 1) / 2; a.assign((size_t)(2 * H), 0.f); b.assign((size_t)(2 * Hb), 0.f); ia = ib = 0; }
    // (va, vb) : deux échantillons consécutifs du flux rapide (pair puis impair) -> un échantillon
    float process(float va, float vb) {
        if (--ia < 0) ia = H - 1;
        a[(size_t)ia] = a[(size_t)(ia + H)] = va;
        const float* x = &a[(size_t)ia];
        const float* t = tp->t.data();
        float s0 = 0.f, s1 = 0.f;
        const int M = tp->M;
        for (int i = 0; i + 1 <= M; i += 2) { s0 += t[i] * x[i]; s1 += t[i + 1] * x[i + 1]; }
        const float z = 0.5f * (s0 + s1) + 0.5f * b[(size_t)(ib + (M - 1) / 2)];
        if (--ib < 0) ib = Hb - 1;
        b[(size_t)ib] = b[(size_t)(ib + Hb)] = vb;
        return z;
    }
};

// Suréchantillonneur ×2, ×4 ou ×8 (1 à 3 étages en cascade) pour traiter une non-linéarité sans repliement. Le premier étage
// (le plus raide) coupe à 0,42 fs ; les suivants ont des transitions larges (quelques coefficients). Latence entière en
// échantillons de base (retard de phase linéaire), à ajouter à la latence déclarée du plugin.
struct Oversampler {
    static constexpr int kMaxStages = 3;
    HalfbandTaps taps[kMaxStages];
    HbUp up[kMaxStages];
    HbDown dn[kMaxStages];
    std::vector<float> pad[kMaxStages];
    int padPos[kMaxStages] = {}, padLen[kMaxStages] = {};
    int stages = 0, lat = 0;
    std::vector<float> ring;   // entrées récentes : signal sec retardé exactement de `lat` échantillons
    int rpos = 0;
    // Nombre d'étages conseillé : `base` à 44,1/48 kHz, un de moins à 88,2/96 kHz et plus (le spectre y est déjà large).
    static int stagesFor(double sr, int base) { return std::max(1, sr > 60000.0 ? base - 1 : base); }
    void init(int nStages) {
        stages = std::clamp(nStages, 1, kMaxStages);
        static const int Ms[kMaxStages] = {37, 11, 9};
        static const double betas[kMaxStages] = {8.96, 8.96, 8.96};
        double total = 0.0;
        for (int s = 0; s < stages; ++s) {
            taps[s].design(Ms[s], betas[s]);
            up[s].init(&taps[s]); dn[s].init(&taps[s]);
            const int hi = 2 << s;   // facteur de suréchantillonnage de la sortie de cet étage
            padLen[s] = (hi - (2 * Ms[s]) % hi) % hi;
            pad[s].assign((size_t)std::max(1, padLen[s]), 0.f); padPos[s] = 0;
            total += (double)(2 * Ms[s] + padLen[s]) / (double)hi;
        }
        lat = (int)std::lround(total);
        ring.assign((size_t)lat + 1, 0.f); rpos = 0;
    }
    void reset() {
        for (int s = 0; s < stages; ++s) { up[s].init(&taps[s]); dn[s].init(&taps[s]); std::fill(pad[s].begin(), pad[s].end(), 0.f); padPos[s] = 0; }
        std::fill(ring.begin(), ring.end(), 0.f); rpos = 0;
    }
    void pushRing(float x) { ring[(size_t)rpos] = x; if (++rpos >= (int)ring.size()) rpos = 0; }
    // Dernière entrée traitée, retardée de latency() échantillons (exact) : à mélanger avec la sortie de process().
    float dry() const { return ring[(size_t)rpos]; }
    // Chemin sans traitement, de même latence : pousse x et rend le sec retardé.
    float bypass(float x) { pushRing(x); return dry(); }
    int latency() const { return lat; }
    int factor() const { return 1 << stages; }
    template <class F>
    float process(float x, F&& f) {
        pushRing(x);
        float bufA[1 << kMaxStages], bufB[1 << kMaxStages];
        float *a = bufA, *b = bufB;
        a[0] = x;
        int n = 1;
        for (int s = 0; s < stages; ++s) {
            for (int i = 0; i < n; ++i) up[s].process(a[i], b[2 * i], b[2 * i + 1]);
            n *= 2;
            std::swap(a, b);
        }
        for (int i = 0; i < n; ++i) a[i] = f(a[i]);
        for (int s = stages - 1; s >= 0; --s) {
            if (padLen[s] > 0)
                for (int i = 0; i < n; ++i) { const float o = pad[s][(size_t)padPos[s]]; pad[s][(size_t)padPos[s]] = a[i]; if (++padPos[s] >= padLen[s]) padPos[s] = 0; a[i] = o; }
            for (int i = 0; i < n / 2; ++i) b[i] = dn[s].process(a[2 * i], a[2 * i + 1]);
            n /= 2;
            std::swap(a, b);
        }
        return a[0];
    }
};

struct Smooth {
    float v = 0.f, c = 0.001f;
    float next(float t) { v += (t - v) * c; return v; }
};

// ANCIEN décaleur à deux têtes de lecture croisées (fenêtre sin^2). Latence = win/2 (retard du signal sec). Gardé tel quel pour
// le mode Classique/Qualité d'Orbe Autotune (son inchangé) ; mesuré : modulation d'amplitude ~18 %, ne plus l'utiliser ailleurs.
// Décalage nul ou presque (moins de 0,5 cent) : sortie = signal sec retardé de win/2, exactement, et les têtes sont ramenées à
// l'état « une seule tête » (ph = 0) : plus de peigne à décalage nul, et la reprise du décalage part d'une tête alignée sur le sec.
// Entre 0,5 et 3 cents : fondu sec -> décalé.
struct HeadShifter {
    Delay line;
    float ph = 0.f, win = 1536.f;
    void init(int window) { win = (float)window; line.init(window + 4); ph = 0.f; }
    float process(float x, float ratio) {
        line.push(x);
        const float dry = line.read(0.5f * win);
        const float cents = std::fabs(ratio - 1.f) * 1731.234f;
        const float a = std::min(1.f, std::max(0.f, (cents - 0.5f) * 0.4f));
        if (a <= 0.f) { ph = 0.f; return dry; }
        ph += (1.f - ratio) / win;
        ph -= std::floor(ph);
        float p2 = ph + 0.5f;
        if (p2 >= 1.f) p2 -= 1.f;
        const float s1 = std::sin(kPi * ph), s2 = std::sin(kPi * p2);
        const float sh = line.readCubic(ph * win) * s1 * s1 + line.readCubic(p2 * win) * s2 * s2;
        return dry + a * (sh - dry);
    }
};

// Décaleur de hauteur de la suite (Voile, un-potard, Looks, Atelier) : dsp::VoiceShifter = raccords synchrones à la période
// (dsp::Splicer, le décaleur propre d'Orbe Autotune) + formants préservés en option. Décalage < 0,5 cent : sortie = entrée retardée
// de la latence, bit à bit. init(window) garde l'ancienne interface et l'ancienne latence (window/2) ; les formants ne sont actifs
// que si la fréquence d'échantillonnage est donnée. La période de la voix se donne par setPeriod() (défaut : 300 échantillons).
struct Shifter : dsp::VoiceShifter {
    void init(int window, double sampleRate = 0.0) { dsp::VoiceShifter::init(sampleRate > 0.0 ? sampleRate : 48000.0, window / 2, sampleRate > 0.0); }
    void initRest(double sampleRate, int restDelay, bool withFormants) { dsp::VoiceShifter::init(sampleRate, restDelay, withFormants); }
};

// Détecteur de hauteur YIN sur signal décimé.
struct Yin {
    static constexpr int W = 512;
    static constexpr int kMask = 4095;
    int dec = 2, tMax = 340;
    float decSr = 24000.f;
    std::vector<float> ring, seg, cm;
    int head = 0;
    void init(double sr) {
        dec = sr >= 88000.0 ? 4 : 2;
        decSr = (float)(sr / dec);
        tMax = (int)(decSr / 70.f) + 1;
        ring.assign(4096, 0.f);
        head = 0;
        seg.assign((size_t)(W + tMax + 2), 0.f);
        cm.assign((size_t)tMax + 2, 1.f);
    }
    void push(float v) { ring[(size_t)head] = v; head = (head + 1) & kMask; }
    // Renvoie la fréquence en Hz, ou 0 si non voisé.
    float analyze() {
        const int n = W + tMax;
        for (int i = 0; i < n; ++i) seg[(size_t)i] = ring[(size_t)((head - n + i) & kMask)];
        const float* a = &seg[(size_t)tMax];
        double e = 0.0;
        for (int j = 0; j < W; ++j) e += (double)a[j] * a[j];
        if (std::sqrt(e / W) < 0.003) return 0.f;
        float cum = 0.f;
        cm[0] = 1.f;
        for (int tau = 1; tau <= tMax; ++tau) {
            float d = 0.f;
            for (int j = 0; j < W; ++j) { const float v = a[j] - a[j - tau]; d += v * v; }
            cum += d;
            cm[(size_t)tau] = cum > 1e-9f ? d * (float)tau / cum : 1.f;
        }
        const int tMin = std::max(2, (int)(decSr / 900.f));
        int best = -1;
        for (int t = tMin; t < tMax; ++t) {
            if (cm[(size_t)t] < 0.2f) {
                while (t + 1 < tMax && cm[(size_t)t + 1] < cm[(size_t)t]) ++t;
                best = t;
                break;
            }
        }
        if (best < 1) return 0.f;
        float t = (float)best;
        const float a0 = cm[(size_t)best - 1], b0 = cm[(size_t)best], c0 = cm[(size_t)best + 1];
        const float den = a0 - 2.f * b0 + c0;
        if (std::fabs(den) > 1e-9f) t += 0.5f * (a0 - c0) / den;
        return decSr / t;
    }
};

// Delay stéréo ping-pong avec filtrage dans la boucle.
struct PingPong {
    Delay l, r;
    float lpA = 0.f, hpA = 0.f, lpL = 0.f, hpL = 0.f, lpR = 0.f, hpR = 0.f;
    Smooth ds;
    void init(double sr) {
        l.init((int)(sr * 2.1));
        r.init((int)(sr * 2.1));
        lpA = std::exp(-2.f * kPi * 5500.f / (float)sr);
        hpA = std::exp(-2.f * kPi * 220.f / (float)sr);
        ds.v = (float)(sr * 0.25);
        ds.c = 2e-4f;
        lpL = hpL = lpR = hpR = 0.f;
    }
    float filt(float x, float& lp, float& hp) {
        lp = (1.f - lpA) * x + lpA * lp;
        hp = (1.f - hpA) * lp + hpA * hp;
        return lp - hp;
    }
    void process(float in, float tgtD, float fb, float& oL, float& oR) {
        const float D = std::max(64.f, ds.next(tgtD));
        const float dl = l.read(D - 1.f), dr = r.read(D - 1.f);
        l.push(in + fb * filt(dr, lpL, hpL));
        r.push(fb * filt(dl, lpR, hpR));
        oL = dl;
        oR = dr;
    }
};

// Reverb de la suite (Voile, un-potard, Looks, Atelier, Cabine, Synth, Sampler) : FDN 8 lignes (Jot), même interface qu'avant.
//  - premières réflexions : 8 prises par canal de 4 à 30 ms, décorrélées G/D, diffusées par 3 passe-tout courts (comble les
//    50 premières ms, qui étaient creuses avec l'ancien FDN 4 lignes) ;
//  - diffusion d'entrée : 4 passe-tout de Dattorro (g = 0,75 / 0,75 / 0,625 / 0,625) ;
//  - 8 lignes de 29 à 72 ms (longueurs premières), matrice de Householder, lecture modulée (±0,19 ms, LFO 0,13 à 0,83 Hz
//    décorrélés : anti-métal, anti-flutter) ;
//  - absorption à 3 bandes dans la boucle : g_i = 10^(-3 L_i / (RT60 fs)) au médium, plateaux 1er ordre à 250 Hz (graves,
//    setTone) et 3 kHz (aigus, `damp`) ;
//  - sorties G/D sur deux combinaisons orthogonales des lignes (stéréo décorrélée).
// setDecay() est mémorisé : init() (prepare de l'hôte) le réapplique, la queue ne se raccourcit pas (correctif G3).
struct Reverb {
    static constexpr int N = 8, kEr = 8;
    struct Ring {
        std::vector<float> b; int w = 0, m = 0;
        void init(int n) { int s = 1; while (s < n) s <<= 1; b.assign((size_t)s, 0.f); m = s - 1; w = 0; }
        void clear() { std::fill(b.begin(), b.end(), 0.f); w = 0; }
        void push(float x) { b[(size_t)w] = x; w = (w + 1) & m; }
        float at(int d) const { return b[(size_t)((w - 1 - d) & m)]; }
        float cubic(float d) const {
            const float p = (float)(w - 1) - d; const int i = (int)std::floor(p); const float f = p - (float)i;
            const float y0 = b[(size_t)((i - 1) & m)], y1 = b[(size_t)(i & m)], y2 = b[(size_t)((i + 1) & m)], y3 = b[(size_t)((i + 2) & m)];
            return ((((0.5f * (y3 - y0) + 1.5f * (y1 - y2)) * f + (y0 - 2.5f * y1 + 2.f * y2 - 0.5f * y3)) * f) + 0.5f * (y2 - y0)) * f + y1;
        }
    };
    struct Ap {   // passe-tout de Schroeder, retard fixe
        Ring r; int len = 1; float g = 0.5f;
        void init(int L, float gg) { len = std::max(1, L); g = gg; r.init(len + 2); }
        float run(float x) { const float z = r.at(len - 1), v = x + g * z; r.push(v); return z - g * v; }
    };
    Ring line[N], erIn;
    Ap diff[4], erAp[2][3];
    int len[N] = {}, erT[2][kEr] = {};
    float erG[2][kEr] = {};
    float gM[N] = {}, kL[N] = {}, kH[N] = {}, lo[N] = {}, hi[N] = {};
    float mc[N] = {}, ms[N] = {}, mwc[N] = {}, mws[N] = {}, depth = 9.f;
    float sr = 48000.f, rt = 1.5f, lowMul = 1.f, highMul = 1.f, lastDamp = -1.f, cLo = 0.03f, cHi = 0.3f;
    static int nextPrime(int n) {
        if (n < 3) return 3;
        if ((n & 1) == 0) ++n;
        for (;; n += 2) { bool p = true; for (int d = 3; d * d <= n; d += 2) if (n % d == 0) { p = false; break; } if (p) return n; }
    }
    void init(double s) {
        sr = (float)s;
        const float k = sr / 48000.f;
        static const float lms[N] = {29.3f, 33.7f, 38.9f, 44.3f, 49.9f, 56.3f, 63.7f, 71.9f};
        static const float rate[N] = {0.13f, 0.21f, 0.29f, 0.37f, 0.47f, 0.59f, 0.71f, 0.83f};
        static const float ph[N] = {0.1f, 2.3f, 4.1f, 0.9f, 5.6f, 3.3f, 1.7f, 4.9f};
        depth = 9.f * k;
        for (int i = 0; i < N; ++i) {
            len[i] = nextPrime((int)(lms[i] * 1e-3f * sr));
            line[i].init(len[i] + (int)depth + 8);
            mc[i] = std::cos(ph[i]); ms[i] = std::sin(ph[i]);
            mwc[i] = std::cos(2.f * kPi * rate[i] / sr); mws[i] = std::sin(2.f * kPi * rate[i] / sr);
            lo[i] = hi[i] = 0.f;
        }
        static const float dms[4] = {4.77f, 3.60f, 12.73f, 9.31f}, dg[4] = {0.75f, 0.75f, 0.625f, 0.625f};
        for (int i = 0; i < 4; ++i) diff[i].init(nextPrime((int)(dms[i] * 1e-3f * sr)), dg[i]);
        static const float tL[kEr] = {4.3f, 7.9f, 11.3f, 14.7f, 17.9f, 21.1f, 24.6f, 28.3f};
        static const float tR[kEr] = {5.1f, 8.8f, 12.4f, 15.6f, 19.3f, 22.4f, 26.1f, 29.6f};
        static const float sg[2][kEr] = {{1, -1, 1, 1, -1, 1, -1, -1}, {-1, 1, 1, -1, 1, -1, -1, 1}};
        for (int c = 0; c < 2; ++c)
            for (int j = 0; j < kEr; ++j) {
                const float t = c ? tR[j] : tL[j];
                erT[c][j] = (int)(t * 1e-3f * sr);
                erG[c][j] = sg[c][j] * 0.3f * std::exp(-t / 30.f);
            }
        erIn.init((int)(0.031f * sr) + 4);
        erAp[0][0].init(nextPrime((int)(0.0017f * sr)), 0.6f); erAp[0][1].init(nextPrime((int)(0.0009f * sr)), 0.5f);
        erAp[1][0].init(nextPrime((int)(0.0023f * sr)), 0.6f); erAp[1][1].init(nextPrime((int)(0.0012f * sr)), 0.5f);
        erAp[0][2].init(nextPrime((int)(0.0047f * sr)), 0.65f); erAp[1][2].init(nextPrime((int)(0.0053f * sr)), 0.65f);
        cLo = 1.f - std::exp(-2.f * kPi * 250.f / sr);
        cHi = 1.f - std::exp(-2.f * kPi * 3000.f / sr);
        lastDamp = -1.f;
        updateGains();
    }
    // Temps de décroissance au médium (s).
    void setDecay(float rt60) { rt = std::max(0.05f, rt60); updateGains(); }
    // Graves : RT60(grave) = lowMultiplier × RT60(médium). 1 = neutre, < 1 = grave plus court (voix moins boueuse).
    void setTone(float lowMultiplier) { lowMul = std::max(0.1f, std::min(2.f, lowMultiplier)); updateGains(); }
    void updateGains() {
        for (int i = 0; i < N; ++i) {
            const float L = (float)len[i] + 0.5f * depth;
            const auto g = [&](float r) { return std::pow(10.f, -3.f * L / (r * sr)); };
            gM[i] = g(rt);
            kL[i] = g(rt * lowMul) / gM[i];
            kH[i] = g(rt * highMul) / gM[i];
        }
    }
    void clear() {
        for (auto& l : line) l.clear();
        for (auto& d : diff) d.r.clear();
        for (auto& c : erAp) for (auto& a : c) a.r.clear();
        erIn.clear();
        for (int i = 0; i < N; ++i) lo[i] = hi[i] = 0.f;
    }
    // damp : 0 = aigus aussi longs que le médium, 0,95 = très sombre (RT60 aigu × 0,1). Même plage qu'avant.
    void process(float in, float damp, float& oL, float& oR) {
        if (damp != lastDamp) {
            lastDamp = damp;
            const float d = std::max(0.f, std::min(0.98f, damp));
            highMul = 0.1f + 0.9f * std::pow(1.f - d, 2.2f);
            updateGains();
        }
        if (!std::isfinite(in)) in = 0.f;
        // premières réflexions
        erIn.push(in);
        float e[2];
        for (int c = 0; c < 2; ++c) {
            float a = 0.f;
            for (int j = 0; j < kEr; ++j) a += erG[c][j] * erIn.at(erT[c][j]);
            e[c] = erAp[c][2].run(erAp[c][1].run(erAp[c][0].run(a)));
        }
        // diffusion d'entrée
        float u = in;
        for (auto& d : diff) u = d.run(u);
        // FDN
        float s[N], sum = 0.f;
        for (int i = 0; i < N; ++i) {
            const float c0 = mc[i];
            mc[i] = c0 * mwc[i] - ms[i] * mws[i]; ms[i] = c0 * mws[i] + ms[i] * mwc[i];
            const float nrm = 1.5f - 0.5f * (mc[i] * mc[i] + ms[i] * ms[i]); mc[i] *= nrm; ms[i] *= nrm;
            s[i] = line[i].cubic((float)len[i] - 1.f - depth * (1.f + ms[i]) * 0.5f);
            float v = s[i] * gM[i];
            lo[i] += cLo * (v - lo[i]); v += (kL[i] - 1.f) * lo[i];      // plateau grave
            hi[i] += cHi * (v - hi[i]); v = hi[i] + kH[i] * (v - hi[i]); // plateau aigu
            sum += v;
            sOut[i] = v;
        }
        static const float inS[N] = {1, -1, 1, -1, -1, 1, 1, -1};
        const float h = 2.f / (float)N * sum, ui = 0.35f * u;
        for (int i = 0; i < N; ++i) line[i].push(sOut[i] - h + inS[i] * ui);
        static const float oLs[N] = {1, 1, -1, -1, 1, -1, 1, -1}, oRs[N] = {1, -1, 1, -1, -1, -1, 1, 1};
        float l = 0.f, r = 0.f;
        for (int i = 0; i < N; ++i) { l += oLs[i] * s[i]; r += oRs[i] * s[i]; }
        oL = kOut * l + e[0];
        oR = kOut * r + e[1];
        if (!std::isfinite(oL) || !std::isfinite(oR)) { clear(); oL = oR = 0.f; }
    }
    float sOut[N] = {};
    float kOut = 0.19f;   // énergie de la réponse ~ celle de l'ancien FDN 4 lignes (même niveau d'envoi dans les plugins)
};

class Engine {
public:
    void prepare(double sampleRate) {
        sr = (float)sampleRate;
        // décaleur : retard au repos Lz = 16 ms, comme l'ancien décaleur (latence déclarée inchangée). Mesuré sur une prise réelle :
        // un Lz plus court (8 ms) avance la voix sur l'analyse YIN et la correction devient moins juste (écart moyen 0,18 dt au lieu
        // de 0,12) ; 16 ms garde aussi la fenêtre des raccords [Lz - T0/2, Lz + T0/2[ centrée jusqu'à 70 Hz.
        win = std::max(64, (int)(0.032 * sampleRate)) / 2;
        // saturation suréchantillonnée ×8 (×4 à 88,2/96 kHz) : sa latence (phase linéaire) s'ajoute à celle du décaleur
        sat.init(Oversampler::stagesFor(sampleRate, 3));
        latSamples = win + sat.latency();
        tune.initRest(sampleRate, win, P.formants);
        // doubles « seconde prise » : graines, retards et filtres différents par voix (pas d'harmonie fixe)
        dbl1.init(sampleRate, 0x9E3779B9u, 19.f, 3.f, 8.f, 6500.f);
        dbl2.init(sampleRate, 0x85EBCA6Bu, 27.f, 3.5f, 9.f, 5200.f);
        voiced = false;
        sendHpC = std::exp(-2.f * kPi * 160.f / sr); sendLpC = 1.f - std::exp(-2.f * kPi * 9000.f / sr); sendHp = sendLp = 0.f;
        dryLine.init(latSamples + 8);
        yin.init(sampleRate);
        hop = std::max(64, (int)(0.0053 * sampleRate));
        pre.init((int)(0.03 * sampleRate) + 4);
        pp.init(sampleRate);
        rev.init(sampleRate);
        rev.setTone(0.8f);   // graves de la queue un peu plus courts : la voix ne s'empâte pas
        // décroissance de la reverb remise d'après BRUME (et non à une valeur fixe) : un nouveau prepareToPlay de l'hôte
        // (tampon, fréquence, réactivation) ne raccourcit plus la queue ; lastRt est réarmé pour process()
        lastRt = 0.8f + 1.7f * P.brume;
        rev.setDecay(lastRt);
        const float c = 1.f - std::exp(-1.f / (0.02f * sr));
        mixS.c = mordS.c = haloS.c = brumeS.c = outS.c = c;
        mixS.v = P.mix; mordS.v = P.mordant; haloS.v = P.halo; brumeS.v = P.brume;
        outS.v = std::pow(10.f, P.outDb / 20.f);
        ratioC = 1.f - std::exp(-1.f / (0.003f * sr));
        mcC = 1.f - std::exp(-1.f / (0.4f * sr)); mcAtk = 1.f - std::exp(-1.f / (0.12f * sr)); mcRel = 1.f - std::exp(-1.f / (0.5f * sr));
        mcIn = mcOut = 0.f; mcG = 1.f; fhpC = std::exp(-2.f * kPi * 250.f / sr); fhp1 = fhp2 = 0.f;
        dcR = std::exp(-2.f * kPi * 38.2f / sr);   // coupe-continu : 38,2 Hz = ancien 0.995 a 48 kHz (meme son a 48 kHz, identique a toute frequence)
        ratio = ratioT = 1.f;
        shiftSm = shiftT = 0.f;
        decCount = hopCount = holdCnt = 0;
        accum = lvl = held = dcX = dcY = 0.f;
        lastTgt = -1;
        hold = 0;
        for (auto& v : hist) v = 0.f;
        histWeight = 0.f; keyTick = 0; haveKey = false;
        for (auto& v : fastHist) v = 0.f; fastW = 0.f; stale = false;
        runLen = 0; confirm = switchCnt = lostCnt = 0; candK = candS = -1; stC = stS = stW = 0.f; tuneOff = 0.f;
        effKey = P.key; effScale = P.scale;
        applySelection();
    }
    void set(const Params& p) {
        const bool changed = p.autoKey != P.autoKey || p.autoScale != P.autoScale || p.safe != P.safe ||
                             (!p.autoKey && p.key != P.key) || (!p.autoScale && p.scale != P.scale);
        if (p.formants != P.formants) tune.formants = p.formants;
        P = p;
        if (changed) { haveKey = false; confirm = switchCnt = lostCnt = 0; candK = candS = -1; }
        applySelection();
    }
    // Vrai quand une tonalité exploitable existe (imposée, ou détectée de façon fiable).
    bool keyKnown() const { return haveKey; }
    int detectedKey() const { return effKey; }
    int detectedScale() const { return effScale; }
    // 0 = imposée par l'utilisateur, 1 = auto verrouillée, 2 = auto en recherche (aucune correction vers une note)
    int keyStatus() const { return detecting() ? (haveKey ? 1 : 2) : 0; }
    void setBpm(double b) { bpm = (float)(b > 20.0 ? b : 120.0); }
    int latency() const { return latSamples; }
    float level() const { return lvl; }
    float shiftSemis() const { return shiftSm; }
    // Lecture pour l'interface, sans effet sur le son (chemin Voile, mode sûr exclu) : note visée et hauteur chantée
    // du dernier hop voisé, reconstruites depuis l'état existant (shiftT = (cible - chantée) × tension). Faux si illisible.
    bool voiceReadout(float& sungMidi, int& targetMidi) const {
        if (P.safe || !voiced || lastTgt < 0 || P.tension < 0.02f) return false;
        sungMidi = (float)lastTgt - shiftT / P.tension;
        targetMidi = lastTgt;
        return true;
    }

    // inR peut être nul (entrée mono). Fonctionne en place.
    void process(const float* inL, const float* inR, float* outL, float* outR, int n) {
        const float tgtD = 60.f / bpm * 0.5f * sr;
        const float rt60 = 0.8f + 1.7f * P.brume;
        if (std::fabs(rt60 - lastRt) > 0.02f) { rev.setDecay(rt60); lastRt = rt60; }
        const float fb = 0.2f + 0.3f * P.brume;
        for (int i = 0; i < n; ++i) {
            float x = inR ? 0.5f * (inL[i] + inR[i]) : inL[i];
            if (!std::isfinite(x)) x = 0.f;   // entrée hostile (NaN/inf) : silence, rien ne se propage

            accum += x;
            if (++decCount >= yin.dec) { yin.push(accum / (float)yin.dec); accum = 0.f; decCount = 0; }
            if (++hopCount >= hop) { hopCount = 0; analyzeHop(); }

            const float ax = std::fabs(x);
            lvl += (ax - lvl) * (ax > lvl ? 0.05f : 0.002f);

            dryLine.push(x);
            const float dry = dryLine.read((float)latSamples);

            ratio += (ratioT - ratio) * ratioC;
            const float tuned = tune.process(x, ratio);

            // saturation asymétrique (suréchantillonnée, sans repliement) puis bitcrush + sample-rate reduction (au débit de base :
            // les images de la réduction de débit font partie de l'effet lo-fi). Sous m = 0,02 : fondu vers le signal sec aligné.
            const float m = mordS.next(P.mordant);
            float y;
            if (m > 0.0005f) {
                const float bias = 0.25f * m, kd = 1.f + 9.f * m, nb = std::tanh(bias), gn = 1.f / (1.f + 1.2f * m);
                y = sat.process(tuned, [&](float v) { return (std::tanh(v * kd + bias) - nb) * gn; });
                const float q = std::exp2(14.f - 8.f * m * m);
                y = std::round(y * q) / q;
                const int h = 1 + (int)(m * m * 4.f);
                if (++holdCnt >= h) { holdCnt = 0; held = y; }
                y = held;
                const float wd = std::min(1.f, m * 50.f);
                if (wd < 1.f) { const float yd = sat.dry(); y = yd + (y - yd) * wd; }
            } else y = sat.bypass(tuned);
            if (P.mordComp) {
                // compensation lente : le niveau (RMS ~400 ms) de la sortie saturée est ramené à celui de l'entrée de la saturation.
                // Le gain est gelé sous -70 dBFS (aucun gonflement du bruit entre deux phrases) et ne dépasse jamais +1,5 dB.
                mcIn += (tuned * tuned - mcIn) * mcC; mcOut += (y * y - mcOut) * mcC;
                if (m <= 0.0005f) mcG += (1.f - mcG) * mcAtk;   // Mordant coupé : retour rapide au gain 1
                else if (mcIn > 1e-7f) {
                    const float t = std::min(1.19f, std::max(0.2f, std::sqrt(mcIn / (mcOut + 1e-12f))));
                    mcG += (t - mcG) * (t < mcG ? mcAtk : mcRel);
                }
                y *= mcG;
            }
            const float mainS = y - dcX + dcR * dcY;
            dcX = y; dcY = mainS;

            // doubles « seconde prise » (ADT, unisson qui bouge), un peu plus à gauche / un peu plus à droite
            const float h = haloS.next(P.halo);
            float d1 = dbl1.process(mainS, voiced) * h * 0.35f;
            float d2 = dbl2.process(mainS, voiced) * h * 0.30f;
            if (P.haloStereo) {   // Focus : les doubles ne reprennent que le médium et l'aigu (le grave reste mono, au centre)
                fhp1 = fhpC * fhp1 + (1.f - fhpC) * d1; d1 -= fhp1;
                fhp2 = fhpC * fhp2 + (1.f - fhpC) * d2; d2 -= fhp2;
            }

            // delay + reverb, avec ducking du delay sous la voix
            const float b = brumeS.next(P.brume);
            float dL, dR, rL, rR;
            pp.process(mainS * b * 0.55f, tgtD, fb, dL, dR);
            const float duck = 1.f - 0.75f * std::min(1.f, lvl * 6.f);
            sendLp += (mainS - sendLp) * sendLpC;                      // envoi reverb filtré : 160 Hz - 9 kHz
            sendHp = sendHpC * sendHp + (1.f - sendHpC) * sendLp;
            pre.push((sendLp - sendHp) * b * 0.5f);
            rev.process(pre.read((float)(0.02f * sr)), 0.35f, rL, rR);

            // Halo : d1 seulement à gauche, d2 seulement à droite (l'ancien dosage 0,4 / 0,8 envoyait les deux doubles des deux côtés)
            const float wl = P.haloStereo ? mainS + 1.25f * d1 + duck * dL + rL : mainS + 0.4f * d1 + 0.8f * d2 + duck * dL + rL;
            const float wr = P.haloStereo ? mainS + 1.25f * d2 + duck * dR + rR : mainS + 0.8f * d1 + 0.4f * d2 + duck * dR + rR;
            const float mx = mixS.next(P.mix), og = outS.next(std::pow(10.f, P.outDb / 20.f));
            outL[i] = softClip((dry * (1.f - mx) + wl * mx) * og);
            outR[i] = softClip((dry * (1.f - mx) + wr * mx) * og);
        }
    }

private:
    static float softClip(float v) {
        const float a = std::fabs(v);
        if (a < 0.9f) return v;
        const float s = 0.9f + 0.1f * std::tanh((a - 0.9f) / 0.1f);
        return v < 0.f ? -s : s;
    }
    bool detecting() const { return P.safe && (P.autoKey || P.autoScale) && !(P.scale == 0 && !P.autoScale); }
    void applySelection() {
        if (!detecting()) { effKey = P.key; effScale = P.scale; haveKey = true; }
        else if (!haveKey) { effKey = P.autoKey ? effKey : P.key; if (!P.autoScale) effScale = P.scale; }
    }
    static unsigned maskOf(int key, int sc) {
        unsigned m = 0;
        for (int i = 0; i < 12; ++i) if (allowedNote(i, key, sc)) m |= 1u << i;
        return m;
    }
    float corrKey(const float* prof, int k) const {
        float mh = 0, mp = 0;
        for (int i = 0; i < 12; ++i) { mh += hist[i]; mp += prof[i]; }
        mh /= 12; mp /= 12;
        float num = 0, dh = 0, dp = 0;
        for (int i = 0; i < 12; ++i) { const float a = hist[(i + k) % 12] - mh, b = prof[i] - mp; num += a * b; dh += a * a; dp += b * b; }
        return num / (std::sqrt(dh * dp) + 1e-9f);
    }
    float fitOf(unsigned mask) const {
        float in = 0, tot = 0;
        for (int i = 0; i < 12; ++i) { tot += hist[i]; if (mask & (1u << i)) in += hist[i]; }
        return tot > 0.f ? in / tot : 0.f;
    }
    // Tonalité : histogramme des classes de hauteur (notes STABLES seulement, mémoire ~8 s) comparé aux profils de Krumhansl.
    // Verrouillage seulement si : assez de matière, assez de notes différentes, bon accord avec la gamme, et marge nette
    // sur le meilleur profil qui n'a PAS les mêmes notes (relatif majeur/mineur = mêmes notes, sans conséquence).
    void trackKey(float idMidi) {
        for (auto& v : hist) v *= 0.999f;
        hist[((int)std::lround(idMidi) % 12 + 12) % 12] += 1.f;
        histWeight = histWeight * 0.999f + 1.f;
        if (++keyTick < 96) return;
        keyTick = 0;
        static const float maj[12] = {6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f, 2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f};
        static const float mnr[12] = {6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f, 2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f};
        struct C { int k, sc; float c; unsigned m; } cand[24]; int nc = 0;
        for (int k = 0; k < 12; ++k) {
            if (!P.autoKey && k != P.key) continue;
            for (int sc = 1; sc <= 2; ++sc) {
                if (!P.autoScale && sc != P.scale) continue;
                cand[nc++] = {k, sc, corrKey(sc == 1 ? maj : mnr, k), maskOf(k, sc)};
            }
        }
        if (nc == 0) return;
        int bi = 0;
        for (int i = 1; i < nc; ++i) if (cand[i].c > cand[bi].c) bi = i;
        float second = -2.f;
        for (int i = 0; i < nc; ++i) if (cand[i].m != cand[bi].m && cand[i].c > second) second = cand[i].c;
        if (second < -1.5f) second = cand[bi].c - 1.f;  // un seul jeu de notes possible
        float tot = 0; for (int i = 0; i < 12; ++i) tot += hist[i];
        int npc = 0; for (int i = 0; i < 12; ++i) if (hist[i] >= 0.03f * tot) ++npc;
        const bool reliable = histWeight >= 220.f && npc >= 4 && cand[bi].c >= 0.6f && fitOf(cand[bi].m) >= 0.9f && cand[bi].c - second >= 0.05f;
        if (!haveKey) {
            if (reliable && cand[bi].k == candK && cand[bi].sc == candS) ++confirm; else confirm = reliable ? 1 : 0;
            candK = reliable ? cand[bi].k : -1; candS = reliable ? cand[bi].sc : -1;
            if (confirm >= 2) { effKey = cand[bi].k; effScale = cand[bi].sc; haveKey = true; confirm = switchCnt = lostCnt = 0; }
            return;
        }
        // déjà verrouillé : hystérésis. On change seulement si une autre tonalité (autres notes) est fiable, nettement
        // meilleure, et le reste ~1,5 s ; on lâche (retour en recherche) si la tonalité ne colle plus du tout (~3 s).
        float cur = -2.f; unsigned curMask = maskOf(effKey, effScale);
        for (int i = 0; i < nc; ++i) if (cand[i].k == effKey && cand[i].sc == effScale) cur = cand[i].c;
        if (cur < -1.5f) cur = corrKey(effScale == 1 ? maj : mnr, effKey);
        if (reliable && cand[bi].m != curMask && cand[bi].c > cur + 0.08f) {
            if (cand[bi].k == candK && cand[bi].sc == candS) ++switchCnt; else switchCnt = 1;
            candK = cand[bi].k; candS = cand[bi].sc;
            if (switchCnt >= 3) { effKey = candK; effScale = candS; switchCnt = 0; lostCnt = 0; }
        } else { switchCnt = 0; candK = candS = -1; }
        if (fitOf(curMask) < 0.75f) { if (++lostCnt >= 6) { haveKey = false; lostCnt = 0; confirm = 0; for (auto& v : hist) v *= 0.25f; histWeight *= 0.25f; } } else lostCnt = 0;
    }
    // Écart de calage global de la prise (en demi-tons, moyenne circulaire sur les notes stables) : sert seulement à
    // identifier la bonne note (une prise à +45 cents ne bascule pas sur la note voisine), jamais à fixer la grille.
    void trackTuning(float midi) {
        const float a = 2.f * kPi * midi;
        stC = stC * 0.9995f + std::cos(a); stS = stS * 0.9995f + std::sin(a); stW = stW * 0.9995f + 1.f;
        const float R = std::sqrt(stC * stC + stS * stS) / std::max(1.f, stW);
        tuneOff = (stW > 150.f && R > 0.6f) ? std::max(-0.4f, std::min(0.4f, std::atan2(stS, stC) / (2.f * kPi))) : 0.f;
    }
    static float median3(float a, float b, float c) { return std::max(std::min(a, b), std::min(std::max(a, b), c)); }
    void analyzeHop() {
        const float f0 = yin.analyze();
        voiced = f0 > 0.f;
        if (voiced) { const float per = sr / f0; tune.setPeriod(per); dbl1.setPeriod(per); dbl2.setPeriod(per); }
        float cap = 6.5f;
        if (!P.safe) {   // chemin historique (Voile)
            if (f0 > 0.f) {
                const float midi = 69.f + 12.f * std::log2(f0 / 440.f);
                int tgt = nearestAllowed(midi, effKey, effScale);
                if (lastTgt >= 0 && allowedNote(lastTgt, effKey, effScale) && std::fabs(midi - (float)lastTgt) < 0.62f) tgt = lastTgt;
                lastTgt = tgt;
                shiftT = ((float)tgt - midi) * P.tension;
                hold = 0;
            } else if (++hold > 4) {
                shiftT *= 0.7f;
                lastTgt = -1;
            }
        } else {
            cap = 1.5f;
            if (f0 > 0.f) {
                hold = 0;
                const float midi = 69.f + 12.f * std::log2(f0 / 440.f);
                mBuf[2] = mBuf[1]; mBuf[1] = mBuf[0]; mBuf[0] = midi;
                ++runLen;
                const float lo = std::min(mBuf[0], std::min(mBuf[1], mBuf[2])), hi = std::max(mBuf[0], std::max(mBuf[1], mBuf[2]));
                if (runLen >= 3 && hi - lo < 0.5f) {   // hauteur stable : seule situation où l'on corrige
                    const float m = median3(mBuf[0], mBuf[1], mBuf[2]);
                    trackTuning(m);
                    const float idM = m - tuneOff;
                    if (detecting()) {
                        trackKey(idM);
                        // mémoire courte : si les notes chantées ne collent plus à la tonalité verrouillée, on suspend la correction tout de suite
                        for (auto& v : fastHist) v *= 0.996f;
                        fastHist[((int)std::lround(idM) % 12 + 12) % 12] += 1.f;
                        fastW = fastW * 0.996f + 1.f;
                        if (haveKey && fastW >= 60.f) {
                            float in = 0; const unsigned mk = maskOf(effKey, effScale);
                            for (int i = 0; i < 12; ++i) if (mk & (1u << i)) in += fastHist[i];
                            const float fit = in / fastW;
                            if (fit < 0.7f) stale = true; else if (fit >= 0.8f) stale = false;
                        } else if (!haveKey) stale = false;
                    } else stale = false;
                    if (!haveKey || stale) {            // tonalité inconnue ou peu fiable : hauteur laissée telle quelle
                        shiftT *= 0.5f; lastTgt = -1;
                    } else {
                        int tgt = nearestAllowed(idM, effKey, effScale);
                        const bool autoMode = detecting();
                        if (autoMode && std::fabs(idM - (float)tgt) > 0.65f) tgt = (int)std::lround(idM);   // note accidentelle : on l'ajuste au demi-ton, sans la déplacer vers la gamme
                        if (lastTgt >= 0 && std::fabs(idM - (float)lastTgt) < 0.6f &&
                            (allowedNote(lastTgt, effKey, effScale) || (autoMode && std::fabs(idM - (float)lastTgt) < 0.35f))) tgt = lastTgt;
                        lastTgt = tgt;
                        float e = ((float)tgt - m) * 100.f;                  // cents
                        const float dz = P.deadCents;
                        e -= std::max(-dz, std::min(dz, e));                 // zone morte douce
                        shiftT = e * 0.01f * P.tension;
                    }
                } else {
                    shiftT *= 0.5f;                                          // attaque, transition, glissando : on laisse passer tel quel
                }
            } else {
                runLen = 0;
                if (++hold > 4) { shiftT *= 0.7f; lastTgt = -1; }
            }
        }
        const float tms = P.retuneMs >= 0.f ? P.retuneMs : 1.f + 140.f * (1.f - P.tension) * (1.f - P.tension);
        const float tc = std::max(0.5f, tms) * 1e-3f;
        const float a = 1.f - std::exp(-((float)hop / sr) / tc);
        shiftSm += (shiftT - shiftSm) * a;
        shiftSm = std::max(-cap, std::min(cap, shiftSm));
        ratioT = std::exp2(shiftSm / 12.f);
    }

    Params P;
    float sr = 48000.f, bpm = 120.f, lastRt = 0.f;
    int win = 1536, latSamples = 768, hop = 256;
    Shifter tune;
    dsp::AdtVoice dbl1, dbl2;
    bool voiced = false;
    float sendHp = 0.f, sendLp = 0.f, sendHpC = 0.99f, sendLpC = 0.5f;
    Oversampler sat;
    Delay dryLine, pre;
    Yin yin;
    PingPong pp;
    Reverb rev;
    Smooth mixS, mordS, haloS, brumeS, outS;
    float mcIn = 0.f, mcOut = 0.f, mcG = 1.f, mcC = 1e-4f, mcAtk = 1e-3f, mcRel = 1e-4f, fhpC = 0.97f, fhp1 = 0.f, fhp2 = 0.f;
    float ratio = 1.f, ratioT = 1.f, ratioC = 0.01f, shiftSm = 0.f, shiftT = 0.f;
    float accum = 0.f, lvl = 0.f, held = 0.f, dcX = 0.f, dcY = 0.f, dcR = 0.995f;
    int decCount = 0, hopCount = 0, holdCnt = 0, hold = 0, lastTgt = -1;
    float hist[12] = {}, histWeight = 0.f;
    int keyTick = 0, effKey = 0, effScale = 1;
    bool haveKey = false;
    float mBuf[3] = {0, 0, 0}, stC = 0.f, stS = 0.f, stW = 0.f, tuneOff = 0.f;
    float fastHist[12] = {}, fastW = 0.f; bool stale = false;
    int runLen = 0, confirm = 0, switchCnt = 0, lostCnt = 0, candK = -1, candS = -1;
};

}  // namespace voile
