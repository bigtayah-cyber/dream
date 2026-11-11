// Moteur d'Orbe Beat : 8 pistes de percussions synthétisées, 7 kits, motifs et export MIDI. C++ pur (sans JUCE).
// Transposition fidèle de hit(), KITS, TPL, generate() et vary() du prototype beat.html.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>
#include "../../OneKnob/OneKnob.h"

namespace obeat {

constexpr float kPi = 3.14159265358979f;
constexpr int kLanes = 8, kMaxSteps = 32, kPats = 4, kKits = 7;
enum Lane { K = 0, S, C, H, O, R, P, B };

struct LaneInfo { const char* label; int midi; uint32_t rgb; };
inline const LaneInfo* lanes() {
    static const LaneInfo l[kLanes] = {
        {"Kick", 36, 0xFFFFC4AB}, {"Snare", 38, 0xFFECEBF2}, {"Clap", 39, 0xFFCFC6FF}, {"Hi-hat", 42, 0xFFA9ECFF},
        {"Hi-hat ouvert", 46, 0xFF7FD3E6}, {"Rim", 37, 0xFFFFD27A}, {"Percu", 47, 0xFF9BE6B4}, {"808", 33, 0xFFFF6B6B}};
    return l;
}

struct KitDef { const char* label; int bpm; float k[4], s[4], c[2], h[2], o[2], b[2]; float lp, sat; };
inline const KitDef* kits() {
    static const KitDef t[kKits] = {
        {"Trap", 142, {160, 45, .32f, 2}, {190, .55f, .18f, 1900}, {.22f, 1250}, {7600, .04f}, {7000, .28f}, {1.6f, .35f}, 0, 0},
        {"Drill", 142, {150, 50, .3f, 3}, {210, .7f, .16f, 2300}, {.2f, 1400}, {8000, .035f}, {7200, .22f}, {2.2f, .45f}, 0, 0},
        {"Boom bap", 90, {125, 48, .45f, 1.5f}, {180, .85f, .24f, 1600}, {.25f, 1100}, {6500, .05f}, {6200, .3f}, {.9f, .1f}, 7000, 0},
        {"Lo-fi", 82, {110, 46, .4f, 1.2f}, {170, .6f, .2f, 1400}, {.24f, 1000}, {5500, .05f}, {5200, .25f}, {.9f, .05f}, 3800, 0},
        {"Jersey club", 140, {180, 50, .22f, 2.5f}, {220, .6f, .14f, 2200}, {.18f, 1500}, {8200, .03f}, {7400, .2f}, {.8f, .3f}, 0, 0},
        {"House", 124, {140, 48, .3f, 1.8f}, {200, .5f, .17f, 2000}, {.2f, 1300}, {9000, .035f}, {8500, .22f}, {.5f, .1f}, 0, 0},
        {"Rage", 155, {190, 44, .28f, 5}, {230, .7f, .15f, 2500}, {.18f, 1600}, {8500, .03f}, {7800, .2f}, {1.4f, .85f}, 0, 2.5f}};
    return t;
}

// ---- cases : bits 0-6 vélocité (0 = vide), 7-8 roll−1, 9-11 degré (piste 808) ----
using Cell = uint16_t;
inline Cell mk(float v, int r = 1, int n = 0) {
    const int vel = std::max(1, std::min(127, (int)std::lround(v * 127.f)));
    return (Cell)(vel | ((std::max(1, std::min(4, r)) - 1) << 7) | ((n & 7) << 9));
}
inline bool filled(Cell c) { return (c & 127) != 0; }
inline float velOf(Cell c) { return (float)(c & 127) / 127.f; }
inline int rollOf(Cell c) { return ((c >> 7) & 3) + 1; }
inline int degOf(Cell c) { return (c >> 9) & 7; }
inline Cell withRoll(Cell c, int r) { return (Cell)((c & ~(3 << 7)) | ((std::max(1, std::min(4, r)) - 1) << 7)); }
inline Cell withVel(Cell c, float v) { return (Cell)((c & ~127) | std::max(1, std::min(127, (int)std::lround(v * 127.f)))); }
inline Cell withDeg(Cell c, int n) { return (Cell)((c & ~(7 << 9)) | ((n & 7) << 9)); }

struct Pattern { Cell c[kLanes][kMaxSteps] = {}; };

struct Rng {
    uint32_t s = 2463534242u;
    explicit Rng(uint32_t seed = 1) : s(seed ? seed : 1) {}
    uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float f() { return (float)(next() >> 8) / 16777216.f; }
    int pick(int n) { return (int)(next() % (uint32_t)std::max(1, n)); }
};

// Degré d'échelle (0..7) -> note MIDI de la piste 808. key 0..11 (C..B), mode 0 mineur / 1 majeur.
inline int degNote(int key, int mode, int i) {
    static const int mn[7] = {0, 2, 3, 5, 7, 8, 10}, mj[7] = {0, 2, 4, 5, 7, 9, 11};
    const int o = i / 7, d = i % 7;
    return 24 + 12 + key - (key > 6 ? 12 : 0) + (mode ? mj : mn)[d] + 12 * o;
}

// ---- motifs ----
struct Tpl { std::vector<std::vector<int>> k, s, c; int hmode; std::vector<std::vector<int>> h, o; int b; };  // hmode : 0 liste, 1 h8, 2 h8 + rolls, 3 h8 swing, 4 h16 ; b : 0 sur les kicks, 1 drill, 2 contretemps
inline const Tpl& tpl(int kit) {
    static const Tpl t[kKits] = {
        {{{0, 10}, {0, 7, 10}, {0, 10, 13}}, {{8}}, {{8}}, 2, {}, {{14}}, 0},
        {{{0, 14}, {0, 3, 14}, {0, 11}}, {{8, 12}, {8, 13}}, {{8}}, 0, {{0, 3, 6, 8, 11, 14}, {0, 3, 6, 8, 10, 11, 14}}, {{}}, 1},
        {{{0, 10}, {0, 7, 10}, {0, 3, 10}}, {{4, 12}}, {{}}, 1, {}, {{14}}, 0},
        {{{0, 10}, {0, 7, 10}}, {{4, 12}}, {{}}, 3, {}, {{}}, 0},
        {{{0, 6, 10, 12, 14}, {0, 3, 6, 10, 12, 14}}, {{}}, {{4, 12}}, 1, {}, {{2}}, 0},
        {{{0, 4, 8, 12}}, {{}}, {{4, 12}}, 0, {{2, 6, 10, 14}}, {{2, 6, 10, 14}}, 2},
        {{{0, 10}, {0, 7, 10, 14}}, {{8}}, {{8}}, 4, {}, {{}}, 0}};
    return t[std::max(0, std::min(kKits - 1, kit))];
}

inline void generate(Pattern& out, int kit, int steps, float intensity, Rng& rng) {
    const Tpl& T = tpl(kit);
    out = Pattern{};
    auto pick = [&](const std::vector<std::vector<int>>& a) -> const std::vector<int>& { return a[(size_t)rng.pick((int)a.size())]; };
    auto set = [&](int li, int s, Cell c) { if (s < kMaxSteps) out.c[li][s] = c; };
    for (int b = 0; b < std::max(1, steps / 16); ++b) {
        const int o = b * 16;
        for (int s : pick(T.k)) set(K, o + s, mk(s == 0 ? 1.f : .85f));
        for (int s : pick(T.s)) set(S, o + s, mk(1.f));
        for (int s : pick(T.c)) set(C, o + s, mk(.9f));
        if (T.hmode >= 1 && T.hmode <= 3) for (int s = 0; s < 16; s += 2) set(H, o + s, mk(s % 4 ? .7f : 1.f));
        else if (T.hmode == 4) for (int s = 0; s < 16; ++s) set(H, o + s, mk(s % 2 ? .6f : .95f));
        else for (int s : pick(T.h)) set(H, o + s, mk(1.f));
        if (T.hmode == 2) {
            static const int rr[4][2] = {{6, 2}, {11, 3}, {14, 4}, {15, 2}};
            for (auto& q : rr) if (rng.f() < .35f + intensity * .4f) set(H, o + q[0], mk(.8f, q[1]));
        }
        for (int s : pick(T.o)) if (rng.f() < .8f) { set(O, o + s, mk(.8f)); out.c[H][o + s] = 0; }
        if (rng.f() < .5f) set(R, o + 3 + 4 * rng.pick(4), mk(.7f));
        if (rng.f() < .4f) { static const int pp[3] = {5, 13, 9}; set(P, o + pp[rng.pick(3)], mk(.7f)); }
        std::vector<int> bpos;
        if (T.b == 1) { static const int d[5] = {0, 3, 6, 10, 14}; for (int i = 0; i < 5; ++i) if (!i || rng.f() < .6f) bpos.push_back(d[i]); }
        else if (T.b == 2) bpos = {2, 6, 10, 14};
        else for (int s = 0; s < 16; ++s) if (filled(out.c[K][o + s])) bpos.push_back(s);
        static const int deg[8] = {0, 0, 0, 3, 4, 5, 6, 7};
        for (size_t i = 0; i < bpos.size(); ++i) set(B, o + bpos[i], mk(1.f, 1, i == 0 && b == 0 ? 0 : deg[rng.pick(8)]));
    }
}

// Variation : roll sur des charleys, kicks retirés, relance de caisse claire en fin de mesure.
inline void vary(const Pattern& src, Pattern& dst, Rng& rng) {
    dst = src;
    for (int s = 0; s < kMaxSteps; ++s) {
        if (filled(dst.c[H][s]) && rng.f() < .3f) dst.c[H][s] = withRoll(dst.c[H][s], 2 + rng.pick(3));
        if (filled(dst.c[K][s]) && s % 16 && rng.f() < .25f) dst.c[K][s] = 0;
    }
    for (int s : {15, 31}) dst.c[S][s] = mk(.7f, 2);
    if (rng.f() < .5f) for (int s = 12; s <= 15; ++s) dst.c[S][s] = mk(.5f + (float)(s - 12) * .15f);
}

// Parcourt les coups dont l'instant (en noires) tombe dans [ppq0, ppq1). cellAt(pasGlobal, piste) -> Cell ; emit(ppq, piste, cell, k).
template <class CellAt, class Emit>
inline void scan(double ppq0, double ppq1, float swing, CellAt&& cellAt, Emit&& emit) {
    if (ppq1 <= ppq0) return;
    const long long s0 = (long long)std::floor(ppq0 * 4.0) - 1, s1 = (long long)std::floor(ppq1 * 4.0) + 1;
    for (long long s = std::max(0LL, s0); s <= s1; ++s)
        for (int li = 0; li < kLanes; ++li) {
            const Cell c = cellAt(s, li);
            if (!filled(c)) continue;
            const int rep = rollOf(c);
            for (int k = 0; k < rep; ++k) {
                const double t = (double)s * 0.25 + ((s & 1) ? (double)swing * 0.25 : 0.0) + (double)k * 0.25 / rep;
                if (t >= ppq0 && t < ppq1) emit(t, li, c, k);
            }
        }
}

// ---- export MIDI (format 1, 480 PPQ) : piste de tempo, batterie (canal 10, notes GM) et 808 (canal 1) ----
inline std::vector<uint8_t> midiFile(const std::vector<const Pattern*>& list, int steps, float swing, float bpm, int key, int mode) {
    struct Ev { uint32_t tick; bool on; int n, v; };
    std::vector<Ev> drums, bass;
    for (size_t bi = 0; bi < list.size(); ++bi)
        for (int s = 0; s < steps; ++s)
            for (int li = 0; li < kLanes; ++li) {
                const Cell c = list[bi]->c[li][s];
                if (!filled(c)) continue;
                const int rep = rollOf(c);
                for (int k = 0; k < rep; ++k) {
                    const double t = ((double)bi * steps + s + (double)k / rep + ((s & 1) ? swing : 0.f)) / 4.0;
                    const uint32_t t0 = (uint32_t)std::lround(t * 480.0), len = li == B ? 240u : 58u;
                    const int vel = std::max(1, std::min(127, (int)std::lround(velOf(c) * (k ? .75f : 1.f) * 127.f)));
                    const int n = li == B ? degNote(key, mode, degOf(c)) : lanes()[li].midi;
                    (li == B ? bass : drums).push_back({t0, true, n, vel});
                    (li == B ? bass : drums).push_back({t0 + len, false, n, 0});
                }
            }
    auto w32 = [](std::vector<uint8_t>& v, uint32_t x) { for (int s = 24; s >= 0; s -= 8) v.push_back((uint8_t)(x >> s)); };
    auto w16 = [](std::vector<uint8_t>& v, uint16_t x) { v.push_back((uint8_t)(x >> 8)); v.push_back((uint8_t)x); };
    auto vlq = [](std::vector<uint8_t>& v, uint32_t x) {
        uint8_t b[5]; int n = 0; b[n++] = x & 0x7f;
        while ((x >>= 7)) b[n++] = (uint8_t)((x & 0x7f) | 0x80);
        while (n--) v.push_back(b[n]);
    };
    auto name = [&](std::vector<uint8_t>& t, const char* s) { vlq(t, 0); t.push_back(0xFF); t.push_back(0x03); t.push_back((uint8_t)std::strlen(s)); for (const char* p = s; *p; ++p) t.push_back((uint8_t)*p); };
    std::vector<std::vector<uint8_t>> tracks;
    {
        std::vector<uint8_t> t; name(t, "Orbe Beat");
        const uint32_t us = (uint32_t)(60000000.0 / std::max(20.f, bpm));
        vlq(t, 0); t.insert(t.end(), {0xFF, 0x51, 0x03, (uint8_t)(us >> 16), (uint8_t)(us >> 8), (uint8_t)us});
        vlq(t, 0); t.insert(t.end(), {0xFF, 0x58, 0x04, 4, 2, 24, 8});
        vlq(t, (uint32_t)(list.size() * (size_t)steps * 120)); t.insert(t.end(), {0xFF, 0x2F, 0x00});
        tracks.push_back(t);
    }
    auto add = [&](std::vector<Ev> ev, int ch, const char* nm) {
        std::stable_sort(ev.begin(), ev.end(), [](const Ev& a, const Ev& b) { return a.tick != b.tick ? a.tick < b.tick : (!a.on && b.on); });
        std::vector<uint8_t> t; name(t, nm);
        uint32_t last = 0;
        for (auto& e : ev) { vlq(t, e.tick - last); last = e.tick; t.push_back((uint8_t)((e.on ? 0x90 : 0x80) | (ch - 1))); t.push_back((uint8_t)e.n); t.push_back((uint8_t)e.v); }
        vlq(t, 0); t.insert(t.end(), {0xFF, 0x2F, 0x00});
        tracks.push_back(t);
    };
    add(drums, 10, "Batterie");
    add(bass, 1, "808");
    std::vector<uint8_t> out{'M', 'T', 'h', 'd'};
    w32(out, 6); w16(out, 1); w16(out, (uint16_t)tracks.size()); w16(out, 480);
    for (auto& t : tracks) { out.insert(out.end(), {'M', 'T', 'r', 'k'}); w32(out, (uint32_t)t.size()); out.insert(out.end(), t.begin(), t.end()); }
    return out;
}

// ---- synthèse ----
inline float mtof(float n) { return 440.f * std::exp2((n - 69.f) / 12.f); }
inline void bandpass(onek::Biquad& b, float sr, float f, float q) {
    const float w = 2.f * kPi * f / sr, a = std::sin(w) / (2.f * q);
    b.set(a, 0.f, -a, 1.f + a, -2.f * std::cos(w), 1.f - a);
}
inline float softClip(float x) {
    const float a = std::fabs(x);
    if (a <= 0.9f) return x;
    const float y = 0.9f + 0.1f * std::tanh((a - 0.9f) * 10.f);
    return x < 0 ? -y : y;
}

class Engine {
public:
    static constexpr int kPerLane = 6;
    void prepare(double sampleRate) {
        sr = (float)sampleRate;
        reset();
        tail = std::exp(-1.f / (0.002f * sr));
        chokeC = std::exp(-1.f / (0.008f * sr));
        setKit(kit);
    }
    void reset() { for (auto& x : vs) x = Voice{}; rng = 0x2468ACEu; lp.reset(); for (auto& f : lpl) f.reset(); }
    void setKit(int k) {
        kit = std::max(0, std::min(kKits - 1, k));
        const auto& D = kits()[kit];
        lp.reset();
        if (D.lp > 0.f) lp.lowpass(sr, std::min(D.lp, 0.45f * sr), 0.8f);
        for (auto& f : lpl) { f.reset(); if (D.lp > 0.f) f.lowpass(sr, std::min(D.lp, 0.45f * sr), 0.8f); }
    }
    void setLevel(float l) { level = l; }

    // v : vélocité 0..1 (volume de piste inclus). note : hauteur de la piste 808.
    void trigger(int lane, float v, int note = 33) {
        if (v < 0.002f) return;
        v = std::min(1.f, v);
        if (lane == H || lane == O) chokeLane(O);
        if (lane == B) chokeLane(B);
        Voice* slot = &vs[lane * kPerLane];
        for (int i = 0; i < kPerLane; ++i) { auto& c = vs[lane * kPerLane + i]; if (!c.on) { slot = &c; break; } if (c.age > slot->age) slot = &c; }
        init(*slot, lane, v, note);
    }
    // Fin de note de l'808 (note < 0 : toutes).
    void release808(int note = -1) { for (int i = 0; i < kPerLane; ++i) { auto& c = vs[B * kPerLane + i]; if (c.on && !c.rel && (note < 0 || c.note == note)) { c.rel = true; c.relc = std::exp(-1.f / (0.012f * sr)); } } }
    bool active() const { for (auto& x : vs) if (x.on) return true; return false; }

    // Mix mono. Si `stems` n'est pas nul, stems[k] (ou nul pour une piste ignorée) reçoit en plus la piste k seule, avec la même chaîne de bus
    // (saturation du kit, filtre, niveau, écrêtage) : le mix n'est pas la somme exacte des pistes (la saturation de bus est non linéaire).
    void render(float* out, int n, float* const* stems = nullptr) {
        const auto& D = kits()[kit];
        const float sat = D.sat, ts = sat > 0.f ? std::tanh(sat) : 1.f;
        for (int i = 0; i < n; ++i) {
            float m = 0.f, ln[kLanes] = {};
            for (auto& x : vs) if (x.on) { const float y = sample(x); m += y; ln[x.lane] += y; }
            if (sat > 0.f) m = std::tanh(std::max(-1.f, std::min(1.f, m)) * sat) / ts;
            if (D.lp > 0.f) m = lp.process(m);
            out[i] = softClip(m * 0.9f * level);
            if (stems)
                for (int k = 0; k < kLanes; ++k) if (stems[k]) {
                    float y = ln[k];
                    if (sat > 0.f) y = std::tanh(std::max(-1.f, std::min(1.f, y)) * sat) / ts;
                    if (D.lp > 0.f) y = lpl[k].process(y);
                    stems[k][i] = softClip(y * 0.9f * level);
                }
        }
    }

private:
    struct Voice {
        bool on = false, rel = false;
        int lane = 0, note = 0;
        uint32_t age = 0, dur = 0, clkN = 0, n1 = 0, n2 = 0;
        float v = 0, g = 0, gm = 1, f = 0, fm = 1, ph = 0, ph2 = 0, f2 = 0, f2m = 1, g2 = 0, g2m = 1, nz = 0, drive = 1, tsat = 1, relc = 0.99f;
        float cg = 0, cgm = 1, cf = 0, cfm = 1, cph = 0, att = 0, dc = 0.99f;
        onek::Biquad bq;
    };
    Voice vs[kLanes * kPerLane];
    float sr = 48000.f, tail = 0.9f, chokeC = 0.99f, level = 1.f;
    int kit = 0;
    uint32_t rng = 1;
    onek::Biquad lp, lpl[kLanes];

    float noise() { rng = rng * 1664525u + 1013904223u; return (float)(int32_t)rng / 2147483648.f; }
    uint32_t smp(float t) const { return std::max(1u, (uint32_t)(t * sr)); }
    float ratePow(float a, float b, float t) const { return std::pow(b / a, 1.f / (t * sr)); }
    static float tri(float p) { float q = p + 0.25f; q -= std::floor(q); return 1.f - 4.f * std::fabs(q - 0.5f); }
    static float tanhN(float x, float k) { return std::tanh(std::max(-1.f, std::min(1.f, x)) * k) / std::tanh(k); }
    void chokeLane(int lane) { for (int i = 0; i < kPerLane; ++i) { auto& c = vs[lane * kPerLane + i]; if (c.on && !c.rel) { c.rel = true; c.relc = chokeC; } } }

    void init(Voice& x, int lane, float v, int note) {
        const auto& D = kits()[kit];
        x = Voice{};
        x.on = true; x.lane = lane; x.v = v; x.note = note;
        const float vv = std::max(v, 0.0015f);
        auto expTo = [&](float from, float to, float t) { x.g = from; x.gm = ratePow(from, to, t); x.dur = smp(t); };
        switch (lane) {
        case K:
            x.f = D.k[0]; x.fm = ratePow(D.k[0], D.k[1], 0.1f); x.n1 = smp(0.1f); x.drive = D.k[3]; expTo(vv, 0.001f, D.k[2]);
            x.cg = std::max(vv * 0.25f, 0.0015f); x.cgm = ratePow(x.cg, 0.001f, 0.015f); x.cf = 2500.f; x.cfm = ratePow(2500.f, 300.f, 0.01f); x.n2 = smp(0.01f); x.clkN = smp(0.015f);
            break;
        case S:
            x.nz = D.s[1]; x.f = D.s[0] * 1.6f; x.fm = ratePow(1.6f, 1.f, 0.03f); x.n1 = smp(0.03f);
            x.g2 = 0.6f; x.g2m = ratePow(0.6f, 0.001f, D.s[2] * 0.6f); x.n2 = smp(D.s[2] * 0.6f); expTo(vv, 0.001f, D.s[2]);
            bandpass(x.bq, sr, D.s[3], 0.7f); x.f2 = D.s[0];
            break;
        case C: bandpass(x.bq, sr, D.c[1], 1.1f); x.dur = smp(D.c[0]); x.g = vv; break;
        case H: case O: { const float* q = lane == H ? D.h : D.o; x.bq.highpass(sr, q[0], 1.f); expTo(vv * 0.55f, 0.001f, q[1]); break; }
        case R: bandpass(x.bq, sr, 3200.f, 1.f); expTo(vv * 0.5f, 0.001f, 0.045f); break;
        case P: x.f = 520.f; x.fm = ratePow(520.f, 330.f, 0.08f); x.n1 = smp(0.08f); expTo(vv * 0.7f, 0.001f, 0.16f); break;
        case B: {
            const float f = mtof((float)note);
            x.f = f * 3.f; x.fm = ratePow(3.f, 1.f, 0.05f); x.n1 = smp(0.05f);
            x.drive = 1.f + D.b[1] * 6.f; x.att = (float)smp(0.004f); x.n2 = smp(0.03f);
            x.dc = std::exp(-1.f / (std::max(0.01f, D.b[0] / 3.f) * sr)); x.g = 0.f; x.dur = 0;
            break; }
        }
    }
    float clapEnv(float t, float v, float d) const {
        auto ex = [&](float a, float b, float t0, float t1) { return a * std::pow(b / a, (t - t0) / (t1 - t0)); };
        if (t < .009f) return ex(v, v * .15f, 0.f, .009f);
        if (t < .011f) return v * .15f;
        if (t < .020f) return ex(v, v * .15f, .011f, .020f);
        if (t < .022f) return v * .15f;
        if (t < .030f) return v;
        if (t < .031f) return ex(v * .9f, v * .15f, .030f, .031f);
        return t < d ? ex(v * .15f, 0.001f, .031f, std::max(d, .032f)) : 0.f;
    }

    float sample(Voice& x) {
        const float t = (float)x.age / sr;
        float o = 0.f;
        switch (x.lane) {
        case K: {
            o = tanhN(std::sin(2.f * kPi * x.ph), x.drive) * x.g;
            x.ph += x.f / sr; x.ph -= std::floor(x.ph);
            if (x.age < x.n1) x.f *= x.fm;
            if (x.age < x.clkN) { o += x.cg * std::sin(2.f * kPi * x.cph); x.cph += x.cf / sr; x.cph -= std::floor(x.cph); if (x.age < x.n2) x.cf *= x.cfm; x.cg *= x.cgm; }
            break; }
        case S: {
            o = (x.bq.process(noise()) * x.nz + tri(x.ph) * x.g2) * x.g;
            x.ph += x.f / sr; x.ph -= std::floor(x.ph);
            if (x.age < x.n1) x.f *= x.fm;
            x.g2 *= x.age < x.n2 ? x.g2m : tail;
            break; }
        case C: {
            const float e = clapEnv(t, x.v, (float)x.dur / sr);
            o = x.bq.process(noise()) * e;
            if (x.age >= x.dur) x.on = false;
            ++x.age;
            return o;
        }
        case H: case O: o = x.bq.process(noise()) * x.g; break;
        case R: o = (x.bq.process(noise()) + tri(x.ph)) * x.g; x.ph += 1750.f / sr; x.ph -= std::floor(x.ph); break;
        case P: o = std::sin(2.f * kPi * x.ph) * x.g; x.ph += x.f / sr; x.ph -= std::floor(x.ph); if (x.age < x.n1) x.f *= x.fm; break;
        case B: {
            const float y = tanhN(std::sin(2.f * kPi * x.ph) * 0.9f * 0.9f, x.drive);
            x.ph += x.f / sr; x.ph -= std::floor(x.ph);
            if (x.age < x.n1) x.f *= x.fm;
            const float pk = x.v * 0.9f;
            if (x.rel) x.g *= x.relc;
            else if (x.age < (uint32_t)x.att) x.g = pk * (float)(x.age + 1) / x.att;
            else if (x.age < x.n2) x.g = pk;
            else x.g = 1e-4f + (x.g - 1e-4f) * x.dc;
            o = y * x.g;
            ++x.age;
            if ((x.rel || x.age > x.n2) && x.g < 2e-4f) x.on = false;
            return o;
        }
        }
        if (x.lane != B) {
            if (x.rel) x.g *= x.relc;
            else x.g *= x.age < x.dur ? x.gm : tail;
            ++x.age;
            const bool noteDone = x.age >= x.dur && x.g < 1e-4f;
            if (noteDone || (x.rel && x.g < 1e-4f)) x.on = false;
        }
        return o;
    }
};

}  // namespace obeat
