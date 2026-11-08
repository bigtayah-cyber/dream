// Orbe Chain : moteur commun à Orbe Looks et Orbe Atelier. C++17 pur, sans JUCE (testable hors-ligne).
// Chaîne fixe : Tune/Shifter -> EQ 4 bandes -> Compresseur -> Saturation/crush -> De-ess -> Doubleur -> Delay -> Reverb -> Mix -> Sortie -> Limiteur.
// Briques réutilisées : voile::{Yin, Shifter, Delay, PingPong, Reverb, Smooth, nearestAllowed}, onek::{Biquad, Comp, DeEss}.
// Les valeurs des looks sont des HYPOTHÈSES d'écoute (voir docs/looks-voix.md), pas des mesures de chaînes réelles.
#pragma once
#include <algorithm>
#include <cmath>
#include <vector>
#include "../../OneKnob/OneKnob.h"

namespace oc {

constexpr float kPi = 3.14159265358979f;
using voile::Delay;
using voile::Smooth;
using onek::Biquad;

// Réglages complets de la chaîne. Tout est float (les interrupteurs valent 0/1, les choix sont des indices).
struct S {
    float eqOn = 0, hp = 20, lsF = 150, lsG = 0, b1F = 400, b1G = 0, b1Q = 1, b2F = 3000, b2G = 0, b2Q = 1, hsF = 9000, hsG = 0;
    float cpOn = 0, thr = -20, ratio = 3, atk = 10, rel = 120, mk = 0, cpMix = 1;
    float saOn = 0, drive = 0, bias = 0, crush = 0, saMix = 1;
    float tnOn = 0, tnAmt = 1, tnSpeed = 20, shift = 0, key = 0, scale = 0;  // key : 0 = Auto, 1..12 = C..B ; scale : 0 majeur, 1 mineur
    float dsOn = 0, dsF = 6500, dsAmt = 0.5f, dsThr = -26;
    float dbOn = 0, dbMix = 0.3f, dbA = 0.14f, dbB = -0.14f, dbWide = 0.8f;
    float dyOn = 0, dyMs = 250, dyDiv = 0, dyFb = 0.3f, dyMix = 0.2f, dyDuck = 0.7f;  // dyDiv : 0 libre, 1 1/16, 2 1/8, 3 1/8 pointée, 4 1/4, 5 1/2
    float rvOn = 0, rvSize = 1.2f, rvPre = 15, rvDamp = 0.45f, rvMix = 0.2f;
    float lmOn = 1, lmCeil = -1;
    float mix = 1, out = 0, lvBias = 0;  // lvBias : décalage du niveau automatique (dB), interne
};

// Contournement : l'entrée exactement retardée de la latence déclarée.
struct BypassDelay {
    Delay l, r; int lat = 0;
    void init(int latency) { lat = latency; l.init(latency + 8); r.init(latency + 8); }
    void process(float* a, float* b, int n, bool stereoIn) {
        for (int i = 0; i < n; ++i) { const float x0 = a[i], x1 = stereoIn ? b[i] : x0; l.push(x0); r.push(x1); a[i] = l.read((float)lat); b[i] = r.read((float)lat); }
    }
};

// Limiteur à anticipation : gain = moyenne glissante du minimum glissant des gains cibles, signal retardé de L-1 échantillons.
struct Limiter {
    int L = 64; Delay dl, dr; std::vector<float> tg, rg; int p = 0; double sum = 0; float rc = 1.f, step = 1e-4f;
    void init(int look, double sr) { L = std::max(8, look); dl.init(L + 2); dr.init(L + 2); tg.assign((size_t)L, 1.f); rg.assign((size_t)L, 1.f); p = 0; sum = L; rc = 1.f; step = (float)(1.0 / (0.12 * sr)); }
    int latency() const { return L - 1; }
    void process(float l, float r, float ceil, float& oL, float& oR) {
        const float pk = std::max(std::fabs(l), std::fabs(r));
        tg[(size_t)p] = pk > ceil ? ceil / pk : 1.f;
        float h = 1.f; for (int i = 0; i < L; ++i) h = std::min(h, tg[(size_t)i]);
        rc = std::min(h, rc + step);
        sum += (double)rc - rg[(size_t)p]; rg[(size_t)p] = rc;
        const float g = (float)(sum / L);
        if (++p >= L) p = 0;
        dl.push(l); dr.push(r);
        oL = dl.read((float)(L - 1)) * g; oR = dr.read((float)(L - 1)) * g;
    }
};

inline void peaking(Biquad& b, float sr, float f, float db, float q) {
    const float A = std::pow(10.f, db / 40.f), w = 2 * kPi * f / sr, c = std::cos(w), al = std::sin(w) / (2 * q);
    b.set(1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
}

class Chain {
public:
    void prepare(double sampleRate) {
        sr = (float)sampleRate;
        win = std::max(64, (int)(0.032 * sampleRate)) & ~1;
        tune.init(win); dbl1.init(win); dbl2.init(win);
        lim.init((int)(0.0015 * sampleRate), sampleRate);
        sat.init(voile::Oversampler::stagesFor(sampleRate, 3));   // saturation suréchantillonnée ×8 (×4 à 88,2/96 kHz), phase linéaire
        satLat = sat.latency();
        lat = win / 2 + satLat + lim.latency();
        dryL.init(win / 2 + satLat + 16); dryR.init(win / 2 + satLat + 16);
        yin.init(sampleRate); hop = std::max(64, (int)(0.0053 * sampleRate));
        pp.init(sampleRate); rev.init(sampleRate); rev.setDecay(1.2f); lastRt = 1.2f;
        pre.init((int)(0.16 * sampleRate) + 4);
        const float c = 1.f - std::exp(-1.f / (0.02f * sr));
        for (auto* s : {&eqE, &cpE, &saE, &dsE, &dbS, &dyS, &rvS, &mixS, &outS, &drvS, &crS, &saMS, &cpMS}) s->c = c;
        ratioC = 1.f - std::exp(-1.f / (0.003f * sr)); agC = 1.f - std::exp(-1.f / (0.6f * sr)); agD = 1.f - std::exp(-1.f / (0.1f * sr)); rmsC = 1.f - std::exp(-1.f / (0.15f * sr));
        for (auto* b : {&hpF, &lsF, &b1F, &b2F, &hsF, &apHp}) b->reset();
        apHp.highpass(sr, 60.f);
        ds.prepare(sr); comp.prepare(sr);
        for (auto* s : {&eqE, &cpE, &saE, &dsE, &dbS, &dyS, &rvS, &drvS, &crS}) s->v = 0.f;
        saMS.v = cpMS.v = 1.f; mixS.v = 1.f; outS.v = 1.f;
        ratio = ratioT = 1.f; shiftSm = shiftT = 0.f; acc = 0.f; decCount = hopCount = holdCnt = hold = 0; lastTgt = -1;
        for (auto& v : hist) v = 0.f;
        wgt = 0.f; keyTick = 0; haveKey = false; effKey = 0; effScale = 1;
        held = dcX = dcY = 0.f; inMs = 1e-6f; outMs = 1e-6f; ag = 1.f; lvl = 0.f; peak = 0.f; meanSq = 0.f;
        last = {}; lastValid = false; set(P);
    }
    void set(const S& s) {
        P = s;
        if (!lastValid || s.hp != last.hp) hpF.highpass(sr, std::clamp(s.hp, 20.f, 0.4f * sr));
        if (!lastValid || s.lsF != last.lsF || s.lsG != last.lsG) lsF.lowShelf(sr, s.lsF, s.lsG);
        if (!lastValid || s.b1F != last.b1F || s.b1G != last.b1G || s.b1Q != last.b1Q) peaking(b1F, sr, s.b1F, s.b1G, s.b1Q);
        if (!lastValid || s.b2F != last.b2F || s.b2G != last.b2G || s.b2Q != last.b2Q) peaking(b2F, sr, s.b2F, s.b2G, s.b2Q);
        if (!lastValid || s.hsF != last.hsF || s.hsG != last.hsG) hsF.highShelf(sr, std::min(s.hsF, 0.45f * sr), s.hsG);
        comp.thr = s.thr; comp.ratio = std::max(1.f, s.ratio); comp.makeup = s.mk;
        comp.aC = std::exp(-1.f / (std::max(0.1f, s.atk) * 1e-3f * sr)); comp.rC = std::exp(-1.f / (std::max(1.f, s.rel) * 1e-3f * sr));
        if (!lastValid || s.dsF != last.dsF) ds.hp.highpass(sr, std::min(s.dsF, 0.45f * sr), 0.8f);
        ds.thr = std::pow(10.f, s.dsThr / 20.f); ds.amt = s.dsAmt * 1.5f;
        if (!lastValid || s.rvSize != last.rvSize) { rev.setDecay(std::max(0.2f, s.rvSize)); lastRt = s.rvSize; }
        last = s; lastValid = true;
    }
    void setBpm(double b) { bpm = (float)(b > 20.0 ? b : 120.0); }
    int latency() const { return lat; }
    bool keyKnown() const { return haveKey; }
    int detectedKey() const { return effKey; }
    int detectedScale() const { return effScale; }  // 1 majeur, 2 mineur
    float outPeak() const { return peak; }
    float outMeanSquare() const { return meanSq; }
    float gainReduction() const { return comp.gr * cpE.v; }
    float shiftSemis() const { return shiftSm; }
    void setAutoLevel(bool on) { autoLevel = on; }

    // Entrée mono (inR nul) ou stéréo -> sortie stéréo. Fonctionne en place.
    void process(const float* inL, const float* inR, float* outL, float* outR, int n) {
        static const float divs[6] = {0.f, 0.25f, 0.5f, 0.75f, 1.f, 2.f};
        const float beat = 60.f / bpm * sr;
        const float tgtD = std::min(2.0f * sr, P.dyDiv >= 0.5f ? beat * divs[std::min(5, (int)(P.dyDiv + 0.5f))] : P.dyMs * 1e-3f * sr);
        const float preS = std::clamp(P.rvPre, 0.f, 150.f) * 1e-3f * sr;
        const float ceil = P.lmOn > 0.5f ? std::pow(10.f, P.lmCeil / 20.f) : 1e6f;
        float pk = 0.f, ms = 0.f;
        for (int i = 0; i < n; ++i) {
            const float iL = inL[i], iR = inR ? inR[i] : iL;
            const float x = 0.5f * (iL + iR);
            dryL.push(iL); dryR.push(iR);
            const float ax = std::fabs(x);
            lvl += (ax - lvl) * (ax > lvl ? 0.05f : 0.002f);
            inMs += (x * x - inMs) * rmsC;

            // 1. hauteur
            acc += apHp.process(x);
            if (++decCount >= yin.dec) { yin.push(acc / (float)yin.dec); acc = 0.f; decCount = 0; }
            if (++hopCount >= hop) { hopCount = 0; analyzeHop(); }
            ratio += (ratioT - ratio) * ratioC;
            float y = tune.process(x, ratio);

            // 2. EQ  3. compresseur
            const float e = eqE.next(P.eqOn > 0.5f ? 1.f : 0.f);
            if (e > 1e-4f) { const float q = b2F.process(b1F.process(lsF.process(hpF.process(y)))); y += (hsF.process(q) - y) * e; }
            else { hpF.process(y); }
            const float ce = cpE.next(P.cpOn > 0.5f ? 1.f : 0.f), cm = cpMS.next(P.cpMix) * ce;
            { const float g = comp.gain(y, y); y += (y * g - y) * cm; if (ce < 1e-4f) comp.gr = 0.f; }

            // 4. saturation + crush
            const float se = saE.next(P.saOn > 0.5f ? 1.f : 0.f), sm = saMS.next(P.saMix) * se;
            const float d = drvS.next(P.drive), cr = crS.next(P.crush);
            if (sm > 1e-4f) {
                float w;
                if (d > 0.001f) {
                    const float b = 0.4f * P.bias, kd = 1.f + 9.f * d, nb = std::tanh(b), gn = 1.f / (1.f + 1.2f * d);
                    w = sat.process(y, [&](float v) { return (std::tanh(v * kd + b) - nb) * gn; });
                } else { sat.pushRing(y); w = sat.dry(); }
                const float yd = sat.dry();
                if (cr > 0.001f) {
                    const float q = std::exp2(14.f - 8.f * cr * cr); w = std::round(w * q) / q;
                    if (++holdCnt >= 1 + (int)(cr * cr * 4.f)) { holdCnt = 0; held = w; }
                    w = held;
                }
                const float dc = w - dcX + 0.995f * dcY; dcX = w; dcY = dc;
                y = yd + (dc - yd) * sm;
            } else y = sat.bypass(y);

            // 5. de-ess
            const float de = dsE.next(P.dsOn > 0.5f ? 1.f : 0.f);
            { const float z = ds.process(y); y += (z - y) * de; }

            // 6. doubleur (deux voix décalées en hauteur, ouvertes en stéréo)
            const float m = dbS.next(P.dbOn > 0.5f ? P.dbMix : 0.f) * 0.7f;
            const float d1 = dbl1.process(y, std::exp2(P.dbA / 12.f)), d2 = dbl2.process(y, std::exp2(P.dbB / 12.f));
            const float wa = 0.5f + 0.5f * P.dbWide, wb = 1.f - wa;
            float wl = y + m * (wa * d1 + wb * d2), wr = y + m * (wb * d1 + wa * d2);

            // 7. delay ping-pong avec ducking sous la voix  8. reverb
            float dL, dR, rL, rR;
            pp.process(y, std::max(64.f, tgtD), std::clamp(P.dyFb, 0.f, 0.9f), dL, dR);
            const float dm = dyS.next(P.dyOn > 0.5f ? P.dyMix : 0.f), duck = 1.f - P.dyDuck * std::min(1.f, lvl * 6.f);
            wl += dm * duck * dL; wr += dm * duck * dR;
            pre.push(y * 0.6f);
            rev.process(pre.read(preS), std::clamp(P.rvDamp, 0.f, 0.95f), rL, rR);
            const float rm = rvS.next(P.rvOn > 0.5f ? P.rvMix : 0.f);
            wl += rm * rL; wr += rm * rR;

            // niveau automatique lent (Orbe Looks) : le traité garde le niveau de la voix d'entrée
            outMs += (0.5f * (wl * wl + wr * wr) - outMs) * rmsC;
            if (autoLevel && inMs > 1e-4f) { const float t = std::clamp(std::sqrt(inMs / (outMs + 1e-9f)) * std::pow(10.f, P.lvBias / 20.f), 0.1f, 3.f); ag += (t - ag) * (t < ag ? agD : agC); }
            if (!autoLevel) ag = 1.f;
            wl *= ag; wr *= ag;

            // 9. mix, sortie, limiteur
            const float mx = mixS.next(P.mix), og = outS.next(std::pow(10.f, P.out / 20.f));
            const float dL_ = dryL.read((float)(win / 2 + satLat)), dR_ = dryR.read((float)(win / 2 + satLat));  // sec aligné sur le traité (avant le limiteur)
            float oL, oR;
            lim.process((dL_ * (1.f - mx) + wl * mx) * og, (dR_ * (1.f - mx) + wr * mx) * og, ceil, oL, oR);
            outL[i] = fin(oL); outR[i] = fin(oR);
            pk = std::max(pk, std::max(std::fabs(outL[i]), std::fabs(outR[i])));
            ms = std::max(ms, 0.5f * (outL[i] * outL[i] + outR[i] * outR[i]));
        }
        peak = pk; meanSq = ms;
    }

private:
    static float fin(float v) { return std::isfinite(v) ? std::clamp(v, -4.f, 4.f) : 0.f; }
    void trackKey(float midi) {
        for (auto& v : hist) v *= 0.9993f;
        hist[((int)std::lround(midi) % 12 + 12) % 12] += 1.f;
        wgt = wgt * 0.9993f + 1.f;
        if (++keyTick < 96 || wgt < 60.f) return;
        keyTick = 0;
        static const float maj[12] = {6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f, 2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f};
        static const float mnr[12] = {6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f, 2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f};
        float c[2][12]; float best = -2.f; int bk = 0, bs = 1;
        float mh = 0; for (float v : hist) mh += v; mh /= 12;
        for (int sc = 0; sc < 2; ++sc) for (int k = 0; k < 12; ++k) {
            const float* prof = sc == 0 ? maj : mnr; float mp = 0; for (int i = 0; i < 12; ++i) mp += prof[i]; mp /= 12;
            float num = 0, dh = 0, dp = 0;
            for (int i = 0; i < 12; ++i) { const float a = hist[(i + k) % 12] - mh, b = prof[i] - mp; num += a * b; dh += a * a; dp += b * b; }
            c[sc][k] = num / (std::sqrt(dh * dp) + 1e-9f);
            if (c[sc][k] > best) { best = c[sc][k]; bk = k; bs = sc + 1; }
        }
        // fiabilité : corrélation forte ET nette avance sur toute tonalité d'un autre jeu de notes (majeur k = mineur k+9 : même jeu)
        auto setId = [](int k, int sc) { return sc == 1 ? k : (k + 3) % 12; };
        float other = -2.f; for (int sc = 0; sc < 2; ++sc) for (int k = 0; k < 12; ++k) if (setId(k, sc + 1) != setId(bk, bs)) other = std::max(other, c[sc][k]);
        const float margin = best - other;
        const bool strong = haveKey ? (best > 0.50f && margin > 0.04f) : (best > 0.60f && margin > 0.08f && wgt > 150.f);
        if (!strong) { haveKey = false; return; }
        const float cur = haveKey ? c[effScale - 1][effKey] : -2.f;
        if (!haveKey || best > cur + 0.06f) { effKey = bk; effScale = bs; }
        haveKey = true;
    }
    void analyzeHop() {
        const bool on = P.tnOn > 0.5f, manual = P.key >= 0.5f;
        const float f0 = yin.analyze();
        if (f0 > 0.f) {
            const float midi = 69.f + 12.f * std::log2(f0 / 440.f);
            if (!manual) trackKey(midi);
            const int key = manual ? (int)(P.key + 0.5f) - 1 : effKey, sc = manual ? 1 + (int)(P.scale + 0.5f) : effScale;
            if (on && (manual || haveKey)) {   // tonalité non fiable : aucune correction vers une note
                int tgt = voile::nearestAllowed(midi, key, sc);
                if (lastTgt >= 0 && voile::allowedNote(lastTgt, key, sc) && std::fabs(midi - (float)lastTgt) < 0.62f) tgt = lastTgt;
                lastTgt = tgt; shiftT = ((float)tgt - midi) * P.tnAmt;
            } else { lastTgt = -1; shiftT = 0.f; }
            hold = 0;
        } else if (++hold > 4) { shiftT *= 0.7f; lastTgt = -1; }
        const float a = 1.f - std::exp(-((float)hop / sr) / std::max(1e-3f, P.tnSpeed * 1e-3f));
        shiftSm += (shiftT - shiftSm) * a;
        shiftSm = std::clamp(shiftSm, -6.5f, 6.5f);
        ratioT = std::exp2((shiftSm + (on ? P.shift : 0.f)) / 12.f);
    }

    S P, last; bool lastValid = false;
    float sr = 48000.f, bpm = 120.f, lastRt = 1.2f;
    int win = 1536, lat = 768, hop = 256, satLat = 0;
    voile::Shifter tune, dbl1, dbl2;
    voile::Oversampler sat;
    Delay dryL, dryR, pre;
    voile::Yin yin; voile::PingPong pp; voile::Reverb rev; Limiter lim;
    Biquad hpF, lsF, b1F, b2F, hsF, apHp;
    onek::Comp comp; onek::DeEss ds;
    Smooth eqE, cpE, saE, dsE, dbS, dyS, rvS, mixS, outS, drvS, crS, saMS, cpMS;
    float ratio = 1.f, ratioT = 1.f, ratioC = 0.01f, shiftSm = 0.f, shiftT = 0.f, acc = 0.f, held = 0.f, dcX = 0.f, dcY = 0.f;
    float inMs = 1e-6f, outMs = 1e-6f, ag = 1.f, agC = 1e-4f, agD = 1e-3f, rmsC = 1e-4f, lvl = 0.f, peak = 0.f, meanSq = 0.f;
    int decCount = 0, hopCount = 0, holdCnt = 0, hold = 0, lastTgt = -1;
    float hist[12] = {}, wgt = 0.f; int keyTick = 0, effKey = 0, effScale = 1; bool haveKey = false, autoLevel = false;
};

}  // namespace oc
