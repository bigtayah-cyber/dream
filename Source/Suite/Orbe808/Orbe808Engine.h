// Moteur de l'Orbe 808 : basse monophonique à glissé legato. C++ pur (sans JUCE), sans allocation dans le thread audio.
// Transposition fidèle de la fonction hit() du prototype 808.html.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include "../../OneKnob/OneKnob.h"

namespace o808 {

constexpr float kPi = 3.14159265358979f;

struct Params {
    float len = 1.8f;     // durée de résonance (s)
    float punch = 14.f;   // demi-tons au départ de la note
    float pt = 0.045f;    // vitesse de retombée du punch (s)
    float drive = 0.55f;  // saturation 0..1
    int sat = 0;          // 0 douce, 1 écrêtée, 2 repliée
    float tone = 0.45f;   // harmoniques (triangle à l'octave)
    float cut = 0.85f;    // filtre : 120 Hz * 100^cut
    float click = 0.35f;  // attaque (clic)
    float glide = 0.12f;  // durée des glissés (s)
    float level = 0.8f;
    float bendRange = 2.f;  // demi-tons
};

struct Preset { const char* label; float len, punch, pt, drive; int sat; float tone, cut, click; };
constexpr int kNumPresets = 6;
inline const Preset* presets() {
    static const Preset p[kNumPresets] = {
        {"Classique", 1.6f, 12, .05f, .15f, 0, .25f, .8f, .25f},
        {"Trap satur\xC3\xA9" "e", 1.8f, 14, .045f, .55f, 0, .45f, .85f, .35f},
        {"Drill", 2.4f, 7, .06f, .4f, 1, .35f, .7f, .2f},
        {"Distordue", 1.4f, 16, .035f, .9f, 2, .6f, 1.f, .45f},
        {"Douce", 1.2f, 5, .08f, .05f, 0, .1f, .45f, .05f},
        {"Longue", 3.6f, 9, .06f, .3f, 0, .3f, .75f, .2f}};
    return p;
}

// Écrête en douceur : linéaire jusqu'à 0,9, jamais au-delà de 1.
inline float softClip(float x) {
    const float a = std::fabs(x);
    if (a <= 0.9f) return x;
    const float y = 0.9f + 0.1f * std::tanh((a - 0.9f) * 10.f);
    return x < 0 ? -y : y;
}

class Engine {
public:
    void prepare(double sampleRate) {
        sr = (float)sampleRate;
        rng = 0x1234567u;
        v = Voice{}; nHeld = 0; bendNorm = bendS = 0.f; lastCut = -1.f;
        lp.reset(); hpClick.reset();
        hpClick.highpass(sr, 2500.f, 1.f);
        // saturation suréchantillonnée ×8 (×4 à 88,2/96 kHz) ; l'enveloppe et le clic sont retardés d'autant (voix entière retardée)
        sat.init(voile::Oversampler::stagesFor(sampleRate, 3)); adx = adS = 0.0;
        clkDel.init(sat.latency() + 4); normDel.init(sat.latency() + 4);
        dfDrive = -1.f;
        relC = std::exp(-1.f / (0.012f * sr));
        stealC = std::exp(-1.f / (0.0007f * sr));
        bendCoef = 1.f - std::exp(-1.f / (0.004f * sr));
    }
    void setParams(const Params& p) {
        P = p;
        if (P.drive != dfDrive || P.sat != dfSat || P.tone != dfTone) buildDescribing();
        if (std::fabs(P.cut - lastCut) > 1e-5f) {
            lastCut = P.cut;
            lp.lowpass(sr, std::min(120.f * std::pow(100.f, P.cut), 0.45f * sr), 0.6f);
        }
    }
    void setBend(float norm) { bendNorm = std::max(-1.f, std::min(1.f, norm)); }

    void noteOn(int note, float vel) {
        vel = std::max(0.f, std::min(1.f, vel));
        const bool legato = v.active && !v.rel && nHeld > 0 && v.env > 0.02f * v.peak;
        push(note);
        if (legato) { retarget((float)note); return; }
        if (v.active && v.env > 0.003f) {  // note en cours d'extinction : fondu très court puis départ
            v.stealing = true; v.stealNote = note; v.stealVel = vel; v.rel = true; v.relC = stealC;
            return;
        }
        start(note, vel);
    }
    void noteOff(int note) {
        for (int i = 0; i < nHeld; ++i) if (held[i] == note) { for (int j = i; j + 1 < nHeld; ++j) held[j] = held[j + 1]; --nHeld; break; }
        if (nHeld == 0) {
            if (v.stealing) v.stealing = false;
            else { v.rel = true; v.relC = relC; }
        } else if (v.active && !v.stealing && (int)std::lround(v.target) == note) retarget((float)held[nHeld - 1]);
    }
    void allOff() { nHeld = 0; if (v.active) { v.stealing = false; v.rel = true; v.relC = relC; } }

    int latency() const { return sat.latency(); }   // échantillons : à déclarer à l'hôte
    bool active() const { return v.active; }
    int currentNote() const { return v.active ? (int)std::lround(v.target) : -1; }

    // Écrit n échantillons mono (remplace le contenu).
    void render(float* out, int n) {
        for (int i = 0; i < n; ++i) {
            bendS += (bendNorm - bendS) * bendCoef;
            out[i] = v.active ? softClip(voiceSample() * P.level) : 0.f;
        }
    }

private:
    struct Voice {
        bool active = false, rel = false, stealing = false;
        float pitch = 33.f, target = 33.f, glideStep = 0.f;
        int glideLeft = 0, stealNote = 0;
        uint32_t age = 0, attackN = 1, holdN = 1, ptN = 1;
        float env = 0.f, peak = 0.f, dc = 0.99f, relC = 0.99f, punch = 0.f, vel = 1.f, stealVel = 1.f;
        float p1 = 0.f, p2 = 0.f;
        float ng = 0.f, ngm = 1.f, kg = 0.f, kgm = 1.f, cf = 1800.f, cfm = 1.f, cph = 0.f;
        uint32_t clkN = 0;
    };
    Params P;
    Voice v;
    float sr = 48000.f, relC = 0.99f, stealC = 0.9f, bendCoef = 0.01f, bendNorm = 0.f, bendS = 0.f, lastCut = -1.f;
    int held[16];
    int nHeld = 0;
    uint32_t rng = 1;
    onek::Biquad lp, hpClick;
    voile::Oversampler sat;
    voile::Delay clkDel, normDel;
    double adx = 0.0, adS = 0.0;
    // Gain RMS de la saturation selon le niveau relatif e = env/crête (table sur e, 0..1) : h(e) = RMS(shape(m·K·e²)) / e².
    static constexpr int kDf = 96;
    float df[kDf + 1] = {};
    float dfDrive = -1.f, dfTone = -1.f;
    int dfSat = -1;

    // Recalculée seulement quand drive, sat ou tone changent (97 x 32 évaluations, sans allocation). m = l'onde réelle
    // (sinus + triangle d'octave). Le gain RMS (et non celui du fondamental) reste défini pour la saturation repliée.
    void buildDescribing() {
        dfDrive = P.drive; dfSat = P.sat; dfTone = P.tone;
        const float K = (1.f + P.drive * 7.f) * 0.8f;
        constexpr int N = 32;
        for (int j = 0; j <= kDf; ++j) {
            const float e = std::max(1e-3f, (float)j / (float)kDf), u = e * e;
            double acc = 0.0;
            for (int i = 0; i < N; ++i) {
                const float th = ((float)i + 0.5f) / (float)N, ph2 = 2.f * th - std::floor(2.f * th);
                const float m = std::sin(2.f * kPi * th) + P.tone * 0.55f * tri(ph2);
                const double v = shape(m * K * u);
                acc += v * v;
            }
            df[j] = std::max(1e-6f, (float)std::sqrt(acc / N) / u);
        }
    }
    float rmsGain(float e) const {
        const float p = std::max(0.f, std::min(1.f, e)) * (float)kDf;
        const int i = std::min(kDf - 1, (int)p);
        return df[i] + (df[i + 1] - df[i]) * (p - (float)i);
    }

    float noise() { rng = rng * 1664525u + 1013904223u; return (float)(int32_t)rng / 2147483648.f; }
    void push(int note) {
        for (int i = 0; i < nHeld; ++i) if (held[i] == note) { for (int j = i; j + 1 < nHeld; ++j) held[j] = held[j + 1]; --nHeld; break; }
        if (nHeld == 16) { for (int j = 0; j + 1 < 16; ++j) held[j] = held[j + 1]; --nHeld; }
        held[nHeld++] = note;
    }
    void retarget(float t) {
        v.target = t;
        v.glideLeft = std::max(1, (int)(P.glide * sr));
        v.glideStep = (t - v.pitch) / (float)v.glideLeft;
    }
    void start(int note, float vel) {
        Voice n;
        n.active = true; n.pitch = n.target = (float)note; n.vel = vel; n.peak = 0.78f * vel;
        n.attackN = std::max(1u, (uint32_t)(0.004f * sr)); n.holdN = (uint32_t)(0.02f * sr);
        n.ptN = std::max(1u, (uint32_t)(P.pt * sr)); n.punch = P.punch;
        n.dc = std::exp(-1.f / (std::max(0.05f, P.len / 3.2f) * sr));
        const float c0 = P.click * 0.5f * vel, k0 = P.click * 0.3f * vel;
        if (c0 > 0.001f) { n.ng = c0; n.ngm = std::pow(0.001f / c0, 1.f / (0.025f * sr)); n.kg = k0; n.kgm = std::pow(0.001f / k0, 1.f / (0.03f * sr)); n.cfm = std::pow(200.f / 1800.f, 1.f / (0.02f * sr)); n.clkN = (uint32_t)(0.04f * sr); }
        hpClick.reset();
        v = n;
    }
    static float tri(float p) { float q = p + 0.25f; q -= std::floor(q); return 1.f - 4.f * std::fabs(q - 0.5f); }
    float shape(float x) const {
        x = std::max(-1.f, std::min(1.f, x));
        const float k = 1.f + P.drive * 7.f;
        if (P.sat == 1) return std::max(-1.f, std::min(1.f, x * k)) * 0.92f;
        if (P.sat == 2) return std::sin(x * k * kPi * 0.5f);
        return std::tanh(x * k) / std::tanh(k);
    }
    // Même fonction, antidérivée d'ordre 1 (ADAA) : l'écrêtage dur et les cassures de la courbe ne se replient presque plus (-13 dB de plus
    // que le suréchantillonnage seul). Appelée dans l'ordre des échantillons suréchantillonnés ; calcul en double (différences voisines).
    float shapeAdaa(float xf) {
        const double k = 1.0 + (double)P.drive * 7.0, x = std::max(-1.0, std::min(1.0, (double)xf));
        double S;
        if (P.sat == 1) {
            const double ax = std::fabs(x);
            S = ax * k <= 1.0 ? 0.46 * k * ax * ax : 0.46 / k + 0.92 * (ax - 1.0 / k);
        } else if (P.sat == 2) {
            const double a = k * 3.14159265358979323846 * 0.5, ax = std::fabs(x);
            S = (1.0 - std::cos(a * ax)) / a;   // |x| <= 1 : l'entrée est déjà bornée
        } else {
            const double ax = std::fabs(x), t = std::tanh(k), kx = k * ax;
            S = (kx + std::log1p(std::exp(-2.0 * kx)) - 0.6931471805599453) / (k * t);
        }
        const double dx = x - adx;
        double y;
        if (std::fabs(dx) > 1e-7) y = (S - adS) / dx;
        else { const double m = 0.5 * (x + adx); y = P.sat == 1 ? std::max(-1.0, std::min(1.0, m * k)) * 0.92 : P.sat == 2 ? std::sin(m * k * 3.14159265358979323846 * 0.5) : std::tanh(m * k) / std::tanh(k); }
        adx = x; adS = S;
        return (float)y;
    }

    float voiceSample() {
        Voice& V = v;
        if (V.glideLeft > 0) { V.pitch += V.glideStep; if (--V.glideLeft == 0) V.pitch = V.target; }
        float semi = V.pitch + bendS * P.bendRange;
        if (V.age < V.ptN) semi += V.punch * (1.f - (float)V.age / (float)V.ptN);
        const float f = std::min(440.f * std::exp2((semi - 69.f) / 12.f), 0.45f * sr);
        V.p1 += f / sr; V.p1 -= std::floor(V.p1);
        V.p2 += 2.f * f / sr; V.p2 -= std::floor(V.p2);
        const float mix = std::sin(2.f * kPi * V.p1) + P.tone * 0.55f * tri(V.p2);
        // enveloppe
        const float tgt = V.peak * 1e-4f;
        if (V.rel) V.env *= V.relC;
        else if (V.age < V.attackN) V.env = V.peak * (float)(V.age + 1) / (float)V.attackN;
        else if (V.age < V.holdN) V.env = V.peak;
        else V.env = tgt + (V.env - tgt) * V.dc;
        // Saturation APRÈS l'enveloppe (audit 2, point 7) : l'entrée du saturateur suit u = (env/crête)^2, donc le taux d'harmoniques
        // suit le niveau (« bloom » : attaque grasse, queue qui redevient sinus ; le carré compense le drive interne déjà très fort).
        // Le niveau est ramené par le gain RMS h(e) de la courbe : sortie = shape(m·K·u)·crête·h(1)/(e·h(e)), e = env/crête. À e = 1, son
        // identique à l'ancien moteur ; ensuite, même contour de niveau RMS qu'avant (la longueur perçue ne change pas).
        const float K = (1.f + P.drive * 7.f) * 0.8f, e = V.peak > 0.f ? std::min(1.f, V.env / V.peak) : 0.f, u = e * e;
        const float y = sat.process(mix * K * u, [this](float x) { return shapeAdaa(x); });
        normDel.push(V.peak * rmsGain(1.f) / (std::max(e, 1e-3f) * rmsGain(e)));
        float o = lp.process(y) * normDel.read((float)sat.latency());
        float clk = 0.f;
        if (V.age < V.clkN) {  // clic : bruit filtré + balayage grave, hors enveloppe et hors filtre
            if (V.age < (uint32_t)(0.025f * sr)) { clk += V.ng * hpClick.process(noise()); V.ng *= V.ngm; }
            clk += V.kg * std::sin(2.f * kPi * V.cph);
            V.cph += V.cf / sr; V.cph -= std::floor(V.cph);
            if (V.age < (uint32_t)(0.02f * sr)) V.cf *= V.cfm;
            V.kg *= V.kgm;
        }
        clkDel.push(clk);
        o += clkDel.read((float)sat.latency());
        ++V.age;
        if ((V.rel || V.age > V.holdN) && V.env < (V.stealing ? 0.003f : 2e-4f)) {
            if (V.stealing) { const int nn = V.stealNote; const float nv = V.stealVel; start(nn, nv); }
            else V.active = false;
        }
        return o;
    }
};

}  // namespace o808
