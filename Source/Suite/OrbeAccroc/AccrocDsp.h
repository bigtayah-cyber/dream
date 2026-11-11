// Orbe Accroc : accidents de lecture volontaires sur la voix (moteur sans JUCE, header-only).
// Principe : un historique circulaire de l'entrée ; la sortie neutre relit l'historique avec un retard fixe (latence déclarée).
// Chaque accident est une « voix » qui relit l'historique autrement (répétition, sens inverse, vitesse variable, grains figés,
// saut, trous, réduction). Toute rupture de lecture passe par un fondu croisé en cosinus surélevé de la durée de la latence :
// la tête quittée continue sa course le temps du fondu (elle ne lit jamais au-delà de l'échantillon le plus récent).
// Déclenchement : séquenceur 16 pas calé sur la PPQ de l'hôte, hasard à graine (tirage déterminé par la graine et l'indice
// du pas : même position = même résultat, quelle que soit la taille des blocs), notes MIDI / pads (C … G de chaque octave).
#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace acr {
constexpr double kPi = 3.14159265358979323846;

enum Type { None = -1, Stutter = 0, Reverse, TapeStop, Freeze, Stammer, Crush, Skip, Gate, kNumTypes };
// Noms affichés (UTF-8 : passer par kit::u8 côté JUCE).
inline const char* const kTypeNames[kNumTypes] = {"Stutter", "Retour", "Arr\xC3\xAAt", "Gel", "B\xC3\xA9gaie", "Crush", "Saut", "Trous"};
inline const char* const kTypeShort[kNumTypes] = {"STUT", "RETR", "STOP", "GEL", "B\xC3\x89G", "CRSH", "SAUT", "TROU"};
inline const char kTypeCode[kNumTypes] = {'S', 'R', 'T', 'F', 'B', 'C', 'J', 'G'};

// Divisions (en noires)
inline constexpr double kStepQ[3] = {0.5, 0.25, 0.125};                    // pas : 1/8, 1/16, 1/32
inline constexpr double kStuQ[5] = {1.0, 0.5, 0.25, 0.125, 0.0625};        // stutter : 1/4 … 1/64
inline constexpr double kRevQ[4] = {1.0, 0.5, 0.25, 0.125};                // retour : 1/4 … 1/32
inline constexpr double kRepQ[3] = {1.0, 0.5, 0.25};                       // bégaiement : 1/4, 1/8, 1/16
inline constexpr double kGateQ[3] = {0.25, 0.125, 0.0625};                 // trous : 1/16, 1/32, 1/64
inline constexpr double kTailQ[3] = {0.5, 0.75, 1.0};                      // traîne : 1/8, 3/16, 1/4

struct Params {
    float mix = 1.f, outDb = 0.f, chaos = 0.f, prob = 1.f;
    int seed = 1, clock = 1, stepDiv = 1;                  // clock : 0 = seulement en lecture, 1 = toujours (horloge interne à l'arrêt)
    int stuDiv = 3; float stuDecay = 0.2f, stuPitch = 0.f;
    int revDiv = 1;
    float tapeDepth = 1.f, tapeCurve = 0.5f;
    float frzMs = 80.f, frzJitter = 0.3f;
    int repDiv = 2; float repGap = 0.35f;
    float crBits = 6.f, crRate = 6000.f, crSmooth = 0.5f;
    int skipRange = 4;
    int gateDiv = 1; float gateDens = 0.5f;
    float tail = 0.f; int tailDiv = 1;                     // lot E : traîne d'écho après l'accident (0 = coupée : sortie d'avant, identique)
};

// ------------------------------------------------------------------ séquence
struct Step { int type = None; float prob = 1.f, inten = 0.9f; int len = 1; };
struct Seq { std::array<Step, 16> s{}; };
constexpr int kMaxLen = 16;

inline void sanitize(Seq& q) {
    for (auto& st : q.s) {
        st.type = std::clamp(st.type, (int)None, (int)kNumTypes - 1);
        st.prob = std::isfinite(st.prob) ? std::clamp(st.prob, 0.f, 1.f) : 1.f;
        st.inten = std::isfinite(st.inten) ? std::clamp(st.inten, 0.f, 1.f) : 0.9f;
        st.len = std::clamp(st.len, 1, kMaxLen);
    }
}
// Format versionné : "ACR1" puis 16 × ";type,prob%,int%,len".
inline std::string toString(const Seq& q) {
    std::string o = "ACR1"; char b[48];
    for (const auto& st : q.s) {
        std::snprintf(b, sizeof b, ";%d,%d,%d,%d", st.type, (int)std::lround(st.prob * 100.f), (int)std::lround(st.inten * 100.f), st.len);
        o += b;
    }
    return o;
}
inline bool fromString(const std::string& str, Seq& out) {
    if (str.rfind("ACR1", 0) != 0) return false;   // version inconnue : refus (l'état courant est conservé)
    Seq q; const char* c = str.c_str() + 4;
    for (auto& st : q.s) {
        int t, p, i, l, used = 0;
        if (std::sscanf(c, ";%d,%d,%d,%d%n", &t, &p, &i, &l, &used) != 4) return false;
        st = {t, (float)p / 100.f, (float)i / 100.f, l}; c += used;
    }
    sanitize(q); out = q; return true;
}
// Motif compact : 16 caractères (S R T F B C J G ; minuscule = probabilité 50 %, '.' = rien), durées en chiffres hexa optionnelles.
inline Seq pattern(const char* types, const char* lens = nullptr, float inten = 0.9f) {
    Seq q;
    for (int i = 0; i < 16 && types[i]; ++i) {
        const char c = types[i]; const char up = (char)(c >= 'a' && c <= 'z' ? c - 32 : c);
        for (int k = 0; k < kNumTypes; ++k) if (kTypeCode[k] == up) { q.s[(size_t)i].type = k; q.s[(size_t)i].prob = up == c ? 1.f : 0.5f; }
        q.s[(size_t)i].inten = inten;
        if (lens && lens[i]) { const char d = lens[i]; q.s[(size_t)i].len = d >= '1' && d <= '9' ? d - '0' : d >= 'a' && d <= 'g' ? d - 'a' + 10 : 1; }
    }
    sanitize(q); return q;
}

struct Host { bool playing = false, hasPos = false, hasBpm = false; double ppq = 0, bpm = 120; };
struct Ev { int offset = 0, note = 60; float vel = 0.f; };   // vel 0 : relâchement

// Hasard reproductible (splitmix64)
inline uint64_t mix64(uint64_t x) {
    x += 0x9E3779B97F4A7C15ull; x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull; x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
    return x ^ (x >> 31);
}
inline float unit(uint64_t& s) { s = mix64(s); return (float)((double)(s >> 40) * (1.0 / 16777216.0)); }

// Limiteur doux de sortie : identité jusqu'à 0,9 (−0,9 dBFS), puis courbe tangente vers 1,0 (pente continue).
inline float safeOut(float y) {
    const float a = std::fabs(y);
    if (a <= 0.9f) return y;
    return std::copysign(0.9f + 0.1f * std::tanh((a - 0.9f) * 10.f), y);
}

// ------------------------------------------------------------------ moteur
class Engine {
public:
    static constexpr int kVoices = 3, kMaxEv = 512;
    static constexpr int64_t kNoStep = INT64_MIN;

    void prepare(double sampleRate, int maxBlock) {
        (void)maxBlock;
        sr = sampleRate > 1000.0 ? sampleRate : 48000.0;
        lat = std::max(32, (int)std::lround(sr * 0.004));       // 4 ms : durée des fondus et de l'avance de lecture
        size_t n = 1; while ((double)n < sr * 10.0) n <<= 1;    // ~10 s d'historique
        ringLen = (int64_t)n; mask = n - 1;
        hl.assign(n, 0.f); hr.assign(n, 0.f);
        { size_t m = 1; while ((double)m < sr * 4.0) m <<= 1; tailMask = m - 1; tl.assign(m, 0.f); tr.assign(m, 0.f); }
        envStep = 1.f / (float)lat; gateStep = 1.f / (float)(0.002 * sr);
        cs = (float)(1.0 - std::exp(-1.0 / (0.02 * sr)));
        reset();
    }
    void reset() {
        std::fill(hl.begin(), hl.end(), 0.f); std::fill(hr.begin(), hr.end(), 0.f);
        w = 0; for (auto& v : voices) v = Voice{};
        anchored = false; anchorQ = 0; anchorAbs = 0; lastStep = kNoStep; running = false;
        sMix = p.mix; sOut = gain(p.outDb); sBits = p.crBits; sRate = p.crRate; sSmooth = p.crSmooth; sDens = p.gateDens;
        uiStep = -1; for (auto& a : uiAct) a = 0.f;
        std::fill(tl.begin(), tl.end(), 0.f); std::fill(tr.begin(), tr.end(), 0.f); tw = 0; sTail = 0.f; dT = 0.f; tlp[0] = tlp[1] = 0.f; tailDirty = false;
    }
    int latency() const { return lat; }
    void set(const Params& np) { p = np; p.seed = std::max(1, p.seed); }
    const Params& params() const { return p; }

    // Séquence : écrite par le fil de message, lue sans attente par le fil audio (essai de verrou).
    void pushSeq(const Seq& q) {
        while (busy.exchange(true, std::memory_order_acquire)) {}
        box = q; ver.fetch_add(1, std::memory_order_relaxed);
        busy.store(false, std::memory_order_release);
    }

    void beginBlock(const Host& h, const Ev* ev, int n) {
        nEv = std::clamp(n, 0, kMaxEv); for (int i = 0; i < nEv; ++i) evs[(size_t)i] = ev[i];
        evI = 0; blockPos = 0;
        const double nb = h.hasBpm && h.bpm > 1.0 && h.bpm < 1000.0 ? h.bpm : bpm;
        const double pred = anchorQ + (double)(w - anchorAbs) * qps;
        if (nb != bpm) { anchorQ = pred; anchorAbs = w; bpm = nb; }
        qps = bpm / 60.0 / sr; spq = sr * 60.0 / bpm;
        stepQ = kStepQ[std::clamp(p.stepDiv, 0, 2)]; stepSamples = stepQ * spq;
        const bool play = h.playing && h.hasPos;
        if (play) {
            const double tol = 2.0 * qps;
            if (!anchored || !wasPlaying || std::fabs(h.ppq - (anchorQ + (double)(w - anchorAbs) * qps)) > tol) { anchorQ = h.ppq; anchorAbs = w; anchored = true; }
        }
        wasPlaying = play;
        running = play || p.clock == 1;
        if (!running) lastStep = kNoStep;
    }

    void process(const float* inL, const float* inR, float* outL, float* outR, int n) {
        pullSeq();
        const float tOut = gain(p.outDb);
        for (int i = 0; i < n; ++i, ++blockPos) {
            const int64_t a = w;
            float xl = inL[i], xr = inR ? inR[i] : xl;
            if (!std::isfinite(xl)) xl = 0.f;
            if (!std::isfinite(xr)) xr = 0.f;
            hl[(size_t)a & mask] = std::clamp(xl, -64.f, 64.f); hr[(size_t)a & mask] = std::clamp(xr, -64.f, 64.f);
            while (evI < nEv && evs[(size_t)evI].offset <= blockPos) midi(evs[(size_t)evI++], a);
            if (running) tick(a);
            smooth(sMix, p.mix); smooth(sOut, tOut); smooth(sBits, p.crBits); smooth(sRate, p.crRate); smooth(sSmooth, p.crSmooth); smooth(sDens, p.gateDens);

            const int64_t t = a - lat;
            const float dL = hl[(size_t)t & mask], dR = hr[(size_t)t & mask];
            float eSum = 0.f, wl = 0.f, wr = 0.f;
            for (auto& v : voices) {
                if (v.type == None) continue;
                float vl = 0.f, vr = 0.f;
                render(v, a, dL, dR, vl, vr);
                wl += v.env * vl; wr += v.env * vr; eSum += v.env;
                if (v.active && !v.held && --v.remain <= 0) v.active = false;
                const float tg = v.active ? v.inten : 0.f;
                v.env = v.env < tg ? std::min(tg, v.env + envStep) : std::max(tg, v.env - envStep);
                if (!v.active && v.env <= 0.f) v.type = None;
            }
            float yl = dL, yr = dR;
            if (eSum > 0.f) {
                const float k = eSum > 1.f ? 1.f / eSum : 1.f; const float rest = 1.f - std::min(1.f, eSum);
                yl = dL * rest + wl * k; yr = dR * rest + wr * k;
            }
            const float gl = yl, gr = yr;
            yl = (dL + sMix * (yl - dL)) * sOut; yr = (dR + sMix * (yr - dR)) * sOut;
            if (p.tail > 0.001f || sTail > 1e-4f || tailDirty) {   // traîne : écho synchro en ping-pong, alimenté par l'accident seul (jamais par le signal sec)
                smooth(sTail, p.tail);
                const float tq = (float)(kTailQ[std::clamp(p.tailDiv, 0, 2)] * spq), tgt = std::clamp(tq, 32.f, (float)tailMask - 4.f);
                if (dT <= 0.f) dT = tgt;
                dT += (tgt - dT) * 0.0005f;
                const float rp = (float)(tw & (int64_t)tailMask) - dT; const double fl = std::floor((double)rp); const float fr = rp - (float)fl;
                const size_t i0 = (size_t)((int64_t)fl & (int64_t)tailMask), i1 = (i0 + 1) & tailMask;
                const float d0 = tl[i0] + fr * (tl[i1] - tl[i0]), d1 = tr[i0] + fr * (tr[i1] - tr[i0]);
                const float lpC = 0.55f, fb = 0.25f + 0.5f * std::clamp(sTail, 0.f, 1.f);
                tlp[0] += lpC * (d0 - tlp[0]); tlp[1] += lpC * (d1 - tlp[1]);
                const float send = std::min(1.f, eSum) * sMix, mono = 0.5f * (gl + gr) * send;
                tl[(size_t)tw & tailMask] = std::clamp(mono + fb * tlp[1], -8.f, 8.f);
                tr[(size_t)tw & tailMask] = std::clamp(fb * tlp[0], -8.f, 8.f);
                ++tw;
                const float g = 0.9f * std::min(1.f, sTail * 2.f) * sOut;
                yl += g * d0; yr += g * d1;
                tailDirty = std::fabs(d0) + std::fabs(d1) > 1e-6f || send > 0.f;
                if (!tailDirty && p.tail <= 0.001f) { std::fill(tl.begin(), tl.end(), 0.f); std::fill(tr.begin(), tr.end(), 0.f); tlp[0] = tlp[1] = 0.f; sTail = 0.f; dT = 0.f; }
            }
            outL[i] = safeOut(yl); outR[i] = safeOut(yr);
            ++w;
        }
        uiStep = running && lastStep != kNoStep ? (int)(((lastStep % 16) + 16) % 16) : -1;
        for (auto& x : uiAct) x = 0.f;
        for (const auto& v : voices) if (v.type >= 0) uiAct[(size_t)v.type] += v.env;
    }

    // lecture par l'interface (copiée en atomiques par le processeur)
    int stepNow() const { return uiStep; }
    float activity(int type) const { return uiAct[(size_t)std::clamp(type, 0, kNumTypes - 1)]; }
    double bpmNow() const { return bpm; }

private:
    struct Head { double pos = 0, rate = 1; float g = 1.f; int64_t ph = 0; };
    struct Voice {
        int type = None, note = -1; bool active = false, held = false, seq = false;
        float env = 0.f, inten = 0.f;
        int64_t age = 0, remain = 0, dur = 1, t0 = 0, slice = 1;
        Head cur, old; int xf = 0, xfLen = 1;
        float decay = 0.f, pitch = 0.f, gap = 0.f, depth = 1.f, jit = 0.f; double curveExp = 1.0;
        int range = 0; int64_t gLen = 2; double gStart[2] = {0, 0};
        float gate = 1.f, gateT = 1.f;
        double acc = 1.0; float hold[2] = {0, 0}, lp[2][2] = {{0, 0}, {0, 0}};
        uint64_t rng = 1;
    };

    static float gain(float db) { return db == 0.f ? 1.f : std::pow(10.f, db / 20.f); }
    void smooth(float& s, float target) const { const float d = target - s; s = std::fabs(d) < 1e-6f ? target : s + d * cs; }
    int64_t qs(double quarters) const { return std::max<int64_t>(16, (int64_t)std::llround(quarters * spq)); }

    void pullSeq() {
        if (ver.load(std::memory_order_relaxed) == seenVer) return;
        if (!busy.exchange(true, std::memory_order_acquire)) { seq = box; seenVer = ver.load(std::memory_order_relaxed); busy.store(false, std::memory_order_release); }
    }

    float rd(const std::vector<float>& h, double pos) const {
        const double fl = std::floor(pos); const int64_t i = (int64_t)fl; const float f = (float)(pos - fl);
        const float y1 = h[(size_t)i & mask];
        if (f == 0.f) return y1;
        const float y0 = h[(size_t)(i - 1) & mask], y2 = h[(size_t)(i + 1) & mask], y3 = h[(size_t)(i + 2) & mask];
        const float c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * f + c2) * f + c1) * f + y1;
    }

    // ---- horloge : position logique de la sortie (entrée retardée de la latence) en noires, pas par pas
    void tick(int64_t a) {
        const double q = anchorQ + (double)(a - lat - anchorAbs) * qps, sp = q / stepQ, fk = std::floor(sp);
        const int64_t k = (int64_t)fk;
        if (k == lastStep) return;
        if ((lastStep != kNoStep && k == lastStep + 1) || (sp - fk) * stepSamples < 1.0) onStep(k, a);
        lastStep = k;
    }
    void onStep(int64_t k, int64_t a) {
        const Step& s = seq.s[(size_t)(((k % 16) + 16) % 16)];
        uint64_t h = mix64((uint64_t)p.seed * 0x2545F4914F6CDD1Dull ^ (uint64_t)k);
        const float u1 = unit(h), u2 = unit(h), u3 = unit(h), u4 = unit(h), u5 = unit(h), u6 = unit(h);
        const int shift = u5 < p.chaos * 0.5f ? (u6 < 0.5f ? -1 : 1) : 0;   // le chaos fait aussi varier la division
        if (s.type >= 0 && u1 < s.prob * p.prob) {
            for (auto& v : voices) if (v.active && v.seq) v.active = false;
            start(s.type, s.inten, (int64_t)std::llround((double)s.len * stepSamples), false, true, -1, h, a, shift);
            return;
        }
        bool busySeq = false; for (const auto& v : voices) busySeq |= v.active && v.seq;
        if (!busySeq && p.chaos > 0.f && u2 < p.chaos * 0.5f)
            start(std::min((int)(u3 * (float)kNumTypes), kNumTypes - 1), 0.45f + 0.55f * u4,
                  (int64_t)std::llround((u6 < 0.35f ? 2.0 : 1.0) * stepSamples), false, true, -1, h, a, shift);
    }
    void midi(const Ev& e, int64_t a) {
        if (e.note < 0) { for (auto& v : voices) if (v.held) v.active = false; return; }   // toutes notes coupées
        const int pc = ((e.note % 12) + 12) % 12;
        if (pc >= kNumTypes) return;
        for (auto& v : voices) if (v.active && v.note == e.note) v.active = false;
        if (e.vel > 0.f) start(pc, std::min(1.f, e.vel), (int64_t)std::llround(spq), true, false, e.note, mix64((uint64_t)p.seed ^ (uint64_t)a * 0x9E3779B97F4A7C15ull), a, 0);
    }

    void start(int type, float inten, int64_t dur, bool held, bool fromSeq, int note, uint64_t r, int64_t a, int shift) {
        Voice* v = nullptr;
        for (auto& x : voices) if (x.type == None) { v = &x; break; }
        if (!v) { float be = 2.f; for (auto& x : voices) if (!x.active && x.env < be) { be = x.env; v = &x; } }
        if (!v) { float be = 2.f; for (auto& x : voices) if (x.env < be) { be = x.env; v = &x; } }
        *v = Voice{};
        v->type = type; v->active = true; v->held = held; v->seq = fromSeq; v->note = note;
        v->inten = std::clamp(inten, 0.f, 1.f); v->rng = r | 1u;
        v->dur = std::max<int64_t>(1, dur); v->remain = v->dur;
        const int64_t t = a - lat; v->t0 = t; v->cur = {(double)t, 1.0, 1.f, 0}; v->xfLen = lat;
        switch (type) {
            case Stutter: v->slice = qs(kStuQ[std::clamp(p.stuDiv + shift, 0, 4)]); v->decay = 0.6f * p.stuDecay; v->pitch = p.stuPitch; break;
            case Stammer: v->slice = qs(kRepQ[std::clamp(p.repDiv + shift, 0, 2)]); v->decay = 0.08f; v->gap = std::clamp(p.repGap, 0.f, 0.8f); break;
            case Reverse: v->slice = qs(kRevQ[std::clamp(p.revDiv + shift, 0, 3)]); v->cur.rate = -1.0; break;
            case TapeStop:
                v->depth = std::clamp(p.tapeDepth, 0.f, 1.f); v->curveExp = std::pow(4.0, 1.0 - 2.0 * std::clamp(p.tapeCurve, 0.f, 1.f));
                if (held) v->dur = qs(1.0);
                break;
            case Freeze: {
                v->gLen = std::max<int64_t>(64, (int64_t)std::llround(std::clamp(p.frzMs, 10.f, 500.f) * 0.001 * sr)) & ~(int64_t)1;
                v->jit = std::clamp(p.frzJitter, 0.f, 1.f);
                for (int j = 0; j < 2; ++j) v->gStart[j] = grainStart(*v);
                break; }
            case Skip: v->slice = qs(0.25 * std::exp2(-shift)); v->range = std::clamp(p.skipRange, 1, 8); break;
            case Gate: v->slice = qs(kGateQ[std::clamp(p.gateDiv + shift, 0, 2)]); break;
            case Crush: v->acc = 1.0; { const float dl = hl[(size_t)t & mask], dr = hr[(size_t)t & mask]; v->hold[0] = v->lp[0][0] = v->lp[0][1] = dl; v->hold[1] = v->lp[1][0] = v->lp[1][1] = dr; } break;
            default: break;
        }
        if (type == Stutter || type == Stammer || type == Reverse || type == Skip) v->xfLen = (int)std::clamp<int64_t>(v->slice / 4, 8, lat);
    }

    void jump(Voice& v, Head nh) { v.old = v.cur; v.cur = nh; v.xf = v.xfLen; }
    double grainStart(Voice& v) { return (double)(v.t0 - v.gLen) - std::floor((double)(unit(v.rng) * v.jit) * (double)v.gLen); }

    // fenêtre de bégaiement : silence en fin de tranche (fondu de xfLen)
    float window(const Voice& v, int64_t ph) const {
        if (v.gap <= 0.f) return 1.f;
        const double end = (double)v.slice * (1.0 - v.gap), F = (double)v.xfLen, x = (double)ph;
        return x < end - F ? 1.f : x < end ? (float)((end - x) / F) : 0.f;
    }
    void readHead(Voice& v, Head& h, int64_t a, float& l, float& r) {
        if (h.pos > (double)(a - 2)) h.pos = (double)(a - 2);
        if (h.pos < (double)(a - ringLen + 4)) { l = r = 0.f; }
        else { const float g = h.g * window(v, h.ph); l = rd(hl, h.pos) * g; r = rd(hr, h.pos) * g; }
        h.pos += h.rate; ++h.ph;
    }
    void heads(Voice& v, int64_t a, float& oL, float& oR) {
        float cl, cr; readHead(v, v.cur, a, cl, cr);
        if (v.xf > 0) {
            float ol, orr; readHead(v, v.old, a, ol, orr);
            const float x = (float)(v.xfLen - v.xf + 1) / (float)(v.xfLen + 1), m = 0.5f - 0.5f * std::cos((float)kPi * x);
            oL = ol + (cl - ol) * m; oR = orr + (cr - orr) * m; --v.xf;
        } else { oL = cl; oR = cr; }
    }

    void render(Voice& v, int64_t a, float dL, float dR, float& oL, float& oR) {
        const int64_t t = a - lat, age = v.age++;
        switch (v.type) {
            case Stutter: case Stammer:
                if (age > 0 && age % v.slice == 0) {
                    if (a - v.t0 > ringLen / 2) v.t0 = t - v.slice;   // maintien très long : nouvelle capture (tranche la plus récente)
                    const double k = (double)(age / v.slice);
                    const double r = std::clamp(std::exp2((double)v.pitch * k / 12.0), 0.25, std::min(4.0, 1.0 + k));
                    jump(v, {(double)v.t0, r, std::pow(1.f - v.decay, (float)k), 0});
                }
                heads(v, a, oL, oR); break;
            case Reverse:
                if (age > 0 && age % v.slice == 0) jump(v, {(double)t, -1.0, 1.f, 0});
                heads(v, a, oL, oR); break;
            case TapeStop: {
                const double x = std::min(1.0, (double)age / (double)v.dur), r = 1.0 - (double)v.depth * std::pow(x, v.curveExp);
                v.cur.rate = r;
                const float g = (float)std::min(1.0, r / 0.2); v.cur.g = g * g * (3.f - 2.f * g);   // la bande s'éteint en ralentissant
                heads(v, a, oL, oR); break; }
            case Freeze: {
                const int64_t G = v.gLen, H = G / 2; oL = oR = 0.f;
                for (int j = 0; j < 2; ++j) {
                    const int64_t gp = (age + (int64_t)j * H) % G;
                    if (gp == 0 && age > 0) { if (a - v.t0 > ringLen / 2) v.t0 = t; v.gStart[j] = grainStart(v); }
                    const double pos = std::max(v.gStart[j] + (double)gp, (double)(a - ringLen + 4));
                    const float s = std::sin((float)kPi * (float)gp / (float)G), wg = s * s;   // deux grains de Hann décalés : somme = 1
                    oL += wg * rd(hl, pos); oR += wg * rd(hr, pos);
                }
                break; }
            case Skip:
                if (age % v.slice == 0) {
                    const int m = std::min(v.range, (int)(unit(v.rng) * (float)(v.range + 1)));
                    const double np = (double)(t - (int64_t)m * v.slice);
                    if (np != v.cur.pos) jump(v, {np, 1.0, 1.f, 0});
                }
                heads(v, a, oL, oR); break;
            case Gate:
                if (age % v.slice == 0) v.gateT = unit(v.rng) < sDens ? 0.f : 1.f;
                v.gate += std::clamp(v.gateT - v.gate, -gateStep, gateStep);
                oL = dL * v.gate; oR = dR * v.gate; break;
            case Crush: {
                v.acc += (double)sRate / sr;
                if (v.acc >= 1.0) {
                    v.acc -= std::floor(v.acc);
                    const float q = std::exp2(std::clamp(sBits, 1.f, 16.f) - 1.f);
                    v.hold[0] = std::round(dL * q) / q; v.hold[1] = std::round(dR * q) / q;
                }
                const float sm = std::clamp(sSmooth, 0.f, 1.f);
                const double fc = std::exp(std::log(sr * 0.45) * (1.0 - sm) + std::log(std::max(200.0, (double)sRate * 0.35)) * sm);
                const float c = sm <= 0.f ? 0.f : (float)std::exp(-2.0 * kPi * fc / sr);
                for (int ch = 0; ch < 2; ++ch) { v.lp[ch][0] = v.hold[ch] + (v.lp[ch][0] - v.hold[ch]) * c; v.lp[ch][1] = v.lp[ch][0] + (v.lp[ch][1] - v.lp[ch][0]) * c; }
                oL = v.lp[0][1]; oR = v.lp[1][1]; break; }
            default: oL = dL; oR = dR; break;
        }
    }

    Params p;
    double sr = 48000.0, bpm = 120.0, qps = 120.0 / 60.0 / 48000.0, spq = 24000.0, stepQ = 0.25, stepSamples = 6000.0;
    int lat = 192; int64_t ringLen = 1; size_t mask = 0;
    std::vector<float> hl, hr;
    int64_t w = 0;
    std::array<Voice, kVoices> voices{};
    std::array<Ev, kMaxEv> evs{}; int nEv = 0, evI = 0, blockPos = 0;
    bool anchored = false, wasPlaying = false, running = false; double anchorQ = 0; int64_t anchorAbs = 0, lastStep = kNoStep;
    float envStep = 0.005f, gateStep = 0.01f, cs = 0.001f;
    float sMix = 1.f, sOut = 1.f, sBits = 6.f, sRate = 6000.f, sSmooth = 0.5f, sDens = 0.5f;
    std::vector<float> tl, tr; size_t tailMask = 0; int64_t tw = 0; float sTail = 0.f, dT = 0.f, tlp[2] = {0, 0}; bool tailDirty = false;
    Seq seq, box; std::atomic<bool> busy{false}; std::atomic<int> ver{0}; int seenVer = -1;
    int uiStep = -1; std::array<float, kNumTypes> uiAct{};
};
}  // namespace acr
