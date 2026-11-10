// De-esser à détection RELATIVE + « air dynamique » (C++17 pur, sans JUCE, header-only, sans allocation, latence nulle).
// Vague 2B (audit 2, point 8). Principe des de-essers de référence (Zölzer, DAFX ; détection relative = heuristique du dossier) :
//  - détection : rapport (dB) entre l'énergie de la bande sibilante (passe-haut 2e ordre à `freqHz` puis passe-bas 12 kHz) et celle
//    du corps de la voix (passe-bas 2e ordre à 0,6·freqHz). On réduit quand ce rapport dépasse `thresholdDb` : le réglage ne dépend
//    pas du niveau d'entrée (même action à -30 et à -8 dBFS). Plancher absolu `floorDb` : rien sous le bruit de fond.
//  - action « split » : plateau aigu SVF (forme trapézoïdale linéaire d'A. Simper) dont le gain suit la réduction échantillon par
//    échantillon, coin à 0,85·freqHz ; transparence exacte à 0 dB (sortie = entrée). Action « wide » : tout le signal.
//  - air dynamique : plateau aigu du 1er ordre (y += (A-1)·HP1(x), coin à `airHz`) dont le gain A n'est monté que quand la voix est
//    « ouverte » (corps à moins de 15 dB de son maximum récent et au-dessus du plancher) et qu'il n'y a PAS de sibilance (le gain
//    retombe dès que le rapport approche le seuil, 3 dB avant). Résultat : brillance sur les voyelles, aucun sifflement ajouté.
// Stéréo liée (détection sur la somme). Interface : prepare(sr) ; setParams(p) ; process(L, R, n) (R nullptr = mono) ;
// reductionDb() / airDb() pour l'affichage.
#pragma once
#include <algorithm>
#include <cmath>

namespace dsp {
namespace deess {

constexpr double kPi = 3.14159265358979323846;
inline float sane(float x) { return std::isfinite(x) && std::fabs(x) > 1e-20f ? x : 0.f; }
inline float coefFor(double s, double sr) { return (float)std::exp(-1.0 / (s * sr)); }

// SVF TPT (Zavalishin) : sorties passe-bas / passe-haut.
struct Svf {
    float g = 0, k = 1.41421356f, a1 = 1, ic1 = 0, ic2 = 0, lp = 0, hp = 0;
    void set(double sr, double hz, float q = 0.7071f) { g = (float)std::tan(kPi * std::min(hz, 0.45 * sr) / sr); k = 1.f / q; a1 = 1.f / (1.f + g * (g + k)); }
    void reset() { ic1 = ic2 = 0; }
    void process(float x) {
        const float v1 = a1 * (ic1 + g * (x - ic2)), v2 = ic2 + g * v1;
        ic1 = sane(2 * v1 - ic1); ic2 = sane(2 * v2 - ic2);
        lp = v2; hp = x - k * v1 - v2;
    }
};
// Plateau aigu SVF à gain modulable par échantillon (A. Simper, « Linear Trapezoidal Integrated SVF »). gainDb <= 0 ici.
struct ShelfSvf {
    float t = 0, k = 1.41421356f, ic1 = 0, ic2 = 0;
    void set(double sr, double hz) { t = (float)std::tan(kPi * std::min(hz, 0.45 * sr) / sr); }
    void reset() { ic1 = ic2 = 0; }
    float process(float x, float gainDb) {
        const float A = std::exp(gainDb * 0.05756463f), g = t * std::sqrt(A);
        const float a1 = 1.f / (1.f + g * (g + k)), a2 = g * a1, a3 = g * a2;
        const float v3 = x - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = sane(2 * v1 - ic1); ic2 = sane(2 * v2 - ic2);
        return A * A * x + k * (1.f - A) * A * v1 + (1.f - A * A) * v2;
    }
};
// Passe-haut du 1er ordre TPT.
struct Hp1 {
    float G = 0, s = 0;
    void set(double sr, double hz) { const float g = (float)std::tan(kPi * std::min(hz, 0.45 * sr) / sr); G = g / (1 + g); }
    void reset() { s = 0; }
    float process(float x) { const float v = (x - s) * G, lp = v + s; s = sane(lp + v); return x - lp; }
};

struct Params {
    float thresholdDb = 0.f;     // -12..+12 : rapport sibilance/corps au-delà duquel on réduit
    float ratio = 3.f;           // pente de réduction au-dessus du seuil
    float rangeDb = 12.f;        // réduction maximale
    float freqHz = 5500.f;       // début de la bande sibilante
    bool wide = false;           // false : bande haute seule ; true : tout le signal
    float floorDb = -60.f;       // plancher absolu de détection
    float airDb = 0.f;           // 0..+8 : air dynamique
    float airHz = 9000.f;        // coin du plateau d'air
};

class DeEsser {
public:
    void prepare(double sampleRate) { sr = sampleRate; setParams(P); reset(); }
    void reset() { for (int c = 0; c < 2; ++c) { split[c].reset(); air[c].reset(); } sibHp.reset(); sibLp.reset(); body.reset(); es = eb = 0; red = 0; airG = 0; bodyPk = -120; }
    void setParams(const Params& p) {
        P = p;
        P.freqHz = std::clamp(P.freqHz, 2000.f, (float)(0.4 * sr));
        for (auto& s : split) s.set(sr, 0.85 * P.freqHz);
        sibHp.set(sr, P.freqHz); sibLp.set(sr, std::min(12000.0, 0.45 * sr)); body.set(sr, 0.6 * P.freqHz);
        for (auto& a : air) a.set(sr, P.airHz);
        eC = coefFor(0.003, sr);
        atkC = coefFor(0.001, sr); relC = coefFor(0.06, sr);
        airAtk = coefFor(0.03, sr); airRel = coefFor(0.012, sr);
        pkRel = (float)(6.0 / sr);   // le maximum récent du corps redescend de 6 dB/s
    }
    float reductionDb() const { return red; }
    float airDb() const { return airG; }
    void process(float* L, float* R, int n) {
        const bool st = R != nullptr;
        for (int i = 0; i < n; ++i) {
            const float xl = sane(L[i]), xr = st ? sane(R[i]) : xl, m = st ? 0.5f * (xl + xr) : xl;
            sibHp.process(m); sibLp.process(sibHp.hp); body.process(m);
            es = sane(eC * es + (1 - eC) * sibLp.lp * sibLp.lp);
            eb = sane(eC * eb + (1 - eC) * body.lp * body.lp);
            const float sDb = 4.343f * std::log(es + 1e-14f), bDb = 4.343f * std::log(eb + 1e-14f);
            const float tot = 4.343f * std::log(es + eb + 1e-14f) + 3.f;   // niveau crête approx. (dBFS)
            const float rel = sDb - bDb;
            // réduction cible : relative, au-dessus du plancher absolu
            float tgt = 0.f;
            if (tot > P.floorDb) tgt = std::min(P.rangeDb, std::max(0.f, rel - P.thresholdDb) * (1.f - 1.f / std::max(1.f, P.ratio)));
            red = tgt > red ? atkC * red + (1 - atkC) * tgt : relC * red + (1 - relC) * tgt;
            // air : voix ouverte = corps proche de son maximum récent ; coupé dès que la sibilance approche le seuil
            bodyPk = std::max(bDb, bodyPk - pkRel);
            const float open = std::clamp((bDb - (bodyPk - 15.f)) / 9.f, 0.f, 1.f) * (bDb + 3.f > P.floorDb + 10.f ? 1.f : 0.f);
            const float noSib = std::clamp((P.thresholdDb - rel) / 3.f, 0.f, 1.f);
            const float airT = P.airDb * open * noSib;
            airG = airT > airG ? airAtk * airG + (1 - airAtk) * airT : airRel * airG + (1 - airRel) * airT;
            const float gs = std::exp(-red * 0.11512925f), ga = std::exp(airG * 0.11512925f) - 1.f, r = red > 1e-4f ? red : 0.f;
            float x[2] = {xl, xr};
            for (int c = 0; c < (st ? 2 : 1); ++c) {
                float y;
                if (P.wide) y = x[c] * gs;
                else y = split[c].process(x[c], -r);
                y += ga * air[c].process(y);
                x[c] = y;
            }
            L[i] = x[0];
            if (st) R[i] = x[1];
        }
    }
private:
    Params P;
    ShelfSvf split[2];
    Svf sibHp, sibLp, body;
    Hp1 air[2];
    double sr = 48000;
    float es = 0, eb = 0, red = 0, airG = 0, bodyPk = -120, eC = 0, atkC = 0, relC = 0, airAtk = 0, airRel = 0, pkRel = 0;
};

}  // namespace deess
}  // namespace dsp
