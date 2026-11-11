// Orbe Synth : moteur audio. C++17 pur, sans JUCE (testable hors-ligne).
// Porte fidèlement le prototype synth.html (Web Audio) : 2 oscillateurs PolyBLEP, unisson 1–7, FM, sub (hors filtre),
// passe-bas résonant (Q en dB comme Web Audio : Q = 0,5 + res·14 dB), ADSR exponentiels, puis bus d'effets
// saturation -> chorus -> (sec | reverb | delay synchronisé) -> limiteur. Gain de voix : le 0,3·(1-mix·0,4) seul compte.
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>
#include "../../OneKnob/OneKnob.h"

namespace osyn {

constexpr int kMaxVoices = 16;  // plafond de polyphonie (voix tenues ou en relâche)
constexpr int kSlots = 24;      // + voix volées en fondu de 5 ms
constexpr int kMaxUni = 7;
constexpr int kCtrl = 16;       // pas de contrôle (lissage des réglages, coefficients du filtre)
constexpr float kTwoPi = 6.28318530717958647692f;

enum Wave { Sine, Tri, Saw, Square };

struct Params {
    int w1 = Saw, w2 = Square, uni = 3, dlyDiv = 2;
    float semi = 0, mix = .3f, fm = 0, det = 12, sub = 0, cut = .55f, res = .2f, famt = .4f, fdec = .4f,
          a = .005f, d = .4f, s = .4f, r = .3f, drive = 0, cho = 0, dly = .15f, rev = .25f, vol = .7f;
    // Lot E (valeurs par défaut = ancien son)
    int aa = 0;             // 0 : PolyBLEP 2 points (d'origine) ; 1 : 4 points, triangle en BLAMP (repliement -47..-38 dB -> sous -60 dB)
    int uniRand = 0;        // 1 : phases d'unisson aléatoires (graine par note), la voix 0 garde la phase 0
    float detx = 0;         // centièmes de demi-tone ajoutés au désaccord (0..50) : jusqu'à 100 ct
    int mode = 0;           // 0 polyphonique, 1 mono (relance l'enveloppe), 2 legato (ne relance pas)
    float glide = 0;        // s : portamento des modes mono et legato
    float bendSemi = 2;     // étendue de la molette de pitch
    int lfoShape[2] = {0, 0}, lfoDest[2] = {0, 0};   // forme (sinus, triangle, scie, carré, aléatoire) ; cible (0 aucune, 1 coupure, 2 hauteur, 3 volume, 4 FM, 5 coupure du filtre 2)
    float lfoRate[2] = {2.f, 2.f}, lfoDepth[2] = {0, 0};   // Hz ; 0..1
    int f2mode = 0;         // second filtre en série : 0 coupé, 1 passe-bas, 2 passe-haut, 3 passe-bande (SVF à zéro délai, 12 dB/oct)
    float f2cut = .5f, f2res = .2f;
};

// Valeurs de délai en temps (croches pointées par défaut : 0,375 s à 120 bpm, comme le prototype).
inline const float* divBeats() { static const float b[6] = {.25f, .5f, .75f, 1.f, 1.5f, 2.f}; return b; }

struct Preset { const char* name; Params p; };
inline const std::array<Preset, 10>& presets() {
    auto mk = [](auto fn) { Params x; fn(x); return x; };
    static const std::array<Preset, 10> a = {{
        {"Pluck", mk([](Params& x) { x.semi = 12; x.mix = .25f; x.uni = 2; x.det = 8; x.cut = .25f; x.res = .35f; x.famt = .7f; x.fdec = .18f; x.a = .002f; x.d = .35f; x.s = 0; x.r = .25f; x.dly = .28f; x.rev = .2f; })},
        {"Pad", mk([](Params& x) { x.w2 = Saw; x.semi = 7; x.mix = .35f; x.uni = 5; x.det = 22; x.cut = .35f; x.res = .1f; x.famt = .35f; x.fdec = 1.6f; x.a = .9f; x.d = 1.2f; x.s = .8f; x.r = 1.8f; x.cho = .5f; x.rev = .55f; x.dly = .1f; })},
        {"Lead", mk([](Params& x) { x.semi = -12; x.mix = .3f; x.uni = 3; x.det = 14; x.cut = .6f; x.res = .3f; x.famt = .3f; x.fdec = .3f; x.a = .01f; x.d = .3f; x.s = .7f; x.r = .2f; x.drive = .3f; x.dly = .25f; x.rev = .2f; })},
        {"Cloche", mk([](Params& x) { x.w1 = Sine; x.w2 = Sine; x.semi = 19; x.mix = .1f; x.fm = .55f; x.uni = 1; x.det = 0; x.cut = .9f; x.res = 0; x.famt = 0; x.fdec = .2f; x.a = .002f; x.d = 1.4f; x.s = 0; x.r = 1.2f; x.rev = .35f; x.dly = .3f; })},
        {"Stab", mk([](Params& x) { x.semi = 0; x.mix = .5f; x.uni = 3; x.det = 10; x.cut = .3f; x.res = .45f; x.famt = .65f; x.fdec = .12f; x.a = .002f; x.d = .18f; x.s = 0; x.r = .12f; x.drive = .2f; x.rev = .25f; x.dly = .2f; })},
        {"Supersaw", mk([](Params& x) { x.w2 = Saw; x.semi = 12; x.mix = .2f; x.uni = 7; x.det = 30; x.cut = .7f; x.res = .1f; x.famt = .15f; x.fdec = .5f; x.a = .01f; x.d = .5f; x.s = .8f; x.r = .4f; x.cho = .3f; x.rev = .35f; x.dly = .15f; })},
        {"Keys", mk([](Params& x) { x.w1 = Sine; x.w2 = Sine; x.semi = 12; x.mix = .25f; x.fm = .28f; x.uni = 1; x.det = 0; x.cut = .7f; x.res = 0; x.famt = .1f; x.fdec = .5f; x.a = .003f; x.d = 1.1f; x.s = .25f; x.r = .5f; x.cho = .35f; x.rev = .3f; x.dly = 0; })},
        {"Basse synth", mk([](Params& x) { x.semi = -12; x.mix = .5f; x.uni = 1; x.det = 0; x.sub = .6f; x.cut = .3f; x.res = .4f; x.famt = .45f; x.fdec = .25f; x.a = .003f; x.d = .3f; x.s = .6f; x.r = .12f; x.drive = .35f; x.rev = 0; x.dly = 0; })},
        {"Cuivres", mk([](Params& x) { x.w2 = Saw; x.semi = 0; x.mix = .5f; x.uni = 3; x.det = 9; x.cut = .3f; x.res = .15f; x.famt = .55f; x.fdec = .35f; x.a = .06f; x.d = .4f; x.s = .7f; x.r = .25f; x.drive = .15f; x.rev = .3f; })},
        {"Verre", mk([](Params& x) { x.w1 = Tri; x.w2 = Sine; x.semi = 24; x.mix = .4f; x.fm = .2f; x.uni = 2; x.det = 6; x.cut = .8f; x.res = .5f; x.famt = .2f; x.fdec = .6f; x.a = .004f; x.d = .9f; x.s = .15f; x.r = .9f; x.cho = .4f; x.rev = .5f; x.dly = .35f; })},
    }};
    return a;
}

// ---------- oscillateurs (convention Web Audio : phase 0 au départ, scie 0→1→-1→0, carré +1 puis -1) ----------
inline float blep(float t, float dt) {
    if (t < dt) { t /= dt; return t + t - t * t - 1.f; }
    if (t > 1.f - dt) { t = (t - 1.f) / dt; return t * t + t + t + 1.f; }
    return 0.f;
}
// BLEP / BLAMP à table (marche et rampe bande-limitées par un sinc fenêtré de Kaiser, 16 échantillons de large, coupure 0,40·fs) :
// résidu de marche res(x) = Φ(x) - H(x) et résidu de rampe ramp(x) = ∫res, x = distance à la discontinuité en échantillons (-8..8).
// Interpolation linéaire, 256 points par échantillon, table calculée une fois (appelée par Engine::prepare, jamais dans le thread audio).
struct BlepTab {
    static constexpr int W = 8, R = 256, N = 2 * W * R + 1;
    float phi[N], rho[N];
    BlepTab() {
        auto i0 = [](double x) { double s = 1, t = 1; for (int k = 1; k < 30; ++k) { t *= (x / (2.0 * k)) * (x / (2.0 * k)); s += t; } return s; };
        const double beta = 9.0, fc = .40, pi = 3.14159265358979323846;
        std::vector<double> h((size_t)N), c((size_t)N);
        double tot = 0;
        for (int i = 0; i < N; ++i) {
            const double u = (double)(i - W * R) / (double)R, w = i0(beta * std::sqrt(std::max(0.0, 1.0 - (u / W) * (u / W)))) / i0(beta);
            h[(size_t)i] = (u == 0.0 ? 2 * fc : std::sin(2 * pi * fc * u) / (pi * u)) * w;
            tot += h[(size_t)i];
        }
        double acc = 0;
        for (int i = 0; i < N; ++i) { acc += (h[(size_t)i] - (i == 0 ? 0.0 : 0.0)) / tot; c[(size_t)i] = acc - .5 * h[(size_t)i] / tot; phi[i] = (float)c[(size_t)i]; }
        double r = 0;
        for (int i = 0; i < N; ++i) {
            const double x = (double)(i - W * R) / (double)R, res = c[(size_t)i] - (x >= 0 ? 1.0 : 0.0);
            r += res / (double)R; rho[i] = (float)r;
        }
        const double off = rho[N - 1];   // la fin doit valoir 0 : on retire la dérive résiduelle (arrondis) linéairement
        for (int i = 0; i < N; ++i) rho[i] = (float)(rho[i] - off * (double)i / (double)(N - 1));
    }
    static const BlepTab& get() { static const BlepTab t; return t; }
    float step(float x) const {   // Φ(x), 0 avant, 1 après
        if (x <= -W) return 0.f;
        if (x >= W) return 1.f;
        const float p = (x + W) * R; const int i = (int)p; const float f = p - (float)i;
        return phi[i] + (phi[i + 1] - phi[i]) * f;
    }
    float res(float x) const { return step(x) - (x >= 0.f ? 1.f : 0.f); }
    float ramp(float x) const {
        if (x <= -W || x >= W) return 0.f;
        const float p = (x + W) * R; const int i = (int)p; const float f = p - (float)i;
        return rho[i] + (rho[i + 1] - rho[i]) * f;
    }
};
inline float res4(float x) { return BlepTab::get().res(x); }
inline float ramp4(float x) { return BlepTab::get().ramp(x); }
inline float wrapHalf(float d) { return d > .5f ? d - 1.f : d < -.5f ? d + 1.f : d; }
struct Osc { float ph = 0.f; };
inline float oscTick(Osc& o, int w, float inc, int aa = 0) {
    const float p = o.ph, dt = std::min(std::fabs(inc), .49f);
    float y;
    if (aa && dt <= .0624f) {   // au-delà de ~3 kHz les fenêtres de deux discontinuités se recouvrent : retour au PolyBLEP à 2 points
        const float idt = 1.f / std::max(dt, 1e-6f);
        switch (w) {
            case Sine: y = std::sin(kTwoPi * p); break;
            case Tri: y = p < .25f ? 4.f * p : p < .75f ? 2.f - 4.f * p : 4.f * p - 4.f;
                y += 8.f * dt * (ramp4(wrapHalf(p - .75f) * idt) - ramp4(wrapHalf(p - .25f) * idt)); break;
            case Saw: { float q = p + .5f; if (q >= 1.f) q -= 1.f; y = 2.f * q - 1.f - 2.f * res4(wrapHalf(q) * idt); break; }
            default: { float q = p + .5f; if (q >= 1.f) q -= 1.f; y = (p < .5f ? 1.f : -1.f) + 2.f * res4(wrapHalf(p) * idt) - 2.f * res4(wrapHalf(q) * idt); break; }
        }
    } else switch (w) {
        case Sine: y = std::sin(kTwoPi * p); break;
        case Tri: y = p < .25f ? 4.f * p : p < .75f ? 2.f - 4.f * p : 4.f * p - 4.f; break;
        case Saw: { float q = p + .5f; if (q >= 1.f) q -= 1.f; y = 2.f * q - 1.f - blep(q, dt); break; }
        default: { float q = p + .5f; if (q >= 1.f) q -= 1.f; y = (p < .5f ? 1.f : -1.f) + blep(p, dt) - blep(q, dt); break; }
    }
    float n = p + inc;
    n -= std::floor(n);
    o.ph = n >= 1.f ? 0.f : n;
    return y;
}

struct Voice {
    enum St { Dead, Att, Dec, Rel, Fast } st = Dead;
    int note = 0, N = 1, attLeft = 0, relN = 0, relMax = 0, age = 0;
    uint32_t serial = 0;
    bool gate = false, sustained = false;  // sustained : touche relâchée mais pédale enfoncée
    float f0 = 440.f, pk = 0.f, lvl = 0.f, attInc = 0.f, ra = .004f, modLvl = 6.f, vg = 0.f, fb = 440.f, b2 = 440.f, incMod = 0.f, incSub = 0.f;
    float gl[kMaxUni] = {}, gr[kMaxUni] = {}, r1[kMaxUni] = {}, r2[kMaxUni] = {};
    Osc o1[kMaxUni], o2[kMaxUni], mod, sub;
    onek::Biquad fl, fr;
    float semi = 69.f, semiT = 69.f, glStep = 0.f;   // hauteur courante / visée (demi-tons MIDI) et pas de glissé par échantillon
    int glLeft = 0;
    float s2l[2] = {0, 0}, s2r[2] = {0, 0};          // état du second filtre (SVF à zéro délai) : gauche, droite
};

class Engine {
public:
    void prepare(double sampleRate) {
        sr = (float)sampleRate;
        BlepTab::get();   // table de BLEP construite ici (hors thread audio)
        rev.init(sampleRate);
        rev.setDecay(2.0f);
        for (auto& c : cho) c.init((int)(.06f * sr) + 4);
        for (auto& d : dl) d.init((int)(4.1f * sr) + 4);
        reset();
    }
    void reset() {
        for (auto& v : vc) v = Voice();
        serial = 0; fresh = true; ph1 = ph2 = 0.f; dSm.v = 0.f; dSm.c = 1.f - std::exp(-1.f / (.08f * sr));
        for (auto& b : os) { b.init(voile::Oversampler::stagesFor(sr, 3)); }   // saturation suréchantillonnée ×8 (×4 à 88,2/96 kHz)
        adaa[0] = adaa[1] = limS[0] = limS[1] = AdaaState();
        for (auto& b : dlp) { b.lowpass(sr, 3500.f, 1.122f); b.reset(); }
        bendR = 1.f; modMul = 1.f; pedal = false; nHeld = 0; monoSlot = -1; monoSerial = 0; lastSemi = -1.f;
        lfoPh[0] = lfoPh[1] = 0.f; lfoSm[0] = lfoSm[1] = 0.f; lfoSh[0] = lfoSh[1] = 0.f; lfoRng = 0x9E3779B9u; mCut = mPitch = mFm = mCut2 = 0.f; mVol = mVolPrev = 1.f;
        rev.init(sr);
        for (auto& c : cho) c.init((int)c.b.size() - 2);
        for (auto& d : dl) d.init((int)d.b.size() - 2);
    }
    void setParams(const Params& p) { tgt = p; }
    void setTempo(double bpm) { tempo = (float)std::clamp(bpm, 30., 300.); }
    void setBend(float v) { bendR = std::exp2(std::clamp(v, -1.f, 1.f) * std::clamp(tgt.bendSemi, 1.f, 12.f) / 12.f); }  // ±2 demi-tons par défaut
    void setMod(float v) { modMul = std::exp2(std::clamp(v, 0.f, 1.f) * 3.f); }           // molette -> coupure jusqu'à +3 octaves
    struct AdaaState { double x = 0.0, F = 0.0; };
    AdaaState adaa[2], limS[2];
    int latency() const { return os[0].latency(); }   // échantillons de retard de la saturation (phase linéaire) : à déclarer à l'hôte
    int activeVoices() const { int n = 0; for (auto& v : vc) n += (v.st == Voice::Att || v.st == Voice::Dec || v.st == Voice::Rel); return n; }

    void noteOn(int note, float vel) {
        if (vel <= 0.f) { noteOff(note); return; }
        const bool monoMode = tgt.mode != 0;
        if (monoMode) {
            for (int i = 0; i < nHeld; ++i) if (held[i] == note) { for (int j = i; j + 1 < nHeld; ++j) held[j] = held[j + 1]; --nHeld; break; }
            if (nHeld == 16) { for (int j = 0; j + 1 < 16; ++j) held[j] = held[j + 1]; --nHeld; }
            held[nHeld++] = note;
            if (monoSlot >= 0) {
                Voice& m = vc[monoSlot];
                if (m.serial == monoSerial && m.gate && (m.st == Voice::Att || m.st == Voice::Dec)) {
                    glideTo(m, note);
                    if (tgt.mode == 1) {  // mono : l'enveloppe repart du niveau courant, les oscillateurs et le filtre gardent leur état
                        m.pk = std::clamp(vel, 0.f, 1.f) * .9f; m.st = Voice::Att;
                        m.attLeft = std::max(1, (int)std::lround(tgt.a * sr)); m.attInc = (m.pk - m.lvl) / (float)m.attLeft; m.age = 0;
                    }
                    return;
                }
            }
        } else {
            for (auto& v : vc) if (v.gate && v.note == note) release(v, true);
        }
        if (activeVoices() >= kMaxVoices) steal();
        Voice* nv = nullptr;
        for (auto& v : vc) if (v.st == Voice::Dead) { nv = &v; break; }
        if (!nv) { nv = &vc[0]; for (auto& v : vc) if (v.lvl < nv->lvl) nv = &v; }
        Voice& v = *nv;
        v = Voice();
        v.st = Voice::Att; v.note = note; v.gate = true; v.serial = ++serial;
        v.semi = v.semiT = (float)note;
        if (monoMode) {   // une seule voix tenue : les autres finissent leur relâche ; le glissé part de la dernière note jouée
            for (auto& o : vc) if (&o != &v && o.gate && (o.st == Voice::Att || o.st == Voice::Dec)) release(o, true);
            if (tgt.glide > .0005f && lastSemi >= 0.f) { v.semi = lastSemi; v.glLeft = std::max(1, (int)(tgt.glide * sr)); v.glStep = ((float)note - v.semi) / (float)v.glLeft; }
            monoSlot = (int)(&v - vc); monoSerial = v.serial;
        }
        lastSemi = (float)note;
        v.f0 = 440.f * std::exp2((v.semi - 69.f) / 12.f);
        v.pk = std::clamp(vel, 0.f, 1.f) * .9f;
        v.attLeft = std::max(1, (int)std::lround(tgt.a * sr));
        v.attInc = v.pk / (float)v.attLeft;
        v.ra = std::max(.004f, tgt.a * .5f);
        v.N = std::clamp(tgt.uni, 1, kMaxUni);
        for (int k = 0; k < v.N; ++k) {
            const float spread = v.N > 1 ? ((float)k / (float)(v.N - 1) - .5f) * 2.f : 0.f, x = (spread * .7f + 1.f) * .5f;  // panoramique à puissance constante
            v.gl[k] = std::cos(x * kTwoPi * .25f); v.gr[k] = std::sin(x * kTwoPi * .25f);
        }
        if (tgt.uniRand && v.N > 1) {   // phases d'unisson aléatoires, graine = note et numéro de voix (reproductible) ; la voix 0 garde la phase 0
            uint32_t h = (uint32_t)note * 2654435761u ^ v.serial * 2246822519u ^ 0x85EBCA6Bu;
            auto rnd = [&h]() { h ^= h << 13; h ^= h >> 17; h ^= h << 5; return (float)(h >> 8) * (1.f / 16777216.f); };
            for (int k = 1; k < v.N; ++k) { v.o1[k].ph = rnd(); v.o2[k].ph = rnd(); }
        }
        v.fl.reset(); v.fr.reset();
    }
    void noteOff(int note) {
        if (tgt.mode != 0 && monoSlot >= 0) {
            for (int i = 0; i < nHeld; ++i) if (held[i] == note) { for (int j = i; j + 1 < nHeld; ++j) held[j] = held[j + 1]; --nHeld; break; }
            Voice& m = vc[monoSlot];
            if (m.serial == monoSerial && m.gate && (m.st == Voice::Att || m.st == Voice::Dec)) {
                if (nHeld > 0) glideTo(m, held[nHeld - 1]); else release(m, false);
                return;
            }
        }
        for (auto& v : vc) if (v.gate && v.note == note) release(v, false);
    }
    void sustain(bool on) {
        pedal = on;
        if (!on) for (auto& v : vc) if (v.sustained) { v.sustained = false; release(v, true); }
    }
    void allNotesOff() { nHeld = 0; for (auto& v : vc) if (v.st != Voice::Dead) { v.sustained = false; release(v, true); } }

    // Écrit n échantillons stéréo (écrase).
    void render(float* L, float* R, int n) {
        for (int off = 0; off < n; off += kCtrl) {
            const int c = std::min(kCtrl, n - off);
            step();
            float *l = L + off, *r = R + off;
            std::fill(l, l + c, 0.f); std::fill(r, r + c, 0.f);
            for (auto& v : vc) if (v.st != Voice::Dead) voice(v, l, r, c);
            fx(l, r, c);
        }
    }

private:
    float sr = 48000.f, tempo = 120.f, bendR = 1.f, modMul = 1.f;
    int held[16] = {}, nHeld = 0, monoSlot = -1;
    uint32_t monoSerial = 0, lfoRng = 0x9E3779B9u;
    float lastSemi = -1.f;
    // modulation (recalculée à chaque pas de contrôle) : octaves de coupure, demi-tons, gain de volume, ajout de FM, octaves de la coupure 2
    float lfoPh[2] = {0, 0}, lfoSm[2] = {0, 0}, lfoSh[2] = {0, 0}, mCut = 0, mPitch = 0, mFm = 0, mCut2 = 0, mVol = 1.f, mVolPrev = 1.f;
    float f2g = 0.f, f2a1 = 0.f, f2a2 = 0.f, f2a3 = 0.f, f2k = 1.f;  // coefficients du second filtre (communs à toutes les voix)
    Params tgt, cur;
    bool fresh = true, pedal = false;
    uint32_t serial = 0;
    Voice vc[kSlots];
    // valeurs dérivées, recalculées à chaque pas de contrôle
    float fBase = 1000.f, fPeak = 1000.f, qLin = 1.f, dCoef = .01f, rCoef = .01f, modCoef = .01f, semiR = 1.f;
    // effets
    voile::Oversampler os[2];
    voile::Delay cho[2], dl[2];
    onek::Biquad dlp[2];
    voile::Reverb rev;
    voile::Smooth dSm;
    float ph1 = 0.f, ph2 = 0.f;
    static constexpr float kRevCal = .85f;  // gain de puissance de la FDN (0,27) ramené à ~0,2, la valeur du convolveur normalisé du prototype (voir orbesynth_test cal)

    void glideTo(Voice& v, int note) {
        v.note = note; v.semiT = (float)note; lastSemi = (float)note;
        if (tgt.glide > .0005f) { v.glLeft = std::max(1, (int)(tgt.glide * sr)); v.glStep = (v.semiT - v.semi) / (float)v.glLeft; }
        else { v.semi = v.semiT; v.glLeft = 0; }
    }
    void release(Voice& v, bool force) {
        if (v.st != Voice::Att && v.st != Voice::Dec) return;
        if (pedal && !force) { v.sustained = true; v.gate = false; return; }
        v.gate = false; v.st = Voice::Rel; v.relN = 0;
        v.relMax = std::max(1, (int)((cur.r * 1.6f + .05f) * sr));
    }
    void steal() {  // vole la voix en relâche la plus discrète, sinon la plus ancienne
        Voice* best = nullptr;
        for (auto& v : vc) if (v.st == Voice::Rel && (!best || v.lvl < best->lvl)) best = &v;
        if (!best) for (auto& v : vc) if ((v.st == Voice::Att || v.st == Voice::Dec) && (!best || v.serial < best->serial)) best = &v;
        if (best) { best->st = Voice::Fast; best->gate = false; best->sustained = false; }
    }
    void step() {
        const float k = fresh ? 1.f : 1.f - std::exp(-(float)kCtrl / (.015f * sr));
        fresh = false;
        auto m = [k](float& a, float b) { a += (b - a) * k; };
        m(cur.semi, tgt.semi); m(cur.mix, tgt.mix); m(cur.fm, tgt.fm); m(cur.det, tgt.det); m(cur.sub, tgt.sub); m(cur.cut, tgt.cut);
        m(cur.res, tgt.res); m(cur.famt, tgt.famt); m(cur.fdec, tgt.fdec); m(cur.a, tgt.a); m(cur.d, tgt.d); m(cur.s, tgt.s); m(cur.r, tgt.r);
        m(cur.drive, tgt.drive); m(cur.cho, tgt.cho); m(cur.dly, tgt.dly); m(cur.rev, tgt.rev); m(cur.vol, tgt.vol);
        cur.w1 = tgt.w1; cur.w2 = tgt.w2; cur.uni = tgt.uni; cur.dlyDiv = tgt.dlyDiv; cur.aa = tgt.aa;
        m(cur.detx, tgt.detx); m(cur.f2cut, tgt.f2cut); m(cur.f2res, tgt.f2res);
        cur.f2mode = tgt.f2mode; cur.mode = tgt.mode; cur.glide = tgt.glide; cur.bendSemi = tgt.bendSemi; cur.uniRand = tgt.uniRand;
        for (int i = 0; i < 2; ++i) { m(cur.lfoDepth[i], tgt.lfoDepth[i]); m(cur.lfoRate[i], tgt.lfoRate[i]); cur.lfoShape[i] = tgt.lfoShape[i]; cur.lfoDest[i] = tgt.lfoDest[i]; }
        lfos();
        fBase = 60.f * std::pow(300.f, cur.cut);
        fPeak = std::min(19000.f, fBase * (1.f + cur.famt * 18.f));
        qLin = std::pow(10.f, (.5f + cur.res * 14.f) / 20.f);  // Q en dB (Web Audio)
        dCoef = 1.f - std::exp(-3.f / (std::max(cur.d, .001f) * sr));
        rCoef = 1.f - std::exp(-3.f / (std::max(cur.r, .001f) * sr));
        modCoef = 1.f - std::exp(-1.f / (std::max(.05f, cur.d * .4f) * sr));
        semiR = std::exp2(cur.semi / 12.f);
    }
    void lfos() {   // 2 LFO libres : valeur lissée sur ~1,5 ms (pas de clic avec le carré), sommée par cible
        float cut = 0, pit = 0, fm = 0, c2 = 0, vol = 0;
        const float sm = 1.f - std::exp(-(float)kCtrl / (.0015f * sr));
        for (int i = 0; i < 2; ++i) {
            lfoPh[i] += cur.lfoRate[i] * (float)kCtrl / sr;
            if (lfoPh[i] >= 1.f) { lfoPh[i] -= std::floor(lfoPh[i]); lfoRng = lfoRng * 1664525u + 1013904223u; lfoSh[i] = (float)(int32_t)lfoRng / 2147483648.f; }
            const float p = lfoPh[i];
            float y;
            switch (cur.lfoShape[i]) {
                case 0: y = std::sin(kTwoPi * p); break;
                case 1: y = p < .5f ? 4.f * p - 1.f : 3.f - 4.f * p; break;
                case 2: y = 2.f * p - 1.f; break;
                case 3: y = p < .5f ? 1.f : -1.f; break;
                default: y = lfoSh[i]; break;
            }
            lfoSm[i] += (y - lfoSm[i]) * sm;
            const float d = cur.lfoDepth[i] * lfoSm[i];
            switch (cur.lfoDest[i]) {
                case 1: cut += d * 3.f; break;           // ±3 octaves de coupure à pleine profondeur
                case 2: pit += d * 2.f; break;           // ±2 demi-tons
                case 3: vol += cur.lfoDepth[i] * (.5f - .5f * lfoSm[i]); break;   // trémolo : le gain descend de 0 à -profondeur
                case 4: fm += d * .5f; break;
                case 5: c2 += d * 3.f; break;
                default: break;
            }
        }
        mCut = cut; mPitch = pit; mFm = fm; mCut2 = c2; mVolPrev = mVol; mVol = std::clamp(1.f - vol, 0.f, 1.f);
        if (cur.f2mode > 0) {   // SVF à zéro délai (TPT, Zavalishin) : g = tan(pi f / fs), k = 1/Q
            const float f = std::clamp(60.f * std::pow(300.f, std::clamp(cur.f2cut + mCut2 / 8.f, 0.f, 1.f)) , 20.f, .45f * sr);
            f2g = std::tan(3.14159265f * f / sr); f2k = 1.f / (.5f + cur.f2res * 10.f);
            f2a1 = 1.f / (1.f + f2g * (f2g + f2k)); f2a2 = f2g * f2a1; f2a3 = f2g * f2a2;
        }
    }
    void ctrl(Voice& v, int n) {
        const float isr = 1.f / sr;
        if (v.glLeft > 0) {
            const int k = std::min(v.glLeft, n); v.semi += v.glStep * (float)k; v.glLeft -= k;
            if (v.glLeft == 0) v.semi = v.semiT;
        }
        v.f0 = 440.f * std::exp2((v.semi - 69.f) / 12.f);
        v.fb = v.f0 * bendR * (mPitch != 0.f ? std::exp2(mPitch / 12.f) : 1.f); v.b2 = v.fb * semiR; v.incMod = v.b2 * isr; v.incSub = v.fb * .5f * isr;
        const float detTot = cur.det + cur.detx;
        for (int k = 0; k < v.N; ++k) {
            const float spread = v.N > 1 ? ((float)k / (float)(v.N - 1) - .5f) * 2.f : 0.f;
            v.r1[k] = std::exp2(spread * detTot / 1200.f) * isr;
            v.r2[k] = std::exp2(-spread * detTot * .7f / 1200.f) * isr;
        }
        v.vg = .3f * (1.f - cur.mix * .4f) / std::sqrt((float)v.N);
        const float t = (float)v.age * isr;
        float f = t < v.ra ? fBase + (fPeak - fBase) * t / v.ra : fBase + (fPeak - fBase) * std::exp(-(t - v.ra) * 3.f / std::max(cur.fdec, .001f));
        f = std::clamp(f * modMul * (mCut != 0.f ? std::exp2(mCut) : 1.f), 20.f, .45f * sr);
        v.fl.lowpass(sr, f, qLin);
        v.fr.b0 = v.fl.b0; v.fr.b1 = v.fl.b1; v.fr.b2 = v.fl.b2; v.fr.a1 = v.fl.a1; v.fr.a2 = v.fl.a2;
    }
    float svf(float* z, float x) {   // sortie selon le mode : 1 passe-bas, 2 passe-haut, 3 passe-bande (12 dB/oct)
        const float v3 = x - z[1], v1 = f2a1 * z[0] + f2a2 * v3, v2 = z[1] + f2a2 * z[0] + f2a3 * v3;
        z[0] = 2.f * v1 - z[0]; z[1] = 2.f * v2 - z[1];
        return cur.f2mode == 1 ? v2 : cur.f2mode == 2 ? x - f2k * v1 - v2 : v1 * f2k;
    }
    void voice(Voice& v, float* L, float* R, int n) {
        ctrl(v, n);
        const float fmv = std::clamp(cur.fm + mFm, 0.f, 1.f);
        const bool useFm = fmv > 0.f, useO2 = cur.mix > 0.f, useF2 = cur.f2mode > 0;
        const float vm0 = mVolPrev, dvm = (mVol - mVolPrev) / (float)n;
        const int fadeN = std::max(1, (int)(.005f * sr));
        const float fastC = std::exp(-1.f / (.0007f * sr));
        for (int i = 0; i < n; ++i) {
            float modHz = 0.f;
            v.modLvl += (1.2f - v.modLvl) * modCoef;
            if (useFm) modHz = oscTick(v.mod, cur.w2, v.incMod, cur.aa) * v.f0 * fmv * v.modLvl;
            float sl = 0.f, sr_ = 0.f;
            for (int k = 0; k < v.N; ++k) {
                float s = oscTick(v.o1[k], cur.w1, (v.fb + modHz) * v.r1[k], cur.aa);
                if (useO2 && !(useFm && k > 0)) s += cur.mix * oscTick(v.o2[k], cur.w2, v.b2 * v.r2[k], cur.aa);
                sl += s * v.gl[k]; sr_ += s * v.gr[k];
            }
            sl = v.fl.process(sl * v.vg); sr_ = v.fr.process(sr_ * v.vg);
            if (useF2) { sl = svf(v.s2l, sl); sr_ = svf(v.s2r, sr_); }
            float g = 1.f;
            switch (v.st) {
                case Voice::Att: v.lvl += v.attInc; if (--v.attLeft <= 0) { v.lvl = v.pk; v.st = Voice::Dec; } break;
                case Voice::Dec:
                    v.lvl += (v.pk * cur.s - v.lvl) * dCoef;
                    if (cur.s <= 0.f && v.lvl < 1e-5f) v.st = Voice::Dead;
                    break;
                case Voice::Rel:
                    v.lvl -= v.lvl * rCoef; ++v.relN;
                    if (v.relN > v.relMax - fadeN) g = std::max(0.f, (float)(v.relMax - v.relN) / (float)fadeN);
                    if (v.relN >= v.relMax || v.lvl < 1e-6f) v.st = Voice::Dead;
                    break;
                case Voice::Fast: v.lvl *= fastC; if (v.lvl < 1e-4f) v.st = Voice::Dead; break;
                default: break;
            }
            float sb = 0.f;
            if (cur.sub > 0.f) sb = oscTick(v.sub, Sine, v.incSub) * cur.sub * .35f;  // le sub contourne le filtre
            const float a = v.lvl * g * (vm0 + dvm * (float)(i + 1));
            L[i] += (sl + sb) * a; R[i] += (sr_ + sb) * a;
        }
        v.age += n;
    }
    static float lim(float x) {  // transparent jusqu'à 0,8, puis arrondi vers 1 (jamais atteint)
        const float a = std::fabs(x);
        return a <= .8f ? x : std::copysign(.8f + .195f * std::tanh((a - .8f) * 5.f), x);
    }
    // Même limiteur en ADAA d'ordre 1 (antidérivée en double) : la sortie, saturée par un drive fort, ne se replie plus (retard d'une demi-échantillon)
    static float limAd(AdaaState& st, float xf) {
        const double x = xf, a = std::fabs(x);
        double F;
        if (a <= 0.8) F = 0.5 * a * a;
        else { const double u = (a - 0.8) * 5.0; F = 0.32 + 0.8 * (a - 0.8) + 0.195 * (u + std::log1p(std::exp(-2.0 * u)) - 0.6931471805599453) / 5.0; }
        const double dx = x - st.x; double y;
        if (std::fabs(dx) > 1e-7) y = (F - st.F) / dx; else y = lim((float)(0.5 * (x + st.x)));
        st.x = x; st.F = F;
        return (float)y;
    }
    void fx(float* l, float* r, int n) {
        const float vol = cur.vol, gc = cur.cho * .7f, dg = cur.dly * .6f * vol, rg = cur.rev * .55f * vol * kRevCal;
        const bool dr = cur.drive > 5e-4f;
        const float pre = 1.f + cur.drive * 2.f, kk = 1.f + cur.drive * 8.f, nk = 1.f / std::tanh(kk), amt = std::min(1.f, cur.drive * 50.f);
        const float tD = std::min(60.f / tempo * divBeats()[std::clamp(cur.dlyDiv, 0, 5)], 3.9f) * sr;
        // tanh(kk*clamp(pre*x)) en ADAA d'ordre 1 (antidérivée en double) : l'arête de l'écrêtage ne se replie presque plus, même à drive maxi
        auto shapeAd = [&](AdaaState& st, float xf) {
            const double x = xf, ax = std::fabs(x), P = pre, K = kk, t = std::tanh(K);
            double F;
            if (ax * P <= 1.0) { const double u = K * ax * P; F = (u + std::log1p(std::exp(-2.0 * u)) - 0.6931471805599453) / (K * P); }
            else F = ((K + std::log1p(std::exp(-2.0 * K)) - 0.6931471805599453) / (K * P) + t * (ax - 1.0 / P));
            const double dx = x - st.x; double y;
            if (std::fabs(dx) > 1e-7) y = (F - st.F) / dx; else y = std::tanh(K * std::max(-1.0, std::min(1.0, 0.5 * (x + st.x) * P)));
            st.x = x; st.F = F;
            return (float)(y * nk);
        };
        for (int i = 0; i < n; ++i) {
            float x[2] = {l[i], r[i]};
            for (int c = 0; c < 2; ++c) {  // saturation tanh suréchantillonnée ; sans drive, le signal passe par le retard seul (même latence)
                voile::Oversampler& o = os[c];
                if (dr) { const float yb = o.process(x[c], [&](float v) { return shapeAd(adaa[c], v); }); const float xd = o.dry(); x[c] = xd + (yb - xd) * amt; }
                else { x[c] = o.bypass(x[c]); adaa[c] = AdaaState(); }
            }
            // chorus : deux voix modulées, panoramiquées à -0,8 / +0,8
            cho[0].push(x[0]); cho[1].push(x[1]);
            ph1 += .6f / sr; if (ph1 >= 1.f) ph1 -= 1.f;
            ph2 += .43f / sr; if (ph2 >= 1.f) ph2 -= 1.f;
            const float d1 = (.012f + .003f * std::sin(kTwoPi * ph1)) * sr, d2 = (.019f + .003f * std::sin(kTwoPi * ph2)) * sr;
            const float a1l = cho[0].read(d1) * gc, a1r = cho[1].read(d1) * gc, a2l = cho[0].read(d2) * gc, a2r = cho[1].read(d2) * gc;
            const float ml = x[0] + a1l + a1r * .9511f + a2l * .3090f, mr = x[1] + a1r * .3090f + a2r + a2l * .9511f;
            float ol = ml * vol, orr = mr * vol, rl, rr;
            rev.process(.5f * (ml + mr), .4f, rl, rr);
            ol += rl * rg; orr += rr * rg;
            // delay synchronisé : réinjection 0,38, passe-bas 3,5 kHz dans la boucle, panoramique +0,35
            if (dSm.v < 2.f) dSm.v = tD;
            const float D = dSm.next(tD);
            const float zl = dlp[0].process(dl[0].read(D - 1.f)), zr = dlp[1].process(dl[1].read(D - 1.f));
            dl[0].push(ml + zl * .38f); dl[1].push(mr + zr * .38f);
            ol += zl * .8526f * dg; orr += (zr + zl * .5225f) * dg;
            l[i] = limAd(limS[0], ol); r[i] = limAd(limS[1], orr);
        }
    }
};

}  // namespace osyn
