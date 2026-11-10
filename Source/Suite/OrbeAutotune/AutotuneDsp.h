// Orbe Autotune : moteur audio. C++17 pur, sans JUCE (testable hors-ligne).
// Chaîne : détection YIN -> tonalité (manuelle ou estimée, avec confiance) -> note cible dans la gamme -> décalage de hauteur
// à deux têtes (voile::HeadShifter, Engine.h) -> décalage de formants (STFT, enveloppe cepstrale) -> harmonie / doublage -> mix.
// Deux modes de latence : « Qualité » (décaleur à deux têtes + formants STFT : 37 ms) et « Direct » (décaleur à raccords synchrones
// à la période de la voix, sans formants : latence <= 256 échantillons).
// Règle d'or : la correction ne désaccorde jamais. Tonalité non fiable, trame non voisée ou masque vide = pas de décalage.
#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>
#include <vector>
#include "../../Engine.h"
#include "TuneCore.h"
#include "NoteEdit.h"

namespace atn {

constexpr float kPi = 3.14159265358979323846f;
inline float f2m(float f) { return 69.f + 12.f * std::log2(f / 440.f); }
inline float cl(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }
inline int pcOf(int n) { return ((n % 12) + 12) % 12; }

// ---------------------------------------------------------------- gammes, presets
constexpr int kScales = 9, kPresets = 7;
struct ScaleDef { const char* name; const char* tag; int n; int iv[12]; bool minorLike; };
inline const ScaleDef* scales() {
    static const ScaleDef s[kScales] = {
        {"Majeur", "maj", 7, {0, 2, 4, 5, 7, 9, 11}, false},
        {"Mineur", "min", 7, {0, 2, 3, 5, 7, 8, 10}, true},
        {"Mineur harmonique", "min harm.", 7, {0, 2, 3, 5, 7, 8, 11}, true},
        {"Dorien", "dor", 7, {0, 2, 3, 5, 7, 9, 10}, true},
        {"Phrygien", "phr", 7, {0, 1, 3, 5, 7, 8, 10}, true},
        {"Pentatonique mineure", "penta min", 5, {0, 3, 5, 7, 10}, true},
        {"Pentatonique majeure", "penta maj", 5, {0, 2, 4, 7, 9}, false},
        {"Blues", "blues", 6, {0, 3, 5, 6, 7, 10}, true},
        {"Chromatique", "chrom.", 12, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}, false}};
    return s;
}
inline const char* const* noteNames() {
    static const char* const n[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    return n;
}
// Masque de 12 bits (bit 0 = C) de la gamme `scale` sur la tonique `key`.
inline int scaleMask(int key, int scale) {
    const auto& s = scales()[std::clamp(scale, 0, kScales - 1)];
    int m = 0;
    for (int i = 0; i < s.n; ++i) m |= 1 << pcOf(key + s.iv[i]);
    return m;
}
// Valeurs de autotune.html (PRE).
struct Preset { const char* name; float speed, tol, amt, trans, form, vd, vr; int harm; float hv, dbl; };
inline const Preset* presets() {
    static const Preset p[kPresets] = {
        {"Naturel", 120, 25, 1, 0, 0, 0, 5.5f, 0, .5f, 0},
        {"Pop", 45, 10, 1, 0, 0, 0, 5.5f, 0, .5f, .2f},
        {"Trap moderne", 12, 0, 1, 0, 0, 0, 5.5f, 0, .5f, .3f},
        {"Robot", 0, 0, 1, 0, 0, 0, 5.5f, 0, .5f, 0},
        {"Hyperpop", 0, 0, 1, 0, 3, 0, 5.5f, 4, .3f, .45f},
        {"Grosse voix", 25, 5, 1, 0, -3, 0, 5.5f, 0, .5f, .2f},
        {"Harmonie", 20, 5, 1, 0, 0, 12, 5.2f, 1, .55f, .25f}};
    return p;
}

struct Params {
    float speed = 12.f, tol = 0.f, amt = 1.f, trans = 0.f, form = 0.f, vd = 0.f, vr = 5.5f, hv = .5f, dbl = .3f, mix = 1.f;
    bool formPres = true;
    int harm = 0;      // 0 off, 1 tierce, 2 quinte, 3 tierce basse, 4 octave haute, 5 octave basse
    int key = -1;      // -1 auto, 0..11 = C..B
    int scale = 1;
    int flips = 0;     // notes (bit 0 = C) inversées à la main par rapport à la gamme
    bool lowLat = false;   // mode Direct : latence <= 256 échantillons, sans décalage de formants
    bool orbe = false;     // algorithme « Orbe » (TuneCore.h) : mêmes latences déclarées que l'ancien moteur
    float expr = 1.f;      // Orbe : expression (vibrato) conservée quand la vitesse est naturelle
};

// ---------------------------------------------------------------- FFT et décaleur de formants
struct Fft {
    int n = 0;
    std::vector<float> c, s;
    std::vector<int> rv;
    void init(int N) {
        n = N; c.resize((size_t)N / 2); s.resize((size_t)N / 2); rv.resize((size_t)N);
        int lg = 0; while ((1 << lg) < N) ++lg;
        for (int i = 0; i < N / 2; ++i) { c[(size_t)i] = std::cos(2.f * kPi * (float)i / (float)N); s[(size_t)i] = -std::sin(2.f * kPi * (float)i / (float)N); }
        for (int i = 0; i < N; ++i) { int r = 0; for (int b = 0; b < lg; ++b) if (i >> b & 1) r |= 1 << (lg - 1 - b); rv[(size_t)i] = r; }
    }
    void run(float* re, float* im, bool inv) const {
        for (int i = 0; i < n; ++i) { const int j = rv[(size_t)i]; if (j > i) { std::swap(re[i], re[j]); std::swap(im[i], im[j]); } }
        for (int len = 2; len <= n; len <<= 1) {
            const int half = len / 2, step = n / len;
            for (int i = 0; i < n; i += len)
                for (int k = 0; k < half; ++k) {
                    const float wr = c[(size_t)(k * step)], wi = inv ? -s[(size_t)(k * step)] : s[(size_t)(k * step)];
                    const int a = i + k, b = a + half;
                    const float tr = re[b] * wr - im[b] * wi, ti = re[b] * wi + im[b] * wr;
                    re[b] = re[a] - tr; im[b] = im[a] - ti; re[a] += tr; im[a] += ti;
                }
        }
        if (inv) { const float k = 1.f / (float)n; for (int i = 0; i < n; ++i) { re[i] *= k; im[i] *= k; } }
    }
};

// Déplace l'enveloppe spectrale (les formants) d'un facteur 1/s : gain(k) = E(k*s)/E(k), enveloppe cepstrale lissée.
// STFT 75 % de recouvrement, fenêtres en racine de Hann. Latence fixe N-1, transparent quand s = 1.
struct Formant {
    int N = 1024, H = 256, M = 1023, Q = 40, t = 0;
    float s = 1.f, sNext = 1.f;
    Fft fft;
    std::vector<float> win, in, acc, xr, xi, cr, ci;
    void init(double sr) {
        N = sr >= 88000.0 ? 2048 : 1024; H = N / 4; M = N - 1; Q = std::max(8, (int)std::lround(sr * 0.00085)); t = 0; s = sNext = 1.f;
        fft.init(N);
        win.resize((size_t)N);
        for (int j = 0; j < N; ++j) win[(size_t)j] = std::sqrt(0.5f - 0.5f * std::cos(2.f * kPi * (float)j / (float)N));
        for (auto* v : {&in, &acc, &xr, &xi, &cr, &ci}) v->assign((size_t)N, 0.f);
    }
    int latency() const { return N - 1; }
    float process(float x, float scale) {
        sNext = scale;
        in[(size_t)t] = x;
        t = (t + 1) & M;
        if ((t & (H - 1)) == 0) frame();
        const float y = acc[(size_t)t];
        acc[(size_t)t] = 0.f;
        return y;
    }
    void frame() {
        s = sNext;
        for (int j = 0; j < N; ++j) { xr[(size_t)j] = in[(size_t)((t + j) & M)] * win[(size_t)j]; xi[(size_t)j] = 0.f; }
        if (std::fabs(s - 1.f) > 0.004f) {
            fft.run(xr.data(), xi.data(), false);
            const int B = N / 2;
            for (int k = 0; k <= B; ++k) cr[(size_t)k] = std::log(std::sqrt(xr[(size_t)k] * xr[(size_t)k] + xi[(size_t)k] * xi[(size_t)k]) + 1e-7f);
            for (int k = 1; k < B; ++k) cr[(size_t)(N - k)] = cr[(size_t)k];
            std::fill(ci.begin(), ci.end(), 0.f);
            fft.run(cr.data(), ci.data(), true);
            for (int n = 0; n < N; ++n) {
                const int q = n <= N / 2 ? n : N - n;
                cr[(size_t)n] *= q <= Q ? 0.5f * (1.f + std::cos(kPi * (float)q / (float)(Q + 1))) : 0.f;
                ci[(size_t)n] = 0.f;
            }
            fft.run(cr.data(), ci.data(), false);
            for (int k = 1; k < B; ++k) {
                const float kk = (float)k * s;
                float e;
                if (kk >= (float)B) e = cr[(size_t)B];
                else { const int i0 = (int)kk; const float f = kk - (float)i0; e = cr[(size_t)i0] * (1.f - f) + cr[(size_t)i0 + 1] * f; }
                const float g = std::exp(cl(e - cr[(size_t)k], -1.6f, 1.6f));
                xr[(size_t)k] *= g; xi[(size_t)k] *= g; xr[(size_t)(N - k)] *= g; xi[(size_t)(N - k)] *= g;
            }
            fft.run(xr.data(), xi.data(), true);
        }
        for (int j = 0; j < N; ++j) acc[(size_t)((t + j) & M)] += xr[(size_t)j] * win[(size_t)j] * 0.5f;
    }
};


// ---------------------------------------------------------------- décaleur à raccords synchrones (mode Direct)
// Code partagé avec Voile, les un-potard, Looks et Atelier : Source/Dsp/Splicer.h (dsp::Splicer, inchangé, même son).
using Splicer = dsp::Splicer;

// ---------------------------------------------------------------- détection de hauteur YIN
struct Biquad {
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void lowpass(double sr, double fc) {
        const double w = 2 * 3.14159265358979 * fc / sr, al = std::sin(w) / (2 * 0.7071), c = std::cos(w), a0 = 1 + al;
        b0 = (float)((1 - c) / 2 / a0); b1 = (float)((1 - c) / a0); b2 = b0; a1 = (float)(-2 * c / a0); a2 = (float)((1 - al) / a0); z1 = z2 = 0;
    }
    float process(float x) { const float y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
};

struct Detector {
    static constexpr int W = 512, kRing = 2048;
    int dec = 2, tMax = 440, tMin = 20, head = 0, phase = 0;
    float decSr = 24000.f, thr = 0.2f, f = 0.f, conf = 0.f;
    Biquad lp1, lp2;
    std::vector<float> ring, seg, cm;
    void init(double sr) {
        dec = sr >= 88000.0 ? 4 : 2; decSr = (float)(sr / dec);
        tMax = (int)(decSr / 55.f) + 2; tMin = std::max(2, (int)(decSr / 1150.f));
        ring.assign(kRing, 0.f); seg.assign((size_t)(W + tMax + 2), 0.f); cm.assign((size_t)tMax + 3, 1.f);
        lp1.lowpass(sr, 3000.0); lp2.lowpass(sr, 3000.0); head = phase = 0; f = conf = 0.f;
    }
    void push(float x) {
        const float y = lp2.process(lp1.process(x));
        if (++phase >= dec) { phase = 0; ring[(size_t)head] = y; head = (head + 1) & (kRing - 1); }
    }
    // Vrai si voisé ; f en Hz (interpolation parabolique), conf = 1 - d' au creux.
    bool analyze() {
        const int n = W + tMax;
        for (int i = 0; i < n; ++i) seg[(size_t)i] = ring[(size_t)((head - n + i) & (kRing - 1))];
        const float* a = &seg[(size_t)tMax];
        double e = 0.0;
        for (int j = 0; j < W; ++j) e += (double)a[j] * a[j];
        if (std::sqrt(e / W) < 0.002) return false;
        float cum = 0.f;
        cm[0] = 1.f;
        for (int tau = 1; tau <= tMax; ++tau) {
            float d0 = 0.f, d1 = 0.f, d2 = 0.f, d3 = 0.f;
            const float* b = a - tau;
            for (int j = 0; j < W; j += 4) {
                const float v0 = a[j] - b[j], v1 = a[j + 1] - b[j + 1], v2 = a[j + 2] - b[j + 2], v3 = a[j + 3] - b[j + 3];
                d0 += v0 * v0; d1 += v1 * v1; d2 += v2 * v2; d3 += v3 * v3;
            }
            const float d = d0 + d1 + d2 + d3;
            cum += d;
            cm[(size_t)tau] = cum > 1e-9f ? d * (float)tau / cum : 1.f;
            const int p = tau - 1;   // creux candidat : premier minimum local sous le seuil
            if (p > tMin && cm[(size_t)p] < thr && cm[(size_t)p] <= cm[(size_t)p - 1] && cm[(size_t)tau] > cm[(size_t)p]) {
                const float a0 = cm[(size_t)p - 1], b0 = cm[(size_t)p], c0 = cm[(size_t)tau], den = a0 - 2.f * b0 + c0;
                float t = (float)p;
                if (den > 1e-9f) t += cl(0.5f * (a0 - c0) / den, -1.f, 1.f);
                f = decSr / t; conf = 1.f - b0;
                return true;
            }
        }
        return false;
    }
};

// Suivi de hauteur : médiane de 5, rejet des sauts isolés (il faut 3 trames concordantes pour changer de registre).
struct Tracker {
    static constexpr int kMed = 3;
    Detector det;
    float buf[5] = {}, midi = 0.f, conf = 0.f, freq = 0.f, candM = 0.f, lastMed = 0.f;
    int nb = 0, gap = 99, candRun = 0, gapMax = 8, candN = 3;
    void init(double sr, int hop = 256) { det.init(sr); gapMax = std::max(2, 8 * 256 / hop); candN = std::max(2, 3 * 256 / hop); nb = 0; gap = 99; candRun = 0; midi = conf = freq = 0.f; }
    bool step() {
        bool ok = det.analyze();
        float m = ok ? f2m(det.f) : 0.f;
        if (ok && (m < 33.f || m > 88.f)) ok = false;
        if (ok && nb > 0 && gap < gapMax && std::fabs(m - lastMed) > 3.5f) {
            candRun = (candRun > 0 && std::fabs(m - candM) < 1.f) ? candRun + 1 : 1;
            candM = m;
            if (candRun >= candN) { nb = 0; candRun = 0; } else ok = false;
        } else if (ok) candRun = 0;
        if (ok) {
            gap = 0;
            if (nb < 5) buf[nb++] = m; else { std::memmove(buf, buf + 1, 4 * sizeof(float)); buf[4] = m; }
            const int k = std::min(nb, kMed);
            float t[5];
            for (int i = 0; i < k; ++i) t[i] = buf[nb - k + i];
            std::sort(t, t + k);
            midi = lastMed = t[k / 2]; conf = det.conf; freq = det.f;
            return true;
        }
        if (++gap >= gapMax) { nb = 0; candRun = 0; }
        return false;
    }
};

// ---------------------------------------------------------------- estimation de tonalité (Krumhansl-Kessler)
struct KeyEst {
    float h[12] = {}, tot = 0.f;
    bool locked = false;
    int root = -1, cand = -1, cnt = 0, fam = -1;
    void reset() { std::fill(h, h + 12, 0.f); tot = 0.f; locked = false; root = cand = -1; cnt = 0; }
    void decay(float k) { for (auto& v : h) v *= k; tot *= k; }
    void add(float m, float conf, float scale) {
        const int p = pcOf((int)std::lround(m));
        const float dev = std::fabs(m - std::round(m)), w = scale * conf * (dev < 0.35f ? 1.f : 0.35f);
        h[p] += w; tot += w;
    }
    // Évalue les 12 toniques de la famille ; renvoie le meilleur, r1, r2 (meilleur des autres) et le score de `root`.
    void score(bool minorLike, int& best, float& r1, float& r2, float* r) const {
        static const float maj[12] = {6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f, 2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f};
        static const float mnr[12] = {6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f, 2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f};
        const float* pr = minorLike ? mnr : maj;
        float hm = 0.f, pm = 0.f;
        for (int i = 0; i < 12; ++i) { hm += h[i]; pm += pr[i]; }
        hm /= 12.f; pm /= 12.f;
        float hv = 0.f, pv = 0.f;
        for (int i = 0; i < 12; ++i) { hv += (h[i] - hm) * (h[i] - hm); pv += (pr[i] - pm) * (pr[i] - pm); }
        best = 0; r1 = -2.f; r2 = -2.f;
        for (int t = 0; t < 12; ++t) {
            float c = 0.f;
            for (int i = 0; i < 12; ++i) c += (h[i] - hm) * (pr[pcOf(i - t)] - pm);
            r[t] = hv > 1e-9f ? c / std::sqrt(hv * pv) : 0.f;
            if (r[t] > r1) { r2 = r1; r1 = r[t]; best = t; } else if (r[t] > r2) r2 = r[t];
        }
    }
    // Appelé toutes les ~85 ms. Verrouillage : poids suffisant, >= 3 classes de hauteur, r1 >= .6, marge >= .06, 5 évaluations concordantes.
    // Changement de tonalité : r1 dépasse le score de la tonalité verrouillée de .08, pendant 10 évaluations.
    void update(bool minorLike) {
        int best; float r1, r2, r[12];
        score(minorLike, best, r1, r2, r);
        int distinct = 0;
        for (float v : h) if (v >= 0.06f * tot) ++distinct;
        const bool ok = tot >= 150.f && distinct >= 3 && r1 >= 0.6f && r1 - r2 >= 0.06f;
        if (!locked) {
            if (ok) { if (best == cand) ++cnt; else { cand = best; cnt = 1; } if (cnt >= 5) { locked = true; root = best; cnt = 0; } }
            else cnt = 0;
        } else if (best != root && ok && r1 - r[root] >= 0.08f) {
            if (best == cand) ++cnt; else { cand = best; cnt = 1; }
            if (cnt >= 10) { root = best; cnt = 0; }
        } else cnt = 0;
    }
};

// ---------------------------------------------------------------- télémétrie pour l'interface
struct Telemetry {
    static constexpr int N = 1024;
    std::atomic<float> det[N], tgt[N], out[N];   // notes MIDI (0 = absent) : chantée, cible, sortie
    std::atomic<int> wr{0}, mode{2}, root{-1}, mask{0}, scale{1}, flips{0};   // mode : 0 manuel, 1 auto verrouillé, 2 en recherche
    std::atomic<float> corr{0.f}, hopSec{0.0058f};
    Telemetry() { for (int i = 0; i < N; ++i) det[i] = tgt[i] = out[i] = 0.f; }
};

// Contournement à latence compensée : sortie = entrée retardée de `latency` échantillons (exact).
struct BypassDelay {
    voile::Delay l, r; int lat = 0;
    void init(int latency) { lat = latency; l.init(latency + 8); r.init(latency + 8); }
    void process(float* a, float* b, int n, bool stereoIn) {
        for (int i = 0; i < n; ++i) { const float x0 = a[i], x1 = stereoIn ? b[i] : x0; l.push(x0); r.push(x1); a[i] = l.read((float)lat); b[i] = r.read((float)lat); }
    }
};

// ---------------------------------------------------------------- moteur
class Engine {
public:
    void prepare(double sampleRate) {
        sr = sampleRate;
        hop = sr >= 88000.0 ? 256 : 128;   // 2,9 ms (44,1/48 k) : suit mieux une voix qui bouge
        auto hn = [&](int k) { return std::max(1, (int)std::lround(k * 256.0 / hop)); };   // durées exprimées en trames de 256 échantillons
        nHold = hn(4); nSil = hn(6); nReset = hn(8); keyEvery = hn(16); keDecay = (float)std::pow(0.9997, (double)hop / 256.0); keAdd = (float)hop / 256.f;
        win = 2 * (int)std::lround(sr * 0.016);
        trk.init(sr, hop);
        for (auto& c : ch) {
            c.fm.init(sr); c.fh.init(sr);
            c.sh.init(win); c.shH.init(win);
            c.dry.init(win / 2 + c.fm.latency() + 16);
        }
        latQ = win / 2 + ch[0].fm.latency();
        latD = std::min(256, (int)std::lround(sr * 0.005));   // retard au repos du mode Direct (~5 ms, jamais plus de 256 échantillons)
        spl.init(2, latD); splH.init(2, latD); splD[0].init(1, latD); splD[1].init(1, latD);
        lowLat = P.lowLat; lat = lowLat ? latD : latQ;
        for (auto& d : dsh) d.init(win);
        for (auto& d : ddl) d.init((int)(sr * 0.05) + latQ + 16);
        core.prepare(sr, latD, win / 2);   // Qualité : anticipation win/2 + formants STFT = latQ, exactement comme l'ancien moteur
        core.setQuality(!lowLat); core.reset(); orbeOn = P.orbe;
        ke.reset(); ke.fam = -1;
        hopC = keTick = unv = 0; curNote = -1;
        shiftC = curMain = curH = tgtMain = tgtH = 0.f; stepMain = stepH = 0.f; itvS = 0.f; hvS = dblS = 0.f; mixS = P.mix;
        vph = 0.f; voicedT = 0.f; held = 0.f; sMain = sHarm = 1.f;
        tel.wr = 0; tel.mode = 2; tel.root = -1; tel.mask = 0;
        for (int i = 0; i < Telemetry::N; ++i) tel.det[i] = tel.tgt[i] = tel.out[i] = 0.f;
        tel.hopSec = (float)(hop / sr);
    }
    int latency() const { return lat; }
    int latencyQuality() const { return latQ; }   // retard déclaré en mode Qualité (formants)
    int latencyDirect() const { return latD; }    // retard déclaré en mode Direct (<= 256)
    void set(const Params& p) {
        if (p.lowLat != lowLat) setLowLatency(p.lowLat);
        if (p.orbe != orbeOn) {   // changement d'algorithme : mémoires repartent à zéro (sans allocation)
            orbeOn = p.orbe; core.setQuality(!lowLat); core.reset(); setLowLatency(lowLat);
            curMain = curH = tgtMain = tgtH = stepMain = stepH = shiftC = 0.f;
        }
        P = p;
    }
    // Position de timeline (échantillons) du début du bloc suivant, éditions de notes et capture (mode Orbe). Appelé avant process().
    void setTimeline(double pos, bool playing, const ned::EditList* e, ned::Capture* cap) { tlPos = pos; tlPlay = playing; edits = e; capture = cap; }
    const tc::TuneCore& orbeCore() const { return core; }
    // Change de mode sans allocation : la mémoire du mode entrant est remise à zéro (une coupure brève, pas de bruit résiduel).
    void setLowLatency(bool low) {
        lowLat = low; lat = low ? latD : latQ;
        if (low) { spl.init(2, latD); splH.init(2, latD); splD[0].init(1, latD); splD[1].init(1, latD); }
        else for (auto& c : ch) { c.fm.init(sr); c.fh.init(sr); c.sh.init(win); c.shH.init(win); }
        core.setQuality(!low);
        for (auto& c : ch) c.dry.init(win / 2 + c.fm.latency() + 16);
        for (auto& d : dsh) d.init(win);
        for (auto& d : ddl) d.init((int)(sr * 0.05) + latQ + 16);
    }

    // Traite n échantillons stéréo sur place.
    void process(float* l, float* r, int n) {
        if (P.orbe) { processOrbe(l, r, n); return; }
        float* io[2] = {l, r};
        for (int i = 0; i < n; ++i) {
            float x[2];
            for (int c = 0; c < 2; ++c) { x[c] = io[c][i]; if (!std::isfinite(x[c])) x[c] = 0.f; }
            const float mid = 0.5f * (x[0] + x[1]);
            trk.det.push(mid);
            if (++hopC >= hop) { hopC = 0; hopUpdate(); }
            curMain += stepMain; curH += stepH;
            mixS += (P.mix - mixS) * 0.002f;
            const float ratio = std::exp2(curMain * (1.f / 1200.f));
            const bool hAct = hvS > 0.002f, dAct = dblS > 0.002f;
            wake(hAct, dAct);
            const float rH = hAct ? std::exp2(curH * (1.f / 1200.f)) : 1.f;
            float sd[2] = {0.f, 0.f}, hw[2] = {0.f, 0.f}, ws[2];
            if (lowLat) {
                // Direct : raccords synchrones à la période, pas de formants. Le retard au repos est latD pour toutes les voix.
                spl.process(x, ws, ratio);
                if (dAct) {
                    const float dc = 5.f + 10.f * P.dbl, m1 = mid;
                    float d1, d2;
                    splD[0].process(&m1, &d1, std::exp2((curMain + dc) * (1.f / 1200.f)));
                    splD[1].process(&m1, &d2, std::exp2((curMain - dc) * (1.f / 1200.f)));
                    ddl[0].push(d1); ddl[1].push(d2);
                    const float A = ddl[0].read((float)(int)(sr * 0.011)), B = ddl[1].read((float)(int)(sr * 0.017));
                    const float lev = 0.55f * dblS, wd = 0.5f + 0.5f * P.dbl;
                    sd[0] = lev * (A + (1.f - wd) * B); sd[1] = lev * (B + (1.f - wd) * A);
                } else { splD[0].feed(&mid); splD[1].feed(&mid); }
                if (hAct) { splH.process(x, hw, rH); } else splH.feed(x);
            } else {
                if (dAct) {
                    const float dc = 5.f + 10.f * P.dbl;
                    const float d1 = dsh[0].process(mid, std::exp2((curMain + dc) * (1.f / 1200.f)));
                    const float d2 = dsh[1].process(mid, std::exp2((curMain - dc) * (1.f / 1200.f)));
                    ddl[0].push(d1); ddl[1].push(d2);
                    const float A = ddl[0].read((float)(ch[0].fm.latency() + (int)(sr * 0.011))), B = ddl[1].read((float)(ch[0].fm.latency() + (int)(sr * 0.017)));
                    const float lev = 0.55f * dblS, wd = 0.5f + 0.5f * P.dbl;
                    sd[0] = lev * (A + (1.f - wd) * B); sd[1] = lev * (B + (1.f - wd) * A);
                }
            }
            for (int c = 0; c < 2; ++c) {
                Chan& k = ch[c];
                k.dry.push(x[c]);
                float wet;
                if (lowLat) {
                    wet = ws[c] + sd[c] + (hAct ? 0.8f * hvS * hw[c] : 0.f);
                } else {
                    // Qualité : le décaleur aligne lui-même le sec (décalage < 0,5 cent) ; formants STFT en sortie
                    const float y = k.fm.process(k.sh.process(x[c], ratio), sMain);
                    wet = y + sd[c];
                    if (hAct) wet += 0.8f * hvS * k.fh.process(k.shH.process(x[c], rH), sHarm);
                }
                float o = k.dry.read((float)lat) * (1.f - mixS) + wet * mixS;
                if (!std::isfinite(o)) o = 0.f;
                io[c][i] = cl(o, -8.f, 8.f);
            }
        }
        tlPos += n;
    }

    Telemetry tel;
    Tracker trk;
    KeyEst ke;
    Params P;

private:
    struct Chan { voile::HeadShifter sh, shH; Formant fm, fh; voile::Delay dry; };
    Splicer spl, splH, splD[2];
    bool lowLat = false;
    int latQ = 0, latD = 0;


    // ------------------------------------------------------------ chemin « Orbe »
    // Voix principale : TuneCore (analyse + correction + resynthèse à formants préservés). Harmonie et doublage : décaleurs
    // existants, pilotés par la correction de TuneCore. Qualité : étage de formants STFT après TuneCore (latence totale = latQ).
    void processOrbe(float* l, float* r, int n) {
        int i = 0;
        while (i < n) {
            const int c = std::min(n - i, std::min(core.samplesToHop(), kChunk));
            orbeParams(i + c);
            std::memcpy(wl, l + i, sizeof(float) * (size_t)c); std::memcpy(wr, r + i, sizeof(float) * (size_t)c);
            for (int j = 0; j < c; ++j) { if (!std::isfinite(wl[j])) wl[j] = 0.f; if (!std::isfinite(wr[j])) wr[j] = 0.f; }
            float* in[2] = {l + i, r + i};
            for (int j = 0; j < c; ++j) for (int k = 0; k < 2; ++k) if (!std::isfinite(in[k][j])) in[k][j] = 0.f;
            core.process(wl, wr, c);
            float* io[2] = {l + i, r + i};
            for (int j = 0; j < c; ++j) {
                float x[2] = {io[0][j], io[1][j]};
                const float mid = 0.5f * (x[0] + x[1]);
                curMain += stepMain; curH += stepH;
                mixS += (P.mix - mixS) * 0.002f;
                const bool hAct = hvS > 0.002f, dAct = dblS > 0.002f;
                wake(hAct, dAct);
                const float rH = hAct ? std::exp2(curH * (1.f / 1200.f)) : 1.f;
                float sd[2] = {0.f, 0.f}, hw[2] = {0.f, 0.f};
                if (lowLat) {
                    if (dAct) {
                        const float dc = 5.f + 10.f * P.dbl, m1 = mid; float d1, d2;
                        splD[0].process(&m1, &d1, std::exp2((curMain + dc) * (1.f / 1200.f)));
                        splD[1].process(&m1, &d2, std::exp2((curMain - dc) * (1.f / 1200.f)));
                        ddl[0].push(d1); ddl[1].push(d2);
                        const float A = ddl[0].read((float)(int)(sr * 0.011)), B = ddl[1].read((float)(int)(sr * 0.017));
                        const float lev = 0.55f * dblS, wd = 0.5f + 0.5f * P.dbl;
                        sd[0] = lev * (A + (1.f - wd) * B); sd[1] = lev * (B + (1.f - wd) * A);
                    } else { splD[0].feed(&mid); splD[1].feed(&mid); }
                    if (hAct) splH.process(x, hw, rH); else splH.feed(x);
                } else if (dAct) {
                    const float dc = 5.f + 10.f * P.dbl;
                    const float d1 = dsh[0].process(mid, std::exp2((curMain + dc) * (1.f / 1200.f)));
                    const float d2 = dsh[1].process(mid, std::exp2((curMain - dc) * (1.f / 1200.f)));
                    ddl[0].push(d1); ddl[1].push(d2);
                    const float A = ddl[0].read((float)(ch[0].fm.latency() + (int)(sr * 0.011))), B = ddl[1].read((float)(ch[0].fm.latency() + (int)(sr * 0.017)));
                    const float lev = 0.55f * dblS, wd = 0.5f + 0.5f * P.dbl;
                    sd[0] = lev * (A + (1.f - wd) * B); sd[1] = lev * (B + (1.f - wd) * A);
                }
                const float main[2] = {wl[j], wr[j]};
                for (int k = 0; k < 2; ++k) {
                    Chan& q = ch[k];
                    q.dry.push(x[k]);
                    float wet;
                    if (lowLat) wet = main[k] + sd[k] + (hAct ? 0.8f * hvS * hw[k] : 0.f);
                    else {
                        wet = q.fm.process(main[k], sOrbe) + sd[k];
                        if (hAct) wet += 0.8f * hvS * q.fh.process(q.shH.process(x[k], rH), sHarm);
                    }
                    float o = q.dry.read((float)lat) * (1.f - mixS) + wet * mixS;
                    if (!std::isfinite(o)) o = 0.f;
                    io[k][j] = cl(o, -8.f, 8.f);
                }
            }
            i += c;
            if (core.samplesToHop() == core.hopSize()) orbeHop(i);
        }
        tlPos += n;
    }
    // Réglages de TuneCore pour la trame qui se termine à l'échantillon `endInBlock` du bloc.
    void orbeParams(int endInBlock) {
        tc::Params q;
        q.speedMs = P.speed; q.tolCents = P.tol; q.amount = P.amt; q.expr = P.expr; q.formants = P.formPres;
        q.mask = orbeMask; q.offsetCents = orbeOff;
        if (tlPlay && edits) {
            const double pos = tlPos + endInBlock - core.latency();   // instant de timeline de l'audio analysé pour cette trame
            if (const ned::Note* nt = edits->at(pos)) { q.editTarget = nt->target(); q.editAmount = nt->amount; q.editExpr = nt->expr; q.editSpeedMs = nt->speedMs; }
        }
        core.setParams(q);
    }
    void orbeHop(int endInBlock) {
        const tc::State& s = core.state();
        const bool voiced = s.voiced;
        if (tlPlay && capture) capture->push(tlPos + endInBlock - core.latency(), voiced ? s.pitch : 0.f);
        const float hopS = (float)(hop / sr), hopMs = hopS * 1000.f;
        const auto& sc = scales()[std::clamp(P.scale, 0, kScales - 1)];
        if (P.key < 0) {
            if (sc.n == 12) { ke.locked = true; ke.root = 0; }
            else {
                if (ke.fam != (int)sc.minorLike) { ke.fam = (int)sc.minorLike; ke.locked = false; ke.cnt = 0; ke.cand = -1; }
                ke.decay(keDecay);
                if (voiced) ke.add(s.pitch, std::max(0.5f, s.conf), keAdd);
                if (++keTick % keyEvery == 0) ke.update(sc.minorLike);
            }
        }
        int mode = 2, root = -1, base = 0;
        if (P.key >= 0) { mode = 0; root = P.key; base = scaleMask(root, P.scale); }
        else if (ke.locked) { mode = 1; root = ke.root; base = scaleMask(root, P.scale); }
        const int eff = mode == 2 ? 0 : ((base ^ P.flips) & 0xFFF);
        orbeMask = eff;
        unv = voiced ? 0 : unv + 1;
        voicedT = voiced ? voicedT + hopS : (unv > nReset ? 0.f : voicedT);
        vph += 2.f * kPi * P.vr * hopS; vph -= 2.f * kPi * std::floor(vph / (2.f * kPi));
        const float vib = P.vd * std::sin(vph) * cl((voicedT - 0.15f) / 0.35f, 0.f, 1.f);
        orbeOff = P.trans * 100.f + vib;
        curNote = s.note >= 0.f ? (int)std::lround(s.note) : -1;
        shiftC = voiced ? s.shift * 100.f : 0.f;
        tgtMain = s.shift * 100.f + orbeOff;
        int itv = 0; bool hOK = false;
        const int bn = curNote;
        auto ok = [&](int d) { return bn >= 0 && ((eff >> pcOf(bn + d)) & 1); };
        switch (P.harm) {
            case 1: if (bn >= 0 && eff) { itv = ok(4) && !ok(3) ? 4 : ok(3) ? 3 : 4; hOK = true; } break;
            case 2: itv = (bn >= 0 && eff && !ok(7) && ok(6)) ? 6 : 7; hOK = true; break;
            case 3: if (bn >= 0 && eff) { itv = ok(-4) && !ok(-3) ? -4 : ok(-3) ? -3 : -4; hOK = true; } break;
            case 4: itv = 12; hOK = true; break;
            case 5: itv = -12; hOK = true; break;
            default: break;
        }
        const float hvT = (hOK && unv <= nSil) ? P.hv : 0.f;
        const float sm = 1.f - std::exp(-hopMs / 12.f);
        if (hvS < 0.01f) itvS = (float)itv; else if (hOK) itvS += ((float)itv - itvS) * sm;
        hvS += (hvT - hvS) * sm;
        dblS += ((P.dbl > 0.001f && unv <= nSil ? P.dbl : 0.f) - dblS) * sm;
        tgtH = tgtMain + itvS * 100.f;
        const float fu = std::exp2(P.form / 12.f), pH = std::exp2(tgtH / 1200.f);
        sOrbe = cl(1.f / fu, 0.4f, 2.5f);   // TuneCore garde l'enveloppe (ou la laisse suivre la hauteur si formPres = 0) : reste le décalage voulu
        sHarm = cl(pH / (P.formPres ? fu : pH * fu), 0.4f, 2.5f);
        stepMain = (tgtMain - curMain) / (float)hop; stepH = (tgtH - curH) / (float)hop;
        const int w = tel.wr.load(std::memory_order_relaxed) % Telemetry::N;
        tel.det[w] = voiced ? s.pitch : 0.f;
        tel.tgt[w] = (voiced && s.note >= 0.f) ? s.note : 0.f;
        tel.out[w] = voiced ? s.pitch + s.shift + orbeOff * 0.01f : 0.f;
        tel.mode = mode; tel.root = root; tel.mask = eff; tel.scale = P.scale; tel.flips = P.flips; tel.corr = voiced ? s.shift * 100.f : 0.f;
        tel.wr.store(w + 1, std::memory_order_release);
    }
    static constexpr int kChunk = 256;
    tc::TuneCore core;
    bool orbeOn = false, tlPlay = false;
    double tlPos = 0.0;
    const ned::EditList* edits = nullptr;
    ned::Capture* capture = nullptr;
    int orbeMask = 0;
    float orbeOff = 0.f, sOrbe = 1.f;
    float wl[kChunk] = {}, wr[kChunk] = {};

    // B1 : au réveil de l'harmonie ou du doublage, leurs mémoires (figées pendant le silence) sont vidées, sans allocation :
    // sinon elles rejouent 20 à 40 ms de la phrase précédente (souffle, sifflante).
    void wake(bool hAct, bool dAct) {
        if (hAct && !wasH) for (auto& c : ch) { c.shH.init((int)c.shH.win); std::fill(c.fh.in.begin(), c.fh.in.end(), 0.f); std::fill(c.fh.acc.begin(), c.fh.acc.end(), 0.f); }
        if (dAct && !wasD) { for (auto& d : dsh) d.init((int)d.win); for (auto& d : ddl) std::fill(d.b.begin(), d.b.end(), 0.f); }
        wasH = hAct; wasD = dAct;
    }
    bool wasH = false, wasD = false;

    static int pickNote(float m, int mask, int cur) {
        int best = -1; float bd = 1e9f;
        const int b = (int)std::lround(m);
        for (int n = b - 7; n <= b + 7; ++n) {
            if (!((mask >> pcOf(n)) & 1)) continue;
            const float d = std::fabs(m - (float)n);
            if (d < bd) { bd = d; best = n; }
        }
        if (best < 0) return -1;
        if (cur >= 0 && cur != best && ((mask >> pcOf(cur)) & 1) && std::fabs(m - (float)cur) < bd + 0.12f && std::fabs(m - (float)cur) <= 7.f) return cur;
        return best;
    }

    void hopUpdate() {
        const bool voiced = trk.step();
        if (voiced) {   // période de la voix (échantillons) pour les raccords du mode Direct
            const float per = (float)sr / (440.f * std::exp2((trk.midi - 69.f) * (1.f / 12.f)));
            spl.setPeriod(per); splH.setPeriod(per); splD[0].setPeriod(per); splD[1].setPeriod(per);
        }
        const float hopS = (float)(hop / sr), hopMs = hopS * 1000.f;
        const auto& sc = scales()[std::clamp(P.scale, 0, kScales - 1)];
        // --- tonalité
        if (P.key < 0) {
            if (sc.n == 12) { ke.locked = true; ke.root = 0; }
            else {
                if (ke.fam != (int)sc.minorLike) { ke.fam = (int)sc.minorLike; ke.locked = false; ke.cnt = 0; ke.cand = -1; }
                ke.decay(keDecay);
                if (voiced) ke.add(trk.midi, trk.conf, keAdd);
                if (++keTick % keyEvery == 0) ke.update(sc.minorLike);
            }
        }
        int mode = 2, root = -1, base = 0;
        if (P.key >= 0) { mode = 0; root = P.key; base = scaleMask(root, P.scale); }
        else if (ke.locked) { mode = 1; root = ke.root; base = scaleMask(root, P.scale); }
        int eff = mode == 2 ? 0 : ((base ^ P.flips) & 0xFFF);
        // --- correction
        float desired = 0.f;
        if (voiced) {
            unv = 0;
            if (eff) {
                curNote = pickNote(trk.midi, eff, curNote);
                if (curNote >= 0) {
                    float e = ((float)curNote - trk.midi) * 100.f;
                    e = e > P.tol ? e - P.tol : e < -P.tol ? e + P.tol : 0.f;   // zone morte continue
                    desired = cl(P.amt * e, -600.f, 600.f);
                }
            } else curNote = -1;
            held = desired;
        } else {
            ++unv;
            desired = unv <= nHold ? held : 0.f;   // maintien ~12 ms puis retour doux à 0
            if (unv > nHold) curNote = -1;
        }
        const float tc = (voiced || unv <= nHold) ? P.speed : 50.f;
        const float al = tc > 0.5f ? 1.f - std::exp(-hopMs / tc) : 1.f;
        shiftC += al * (desired - shiftC);
        // --- vibrato (fondu d'entrée après le début d'une note)
        voicedT = voiced ? voicedT + hopS : (unv > nReset ? 0.f : voicedT);
        vph += 2.f * kPi * P.vr * hopS; vph -= 2.f * kPi * std::floor(vph / (2.f * kPi));
        const float vib = P.vd * std::sin(vph) * cl((voicedT - 0.15f) / 0.35f, 0.f, 1.f);
        tgtMain = shiftC + P.trans * 100.f + vib;
        // --- harmonie
        int itv = 0; bool hOK = false;
        const int bn = curNote >= 0 ? curNote : -1;
        auto ok = [&](int d) { return bn >= 0 && ((eff >> pcOf(bn + d)) & 1); };
        switch (P.harm) {
            case 1: if (bn >= 0 && eff) { itv = ok(4) && !ok(3) ? 4 : ok(3) ? 3 : 4; hOK = true; } break;
            case 2: itv = (bn >= 0 && eff && !ok(7) && ok(6)) ? 6 : 7; hOK = true; break;
            case 3: if (bn >= 0 && eff) { itv = ok(-4) && !ok(-3) ? -4 : ok(-3) ? -3 : -4; hOK = true; } break;
            case 4: itv = 12; hOK = true; break;
            case 5: itv = -12; hOK = true; break;
            default: break;
        }
        const float hvT = (hOK && unv <= nSil) ? P.hv : 0.f;
        const float sm = 1.f - std::exp(-hopMs / 12.f);   // lissage ~12 ms
        if (hvS < 0.01f) itvS = (float)itv; else if (hOK) itvS += ((float)itv - itvS) * sm;
        hvS += (hvT - hvS) * sm;
        dblS += ((P.dbl > 0.001f && unv <= nSil ? P.dbl : 0.f) - dblS) * sm;
        tgtH = tgtMain + itvS * 100.f;
        // --- formants : facteur de déplacement d'enveloppe s = p / F
        const float fu = std::exp2(P.form / 12.f), pM = std::exp2(tgtMain / 1200.f), pH = std::exp2(tgtH / 1200.f);
        sMain = cl(pM / (P.formPres ? fu : pM * fu), 0.4f, 2.5f);
        sHarm = cl(pH / (P.formPres ? fu : pH * fu), 0.4f, 2.5f);
        stepMain = (tgtMain - curMain) / (float)hop; stepH = (tgtH - curH) / (float)hop;
        // --- télémétrie
        const int w = tel.wr.load(std::memory_order_relaxed) % Telemetry::N;
        tel.det[w] = voiced ? trk.midi : 0.f;
        tel.tgt[w] = (voiced && curNote >= 0) ? (float)curNote : 0.f;
        tel.out[w] = voiced ? trk.midi + shiftC * 0.01f : 0.f;
        tel.mode = mode; tel.root = root; tel.mask = eff; tel.scale = P.scale; tel.flips = P.flips; tel.corr = voiced ? shiftC : 0.f;
        tel.wr.store(w + 1, std::memory_order_release);
    }

    double sr = 48000.0;
    int nHold = 4, nSil = 6, nReset = 8, keyEvery = 16;
    float keDecay = 0.9997f, keAdd = 1.f;
    int hop = 128, win = 1536, lat = 0, hopC = 0, keTick = 0, unv = 0, curNote = -1;
    std::array<Chan, 2> ch;
    std::array<voile::HeadShifter, 2> dsh;
    std::array<voile::Delay, 2> ddl;
    float shiftC = 0.f, curMain = 0.f, curH = 0.f, tgtMain = 0.f, tgtH = 0.f, stepMain = 0.f, stepH = 0.f, itvS = 0.f, hvS = 0.f, dblS = 0.f, mixS = 1.f;
    float vph = 0.f, voicedT = 0.f, held = 0.f, sMain = 1.f, sHarm = 1.f;
};

}  // namespace atn
