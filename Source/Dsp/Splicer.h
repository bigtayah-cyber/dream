// Briques de hauteur partagées (C++17 pur, sans JUCE, sans allocation dans process).
//  - dsp::Splicer      : décaleur à raccords synchrones à la période (extrait tel quel d'Orbe Autotune, mode Direct).
//  - dsp::VoiceShifter : Splicer + préservation optionnelle des formants (blanchiment LPC en treillis, raccords sur le résidu,
//                        recoloration avec l'enveloppe de l'instant lu). Utilisé par voile::Shifter (Voile, un-potard, Looks, Atelier).
//  - dsp::AdtVoice     : double « seconde prise » (ADT) : retard et hauteur en bruit filtré lent, attaques désynchronisées par
//                        phrase, souffle adouci. Utilisé par les doubles de voile::Engine (Voile et un-potard).
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace dsp {

constexpr float kPiF = 3.14159265358979323846f;

// ---------------------------------------------------------------- décaleur à raccords synchrones
// PSOLA causal « léger » : un seul lecteur à retard variable dans la mémoire des derniers 4096 échantillons. Le retard dérive à la
// vitesse (1 - ratio) échantillons par échantillon ; quand il sort de la fenêtre [lo, lo + T0[ (T0 = période de la voix), il est
// déplacé d'un nombre ENTIER de périodes : le raccord tombe sur la même phase de la même onde, donc sans peigne ni creux. L'écart
// exact est affiné par corrélation normalisée (+-R échantillons, interpolation parabolique) et fondu en quelques ms. Fenêtre centrée
// sur le retard au repos Lz (latence déclarée) tant que la voix est assez aiguë, sinon calée contre le minimum (kMinD).
// snapDry (défaut, comportement d'Orbe Autotune) : décalage de moins de 0,5 cent -> sortie = signal sec retardé de Lz, exactement ;
// entre 0,5 et 3 cents : fondu sec -> décalé. snapDry = false (doubles) : le lecteur ne revient jamais au sec.
struct Splicer {
    static constexpr int kBuf = 4096, kMask = kBuf - 1, kMinD = 32;
    int nch = 1, w = 0, xf = 0, xfLen = 64, xfCur = 64;
    bool xfOn = false, snapDry = true;
    float lz = 240.f, d = 240.f, dOld = 0.f, t0 = 300.f;
    std::vector<float> b[2];
    void init(int channels, int restDelay) {
        nch = std::min(2, std::max(1, channels));
        for (int c = 0; c < 2; ++c) b[c].assign(c < nch ? (size_t)kBuf : 0u, 0.f);
        lz = (float)restDelay; d = lz; dOld = lz; w = 0; xf = 0; xfOn = false; setPeriod(300.f);
    }
    void setPeriod(float p) { t0 = std::min(1500.f, std::max(40.f, p)); xfLen = std::min(160, std::max(32, (int)(0.5f * t0))); }
    // Mémorise l'entrée sans calculer de décalage (instance au repos) : la mémoire reste à jour.
    void feed(const float* in) {
        for (int c = 0; c < nch; ++c) b[c][(size_t)w] = in[c];
        w = (w + 1) & kMask; d = lz; xfOn = false;
    }
    void process(const float* in, float* out, float ratio) {
        for (int c = 0; c < nch; ++c) b[c][(size_t)w] = in[c];
        w = (w + 1) & kMask;
        const int lzi = (int)lz;
        const float cents = std::fabs(ratio - 1.f) * 1731.234f;
        float a = std::min(1.f, std::max(0.f, (cents - 0.5f) * 0.4f));
        if (!snapDry) a = 1.f;
        if (a <= 0.f) {   // décalage nul : sec exactement aligné, lecteur ramené au repos (inaudible, il n'est pas utilisé)
            d = lz; xfOn = false;
            for (int c = 0; c < nch; ++c) out[c] = b[c][(size_t)((w - 1 - lzi) & kMask)];
            return;
        }
        const float rate = 1.f - ratio;
        d += rate; if (xfOn) dOld += rate;
        const float lo = std::max((float)kMinD, lz - 0.5f * t0) + (rate < 0.f ? -rate * (float)xfLen : 0.f);
        if (!xfOn && (d < lo || d >= lo + t0)) splice(lo);
        d = std::max(d, 2.f); dOld = std::max(dOld, 2.f);
        float g = 1.f;
        if (xfOn) { g = 0.5f - 0.5f * std::cos(kPiF * (float)(xf + 1) / (float)xfCur); if (++xf >= xfCur) xfOn = false; }
        for (int c = 0; c < nch; ++c) {
            const float dry = b[c][(size_t)((w - 1 - lzi) & kMask)];
            float sh = rd(c, d);
            if (g < 1.f) { const float so = rd(c, dOld); sh = so + (sh - so) * g; }
            out[c] = dry + a * (sh - dry);
        }
    }
    // Gain du fondu tel qu'appliqué au dernier échantillon rendu (1 hors raccord).
    float lastFade() const { return xfOn ? 0.5f - 0.5f * std::cos(kPiF * (float)xf / (float)xfCur) : 1.f; }
    float rd(int c, float D) const {   // lecture cubique (Catmull-Rom), D >= 2
        const float p = (float)(w - 1) - D;
        const int i = (int)std::floor(p);
        const float f = p - (float)i;
        const float* x = b[c].data();
        const float y0 = x[(i - 1) & kMask], y1 = x[i & kMask], y2 = x[(i + 1) & kMask], y3 = x[(i + 2) & kMask];
        return ((((0.5f * (y3 - y0) + 1.5f * (y1 - y2)) * f + (y0 - 2.5f * y1 + 2.f * y2 - 0.5f * y3)) * f) + 0.5f * (y2 - y0)) * f + y1;
    }
private:
    float mid(int delay) const {
        const size_t k = (size_t)((w - 1 - delay) & kMask);
        return nch == 2 ? 0.5f * (b[0][k] + b[1][k]) : b[0][k];
    }
    void splice(float lo) {
        int k = (int)std::lround((lo + 0.5f * t0 - d) / t0);
        if (k == 0) k = d < lo ? 1 : -1;
        const float dn = d + (float)k * t0;
        const int R = std::min(24, std::max(4, (int)(t0 * 0.125f))), Lc = xfLen, D0 = (int)std::lround(d), N0 = (int)std::lround(dn);
        float sc[2 * 24 + 3]; for (auto& v : sc) v = -2.f;
        float eo = 1e-9f; for (int j = 0; j < Lc; ++j) { const float v = mid(D0 + j); eo += v * v; }
        int best = 0; float bs = -3.f;
        for (int dl = -R; dl <= R; ++dl) {
            const int Dn = N0 + dl;
            if (Dn < 3 || Dn + Lc >= kBuf - 8 || D0 + Lc >= kBuf - 8) continue;
            float xy = 0.f, en = 1e-9f;
            for (int j = 0; j < Lc; ++j) { const float yo = mid(D0 + j), yn = mid(Dn + j); xy += yo * yn; en += yn * yn; }
            const float s = xy / std::sqrt(eo * en);
            sc[dl + R + 1] = s;
            if (s > bs) { bs = s; best = dl; }
        }
        if (bs < -2.f) best = 0;
        float frac = 0.f;
        if (best > -R && best < R) {
            const float sm = sc[best + R], s0 = sc[best + R + 1], sp = sc[best + R + 2], den = sm - 2.f * s0 + sp;
            if (sm > -1.5f && sp > -1.5f && den < -1e-6f) frac = std::max(-0.5f, std::min(0.5f, 0.5f * (sm - sp) / den));
        }
        dOld = d; d = d + (float)(N0 + best - D0) + frac;
        xfCur = Lc; xf = 0; xfOn = true;
    }
};

// ---------------------------------------------------------------- décaleur de voix (formants préservés en option)
// Mono. Latence = retard au repos Lz (exact). Décalage < 0,5 cent : sortie = entrée retardée de Lz, bit à bit (aucun filtre).
// Formants : préaccentuation 0,9 -> treillis LPC d'analyse (ordre fs/700+4, fenêtre de Hann de 23 ms, trame tous les 128/256
// échantillons, fenêtre de retard gaussienne de 0,35 f0 pour ne pas modéliser les harmoniques) -> résidu. Le Splicer lit le
// résidu ; le treillis de synthèse le recolore avec l'enveloppe de l'instant OÙ IL A ÉTÉ BLANCHI (même schéma que TuneCore) :
// le timbre ne suit pas la hauteur (fin de l'effet « chipmunk »), et sans décalage la chaîne analyse/synthèse est l'identité.
// La période de la voix (échantillons) doit être fournie par setPeriod() (détecteur de l'appelant) ; défaut 300.
struct VoiceShifter {
    static constexpr int kMaxP = 80, kKRing = 64;
    Splicer sp;
    bool formants = false;
    double sr = 48000.0;
    int P = 24, hop = 128, Nw = 1024, hopC = 0;
    int64_t nAbs = 0, hopIdx = 0;
    double lagB = 60.0;
    std::vector<float> res, Kr, lpcWin, lpcBuf, sinc;
    float kA[kMaxP] = {}, dkA[kMaxP] = {}, kS[kMaxP] = {}, bA[kMaxP] = {}, bS[kMaxP] = {};
    double rS[kMaxP + 1] = {};
    float pePrev = 0.f, dePrev = 0.f;

    void init(double sampleRate, int restDelay, bool withFormants) {
        sr = sampleRate; formants = withFormants;
        sp.init(1, restDelay);
        hop = sr >= 88000.0 ? 256 : 128;
        P = std::min(kMaxP, (int)std::lround(sr / 700.0) + 4);
        Nw = std::min(Splicer::kBuf - 64, (int)std::lround(sr * 0.023));
        res.assign((size_t)Splicer::kBuf, 0.f);
        Kr.assign((size_t)kKRing * kMaxP, 0.f);
        lpcWin.resize((size_t)Nw); lpcBuf.assign((size_t)Nw, 0.f);
        buildSinc();
        for (int j = 0; j < Nw; ++j) lpcWin[(size_t)j] = 0.5f - 0.5f * std::cos(2.f * kPiF * ((float)j + 0.5f) / (float)Nw);
        reset();
    }
    void reset() {
        std::fill(res.begin(), res.end(), 0.f); std::fill(Kr.begin(), Kr.end(), 0.f);
        for (int c = 0; c < sp.nch; ++c) std::fill(sp.b[c].begin(), sp.b[c].end(), 0.f);
        sp.w = 0; sp.d = sp.dOld = sp.lz; sp.xf = 0; sp.xfOn = false;
        for (int i = 0; i < kMaxP; ++i) kA[i] = dkA[i] = kS[i] = bA[i] = bS[i] = 0.f;
        for (auto& v : rS) v = 0.0;
        pePrev = dePrev = 0.f; hopC = 0; nAbs = 0; hopIdx = 0; lagB = 60.0;
    }
    int latency() const { return (int)sp.lz; }
    void setPeriod(float p) {
        sp.setPeriod(p);
        lagB = std::max(40.0, std::min(250.0, 0.35 * sr / (double)sp.t0));
    }
    float process(float x, float ratio) {
        if (!std::isfinite(x)) x = 0.f;
        if (!formants) { float o; sp.process(&x, &o, ratio); return o; }
        const int w0 = sp.w;
        const float pe = x - 0.9f * pePrev; pePrev = x;
        res[(size_t)w0] = latticeA(pe);
        for (int j = 0; j < P; ++j) kA[j] += dkA[j];
        float o; sp.process(&x, &o, ratio);
        ++nAbs;
        const float cents = std::fabs(ratio - 1.f) * 1731.234f;
        const float a = std::min(1.f, std::max(0.f, (cents - 0.5f) * 0.4f));
        if (a <= 0.f) {
            // sec exact (o) ; le treillis de synthèse tourne sur le résidu aligné : son état reste cohérent pour la reprise
            kAt((double)(nAbs - 1) - (double)sp.lz);
            const float yv = latticeS(resAt(sp.lz)); dePrev = yv + 0.9f * dePrev;
            if (!std::isfinite(dePrev)) clearSynth();
        } else {
            const float g = sp.lastFade();
            float v = resAt(sp.d);
            if (g < 1.f) { const float vo = resAt(sp.dOld); v = vo + (v - vo) * g; }
            kAt((double)(nAbs - 1) - (double)sp.d);   // enveloppe de l'instant où le résidu lu a été blanchi
            float wv = latticeS(v) + 0.9f * dePrev; dePrev = wv;
            if (!std::isfinite(wv) || std::fabs(wv) > 32.f) { wv = 0.f; clearSynth(); }
            const float dry = sp.b[0][(size_t)((sp.w - 1 - (int)sp.lz) & Splicer::kMask)];
            o = dry + a * (wv - dry);
        }
        if (++hopC >= hop) { hopC = 0; hopUpdate(); }
        return o;
    }
private:
    void clearSynth() { dePrev = 0.f; for (int i = 0; i < kMaxP; ++i) bS[i] = 0.f; }
    // Lecture du résidu par sinus cardinal fenêtré (16 points, table de 512 phases) : le résidu blanchi a autant d'énergie près de
    // Nyquist qu'au médium ; une lecture cubique y crée une modulation qui, recolorée, devenait un bruit entre harmoniques
    // (mesuré à +3 dt, voix à 196 Hz, 44,1 kHz : -34 dB en cubique, -50 dB avec cette lecture, comme le Splicer sur le signal brut).
    static constexpr int kTaps = 16, kPh = 512;
    float resAt(float D) const {
        const float p = (float)(sp.w - 1) - D;
        const int i = (int)std::floor(p);
        const float f = p - (float)i;
        const float fp = f * (float)kPh; const int ph = std::min(kPh - 1, (int)fp); const float fr = fp - (float)ph;
        const float* t0 = &sinc[(size_t)ph * kTaps];
        const float* t1 = &sinc[(size_t)(ph + 1) * kTaps];
        const float* x = res.data();
        const int M = Splicer::kMask;
        float acc = 0.f;
        for (int k = 0; k < kTaps; ++k) { const float c = t0[k] + fr * (t1[k] - t0[k]); acc += c * x[(i - kTaps / 2 + 1 + k) & M]; }
        return acc;
    }
    void buildSinc() {
        sinc.assign((size_t)(kPh + 1) * kTaps, 0.f);
        const double pi = 3.14159265358979323846, beta = 7.0, fc = 0.92;   // coupure à 0,92 Nyquist : pas d'images sur le résidu
        auto i0 = [](double x) { double s = 1.0, t = 1.0; for (int k = 1; k < 40; ++k) { t *= (x / (2.0 * k)) * (x / (2.0 * k)); s += t; } return s; };
        for (int ph = 0; ph <= kPh; ++ph) {
            const double f = (double)ph / kPh;
            double sum = 0.0;
            for (int k = 0; k < kTaps; ++k) {
                const double xk = (double)(k - kTaps / 2 + 1) - f;   // distance au point lu
                const double u = xk / (kTaps / 2.0);
                const double win = std::fabs(u) < 1.0 ? i0(beta * std::sqrt(1.0 - u * u)) / i0(beta) : 0.0;
                const double sn = std::fabs(xk) < 1e-9 ? fc : std::sin(pi * fc * xk) / (pi * xk);
                sinc[(size_t)ph * kTaps + k] = (float)(sn * win); sum += sn * win;
            }
            for (int k = 0; k < kTaps; ++k) sinc[(size_t)ph * kTaps + k] = (float)(sinc[(size_t)ph * kTaps + k] / sum);
        }
    }
    float latticeA(float x) {
        float f = x, carry = x;
        for (int i = 0; i < P; ++i) { const float bo = bA[i], nb = kA[i] * f + bo; f += kA[i] * bo; bA[i] = carry; carry = nb; }
        return f;
    }
    float latticeS(float e) {
        float fs[kMaxP + 1];
        fs[P] = e;
        for (int i = P - 1; i >= 0; --i) fs[i] = fs[i + 1] - kS[i] * bS[i];
        float carry = fs[0];
        for (int i = 0; i < P; ++i) { const float nb = kS[i] * fs[i] + bS[i]; bS[i] = carry; carry = nb; }
        return fs[0];
    }
    // Les entrées d'instant absolu m dans [q·hop, (q+1)·hop[ ont été blanchies avec K[q-2] -> K[q-1] (interpolation linéaire).
    void kAt(double m) {
        const int64_t mi = (int64_t)std::floor(m), q = mi >= 0 ? mi / hop : -1;
        const float j = (float)(mi - q * hop) / (float)hop;
        const float* k0 = &Kr[(size_t)(((q - 2) & (kKRing - 1)) * kMaxP)];
        const float* k1 = &Kr[(size_t)(((q - 1) & (kKRing - 1)) * kMaxP)];
        for (int i = 0; i < P; ++i) kS[i] = k0[i] + j * (k1[i] - k0[i]);
    }
    float rawAt(int back) const { return sp.b[0][(size_t)((sp.w - 1 - back) & Splicer::kMask)]; }
    void hopUpdate() {
        float* Kn = &Kr[(size_t)((hopIdx & (kKRing - 1)) * kMaxP)];
        computeLpc(Kn);
        const float* Kp = &Kr[(size_t)(((hopIdx - 1) & (kKRing - 1)) * kMaxP)];
        for (int j = 0; j < P; ++j) { kA[j] = Kp[j]; dkA[j] = (Kn[j] - Kp[j]) / (float)hop; }
        ++hopIdx;
    }
    void computeLpc(float* K) {
        float pp = rawAt(Nw);
        for (int j = 0; j < Nw; ++j) { const float v = rawAt(Nw - 1 - j); lpcBuf[(size_t)j] = (v - 0.9f * pp) * lpcWin[(size_t)j]; pp = v; }
        double r[kMaxP + 1];
        for (int lag = 0; lag <= P; ++lag) {
            double s = 0.0;
            for (int j = lag; j < Nw; ++j) s += (double)lpcBuf[(size_t)j] * lpcBuf[(size_t)(j - lag)];
            const double bb = 2.0 * 3.14159265358979 * lagB * lag / sr;   // fenêtre de retard gaussienne : lisse les harmoniques
            r[lag] = s * std::exp(-0.5 * bb * bb);
            rS[lag] += 0.5 * (r[lag] - rS[lag]); r[lag] = rS[lag];          // autocorrélation lissée : enveloppe stable d'une trame à l'autre
        }
        for (int j = 0; j < P; ++j) K[j] = 0.f;
        if (!(r[0] >= 1e-9 * Nw) || !std::isfinite(r[0])) { if (!std::isfinite(r[0])) for (auto& v : rS) v = 0.0; return; }
        r[0] *= 1.0001;   // plancher de bruit blanc à -40 dB : conditionnement
        double a[kMaxP + 1] = {1.0}, t[kMaxP + 1], E = r[0];
        for (int i = 1; i <= P; ++i) {
            double acc = r[i];
            for (int j = 1; j < i; ++j) acc += a[j] * r[i - j];
            double k = -acc / E;
            k = std::max(-0.995, std::min(0.995, k));
            for (int j = 1; j < i; ++j) t[j] = a[j] + k * a[i - j];
            for (int j = 1; j < i; ++j) a[j] = t[j];
            a[i] = k; K[i - 1] = (float)k;
            E *= (1.0 - k * k);
            if (E <= r[0] * 1e-7) break;
        }
    }
};

// ---------------------------------------------------------------- double « seconde prise » (ADT)
// Une voix de double : Splicer sans retour au sec (hauteur ±cents en bruit filtré indépendant, raccords synchrones à la période)
// -> retard variable lent (base + bruit filtré ~0,15 Hz) -> passe-bas doux (souffle et sifflantes adoucis) ; gain réduit sur le
// non voisé. À chaque nouvelle phrase (après > 150 ms de silence), le décalage d'attaque est retiré au hasard (0 à 18 ms) pendant
// le silence (inaudible). Pas d'harmonie fixe : c'est un unisson qui « bouge » comme une autre prise.
struct AdtVoice {
    Splicer sp;
    std::vector<float> line; int lw = 0, lmask = 0;
    float sr = 48000.f, baseD = 1000.f, walkD = 100.f, centsAmt = 8.f, ofs = 0.f;
    uint32_t rng = 1;
    // bruits filtrés (deux pôles) : cible tirée à intervalle aléatoire, lissée
    struct Walk { float tgt = 0.f, a = 0.f, b = 0.f, c = 0.001f; int cnt = 0; };
    Walk wT, wP;
    float lpC = 0.5f, lp = 0.f, hpC = 0.99f, hpS = 0.f, env = 0.f, envC = 0.001f, vG = 1.f, vC = 0.01f;
    int quiet = 0, quietMin = 7000;
    float urand() { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return (float)(rng >> 8) * (1.f / 16777216.f) * 2.f - 1.f; }
    void init(double s, uint32_t seed, float baseMs, float walkMs, float cents, float lpHz) {
        sr = (float)s; rng = seed ? seed : 1u;
        sp.init(1, (int)(0.5 * s / 70.0) + 48); sp.snapDry = false;
        baseD = baseMs * 1e-3f * sr; walkD = walkMs * 1e-3f * sr; centsAmt = cents;
        int n = 1; while (n < (int)(baseD + walkD + 0.02f * sr) + 8) n <<= 1;
        line.assign((size_t)n, 0.f); lmask = n - 1; lw = 0;
        wT = Walk{}; wP = Walk{};
        wT.c = 1.f - std::exp(-2.f * kPiF * 0.15f / sr);   // timing : ~0,15 Hz
        wP.c = 1.f - std::exp(-2.f * kPiF * 0.6f / sr);    // hauteur : ~0,6 Hz
        lpC = 1.f - std::exp(-2.f * kPiF * lpHz / sr); hpC = std::exp(-2.f * kPiF * 110.f / sr);
        envC = 1.f - std::exp(-1.f / (0.01f * sr)); vC = 1.f - std::exp(-1.f / (0.015f * sr));
        quietMin = (int)(0.15f * sr);
        lp = hpS = env = 0.f; vG = 1.f; quiet = 0; ofs = 0.5f * (urand() + 1.f) * 0.012f * sr;
    }
    void setPeriod(float p) { sp.setPeriod(p); }
    float step(Walk& W, float hold) {
        if (--W.cnt <= 0) { W.tgt = urand(); W.cnt = (int)(hold * sr * (0.6f + 0.4f * (urand() + 1.f))); }
        W.a += (W.tgt - W.a) * W.c; W.b += (W.a - W.b) * W.c;
        return W.b;
    }
    // voiced : la voix est voisée (détecteur de l'appelant) ; le souffle et les consonnes du double sont atténués.
    float process(float x, bool voiced) {
        if (!std::isfinite(x)) x = 0.f;
        const float ax = std::fabs(x);
        env += (ax - env) * (ax > env ? 0.2f : envC);
        if (env < 1e-3f) { if (quiet < quietMin && ++quiet == quietMin) ofs = 0.5f * (urand() + 1.f) * 0.018f * sr; } else quiet = 0;   // nouvelle phrase : attaque redistribuée
        vG += ((voiced ? 1.f : 0.35f) - vG) * vC;
        const float cents = centsAmt * step(wP, 0.9f);
        float y; const float in = x * vG;
        sp.process(&in, &y, std::exp2(cents * (1.f / 1200.f)));
        line[(size_t)lw] = y; lw = (lw + 1) & lmask;
        const float D = baseD + ofs + walkD * step(wT, 2.5f);
        const float p = (float)(lw - 1) - D;
        const int i = (int)std::floor(p);
        const float f = p - (float)i;
        const float* b = line.data();
        const float y0 = b[(i - 1) & lmask], y1 = b[i & lmask], y2 = b[(i + 1) & lmask], y3 = b[(i + 2) & lmask];
        float o = ((((0.5f * (y3 - y0) + 1.5f * (y1 - y2)) * f + (y0 - 2.5f * y1 + 2.f * y2 - 0.5f * y3)) * f) + 0.5f * (y2 - y0)) * f + y1;
        lp += (o - lp) * lpC;              // aigus adoucis (souffle, sifflantes)
        hpS = hpC * hpS + (1.f - hpC) * lp; // grave allégé : le double ne grossit pas le bas
        o = lp - hpS;
        if (!std::isfinite(o)) { o = 0.f; lp = hpS = 0.f; }
        return o;
    }
    int latency() const { return (int)(sp.lz + baseD); }   // retard moyen (le double est volontairement en retard)
};

}  // namespace dsp
