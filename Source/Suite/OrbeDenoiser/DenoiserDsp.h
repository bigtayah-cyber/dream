// Moteur d'Orbe Denoiser : STFT 2048 / saut 512 (fenêtre racine de Hann, WOLA), gain de Wiener à rapport signal/bruit a priori
// « dirigé par la décision » (lissage temporel anti « bruit musical »), profil appris + minima statistiques. Sans JUCE.
#pragma once
#include "../OrbeDsp/OrbeDsp.h"

namespace dn {
constexpr int kN = 2048, kHop = 512, kBins = kN / 2 + 1, kLatency = kN;      // mode normal (et tailles maximales des tampons)
constexpr int kNLive = 512, kHopLive = 128, kLatencyLive = kNLive;            // mode « Prise » : FFT 512, saut 128, latence 512 (≈ 10,7 ms à 48 kHz)

struct Params {
    float reduction = 18.f;  // dB, 0..24
    float sens = 0.5f;       // 0..1 : sur-soustraction du bruit estimé
    float smooth = 0.5f;     // 0..1
    float air = 0.25f;       // 0..1 : préservation des aigus
    int hum = 0;             // 0 coupé, 1 = 50 Hz, 2 = 60 Hz
    bool adaptive = true, listen = false;
    float gapDb = 0.f;       // 0..20 : réduction supplémentaire entre les phrases (hors voix), 0 = coupé
    float bandLo = 0.f;      // -12..+12 dB : réduction ajoutée sous 200 Hz (le potard Réduction reste le maître)
    float bandHi = 0.f;      // -12..+12 dB : réduction ajoutée au-dessus de 5 kHz
    bool live = false;       // mode Prise : latence ≈ 11 ms, réduction plafonnée à 12 dB (changement : par prepare())
};

class Engine {
public:
    // Affichage (écrit par le fil audio, lu par l'interface) : dBFS par bin
    std::array<std::atomic<float>, kBins> dispIn, dispOut, dispNoise;

    Engine() {
        fftBig.init(kN); fftLive.init(kNLive); win.assign(kN, 0.f); winLive.assign(kNLive, 0.f);
        for (int i = 0; i < kN; ++i) win[(size_t)i] = std::sin((float)odsp::kPi * (float)i / (float)kN);
        for (int i = 0; i < kNLive; ++i) winLive[(size_t)i] = std::sin((float)odsp::kPi * (float)i / (float)kNLive);
        Ncur.assign(kBins, 0.f); profN.assign(kBins, 0.f);
        for (auto* a : {&dispIn, &dispOut, &dispNoise}) for (auto& v : *a) v.store(-120.f);
        Pbuf.assign(kBins, 0.f); learnAcc.assign(kBins, 0.f); pending.assign(kBins, 0.f);   // tampons du fil audio : jamais d'allocation dans process()
    }
    // Le mode (normal / Prise) ne change que dans prepare() : jamais pendant process(). Le profil appris reste valable dans les deux modes.
    void prepare(double sampleRate, bool live = false) {
        sr = (float)sampleRate;
        N = live ? kNLive : kN; H = live ? kHopLive : kHop; B = N / 2 + 1; liveMode = live;
        for (auto& c : ch) c.reset();
        subLen = std::max(4, (int)(0.25f * sr / (float)H));
        learnLeft.store(0); learnFrames = std::max(8, (int)(2.0f * sr / (float)H));
        gapG = 1.f; voiceHold = 0; gapAtk = 1.f - std::exp(-(float)H / (0.02f * sr)); gapRel = 1.f - std::exp(-(float)H / (0.30f * sr));
        holdFrames = std::max(1, (int)(0.25f * sr / (float)H));
        rebuildProf();
        setHum(0, true);
    }
    void set(const Params& np) { p = np; setHum(p.hum, false); }
    int latency() const { return N; }
    bool liveActive() const { return liveMode; }

    // Apprentissage sur environ 2 s de silence
    void startLearn() { learnReq.store(true); }
    float learnProgress() const { const int l = learnLeft.load(); return l <= 0 ? (hasProfile.load() ? 1.f : 0.f) : 1.f - (float)l / (float)learnFrames; }
    bool learning() const { return learnLeft.load() > 0 || learnReq.load(); }
    bool profileKnown() const { return hasProfile.load() || pendingReady.load(); }
    void clearProfile() { hasProfile.store(false); }
    void getProfile(std::vector<float>& o) const { if (pendingReady.load()) o = pending; else o.assign(profile.begin(), profile.end()); }
    // srSaved : fréquence d'échantillonnage à l'enregistrement (0 = inconnue) ; le profil est rééchantillonné en fréquence à la lecture si elle diffère.
    void setProfile(const std::vector<float>& v, float srSaved = 0.f) { if ((int)v.size() == kBins) { pending = v; pendingSr = srSaved; pendingReady.store(true); } }
    float profileRate() const { return pendingReady.load() && pendingSr > 1000.f ? pendingSr : sr; }

    void process(const float* inL, const float* inR, float* outL, float* outR, int n) {
        for (int i = 0; i < n; ++i) {
            const float xl = inL[i], xr = inR ? inR[i] : xl;
            outL[i] = tick(ch[0], xl, 0);
            outR[i] = inR ? tick(ch[1], xr, 1) : outL[i];
        }
    }

private:
    struct Chan {
        std::vector<float> in, ola, raw, sm, subMin, hist, aPrev, gPrev, re, im, gain;
        odsp::Bq notch[4];
        int pos = 0, hopPos = 0, frames = 0, subCount = 0, histPos = 0, filled = 0;   // hopPos modulo kHop, frames saturé : pas de débordement int (M5)
        void reset() {
            in.assign(kN, 0.f); ola.assign(kN, 0.f); raw.assign(kN, 0.f); sm.assign(kBins, 0.f); subMin.assign(kBins, 1e30f);
            hist.assign((size_t)kHist * kBins, 1e30f); aPrev.assign(kBins, 0.f); gPrev.assign(kBins, 1.f);
            re.assign(kN, 0.f); im.assign(kN, 0.f); gain.assign(kBins, 1.f);
            pos = hopPos = frames = subCount = histPos = filled = 0;
            for (auto& b : notch) b.reset();
        }
    };
    static constexpr int kHist = 8;
    static constexpr int kNb = 16;         // demi-largeur du voisinage de la garde de tonalité (bins)
    static constexpr float kTonal = 2.f;   // un bin n'entre dans le suivi des minima qu'à 3 dB au plus au-dessus de ses voisins

    void setHum(int h, bool force) {
        if (h == humSet && !force) return;
        humSet = h;
        for (auto& c : ch) for (int k = 0; k < 4; ++k) c.notch[k].c = h ? odsp::design(odsp::Kind::Notch, sr, (h == 1 ? 50.0 : 60.0) * (k + 1), 0, 20.0) : odsp::Coef{};
    }

    float tick(Chan& c, float x, int index) {
        if (!kit::finiteBits(x)) x = 0.f;   // NaN/inf d'un plugin précédent : sinon P, sm, aPrev et gPrev restent NaN pour toujours
        const float rawDelayed = c.raw[(size_t)c.pos];
        c.raw[(size_t)c.pos] = x;
        float xh = x;
        if (humSet) for (auto& b : c.notch) xh = b.process(xh);
        c.in[(size_t)c.pos] = xh;
        const float processed = c.ola[(size_t)c.pos];
        c.ola[(size_t)c.pos] = 0.f;
        const int start = (c.pos + 1) & (N - 1);
        c.pos = (c.pos + 1) & (N - 1);
        if (++c.hopPos == H) { c.hopPos = 0; frame(c, start, index); if (c.frames < 2) ++c.frames; }
        return p.listen ? rawDelayed - processed : processed;
    }

    // premier quart de c.sm sur [a, b] borné au spectre ; 0 si l'intervalle est vide (bord du spectre : l'autre côté décide)
    float sideQuartile(const Chan& c, int a, int b) {
        a = std::max(a, 0); b = std::min(b, B - 1);
        if (b < a) return 0.f;
        const int m = b - a + 1;
        for (int j = 0; j < m; ++j) med[(size_t)j] = c.sm[(size_t)(a + j)];
        std::nth_element(med.begin(), med.begin() + m / 4, med.begin() + m);
        return med[(size_t)(m / 4)];
    }

    void frame(Chan& c, int start, int index) {
        if (index == 0) {
            if (pendingReady.exchange(false)) {
                profile = pending; hasProfile.store(true);
                if (pendingSr > 1000.f && std::fabs(pendingSr - sr) > 1.f) {   // profil enregistré à une autre fréquence : même contenu en Hz
                    const float r = sr / pendingSr; for (int k = 0; k < kBins; ++k) profile[(size_t)k] = pending[(size_t)std::min(kBins - 1, (int)std::lround((float)k / r))];
                }
                rebuildProf();
            }
            if (learnReq.exchange(false)) { learnLeft.store(learnFrames); std::fill(learnAcc.begin(), learnAcc.end(), 0.f); learnN = 0; }
        }
        float* re = c.re.data(); float* im = c.im.data();
        const std::vector<float>& wn = liveMode ? winLive : win; odsp::Fft& fft = liveMode ? fftLive : fftBig;
        for (int i = 0; i < N; ++i) { re[i] = c.in[(size_t)((start + i) & (N - 1))] * wn[(size_t)i]; im[i] = 0.f; }
        fft.run(re, im, false);
        auto& P = Pbuf;   // pré-alloué (constructeur) : écrasé intégralement ci-dessous
        for (int k = 0; k < B; ++k) P[(size_t)k] = re[k] * re[k] + im[k] * im[k];

        // minima statistiques (suivi du plancher de bruit), avec garde de tonalité : un partiel stable (note tenue, pad, 808)
        // dure souvent plus que la fenêtre des minima et deviendrait son propre plancher. Le suivi ne voit donc chaque bin
        // qu'au plus à kTonal × le premier quart de ses voisins, pris de CHAQUE côté (k-kNb..k-2 et k+2..k+kNb, lissés), et on garde
        // le côté le plus fort : sur du bruit seul rien ne change ; sur un pic tonal (les deux côtés plus bas), le plancher reste celui
        // du bruit qui l'entoure ; sur une pente forte (bruit de pièce, grondement < 300 Hz), le côté haut tient le plancher.
        // Audit 3 : la fenêtre unique (±kNb) prenait le quart bas d'une pente de 15 dB et sous-estimait le bruit grave
        // (bruit de pièce 80 Hz-16 kHz : -3,2 dB au lieu de -17 dB de réduction).
        for (int k = 0; k < B; ++k)
            c.sm[(size_t)k] = c.frames == 0 ? P[(size_t)k] : 0.85f * c.sm[(size_t)k] + 0.15f * P[(size_t)k];
        for (int k = 0; k < B; ++k) {
            const float ref = std::max(sideQuartile(c, k - kNb, k - 2), sideQuartile(c, k + 2, k + kNb));
            const float q = std::min(c.sm[(size_t)k], kTonal * ref);
            c.subMin[(size_t)k] = std::min(c.subMin[(size_t)k], q);
        }
        if (++c.subCount >= subLen) {
            std::copy(c.subMin.begin(), c.subMin.begin() + B, c.hist.begin() + (long)c.histPos * kBins);
            std::fill(c.subMin.begin(), c.subMin.end(), 1e30f);
            c.histPos = (c.histPos + 1) % kHist; c.filled = std::min(c.filled + 1, kHist); c.subCount = 0;
        }
        const bool learnt = hasProfile.load();
        const bool learningNow = learnLeft.load() > 0;
        if (learningNow) {   // les deux voies sont moyennées (M4) ; la voie 0 décompte les trames
            for (int k = 0; k < B; ++k) learnAcc[(size_t)k] += P[(size_t)k];
            ++learnN;
            if (index == 0 && learnLeft.fetch_sub(1) == 1) {   // profil rangé à la résolution de 2048 (puissance ∝ N : ×4 depuis le mode Prise)
                const int ds = kN / N; const float sc = (float)ds / (float)learnN;
                for (int k = 0; k < kBins; ++k) profile[(size_t)k] = learnAcc[(size_t)std::min(B - 1, (k + ds / 2) / ds)] * sc;
                hasProfile.store(true); rebuildProf();
            }
        }
        const bool haveNoise = learnt || p.adaptive;
        const float red = liveMode ? std::min(p.reduction, 12.f) : p.reduction, alpha = 1.f + 2.f * p.sens, beta = 0.90f + 0.09f * p.smooth, aRel = 0.6f - 0.45f * p.smooth;
        const float binHz = sr / (float)N;
        float sumP = 0.f, sumN = 0.f; const int vLo = std::max(1, (int)(100.f / binHz)), vHi = std::min(B - 1, (int)(4000.f / binHz));
        for (int k = 0; k < B; ++k) {
            float Nn = 0.f;
            if (p.adaptive) {
                float m = c.subMin[(size_t)k];
                for (int h = 0; h < c.filled; ++h) m = std::min(m, c.hist[(size_t)h * kBins + (size_t)k]);
                Nn = 2.0f * m;
            }
            // avec un profil, l'adaptatif ne peut relever le bruit que de 6 dB (un son tenu n'est pas pris pour du bruit)
            if (learnt) Nn = p.adaptive ? std::max(std::min(Nn, 4.f * profN[(size_t)k]), profN[(size_t)k]) : profN[(size_t)k];
            const float Np = std::max(Nn * alpha, 1e-12f);
            if (k >= vLo && k <= vHi) { sumP += P[(size_t)k]; sumN += Np; }
            const float f = (float)k * binHz, w = std::clamp((f - 4000.f) / 5000.f, 0.f, 1.f), ws = w * w * (3.f - 2.f * w);
            const float wlo = 1.f - sstep((f - 150.f) / 150.f), whi = sstep((f - 4000.f) / 2000.f);   // 3 bandes : grave < 200 Hz, médium, aigu > 5 kHz
            const float redK = std::clamp(red + p.bandLo * wlo + p.bandHi * whi, 0.f, 36.f);
            const float floorG = odsp::db2lin(-redK * (1.f - 0.7f * p.air * ws));
            float g = 1.f;
            if (haveNoise && (!learningNow || p.adaptive)) {   // l'adaptatif continue pendant l'apprentissage du profil
                const float gamma = P[(size_t)k] / Np;
                const float xi = beta * c.aPrev[(size_t)k] / Np + (1.f - beta) * std::max(gamma - 1.f, 0.f);
                g = xi / (1.f + xi);
            }
            c.gain[(size_t)k] = g;
            c.aPrev[(size_t)k] = g * g * P[(size_t)k];
            if (index == 0) Ncur[(size_t)k] = haveNoise ? Nn : -1.f;
            c.gain[(size_t)k] = std::max(g, floorG);
        }
        // lissage fréquentiel puis temporel du gain
        for (int pass = 0; pass < (p.smooth > 0.66f ? 2 : 1); ++pass) {
            float prev = c.gain[0];
            for (int k = 1; k < B - 1; ++k) { const float cur = c.gain[(size_t)k]; c.gain[(size_t)k] = 0.25f * prev + 0.5f * cur + 0.25f * c.gain[(size_t)k + 1]; prev = cur; }
        }
        for (int k = 0; k < B; ++k) {
            float& gp = c.gPrev[(size_t)k]; const float g = c.gain[(size_t)k];
            gp += (g - gp) * (g > gp ? 0.85f : aRel);
            c.gain[(size_t)k] = gp;
        }
        // « entre les phrases » : la voix est jugée sur 100 Hz à 4 kHz (énergie de la trame contre le bruit estimé), voie 0 pour les deux voies ;
        // hors voix, après 250 ms de maintien, gain supplémentaire (attaque 20 ms, relâchement 300 ms)
        if (index == 0) {
            const bool voice = !(haveNoise && !learningNow) || sumP > 3.f * sumN;
            if (voice) voiceHold = holdFrames; else if (voiceHold > 0) --voiceHold;
            const float target = (p.gapDb > 0.05f && voiceHold == 0) ? odsp::db2lin(-p.gapDb) : 1.f;
            gapG += (target - gapG) * (target < gapG ? gapAtk : gapRel);
        }
        if (index == 0) {
            const int ds = kN / N;
            for (int k = 0; k < kBins; ++k) {
                const int b = std::min(B - 1, (k + ds / 2) / ds);
                dispIn[(size_t)k].store(toDb(P[(size_t)b] * (float)ds), std::memory_order_relaxed);
                dispOut[(size_t)k].store(toDb(P[(size_t)b] * (float)ds * c.gain[(size_t)b] * c.gain[(size_t)b] * gapG * gapG), std::memory_order_relaxed);
                dispNoise[(size_t)k].store(Ncur[(size_t)b] < 0.f ? -120.f : toDb(Ncur[(size_t)b] * (float)ds), std::memory_order_relaxed);
            }
        }
        for (int k = 0; k < B; ++k) { const float g = c.gain[(size_t)k] * gapG; re[k] *= g; im[k] *= g; if (k > 0 && k < N / 2) { re[N - k] *= g; im[N - k] *= g; } }
        fft.run(re, im, true);
        const float sc = 0.5f / (float)N;
        for (int i = 0; i < N; ++i) c.ola[(size_t)((start + i) & (N - 1))] += re[i] * wn[(size_t)i] * sc;
    }
    static float sstep(float x) { x = std::clamp(x, 0.f, 1.f); return x * x * (3.f - 2.f * x); }
    // profil à la résolution courante : identique à `profile` en mode normal ; moyenne de 4 bins et puissance /4 en mode Prise
    void rebuildProf() {
        const int ds = kN / N;
        for (int k = 0; k < B; ++k) {
            if (ds == 1) { profN[(size_t)k] = profile[(size_t)k]; continue; }
            float a = 0.f; int n = 0; for (int j = k * ds - ds / 2; j < k * ds + ds / 2 + ds % 2 + 0; ++j) if (j >= 0 && j < kBins) { a += profile[(size_t)j]; ++n; }
            profN[(size_t)k] = n ? a / (float)n / (float)ds : 0.f;
        }
    }
    // Fenêtre racine de Hann : somme = 2N/pi, donc |X| d'un sinus d'amplitude A vaut A*N/pi ; 0 dBFS = sinus pleine échelle.
    static float toDb(float pw) { return 10.f * std::log10(pw + 1e-20f) - 56.28f; }

    odsp::Fft fftBig, fftLive;
    std::vector<float> med = std::vector<float>(2 * kNb + 1, 0.f);   // tampon du quantile (fil audio, pré-alloué)
    std::vector<float> Pbuf, win, winLive, Ncur, profN, profile = std::vector<float>(kBins, 0.f), pending, learnAcc;
    Chan ch[2];
    Params p;
    float sr = 48000.f;
    int N = kN, H = kHop, B = kBins, subLen = 24, learnFrames = 180, learnN = 0, humSet = 0, voiceHold = 0, holdFrames = 24;
    bool liveMode = false;
    float pendingSr = 0.f, gapG = 1.f, gapAtk = 0.1f, gapRel = 0.03f;
    std::atomic<int> learnLeft{0};
    std::atomic<bool> learnReq{false}, hasProfile{false}, pendingReady{false};
};

}  // namespace dn
