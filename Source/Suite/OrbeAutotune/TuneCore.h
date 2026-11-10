// Orbe Autotune, moteur « Orbe » (dreamaws). C++17 pur, sans JUCE : testable hors-ligne (tests/tune_core_test.cpp).
// Chaîne : analyse (YIN décimé + affinage NSDF plein débit) -> suivi de Viterbi à décision retardée (candidats + état non voisé)
// -> courbe de correction (centre / expression séparés, hystérésis de note, glissandos respectés, vitesse 0 = robot)
// -> resynthèse « excitation » : blanchiment LPC en treillis, raccords synchrones à la période sur le résidu, recoloration par la
//    même enveloppe (formants préservés), segments non voisés rendus intacts (signal sec exactement retardé).
// Deux latences : Direct (<= 256 échantillons, ~5 ms) et Qualité (anticipation : analyse centrée sur l'échantillon décalé).
// Temps réel : aucune allocation hors prepare(), sorties toujours finies. Documentation : docs/tune-algo.md.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace tc {

constexpr float kPi = 3.14159265358979323846f;
inline float sat(float v) { return v < 0.f ? 0.f : v > 1.f ? 1.f : v; }
inline float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }
inline int pcOf(int n) { return ((n % 12) + 12) % 12; }
inline float hz2midi(float f) { return 69.f + 12.f * std::log2(f / 440.f); }

struct Lp2 {
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void set(double sr, double fc) {
        const double w = 2.0 * 3.14159265358979 * fc / sr, al = std::sin(w) / (2.0 * 0.7071), c = std::cos(w), a0 = 1.0 + al;
        b0 = (float)((1 - c) / 2 / a0); b1 = (float)((1 - c) / a0); b2 = b0; a1 = (float)(-2 * c / a0); a2 = (float)((1 - al) / a0); z1 = z2 = 0.f;
    }
    float run(float x) { const float y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
};

struct Params {
    float speedMs = 12.f;     // constante de temps de la correction ; 0 = robot (correction instantanée, expression aplatie)
    float tolCents = 0.f;     // zone morte autour de la note (sur le centre de la note, pas sur le vibrato)
    float amount = 1.f;       // quantité de correction 0..1
    float expr = 1.f;         // expression conservée (vibrato, micro-variations) quand la vitesse est « naturelle »
    float offsetCents = 0.f;  // décalage ajouté (transposition, vibrato de synthèse) ; appliqué aux segments voisés seulement
    int mask = 0;             // notes autorisées (bit 0 = C) ; 0 = pas de correction vers une note
    int forcedNote = -1;      // note MIDI imposée (octave la plus proche de la voix), -1 = aucune
    bool formants = true;     // préservation des formants (LPC) ; sinon le timbre suit la hauteur
    // édition note par note (prioritaire sur tout le reste) : cible MIDI fractionnaire, < 0 = aucune
    float editTarget = -1.f, editAmount = 1.f, editExpr = -1.f, editSpeedMs = -1.f;
};

// État publié à chaque trame d'analyse (aligné sur l'échantillon qui sort du décaleur).
struct State {
    bool voiced = false;
    float pitch = 0.f;    // hauteur détectée (MIDI fractionnaire), 0 si non voisé
    float center = 0.f;   // centre de note (hauteur sans vibrato)
    float note = -1.f;    // note visée (MIDI, fractionnaire en édition), -1 si aucune
    float shift = 0.f;    // correction appliquée (demi-tons, hors offset)
    float conf = 0.f;
};

class TuneCore {
public:
    static constexpr int kRing = 16384, kMask = kRing - 1, kK = 5, kF = 16, kKRing = 128, kMaxP = 80;

    void prepare(double sampleRate, int directLatency, int qualityLatency) {
        sr = sampleRate;
        hop = sr >= 88000.0 ? 256 : 128;
        D = sr >= 88000.0 ? 8 : 4;
        decSr = (float)(sr / D);
        tMax = (int)(decSr / 62.f); tMin = std::max(2, (int)(decSr / 1150.f));
        W = ((std::max(tMax + 8, 192) + 3) / 4) * 4;
        Wf = W * D;
        latD = directLatency; latQ = qualityLatency;
        P = std::min(kMaxP, (int)std::lround(sr / 700.0) + 4);   // 67 à 44,1 kHz : enveloppe fine (mesuré : meilleur que fs/1000)
        Nw = 1; while (Nw < (int)(sr * 0.023)) Nw <<= 1;
        for (auto* v : {&mid, &dec, &res[0], &res[1], &raw[0], &raw[1]}) v->assign((size_t)kRing, 0.f);
        seg.assign((size_t)(W + tMax + 2), 0.f); cm.assign((size_t)tMax + 3, 1.f);
        lpcWin.resize((size_t)Nw); lpcBuf.resize((size_t)Nw);
        for (int j = 0; j < Nw; ++j) lpcWin[(size_t)j] = 0.5f - 0.5f * std::cos(2.f * kPi * ((float)j + 0.5f) / (float)Nw);
        Kr.assign((size_t)kKRing * kMaxP, 0.f);
        aa1.set(sr, 0.21 * decSr); aa2.set(sr, 0.21 * decSr);
        aFric = 1.f - std::exp(-1.f / (float)(0.003 * sr));
        wetRate = 1.f / (float)(0.004 * sr);
        reset();
    }
    void setQuality(bool q) { if (q != quality) { quality = q; reset(); } }
    bool isQuality() const { return quality; }
    int latency() const { return quality ? latQ : latD; }
    int latencyDirect() const { return latD; }
    int latencyQuality() const { return latQ; }
    int hopSize() const { return hop; }
    int samplesToHop() const { return hop - hopC; }
    void setParams(const Params& p) { Pm = p; }
    const Params& params() const { return Pm; }
    const State& state() const { return st; }
    float appliedShift() const { return sCur; }   // demi-tons, hors offset, à l'échantillon courant

    // Remise à zéro complète, sans allocation.
    void reset() {
        for (auto* v : {&mid, &dec, &res[0], &res[1], &raw[0], &raw[1]}) std::fill(v->begin(), v->end(), 0.f);
        std::fill(Kr.begin(), Kr.end(), 0.f);
        std::memset(kA, 0, sizeof kA); std::memset(dkA, 0, sizeof dkA); std::memset(kS, 0, sizeof kS); std::memset(rS, 0, sizeof rS); lagB = 60.0;
        std::memset(bA, 0, sizeof bA); std::memset(bS, 0, sizeof bS);
        for (auto& f : fr) f = Frame{};
        nAbs = 0; hopC = 0; decPh = 0; decW = 0; frameIdx = 0; hopIdx = 0;
        aa1.z1 = aa1.z2 = aa2.z1 = aa2.z2 = 0.f;
        pePrev[0] = pePrev[1] = 0.f; dePrev[0] = dePrev[1] = 0.f; e1 = e2 = 0.f; prevMid = 0.f;
        rmsPeak = 0.f;
        L = latency();
        LF = 0; Dfr = 0;
        if (quality) {
            const int A = Wf / 2 + (int)(0.5 * sr / 150.0);   // centre de l'analyse : fenêtre / 2 + période / 2 (voix ~150 Hz)
            Dfr = std::min(kF - 4, std::max(0, (int)std::lround((double)(L - A) / hop)));
            LF = std::min(3, Dfr);
        }
        sp.reset(this);
        st = State{}; sCur = sTgt = sStep = 0.f; offCur = offStep = 0.f; wetG = 0.f; wetTgt = 0.f;
        c1 = c2a = c2b = 0.f; vibVar = 0.f; glideRun = 0.f; glideSign = 0; slope = 0.f; pPrev = 0.f; noteCur = -1.f; unvHops = 1000; glide = 0.f; c1Prev = 0.f; heldS = 0.f;
        dryA = 0.f;
    }

    // Traite n échantillons sur place. r peut être nul (mono).
    void process(float* l, float* r, int n) {
        const int nch = r ? 2 : 1;
        float* io[2] = {l, r};
        for (int i = 0; i < n; ++i) {
            float x[2] = {0.f, 0.f};
            for (int c = 0; c < nch; ++c) { x[c] = io[c][i]; if (!std::isfinite(x[c])) x[c] = 0.f; x[c] = clampf(x[c], -16.f, 16.f); }
            const float m = nch == 2 ? 0.5f * (x[0] + x[1]) : x[0];
            const int w = (int)(nAbs & kMask);
            mid[(size_t)w] = m;
            // détecteur rapide de consonnes (énergie de la dérivée / énergie, ~3 ms)
            const float dm = m - prevMid; prevMid = m;
            e1 += aFric * (m * m - e1); e2 += aFric * (dm * dm - e2);
            // décimation pour l'analyse
            const float y = aa2.run(aa1.run(m));
            if (++decPh >= D) { decPh = 0; dec[(size_t)(decW & kMask)] = y; ++decW; }
            // blanchiment (préaccentuation + treillis RIF), coefficients interpolés échantillon par échantillon
            for (int c = 0; c < nch; ++c) {
                raw[c][(size_t)w] = x[c];
                const float pe = x[c] - 0.9f * pePrev[c]; pePrev[c] = x[c];
                res[c][(size_t)w] = latticeA(c, pe);
            }
            if (nch == 1) { raw[1][(size_t)w] = x[0]; res[1][(size_t)w] = res[0][(size_t)w]; }
            for (int j = 0; j < P; ++j) kA[j] += dkA[j];
            ++nAbs;
            if (++hopC >= hop) { hopC = 0; hopUpdate(); }
            // hauteur : rampes par trame
            sCur += sStep; offCur += offStep;
            wetG += clampf(wetTgt - wetG, -wetRate, wetRate);
            const float semi = (wetG > 0.f) ? sCur + offCur * 0.01f : 0.f;
            const float ratio = std::exp2(semi * (1.f / 12.f));
            const float cents = std::fabs(semi) * 100.f;
            const float a = sat((cents - 0.5f) * 0.4f) * wetG;
            float out[2];
            const bool useRes = Pm.formants;
            if (a <= 0.f) {
                sp.rest();
                kAt((double)(nAbs - 1 - L));
                for (int c = 0; c < nch; ++c) out[c] = rawAt(c, L);
                // le treillis de synthèse tourne quand même sur le résidu aligné : son état reste cohérent pour la reprise
                for (int c = 0; c < nch; ++c) { const float e = resAt(c, L); const float yv = latticeS(c, e); dePrev[c] = yv + 0.9f * dePrev[c]; }
            } else {
                float e[2];
                sp.step(this, ratio, useRes, e, nch);
                kAt((double)(nAbs - 1) - (double)sp.d);   // enveloppe de l'instant où le résidu lu a été blanchi
                for (int c = 0; c < nch; ++c) {
                    float wv;
                    if (useRes) { const float yv = latticeS(c, e[c]); wv = yv + 0.9f * dePrev[c]; dePrev[c] = wv; if (!std::isfinite(wv) || std::fabs(wv) > 32.f) { wv = 0.f; dePrev[c] = 0.f; std::memset(bS[c], 0, sizeof bS[c]); } }
                    else { wv = e[c]; const float yv = latticeS(c, resAt(c, L)); dePrev[c] = yv + 0.9f * dePrev[c]; }
                    const float dr = rawAt(c, L);
                    out[c] = dr + a * (wv - dr);
                }
            }
            for (int c = 0; c < nch; ++c) { float o = out[c]; if (!std::isfinite(o)) o = 0.f; io[c][i] = clampf(o, -8.f, 8.f); }
        }
    }

private:
    // ------------------------------------------------------------ anneaux
    float rawAt(int c, int d) const { return raw[c][(size_t)((nAbs - 1 - d) & kMask)]; }
    float resAt(int c, int d) const { return res[c][(size_t)((nAbs - 1 - d) & kMask)]; }
    float midAt(int64_t pos) const { return mid[(size_t)(pos & kMask)]; }

    // ------------------------------------------------------------ treillis LPC
    float latticeA(int c, float x) {
        float f = x, carry = x;
        float* b = bA[c];
        for (int i = 0; i < P; ++i) { const float bo = b[i], nb = kA[i] * f + bo; f += kA[i] * bo; b[i] = carry; carry = nb; }
        return f;
    }
    float latticeS(int c, float e) {
        float* b = bS[c];
        float fs[kMaxP + 1];
        fs[P] = e;
        for (int i = P - 1; i >= 0; --i) fs[i] = fs[i + 1] - kS[i] * b[i];
        float carry = fs[0];
        for (int i = 0; i < P; ++i) { const float nb = kS[i] * fs[i] + b[i]; b[i] = carry; carry = nb; }
        return fs[0];
    }
    // Coefficients de synthèse pour l'instant d'entrée m : même trajectoire que l'analyse (les échantillons [(q)hop, (q+1)hop[
    // vont de K[q-2] vers K[q-1]). La synthèse prend l'enveloppe de l'instant où le résidu lu a été blanchi (et non celle de
    // l'instant de sortie) : sans ce détail, la petite fluctuation de l'enveloppe d'une trame à l'autre devient un bruit de
    // modulation (THD+N d'une voix à 110 Hz : -47 dB -> -86 dB, mesuré).
    void kAt(double m) {
        const int64_t mi = (int64_t)std::floor(m), q = mi >= 0 ? mi / hop : -1;
        const float j = (float)(mi - q * hop) / (float)hop;
        const float* k0 = &Kr[(size_t)(((q - 2) & (kKRing - 1)) * kMaxP)];
        const float* k1 = &Kr[(size_t)(((q - 1) & (kKRing - 1)) * kMaxP)];
        for (int i = 0; i < P; ++i) kS[i] = k0[i] + j * (k1[i] - k0[i]);
    }
    void computeLpc(float* K) {
        const int64_t end = nAbs;
        float pp = midAt(end - Nw - 1);
        for (int j = 0; j < Nw; ++j) { const float v = midAt(end - Nw + j); lpcBuf[(size_t)j] = (v - 0.9f * pp) * lpcWin[(size_t)j]; pp = v; }
        double r[kMaxP + 1];
        for (int lag = 0; lag <= P; ++lag) {
            double s = 0.0;
            for (int j = lag; j < Nw; ++j) s += (double)lpcBuf[(size_t)j] * lpcBuf[(size_t)(j - lag)];
            const double bb = 2.0 * 3.14159265358979 * lagB * lag / sr;   // fenêtre de retard gaussienne : lisse les harmoniques
            r[lag] = s * std::exp(-0.5 * bb * bb);
            rS[lag] += 0.5f * (r[lag] - rS[lag]); r[lag] = rS[lag];   // autocorrélation lissée (~4 ms) : enveloppe moins fluctuante d'une trame à l'autre
        }
        for (int j = 0; j < P; ++j) K[j] = 0.f;
        if (r[0] < 1e-9 * Nw) return;
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

    // ------------------------------------------------------------ analyse de hauteur
    struct Frame {
        int n = 0;
        float midi[kK] = {}, tauD[kK] = {}, cost[kK] = {};
        float costU = 0.f, acc[kK + 1] = {}, conf = 0.f, v = 0.f;
        int bp[kK + 1] = {};
        int64_t end = 0;
        bool valid = false;
        // décision (après retard LF)
        bool dVoiced = false; float dMidi = 0.f, dConf = 0.f; float fric = 0.f;
    };

    void analyze(Frame& F) {
        F.n = 0; F.end = nAbs; F.valid = true;
        const int nn = W + tMax;
        for (int i = 0; i < nn; ++i) seg[(size_t)i] = dec[(size_t)((decW - nn + i) & kMask)];
        const float* a = &seg[(size_t)tMax];
        double e = 0.0;
        for (int j = 0; j < W; ++j) e += (double)a[j] * a[j];
        const float rms = (float)std::sqrt(e / W);
        rmsPeak = std::max(rms, rmsPeak * peakDecay);
        // énergie haute fréquence sur la fenêtre plein débit (consonnes, souffle)
        double s0 = 1e-12, s1 = 0.0; float pv = midAt(nAbs - Wf - 1);
        for (int j = 0; j < Wf; ++j) { const float v = midAt(nAbs - Wf + j), d = v - pv; s0 += (double)v * v; s1 += (double)d * d; pv = v; }
        const float hf = (float)(s1 / (4.0 * s0));
        float cmin = 1.f;
        if (rms > 1e-5f) {
            float cum = 0.f; cm[0] = 1.f;
            for (int tau = 1; tau <= tMax; ++tau) {
                const float* b = a - tau;
                float d0 = 0.f, d1 = 0.f, d2 = 0.f, d3 = 0.f;
                for (int j = 0; j < W; j += 4) {
                    const float v0 = a[j] - b[j], v1 = a[j + 1] - b[j + 1], v2 = a[j + 2] - b[j + 2], v3 = a[j + 3] - b[j + 3];
                    d0 += v0 * v0; d1 += v1 * v1; d2 += v2 * v2; d3 += v3 * v3;
                }
                const float d = d0 + d1 + d2 + d3;
                cum += d;
                cm[(size_t)tau] = cum > 1e-12f ? d * (float)tau / cum : 1.f;
            }
            // candidats : minima locaux assez profonds, pris dans l'ordre des retards croissants (les kK premiers). Garder les
            // meilleurs en valeur serait une erreur : la CMNDF d'un signal périodique est souvent plus basse aux multiples de la
            // période (2T, 3T...) qu'à T ; on perdrait la bonne octave.
            float gmin = 1.f;
            for (int t = tMin + 1; t < tMax; ++t) gmin = std::min(gmin, cm[(size_t)t]);
            const float thr = std::min(0.6f, gmin + 0.3f);
            for (int t = tMin + 1; t < tMax && F.n < kK; ++t) {
                const float v = cm[(size_t)t];
                if (!(v < thr && v <= cm[(size_t)t - 1] && v < cm[(size_t)t + 1])) continue;
                const float a0 = cm[(size_t)t - 1], c0 = cm[(size_t)t + 1], den = a0 - 2.f * v + c0;
                float tf = (float)t, vm = v;
                if (den > 1e-9f) { const float dlt = clampf(0.5f * (a0 - c0) / den, -1.f, 1.f); tf += dlt; vm = v - 0.25f * (a0 - c0) * dlt; }
                const float md = hz2midi(decSr / tf);
                if (md < 34.f || md > 90.f) continue;
                cmin = std::min(cmin, vm);
                const int slot = F.n++;
                F.midi[slot] = md; F.tauD[slot] = tf; F.cost[slot] = std::max(0.f, vm);
            }
        }
        // score de voisement : périodicité x pas de bruit haute fréquence x énergie suffisante
        const float per = 1.f - cmin;
        const float gate = (rms > 2.5e-4f && rms > 0.008f * rmsPeak) ? 1.f : 0.f;
        const float v = sat((per - 0.5f) / 0.3f) * sat((0.42f - hf) / 0.22f) * gate;
        F.v = v; F.fric = hf; F.conf = per;
        // coûts locaux (pénalité de sous-harmonique : un candidat à tau/2 presque aussi bon = probable erreur d'octave basse)
        const float raw0[kK] = {F.cost[0], F.cost[1], F.cost[2], F.cost[3], F.cost[4]};
        for (int k = 0; k < F.n; ++k) {
            float pen = 0.f;
            for (int j = 0; j < F.n; ++j) {   // un candidat à tau/m (m entier >= 2) presque aussi bon : celui-ci est un sous-multiple
                if (j == k || F.tauD[j] >= F.tauD[k]) continue;
                const float m = F.tauD[k] / F.tauD[j], mr = std::round(m);
                if (mr >= 2.f && std::fabs(m - mr) < 0.04f * mr && raw0[j] < raw0[k] + 0.15f) pen = 0.3f;
            }
            F.cost[k] = raw0[k] + pen + 0.9f * (1.f - v);
        }
        F.costU = 0.12f + 0.9f * v;
    }

    void viterbi(Frame& F, const Frame& Pv) {
        const int ns = F.n + 1;   // dernier état = non voisé
        float mn = 1e30f;
        for (int j = 0; j < ns; ++j) {
            const bool uj = j == F.n;
            const float loc = uj ? F.costU : F.cost[j];
            float best = 1e30f; int bi = Pv.n;
            if (!Pv.valid) { best = 0.f; bi = Pv.n; }
            else for (int i = 0; i <= Pv.n; ++i) {
                const bool ui = i == Pv.n;
                float tr;
                if (ui && uj) tr = 0.f;
                else if (ui || uj) tr = 0.22f;
                else { const float dst = std::fabs(F.midi[j] - Pv.midi[i]); tr = std::min(1.2f, 0.035f * dst + (dst > 9.f ? 0.3f : 0.f)); }
                const float c = Pv.acc[i] + tr;
                if (c < best) { best = c; bi = i; }
            }
            F.acc[j] = best + loc; F.bp[j] = bi;
            mn = std::min(mn, F.acc[j]);
        }
        for (int j = 0; j < ns; ++j) F.acc[j] -= mn;
    }

    // Affinage plein débit : NSDF autour du retard décimé, interpolation parabolique.
    float refine(int64_t end, float tauD) const {
        const float tc0 = tauD * (float)D;
        const int R = D + 1, t0 = (int)std::lround(tc0);
        float best = -2.f, sc[2 * 9 + 3]; int bt = 0;
        for (int dt = -R; dt <= R; ++dt) {
            const int tau = t0 + dt;
            double xy = 0.0, ee = 1e-12;
            for (int j = 0; j < Wf; ++j) { const float u = midAt(end - Wf + j), v = midAt(end - Wf + j - tau); xy += (double)u * v; ee += (double)u * u + (double)v * v; }
            const float s = (float)(2.0 * xy / ee);
            sc[dt + R] = s;
            if (s > best) { best = s; bt = dt; }
        }
        if (bt == -R || bt == R) return (float)sr / tc0;
        const float sm = sc[bt + R - 1], s0 = sc[bt + R], spp = sc[bt + R + 1], den = sm - 2.f * s0 + spp;
        float tf = (float)(t0 + bt);
        if (den < -1e-9f) tf += clampf(0.5f * (sm - spp) / den, -0.5f, 0.5f);
        return (float)sr / tf;
    }

    // ------------------------------------------------------------ trame
    void hopUpdate() {
        // LPC : nouvelle enveloppe, interpolée sur la trame qui suit (analyse) ; mémorisée pour la synthèse
        float* Kn = &Kr[(size_t)((hopIdx & (kKRing - 1)) * kMaxP)];
        computeLpc(Kn);
        const float* Kp = &Kr[(size_t)(((hopIdx - 1) & (kKRing - 1)) * kMaxP)];
        for (int j = 0; j < P; ++j) { kA[j] = Kp[j]; dkA[j] = (Kn[j] - Kp[j]) / (float)hop; }
        // NB : les échantillons d'entrée [(h+1)hop, (h+2)hop[ vont de K[h-1] vers K[h] (voir kAt).
        ++hopIdx;

        // hauteur
        Frame& F = fr[frameIdx & (kF - 1)];
        { const float f0 = st.voiced && st.pitch > 0.f ? 440.f * std::exp2((st.pitch - 69.f) / 12.f) : 150.f; lagB = std::max(40.0, std::min(250.0, 0.35 * f0)); }
        const Frame& Pv = fr[(frameIdx - 1) & (kF - 1)];
        analyze(F);
        viterbi(F, Pv);
        int s = 0; for (int j = 1; j <= F.n; ++j) if (F.acc[j] < F.acc[s]) s = j;
        int64_t fi = frameIdx;
        for (int k = 0; k < LF && fr[(fi) & (kF - 1)].valid; ++k) { s = fr[fi & (kF - 1)].bp[s]; --fi; }
        Frame& Dc = fr[fi & (kF - 1)];
        if (Dc.valid && s < Dc.n) { Dc.dVoiced = true; Dc.dMidi = hz2midi(refine(Dc.end, Dc.tauD[s])); Dc.dConf = Dc.conf; }
        else { Dc.dVoiced = false; Dc.dMidi = 0.f; Dc.dConf = Dc.conf; }
        ++frameIdx;
        // trame utilisée pour la correction : celle dont le centre d'analyse tombe sur l'échantillon qui sort
        const Frame& U = fr[(frameIdx - 1 - Dfr) & (kF - 1)];
        const bool fricNow = !quality && e2 > 0.30f * 4.f * (e1 + 1e-12f);
        correct(U.valid && U.dVoiced && !fricNow, U.dMidi, U.dConf);
    }

    // ------------------------------------------------------------ courbe de correction
    void correct(bool voiced, float pRaw, float conf) {
        const float dt = (float)hop / (float)sr;
        // Prédiction : l'estimation de hauteur décrit l'instant (fin de trame - fenêtre/2 - période/2) ; l'échantillon qui sort
        // est l'entrée d'il y a L. L'écart (positif en Direct, ~0 en Qualité) est compensé par extrapolation linéaire.
        float p = pRaw;
        if (voiced && pRaw > 0.f) {
            const float T0 = (float)sr / (440.f * std::exp2((pRaw - 69.f) / 12.f));
            const float lead = ((float)(Dfr * hop) + 0.5f * (float)Wf + 0.5f * T0 - (float)L) / (float)sr;
            if (unvHops == 0 && pPrev > 0.f) slope += (1.f - std::exp(-dt / 0.008f)) * ((pRaw - pPrev) / dt - slope); else slope = 0.f;
            pPrev = pRaw;
            p = pRaw + clampf(slope * lead, -0.5f, 0.5f);
        } else pPrev = 0.f;
        const bool edit = Pm.editTarget >= 0.f;
        const float speed = edit && Pm.editSpeedMs >= 0.f ? Pm.editSpeedMs : Pm.speedMs;
        const float exprU = edit && Pm.editExpr >= 0.f ? Pm.editExpr : Pm.expr;
        const float amount = edit ? Pm.editAmount : Pm.amount;
        // expression conservée : nulle à vitesse 0 (robot), pleine à partir de 50 ms
        const float h = sat(exprU) * sat((speed - 5.f) / 45.f);
        float desired = 0.f;
        if (voiced) {
            if (unvHops * dt > 0.06f || c1 <= 0.f) {   // début de phrase : centres repartis de la hauteur, correction depuis 0
                c1 = c2a = c2b = c1Prev = p; noteCur = -1.f; vibVar = 0.f; glide = 0.f; glideRun = 0.f; glideSign = 0; heldS = 0.f;
                sCur = sTgt = 0.f;
            }
            unvHops = 0;
            const float a1 = 1.f - std::exp(-dt / 0.030f), a2 = 1.f - std::exp(-dt / 0.090f);
            c1 += a1 * (p - c1);
            c2a += a2 * (p - c2a); c2b += a2 * (c2a - c2b);
            if (std::fabs(c1 - c2b) > 0.7f) { c2a = c2b = c1; }   // changement de note : le centre lent repart
            const float dv = p - c2b;
            vibVar += (1.f - std::exp(-dt / 0.3f)) * (dv * dv - vibVar);
            // glissando : pente du centre rapide de même signe pendant plus de 80 ms (un vibrato de 5 à 6 Hz change de sens avant)
            const float sl = (c1 - c1Prev) / dt; c1Prev = c1;
            const int sg = sl > 2.5f ? 1 : sl < -2.5f ? -1 : 0;
            glideRun = (sg != 0 && sg == glideSign) ? glideRun + dt : 0.f; glideSign = sg;
            glide += (1.f - std::exp(-dt / 0.02f)) * ((glideRun > 0.08f ? 1.f : 0.f) - glide);
            // note visée
            float N = -1.f;
            if (edit) N = Pm.editTarget;
            else if (Pm.forcedNote >= 0) { const float f = (float)Pm.forcedNote; N = f + 12.f * std::round((c1 - f) / 12.f); }
            else if (Pm.mask & 0xFFF) N = pickNote(c1, Pm.mask, noteCur);
            if (N >= 0.f && N != noteCur && noteCur >= 0.f) { c2a = c2b = c1; }
            noteCur = N;
            if (N >= 0.f) {
                float ec = N - c2b;   // erreur du centre
                const float tol = edit ? 0.f : Pm.tolCents * 0.01f;
                ec = ec > tol ? ec - tol : ec < -tol ? ec + tol : 0.f;
                const float flat = -(1.f - h) * (p - c2b);   // part d'expression aplatie
                desired = amount * (ec + flat);
                // glissando volontaire : la correction s'efface (jamais en édition ni en robot)
                if (!edit) desired *= 1.f - h * glide;
                desired = clampf(desired, -12.f, 12.f);
            }
            heldS = desired;
        } else {
            ++unvHops;
            desired = unvHops * dt <= 0.06f ? heldS : 0.f;
        }
        // pendant un glissando reconnu, la correction se retire vite (25 ms) quelle que soit la vitesse
        const float spd = glide > 0.5f && voiced && !edit ? std::min(speed, 25.f) : speed;
        const float al = spd > 0.5f ? 1.f - std::exp(-dt * 1000.f / spd) : 1.f;
        sTgt += al * (desired - sTgt);
        sStep = (sTgt - sCur) / (float)hop;
        offStep = (Pm.offsetCents - offCur) / (float)hop;
        const bool active = voiced && (noteCur >= 0.f || std::fabs(Pm.offsetCents) > 0.4f);
        wetTgt = active ? 1.f : 0.f;
        st.voiced = voiced; st.pitch = voiced ? p : 0.f; st.center = voiced ? c2b : 0.f; st.note = voiced ? noteCur : -1.f; st.shift = sTgt; st.conf = conf;
    }

    static float pickNote(float m, int mask, float cur) {
        int best = -1; float bd = 1e9f;
        const int b = (int)std::lround(m);
        for (int n = b - 7; n <= b + 7; ++n) {
            if (!((mask >> pcOf(n)) & 1)) continue;
            const float d = std::fabs(m - (float)n);
            if (d < bd) { bd = d; best = n; }
        }
        if (best < 0) return -1.f;
        const int ci = (int)std::lround(cur);
        if (cur >= 0.f && ci != best && ((mask >> pcOf(ci)) & 1) && std::fabs(m - (float)ci) < bd + 0.2f) return (float)ci;   // hystérésis
        return (float)best;
    }

    // ------------------------------------------------------------ raccords synchrones à la période
    // Un lecteur à retard variable d (autour de la latence L) dérive à la vitesse (1 - ratio) ; quand il sort de la fenêtre
    // [lo, lo + T0[, il saute d'un nombre entier de périodes, affiné par corrélation normalisée sur le signal (mid), avec un fondu
    // sur le résidu. Dans le domaine du résidu (impulsions glottiques blanchies), le raccord ne déplace pas l'enveloppe.
    struct Splicer {
        float d = 0.f, dOld = 0.f, t0 = 300.f;
        int xf = 0, xfLen = 64, xfCur = 64; bool xfOn = false, moving = false;
        void reset(TuneCore* o) { d = dOld = (float)o->L; xf = 0; xfOn = false; moving = false; t0 = 300.f; }
        void rest() { moving = false; xfOn = false; }
        void step(TuneCore* o, float ratio, bool useRes, float* e, int nch) {
            if (!moving) { d = (float)o->L; moving = true; xfOn = false; }
            const float sr = (float)o->sr;
            const float p = o->st.voiced && o->st.pitch > 0.f ? sr / (440.f * std::exp2((o->st.pitch - 69.f) / 12.f)) : t0;
            t0 = clampf(p, 40.f, 2400.f);
            const bool q = o->quality;
            xfLen = q ? (int)clampf(0.75f * t0, 48.f, 480.f) : (int)clampf(0.5f * t0, 32.f, 160.f);
            const float rate = 1.f - ratio;
            d += rate; if (xfOn) dOld += rate;
            const float L = (float)o->L, minD = 24.f;
            const float lo = std::max(minD, L - 0.5f * t0) + (rate < 0.f ? -rate * (float)xfLen : 0.f);
            if (!xfOn && (d < lo || d >= lo + t0)) splice(o, lo);
            d = std::max(d, 3.f); dOld = std::max(dOld, 3.f);
            float g = 1.f;
            if (xfOn) { g = 0.5f - 0.5f * std::cos(kPi * (float)(xf + 1) / (float)xfCur); if (++xf >= xfCur) xfOn = false; }
            for (int c = 0; c < nch; ++c) {
                const std::vector<float>& b = useRes ? o->res[c] : o->raw[c];
                float v = rd(o, b, d);
                if (g < 1.f) { const float vo = rd(o, b, dOld); v = vo + (v - vo) * g; }
                e[c] = v;
            }
        }
        static float rd(const TuneCore* o, const std::vector<float>& b, float D) {
            const double pos = (double)(o->nAbs - 1) - (double)D;
            const int64_t i = (int64_t)std::floor(pos);
            const float f = (float)(pos - (double)i);
            const float y0 = b[(size_t)((i - 1) & kMask)], y1 = b[(size_t)(i & kMask)], y2 = b[(size_t)((i + 1) & kMask)], y3 = b[(size_t)((i + 2) & kMask)];
            return ((((0.5f * (y3 - y0) + 1.5f * (y1 - y2)) * f + (y0 - 2.5f * y1 + 2.f * y2 - 0.5f * y3)) * f) + 0.5f * (y2 - y0)) * f + y1;
        }
        void splice(TuneCore* o, float lo) {
            int k = (int)std::lround((lo + 0.5f * t0 - d) / t0);
            if (k == 0) k = d < lo ? 1 : -1;
            const float dn = d + (float)k * t0;
            const int R = (int)clampf(t0 * 0.125f, 4.f, 40.f), Lc = std::min(xfLen, 256), D0 = (int)std::lround(d), N0 = (int)std::lround(dn);
            float sc[2 * 40 + 3]; for (auto& v : sc) v = -2.f;
            const int64_t top = o->nAbs - 1;
            // fenêtre de corrélation : de préférence sur les échantillons que le fondu va réellement lire (vers l'avant),
            // sinon (retard trop court, mode Direct) sur ceux qui précèdent
            const int fw = std::min(Lc, std::min(D0, N0 - R) - 3), sgn = fw >= 16 ? 1 : -1, Lw = fw >= 16 ? fw : Lc;
            float eo = 1e-9f; for (int j = 0; j < Lw; ++j) { const float v = o->midAt(top - D0 + sgn * j); eo += v * v; }
            int best = 0; float bs = -3.f;
            for (int dl = -R; dl <= R; ++dl) {
                const int Dn = N0 + dl;
                if (Dn < 3 || Dn + Lw >= kRing - 64) continue;
                float xy = 0.f, en = 1e-9f;
                for (int j = 0; j < Lw; ++j) { const float yo = o->midAt(top - D0 + sgn * j), yn = o->midAt(top - Dn + sgn * j); xy += yo * yn; en += yn * yn; }
                const float s = xy / std::sqrt(eo * en);
                sc[dl + R + 1] = s;
                if (s > bs) { bs = s; best = dl; }
            }
            if (bs < -2.f) best = 0;
            float frac = 0.f;
            if (best > -R && best < R) {
                const float sm = sc[best + R], s0 = sc[best + R + 1], spp = sc[best + R + 2], den = sm - 2.f * s0 + spp;
                if (sm > -1.5f && spp > -1.5f && den < -1e-6f) frac = clampf(0.5f * (sm - spp) / den, -0.5f, 0.5f);
            }
            dOld = d; d = d + (float)(N0 + best - D0) + frac;
            xfCur = xfLen; xf = 0; xfOn = true;
        }
    };

    // ------------------------------------------------------------ membres
    double sr = 48000.0;
    int hop = 128, D = 4, tMax = 180, tMin = 10, W = 192, Wf = 768, P = 26, Nw = 1024;
    int latD = 240, latQ = 706, L = 240, LF = 0, Dfr = 0;
    float decSr = 12000.f, aFric = 0.01f, peakDecay = 0.9995f;
    bool quality = false;
    double lagB = 60.0, rS[kMaxP + 1] = {};   // largeur (Hz) du lissage d'enveloppe, suit f0
    Params Pm;
    State st;
    std::vector<float> mid, dec, res[2], raw[2], seg, cm, lpcWin, lpcBuf, Kr;
    float kA[kMaxP] = {}, dkA[kMaxP] = {}, kS[kMaxP] = {}, bA[2][kMaxP] = {}, bS[2][kMaxP] = {};
    Frame fr[kF];
    Lp2 aa1, aa2;
    int64_t nAbs = 0, decW = 0, frameIdx = 0, hopIdx = 0;
    int hopC = 0, decPh = 0;
    float pePrev[2] = {}, dePrev[2] = {}, e1 = 0.f, e2 = 0.f, prevMid = 0.f, rmsPeak = 0.f;
    float sCur = 0.f, sTgt = 0.f, sStep = 0.f, offCur = 0.f, offStep = 0.f, wetG = 0.f, wetTgt = 0.f, wetRate = 1.f / 200.f;
    float c1 = 0.f, c2a = 0.f, c2b = 0.f, c1Prev = 0.f, vibVar = 0.f, noteCur = -1.f, glide = 0.f, heldS = 0.f, dryA = 0.f;
    float glideRun = 0.f, slope = 0.f, pPrev = 0.f;
    int unvHops = 1000, glideSign = 0;
    Splicer sp;
};

}  // namespace tc
