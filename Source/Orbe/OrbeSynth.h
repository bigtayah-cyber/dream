// Synthé d'aperçu d'Orbe : un timbre par piste (accords, lead, basse) et une batterie synthétisée.
// C++ pur, sans allocation dans le thread audio.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace orbe {

class Synth {
public:
    static constexpr int kVoices = 48;

    void prepare(double sampleRate) { sr = (float)sampleRate; for (auto& v : voices) v.on = false; rng = 12345u; }
    void reset() { for (auto& v : voices) v.on = false; }

    // track : 0 accords, 1 lead, 2 basse, 3 batterie
    // glide : sur la basse, glisse depuis la voix encore active (808 qui slide) au lieu de redéclencher
    void trigger(int track, int note, float vel, int gateSamples, bool glide = false) {
        const float fn = 440.f * std::pow(2.f, ((float)note - 69.f) / 12.f);
        if (glide && track == 2)
            for (auto& x : voices)
                if (x.on && x.track == 2 && x.age < x.gate + (int)(0.05f * sr)) { x.fT = fn; x.gate = x.age + gateSamples; x.vel = vel; x.note = note; return; }
        Voice* v = nullptr;
        for (auto& x : voices) if (!x.on) { v = &x; break; }
        if (!v) { v = &voices[0]; for (auto& x : voices) if (x.age > v->age) v = &x; }
        *v = Voice{};
        v->on = true; v->track = track; v->note = note; v->vel = vel; v->gate = gateSamples;
        v->f = 440.f * std::pow(2.f, ((float)note - 69.f) / 12.f);
        v->ph = (track == 0) ? 0.f : 0.f;
        v->ph2 = 0.37f;
    }

    // Ajoute (+=) l'aperçu au tampon, multiplié par `gain` (potard MASTER) : l'audio déjà présent (Orbe FX) n'est pas touché.
    void render(float* L, float* R, int n, float gain = 1.f) {
        for (int i = 0; i < n; ++i) {
            float l = 0, r = 0;
            for (auto& v : voices) if (v.on) voice(v, l, r);
            // saturation douce de sortie
            L[i] += gain * std::tanh(l * 0.9f);
            R[i] += gain * std::tanh(r * 0.9f);
        }
    }

    bool active() const { for (auto& v : voices) if (v.on) return true; return false; }

private:
    struct Voice {
        bool on = false;
        int track = 0, note = 60, age = 0, gate = 0;
        float f = 440.f, fT = 0.f, ph = 0.f, ph2 = 0.f, vel = 0.8f, lp = 0.f, hp1 = 0.f, env = 0.f;
    };
    Voice voices[kVoices];
    float sr = 48000.f;
    uint32_t rng = 1;

    float noise() { rng = rng * 1664525u + 1013904223u; return (float)(int32_t)rng / 2147483648.f; }
    static float saw(float p) { return 2.f * p - 1.f; }
    static float tri(float p) { return 4.f * std::fabs(p - 0.5f) - 1.f; }
    static float wrap(float p) { return p - std::floor(p); }

    void voice(Voice& v, float& oL, float& oR) {
        const float t = (float)v.age / sr;            // secondes écoulées
        const bool held = v.age < v.gate;
        float s = 0, pl = 1.f, pr = 1.f;  // pl/pr : balance de la piste
        switch (v.track) {
            case 0: {  // nappe : deux scies désaccordées, filtre qui s'ouvre
                const float a = std::min(1.f, t / 0.012f);
                if (!held) v.env = std::max(0.f, v.env - 1.f / (0.22f * sr)); else v.env = a;
                const float dt = v.f / sr;
                v.ph = wrap(v.ph + dt); v.ph2 = wrap(v.ph2 + dt * 1.0065f);
                const float cut = 0.04f + 0.16f * std::min(1.f, t / 0.25f);
                v.lp += cut * (saw(v.ph) + saw(v.ph2) - v.lp);
                const float body = v.lp * 0.28f * v.env * v.vel;
                const float side = (saw(v.ph) - saw(v.ph2)) * 0.05f * v.env * v.vel;  // largeur stéréo
                oL += body + side; oR += body - side;
                v.age++;
                if (!held && v.env <= 0.0005f) v.on = false;
                return;

            }
            case 1: {  // lead : triangle + octave, attaque pincée
                if (!held) v.env = std::max(0.f, v.env - 1.f / (0.09f * sr)); else v.env = std::exp(-t / 0.45f);
                v.ph = wrap(v.ph + v.f / sr);
                s = (tri(v.ph) * 0.8f + std::sin(juce_2pi() * v.ph * 2.f) * 0.22f) * v.env * v.vel * 0.34f;
                pl = 0.8f; pr = 1.1f;
                break;
            }
            case 2: {  // basse : sinus + 2e harmonique saturé
                const float a = std::min(1.f, t / 0.004f);
                if (!held) v.env = std::max(0.f, v.env - 1.f / (0.07f * sr)); else v.env = a;
                if (v.fT > 0.f) v.f += (v.fT - v.f) * (1.f / (0.045f * sr));  // glissade 808
                v.ph = wrap(v.ph + v.f / sr);
                const float x = std::sin(juce_2pi() * v.ph) + 0.25f * std::sin(juce_2pi() * v.ph * 2.f);
                s = std::tanh(x * 1.5f) * 0.55f * v.env * v.vel;
                break;
            }
            default: s = drum(v, t); break;
        }
        ++v.age;
        oL += s * pl; oR += s * pr;
        const bool done = (v.track == 3) ? (v.age > (int)(0.6f * sr)) : (!held && v.env <= 0.0005f);
        if (done) v.on = false;
    }

    static float juce_2pi() { return 6.28318530718f; }

    float drum(Voice& v, float t) {
        const float vel = v.vel;
        switch (v.note) {
            case 35: case 36: {  // grosse caisse
                const float fr = 46.f + 130.f * std::exp(-t / 0.028f);
                v.ph = wrap(v.ph + fr / sr);
                float x = std::sin(juce_2pi() * v.ph) * std::exp(-t / (v.note == 35 ? 0.22f : 0.16f));
                x += noise() * 0.25f * std::exp(-t / 0.004f);
                return std::tanh(x * 1.4f) * 0.9f * vel;
            }
            case 38: {  // caisse claire
                v.ph = wrap(v.ph + 188.f / sr);
                return (noise() * std::exp(-t / 0.085f) * 0.7f + std::sin(juce_2pi() * v.ph) * std::exp(-t / 0.045f) * 0.5f) * 0.8f * vel;
            }
            case 39: {  // clap : trois rafales puis queue
                float e = std::exp(-t / 0.11f) * 0.6f;
                for (float o : {0.f, 0.011f, 0.022f}) if (t >= o) e += std::exp(-(t - o) / 0.006f) * 0.9f;
                const float n = noise(); v.hp1 += 0.45f * (n - v.hp1);
                return (n - v.hp1) * e * 0.55f * vel;
            }
            case 37: {  // rimshot
                v.ph = wrap(v.ph + 1650.f / sr);
                return (std::sin(juce_2pi() * v.ph) * std::exp(-t / 0.005f) + noise() * 0.3f * std::exp(-t / 0.006f)) * 0.6f * vel;
            }
            case 42: case 46: {  // charleston : bruit filtré passe-haut
                const float n = noise(); v.hp1 += 0.7f * (n - v.hp1);
                return (n - v.hp1) * std::exp(-t / (v.note == 42 ? 0.028f : 0.17f)) * 0.5f * vel;
            }
            case 49: {  // crash
                const float n = noise(); v.hp1 += 0.5f * (n - v.hp1);
                return (n - v.hp1) * std::exp(-t / 0.3f) * 0.4f * vel;
            }
            default: return 0.f;
        }
    }
};

}  // namespace orbe
