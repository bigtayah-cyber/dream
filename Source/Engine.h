// Voile : moteur audio. C++17 pur, sans JUCE (testable hors-ligne).
// Chaîne : correction de hauteur dure -> saturation/bitcrush -> doubles -> delay ping-pong + reverb.
#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

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
        if (i0 >= n) i0 -= n;
        if (i0 < 0) i0 = 0;
        const float f = p - (float)i0;
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

// Pitch shifter à deux têtes de lecture croisées (fenêtre sin^2). Latence = win/2 (retard du signal sec).
// Décalage nul ou presque (moins de 0,5 cent) : sortie = signal sec retardé de win/2, exactement, et les têtes sont ramenées à
// l'état « une seule tête » (ph = 0) : plus de peigne à décalage nul, et la reprise du décalage part d'une tête alignée sur le sec.
// Entre 0,5 et 3 cents : fondu sec -> décalé.
struct Shifter {
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

// Reverb à réseau de retards (FDN 4 lignes, matrice de Hadamard) avec diffusion d'entrée.
struct Reverb {
    static constexpr int N = 4;
    Delay d[N], ap[2];
    int len[N] = {}, apLen[2] = {};
    float lp[N] = {}, g[N] = {1, 1, 1, 1}, sr = 48000.f;
    void init(double s) {
        sr = (float)s;
        const int base[N] = {1087, 1283, 1481, 1699}, ab[2] = {142, 107};
        for (int i = 0; i < N; ++i) { len[i] = std::max(8, (int)(base[i] * sr / 48000.f)); d[i].init(len[i] + 2); lp[i] = 0.f; }
        for (int i = 0; i < 2; ++i) { apLen[i] = std::max(8, (int)(ab[i] * sr / 48000.f)); ap[i].init(apLen[i] + 2); }
    }
    void setDecay(float rt60) {
        for (int i = 0; i < N; ++i) g[i] = std::pow(10.f, -3.f * (float)len[i] / (rt60 * sr));
    }
    void process(float in, float damp, float& oL, float& oR) {
        float x = in;
        for (int k = 0; k < 2; ++k) {
            const float wd = ap[k].read((float)apLen[k] - 1.f);
            const float w = x + 0.5f * wd;
            x = -0.5f * w + wd;
            ap[k].push(w);
        }
        float s[N];
        for (int i = 0; i < N; ++i) s[i] = d[i].read((float)len[i] - 1.f);
        const float t[N] = {0.5f * (s[0] + s[1] + s[2] + s[3]), 0.5f * (s[0] - s[1] + s[2] - s[3]),
                            0.5f * (s[0] + s[1] - s[2] - s[3]), 0.5f * (s[0] - s[1] - s[2] + s[3])};
        for (int i = 0; i < N; ++i) {
            lp[i] = (1.f - damp) * (t[i] * g[i]) + damp * lp[i];
            d[i].push(0.5f * x + lp[i]);
        }
        oL = 0.5f * (s[0] + s[2]);
        oR = 0.5f * (s[1] + s[3]);
    }
};

class Engine {
public:
    void prepare(double sampleRate) {
        sr = (float)sampleRate;
        win = std::max(64, (int)(0.032 * sampleRate));
        // saturation suréchantillonnée ×8 (×4 à 88,2/96 kHz) : sa latence (phase linéaire) s'ajoute à celle du décaleur
        sat.init(Oversampler::stagesFor(sampleRate, 3));
        latSamples = win / 2 + sat.latency();
        tune.init(win);
        dbl1.init(win);
        dbl2.init(win);
        dryLine.init(latSamples + 8);
        yin.init(sampleRate);
        hop = std::max(64, (int)(0.0053 * sampleRate));
        pre.init((int)(0.03 * sampleRate) + 4);
        pp.init(sampleRate);
        rev.init(sampleRate);
        rev.setDecay(1.2f);
        const float c = 1.f - std::exp(-1.f / (0.02f * sr));
        mixS.c = mordS.c = haloS.c = brumeS.c = outS.c = c;
        mixS.v = P.mix; mordS.v = P.mordant; haloS.v = P.halo; brumeS.v = P.brume;
        outS.v = std::pow(10.f, P.outDb / 20.f);
        ratioC = 1.f - std::exp(-1.f / (0.003f * sr));
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

    // inR peut être nul (entrée mono). Fonctionne en place.
    void process(const float* inL, const float* inR, float* outL, float* outR, int n) {
        const float tgtD = 60.f / bpm * 0.5f * sr;
        const float rt60 = 0.8f + 1.7f * P.brume;
        if (std::fabs(rt60 - lastRt) > 0.02f) { rev.setDecay(rt60); lastRt = rt60; }
        const float fb = 0.2f + 0.3f * P.brume;
        for (int i = 0; i < n; ++i) {
            const float x = inR ? 0.5f * (inL[i] + inR[i]) : inL[i];

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
            const float mainS = y - dcX + 0.995f * dcY;
            dcX = y; dcY = mainS;

            // doubles : +12 demi-tons à droite, -5 à gauche
            const float h = haloS.next(P.halo);
            const float d1 = dbl1.process(mainS, 2.f) * h * 0.35f;
            const float d2 = dbl2.process(mainS, 0.7491535f) * h * 0.30f;

            // delay + reverb, avec ducking du delay sous la voix
            const float b = brumeS.next(P.brume);
            float dL, dR, rL, rR;
            pp.process(mainS * b * 0.55f, tgtD, fb, dL, dR);
            const float duck = 1.f - 0.75f * std::min(1.f, lvl * 6.f);
            pre.push(mainS * b * 0.5f);
            rev.process(pre.read((float)(0.02f * sr)), 0.35f, rL, rR);

            const float wl = mainS + 0.4f * d1 + 0.8f * d2 + duck * dL + rL;
            const float wr = mainS + 0.8f * d1 + 0.4f * d2 + duck * dR + rR;
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
    Shifter tune, dbl1, dbl2;
    Oversampler sat;
    Delay dryLine, pre;
    Yin yin;
    PingPong pp;
    Reverb rev;
    Smooth mixS, mordS, haloS, brumeS, outS;
    float ratio = 1.f, ratioT = 1.f, ratioC = 0.01f, shiftSm = 0.f, shiftT = 0.f;
    float accum = 0.f, lvl = 0.f, held = 0.f, dcX = 0.f, dcY = 0.f;
    int decCount = 0, hopCount = 0, holdCnt = 0, hold = 0, lastTgt = -1;
    float hist[12] = {}, histWeight = 0.f;
    int keyTick = 0, effKey = 0, effScale = 1;
    bool haveKey = false;
    float mBuf[3] = {0, 0, 0}, stC = 0.f, stS = 0.f, stW = 0.f, tuneOff = 0.f;
    float fastHist[12] = {}, fastW = 0.f; bool stale = false;
    int runLen = 0, confirm = 0, switchCnt = 0, lostCnt = 0, candK = -1, candS = -1;
};

}  // namespace voile
