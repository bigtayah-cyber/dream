// Moteur de pads d'Orbe Pads : 16 pads sur des tranches d'un même échantillon, voix polyphoniques,
// hauteur par rééchantillonnage, filtre, bitcrush par quantification, panoramique, envois reverb et delay (synchro tempo).
#pragma once
#include "SamplerDsp.h"

namespace osamp {

// Tranches d'un échantillon (en trames), immuable une fois publiée.
struct Bank {
    std::shared_ptr<const Sample> pcm;
    int nsl = 0;
    int a[16] = {}, b[16] = {};
};

struct PadParams {
    float pitch = 0, volDb = 0, pan = 0, cut = 1, attMs = 0, relMs = 30, crush = 0, rv = 0, dl = 0;
    bool rev = false;
    int grp = 0;  // 0 = aucun, 1..3 = A..C
};

class PadEngine {
public:
    static constexpr int kVoices = 32;

    void prepare(double sampleRate) {
        sr = sampleRate;
        for (auto& v : vox) v.on = false;
        rv.init(sr); rv.setDecay(2.2f);
        pp.init(sr);
        bpm = 120.f;
    }
    void setTempo(double b) { if (b > 20.0) bpm = (float)b; }
    void setDelayBeats(float beats) { dlBeats = beats; }
    void allOff() { for (auto& v : vox) v.on = false; }

    // La banque est lue par les voix : elle doit rester vivante tant que des voix l'utilisent.
    void trigger(const Bank* bank, int pad, float vel, const PadParams& p, bool gate) {
        if (!bank || !bank->pcm || pad < 0 || pad >= bank->nsl) return;
        const int a = bank->a[pad], b = bank->b[pad], len = b - a;
        if (len < 4) return;
        for (auto& v : vox)  // le même pad se coupe lui-même, les pads du même groupe aussi
            if (v.on && v.fadeDec == 0.f && (v.pad == pad || (p.grp > 0 && v.grp == p.grp))) v.fadeDec = 1.f / (0.006f * (float)sr);
        Voice* t = nullptr;
        for (auto& v : vox) if (!v.on) { t = &v; break; }
        if (!t) { t = &vox[0]; for (auto& v : vox) if (v.age > t->age) t = &v; }
        Voice& v = *t;
        const double rate = std::pow(2.0, p.pitch / 12.0) * bank->pcm->sr / sr;
        v = Voice();
        v.on = true; v.bank = bank; v.pad = pad; v.grp = p.grp; v.len = len; v.a = a; v.rev = p.rev; v.rate = rate;
        v.dur = (double)len / rate;
        v.att = std::max(1.0, (p.attMs / 1000.0 + 0.002) * sr);
        v.rel = std::min(p.relMs / 1000.0 * sr, v.dur * 0.9);
        v.amp = std::pow(10.f, p.volDb / 20.f) * vel;
        const float pn = std::max(-1.f, std::min(1.f, p.pan / 100.f));
        v.gl = pn > 0 ? std::cos(pn * 1.5707963f) : 1.f;
        v.gr = pn < 0 ? std::cos(-pn * 1.5707963f) : 1.f;
        v.rv = p.rv; v.dl = p.dl * 0.8f;
        v.crush = p.crush > 0.001f ? std::max(3, (int)std::lround(13.0 - p.crush * 10.0)) : 0;
        v.lp = p.cut < 0.995f;
        if (v.lp) {
            const float f = std::min(0.45f * (float)sr, 200.f * std::pow(100.f, p.cut));
            v.fl.lowpass((float)sr, f, 0.5f); v.fr = v.fl;
        }
        v.gate = gate;
    }
    void release(int pad) { for (auto& v : vox) if (v.on && v.pad == pad && v.gate && v.fadeDec == 0.f) v.fadeDec = 1.f / std::max(1.f, (float)(std::max(v.rel, 0.004 * sr))); }

    // Position de lecture (0..1) d'un pad actif, ou -1.
    float position(int pad) const {
        for (auto& v : vox) if (v.on && v.pad == pad) return (float)std::min(1.0, v.age / std::max(1.0, v.dur));
        return -1.f;
    }

    void render(float* outL, float* outR, int n) {
        for (int i = 0; i < n; ++i) {
            float l = 0, r = 0, sendR = 0, sendD = 0;
            for (auto& v : vox) {
                if (!v.on) continue;
                if (v.age >= v.dur) { v.on = false; continue; }
                const double p = std::min((double)(v.len - 1), v.age * v.rate);
                float xl, xr;
                if (v.rev) {
                    const int base = v.a + v.len - 1;
                    xl = readRev(v.bank->pcm->ch[0], base, v.len, p); xr = readRev(v.bank->pcm->ch[1], base, v.len, p);
                } else {
                    xl = hermite(v.bank->pcm->ch[0].data() + v.a, v.len, p); xr = hermite(v.bank->pcm->ch[1].data() + v.a, v.len, p);
                }
                if (v.lp) { xl = v.fl.process(xl); xr = v.fr.process(xr); }
                if (v.crush) { xl = quantize(xl, v.crush); xr = quantize(xr, v.crush); }
                float g = std::min(1.f, (float)(v.age / v.att));
                const double left = v.dur - v.age;
                if (left < v.rel) g *= (float)(left / std::max(1.0, v.rel));
                if (v.fadeDec > 0.f) { v.fade -= v.fadeDec; if (v.fade <= 0.f) { v.on = false; continue; } g *= v.fade; }
                g *= v.amp;
                const float ol = xl * g * v.gl, orr = xr * g * v.gr;
                l += ol; r += orr;
                const float m = 0.5f * (ol + orr);
                sendR += m * v.rv; sendD += m * v.dl;
                v.age += 1.0;
            }
            float rl, rr, dl, dr;
            rv.process(sendR, 0.4f, rl, rr);
            pp.process(sendD, std::min((float)(sr * 2.0), 60.f / bpm * dlBeats * (float)sr), 0.38f, dl, dr);
            outL[i] += softClip(l + 0.5f * rl + 0.6f * dl);
            outR[i] += softClip(r + 0.5f * rr + 0.6f * dr);
        }
    }

    int activeVoices() const { int c = 0; for (auto& v : vox) c += v.on; return c; }

private:
    struct Voice {
        bool on = false, rev = false, lp = false, gate = false;
        const Bank* bank = nullptr;
        int pad = 0, grp = 0, len = 0, a = 0, crush = 0;
        double rate = 1, dur = 0, age = 0, att = 1, rel = 0;
        float amp = 1, gl = 1, gr = 1, rv = 0, dl = 0, fade = 1.f, fadeDec = 0.f;
        onek::Biquad fl, fr;
    };
    static float readRev(const std::vector<float>& x, int base, int len, double p) {
        const int i1 = (int)std::floor(p);
        const float t = (float)(p - i1);
        auto g = [&](int i) { return x[(size_t)(base - std::max(0, std::min(len - 1, i)))]; };
        const float x0 = g(i1 - 1), x1 = g(i1), x2 = g(i1 + 1), x3 = g(i1 + 2);
        const float c1 = 0.5f * (x2 - x0), c2 = x0 - 2.5f * x1 + 2 * x2 - 0.5f * x3, c3 = 0.5f * (x3 - x0) + 1.5f * (x1 - x2);
        return ((c3 * t + c2) * t + c1) * t + x1;
    }
    Voice vox[kVoices];
    voile::Reverb rv;
    voile::PingPong pp;
    double sr = 48000.0;
    float bpm = 120.f, dlBeats = 0.75f;
};

}  // namespace osamp
