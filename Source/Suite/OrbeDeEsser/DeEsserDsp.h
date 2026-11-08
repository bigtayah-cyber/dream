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
};
inline double qFromOctaves(double oct) { return 1.0 / (2.0 * std::sinh(std::log(2.0) / 2.0 * oct)); }

class Engine {
public:
    void prepare(double sampleRate) {
        sr = (float)sampleRate; look = std::max(1, (int)std::lround(0.001 * sr));
        for (int c = 0; c < 2; ++c) { dly[c].init(look); det[c].reset(); band[c].reset(); }
        env = 0; grDb = 0; coefsFor = -1;
    }
    void set(const Params& np) { p = np; if (np.freq != coefsFor || np.width != widthFor) updateCoefs(); }
    int latency() const { return look; }
    float reduction() const { return grMax; }   // max de réduction sur le dernier bloc (dB, positif)
    void resetMeter() { grMax = 0.f; }

    void process(const float* inL, const float* inR, float* outL, float* outR, int n) {
        const float aEnv = std::exp(-1.f / (0.0002f * sr)), rEnv = std::exp(-1.f / (0.004f * sr));
        const float aG = std::exp(-1.f / (std::max(0.05f, p.atk) * 1e-3f * sr)), rG = std::exp(-1.f / (std::max(1.f, p.rel) * 1e-3f * sr));
        const float knee = p.sharp ? 0.5f : 8.f, ratio = p.sharp ? 20.f : 4.f;
        for (int i = 0; i < n; ++i) {
            const float x0 = inL[i], x1 = inR ? inR[i] : x0;
            const float b0 = det[0].process(x0), b1 = det[1].process(x1);
            const float a = std::max(std::fabs(b0), std::fabs(b1));
            env = a > env ? aEnv * env + (1 - aEnv) * a : rEnv * env + (1 - rEnv) * a;
            const float over = odsp::lin2db(env) - p.thr;
            float red = 0.f;
            if (over > knee / 2) red = over * (1.f - 1.f / ratio);
            else if (over > -knee / 2) { const float t = over + knee / 2; red = (1.f - 1.f / ratio) * t * t / (2.f * knee); }
            red = std::min(red, p.range);
            grDb = red > grDb ? aG * grDb + (1 - aG) * red : rG * grDb + (1 - rG) * red;   // lissage en dB
            grMax = std::max(grMax, grDb);
            const float g = odsp::db2lin(-grDb);
            float o[2];
            for (int c = 0; c < 2; ++c) {
                const float x = c == 0 ? x0 : x1;
                const float d = dly[c].tick(x);                        // audio retardé du lookahead
                const float bd = band[c].process(d);                    // sa bande de sibilance
                o[c] = p.listen ? bd : (p.split ? d + (g - 1.f) * bd : d * g);
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
    }
    Params p; float sr = 48000.f, env = 0, grDb = 0, grMax = 0, coefsFor = -1, widthFor = -1; int look = 48;
    odsp::IntDelay dly[2]; odsp::Bq det[2], band[2];
};
}  // namespace des
