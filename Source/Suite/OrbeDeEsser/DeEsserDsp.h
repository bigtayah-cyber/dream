// Moteur d'Orbe DeEsser : détection sur bande réglable (passe-bande RBJ), calcul de gain doux/net, lissage attaque/release,
// lookahead court, application large-bande ou split (seule la bande de sibilance est atténuée). Sans JUCE.
#pragma once
#include "../OrbeDsp/OrbeDsp.h"

namespace des {
struct Params {
    float freq = 6500.f, width = 1.5f;   // Hz ; octaves (0.5..3)
    float thr = -30.f, range = 10.f;     // dBFS ; dB de réduction maximale
    float atk = 0.5f, rel = 40.f;        // ms
    bool split = true, sharp = false, listen = false;  // Split / large-bande ; Net / Doux ; écoute de la bande détectée
    float link = 1.f;        // 0..1 : liaison stéréo du détecteur (1 = niveau commun G/D comme avant, 0 = chaque voie seule)
    float chAmt = 0.f;       // 0..1 : détecteur « CH » (2,5 à 5 kHz, « ch », « j », « ge ») : part de la plage appliquée ; 0 = coupé
};
constexpr float kLookMaxMs = 10.f;
inline double qFromOctaves(double oct) { return 1.0 / (2.0 * std::sinh(std::log(2.0) / 2.0 * oct)); }

class Engine {
public:
    // lookMs : lookahead (0 à 10 ms), donc latence ; ne change que dans prepare(). Défaut 1 ms = ancien comportement.
    void prepare(double sampleRate, float lookMs = 1.f) {
        sr = (float)sampleRate; look = std::max(0, (int)std::lround((double)std::min(kLookMaxMs, std::max(0.f, lookMs)) * 0.001 * sr));
        for (int c = 0; c < 2; ++c) { dly[c].init(look); det[c].reset(); band[c].reset(); chDet[c].reset(); chBand[c].reset(); lowDet[c].reset(); envC[c] = 0; grC[c] = 0; }
        env = 0; grDb = 0; envCh = envLow = 0; grCh = 0; coefsFor = -1; chFor = -1;
        autoBuf.assign(5000, 0.f); autoN = 0; autoCnt = 0; autoState.store(0);
    }
    void set(const Params& np) { p = np; if (np.freq != coefsFor || np.width != widthFor || chFor < 0) updateCoefs(); }
    // Auto-seuil : écoute 5 s (le moteur tourne normalement), puis propose un seuil 4 dB au-dessus de la médiane des sifflantes (15 % de trames les plus fortes)
    void startAuto() { autoN = 0; autoCnt = 0; autoState.store(1); }
    int autoStatus() const { return autoState.load(); }          // 0 inactif, 1 écoute, 2 résultat prêt
    float autoProgress() const { return autoState.load() == 1 ? (float)autoN / 5000.f : autoState.load() == 2 ? 1.f : 0.f; }
    float takeAutoThreshold() { autoState.store(0); return autoThr.load(); }
    int latency() const { return look; }
    float reduction() const { return grMax; }   // max de réduction sur le dernier bloc (dB, positif)
    void resetMeter() { grMax = 0.f; }

    void process(const float* inL, const float* inR, float* outL, float* outR, int n) {
        const float aEnv = std::exp(-1.f / (0.0002f * sr)), rEnv = std::exp(-1.f / (0.004f * sr));
        const float aG = std::exp(-1.f / (std::max(0.05f, p.atk) * 1e-3f * sr)), rG = std::exp(-1.f / (std::max(1.f, p.rel) * 1e-3f * sr));
        const float knee = p.sharp ? 0.5f : 8.f, ratio = p.sharp ? 20.f : 4.f;
        const float link = std::min(1.f, std::max(0.f, p.link)), chA = std::min(1.f, std::max(0.f, p.chAmt));
        const bool stereo = inR != nullptr;
        auto redOf = [&](float over) {
            float red = 0.f;
            if (over > knee / 2) red = over * (1.f - 1.f / ratio);
            else if (over > -knee / 2) { const float t = over + knee / 2; red = (1.f - 1.f / ratio) * t * t / (2.f * knee); }
            return red;
        };
        for (int i = 0; i < n; ++i) {
            // NaN/inf d'un plugin précédent remplacés par 0 : sinon filtres, enveloppe et lookahead restent NaN pour toujours (G2)
            const float x0 = kit::finiteBits(inL[i]) ? inL[i] : 0.f, x1 = inR ? (kit::finiteBits(inR[i]) ? inR[i] : 0.f) : x0;
            const float b0 = det[0].process(x0), b1 = det[1].process(x1);
            const float a = std::max(std::fabs(b0), std::fabs(b1));
            env = a > env ? aEnv * env + (1 - aEnv) * a : rEnv * env + (1 - rEnv) * a;
            if (link < 0.999f) for (int c = 0; c < 2; ++c) { const float ac = std::fabs(c == 0 ? b0 : b1); envC[c] = ac > envC[c] ? aEnv * envC[c] + (1 - aEnv) * ac : rEnv * envC[c] + (1 - rEnv) * ac; }
            if (autoState.load(std::memory_order_relaxed) == 1 && ++autoCnt >= (int)(0.001f * sr)) { autoCnt = 0; autoSample(odsp::lin2db(env)); }
            float g[2];
            for (int c = 0; c < 2; ++c) {
                const float lvl = link >= 0.999f ? env : link * env + (1.f - link) * envC[c];
                const float red = std::min(redOf(odsp::lin2db(lvl) - p.thr), p.range);
                float& gd = c == 0 ? grDb : grC[1];
                gd = red > gd ? aG * gd + (1 - aG) * red : rG * gd + (1 - rG) * red;   // lissage en dB
                grMax = std::max(grMax, gd);
                g[c] = odsp::db2lin(-gd);
                if (!stereo) { g[1] = g[0]; break; }   // mono : la voie droite n'existe pas
            }
            if (link >= 0.999f) { grC[1] = grDb; g[1] = g[0]; }
            // détecteur « CH » : seulement sur les segments où la bande 2,5-5 kHz domine le bas du spectre (consonne non voisée, pas une voyelle brillante)
            float gCh = 1.f;
            if (chA > 0.001f) {
                const float c0 = chDet[0].process(x0), c1 = chDet[1].process(x1), l0 = lowDet[0].process(x0), l1 = lowDet[1].process(x1);
                const float ac = std::max(std::fabs(c0), std::fabs(c1)), al = std::max(std::fabs(l0), std::fabs(l1));
                envCh = ac > envCh ? aEnv * envCh + (1 - aEnv) * ac : rEnv * envCh + (1 - rEnv) * ac;
                envLow = al > envLow ? aEnv * envLow + (1 - aEnv) * al : 0.9995f * envLow + 0.0005f * al;   // bas du spectre : retombée lente (~40 ms)
                const bool gate = envCh > 0.8f * envLow;
                const float red = gate ? std::min(redOf(odsp::lin2db(envCh) - p.thr), p.range * chA) : 0.f;
                grCh = red > grCh ? aG * grCh + (1 - aG) * red : rG * grCh + (1 - rG) * red;
                grMax = std::max(grMax, grCh); gCh = odsp::db2lin(-grCh);
            } else { grCh = 0.f; envCh = envLow = 0.f; }
            float o[2];
            for (int c = 0; c < 2; ++c) {
                const float x = c == 0 ? x0 : x1;
                const float d = dly[c].tick(x);                        // audio retardé du lookahead
                const float bd = band[c].process(d);                    // sa bande de sibilance
                const float bc = chA > 0.001f ? chBand[c].process(d) : 0.f;
                o[c] = p.listen ? bd + bc : (p.split ? d + (g[c] - 1.f) * bd + (gCh - 1.f) * bc : d * g[c] * gCh);
            }
            outL[i] = o[0]; outR[i] = inR ? o[1] : o[0];
        }
    }
    float currentReduction() const { return grDb; }
private:
    void updateCoefs() {
        coefsFor = p.freq; widthFor = p.width;
        const auto c = odsp::design(odsp::Kind::BandPass, sr, p.freq, 0, qFromOctaves(p.width));
        for (int i = 0; i < 2; ++i) { det[i].c = c; band[i].c = c; }
        if (chFor < 0) {   // « CH » : 3,6 kHz sur 1,2 octave ; bas du spectre : 600 Hz sur 2 octaves
            const auto cc = odsp::design(odsp::Kind::BandPass, sr, std::min(3600.f, 0.45f * sr), 0, qFromOctaves(1.2));
            const auto lc = odsp::design(odsp::Kind::BandPass, sr, 600.f, 0, qFromOctaves(2.0));
            for (int i = 0; i < 2; ++i) { chDet[i].c = cc; chBand[i].c = cc; lowDet[i].c = lc; }
            chFor = 1;
        }
    }
    void autoSample(float db) {
        if (db > -80.f && autoN < 5000) autoBuf[(size_t)autoN++] = db; else if (autoN < 5000 && db <= -80.f) autoBuf[(size_t)autoN++] = -120.f;
        if (autoN >= 5000) {
            std::sort(autoBuf.begin(), autoBuf.end());
            const int top = std::max(1, (int)(0.15f * (float)autoBuf.size())), from = (int)autoBuf.size() - top;
            const float med = autoBuf[(size_t)(from + top / 2)];
            autoThr.store(std::min(0.f, std::max(-60.f, med + 4.f))); autoState.store(2);
        }
    }
    Params p; float sr = 48000.f, env = 0, grDb = 0, grMax = 0, coefsFor = -1, widthFor = -1, envCh = 0, envLow = 0, grCh = 0, chFor = -1; int look = 48;
    float envC[2] = {0, 0}, grC[2] = {0, 0};
    odsp::IntDelay dly[2]; odsp::Bq det[2], band[2], chDet[2], chBand[2], lowDet[2];
    std::vector<float> autoBuf; int autoN = 0, autoCnt = 0; std::atomic<int> autoState{0}; std::atomic<float> autoThr{-30.f};
};
}  // namespace des
