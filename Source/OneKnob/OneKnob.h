// Plugins « un seul potard » : un potard CARACTÈRE (0–10) pilote toute une chaîne vocale. C++ pur (sans JUCE), sans allocation dans
// process(). Deux moteurs :
//  - ACTUEL (défaut des nouvelles instances) : un modèle par plugin (OneKnobModels.h), courbes de macro à points d'arrêt, dynamique
//    AVANT l'espace, bus d'espace parallèle, latence propre au modèle (0 pour Gorge et Patine).
//  - CLASSIQUE (ClassicChain.h) : l'ancien son, pour les projets enregistrés avant la version d'état 2.
// Chaîne actuelle : entrée (NaN/inf -> 0) -> milieu/côté -> trim -> détecteur de voix + auto-entrée (figée hors voix) -> modèle (mono)
// -> gain d'entrée rendu, sonie étalonnée + auto-gain fin (±3 dB, sur la voix) -> ton -> porte de pause -> côté (s'efface quand on
// tourne) -> limiteur de crête (inactif sous -1 dBFS) -> mélange avec le sec retardé -> sortie.
// À k = 0 : sortie = entrée retardée de la latence déclarée (aucune reverb, saturation ni compression résiduelle).
#pragma once
#include <algorithm>
#include <cmath>
#include <vector>
#include "OneKnobShared.h"
#include "OneKnobDsp.h"
#include "OneKnobModels.h"
#include "ClassicChain.h"
#include "../Kit/Sanitize.h"

namespace onek {

// Briques historiques réutilisées par d'autres plugins de la suite (OrbeChain, Cabine, Master, Synth, Sampler, 808, Beat) :
// mêmes classes, au caractère près, que l'ancien OneKnob.h (copiées dans ClassicChain.h). Ne pas modifier : elles font leur son.
using Biquad = classic::Biquad;
using Comp = classic::Comp;
using DeEss = classic::DeEss;
using TanhAdaa = classic::TanhAdaa;
using classic::lerp;

struct Voicing {
    const char* id;        // identifiant technique
    const char* name;      // nom affiché
    const char* tag;       // ligne de description gravée
    const char* serial;    // numéro de modèle gravé
    float defaultK;        // réglage initial
    const char* specName;  // réglage spécifique du volet avancé
    const char* specTip;
    float specDefault;     // 0..1
    const char* optName;   // option propre au modèle (paramètre « opt »)
    const char* optTip;
    float optDefault;      // 0 ou 1
    bool pitch;            // le modèle corrige la hauteur (menus Tonalité / Gamme)
};

inline const std::vector<Voicing>& voicings() {
    static const std::vector<Voicing> v = {
        {"grainnord", "Grain Nord", "Retune, doubles, brume tonale", "DW-501", 0.45f,
         "Largeur", "Largeur stéréo des doubles et de la brume : 0 = mono, 50 = d'origine, 100 = très large.", 0.5f,
         "Brume tonale", "Brume tonale : la queue de la reverb est filtrée par des résonateurs accordés sur la gamme détectée (elle « chante » la tonalité). Sans tonalité fiable : reverb normale.", 1.f, true},
        {"gorge", "Gorge", "Lampes, grain, râle, slapback", "DW-502", 0.50f,
         "Vitesse", "Vitesse du compresseur à lampes : bas = lent et doux, 50 = d'origine (relâche double), haut = rapide.", 0.5f,
         "Râle", "Râle : sous-harmonique et souffle synchrones à la voix, sur les syllabes graves voisées (fin de course). Sans latence.", 1.f, false},
        {"poupee", "Poupée", "Hard-tune, formants, octave, brillance", "DW-503", 0.50f,
         "Brillance", "Brillance : air dynamique (contrôlé par le de-esser) autour du réglage d'origine ; sous 50 %, aigus atténués.", 0.5f,
         "Octave", "Octave : double à l'octave supérieure, formants resserrés (pas de voix de souris), au milieu de la course.", 1.f, true},
        {"patine", "Patine", "Préampli, opto, air, courbe cible", "DW-504", 0.45f,
         "Drive", "Drive : attaque du préampli à triode (0 à +18 dB, niveau compensé).", 0.f,
         "Cible", "Cible : égaliseur lent à 8 bandes qui rapproche le timbre moyen d'une courbe « voix pop finie » (le potard dose la distance).", 1.f, false},
    };
    return v;
}
inline int indexOf(const Voicing& v) { const auto& a = voicings(); const int i = (int)(&v - a.data()); return i >= 0 && i < (int)a.size() ? i : 0; }

// Valeur lissée (≈ 20 ms), saisie directe à la première consigne.
struct Glide {
    float v = 0.f, t = 0.f, c = 0.001f; bool primed = false;
    void prepare(float sr) { c = 1.f / (0.02f * sr); primed = false; }
    void set(float x) { t = x; if (!primed) { v = x; primed = true; } }
    float next() { v += (t - v) * c; if (std::fabs(t - v) < 1e-7f) v = t; return v; }
};

class Chain {
public:
    static constexpr int kSub = 32;
    void prepare(double sampleRate, int maxBlock) {
        sr = (float)sampleRate;
        maxN = std::max(1, maxBlock);
        for (auto* v : {&inL, &inR, &xin, &core, &spL, &spR}) v->assign((size_t)maxN + kSub, 0.f);
        (void)classic::voicings(); (void)voicings(); (void)dsp::sat::TriodeTable::get();   // tables statiques construites hors temps réel
        classic.prepare(sampleRate, maxN);
        gn.prepare(sampleRate); go.prepare(sampleRate); pu.prepare(sampleRate); pa.prepare(sampleRate);
        int maxLat = classic.latency();
        for (int m = 0; m < 4; ++m) maxLat = std::max(maxLat, modelLatency(m));
        dryL.init(maxLat + maxN + 8); dryR.init(maxLat + maxN + 8); side.init(maxLat + 8); gLine.init(maxLat + 8);
        curModel = wantModel; curClassic = wantClassic;
        resetCommon();
        primedRun = false;
    }
    void select(int model) { wantModel = std::clamp(model, 0, 3); if (!primedRun) curModel = wantModel; }
    void setClassic(bool c) { wantClassic = c; if (!primedRun) curClassic = c; }
    bool classicActive() const { return curClassic; }
    int latency() const { return wantClassic ? classic.latency() : modelLatency(wantModel); }
    int maxLatency() const { return std::max(classic.latency(), modelLatency(wantModel)); }   // pour dimensionner les retards de l'hôte
    void setBpm(double b) { bpm = (float)(b > 20.0 && b < 999.0 ? b : 120.0); classic.setBpm(bpm); }
    float outLevel() const { return peakOut; }
    bool keyKnown() const {
        if (curClassic) return classic.keyKnown();
        if (curModel == 0) return gn.keys().haveKey;
        if (curModel == 2) return keyChoice_ > 0;
        return false;
    }
    int key() const { return curClassic ? classic.key() : curModel == 0 ? gn.keys().effKey : std::max(0, keyChoice_ - 1); }
    int scale() const { return curClassic ? classic.scale() : curModel == 0 ? gn.keys().effScale : (scaleChoice_ == 2 ? 2 : 1); }
    int keyStatus() const { return curClassic ? classic.keyStatus() : curModel == 0 ? gn.keys().status() : 0; }   // 0 imposée/chromatique, 1 auto verrouillée, 2 recherche
    void setKeySel(int keyChoice, int scaleChoice) { keyChoice_ = std::clamp(keyChoice, 0, 12); scaleChoice_ = std::clamp(scaleChoice, 0, 2); classic.setKeySel(keyChoice_, scaleChoice_); }
    float gainReduction() const { return curClassic ? classic.gainReduction() : grNow; }
    float meanReduction() const { return curClassic ? classic.meanReduction() : grAvg; }
    float inputDb() const { return curClassic ? classic.inputDb() : inDb; }
    float autoInGainDb() const { return curClassic ? classic.autoInGainDb() : vd.gDb; }
    bool voiceActive() const { return vd.active; }

    void setExtras(const Extras& e) {
        X = e;
        trimG.set(std::pow(10.f, std::clamp(e.trimDb, -12.f, 12.f) / 20.f));
        mixG.set(std::clamp(e.mix, 0.f, 1.f));
        outG.set(std::pow(10.f, std::clamp(e.outDb, -12.f, 12.f) / 20.f));
        classic.setExtras(e);
    }
    void setKnob(float k, const Voicing& v) {
        select(indexOf(v));
        kT = std::clamp(k, 0.f, 1.f);
        if (wantClassic) classic.setKnob(kT, classic::voicings()[(size_t)wantModel]);
    }

    // Entrée mono (inR nul) ou stéréo ; sortie stéréo. Peut être appelée en place. n <= maxBlock (au-delà : découpé).
    void process(const float* inLp, const float* inRp, float* outL, float* outR, int n) {
        for (int p = 0; p < n; p += maxN) {
            const int m = std::min(maxN, n - p);
            processChunk(inLp + p, inRp ? inRp + p : nullptr, outL + p, outR + p, m);
        }
    }

private:
    int modelLatency(int m) const { return m == 0 ? gn.latency() : m == 1 ? go.latency() : m == 2 ? pu.latency() : pa.latency(); }
    void resetCommon() {
        vd.prepare(sr); gate.prepare(sr); lim.prepare(sr);
        trimG.prepare(sr); mixG.prepare(sr); outG.prepare(sr);
        tiltL.prepare(sr, 300.f, true); tiltH.prepare(sr, 3000.f, false);
        kCur = kT; kPrimed = false; pIn = pOut = 1e-6f; agDb = 0.f; post = 1.f; phase = 0; pStep = 0.f; voiceNow = false;
        kSmoothC = 1.f - std::exp(-(float)kSub / (0.05f * sr));
        grAvg = grNow = 0.f; inDb = -100.f; peakOut = 0.f; fade = 1.f; fadeDir = 0;
        std::fill(dryL.b.begin(), dryL.b.end(), 0.f); std::fill(dryR.b.begin(), dryR.b.end(), 0.f);
        std::fill(side.b.begin(), side.b.end(), 0.f); std::fill(gLine.b.begin(), gLine.b.end(), 1.f);
    }
    // Réinitialise le moteur entrant (aucune allocation : mêmes tailles qu'au premier prepare).
    void switchEngine() {
        curModel = wantModel; curClassic = wantClassic;
        if (curClassic) classic.prepare(sr, maxN);
        else if (curModel == 0) gn.prepare(sr); else if (curModel == 1) go.prepare(sr); else if (curModel == 2) pu.prepare(sr); else pa.prepare(sr);
        const bool prim = mixG.primed; resetCommon(); if (prim) setExtras(X);
        if (curClassic) classic.setKnob(kT, classic::voicings()[(size_t)curModel]);
    }
    // Sonie étalonnée hors ligne (dB ajoutés selon k, mesurée sur une prise rap et une voix chantée de synthèse) : le niveau de la voix
    // ne bouge plus quand on tourne le potard ; l'auto-gain fin ne corrige que le reste (±3 dB).
    float loudnessDb(float k) const {
        static const Curve c0{{0.f, 0.f}, {0.25f, -1.1f}, {0.5f, -1.6f}, {0.75f, -1.5f}, {1.f, -0.9f}};
        static const Curve c1{{0.f, 0.f}, {0.25f, -0.5f}, {0.5f, -0.5f}, {0.75f, -0.6f}, {1.f, -0.9f}};
        static const Curve c2{{0.f, 0.f}, {0.25f, 4.1f}, {0.5f, 4.6f}, {0.75f, 3.8f}, {1.f, 2.4f}};
        static const Curve c3{{0.f, 0.f}, {0.25f, 0.3f}, {0.5f, 1.4f}, {0.75f, 3.f}, {1.f, 3.8f}};
        const Curve* t[4] = {&c0, &c1, &c2, &c3};
        return (*t[curModel])(k);
    }

    void processChunk(const float* inLp, const float* inRp, float* outL, float* outR, int n) {
        if (!mixG.primed) setExtras(X);   // première consigne après prepare() : saisie sans glissement
        const bool wantSwitch = wantClassic != curClassic || (wantModel != curModel);
        if (wantSwitch && !primedRun) switchEngine();
        primedRun = true;
        // copie nettoyée de l'entrée (G2 : un NaN/inf d'un plugin précédent ne doit rien empoisonner)
        for (int i = 0; i < n; ++i) { inL[(size_t)i] = inLp[i]; inR[(size_t)i] = inRp ? inRp[i] : inLp[i]; }
        kit::sanitize(inL.data(), n); kit::sanitize(inR.data(), n);
        const bool stereo = inRp != nullptr;
        if (curClassic) classic.process(inL.data(), stereo ? inR.data() : nullptr, outL, outR, n);
        else processModel(n, stereo, outL, outR);
        // bascule de moteur ou de modèle : fondu de sortie (5 ms), puis le moteur entrant repart à zéro avec un fondu d'entrée
        if (wantSwitch && primedRun) {
            const float step = 1.f / (0.005f * sr);
            for (int i = 0; i < n; ++i) { fade = std::max(0.f, fade - step); outL[i] *= fade; outR[i] *= fade; }
            if (fade <= 0.f) { switchEngine(); fade = 0.f; fadeDir = 1; }
        } else if (fadeDir > 0) {
            const float step = 1.f / (0.005f * sr);
            for (int i = 0; i < n; ++i) { fade = std::min(1.f, fade + step); outL[i] *= fade; outR[i] *= fade; }
            if (fade >= 1.f) fadeDir = 0;
        }
        peakOut = 0.f;
        for (int i = 0; i < n; ++i) peakOut = std::max(peakOut, std::max(std::fabs(outL[i]), std::fabs(outR[i])));
    }

    // Tranches de kSub échantillons calées sur l'index ABSOLU (et non sur le début du bloc de l'hôte) : les réglages sont recalculés
    // aux mêmes instants quelle que soit la taille des blocs (1 à 8192), donc la sortie ne dépend pas du découpage de l'hôte.
    void beginSub() {
        kCur += (kT - kCur) * kSmoothC;   // potard lissé (~50 ms) : aucune marche, même quand l'hôte saute de 0 à 10
        if (std::fabs(kT - kCur) < 1e-5f) kCur = kT;
        Ctx c; c.k = kCur; c.spec = X.special < 0.f ? voicings()[(size_t)curModel].specDefault : X.special;
        c.opt = X.option < 0.f ? voicings()[(size_t)curModel].optDefault : X.option;
        c.bpm = bpm; c.keyChoice = keyChoice_; c.scaleChoice = scaleChoice_;
        if (curModel == 0) gn.update(c); else if (curModel == 1) go.update(c); else if (curModel == 2) pu.update(c); else pa.update(c);
        fadeK = smoothstep(0.f, 0.1f, kCur); sideG = 1.f - smoothstep(0.f, 0.2f, kCur);
        tiltL.setDb(-std::clamp(X.toneDb, -6.f, 6.f)); tiltH.setDb(std::clamp(X.toneDb, -6.f, 6.f));
        voiceNow = vd.active;
        // sonie : table étalonnée + auto-gain fin mesuré sur la voix seulement (mesure avant correction : pas de boucle)
        const float base = X.autoGain ? loudnessDb(kCur) : 0.f;
        baseLin = dbToLin(base * fadeK);
        pStep = (dbToLin((base + agDb) * fadeK) - post) / (float)kSub;
        pI = pO = 0.f;
    }
    void endSub() {
        vd.update(kSub, X.autoIn);
        grNow = curModel == 0 ? gn.effGr() : curModel == 1 ? go.effGr() : curModel == 2 ? pu.effGr() : pa.effGr();
        grS += grNow; ++nSub;
        if (vd.open && X.autoGain) {
            const float c = 1.f - std::exp(-(float)kSub / (3.f * sr));
            pIn += (pI / (float)kSub - pIn) * c; pOut += (pO / (float)kSub - pOut) * c;
            const float want = std::clamp(10.f * std::log10((pIn + 1e-12f) / (pOut + 1e-12f)), -3.f, 3.f);
            agDb += std::clamp(want - agDb, -(float)kSub / sr, (float)kSub / sr);
        } else if (!X.autoGain) agDb *= 0.99f;
    }
    void processModel(int n, bool stereo, float* outL, float* outR) {
        const float fL = (float)modelLatency(curModel);
        float inSum = 0.f; grS = 0.f; nSub = 0;
        if (!kPrimed) {   // premier bloc : réglages saisis sans glissement, compresseurs calés sur leur mélange (k = 0 exactement neutre)
            kCur = kT; kPrimed = true; phase = 0; beginSub();
            if (curModel == 0) gn.settle(); else if (curModel == 1) go.settle(); else if (curModel == 2) pu.settle(); else pa.settle();
            phase = 0; post = dbToLin(0.f); kCur = kT; beginSub(); post += 0.f; phase = -1;
        }
        for (int p = 0; p < n;) {
            if (phase < 0) phase = 0; else if (phase == 0) beginSub();
            const int m = std::min(kSub - phase, n - p);
            for (int i = 0; i < m; ++i) {
                const float xl = inL[(size_t)(p + i)], xr = inR[(size_t)(p + i)];
                dryL.push(xl); dryR.push(xr);
                const float mid = stereo ? 0.5f * (xl + xr) : xl, sd = stereo ? 0.5f * (xl - xr) : 0.f;
                const float t = trimG.next();
                side.push(sd * t);
                const float x = mid * t;
                inSum += x * x;
                vd.accumulate(x);
                const float g = vd.nextGain();
                gLine.push(g);
                xin[(size_t)i] = x * g;
            }
            Slice s{xin.data(), core.data(), spL.data(), spR.data(), m, voiceNow};
            if (curModel == 0) gn.process(s); else if (curModel == 1) go.process(s); else if (curModel == 2) pu.process(s); else pa.process(s);
            for (int i = 0; i < m; ++i) {
                post += pStep;
                const float back = fL + (float)(m - 1 - i);
                const float inv = 1.f / std::max(1e-6f, gLine.read(back));
                const float dl = dryL.read(back), dr = dryR.read(back);
                const float ref = (stereo ? 0.5f * (dl + dr) : dl) * trimG.v;
                const float c0 = tiltH.run(tiltL.run(core[(size_t)i] * inv));
                const float s0l = spL[(size_t)i] * inv, s0r = spR[(size_t)i] * inv;
                const float mo = (c0 + 0.5f * (s0l + s0r)) * baseLin;
                pI += ref * ref; pO += mo * mo;
                const float gg = gate.step(ref, c0 * post, voiceNow || fadeK <= 0.f);
                const float cg = c0 * post * (1.f - fadeK * (1.f - gg));
                const float sdd = side.read(back) * sideG;
                float wl = cg + s0l * post + sdd, wr = cg + s0r * post - sdd;
                lim.run(wl, wr);
                const float mx = mixG.next(), og = outG.next();
                outL[p + i] = (dl + (wl - dl) * mx) * og;
                outR[p + i] = (dr + (wr - dr) * mx) * og;
            }
            p += m; phase += m;
            if (phase >= kSub) { phase = 0; endSub(); }
        }
        inDb = 10.f * std::log10(inSum / (float)std::max(1, n) + 1e-10f);
        if (nSub > 0) grAvg = grS / (float)nSub;
    }

    float sr = 48000.f, bpm = 120.f, kT = 0.f, kCur = 0.f, peakOut = 0.f, grAvg = 0.f, grNow = 0.f, inDb = -100.f;
    float pIn = 1e-6f, pOut = 1e-6f, agDb = 0.f, post = 1.f, fade = 1.f, kSmoothC = 0.01f, fadeK = 0.f, sideG = 1.f, baseLin = 1.f, pStep = 0.f, pI = 0.f, pO = 0.f, grS = 0.f;
    int nSub = 0, phase = 0;
    bool voiceNow = false;
    int maxN = 512, keyChoice_ = 0, scaleChoice_ = 0, wantModel = 0, curModel = 0, fadeDir = 0;
    bool wantClassic = false, curClassic = false, primedRun = false, kPrimed = false;
    Extras X;
    classic::Chain classic;
    GrainNord gn; Gorge go; Poupee pu; Patine pa;
    VoiceDetector vd; PauseGate gate; PeakLimiter lim;
    Glide trimG, mixG, outG;
    Shelf tiltL, tiltH;
    voile::Delay dryL, dryR, side, gLine;
    std::vector<float> inL, inR, xin, core, spL, spR;
};

}  // namespace onek
