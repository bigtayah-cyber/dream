// Les quatre modèles de la chaîne actuelle des plugins un-potard (C++17 pur, sans JUCE, sans allocation dans process()).
// Chaque modèle traite la voix en MONO (après auto-entrée) dans l'ordre : hauteur -> saturation -> dynamique -> de-esser/air -> égalisation,
// puis alimente un BUS D'ESPACE parallèle (doubles, reverb, échos) atténué sous la voix. À k = 0 tout est sauté ou à mélange nul :
// sortie = entrée retardée de la latence, exactement. Les courbes (points d'arrêt, Hermite monotone) sont des HYPOTHÈSES d'écoute,
// pas des mesures de référence ; elles ont été vérifiées par les tests objectifs de tests/onek_test.cpp (neutralité, réduction qui
// suit le potard, pas de marche, pas de repliement, latence exacte).
//   Grain Nord : retune sûr (tonalité fiable), doubles ADT, « brume tonale » (reverb dont la queue est filtrée par des résonateurs
//                accordés sur la gamme détectée), bande usée en fin de course (saturation de bande ×4, bande passante, pleurage).
//   Gorge      : compresseur à lampes (VariMuComp), grain dynamique 150-900 Hz qui suit les syllabes, « râle » (sous-harmonique f0/2
//                synchrone + souffle à la période) sans latence, slapback calé sur le tempo.
//   Poupée     : hard-tune chromatique (TuneCore, mode Direct), pédale « hauteur + formants » (formants +0 à +4,5 dt, hauteur sur la
//                grille), octave en double à formants resserrés, FET, écrêtage ×2, air contrôlé par le de-esser, slap (pas de reverb : CPU).
//   Patine     : finisseur sans latence : préampli à lampe (courbe asymétrique ADAA ; la table triode linéaire par morceaux replie
//                sans suréchantillonnage), opto en série, densité FET en parallèle, de-esser + air dynamique,
//                courbe tonale cible « voix pop finie » (égaliseur lent à 8 bandes).
#pragma once
#include <algorithm>
#include <cmath>
#include <vector>
#include "OneKnobDsp.h"
#include "../Engine.h"
#include "../Dsp/Compressors.h"
#include "../Dsp/DeEssDyn.h"
#include "../Dsp/Saturation.h"
#include "../Dsp/Splicer.h"
#include "../Suite/OrbeAutotune/TuneCore.h"

namespace onek {

// Réduction effective (dB) d'un compresseur de réduction g mélangé en parallèle à la part m (affichage, tests).
inline float effective(float g, float m) { return -linToDb(1.f - m + m * dbToLin(-g)); }

// Contexte du potard pour une tranche (k lissé, réglages du volet).
struct Ctx {
    float k = 0.f, spec = 0.5f, opt = 1.f, bpm = 120.f;
    int keyChoice = 0, scaleChoice = 0;
};
// Une tranche (<= 32 échantillons) : entrée mono x, sorties : voix traitée (core) et bus d'espace stéréo.
struct Slice {
    const float* x; float* core; float* sL; float* sR; int n; bool voice;
};

// Détection de hauteur pour les modèles sans moteur de hauteur dédié : YIN décimé (voile::Yin), précédé d'un VRAI passe-bas
// anti-repliement (le moteur historique décimait sans filtre). Une analyse toutes les ~5,3 ms ; désactivable (économie CPU).
struct PitchTrack {
    voile::Yin yin; Bq aa1, aa2; int decC = 0, hop = 256, hopC = 0; float f0 = 0.f; bool voiced = false, enabled = true;
    void prepare(double sr) {
        yin.init(sr); aa1.lp(sr, 0.21 * sr / yin.dec, 0.54); aa2.lp(sr, 0.21 * sr / yin.dec, 1.31);
        hop = std::max(64, (int)(0.0053 * sr)); decC = hopC = 0; f0 = 0.f; voiced = false;
    }
    bool push(float x) {
        const float y = aa2.run(aa1.run(x));
        if (++decC >= yin.dec) { decC = 0; yin.push(y); }
        if (++hopC < hop) return false;
        hopC = 0;
        f0 = enabled ? yin.analyze() : 0.f; voiced = f0 > 0.f;
        return true;
    }
};

// ======================================================================================== GRAIN NORD
class GrainNord {
public:
    void prepare(double s) {
        sr = (float)s;
        pt.prepare(s);
        lz = (int)(0.5 * s / 70.0) + 48;
        vs.init(s, lz, false);
        tape.prepare(s, 4);
        d0 = std::max(8, (int)std::lround(0.001 * s));
        wow.init(2 * d0 + 8);
        lat = lz + tape.latency() + d0;
        opto.prepare(s); ds.prepare(s);
        hpLp.lp(s, 48.0); hiS.prepare(s, 9000.f, false);
        for (auto* f : {&tl1, &tl2}) f->reset();
        tlHz = -1.f;
        dbl1.init(s, 0x9E3779B9u, 19.f, 3.f, 8.f, 6500.f);
        dbl2.init(s, 0x85EBCA6Bu, 27.f, 3.5f, 9.f, 5200.f);
        rev.init(s); rev.setTone(0.8f); rev.setDecay(1.4f); rt = 1.4f;
        pre.init((int)(0.03 * s) + 4);
        pp.init(s);
        sendLpC = 1.f - std::exp(-2.f * kPi * 9000.f / sr); sendHpC = std::exp(-2.f * kPi * 160.f / sr);
        kt.reset(); res.clear(); resKey = resScale = -1;
        ratio = ratioT = 1.f; ratioC = 1.f - std::exp(-1.f / (0.003f * sr));
        shiftT = shiftSm = 0.f; runLen = hold = 0; lastTgt = -1; mBuf[0] = mBuf[1] = mBuf[2] = 0.f;
        ce = sendLp = sendHp = 0.f; wph1 = wph2 = 0.f; share = 0.f; sg = 0.f; sgC = 1.f - std::exp(-1.f / (0.15f * sr));
        P = {};
    }
    int latency() const { return lat; }
    const KeyTracker& keys() const { return kt; }

    void update(const Ctx& c) {
        static const Curve cDbl{{0.f, 0.f}, {0.1f, 0.35f}, {0.3f, 0.8f}, {0.6f, 0.9f}, {1.f, 1.f}};
        static const Curve cTension{{0.f, 0.f}, {0.3f, 0.f}, {0.5f, 0.65f}, {0.7f, 1.f}, {1.f, 1.f}};
        static const Curve cRetune{{0.f, 90.f}, {0.3f, 90.f}, {0.7f, 25.f}, {1.f, 20.f}};
        static const Curve cDead{{0.f, 14.f}, {0.3f, 14.f}, {0.7f, 5.f}, {1.f, 4.f}};
        static const Curve cSend{{0.f, 0.f}, {0.15f, 0.12f}, {0.4f, 0.3f}, {0.7f, 0.5f}, {1.f, 0.65f}};
        static const Curve cRt{{0.f, 1.2f}, {0.5f, 1.8f}, {1.f, 2.4f}};
        static const Curve cShare{{0.f, 0.f}, {0.2f, 0.35f}, {0.6f, 0.7f}, {1.f, 0.85f}};
        static const Curve cPp{{0.f, 0.f}, {0.4f, 0.f}, {0.7f, 0.1f}, {1.f, 0.2f}};
        static const Curve cCompMix{{0.f, 0.f}, {0.12f, 0.6f}, {0.35f, 1.f}, {1.f, 1.f}};
        static const Curve cThr{{0.f, -14.f}, {0.3f, -22.f}, {0.7f, -29.f}, {1.f, -35.f}};
        static const Curve cDsRange{{0.f, 0.f}, {0.2f, 5.f}, {1.f, 8.f}};
        static const Curve cDsThr{{0.f, 8.f}, {1.f, 3.f}};
        static const Curve cHi{{0.f, 0.f}, {0.3f, -1.f}, {1.f, -4.f}};
        static const Curve cHp{{0.f, 0.f}, {0.2f, 1.f}};
        static const Curve cSatMix{{0.f, 0.f}, {0.4f, 0.f}, {0.6f, 0.5f}, {0.8f, 1.f}};
        static const Curve cSatDrive{{0.f, 0.f}, {0.6f, 2.f}, {0.85f, 8.f}, {1.f, 14.f}};
        static const Curve cTapeW{{0.f, 0.f}, {0.7f, 0.f}, {0.78f, 0.5f}, {1.f, 1.f}};
        static const Curve cTapeHz{{0.f, 18000.f}, {0.7f, 18000.f}, {1.f, 5000.f}};
        static const Curve cWow{{0.f, 0.f}, {0.7f, 0.f}, {1.f, 0.35f}};
        const float k = c.k;
        P.dbl = cDbl(k); P.tension = cTension(k); P.retuneMs = cRetune(k); P.dead = cDead(k);
        P.send = cSend(k); P.pp = cPp(k); P.hp = cHp(k); P.tapeW = cTapeW(k); P.wowMs = cWow(k);
        P.width = 2.f * clamp01(c.spec);
        P.shareT = cShare(k) * clamp01(c.opt);
        P.bpm = c.bpm;
        const float r = cRt(k);
        if (std::fabs(r - rt) > 0.02f) { rev.setDecay(r); rt = r; }
        kt.select(c.keyChoice, c.scaleChoice);
        dsp::sat::Params sp; sp.curve = dsp::sat::Curve::Tanh; sp.driveDb = cSatDrive(k); sp.bias = 0.12f; sp.emphasisDb = 4.f; sp.mix = cSatMix(k);
        tape.setParams(sp);
        dsp::comp::OptoParams op; op.common.thresholdDb = cThr(k); op.common.mix = P.compMix = cCompMix(k); op.common.sidechainHz = 90.f;
        opto.setParams(op);
        dsp::deess::Params dp; dp.thresholdDb = cDsThr(k); dp.rangeDb = cDsRange(k); dp.ratio = 3.f; dp.freqHz = 5500.f;
        ds.setParams(dp);
        hiS.setDb(cHi(k));
        const float hz = cTapeHz(k);
        if (std::fabs(hz - tlHz) > 0.01f * hz) { tl1.lp(sr, hz, 0.54); tl2.lp(sr, hz, 1.31); tlHz = hz; }
        // résonateurs de la brume tonale : réaccordés quand la tonalité change
        if (kt.haveKey && (kt.effKey != resKey || kt.effScale != resScale)) { res.tune(sr, kt.effKey, kt.effScale); resKey = kt.effKey; resScale = kt.effScale; }
    }

    // Échantillon par échantillon : les décisions d'analyse (voisé, période) s'appliquent à l'instant exact, quel que soit le découpage.
    void process(const Slice& b) {
        const float tgtD = 60.f / std::max(40.f, P.bpm) * 0.5f * sr;   // croche
        const float shareT = res.n > 0 && kt.haveKey ? P.shareT : 0.f;
        for (int i = 0; i < b.n; ++i) {
            const float x = b.x[i];
            if (pt.push(x)) analyzeHop();
            ratio += (ratioT - ratio) * ratioC;
            float y = vs.process(x, ratio);
            y = tape.processSample(y);
            opto.process(&y, nullptr, 1);
            ds.process(&y, nullptr, 1);
            if (P.hp > 0.f) y -= P.hp * hpLp.run(y); else hpLp.run(y);
            y = hiS.run(y);
            // bande usée : bande passante réduite (mélange de la différence : exact à 0) + pleurage (retard modulé)
            const float lpd = tl2.run(tl1.run(y));
            y += P.tapeW * (lpd - y);
            wow.push(y);
            if (P.wowMs > 0.f) {
                wph1 += 0.55f / sr; wph1 -= std::floor(wph1); wph2 += 6.5f / sr; wph2 -= std::floor(wph2);
                const float m = 0.7f * std::sin(2.f * kPi * wph1) + 0.3f * std::sin(2.f * kPi * wph2);
                y = wow.readCubic((float)d0 + P.wowMs * 1e-3f * sr * m);
            } else y = wow.read((float)d0);
            b.core[i] = y;
            // bus d'espace (parallèle), atténué sous la voix
            const float ay = std::fabs(y);
            ce += (ay - ce) * (ay > ce ? 0.05f : 0.002f);
            const float duck = 1.f - 0.6f * std::min(1.f, ce * 6.f);
            sg += ((b.voice ? 1.f : 0.f) - sg) * sgC;
            const float v1 = dbl1.process(y, pt.voiced) * P.dbl * 0.35f * sg, v2 = dbl2.process(y, pt.voiced) * P.dbl * 0.30f * sg;
            float sl = 0.4f * v1 + 0.8f * v2, srr = 0.8f * v1 + 0.4f * v2;
            sendLp += (y - sendLp) * sendLpC; sendHp = sendHpC * sendHp + (1.f - sendHpC) * sendLp;
            pre.push((sendLp - sendHp) * P.send * 0.5f * sg);
            float rL, rR; rev.process(pre.read(0.02f * sr), 0.4f, rL, rR);
            share += (shareT - share) * 0.0005f;
            if (share > 1e-4f) { const float tL = res.run(0, rL), tR = res.run(1, rR); rL += share * (tL - rL); rR += share * (tR - rR); }
            float dL, dR; pp.process(y * P.pp * sg, tgtD, 0.35f, dL, dR);
            sl += duck * (rL + dL); srr += duck * (rR + dR);
            const float mid = 0.5f * (sl + srr), side = 0.5f * (sl - srr) * P.width;
            b.sL[i] = mid + side; b.sR[i] = mid - side;
        }
    }
    void settle() { opto.prepare(sr); share = P.shareT * (res.n > 0 && kt.haveKey ? 1.f : 0.f); }
    float effGr() const { return effective(opto.gainReductionDb(), P.compMix); }

private:
    // Résonateurs accordés (2 voies) : passe-bandes à crête 0 dB, bande ~3 Hz (queue ~0,1 s qui « chante » la gamme), notes de la
    // gamme de A2 à A5. Normalisés pour rendre à peu près l'énergie de la reverb d'entrée.
    struct ResBank {
        static constexpr int kMax = 24;
        Bq f[2][kMax]; int n = 0; float norm = 1.f;
        void clear() { n = 0; }
        void tune(float sr, int key, int scale) {
            n = 0; double sumBw = 0.0;
            for (int m = 45; m <= 81 && n < kMax; ++m) {
                if (!allowedNote(m, key, scale)) continue;
                const double f0 = 440.0 * std::pow(2.0, (m - 69) / 12.0), bw = 3.0 + 0.004 * f0, q = f0 / bw;
                for (int c = 0; c < 2; ++c) { f[c][n].bp(sr, f0, q); }
                sumBw += 1.5707963 * bw; ++n;
            }
            norm = n > 0 ? (float)std::min(6.0, 0.6 * std::sqrt(0.5 * sr / sumBw) / std::sqrt((double)n) * 2.0) : 1.f;
        }
        float run(int c, float x) { float s = 0.f; for (int i = 0; i < n; ++i) s += f[c][i].run(x); return s * norm; }
    };
    void analyzeHop() {
        const float f0 = pt.f0;
        if (pt.voiced) { const float per = sr / f0; vs.setPeriod(per); dbl1.setPeriod(per); dbl2.setPeriod(per); }
        const float cap = 1.5f;
        if (pt.voiced) {
            hold = 0;
            const float midi = 69.f + 12.f * std::log2(f0 / 440.f);
            mBuf[2] = mBuf[1]; mBuf[1] = mBuf[0]; mBuf[0] = midi; ++runLen;
            const float lo = std::min(mBuf[0], std::min(mBuf[1], mBuf[2])), hi = std::max(mBuf[0], std::max(mBuf[1], mBuf[2]));
            if (runLen >= 3 && hi - lo < 0.5f) {
                const float m = std::max(std::min(mBuf[0], mBuf[1]), std::min(std::max(mBuf[0], mBuf[1]), mBuf[2]));
                const float idM = kt.feedStable(m);
                if (!kt.usable()) { shiftT *= 0.5f; lastTgt = -1; }
                else {
                    int tgt = nearestAllowed(idM, kt.effKey, kt.effScale);
                    const bool autoMode = kt.detecting();
                    if (autoMode && std::fabs(idM - (float)tgt) > 0.65f) tgt = (int)std::lround(idM);
                    if (lastTgt >= 0 && std::fabs(idM - (float)lastTgt) < 0.6f &&
                        (allowedNote(lastTgt, kt.effKey, kt.effScale) || (autoMode && std::fabs(idM - (float)lastTgt) < 0.35f))) tgt = lastTgt;
                    lastTgt = tgt;
                    float e = ((float)tgt - m) * 100.f;
                    e -= std::clamp(e, -P.dead, P.dead);
                    shiftT = e * 0.01f * P.tension;
                }
            } else shiftT *= 0.5f;
        } else { runLen = 0; if (++hold > 4) { shiftT *= 0.7f; lastTgt = -1; } }
        const float tc = std::max(0.5f, P.retuneMs) * 1e-3f;
        shiftSm += (shiftT - shiftSm) * (1.f - std::exp(-((float)pt.hop / sr) / tc));
        shiftSm = std::clamp(shiftSm, -cap, cap);
        ratioT = std::exp2(shiftSm / 12.f);
    }
    struct Pm { float dbl = 0, tension = 0, retuneMs = 90, dead = 14, send = 0, pp = 0, hp = 0, tapeW = 0, wowMs = 0, width = 1, shareT = 0, bpm = 120, compMix = 0; } P;
    float sr = 48000.f, rt = 1.4f, tlHz = -1.f;
    int lz = 400, d0 = 48, lat = 0;
    PitchTrack pt; KeyTracker kt; dsp::VoiceShifter vs;
    dsp::sat::Saturator tape; voile::Delay wow; Bq tl1, tl2, hpLp; Shelf hiS;
    dsp::comp::OptoComp opto; dsp::deess::DeEsser ds;
    dsp::AdtVoice dbl1, dbl2; voile::Reverb rev; voile::Delay pre; voile::PingPong pp; ResBank res; int resKey = -1, resScale = -1;
    float ratio = 1, ratioT = 1, ratioC = 0.01f, shiftT = 0, shiftSm = 0, mBuf[3] = {}, ce = 0, sendLp = 0, sendHp = 0, sendLpC = 0.5f, sendHpC = 0.9f, wph1 = 0, wph2 = 0, share = 0, sg = 0, sgC = 0.001f;
    int runLen = 0, hold = 0, lastTgt = -1;
};

// ======================================================================================== GORGE (latence 0)
class Gorge {
public:
    void prepare(double s) {
        sr = (float)s;
        pt.prepare(s);
        loS.prepare(s, 180.f, true); hiS.prepare(s, 7000.f, false);
        hpLp.lp(s, 52.0);
        vm.prepare(s); ds.prepare(s);
        bHp.hp(s, 150.0); bLp.lp(s, 900.0); sat.reset(); sat.setBias(0.35);
        eF = eS = 0.f; eFc = 1.f - std::exp(-1.f / (0.005f * sr)); eSc = 1.f - std::exp(-1.f / (0.06f * sr));
        // râle
        rLow.lp(s, 1200.0); rTrack.lp(s, 200.0); rNoise.bp(s, 1400.0, 0.6); rTrackHz = 200.f;
        sq = 1.f; sqS = 0.f; sgn = -1; pulse = 0.f; vG = 0.f; envX = 0.f; seed = 0x2545F491u; lfAmp = 0.f;
        pulseC = std::exp(-1.f / (0.0015f * sr)); vGc = 1.f - std::exp(-1.f / (0.02f * sr));
        // slapback calé sur le tempo, filtré
        slap.init((int)(0.32 * s) + 8); sHp.hp(s, 250.0); sLp.lp(s, 3500.0);
        rev.init(s); rev.setTone(0.6f); rev.setDecay(0.9f); sg = 0.f; sgC = 1.f - std::exp(-1.f / (0.15f * sr));
        P = {};
    }
    int latency() const { return 0; }

    void update(const Ctx& c) {
        static const Curve cLow{{0.f, 0.f}, {0.4f, 3.f}, {1.f, 4.f}};
        static const Curve cMix{{0.f, 0.f}, {0.12f, 0.7f}, {0.3f, 1.f}, {1.f, 1.f}};
        static const Curve cThr{{0.f, -12.f}, {0.3f, -22.f}, {0.6f, -28.f}, {1.f, -34.f}};
        static const Curve cCurve{{0.f, 1.5f}, {1.f, 3.f}};
        static const Curve cColor{{0.f, 0.3f}, {1.f, 1.f}};
        static const Curve cGrain{{0.f, 0.f}, {0.3f, 0.f}, {0.55f, 0.6f}, {0.8f, 1.f}, {1.f, 1.f}};
        static const Curve cDrive{{0.f, 4.f}, {0.3f, 6.f}, {0.8f, 14.f}, {1.f, 20.f}};
        static const Curve cRale{{0.f, 0.f}, {0.45f, 0.f}, {0.7f, 0.5f}, {1.f, 1.f}};
        static const Curve cDsRange{{0.f, 0.f}, {0.3f, 6.f}, {1.f, 9.f}};
        static const Curve cDsThr{{0.f, 6.f}, {1.f, 0.f}};
        static const Curve cHi{{0.f, 0.f}, {1.f, -3.f}};
        static const Curve cHp{{0.f, 0.f}, {0.15f, 1.f}};
        static const Curve cSlap{{0.f, 0.f}, {0.55f, 0.f}, {0.75f, 0.14f}, {1.f, 0.25f}};
        static const Curve cRoom{{0.f, 0.f}, {0.4f, 0.f}, {1.f, 0.1f}};
        const float k = c.k;
        loS.setDb(cLow(k)); hiS.setDb(cHi(k));
        P.hp = cHp(k); P.grain = cGrain(k); P.drive = dbToLin(cDrive(k)); P.rale = cRale(k) * clamp01(c.opt);
        P.slap = cSlap(k); P.room = cRoom(k);
        dsp::comp::VariMuParams vp; vp.common.thresholdDb = cThr(k); vp.common.mix = P.mix = cMix(k); vp.common.sidechainHz = 100.f;
        vp.curve = cCurve(k); vp.color = cColor(k);
        // Vitesse (volet) : bas = lent (position 4), 50 % = d'origine (5, double), haut = rapide (1)
        const float s = clamp01(c.spec);
        vp.timeConstant = s < 0.2f ? 4 : s < 0.45f ? 6 : s < 0.65f ? 5 : s < 0.85f ? 2 : 1;
        vm.setParams(vp);
        dsp::deess::Params dp; dp.thresholdDb = cDsThr(k); dp.rangeDb = cDsRange(k); dp.ratio = 3.f; dp.freqHz = 5200.f;
        ds.setParams(dp);
        pt.enabled = P.rale > 0.f;
        // slap : double croche, ou triple croche si elle dépasse 140 ms ; bornée à 60..140 ms
        float t = 15.f / std::max(40.f, c.bpm); if (t > 0.14f) t *= 0.5f;
        P.slapS = std::clamp(t, 0.06f, 0.14f) * sr;
    }

    void process(const Slice& b) {
        for (int i = 0; i < b.n; ++i) {
            const float x = b.x[i];
            if (pt.push(x) && pt.voiced) {
                const float hz = std::clamp(1.3f * pt.f0, 60.f, 500.f); if (std::fabs(hz - rTrackHz) > 2.f) { rTrack.lp(sr, hz, 0.7); rTrackHz = hz; }
                sqC = 1.f - std::exp(-2.f * kPi * std::clamp(pt.f0, 60.f, 260.f) / sr);
            }
            float y = x;
            if (P.hp > 0.f) y -= P.hp * hpLp.run(y); else hpLp.run(y);
            y = loS.run(y);
            vm.process(&y, nullptr, 1);
            // grain dynamique : saturation (ADAA, biais = harmoniques paires) de la bande 150-900 Hz seulement ; on n'ajoute que la
            // DIFFÉRENCE (exact à 0). Le drive suit les syllabes : plus fort sur les attaques et les fins de phrase.
            const float band = bLp.run(bHp.run(y)), ab = std::fabs(band);
            eF += (ab - eF) * eFc; eS += (ab - eS) * eSc;
            if (P.grain > 0.f) {
                const float r = eF / (eS + 1e-6f);
                const float D = P.drive * (1.f + 0.6f * std::clamp(r - 1.f, 0.f, 2.f) + 0.8f * clamp01(1.f - r));
                y += P.grain * (sat.run(band * D) / D - band);
            } else sat.run(0.f);
            // râle : sous-harmonique f0/2 (bascule sur les passages à zéro du fondamental) + souffle synchrone à la période
            const float lf = rTrack.run(y), low = rLow.run(y), ay = std::fabs(y);
            lfAmp += (std::fabs(lf) - lfAmp) * 0.002f; envX += (ay - envX) * (ay > envX ? 0.02f : 0.002f);
            const float h = 0.15f * lfAmp;
            if (sgn < 0 && lf > h) { sgn = 1; sq = -sq; pulse = 1.f; } else if (sgn > 0 && lf < -h) sgn = -1;
            const float on = pt.voiced && pt.f0 < 260.f ? 1.f : 0.f;
            vG += (on - vG) * vGc;
            sqS += (sq - sqS) * sqC;
            pulse *= pulseC;
            seed = seed * 1664525u + 1013904223u;
            const float nz = rNoise.run((float)(seed >> 8) * (1.f / 8388608.f) - 1.f);
            if (P.rale > 0.f) y += P.rale * vG * (0.55f * low * sqS + 0.5f * nz * pulse * envX);
            ds.process(&y, nullptr, 1);
            y = hiS.run(y);
            b.core[i] = y;
            sg += ((b.voice ? 1.f : 0.f) - sg) * sgC;
            slap.push(sLp.run(sHp.run(y * sg)));   // G6 : toujours écrit, seule la lecture dépend du mélange
            float sl = 0.f, srr = 0.f;
            if (P.slap > 0.f) { sl = slap.read(P.slapS) * P.slap; srr = slap.read(P.slapS * 1.13f) * P.slap; }
            float rL, rR; rev.process(y * P.room * sg, 0.6f, rL, rR);
            b.sL[i] = sl + rL; b.sR[i] = srr + rR;
        }
    }
    void settle() { vm.prepare(sr); }
    float effGr() const { return effective(vm.gainReductionDb(), P.mix); }

private:
    struct Pm { float hp = 0, grain = 0, drive = 1, rale = 0, slap = 0, slapS = 4000, room = 0, mix = 0; } P;
    float sr = 48000.f;
    PitchTrack pt; Shelf loS, hiS; Bq hpLp, bHp, bLp, rLow, rTrack, rNoise, sHp, sLp;
    dsp::comp::VariMuComp vm; dsp::deess::DeEsser ds; AdaaTanh sat;
    voile::Delay slap; voile::Reverb rev;
    float sqC = 0.02f, eF = 0, eS = 0, eFc = 0.1f, eSc = 0.01f, rTrackHz = 200.f, sq = 1.f, sqS = 0.f, pulse = 0.f, pulseC = 0.9f, vG = 0.f, vGc = 0.01f, envX = 0.f, lfAmp = 0.f;
    int sgn = -1; uint32_t seed = 1; float sg = 0.f, sgC = 0.001f;
};

// ======================================================================================== POUPÉE
class Poupee {
public:
    void prepare(double s) {
        sr = (float)s;
        latD = std::min(256, (int)std::lround(s * 0.005));
        tune.prepare(s, latD, latD); tune.setQuality(false); tune.reset();
        rp = (int)std::lround(0.006 * s);
        pedal.init(s, rp, false);
        oct.init(0.25 * s, rp, true);  // LPC allégée (ordre ~fs/2800, fenêtre ~6 ms) : enveloppe du double à moindre coût (CPU)
        clip.prepare(s, 2);   // ×2 suffit : l'écrêtage n'entre qu'en fin de course, sur une voix déjà dé-essée en amont
        lat = latD + rp + clip.latency();
        fet.prepare(s); ds.prepare(s);
        hpLp.lp(s, 72.0); hiS.prepare(s, 9000.f, false);
        haas.init((int)(0.012 * s) + 8);
        slap.init((int)(0.2 * s) + 8);
        fr = frT = 0.f; per = 300.f; vS = 0.f; perCnt = 0; P = {};
        vC = 1.f - std::exp(-1.f / (0.004f * sr)); sg = 0.f; sgC = 1.f - std::exp(-1.f / (0.15f * sr)); frC = 1.f - std::exp(-1.f / (0.03f * sr));
    }
    int latency() const { return lat; }

    void update(const Ctx& c) {
        static const Curve cAmt{{0.f, 0.f}, {0.25f, 1.f}, {1.f, 1.f}};
        static const Curve cSpeed{{0.f, 40.f}, {0.15f, 20.f}, {0.35f, 0.f}, {1.f, 0.f}};
        static const Curve cTol{{0.f, 10.f}, {0.3f, 0.f}};
        static const Curve cExpr{{0.f, 1.f}, {0.3f, 0.f}};
        static const Curve cForm{{0.f, 0.f}, {0.3f, 0.f}, {0.7f, 4.f}, {1.f, 4.5f}};
        static const Curve cOct{{0.f, 0.f}, {0.3f, 0.f}, {0.6f, 0.45f}, {1.f, 0.55f}};
        static const Curve cClipMix{{0.f, 0.f}, {0.65f, 0.f}, {0.8f, 1.f}};
        static const Curve cClipDrive{{0.f, 0.f}, {0.65f, 0.f}, {1.f, 15.f}};
        static const Curve cMix{{0.f, 0.f}, {0.1f, 0.6f}, {0.3f, 1.f}};
        static const Curve cThr{{0.f, -14.f}, {0.4f, -24.f}, {0.7f, -30.f}, {1.f, -38.f}};
        static const Curve cAtk{{0.f, 300.f}, {1.f, 80.f}};
        static const Curve cRel{{0.f, 200.f}, {1.f, 100.f}};
        static const Curve cDsRange{{0.f, 0.f}, {0.2f, 6.f}, {1.f, 10.f}};
        static const Curve cDsThr{{0.f, 6.f}, {1.f, -1.f}};
        static const Curve cAir{{0.f, 0.f}, {0.5f, 1.f}, {0.7f, 3.f}, {1.f, 5.f}};
        static const Curve cHp{{0.f, 0.f}, {0.15f, 1.f}};
        static const Curve cSlap{{0.f, 0.f}, {0.5f, 0.f}, {1.f, 0.16f}};
        const float k = c.k, s = clamp01(c.spec);
        // grille : Auto = chromatique (hard-tune immédiat) ; tonique imposée = sa gamme (gamme Auto -> majeur)
        mask = c.keyChoice == 0 ? 0xFFF : maskOf(c.keyChoice - 1, c.scaleChoice == 2 ? 2 : 1);
        keyPc = c.keyChoice - 1; scl = c.scaleChoice == 2 ? 2 : 1;
        frT = cForm(k);
        tc::Params tp; tp.amount = cAmt(k); tp.speedMs = cSpeed(k); tp.tolCents = cTol(k); tp.expr = cExpr(k);
        tp.mask = mask; tp.formants = true; tp.offsetCents = -100.f * fr;
        tune.setParams(tp);
        P.oct = cOct(k) * clamp01(c.opt); P.hp = cHp(k); P.slap = cSlap(k);
        dsp::sat::Params sp; sp.curve = dsp::sat::Curve::Tanh; sp.driveDb = cClipDrive(k); sp.mix = cClipMix(k);
        clip.setParams(sp);
        dsp::comp::FetParams fp; fp.common.thresholdDb = cThr(k); fp.common.mix = P.mix = cMix(k); fp.common.sidechainHz = 90.f;
        fp.ratio = k < 0.4f ? 4 : k < 0.75f ? 8 : 12; fp.attackUs = cAtk(k); fp.releaseMs = cRel(k); fp.color = 0.5f + 0.5f * k;
        fet.setParams(fp);
        // Brillance (volet) : air dynamique +/- autour de la courbe ; sous 50 %, plateau aigu atténué
        dsp::deess::Params dp; dp.thresholdDb = cDsThr(k); dp.rangeDb = cDsRange(k); dp.ratio = 3.f; dp.freqHz = 6000.f;
        dp.airDb = std::clamp(cAir(k) + 8.f * (s - 0.5f), 0.f, 8.f); dp.airHz = 9500.f;
        ds.setParams(dp);
        hiS.setDb(std::min(0.f, 10.f * (s - 0.5f)) * smoothstep(0.f, 0.1f, k));
        P.slapS = std::clamp(7.5f / std::max(40.f, c.bpm), 0.04f, 0.09f) * sr;   // triple croche, 40..90 ms
    }

    void process(const Slice& b) {
        for (int i = 0; i < b.n; ++i) {
            float y = b.x[i];
            if (P.hp > 0.f) y -= P.hp * hpLp.run(y); else hpLp.run(y);
            tune.process(&y, nullptr, 1);
            const auto& st = tune.state();
            // pédale « formants » : seulement sur le voisé (TuneCore n'abaisse la hauteur de fr que là) ; consonnes intactes
            const float vt = st.voiced && st.pitch > 0.f && st.pitch < 83.f ? 1.f : 0.f;   // voix réelle (< ~1 kHz)
            vS += (vt - vS) * vC;
            if (vt > 0.f && --perCnt <= 0) {
                perCnt = 64;
                const float m = st.pitch + tune.appliedShift() - fr;   // hauteur à l'entrée de la pédale
                per = sr / (440.f * std::exp2((m - 69.f) / 12.f));
                pedal.setPeriod(per); oct.setPeriod(per / std::exp2(fr / 12.f));
            }
            fr += (frT - fr) * frC;
            y = pedal.process(y, std::exp2(fr * vS / 12.f));
            // octave en double (formants resserrés) : sur la voix déjà « poupée », +12 dt à formants préservés, voisé seulement
            sg += ((b.voice ? 1.f : 0.f) - sg) * sgC;
            const float o = P.oct > 0.f ? oct.process(y, 2.f) * P.oct * vS * sg : 0.f;
            y = clip.processSample(y);
            fet.process(&y, nullptr, 1);
            ds.process(&y, nullptr, 1);
            y = hiS.run(y);
            b.core[i] = y;
            haas.push(o);
            slap.push(y * sg);
            const float sl = P.slap > 0.f ? slap.read(P.slapS) * P.slap : 0.f;
            b.sL[i] = o + sl;
            b.sR[i] = 0.8f * haas.read(0.007f * sr) + sl * 0.9f;
        }
    }
    void settle() { fet.prepare(sr); fr = frT; }
    float effGr() const { return effective(fet.gainReductionDb(), P.mix); }
    int keyPc = -1, scl = 1, mask = 0xFFF;

private:
    struct Pm { float oct = 0, hp = 0, slap = 0, slapS = 3000, room = 0, mix = 0; } P;
    float sr = 48000.f, fr = 0.f, frT = 0.f, per = 300.f, vS = 0.f, vC = 0.01f, frC = 0.001f, sg = 0.f, sgC = 0.001f; int perCnt = 0;
    int latD = 240, rp = 288, lat = 0;
    tc::TuneCore tune; dsp::VoiceShifter pedal, oct; dsp::sat::Saturator clip;
    dsp::comp::FetComp fet; dsp::deess::DeEsser ds; Bq hpLp; Shelf hiS;
    voile::Delay haas, slap;
};

// ======================================================================================== PATINE (latence 0)
class Patine {
public:
    void prepare(double s) {
        sr = (float)s;
        hpLp.lp(s, 46.0);
        preLp.lp(s, 2500.0, 0.54); preLp2.lp(s, 2500.0, 1.31); tri.setBias(0.3); tri.reset(); emph.prepare(s, 140.f, true);
        opto.prepare(s); dens.prepare(s); ds.prepare(s);
        for (int i = 0; i < kB; ++i) { ana[i].bp(s, kHz[i], 1.4); eq[i].f = Bq{}; eq[i].cur = 0.f; pw[i] = 1e-4f; gDb[i] = 0.f; }
        pwC = 1.f - std::exp(-1.f / (2.5f * sr));
        P = {};
    }
    int latency() const { return 0; }

    void settle() { opto.prepare(sr); dens.prepare(sr); }
    float effGr() const { return std::max(effective(opto.gainReductionDb(), P.optoMix), effective(dens.gainReductionDb(), P.densMix)); }
    void update(const Ctx& c) {
        static const Curve cHp{{0.f, 0.f}, {0.15f, 1.f}};
        static const Curve cPre{{0.f, 0.f}, {0.15f, 1.f}};
        static const Curve cDrive{{0.f, 0.f}, {0.3f, 3.f}, {0.7f, 9.f}, {1.f, 14.f}};
        static const Curve cOptoMix{{0.f, 0.f}, {0.1f, 0.7f}, {0.25f, 1.f}};
        static const Curve cOptoThr{{0.f, -14.f}, {0.5f, -28.f}, {1.f, -33.f}};
        static const Curve cDens{{0.f, 0.f}, {0.3f, 0.f}, {0.6f, 0.25f}, {0.8f, 0.4f}, {1.f, 0.45f}};
        static const Curve cDsRange{{0.f, 0.f}, {0.3f, 6.f}, {1.f, 8.f}};
        static const Curve cDsThr{{0.f, 6.f}, {1.f, 1.f}};
        static const Curve cAir{{0.f, 0.f}, {0.5f, 0.f}, {0.8f, 3.f}, {1.f, 5.f}};
        static const Curve cTarget{{0.f, 0.f}, {0.3f, 0.4f}, {0.7f, 0.8f}, {1.f, 1.f}};
        const float k = c.k;
        P.hp = cHp(k); P.pre = cPre(k);
        P.drive = dbToLin(cDrive(k) + 18.f * clamp01(c.spec));   // Drive (volet) : jusqu'à +18 dB d'attaque
        emph.setDb(3.f * P.pre);                                   // flux du transformateur : le grave sature d'abord
        dsp::comp::OptoParams op; op.common.thresholdDb = cOptoThr(k); op.common.mix = P.optoMix = cOptoMix(k); op.common.sidechainHz = 90.f;
        opto.setParams(op);
        dsp::comp::FetParams fp; fp.common.thresholdDb = -30.f; fp.common.mix = P.densMix = cDens(k); fp.ratio = 8; fp.attackUs = 100.f; fp.releaseMs = 150.f; fp.color = 1.f;
        dens.setParams(fp);
        dsp::deess::Params dp; dp.thresholdDb = cDsThr(k); dp.rangeDb = cDsRange(k); dp.ratio = 3.f; dp.freqHz = 5500.f; dp.airDb = cAir(k); dp.airHz = 10000.f;
        ds.setParams(dp);
        P.target = cTarget(k) * clamp01(c.opt);
        // courbe cible : gains (bornés à ±6 dB, 60 % de l'écart) ; vitesse <= 1 dB/s ; filtres recalculés par pas de 0,0005 dB
        // (des pas de 0,02 dB toutes les 20 ms modulaient le signal : raies à 50 Hz d'écart, mesurées à -90 dB)
        float ref = 0.f, refT = 0.f;
        for (int i = 2; i <= 4; ++i) { ref += 10.f * std::log10(pw[i] + 1e-12f); refT += kTarget[i]; }
        ref /= 3.f; refT /= 3.f;
        const float dt = 32.f / sr;
        for (int i = 0; i < kB; ++i) {
            const float meas = 10.f * std::log10(pw[i] + 1e-12f) - ref, want = std::clamp(0.6f * ((kTarget[i] - refT) - meas), -6.f, 6.f) * P.target;
            gDb[i] += std::clamp(want - gDb[i], -1.f * dt, 1.f * dt);
            if (P.target <= 0.f && std::fabs(gDb[i]) < 0.03f) gDb[i] = 0.f;
            if (std::fabs(gDb[i] - eq[i].cur) > 0.0005f || (gDb[i] == 0.f && eq[i].cur != 0.f)) { eq[i].cur = gDb[i]; eq[i].f.peak(sr, kHz[i], 1.0, gDb[i]); }
        }
    }

    void process(const Slice& b) {
        float a[32];
        const int n = b.n;
        for (int i = 0; i < n; ++i) {
            float y = b.x[i];
            if (P.hp > 0.f) y -= P.hp * hpLp.run(y); else hpLp.run(y);
            // préampli « lampe » : courbe lisse asymétrique (tanh à biais = 2e harmonique dominante, comme une triode), ADAA, sur la voix sous 2,5 kHz avec le grave mis en avant ; seule la DIFFÉRENCE est ajoutée (exact à 0)
            const float lo = emph.run(preLp2.run(preLp.run(y)));
            if (P.pre > 0.f) y += P.pre * (tri.run(lo * P.drive) / P.drive - lo); else tri.run(0.f);
            a[i] = y;
        }
        opto.process(a, nullptr, n);
        dens.process(a, nullptr, n);
        ds.process(a, nullptr, n);
        for (int i = 0; i < n; ++i) {
            float y = a[i];
            if (b.voice) for (int j = 0; j < kB; ++j) { const float v = ana[j].run(y); pw[j] += (v * v - pw[j]) * pwC; }
            else for (int j = 0; j < kB; ++j) ana[j].run(y);
            for (int j = 0; j < kB; ++j) if (eq[j].cur != 0.f) y = eq[j].f.run(y);
            b.core[i] = y; b.sL[i] = b.sR[i] = 0.f;
        }
    }

private:
    static constexpr int kB = 8;
    static constexpr float kHz[kB] = {120.f, 250.f, 500.f, 1000.f, 2000.f, 3500.f, 6000.f, 10000.f};
    // Cible « voix pop finie » (dB par bande à Q constant, relatif au médium). HYPOTHÈSE : pente douce au-dessus de 1 kHz, grave tenu.
    static constexpr float kTarget[kB] = {-10.f, -4.f, -1.f, 0.f, -1.f, -2.5f, -5.f, -8.f};
    struct Pm { float hp = 0, pre = 0, drive = 1, target = 0, optoMix = 0, densMix = 0; } P;
    struct Eq { Bq f; float cur = 0.f; } eq[kB];
    float sr = 48000.f, pw[kB] = {}, gDb[kB] = {}, pwC = 1e-5f;
    Bq hpLp, preLp, preLp2, ana[kB]; Shelf emph; AdaaTanh tri;
    dsp::comp::OptoComp opto; dsp::comp::FetComp dens; dsp::deess::DeEsser ds;
};

}  // namespace onek
