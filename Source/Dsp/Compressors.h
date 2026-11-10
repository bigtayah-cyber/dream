// Compresseurs modélisés (C++17 pur, sans JUCE, header-only, sans allocation, latence nulle). Vague 2B (audit 2, points 3 et 5).
// Socle commun d'après Giannoulis, Massberg, Reiss (JAES 2012) : détecteur crête/RMS, filtre de sidechain (SVF TPT de Zavalishin),
// lien stéréo, calcul de gain à genou, lissage dans le domaine log, mix parallèle SANS plafond (0..1, sec et traité alignés),
// gain de compensation automatique. Trois caractères :
//  - OptoComp  : cellule photo-résistive à deux constantes de temps (allumage ~5 ms, extinction rapide 60 ms + mémoire lente
//                0,5..2,5 s qui se charge avec la durée de compression), d'après le modèle de vactrol et les mesures LA-2A du dossier.
//  - FetComp   : rétroaction (niveau de sortie) résolue implicitement (stable même à 20 µs / ratio 20), attaque plus rapide sur les
//                grands dépassements, relâche qui s'allonge quand la réduction dure, ratios 4/8/12/20 + « all-buttons »,
//                étage de sortie légèrement saturé (H2 proportionnel à la réduction, H3 constant faible). Le gain appliqué passe par un
//                passe-bas 2 pôles à 6 kHz (anti-repliement de la modulation de gain) : attaque effective plancher ~0,1 ms.
//                HEURISTIQUE (pas de publication).
//  - VariMuComp: loi continue g = 1/(1+(Vc/V0)^p) en rétroaction (ratio qui croît avec le niveau, pas de seuil net), relâche double
//                dépendante du programme (positions 5 et 6 du 670), harmoniques paires douces proportionnelles à la commande.
//                HEURISTIQUE (pas de publication).
// Les colorations sont des polynômes de degré <= 3 : aucun repliement tant que 3·f < fs/2 (7,35 kHz à 44,1 kHz), donc pas de
// suréchantillonnage ni de latence.
//
// Interface commune :  prepare(sr) [hors audio] ; reset() ; setParams(p) [audio OK] ; process(L, R, n) [R peut être nullptr : mono] ;
//                      gainReductionDb() [dernière réduction, max des canaux, pour le vumètre].
#pragma once
#include <algorithm>
#include <cmath>

namespace dsp {
namespace comp {

constexpr double kPi = 3.14159265358979323846;
inline float dbToLin(float d) { return std::exp(d * 0.11512925464970228f); }
inline float linToDb(float x) { return 8.685889638065035f * std::log(std::max(x, 1e-6f)); }   // borne -120 dB
inline float coefFor(double seconds, double sr) { return seconds <= 0 ? 0.f : (float)std::exp(-1.0 / (seconds * sr)); }
inline float sane(float x) { return std::isfinite(x) && std::fabs(x) > 1e-20f ? x : 0.f; }

// Passe-haut SVF TPT (Zavalishin, « The Art of VA Filter Design ») pour la sidechain.
struct SvfHp {
    float g = 0, k = 1.41421356f, a1 = 1, ic1 = 0, ic2 = 0; bool on = false;
    void set(double sr, float hz) { on = hz > 1.f; if (!on) return; g = (float)std::tan(kPi * std::min((double)hz, 0.45 * sr) / sr); a1 = 1.f / (1.f + g * (g + k)); }
    void reset() { ic1 = ic2 = 0; }
    float process(float x) {
        if (!on) return x;
        const float v1 = a1 * (ic1 + g * (x - ic2)), v2 = ic2 + g * v1;
        ic1 = sane(2 * v1 - ic1); ic2 = sane(2 * v2 - ic2);
        return x - k * v1 - v2;
    }
};

enum class Detect { Peak, Rms };

// Réglages communs.
struct Common {
    float thresholdDb = -18.f;
    float sidechainHz = 80.f;   // passe-haut de la détection (0 = coupé)
    float link = 1.f;           // 0 = canaux indépendants, 1 = réduction commune (max des deux)
    float mix = 1.f;            // 0..1, parallèle : 1 = tout comprimé (aucun plafond)
    float makeupDb = 0.f;       // ajouté au gain automatique
    bool autoGain = true;       // compense la réduction statique d'un signal de crête -12 dBFS
    Detect detect = Detect::Rms;
    float rmsMs = 10.f;         // fenêtre du RMS
    float peakMs = 10.f;        // retombée du détecteur crête (attaque instantanée) : la réduction ne suit plus chaque alternance
};

// Détecteur + sidechain + lien, partagé par les trois modèles. Rend le niveau en dB par canal (lien appliqué).
struct Sidechain {
    SvfHp hp[2]; float ms[2] = {}, pk[2] = {}, rmsC = 0, pkC = 0, link = 1; Detect det = Detect::Rms;
    void prepare(double sr, const Common& c) { for (auto& h : hp) h.set(sr, c.sidechainHz); rmsC = coefFor(c.rmsMs * 1e-3, sr); pkC = coefFor(c.peakMs * 1e-3, sr); link = std::clamp(c.link, 0.f, 1.f); det = c.detect; }
    void reset() { for (auto& h : hp) h.reset(); ms[0] = ms[1] = pk[0] = pk[1] = 0; }
    void levels(float l, float r, bool stereo, float& dl, float& dr) {
        const int nc = stereo ? 2 : 1; float x[2] = {l, r}, d[2];
        for (int c = 0; c < nc; ++c) {
            const float s = hp[c].process(x[c]);
            if (det == Detect::Peak) { const float a = std::fabs(s); pk[c] = sane(a > pk[c] ? a : pkC * pk[c]); d[c] = linToDb(pk[c]); }
            else { ms[c] = sane(rmsC * ms[c] + (1 - rmsC) * s * s); d[c] = linToDb(std::sqrt(2.f * ms[c])); }   // sinus : crête = RMS·√2
        }
        if (!stereo) { dl = dr = d[0]; return; }
        const float m = std::max(d[0], d[1]);
        dl = d[0] + link * (m - d[0]); dr = d[1] + link * (m - d[1]);
    }
};

// Calcul de gain de Giannoulis : réduction (dB, >= 0) pour un niveau x, seuil t, ratio r, genou w.
inline float giannoulis(float x, float t, float r, float w) {
    const float o = x - t, s = 1.f - 1.f / r;
    if (2 * o < -w) return 0.f;
    if (2 * std::fabs(o) <= w) return s * (o + w / 2) * (o + w / 2) / (2 * w);
    return s * o;
}

// Parallèle + makeup lissés (20 ms) : pas de saut à la modification.
struct OutStage {
    float mix = 1, mixT = 1, mk = 1, mkT = 1, c = 0.999f;
    void prepare(double sr) { c = coefFor(0.02, sr); }
    void target(float m, float makeLin) { mixT = std::clamp(m, 0.f, 1.f); mkT = makeLin; }
    void snap() { mix = mixT; mk = mkT; }
    void tick() { mix = mixT + (mix - mixT) * c; mk = mkT + (mk - mkT) * c; }
    float out(float dry, float wet) const { return dry + mix * (wet * mk - dry); }
};

// Anti-continu à un pôle (10 Hz) pour les termes pairs ajoutés.
struct DcBlock { float x1 = 0, y1 = 0, c = 0.9986f; void prepare(double sr) { c = (float)(1.0 - 2 * kPi * 10.0 / sr); } void reset() { x1 = y1 = 0; }
    float process(float x) { const float y = x - x1 + c * y1; x1 = x; y1 = sane(y); return y; } };

// ======================================================================== OPTIQUE
struct OptoParams {
    Common common;
    bool limit = false;          // « Compress » (ratio 3, genou 12 dB) ou « Limit » (ratio 12, genou 8 dB)
    float speed = 1.f;           // 0,5..2 : multiplie les temps de la cellule (1 = LA-2A)
    OptoParams() { common.detect = Detect::Rms; common.rmsMs = 5.f; common.thresholdDb = -24.f; }
};
class OptoComp {
public:
    void prepare(double sampleRate) { sr = sampleRate; out.prepare(sr); setParams(P); out.snap(); reset(); }
    void reset() { sc.reset(); for (int c = 0; c < 2; ++c) { lf[c] = ls[c] = q[c] = 0; } gr = 0; }
    void setParams(const OptoParams& p) {
        P = p; sc.prepare(sr, P.common);
        const float s = std::clamp(P.speed, 0.5f, 2.f);
        onC = coefFor(0.010 * s, sr);  offC = coefFor(0.060 * s, sr);       // allumage (cellule ~5 ms + RC) / extinction rapide
        chC = coefFor(0.5 * s, sr);                                          // charge de la mémoire
        qOn = coefFor(4.0, sr); qOff = coefFor(8.0, sr);                     // « fatigue » : allonge l'extinction lente
        sr0 = sr * s;
        ratio = P.limit ? 12.f : 3.f; knee = P.limit ? 8.f : 12.f;
        out.target(P.common.mix, dbToLin(P.common.makeupDb + (P.common.autoGain ? staticGrDb(-12.f) : 0.f)));
    }
    float staticGrDb(float levelDb) const { return giannoulis(levelDb, P.common.thresholdDb, ratio, knee); }
    float gainReductionDb() const { return gr; }
    void process(float* L, float* R, int n) {
        const bool st = R != nullptr;
        for (int i = 0; i < n; ++i) {
            out.tick();
            const float xl = sane(L[i]), xr = st ? sane(R[i]) : xl;
            float d[2]; sc.levels(xl, xr, st, d[0], d[1]);
            float g[2] = {1, 1}; gr = 0;
            for (int c = 0; c < (st ? 2 : 1); ++c) {
                // lumière cible : gain = 1/(1+L)  =>  L = 10^(c/20) - 1
                const float lt = dbToLin(staticGrDb(d[c])) - 1.f;
                lf[c] = lt > lf[c] ? onC * lf[c] + (1 - onC) * lt : offC * lf[c] + (1 - offC) * lt;
                // mémoire : se charge depuis la partie rapide, se vide avec tau = 0,5 s · (1 + 4·fatigue)
                if (lf[c] > ls[c]) ls[c] = chC * ls[c] + (1 - chC) * lf[c];
                else ls[c] *= (float)std::exp(-1.0 / (0.5 * (1.0 + 4.0 * q[c]) * sr0));
                q[c] = lt > 0.12f ? qOn * q[c] + (1 - qOn) : qOff * q[c];
                const float lum = std::max(lf[c], 0.45f * ls[c]);
                g[c] = 1.f / (1.f + lum);
                gr = std::max(gr, -linToDb(g[c]));
            }
            if (!st) g[1] = g[0];
            L[i] = out.out(xl, xl * g[0]);
            if (st) R[i] = out.out(xr, xr * g[1]);
        }
    }
private:
    OptoParams P; Sidechain sc; OutStage out;
    double sr = 48000, sr0 = 48000;
    float lf[2] = {}, ls[2] = {}, q[2] = {}, gr = 0, onC = 0, offC = 0, chC = 0, qOn = 0, qOff = 0, ratio = 3, knee = 12;
};

// ======================================================================== FET
struct FetParams {
    Common common;
    int ratio = 4;               // 4, 8, 12 ou 20 (autre valeur : ramenée à la plus proche)
    bool allButtons = false;     // « tous les boutons » : ratio ~20, sur-compression, attaque plus lente, relâche plus courte, plus de grain
    float attackUs = 200.f;      // 20..800 µs
    float releaseMs = 250.f;     // 50..1100 ms
    float color = 1.f;           // 0..1 : saturation de l'étage de sortie
    FetParams() { common.detect = Detect::Peak; common.thresholdDb = -20.f; }
};
class FetComp {
public:
    void prepare(double sampleRate) { sr = sampleRate; out.prepare(sr); dcb[0].prepare(sr); dcb[1].prepare(sr); setParams(P); out.snap(); reset(); }
    void reset() { sc.reset(); for (int c = 0; c < 2; ++c) { g[c] = hold[c] = s1[c] = s2[c] = 0; dcb[c].reset(); } gr = 0; }
    void setParams(const FetParams& p) {
        P = p; sc.prepare(sr, P.common);
        const int rs[4] = {4, 8, 12, 20}; int best = 4;
        for (int r : rs) if (std::abs(r - P.ratio) < std::abs(best - P.ratio)) best = r;
        R = P.allButtons ? 20.f : (float)best;
        ab = P.allButtons ? 0.15f : 0.f;                                    // pente de sur-compression (sortie qui redescend)
        const float att = std::clamp(P.attackUs, 20.f, 800.f) * 1e-6f * (P.allButtons ? 2.f : 1.f);
        const float rel = std::clamp(P.releaseMs, 50.f, 1100.f) * 1e-3f * (P.allButtons ? 0.6f : 1.f);
        attS = att; relC = coefFor(rel, sr);
        holdOn = coefFor(0.3, sr); holdOff = coefFor(2.0, sr);
        lpG = 1.f - (float)std::exp(-2 * kPi * 6000.0 / sr);
        knee = P.allButtons ? 6.f : 2.f;
        k2 = 0.06f * std::clamp(P.color, 0.f, 1.f) * (P.allButtons ? 2.f : 1.f);
        k3 = -0.012f * std::clamp(P.color, 0.f, 1.f);
        out.target(P.common.mix, dbToLin(P.common.makeupDb + (P.common.autoGain ? staticGrDb(-12.f) : 0.f)));
    }
    // Réduction statique (point fixe de la boucle) : R appliqué au-dessus du seuil, plus la sur-compression en « all-buttons ».
    float staticGrDb(float levelDb) const { return giannoulis(levelDb, P.common.thresholdDb, R, knee) + ab * std::max(0.f, levelDb - P.common.thresholdDb); }
    float gainReductionDb() const { return gr; }
    void process(float* L, float* Rr, int n) {
        const bool st = Rr != nullptr;
        for (int i = 0; i < n; ++i) {
            out.tick();
            const float xl = sane(L[i]), xr = st ? sane(Rr[i]) : xl;
            float d[2]; sc.levels(xl, xr, st, d[0], d[1]);
            float y[2] = {xl, xr}; gr = 0;
            for (int c = 0; c < (st ? 2 : 1); ++c) {
                // Rétroaction : la cible dépend du niveau de SORTIE (d - g). On résout g_new = g + b·(cible(g_new) - g) exactement
                // sur la branche linéaire du calcul de gain (genou traité par la cible au point courant) : stable à tout ratio.
                const float over = d[c] - P.common.thresholdDb;
                const float tgtNow = staticGrDb(d[c]);
                const bool attack = tgtNow > g[c];
                float b;
                if (attack) {   // attaque non linéaire : plus rapide quand le dépassement est grand ; ×R compense l'accélération de la boucle
                    const float tau = attS * R / (1.f + std::max(0.f, tgtNow - g[c]) / 10.f);
                    b = 1.f - (float)std::exp(-1.0 / (tau * sr));
                } else {        // relâche dépendante du programme : x0,7 après une crête isolée, jusqu'à x1,6 après une longue réduction
                    b = (1.f - relC) / (0.7f + 0.9f * hold[c]);
                }
                float gn;
                if (over > 0.f && attack) {
                    // cible(g) = (R-1)·(d - g - T) + ab·(d - T)   (forme rétroaction équivalente au point fixe R)
                    const float a = R - 1.f;
                    gn = (g[c] * (1 - b) + b * (a * over + ab * over * R)) / (1 + b * a);
                    gn = std::min(gn, std::max(g[c], tgtNow));
                } else gn = g[c] + b * (tgtNow - g[c]);
                g[c] = std::max(0.f, sane(gn));
                hold[c] = tgtNow > 1.f ? holdOn * hold[c] + (1 - holdOn) : holdOff * hold[c];
                // gain appliqué lissé par un passe-bas 2 pôles à 6 kHz : la modulation de gain ne dépasse plus Nyquist (repliement)
                s1[c] += lpG * (g[c] - s1[c]); s2[c] += lpG * (s1[c] - s2[c]);
                const float w = y[c] * dbToLin(-s2[c]);
                // étage de sortie classe A : H2 proportionnel à la réduction (audit : y += k·(gr/20)·y²), H3 faible constant
                const float wc = std::clamp(w, -2.f, 2.f);
                y[c] = w + dcb[c].process(k2 * (0.15f + s2[c] / 20.f) * wc * wc) + k3 * wc * wc * wc;
                gr = std::max(gr, g[c]);
            }
            L[i] = out.out(xl, y[0]);
            if (st) Rr[i] = out.out(xr, y[1]);
        }
    }
private:
    FetParams P; Sidechain sc; OutStage out; DcBlock dcb[2];
    double sr = 48000;
    float g[2] = {}, hold[2] = {}, s1[2] = {}, s2[2] = {}, lpG = 0.5f, gr = 0, R = 4, ab = 0, attS = 2e-4f, relC = 0, holdOn = 0, holdOff = 0, knee = 2, k2 = 0, k3 = 0;
};

// ======================================================================== VARIABLE-MU
struct VariMuParams {
    Common common;
    float curve = 2.f;           // p, 1..3 : ratio asymptotique ~ 1 + p (genou très large)
    int timeConstant = 5;        // 1..6, d'après le 670 : 1..4 fixes, 5 et 6 doubles (programme)
    float color = 1.f;           // 0..1 : harmoniques paires (push-pull déséquilibré)
    VariMuParams() { common.detect = Detect::Rms; common.rmsMs = 15.f; common.thresholdDb = -20.f; }
};
class VariMuComp {
public:
    void prepare(double sampleRate) { sr = sampleRate; out.prepare(sr); dcb[0].prepare(sr); dcb[1].prepare(sr); setParams(P); out.snap(); reset(); }
    void reset() { sc.reset(); for (int c = 0; c < 2; ++c) { gf[c] = gs[c] = 0; u[c] = 0; dcb[c].reset(); } gr = 0; }
    void setParams(const VariMuParams& p) {
        P = p; sc.prepare(sr, P.common);
        pw = std::clamp(P.curve, 1.f, 3.f);
        // (attaque, relâche rapide, relâche lente) en s ; positions 5-6 : la lente ne se charge qu'en 2 s environ
        static const float tc[6][3] = {{0.0002f, 0.3f, 0.3f}, {0.0002f, 0.8f, 0.8f}, {0.0004f, 2.f, 2.f}, {0.0008f, 5.f, 5.f}, {0.0002f, 0.4f, 6.f}, {0.0004f, 0.3f, 12.f}};
        const int t = std::clamp(P.timeConstant, 1, 6) - 1;
        dual = t >= 4;
        aC = coefFor(std::max(tc[t][0], 0.001f), sr);   // l'attaque utile est bornée par le RMS (15 ms) : 1 ms mini
        rfC = coefFor(tc[t][1], sr); rsC = coefFor(tc[t][2], sr); sChC = coefFor(2.0, sr);
        k2 = 0.05f * std::clamp(P.color, 0.f, 1.f);
        out.target(P.common.mix, dbToLin(P.common.makeupDb + (P.common.autoGain ? staticGrDb(-12.f) : 0.f)));
    }
    // Rétroaction : entrée A = sortie·(1 + (sortie/V0)^p)  =>  on résout u + u^(p+1) = a (u = sortie/V0, a = A/V0).
    static float solveU(float a, float p, float u0) {
        // borne supérieure de la racine : min(a, a^(1/(p+1))) ; Newton converge de façon monotone depuis le dessus (f convexe)
        const float ub = std::min(a, std::pow(a, 1.f / (p + 1.f)));
        float u = u0 > 0.f ? std::min(u0, ub) : ub;
        for (int k = 0; k < 6; ++k) { const float up = std::pow(u, p), f = u + up * u - a, df = 1.f + (p + 1.f) * up; u = std::max(0.f, u - f / df); }
        return u;
    }
    float staticGrDb(float levelDb) const {
        const float a = dbToLin(levelDb - P.common.thresholdDb);
        return linToDb(a / std::max(1e-9f, solveU(a, pw, a)));
    }
    float gainReductionDb() const { return gr; }
    void process(float* L, float* R, int n) {
        const bool st = R != nullptr;
        for (int i = 0; i < n; ++i) {
            out.tick();
            const float xl = sane(L[i]), xr = st ? sane(R[i]) : xl;
            float d[2]; sc.levels(xl, xr, st, d[0], d[1]);
            float y[2] = {xl, xr}; gr = 0;
            for (int c = 0; c < (st ? 2 : 1); ++c) {
                const float a = dbToLin(std::min(d[c] - P.common.thresholdDb, 60.f));
                u[c] = solveU(a, pw, u[c] > 0 ? u[c] : a);
                const float tgt = std::max(0.f, linToDb(a / std::max(1e-9f, u[c])));
                gf[c] = tgt > gf[c] ? aC * gf[c] + (1 - aC) * tgt : rfC * gf[c] + (1 - rfC) * tgt;
                if (dual) gs[c] = tgt > gs[c] ? sChC * gs[c] + (1 - sChC) * tgt : rsC * gs[c] + (1 - rsC) * tgt;
                const float g = sane(dual ? std::max(gf[c], gs[c]) : gf[c]);
                const float w = y[c] * dbToLin(-g), wc = std::clamp(w, -2.f, 2.f);
                y[c] = w + dcb[c].process(k2 * std::min(1.f, g / 12.f) * wc * wc);
                gr = std::max(gr, g);
            }
            L[i] = out.out(xl, y[0]);
            if (st) R[i] = out.out(xr, y[1]);
        }
    }
private:
    VariMuParams P; Sidechain sc; OutStage out; DcBlock dcb[2];
    double sr = 48000;
    float gf[2] = {}, gs[2] = {}, u[2] = {}, gr = 0, pw = 2, aC = 0, rfC = 0, rsC = 0, sChC = 0, k2 = 0; bool dual = true;
};

}  // namespace comp
}  // namespace dsp
