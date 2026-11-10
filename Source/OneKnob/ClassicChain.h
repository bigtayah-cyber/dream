// Chaîne « Classique » des plugins un-potard : copie de l'ancienne onek::Chain (OneKnob.h jusqu'au commit 437bc8e), sur le moteur
// figé onek_classic::Engine. Rechargée pour les états sans numéro de version (anciens projets) : même son qu'à l'enregistrement.
// Seuls écarts voulus : G6 (le tampon du slapback est toujours écrit) ; l'entrée est nettoyée des NaN/inf en amont (Chain).
#pragma once
#include <algorithm>
#include <cmath>
#include <vector>
#include "ClassicEngine.h"
#include "OneKnobShared.h"

namespace onek {
namespace classic {

constexpr float kPi = 3.14159265358979f;

// Filtre biquad RBJ.
struct Biquad {
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void reset() { z1 = z2 = 0; }
    void set(float nb0, float nb1, float nb2, float na0, float na1, float na2) { b0 = nb0 / na0; b1 = nb1 / na0; b2 = nb2 / na0; a1 = na1 / na0; a2 = na2 / na0; }
    void highpass(float sr, float f, float q = 0.707f) { const float w = 2 * kPi * f / sr, c = std::cos(w), a = std::sin(w) / (2 * q); set((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + a, -2 * c, 1 - a); }
    void lowpass(float sr, float f, float q = 0.707f) { const float w = 2 * kPi * f / sr, c = std::cos(w), a = std::sin(w) / (2 * q); set((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + a, -2 * c, 1 - a); }
    void lowShelf(float sr, float f, float db) {
        const float A = std::pow(10.f, db / 40.f), w = 2 * kPi * f / sr, c = std::cos(w), s = std::sin(w), al = s / 2 * std::sqrt(2.f), t = 2 * std::sqrt(A) * al;
        set(A * ((A + 1) - (A - 1) * c + t), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - t), (A + 1) + (A - 1) * c + t, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - t);
    }
    void highShelf(float sr, float f, float db) {
        const float A = std::pow(10.f, db / 40.f), w = 2 * kPi * f / sr, c = std::cos(w), s = std::sin(w), al = s / 2 * std::sqrt(2.f), t = 2 * std::sqrt(A) * al;
        set(A * ((A + 1) + (A - 1) * c + t), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - t), (A + 1) - (A - 1) * c + t, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - t);
    }
    float process(float x) { const float y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
};

// Compresseur feed-forward, détection crête liée stéréo, genou doux.
struct Comp {
    float thr = -18.f, ratio = 3.f, makeup = 0.f, env = 0.f, aC = 0.f, rC = 0.f, gr = 0.f;
    void prepare(float sr, float attackMs = 6.f, float releaseMs = 120.f) { setTimes(sr, attackMs, releaseMs); env = 0; gr = 0; }
    void setTimes(float sr, float attackMs, float releaseMs) { aC = std::exp(-1.f / (attackMs * 1e-3f * sr)); rC = std::exp(-1.f / (releaseMs * 1e-3f * sr)); }
    float gain(float l, float r) {
        const float a = std::max(std::fabs(l), std::fabs(r));
        env = a > env ? aC * env + (1 - aC) * a : rC * env + (1 - rC) * a;
        const float db = 20.f * std::log10(env + 1e-9f), knee = 6.f, over = db - thr;
        float red = 0.f;
        if (over > knee / 2) red = over * (1.f - 1.f / ratio);
        else if (over > -knee / 2) red = (1.f - 1.f / ratio) * (over + knee / 2) * (over + knee / 2) / (2 * knee);
        gr = red;
        return std::pow(10.f, (-red + makeup) / 20.f);
    }
};

// De-esser : atténue la bande haute quand elle dépasse un seuil.
struct DeEss {
    Biquad hp; float env = 0.f, aC = 0.f, rC = 0.f, thr = 0.05f, amt = 0.f;
    void prepare(float sr) { hp.highpass(sr, 5500.f, 0.8f); hp.reset(); aC = std::exp(-1.f / (0.0007f * sr)); rC = std::exp(-1.f / (0.03f * sr)); env = 0; }
    float process(float x) {
        const float hf = hp.process(x), lf = x - hf, a = std::fabs(hf);
        env = a > env ? aC * env + (1 - aC) * a : rC * env + (1 - rC) * a;
        const float g = env > thr ? std::pow(thr / env, 0.6f * amt) : 1.f;
        return lf + hf * g;
    }
};

// tanh antidérivée (ADAA d'ordre 1) : repliement réduit d'environ 13 dB sans latence entière (demi-échantillon de retard de groupe).
// Calcul en double : la différence de deux log(cosh) voisins perd toute précision en float.
struct TanhAdaa {
    double xp = 0.0, fp = 0.0;
    static double lcosh(double x) {
        x = std::fabs(x);
        if (x < 0.5) { const double x2 = x * x; return x2 * (0.5 - x2 * (1.0 / 12.0 - x2 * (1.0 / 45.0 - x2 * (17.0 / 2520.0)))); }
        return x + std::log1p(std::exp(-2.0 * x)) - 0.6931471805599453;
    }
    void reset() { xp = 0.0; fp = 0.0; }
    float process(float xf) {
        const double x = xf, f = lcosh(x), dx = x - xp;
        const double y = std::fabs(dx) > 1e-6 ? (f - fp) / dx : std::tanh(0.5 * (x + xp));
        xp = x; fp = f;
        return (float)y;
    }
};

// Réglages dérivés du potard.
struct Mapped {
    onek_classic::Params eng;       // moteur : tension, mordant, brume, halo
    float hpHz = 20.f, lpHz = 20000.f;
    float lowDb = 0.f, highDb = 0.f;         // plateaux grave (180 Hz) et aigu (9 kHz)
    float comp = 0.f;                        // 0..1 : intensité de compression
    float deess = 0.f;                       // 0..1
    float slapMs = 0.f, slapMix = 0.f;       // écho court
    float lofi = 0.f;                        // 0..1 : réduction de bande + grain
    float level = 1.f;                       // 0..1 : part du son traité (1 = tout)
    // réglage spécifique du volet avancé (voir Voicing::spec)
    float width = 1.f;                       // largeur stéréo des doubles (0 mono .. 2)
    float atkMs = 6.f, relMs = 120.f;        // temps du compresseur
    float driveDb = 0.f;                     // attaque de la saturation d'entrée
};

struct Voicing {
    const char* id;        // identifiant technique
    const char* name;      // nom affiché
    const char* tag;       // ligne de description gravée
    const char* serial;    // numéro de modèle gravé
    float defaultK;        // réglage initial
    void (*map)(float k, Mapped&);
    const char* specName;  // réglage spécifique du volet avancé
    const char* specTip;   // infobulle
    float specDefault;     // 0..1
    void (*spec)(float s, Mapped&);
};

inline float lerp(float a, float b, float t) { return a + (b - a) * t; }

// ---- Les quatre voicings. k va de 0 (presque transparent) à 1. Hypothèses d'écoute, pas des mesures. ----

// Rap mélancolique : retune moyen puis audible, doubles, reverb sombre, lo-fi en fin de course.
inline void mapGrainNord(float k, Mapped& m) {
    m.eng.tension = 0.55f * k + 0.15f * k * k;
    m.eng.mordant = 0.05f + 0.30f * k;
    m.eng.brume = 0.08f + 0.62f * k;
    m.eng.halo = 0.10f + 0.40f * k;
    m.eng.deadCents = 14.f - 9.f * k;                // peu de zone morte quand on pousse
    m.eng.retuneMs = lerp(140.f, 30.f, k);           // correction lente puis franche
    m.comp = 0.25f + 0.35f * k;
    m.highDb = -1.f - 4.f * k;                       // reverb et ton sombres
    m.lpHz = k < 0.6f ? 19000.f : lerp(19000.f, 7500.f, (k - 0.6f) / 0.4f);
    m.lofi = std::max(0.f, (k - 0.6f) / 0.4f);
    m.hpHz = 45.f;
}
// Voix gutturale, nonchalante : grain rauque, compression, slapback discret, reverb sombre derrière. Pas d'autotune.
inline void mapGorge(float k, Mapped& m) {
    m.eng.tension = 0.f;
    m.eng.mordant = 0.12f + 0.42f * k;
    m.eng.brume = 0.04f + 0.24f * k;
    m.eng.halo = 0.18f * k;
    m.comp = 0.35f + 0.55f * k;
    m.lowDb = 0.5f + 3.5f * k;
    m.highDb = -0.5f - 2.5f * k;
    m.deess = 0.3f + 0.5f * k;
    m.slapMs = 85.f; m.slapMix = 0.22f * k;
    m.hpHz = 50.f;
}
// Hyperpop sombre : hauteur nette, doubles +12/-5, brillance, saturation, compression dure. (Pas de formants : voir README.)
inline void mapPoupee(float k, Mapped& m) {
    m.eng.tension = 0.35f + 0.65f * k;
    m.eng.mordant = 0.10f + 0.45f * k;
    m.eng.halo = 0.10f + 0.60f * k;
    m.eng.deadCents = 8.f - 7.f * k;
    m.eng.retuneMs = lerp(60.f, 4.f, k);            // hard-tune net en fin de course
    m.eng.brume = k < 0.5f ? 0.05f : lerp(0.05f, 0.32f, (k - 0.5f) / 0.5f);
    m.comp = 0.40f + 0.50f * k;
    m.highDb = 1.f + 5.f * k;
    m.slapMs = 55.f; m.slapMix = k < 0.5f ? 0.f : 0.16f * (k - 0.5f) / 0.5f;
    m.hpHz = 70.f;
}
// Caractère générique (artiste non identifié) : saturation douce, compression parallèle, air, doubles, reverb courte.
inline void mapPatine(float k, Mapped& m) {
    m.eng.tension = 0.f;
    m.eng.mordant = 0.05f + 0.35f * k;
    m.eng.brume = 0.05f + 0.22f * k;
    m.eng.halo = 0.25f * k;
    m.comp = 0.30f + 0.50f * k;
    m.highDb = 4.5f * k;
    m.hpHz = 45.f;
}

// Réglages spécifiques (0..1, valeur d'origine = rendu historique du modèle).
inline void specGrainNord(float s, Mapped& m) { m.width = 2.f * s; }                                    // 0,5 = 100 %
inline void specGorge(float s, Mapped& m) { m.atkMs = 6.f * std::pow(5.f, 1.f - 2.f * s); m.relMs = 120.f * std::pow(2.5f, 1.f - 2.f * s); }  // 0,5 = 6 ms / 120 ms
inline void specPoupee(float s, Mapped& m) { m.highDb += 10.f * (s - 0.5f); }                          // ±5 dB d'aigus
inline void specPatine(float s, Mapped& m) { m.driveDb = 18.f * s; }

inline const std::vector<Voicing>& voicings() {
    static const std::vector<Voicing> v = {
        {"grainnord", "Grain Nord", "Retune, doubles, brume sombre", "DW-501", 0.45f, mapGrainNord,
         "Largeur", "Largeur stéréo des doubles et de la brume : 0 = mono, 50 = d'origine, 100 = très large.", 0.5f, specGrainNord},
        {"gorge", "Gorge", "Grain rauque, compression, slapback", "DW-502", 0.50f, mapGorge,
         "Vitesse", "Vitesse du compresseur : bas = attaque lente qui laisse passer les consonnes, haut = attaque et relâche rapides.", 0.5f, specGorge},
        {"poupee", "Poupée", "Hauteur nette, doubles, brillance", "DW-503", 0.50f, mapPoupee,
         "Brillance", "Brillance : aigus au-dessus de 9 kHz, ±5 dB autour du réglage d'origine.", 0.5f, specPoupee},
        {"patine", "Patine", "Saturation, compression parallèle, air", "DW-504", 0.45f, mapPatine,
         "Drive", "Drive : saturation douce en entrée de chaîne (0 à +18 dB d'attaque, niveau compensé).", 0.f, specPatine},
    };
    return v;
}

// Étage d'auto-entrée : suiveur RMS lent (≈ 1,2 s), muet sous -55 dBFS (le silence ne fait pas monter le gain),
// gain lissé ramenant le niveau vers -18 dBFS RMS, borné à ±24 dB. Rend les seuils fixes indépendants du niveau de la prise.
struct AutoInput {
    float ms = 0.f, fast = 0.f, g = 1.f, cMs = 0.f, cFast = 0.f, cG = 0.f;
    static constexpr float target = 0.12589254f;  // -18 dBFS RMS
    void prepare(float sr) { cMs = 1.f / (1.2f * sr); cFast = 1.f / (0.05f * sr); cG = 1.f / (0.25f * sr); ms = target * target; fast = 0.f; g = 1.f; }
    float process(float x, bool on) {
        const float x2 = x * x;
        fast += (x2 - fast) * cFast;
        if (fast > 3.2e-6f) ms += (x2 - ms) * cMs;   // -55 dBFS
        const float want = on ? std::clamp(target / std::sqrt(ms + 1e-12f), 0.063f, 15.85f) : 1.f;
        g += (want - g) * cG;
        return x * g;
    }
};

// Valeur lissée (≈ 20 ms), saisie directe à la première consigne.
struct Glide {
    float v = 0.f, t = 0.f, c = 0.001f; bool primed = false;
    void prepare(float sr) { c = 1.f / (0.02f * sr); primed = false; }
    void set(float x) { t = x; if (!primed) { v = x; primed = true; } }
    float next() { v += (t - v) * c; if (std::fabs(t - v) < 1e-7f) v = t; return v; }
};

// Chaîne complète. Latence = celle du moteur de hauteur (constante, déclarée à l'hôte).
// Ordre : entrée (trim, auto-entrée) → [drive] → passe-haut → moteur → plateaux du modèle + ton → compression → de-esser
// → slapback → lo-fi → passe-bas → largeur → auto-gain → limiteur doux → mélange avec le sec retardé → sortie.
class Chain {
public:
    void prepare(double sampleRate, int maxBlock) {
        sr = (float)sampleRate;
        eng.prepare(sampleRate);
        maxN = std::max(1, maxBlock);
        tmpL.assign((size_t)maxN, 0.f); tmpR.assign((size_t)maxN, 0.f); mono.assign((size_t)maxN, 0.f);
        slapL.init((int)(0.2 * sampleRate)); slapR.init((int)(0.2 * sampleRate));
        dryL.init(eng.latency() + maxN + 8); dryR.init(eng.latency() + maxN + 8);
        for (int c = 0; c < 2; ++c) { hp[c].reset(); lp[c].reset(); lowS[c].reset(); highS[c].reset(); tiltL[c].reset(); tiltH[c].reset(); ds[c].prepare(sr); }
        comp.prepare(sr);
        autoIn.prepare(sr); drv.reset();
        trimG.prepare(sr); mixG.prepare(sr); outG.prepare(sr); widthG.prepare(sr); widthG.set(M.width);
        lastHp = lastLp = lastLow = lastHigh = lastTone = -100.f; lastAtk = lastRel = -1.f;
        inMs = outMs = 1e-6f; ag = 1.f; hold[0] = hold[1] = 0.f; hc = 0;
        grAvg = 0.f; inDb = -100.f;
        lim[0].reset(); lim[1].reset();
    }
    int latency() const { return eng.latency(); }
    void setBpm(double b) { eng.setBpm(b); }
    float outLevel() const { return peakOut; }
    bool keyKnown() const { return eng.keyKnown(); }
    int key() const { return eng.detectedKey(); }
    int scale() const { return eng.detectedScale(); }
    int keyStatus() const { return eng.keyStatus(); }   // 0 imposée, 1 auto verrouillée, 2 auto en recherche
    // Sélecteur de tonalité : keyChoice 0 = Auto, 1..12 = C..B ; scaleChoice 0 = Auto, 1 = Majeur, 2 = Mineur.
    void setKeySel(int keyChoice, int scaleChoice) { keyChoice_ = std::clamp(keyChoice, 0, 12); scaleChoice_ = std::clamp(scaleChoice, 0, 2); }
    float gainReduction() const { return comp.gr; }
    float meanReduction() const { return grAvg; }        // réduction moyenne du dernier bloc (dB)
    float inputDb() const { return inDb; }               // niveau RMS après trim (dBFS), avant auto-entrée
    float autoInGainDb() const { return 20.f * std::log10(autoIn.g); }

    void setExtras(const Extras& e) {
        X = e;
        trimG.set(std::pow(10.f, std::clamp(e.trimDb, -12.f, 12.f) / 20.f));
        mixG.set(std::clamp(e.mix, 0.f, 1.f));
        outG.set(std::pow(10.f, std::clamp(e.outDb, -12.f, 12.f) / 20.f));
    }

    void setKnob(float k, const Voicing& v) {
        Mapped m; m.eng.mix = 1.f; m.eng.outDb = 0.f;
        m.eng.safe = true;
        m.eng.autoKey = keyChoice_ == 0;  m.eng.key = keyChoice_ > 0 ? keyChoice_ - 1 : 0;
        m.eng.autoScale = scaleChoice_ == 0;  m.eng.scale = scaleChoice_ > 0 ? scaleChoice_ : 1;
        v.map(std::clamp(k, 0.f, 1.f), m);
        if (v.spec) v.spec(std::clamp(X.special < 0.f ? v.specDefault : X.special, 0.f, 1.f), m);
        M = m; eng.set(M.eng);
        slapSamples = M.slapMs * 1e-3f * sr;
        // compresseur : seuil et rapport suivent l'intensité (seuils fixes : l'auto-entrée en amont les rend indépendants du niveau)
        comp.thr = -14.f - 14.f * M.comp; comp.ratio = 1.5f + 4.5f * M.comp; comp.makeup = 0.f;
        if (M.atkMs != lastAtk || M.relMs != lastRel) { comp.setTimes(sr, M.atkMs, M.relMs); lastAtk = M.atkMs; lastRel = M.relMs; }
        for (int c = 0; c < 2; ++c) ds[c].amt = M.deess;
        widthG.set(M.width);
        drvG = std::pow(10.f, M.driveDb / 20.f);
        updateFilters();
    }

    // Entrée mono (inR nul) ou stéréo ; sortie stéréo. Peut être appelée en place. n ≤ maxBlock.
    void process(const float* inL, const float* inR, float* outL, float* outR, int n) {
        n = std::min(n, maxN);
        if (!mixG.primed) setExtras(X);   // première consigne après prepare() : saisie sans glissement
        float inSum = 0.f;
        for (int i = 0; i < n; ++i) {
            const float xl = inL[i], xr = inR ? inR[i] : xl;
            dryL.push(xl); dryR.push(xr);                 // sec, avant tout traitement
            const float raw = inR ? 0.5f * (xl + xr) : xl;
            inMs += (raw * raw - inMs) * (1.f / (0.5f * sr));
            float x = raw * trimG.next();
            inSum += x * x;
            x = autoIn.process(x, X.autoIn);
            if (drvG > 1.001f) x = drv.process(x * drvG) / drvG * (1.f + 0.25f * (1.f - 1.f / drvG));
            mono[(size_t)i] = hp[0].process(x);   // le moteur de hauteur ne voit que le signal filtré passe-haut
        }
        inDb = 10.f * std::log10(inSum / (float)std::max(1, n) + 1e-10f);
        eng.process(mono.data(), nullptr, tmpL.data(), tmpR.data(), n);
        const float lofiAmt = M.lofi;
        const int hn = 1 + (int)(lofiAmt * 3.f);
        const float cmpMix = 0.35f + 0.65f * std::min(1.f, M.comp * 1.2f);
        const float invIn = 1.f / std::max(1e-3f, autoIn.g);
        float grS = 0.f;
        peakOut = 0.f;
        for (int i = 0; i < n; ++i) {
            float l = tmpL[(size_t)i], r = tmpR[(size_t)i];
            l = lowS[0].process(highS[0].process(l)); r = lowS[1].process(highS[1].process(r));
            l = tiltL[0].process(tiltH[0].process(l)); r = tiltL[1].process(tiltH[1].process(r));
            // compression (parallèle : mélange du signal comprimé et du sec)
            const float g = comp.gain(l, r);
            grS += comp.gr;
            l = l * (1.f - cmpMix * 0.6f) + l * g * (cmpMix * 0.6f);
            r = r * (1.f - cmpMix * 0.6f) + r * g * (cmpMix * 0.6f);
            if (M.deess > 0.f) { l = ds[0].process(l); r = ds[1].process(r); }
            slapL.push(l); slapR.push(r);   // G6 : toujours écrit (plus d'écho fantôme d'un ancien passage quand le mélange remonte)
            if (M.slapMix > 0.f) {
                const float sl = slapL.read(slapSamples), sr2 = slapR.read(slapSamples * 1.13f);
                l += sl * M.slapMix; r += sr2 * M.slapMix;
            }
            if (lofiAmt > 0.001f) {   // grain lo-fi : échantillonnage retenu + quantification douce
                if (++hc >= hn) { hc = 0; hold[0] = l; hold[1] = r; }
                const float q = std::exp2(15.f - 4.f * lofiAmt);
                l = std::round(hold[0] * q) / q; r = std::round(hold[1] * q) / q;
            }
            l = lp[0].process(l); r = lp[1].process(r);
            {   // largeur M/S
                const float w = widthG.next(), mid = 0.5f * (l + r), side = 0.5f * (l - r) * w;
                l = mid + side; r = mid - side;
            }
            if (X.autoGain) {
                // compensation de volume lente : la sortie garde le niveau d'entrée quel que soit le potard
                outMs += ((l * l + r * r) * 0.5f - outMs) * (1.f / (0.5f * sr));
                const float want = std::sqrt((inMs + 1e-9f) / (outMs + 1e-9f));
                const float tgt = std::clamp(want, 0.35f, 2.0f);
                ag += (tgt - ag) * 0.0004f;
            } else {
                ag += (invIn - ag) * 0.0004f;   // sans auto-gain : on rend seulement le gain de l'auto-entrée
                outMs = inMs;
            }
            l *= ag; r *= ag;
            const float wl = lim[0].process(l * 0.9f) / 0.9f * 0.95f;   // limiteur doux de sécurité (tanh ADAA : peu de repliement)
            const float wr = lim[1].process(r * 0.9f) / 0.9f * 0.95f;
            // sec retardé de la latence (lecture exacte, entière) ; tous les échantillons du bloc sont déjà poussés
            const float back = (float)(eng.latency() + (n - 1 - i));
            const float dl = dryL.read(back), dr = dryR.read(back);
            const float mx = mixG.next(), og = outG.next();
            outL[i] = (dl + (wl - dl) * mx) * og;
            outR[i] = (dr + (wr - dr) * mx) * og;
            peakOut = std::max(peakOut, std::max(std::fabs(outL[i]), std::fabs(outR[i])));
        }
        grAvg = grS / (float)std::max(1, n);
    }

private:
    void updateFilters() {
        if (M.hpHz != lastHp) { for (auto& f : hp) f.highpass(sr, M.hpHz); lastHp = M.hpHz; }
        if (M.lpHz != lastLp) { for (auto& f : lp) f.lowpass(sr, std::min(M.lpHz, 0.45f * sr)); lastLp = M.lpHz; }
        if (M.lowDb != lastLow) { for (auto& f : lowS) f.lowShelf(sr, 180.f, M.lowDb); lastLow = M.lowDb; }
        if (M.highDb != lastHigh) { for (auto& f : highS) f.highShelf(sr, 9000.f, M.highDb); lastHigh = M.highDb; }
        const float t = std::clamp(X.toneDb, -6.f, 6.f);
        if (std::fabs(t - lastTone) > 0.02f) {   // ton : inclinaison autour de 1 kHz (grave -t, aigu +t)
            for (auto& f : tiltL) f.lowShelf(sr, 300.f, -t);
            for (auto& f : tiltH) f.highShelf(sr, 3000.f, t);
            lastTone = t;
        }
    }
    float sr = 48000.f, slapSamples = 0.f, peakOut = 0.f, drvG = 1.f;
    float lastHp = -1, lastLp = -1, lastLow = -1, lastHigh = -1, lastTone = -100.f, lastAtk = -1.f, lastRel = -1.f;
    float inMs = 1e-6f, outMs = 1e-6f, ag = 1.f, hold[2] = {0, 0};
    float grAvg = 0.f, inDb = -100.f;
    int hc = 0, keyChoice_ = 0, scaleChoice_ = 0, maxN = 512;
    Mapped M;
    Extras X;
    onek_classic::Engine eng;
    Biquad hp[2], lp[2], lowS[2], highS[2], tiltL[2], tiltH[2];
    TanhAdaa lim[2], drv;
    DeEss ds[2];
    Comp comp;
    AutoInput autoIn;
    Glide trimG, mixG, outG, widthG;
    onek_classic::Delay slapL, slapR, dryL, dryR;
    std::vector<float> tmpL, tmpR, mono;
};

}  // namespace classic
}  // namespace onek
