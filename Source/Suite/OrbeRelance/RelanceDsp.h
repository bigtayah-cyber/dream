// Orbe Relance : moteur sans JUCE (C++17 pur, aucune allocation dans process, sans code GPL).
// Le plugin écoute la voix principale et lui RÉPOND avec elle-même : à chaque fin de phrase il capture les 200 à 600 dernières ms
// et les rejoue une ou deux croches plus tard, transformées (écho de mot, octave basse, téléphone, inversé, bégaiement, harmonie).
//  - Segmentation : enveloppe RMS de 30 ms + voisement YIN (sous-échantillonné vers ~24 kHz, fenêtre ~17 ms, saut ~10 ms).
//  - Fin de phrase : phrase active (au-dessus du seuil pendant plus de 250 ms cumulées), puis niveau sous le seuil (absolu, ou
//    24 dB sous la crête de la phrase) pendant plus de 150 ms. La fin retenue est l'instant où le niveau est retombé.
//  - Décision : probabilité DENSITÉ par phrase ; caractère, retard, panoramique, répétitions tirés au hasard d'une graine.
//    Tirages reproductibles : un générateur par phrase = hachage(graine, rang de la phrase). Graine « Verrouillée » : le rang repart
//    de zéro à chaque lancement de la lecture (même prise -> mêmes relances). « Libre » : il ne repart jamais de zéro.
//  - Calage : sur la grille de croches de l'hôte quand la position est connue, sinon retard en croches au tempo (120 par défaut).
//  - Briques : dsp::Splicer (décaleur à raccords synchrones à la période, Source/Dsp/Splicer.h), dsp::sat::Saturator (grain du
//    téléphone, Source/Dsp/Saturation.h).
// Voie sèche : intacte, sans retard (latence déclarée 0). Les relances sont par nature en retard sur la voix.
#pragma once
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "../../Dsp/Splicer.h"
#include "../../Dsp/Saturation.h"

namespace rel {

enum Type { Echo = 0, OctLow, Phone, Reverse, Stutter, Harmony, kNumTypes };
inline const char* typeName(int t) {
    static const char* n[kNumTypes] = {"\xC3\x89" "cho", "Octave basse", "T\xC3\xA9l\xC3\xA9phone", "Invers\xC3\xA9", "B\xC3\xA9gaiement", "Harmonie"};
    return t >= 0 && t < kNumTypes ? n[t] : "";
}

struct Params {
    float density = 0.35f;   // 0..1 : probabilité de relancer une phrase (faible par défaut : pas d'effet gadget)
    float audace = 0.4f;     // 0..1 : transformations plus marquées, caractères plus « fous » dans le mélange
    int character = 0;       // 0 = Mélange, 1..6 = type imposé (Type + 1)
    int delayMode = 2;       // 0 = 1 croche, 1 = 2 croches, 2 = 1 ou 2 (au hasard)
    float lenMs = 400.f;     // 200..600 : longueur capturée en fin de phrase
    float threshDb = -40.f;  // -60..-20 : seuil de présence de la voix
    float width = 0.6f;      // 0..1 : écart stéréo des relances
    float tone = 0.5f;       // 0..1 : couleur (passe-bas des relances, sombre -> clair)
    float levelDb = -3.f;    // -24..+6 : niveau des relances
    int seed = 1;            // 1..999
    int lock = 1;            // 0 = Libre, 1 = Verrouillée
    int outMode = 0;         // 0 = voix + relances, 1 = relances seules (bus parallèle)
};
inline void sanitize(Params& p) {
    auto cl = [](float v, float lo, float hi, float d) { return std::isfinite(v) ? std::min(hi, std::max(lo, v)) : d; };
    const Params d;
    p.density = cl(p.density, 0.f, 1.f, d.density); p.audace = cl(p.audace, 0.f, 1.f, d.audace);
    p.character = std::min((int)kNumTypes, std::max(0, p.character)); p.delayMode = std::min(2, std::max(0, p.delayMode));
    p.lenMs = cl(p.lenMs, 200.f, 600.f, d.lenMs); p.threshDb = cl(p.threshDb, -60.f, -20.f, d.threshDb);
    p.width = cl(p.width, 0.f, 1.f, d.width); p.tone = cl(p.tone, 0.f, 1.f, d.tone); p.levelDb = cl(p.levelDb, -24.f, 6.f, d.levelDb);
    p.seed = std::min(999, std::max(1, p.seed)); p.lock = p.lock ? 1 : 0; p.outMode = p.outMode ? 1 : 0;
}

struct Host { bool playing = false, hasBpm = false, hasPos = false; double bpm = 120.0, ppq = 0.0; };

// ------------------------------------------------------------ état versionné (hors apvts) : « REL1;prog=3 »
struct Meta { int program = 0; };
inline std::string toString(const Meta& m) { return "REL1;prog=" + std::to_string(m.program); }
inline bool fromString(const std::string& s, Meta& m) {
    if (s.rfind("REL1;", 0) != 0) return false;
    const auto k = s.find("prog=");
    if (k == std::string::npos) return false;
    const int v = std::atoi(s.c_str() + k + 5);
    m.program = std::min(15, std::max(0, v));
    return true;
}

// ------------------------------------------------------------ télémétrie pour l'interface (lue sans verrou)
struct Telemetry {
    static constexpr int N = 256;        // trames de ~20 ms : ~5 s d'historique
    static constexpr int kEv = 24;
    std::atomic<int> wr{0};
    std::atomic<float> inDb[N], wetDb[N];
    std::atomic<char> voiced[N];
    // événements : fin de phrase (type -1 = phrase sans relance), relance programmée (type >= 0)
    std::atomic<int> evWr{0};
    std::atomic<int> evFrame[kEv], evType[kEv], evStart[kEv], evDur[kEv];   // evFrame/evStart : compteur absolu de trames
    std::atomic<int> frameAbs{0}, phrases{0}, relances{0}, lastType{-1}, active{0};
    std::atomic<float> hopSec{0.02f};
    Telemetry() {
        for (int i = 0; i < N; ++i) { inDb[i].store(-90.f); wetDb[i].store(-90.f); voiced[i].store(0); }
        for (int i = 0; i < kEv; ++i) { evFrame[i].store(-1000000); evType[i].store(-1); evStart[i].store(0); evDur[i].store(0); }
    }
};

class Engine {
public:
    static constexpr int kVoices = 4;

    void set(const Params& p) { P = p; sanitize(P); }
    const Params& params() const { return P; }
    int latency() const { return 0; }
    Telemetry& tel() { return T; }
    const Telemetry& tel() const { return T; }

    void prepare(double sampleRate, int /*maxBlock*/) {
        sr = sampleRate > 1000.0 ? sampleRate : 48000.0;
        D = std::max(1, (int)std::lround(sr / 24000.0));
        srd = sr / D;
        yW = (int)std::lround(srd * 0.017); tauMin = (int)(srd / 1000.0); tauMax = (int)(srd / 65.0);
        yHop = (int)std::lround(srd * 0.010);
        int n = 1; while (n < yW + tauMax + 4) n <<= 1;
        yRing.assign((size_t)n, 0.f); yMask = n - 1; yLin.assign((size_t)(yW + tauMax + 2), 0.f); yD.assign((size_t)(tauMax + 2), 0.f);
        n = 1; while (n < (int)(2.0 * sr)) n <<= 1;
        cap.assign((size_t)n, 0.f); cMask = n - 1;
        maxSeg = (int)std::ceil(0.6 * sr) + 8;
        const int lz = (int)(0.5 * sr / 70.0) + 48;
        for (auto& v : vo) { v.buf.assign((size_t)maxSeg, 0.f); v.sp.init(1, lz); v.sp.snapDry = false; v.on = false; }
        sat.prepare(sr, 2);
        dsp::sat::Params sp; sp.driveDb = 14.f; sp.bias = 0.15f; sp.mix = 1.f; sp.outDb = -1.f; sat.setParams(sp);
        envC = 1.f - std::exp(-1.f / (0.030f * (float)sr));
        hop = std::max(1, (int)std::lround(sr * 0.02)); T.hopSec.store((float)hop / (float)sr);
        reset();
    }
    void reset() {
        std::fill(yRing.begin(), yRing.end(), 0.f); std::fill(cap.begin(), cap.end(), 0.f);
        for (auto& v : vo) { v.on = false; v.wait = 0; }
        yw = 0; yAcc = 0.f; yAccN = 0; yHopC = 0; voicedNow = false; periodNow = 0.f;
        cw = 0; ms = 0.f; inPhrase = false; aboveLen = 0; quiet = 0; peakDb = -120.f; perSum = 0.0; perN = 0;
        phraseIdx = 0; nowAbs = 0; wasPlaying = false;
        for (auto* f : {&tLp1L, &tLp2L, &tLp1R, &tLp2R, &tHpL, &tHpR, &pHp1, &pHp2, &pLp1, &pLp2}) *f = 0.f;
        gLvl = std::pow(10.f, P.levelDb / 20.f); gDry = P.outMode ? 0.f : 1.f;
        hopC = 0; fIn = fWet = -120.f; fVoiced = false;
        sat.reset();
    }

    void beginBlock(const Host& h) {
        bpm = (h.hasBpm && std::isfinite(h.bpm) && h.bpm >= 20.0 && h.bpm <= 400.0) ? h.bpm : 120.0;
        gridOk = h.playing && h.hasPos && std::isfinite(h.ppq);
        ppq0 = gridOk ? h.ppq : 0.0;
        if (h.playing && !wasPlaying && P.lock) phraseIdx = 0;   // graine verrouillée : même prise -> mêmes relances
        wasPlaying = h.playing;
        blockPos = 0;
    }

    // inR nul : entrée mono. Sorties stéréo (peuvent pointer sur les entrées).
    void process(const float* inL, const float* inR, float* outL, float* outR, int n) {
        const float lvlT = std::pow(10.f, P.levelDb / 20.f), dryT = P.outMode ? 0.f : 1.f, gs = 1.f - std::exp(-1.f / (0.02f * (float)sr));
        const float fc = 2500.f * std::exp2(P.tone * 2.7f);
        const float lpC = 1.f - std::exp(-2.f * dsp::kPiF * std::min(fc, 0.45f * (float)sr) / (float)sr), hpC = 1.f - std::exp(-2.f * dsp::kPiF * 130.f / (float)sr);
        const float pHpC = 1.f - std::exp(-2.f * dsp::kPiF * 450.f / (float)sr), pLpC = 1.f - std::exp(-2.f * dsp::kPiF * 2800.f / (float)sr);
        for (int i = 0; i < n; ++i) {
            float xl = inL[i], xr = inR ? inR[i] : xl;
            if (!std::isfinite(xl)) xl = 0.f;
            if (!std::isfinite(xr)) xr = 0.f;
            const float m = 0.5f * (xl + xr);
            analyse(m);
            float wl = 0.f, wr = 0.f, ph = 0.f;
            for (auto& v : vo) if (v.on) render(v, wl, wr, ph);
            // téléphone : bande 450 Hz - 2,8 kHz, grain de saturation
            pHp1 += (ph - pHp1) * pHpC; float b = ph - pHp1; pHp2 += (b - pHp2) * pHpC; b -= pHp2;
            pLp1 += (b - pLp1) * pLpC; pLp2 += (pLp1 - pLp2) * pLpC;
            const float phone = sat.processSample(pLp2 * 3.f);
            wl += phone * phPanL; wr += phone * phPanR;
            // couleur des relances : passe-bas doux (2 pôles) et grave allégé
            tLp1L += (wl - tLp1L) * lpC; tLp2L += (tLp1L - tLp2L) * lpC; tHpL += (tLp2L - tHpL) * hpC;
            tLp1R += (wr - tLp1R) * lpC; tLp2R += (tLp1R - tLp2R) * lpC; tHpR += (tLp2R - tHpR) * hpC;
            gLvl += (lvlT - gLvl) * gs; gDry += (dryT - gDry) * gs;
            float ol = (tLp2L - tHpL) * gLvl, orr = (tLp2R - tHpR) * gLvl;
            if (!std::isfinite(ol + orr)) { ol = orr = 0.f; clearWet(); }
            ol = std::max(-2.f, std::min(2.f, ol)); orr = std::max(-2.f, std::min(2.f, orr));   // garde : relances bornées à +6 dBFS
            telemetry(m, 0.5f * (ol + orr));
            outL[i] = xl * gDry + ol; outR[i] = xr * gDry + orr;
            ++nowAbs; ++blockPos;
        }
    }

    // ---- accès pour les tests
    int phrasesSeen() const { return T.phrases.load(); }
    int relancesFired() const { return T.relances.load(); }
    bool voicedState() const { return voicedNow; }
    float period() const { return periodNow; }

private:
    struct Voice {
        bool on = false;
        int wait = 0, type = 0, len = 0, t = 0, rep = 0, reps = 1, gap = 0, slice = 0, stN = 0, tail = 0;
        float g = 1.f, decay = 0.6f, ratio = 1.f, pl = 0.7f, pr = 0.7f;
        dsp::Splicer sp;
        std::vector<float> buf;
    };

    static uint32_t hash(uint32_t x) { x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16; return x ? x : 1u; }
    float rnd() { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return (float)(rng >> 8) * (1.f / 16777216.f); }

    void analyse(float m) {
        // enveloppe RMS 30 ms
        ms += (m * m - ms) * envC;
        const float db = 10.f * std::log10(ms + 1e-12f);
        // YIN sous-échantillonné
        yAcc += m; if (++yAccN >= D) { yRing[(size_t)yw] = yAcc / (float)D; yw = (yw + 1) & yMask; yAcc = 0.f; yAccN = 0; if (++yHopC >= yHop) { yHopC = 0; yin(db); } }
        // mémoire de capture
        cap[(size_t)cw] = m; cw = (cw + 1) & cMask;
        // fin de phrase
        const bool above = db > P.threshDb && db > peakDb - 24.f;
        if (above) {
            if (!inPhrase) { inPhrase = true; aboveLen = 0; peakDb = db; perSum = 0.0; perN = 0; }
            quiet = 0; ++aboveLen;
            peakDb = std::max(peakDb, db);
            if (voicedNow && periodNow > 0.f) { perSum += periodNow; ++perN; }
        } else if (inPhrase) {
            if (++quiet >= (int)(0.15 * sr)) {
                if (aboveLen >= (int)(0.25 * sr)) phraseEnd();
                inPhrase = false; aboveLen = 0; quiet = 0;
            }
        }
        if (!inPhrase) peakDb = std::max(-120.f, peakDb - 30.f / (float)sr);   // la crête retombe doucement (30 dB/s) hors phrase
    }

    void yin(float db) {
        if (db < P.threshDb - 6.f) { voicedNow = false; return; }
        const int L = yW + tauMax;
        for (int j = 0; j < L; ++j) yLin[(size_t)j] = yRing[(size_t)((yw - L + j) & yMask)];
        // différence cumulée normalisée
        float run = 0.f; yD[0] = 1.f;
        int best = -1;
        for (int tau = 1; tau <= tauMax; ++tau) {
            float s = 0.f;
            const float* a = yLin.data(); const float* b = yLin.data() + tau;
            for (int j = 0; j < yW; ++j) { const float d = a[j] - b[j]; s += d * d; }
            run += s;
            yD[(size_t)tau] = run > 0.f ? s * (float)tau / run : 1.f;
        }
        for (int tau = tauMin; tau < tauMax; ++tau)
            if (yD[(size_t)tau] < 0.18f) { while (tau + 1 < tauMax && yD[(size_t)tau + 1] < yD[(size_t)tau]) ++tau; best = tau; break; }
        if (best < 0) { voicedNow = false; return; }
        float frac = 0.f;
        const float a = yD[(size_t)best - 1], b = yD[(size_t)best], c = yD[(size_t)best + 1], den = a - 2.f * b + c;
        if (den > 1e-6f) frac = std::max(-0.5f, std::min(0.5f, 0.5f * (a - c) / den));
        voicedNow = true; periodNow = ((float)best + frac) * (float)D;
    }

    double ppqAt(int64_t absSample) const { return ppq0 + (double)(absSample - (nowAbs - blockPos)) * bpm / 60.0 / sr; }

    void phraseEnd() {
        const int64_t endAbs = nowAbs - quiet;   // instant où le niveau est retombé
        T.phrases.fetch_add(1);
        rng = hash((uint32_t)P.seed * 2654435761u ^ hash((uint32_t)phraseIdx + 0x9e37u));
        ++phraseIdx;
        const float roll = rnd();
        const int frame = T.frameAbs.load() - quiet / hop;
        if (roll >= P.density) { pushEvent(frame, -1, 0, 0); return; }
        // caractère
        int type = P.character - 1;
        const float a = P.audace;
        if (type < 0) {
            const float w[kNumTypes] = {1.2f - 0.6f * a, 0.5f + 0.3f * a, 0.3f + 0.6f * a, 0.2f + 0.8f * a, 0.3f + 0.9f * a, 0.8f - 0.2f * a};
            float sum = 0.f; for (float x : w) sum += x;
            float r = rnd() * sum; type = 0;
            for (int k = 0; k < kNumTypes; ++k) { if (r < w[k]) { type = k; break; } r -= w[k]; type = k; }
        }
        // retard (croches), calage sur la grille de l'hôte si la position est connue
        const int eighths = P.delayMode == 0 ? 1 : P.delayMode == 1 ? 2 : (rnd() < 0.5f ? 1 : 2);
        const double eighthS = 30.0 / bpm * sr;
        int64_t start = endAbs + (int64_t)std::llround(eighths * eighthS);
        if (gridOk) {
            const double target = std::round(ppqAt(endAbs) * 2.0 + eighths) * 0.5;   // croche la plus proche de fin + n croches
            start = (nowAbs - blockPos) + (int64_t)std::llround((target - ppq0) * 60.0 / bpm * sr);
        }
        while (start <= nowAbs) start += (int64_t)std::llround(eighthS);
        // voix libre (sinon la plus ancienne)
        Voice* v = &vo[0];
        for (auto& c : vo) if (!c.on) { v = &c; break; }
        if (v->on) for (auto& c : vo) if (c.wait == 0 && c.t > v->t) v = &c;
        const int len = std::min(maxSeg - 2, (int)(P.lenMs * 1e-3f * (float)sr));
        // copie de la fin de phrase, fondus de 5 ms
        const int fade = std::max(8, (int)(0.005 * sr));
        const int64_t s0 = endAbs - len;
        for (int j = 0; j < len; ++j) {
            float g = 1.f;
            if (j < fade) g = (float)j / (float)fade; else if (j >= len - fade) g = (float)(len - 1 - j) / (float)fade;
            v->buf[(size_t)j] = cap[(size_t)((int64_t)cw - (nowAbs - (s0 + j)) - 1) & (size_t)cMask] * g;
        }
        v->on = true; v->type = type; v->len = len; v->t = 0; v->rep = 0; v->wait = (int)(start - nowAbs);
        v->g = 1.f; v->decay = 0.6f; v->reps = 1; v->ratio = 1.f; v->stN = 0; v->slice = len;
        const int eighthI = (int)eighthS;
        switch (type) {
            case Echo: v->reps = 1 + (int)std::lround(2.f * a * rnd() + a); v->reps = std::min(3, std::max(1, v->reps)); v->decay = 0.5f + 0.15f * a; v->g = 0.9f; break;
            case OctLow: v->ratio = 0.5f; v->g = 1.25f; break;
            case Phone: v->g = 1.1f; break;
            case Reverse: v->g = 0.85f; break;
            case Stutter: v->slice = std::max((int)(0.045 * sr), std::min(len / 3, eighthI / 2)); v->stN = 2 + (int)std::lround(a * 3.f * rnd() + a); v->stN = std::min(5, v->stN); break;
            case Harmony: v->ratio = (a > 0.6f && rnd() < a - 0.3f) ? 2.f : std::exp2(7.f / 12.f); v->g = 0.8f; break;
            default: break;
        }
        const int repLen = v->stN * v->slice + len;
        v->gap = std::max(1, (int)std::ceil((double)repLen / eighthS)) * eighthI;
        v->tail = v->ratio != 1.f ? (int)v->sp.lz + 64 : 0;
        // période : moyenne des trames voisées de la phrase ; décaleur remis au repos
        const float per = perN > 0 ? (float)(perSum / perN) : (float)(sr / 180.0);
        std::fill(v->sp.b[0].begin(), v->sp.b[0].end(), 0.f);
        v->sp.w = 0; v->sp.d = v->sp.dOld = v->sp.lz; v->sp.xf = 0; v->sp.xfOn = false; v->sp.setPeriod(per);
        // panoramique : côtés alternés, écart LARGEUR
        const float side = (phraseIdx & 1) ? 1.f : -1.f, pan = side * P.width * (0.55f + 0.45f * rnd());
        const float th = (pan + 1.f) * 0.25f * dsp::kPiF;
        v->pl = std::cos(th); v->pr = std::sin(th);
        if (type == Phone) { phPanL = v->pl; phPanR = v->pr; }
        T.relances.fetch_add(1); T.lastType.store(type);
        const int startFrame = T.frameAbs.load() + v->wait / hop;
        pushEvent(frame, type, startFrame, std::max(1, ((v->reps - 1) * v->gap + repLen) / hop));
    }

    void pushEvent(int frame, int type, int startFrame, int dur) {
        const int k = T.evWr.load() % Telemetry::kEv;
        T.evFrame[k].store(frame); T.evType[k].store(type); T.evStart[k].store(startFrame); T.evDur[k].store(dur);
        T.evWr.store(T.evWr.load() + 1, std::memory_order_release);
    }

    void render(Voice& v, float& wl, float& wr, float& ph) {
        if (v.wait > 0) { --v.wait; return; }
        const int repLen = v.stN * v.slice + v.len;
        float x = 0.f;
        if (v.t < repLen) {
            if (v.type == Reverse) x = v.buf[(size_t)(v.len - 1 - v.t)];
            else if (v.t < v.stN * v.slice) {
                const int k = v.t % v.slice, f = std::min(v.slice / 4, (int)(0.003 * sr));
                float g = 1.f; if (k < f) g = (float)k / (float)f; else if (k >= v.slice - f) g = (float)(v.slice - 1 - k) / (float)f;
                x = v.buf[(size_t)k] * g * (0.75f + 0.25f * (float)(v.t / v.slice) / (float)std::max(1, v.stN));
            } else x = v.buf[(size_t)(v.t - v.stN * v.slice)];
        }
        x *= v.g;
        float y = x;
        if (v.ratio != 1.f) v.sp.process(&x, &y, v.ratio);
        if (v.type == Phone) ph += y;
        else { wl += y * v.pl; wr += y * v.pr; }
        ++v.t;
        if (v.t >= std::max(v.gap, repLen + v.tail)) {
            if (++v.rep >= v.reps) v.on = false;
            else { v.t = 0; v.g *= v.decay; }
        }
    }

    void telemetry(float m, float w) {
        fIn = std::max(fIn, std::fabs(m)); fWet = std::max(fWet, std::fabs(w)); fVoiced = fVoiced || voicedNow;
        if (++hopC < hop) return;
        hopC = 0;
        const int k = T.wr.load(std::memory_order_relaxed);
        T.inDb[k].store(20.f * std::log10(fIn + 1e-6f)); T.wetDb[k].store(20.f * std::log10(fWet + 1e-6f)); T.voiced[k].store(fVoiced ? 1 : 0);
        T.wr.store((k + 1) % Telemetry::N, std::memory_order_release);
        T.frameAbs.store(T.frameAbs.load() + 1);
        int act = 0; for (auto& v : vo) if (v.on && v.wait == 0) ++act;
        T.active.store(act);
        fIn = fWet = 0.f; fVoiced = false;
    }

    void clearWet() {
        for (auto* f : {&tLp1L, &tLp2L, &tLp1R, &tLp2R, &tHpL, &tHpR, &pHp1, &pHp2, &pLp1, &pLp2}) *f = 0.f;
        for (auto& v : vo) v.on = false;
        sat.reset();
    }

    Params P;
    Telemetry T;
    double sr = 48000.0, srd = 24000.0, bpm = 120.0, ppq0 = 0.0;
    bool gridOk = false, wasPlaying = false;
    int D = 2, yW = 400, tauMin = 24, tauMax = 360, yHop = 240, yMask = 0, yw = 0, yAccN = 0, yHopC = 0;
    float yAcc = 0.f;
    std::vector<float> yRing, yLin, yD, cap;
    int cMask = 0, cw = 0, maxSeg = 0;
    bool voicedNow = false, inPhrase = false;
    float periodNow = 0.f, ms = 0.f, envC = 0.001f, peakDb = -120.f;
    int aboveLen = 0, quiet = 0, perN = 0;
    double perSum = 0.0;
    int64_t phraseIdx = 0, nowAbs = 0, blockPos = 0;
    uint32_t rng = 1;
    Voice vo[kVoices];
    dsp::sat::Saturator sat;
    float phPanL = 0.7071f, phPanR = 0.7071f;
    float tLp1L = 0, tLp2L = 0, tLp1R = 0, tLp2R = 0, tHpL = 0, tHpR = 0, pHp1 = 0, pHp2 = 0, pLp1 = 0, pLp2 = 0;
    float gLvl = 1.f, gDry = 1.f;
    int hop = 960, hopC = 0;
    float fIn = 0.f, fWet = 0.f; bool fVoiced = false;
};

}  // namespace rel
