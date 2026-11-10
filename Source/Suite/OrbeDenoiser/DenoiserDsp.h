// Moteur d'Orbe Denoiser : STFT 2048 / saut 512 (fenêtre racine de Hann, WOLA), gain de Wiener à rapport signal/bruit a priori
// « dirigé par la décision » (lissage temporel anti « bruit musical »), profil appris + minima statistiques. Sans JUCE.
#pragma once
#include "../OrbeDsp/OrbeDsp.h"

namespace dn {
constexpr int kN = 2048, kHop = 512, kBins = kN / 2 + 1, kLatency = kN;

struct Params {
    float reduction = 18.f;  // dB, 0..24
    float sens = 0.5f;       // 0..1 : sur-soustraction du bruit estimé
    float smooth = 0.5f;     // 0..1
    float air = 0.25f;       // 0..1 : préservation des aigus
    int hum = 0;             // 0 coupé, 1 = 50 Hz, 2 = 60 Hz
    bool adaptive = true, listen = false;
};

class Engine {
public:
    // Affichage (écrit par le fil audio, lu par l'interface) : dBFS par bin
    std::array<std::atomic<float>, kBins> dispIn, dispOut, dispNoise;

    Engine() {
        fft.init(kN); win.resize(kN);
        for (int i = 0; i < kN; ++i) win[(size_t)i] = std::sin((float)odsp::kPi * (float)i / (float)kN);
        for (auto* a : {&dispIn, &dispOut, &dispNoise}) for (auto& v : *a) v.store(-120.f);
        Pbuf.assign(kBins, 0.f); learnAcc.assign(kBins, 0.f); pending.assign(kBins, 0.f);   // tampons du fil audio : jamais d'allocation dans process()
    }
    void prepare(double sampleRate) {
        sr = (float)sampleRate;
        for (auto& c : ch) c.reset();
        subLen = std::max(4, (int)(0.25f * sr / kHop));
        learnLeft.store(0); learnFrames = std::max(8, (int)(2.0f * sr / kHop));
        setHum(0, true);
    }
    void set(const Params& np) { p = np; setHum(p.hum, false); }
    int latency() const { return kLatency; }

    // Apprentissage sur environ 2 s de silence
    void startLearn() { learnReq.store(true); }
    float learnProgress() const { const int l = learnLeft.load(); return l <= 0 ? (hasProfile.load() ? 1.f : 0.f) : 1.f - (float)l / (float)learnFrames; }
    bool learning() const { return learnLeft.load() > 0 || learnReq.load(); }
    bool profileKnown() const { return hasProfile.load() || pendingReady.load(); }
    void clearProfile() { hasProfile.store(false); }
    void getProfile(std::vector<float>& o) const { if (pendingReady.load()) o = pending; else o.assign(profile.begin(), profile.end()); }
    void setProfile(const std::vector<float>& v) { if ((int)v.size() == kBins) { pending = v; pendingReady.store(true); } }

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
        int pos = 0, count = 0, subCount = 0, histPos = 0, filled = 0;
        void reset() {
            in.assign(kN, 0.f); ola.assign(kN, 0.f); raw.assign(kN, 0.f); sm.assign(kBins, 0.f); subMin.assign(kBins, 1e30f);
            hist.assign((size_t)kHist * kBins, 1e30f); aPrev.assign(kBins, 0.f); gPrev.assign(kBins, 1.f);
            re.assign(kN, 0.f); im.assign(kN, 0.f); gain.assign(kBins, 1.f);
            pos = count = subCount = histPos = filled = 0;
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
        const float rawDelayed = c.raw[(size_t)c.pos];
        c.raw[(size_t)c.pos] = x;
        float xh = x;
        if (humSet) for (auto& b : c.notch) xh = b.process(xh);
        c.in[(size_t)c.pos] = xh;
        const float processed = c.ola[(size_t)c.pos];
        c.ola[(size_t)c.pos] = 0.f;
        const int start = (c.pos + 1) & (kN - 1);
        c.pos = (c.pos + 1) & (kN - 1);
        if (++c.count % kHop == 0) frame(c, start, index);
        return p.listen ? rawDelayed - processed : processed;
    }

    void frame(Chan& c, int start, int index) {
        if (index == 0) {
            if (pendingReady.exchange(false)) { profile = pending; hasProfile.store(true); }
            if (learnReq.exchange(false)) { learnLeft.store(learnFrames); std::fill(learnAcc.begin(), learnAcc.end(), 0.f); learnN = 0; }
        }
        float* re = c.re.data(); float* im = c.im.data();
        for (int i = 0; i < kN; ++i) { re[i] = c.in[(size_t)((start + i) & (kN - 1))] * win[(size_t)i]; im[i] = 0.f; }
        fft.run(re, im, false);
        auto& P = Pbuf;   // pré-alloué (constructeur) : écrasé intégralement ci-dessous
        for (int k = 0; k < kBins; ++k) P[(size_t)k] = re[k] * re[k] + im[k] * im[k];

        // minima statistiques (suivi du plancher de bruit), avec garde de tonalité : un partiel stable (note tenue, pad, 808)
        // dure souvent plus que la fenêtre des minima et deviendrait son propre plancher. Le suivi ne voit donc chaque bin
        // qu'au plus à kTonal × le premier quart de ses voisins (±kNb bins, lissés) : sur du bruit seul (bin ≈ voisins) rien ne
        // change ; sur un pic tonal, le plancher reste celui du bruit qui l'entoure.
        for (int k = 0; k < kBins; ++k)
            c.sm[(size_t)k] = c.count <= kHop ? P[(size_t)k] : 0.85f * c.sm[(size_t)k] + 0.15f * P[(size_t)k];
        for (int k = 0; k < kBins; ++k) {
            const int lo = std::max(0, k - kNb), hi = std::min(kBins - 1, k + kNb), m = hi - lo + 1;
            for (int j = 0; j < m; ++j) med[(size_t)j] = c.sm[(size_t)(lo + j)];
            std::nth_element(med.begin(), med.begin() + m / 4, med.begin() + m);
            const float q = std::min(c.sm[(size_t)k], kTonal * med[(size_t)(m / 4)]);
            c.subMin[(size_t)k] = std::min(c.subMin[(size_t)k], q);
        }
        if (++c.subCount >= subLen) {
            std::copy(c.subMin.begin(), c.subMin.end(), c.hist.begin() + (long)c.histPos * kBins);
            std::fill(c.subMin.begin(), c.subMin.end(), 1e30f);
            c.histPos = (c.histPos + 1) % kHist; c.filled = std::min(c.filled + 1, kHist); c.subCount = 0;
        }
        const bool learnt = hasProfile.load();
        const bool learningNow = learnLeft.load() > 0;
        if (learningNow && index == 0) {
            for (int k = 0; k < kBins; ++k) learnAcc[(size_t)k] += P[(size_t)k];
            ++learnN;
            if (learnLeft.fetch_sub(1) == 1) { for (int k = 0; k < kBins; ++k) profile[(size_t)k] = learnAcc[(size_t)k] / (float)learnN; hasProfile.store(true); }
        }
        const bool haveNoise = learnt || p.adaptive;
        const float red = p.reduction, alpha = 1.f + 2.f * p.sens, beta = 0.90f + 0.09f * p.smooth, aRel = 0.6f - 0.45f * p.smooth;
        const float binHz = sr / (float)kN;
        for (int k = 0; k < kBins; ++k) {
            float N = 0.f;
            if (p.adaptive) {
                float m = c.subMin[(size_t)k];
                for (int h = 0; h < c.filled; ++h) m = std::min(m, c.hist[(size_t)h * kBins + (size_t)k]);
                N = 2.0f * m;
            }
            // avec un profil, l'adaptatif ne peut relever le bruit que de 6 dB (un son tenu n'est pas pris pour du bruit)
            if (learnt) N = p.adaptive ? std::max(std::min(N, 4.f * profile[(size_t)k]), profile[(size_t)k]) : profile[(size_t)k];
            const float Np = std::max(N * alpha, 1e-12f);
            const float f = (float)k * binHz, w = std::clamp((f - 4000.f) / 5000.f, 0.f, 1.f), ws = w * w * (3.f - 2.f * w);
            const float floorG = odsp::db2lin(-red * (1.f - 0.7f * p.air * ws));
            float g = 1.f;
            if (haveNoise && !learningNow) {
                const float gamma = P[(size_t)k] / Np;
                const float xi = beta * c.aPrev[(size_t)k] / Np + (1.f - beta) * std::max(gamma - 1.f, 0.f);
                g = xi / (1.f + xi);
            }
            c.gain[(size_t)k] = g;
            c.aPrev[(size_t)k] = g * g * P[(size_t)k];
            if (index == 0) dispNoise[(size_t)k].store(haveNoise ? toDb(N) : -120.f, std::memory_order_relaxed);
            c.gain[(size_t)k] = std::max(g, floorG);
        }
        // lissage fréquentiel puis temporel du gain
        for (int pass = 0; pass < (p.smooth > 0.66f ? 2 : 1); ++pass) {
            float prev = c.gain[0];
            for (int k = 1; k < kBins - 1; ++k) { const float cur = c.gain[(size_t)k]; c.gain[(size_t)k] = 0.25f * prev + 0.5f * cur + 0.25f * c.gain[(size_t)k + 1]; prev = cur; }
        }
        for (int k = 0; k < kBins; ++k) {
            float& gp = c.gPrev[(size_t)k]; const float g = c.gain[(size_t)k];
            gp += (g - gp) * (g > gp ? 0.85f : aRel);
            c.gain[(size_t)k] = gp;
        }
        if (index == 0) for (int k = 0; k < kBins; ++k) {
            dispIn[(size_t)k].store(toDb(P[(size_t)k]), std::memory_order_relaxed);
            dispOut[(size_t)k].store(toDb(P[(size_t)k] * c.gain[(size_t)k] * c.gain[(size_t)k]), std::memory_order_relaxed);
        }
        for (int k = 0; k < kBins; ++k) { const float g = c.gain[(size_t)k]; re[k] *= g; im[k] *= g; if (k > 0 && k < kN / 2) { re[kN - k] *= g; im[kN - k] *= g; } }
        fft.run(re, im, true);
        const float sc = 0.5f / (float)kN;
        for (int i = 0; i < kN; ++i) c.ola[(size_t)((start + i) & (kN - 1))] += re[i] * win[(size_t)i] * sc;
    }
    // Fenêtre racine de Hann : somme = 2N/pi, donc |X| d'un sinus d'amplitude A vaut A*N/pi ; 0 dBFS = sinus pleine échelle.
    static float toDb(float pw) { return 10.f * std::log10(pw + 1e-20f) - 56.28f; }

    odsp::Fft fft;
    std::vector<float> med = std::vector<float>(2 * kNb + 1, 0.f);   // tampon du quantile (fil audio, pré-alloué)
    std::vector<float> Pbuf, win, profile = std::vector<float>(kBins, 0.f), pending, learnAcc;
    Chan ch[2];
    Params p;
    float sr = 48000.f;
    int subLen = 24, learnFrames = 180, learnN = 0, humSet = 0;
    std::atomic<int> learnLeft{0};
    std::atomic<bool> learnReq{false}, hasProfile{false}, pendingReady{false};
};

}  // namespace dn
