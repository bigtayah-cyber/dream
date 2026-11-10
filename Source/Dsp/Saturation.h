// Saturation partagée sans repliement (C++17 pur, sans JUCE, header-only, sans allocation dans process).
// Vague 2B (audit 2, point 10). Ré-implémenté depuis les publications, aucun code GPL :
//  - ADAA d'ordre 1 (Parker, Zavalishin, Le Bivic, DAFx 2016 ; Bilbao, Esqueda, Parker, Välimäki, IEEE SPL 2017) ;
//  - triode 12AX7 de Koren (« Improved VT models for SPICE simulations », 1996), étage cathode commune résolu hors ligne en table ;
//  - transformateur : saturation pilotée par le flux (intégrale à fuite du signal), donc plus forte dans le grave ;
//  - pré-/dé-emphase du 1er ordre exactement inverses (+N dB au-dessus de ~3 kHz avant la courbe, inverse après).
// Suréchantillonnage ×2 ou ×4 à phase linéaire (demi-bandes de Kaiser polyphasées), latence entière déclarée par latency().
//
// Utilisation (une instance par canal ; aucun état partagé, donc pas de lien stéréo nécessaire) :
//   dsp::sat::Saturator s; s.prepare(sr, 4);           // hors thread audio (alloue)
//   dsp::sat::Params p; p.curve = dsp::sat::Curve::Triode; p.driveDb = 12; s.setParams(p);   // thread audio OK (sans allocation)
//   s.process(buf, n);                                  // en place ; latence = s.latency() échantillons
#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

namespace dsp {
namespace sat {

constexpr double kPi = 3.14159265358979323846;

// ------------------------------------------------------------------ demi-bande à phase linéaire, polyphasée
// h[n], n = 0..4K-2, centre c = 2K-1 (= 0,5). Les coefficients non nuls hors centre sont aux indices pairs : 2K coefficients
// « latéraux » symétriques. Montée : y[2m] = 2·Σ h[2i]·x[m-i], y[2m+1] = x[m-K+1]. Descente : symétrique.
struct HalfbandDesign {
    std::vector<float> side;   // h[2i], i = 0..2K-1
    int K = 0;
    void design(int k, double beta) {
        K = k;
        const int N = 4 * K - 1, c = 2 * K - 1;
        auto i0 = [](double x) { double s = 1, t = 1; for (int j = 1; j < 30; ++j) { t *= (x / (2 * j)) * (x / (2 * j)); s += t; } return s; };
        side.assign((size_t)(2 * K), 0.f);
        double sum = 0;
        for (int i = 0; i < 2 * K; ++i) {
            const int n = 2 * i, d = n - c;   // d impair
            const double w = i0(beta * std::sqrt(std::max(0.0, 1.0 - (double)d * d / ((double)c * c)))) / i0(beta);
            const double h = std::sin(kPi * d / 2.0) / (kPi * d) * w;
            side[(size_t)i] = (float)h; sum += h;
        }
        for (auto& v : side) v = (float)(v * 0.5 / sum);   // gain continu exact : 0,5 (centre) + 0,5 (côtés)
        (void)N;
    }
};
struct HalfbandUp {
    const HalfbandDesign* d = nullptr;
    std::vector<float> h; int pos = 0, L = 1;
    void init(const HalfbandDesign* des) { d = des; L = 2 * des->K; h.assign((size_t)(2 * L), 0.f); pos = 0; }
    void reset() { std::fill(h.begin(), h.end(), 0.f); pos = 0; }
    void process(float x, float& y0, float& y1) {
        if (--pos < 0) pos = L - 1;
        h[(size_t)pos] = h[(size_t)(pos + L)] = x;
        const float* s = &h[(size_t)pos];
        const float* t = d->side.data();
        float a = 0.f, b = 0.f;
        for (int i = 0; i < L; i += 2) { a += t[i] * s[i]; b += t[i + 1] * s[i + 1]; }
        y0 = 2.f * (a + b);
        y1 = s[d->K - 1];
    }
};
struct HalfbandDown {
    const HalfbandDesign* d = nullptr;
    std::vector<float> he, ho; int pe = 0, po = 0, L = 1, Lo = 1;
    void init(const HalfbandDesign* des) { d = des; L = 2 * des->K; Lo = des->K; he.assign((size_t)(2 * L), 0.f); ho.assign((size_t)(2 * Lo), 0.f); pe = po = 0; }
    void reset() { std::fill(he.begin(), he.end(), 0.f); std::fill(ho.begin(), ho.end(), 0.f); pe = po = 0; }
    // v0, v1 : deux échantillons rapides consécutifs -> un échantillon lent
    float process(float v0, float v1) {
        if (--pe < 0) pe = L - 1;
        he[(size_t)pe] = he[(size_t)(pe + L)] = v0;
        const float* s = &he[(size_t)pe];
        const float* t = d->side.data();
        float a = 0.f, b = 0.f;
        for (int i = 0; i < L; i += 2) { a += t[i] * s[i]; b += t[i + 1] * s[i + 1]; }
        const float centre = ho[(size_t)(po + Lo - 1)];
        if (--po < 0) po = Lo - 1;
        ho[(size_t)po] = ho[(size_t)(po + Lo)] = v1;
        return a + b + 0.5f * centre;
    }
};

// Suréchantillonneur ×2 / ×4 (1 ou 2 étages). Étage 1 : K = 20 (79 coefficients, coupe à ~0,43 fs, réjection > 95 dB) ;
// étage 2 : K = 6 (transition large). Latence entière (retard pur) en échantillons de base.
class Oversampler {
public:
    void prepare(int factor) {
        stages = factor >= 4 ? 2 : 1;
        des[0].design(20, 10.0); des[1].design(6, 9.0);
        for (int s = 0; s < 2; ++s) { up[s].init(&des[s]); dn[s].init(&des[s]); }
        // retard de l'étage 2 : (4·K2 - 2) échantillons ×4 ; on le complète de 2 pour tomber sur un entier de base
        pad.assign(4, 0.f); padPos = 0;
        lat = (2 * des[0].K - 1) + (stages == 2 ? des[1].K : 0);
    }
    void reset() { for (int s = 0; s < 2; ++s) { up[s].reset(); dn[s].reset(); } std::fill(pad.begin(), pad.end(), 0.f); padPos = 0; }
    int factor() const { return 1 << stages; }
    int latency() const { return lat; }
    // f : non-linéarité appelée dans l'ordre du temps au rythme rapide
    template <class F> float process(float x, F&& f) {
        float a[2], b[4];
        up[0].process(x, a[0], a[1]);
        if (stages == 1) return dn[0].process(f(a[0]), f(a[1]));
        up[1].process(a[0], b[0], b[1]); up[1].process(a[1], b[2], b[3]);
        for (float& v : b) { v = f(v); const float o = pad[(size_t)padPos]; pad[(size_t)padPos] = v; padPos = (padPos + 1) & 1; v = o; }   // retard de 2
        const float c0 = dn[1].process(b[0], b[1]), c1 = dn[1].process(b[2], b[3]);
        return dn[0].process(c0, c1);
    }
private:
    HalfbandDesign des[2];
    HalfbandUp up[2];
    HalfbandDown dn[2];
    std::vector<float> pad; int padPos = 0;
    int stages = 1, lat = 0;
};

// ------------------------------------------------------------------ non-linéarités et primitives (ADAA d'ordre 1)
inline double lnCosh(double x) {
    x = std::fabs(x);
    if (x < 0.5) { const double x2 = x * x; return x2 * (0.5 - x2 * (1.0 / 12.0 - x2 * (1.0 / 45.0 - x2 * (17.0 / 2520.0)))); }
    return x + std::log1p(std::exp(-2.0 * x)) - 0.6931471805599453;
}

// Triode 12AX7 (Koren) en étage cathode commune : B+ = 250 V, Rp = 100 kΩ, polarisation -1,5 V. Entrée normalisée : x = 1 amène
// la grille à 0 V (début de conduction de grille, modélisée par une butée douce de 0,7 V). Sortie inversée et normalisée par le gain
// petit signal (pente 1 au repos). Table statique de 4097 points sur [-8, 8], calculée une fois (bissection), avec sa primitive exacte.
struct TriodeTable {
    static constexpr int N = 4096;
    static constexpr double X = 8.0;
    double f[N + 1], F[N + 1], h = 2 * X / N;
    static double koren(double vgk, double vpk) {
        const double mu = 100, ex = 1.4, kg1 = 1060, kp = 600, kvb = 300;
        const double e1 = (vpk / kp) * std::log1p(std::exp(std::min(60.0, kp * (1.0 / mu + vgk / std::sqrt(kvb + vpk * vpk)))));
        return e1 > 0 ? 2.0 * std::pow(e1, ex) / kg1 : 0.0;   // A (vérifié : -2 V, 250 V -> 0,95 mA, fiche 12AX7 : 1,2 mA)
    }
    static double plate(double vin) {
        const double bp = 250, rp = 100e3, vs = 1.5;
        double vg = -1.5 + vin * vs;
        if (vg > 0) vg = 0.7 * std::tanh(vg / 0.7);
        double lo = 0, hi = bp;   // Vp - B+ + Rp·Ip(Vp) croît avec Vp
        for (int i = 0; i < 60; ++i) { const double m = 0.5 * (lo + hi); (m - bp + rp * koren(vg, m) > 0 ? hi : lo) = m; }
        return 0.5 * (lo + hi);
    }
    TriodeTable() {
        const double v0 = plate(0.0), g0 = (plate(-1e-3) - plate(1e-3)) / 2e-3;
        for (int i = 0; i <= N; ++i) f[i] = (v0 - plate(-X + i * h)) / g0;
        F[0] = 0;
        for (int i = 1; i <= N; ++i) F[i] = F[i - 1] + 0.5 * h * (f[i - 1] + f[i]);
        const double F0 = prim(0.0);   // primitive nulle en 0 (précision du quotient ADAA)
        for (int i = 0; i <= N; ++i) F[i] -= F0;
    }
    double eval(double x) const {
        const double p = (std::clamp(x, -X, X) + X) / h; const int i = std::min(N - 1, (int)p); const double t = p - i;
        return f[i] + (f[i + 1] - f[i]) * t;
    }
    double prim(double x) const {   // exacte pour f linéaire par morceaux, prolongée par des constantes hors table
        const double xc = std::clamp(x, -X, X), p = (xc + X) / h; const int i = std::min(N - 1, (int)p); const double t = (p - i) * h;
        double s = F[i] + f[i] * t + 0.5 * (f[i + 1] - f[i]) / h * t * t;
        if (x > X) s += f[N] * (x - X); else if (x < -X) s += f[0] * (x + X);
        return s;
    }
    static const TriodeTable& get() { static const TriodeTable t; return t; }
};

enum class Curve { Tanh, Atan, Triode, Transformer };

struct Params {
    Curve curve = Curve::Tanh;
    float driveDb = 6.f;       // 0..36 dB
    float bias = 0.f;          // -1..1 : asymétrie statique (tanh/atan) -> harmoniques paires
    float dynBias = 0.f;       // 0..1 : asymétrie qui suit l'enveloppe (2e harmonique sur les attaques)
    float emphasisDb = 0.f;    // 0..12 : pré-emphase au-dessus de ~3 kHz avant la courbe, dé-emphase exacte après
    float mix = 1.f;           // 0..1 (sec aligné en latence)
    float outDb = 0.f;
    bool autoLevel = true;     // compensation statique : gain RMS de la courbe pour un sinus à -18 dBFS ramené à 0 dB
};

// Plateau aigu du 1er ordre, H(s) = (1 + s/w1)/(1 + s/w2) (gain 1 au continu, w2/w1 en haut), bilinéaire. inverse() = 1/H exact.
struct Shelf1 {
    float b0 = 1, b1 = 0, a1 = 0, z = 0;
    void set(double sr, double fc, double gDb, bool inverse) {
        const double g = std::pow(10.0, gDb / 20.0), w1 = 2 * kPi * fc / std::sqrt(g), w2 = 2 * kPi * fc * std::sqrt(g);
        const double k = 2 * sr;
        double nb0 = 1 + k / w1, nb1 = 1 - k / w1, na0 = 1 + k / w2, na1 = 1 - k / w2;
        if (inverse) { std::swap(nb0, na0); std::swap(nb1, na1); }
        b0 = (float)(nb0 / na0); b1 = (float)(nb1 / na0); a1 = (float)(na1 / na0);
    }
    float process(float x) { const float y = b0 * x + z; z = b1 * x - a1 * y; return y; }
};

class Saturator {
public:
    void prepare(double sampleRate, int osFactor = 4) {
        sr = sampleRate; os.prepare(osFactor); osr = sr * os.factor();
        dry.assign((size_t)os.latency() + 1, 0.f); dpos = 0;
        TriodeTable::get();
        P.driveDb = -999.f; Params p; setParams(p);
        reset();
    }
    void reset() {
        os.reset(); std::fill(dry.begin(), dry.end(), 0.f); dpos = 0;
        xp = 0; Fp = 0; flux = 0; env = 0; dcx = dcy = 0; pre.z = de.z = 0;
    }
    int latency() const { return os.latency(); }

    void setParams(const Params& p) {
        const bool curveChanged = p.curve != P.curve || p.driveDb != P.driveDb || p.bias != P.bias || p.emphasisDb != P.emphasisDb || p.autoLevel != P.autoLevel;
        P = p;
        P.driveDb = std::clamp(P.driveDb, 0.f, 48.f);
        P.bias = std::clamp(P.bias, -1.f, 1.f);
        P.mix = std::clamp(P.mix, 0.f, 1.f);
        drive = std::pow(10.f, P.driveDb / 20.f);
        pre.set(sr, 3000.0, P.emphasisDb, false); de.set(sr, 3000.0, P.emphasisDb, true);
        envA = (float)std::exp(-1.0 / (0.005 * sr)); envR = (float)std::exp(-1.0 / (0.08 * sr));
        fluxA = (float)std::exp(-2 * kPi * 25.0 / osr);
        dcC = (float)(1.0 - 2 * kPi * 8.0 / sr);
        outGain = std::pow(10.f, P.outDb / 20.f);
        if (curveChanged) comp = P.autoLevel ? levelCompensation() : 1.f;
    }

    float processSample(float x) {
        if (!std::isfinite(x)) x = 0.f;
        dry[(size_t)dpos] = x; if (++dpos >= (int)dry.size()) dpos = 0;
        const float d = dry[(size_t)dpos];
        const float ax = std::fabs(x);
        env = ax > env ? envA * env + (1 - envA) * ax : envR * env;
        bnow = P.bias * 0.5f + P.dynBias * 0.8f * std::min(1.f, env * drive);
        const float in = pre.process(x) * drive;
        float y = os.process(in, [this](float v) { return shape(v); });
        y = de.process(y);
        // anti-continu (asymétries)
        const float hp = y - dcx + dcC * dcy; dcx = y; dcy = hp;
        float o = hp * comp / drive * outGain;
        if (!std::isfinite(o) || std::fabs(o) > 1e4f) { reset(); o = 0.f; }
        return d + P.mix * (o - d);
    }
    void process(float* x, int n) { for (int i = 0; i < n; ++i) x[i] = processSample(x[i]); }

    // Courbe statique (sans mémoire, sans emphase), pente 1 à l'origine à drive 0 dB : pour les tests et l'affichage.
    static double staticCurve(Curve c, double x, double b = 0.0) {
        switch (c) {
            case Curve::Atan: return std::atan(x + b) - std::atan(b);
            case Curve::Triode: return TriodeTable::get().eval(x);
            default: return std::tanh(x + b) - std::tanh(b);
        }
    }

private:
    Params P;
    Oversampler os;
    Shelf1 pre, de;
    std::vector<float> dry; int dpos = 0;
    double sr = 48000, osr = 192000, xp = 0, Fp = 0;
    float drive = 1, comp = 1, outGain = 1, env = 0, envA = 0, envR = 0, bnow = 0, flux = 0, fluxA = 0, dcx = 0, dcy = 0, dcC = 0.999f;

    // Primitive de la courbe courante (biais b) ; valeur exacte de la courbe pour le repli quand dx ~ 0.
    double prim(double x, double b) const {
        switch (P.curve) {
            case Curve::Atan: { const double u = x + b; return u * std::atan(u) - 0.5 * std::log1p(u * u) - std::atan(b) * x; }
            case Curve::Triode: return TriodeTable::get().prim(x);
            default: return lnCosh(x + b) - std::tanh(b) * x;
        }
    }
    float shape(float v) {
        if (P.curve == Curve::Transformer) {
            // flux = intégrale à fuite (25 Hz) : le noyau sature d'abord dans le grave ; y = x·(1 - 0,9·tanh²(Φ))
            flux = fluxA * flux + (1 - fluxA) * v;
            const float t = std::tanh(flux * 2.f);
            return std::tanh(v * (1.f - 0.9f * t * t) * 0.25f) * 4.f;   // butée douce finale pour les crêtes extrêmes
        }
        const double b = P.curve == Curve::Triode ? 0.0 : (double)bnow;
        const double x = v, F = prim(x, b), dx = x - xp;
        double y;
        if (std::fabs(dx) > 1e-5) y = (F - Fp) / dx;
        else y = staticCurve(P.curve, 0.5 * (x + xp), b);
        xp = x; Fp = F;
        return (float)y;
    }
    // Gain RMS de la courbe (sans continu) pour un sinus de crête 0,177 (-15 dBFS crête, ~-18 dBFS RMS) : compensé à 0 dB.
    float levelCompensation() const {
        if (P.curve == Curve::Transformer) return 1.f;
        constexpr int N = 64; const double a = 0.177 * drive, b = P.bias * 0.5;
        double m = 0, s = 0, v[N];
        for (int i = 0; i < N; ++i) { v[i] = staticCurve(P.curve, a * std::sin(2 * kPi * (i + 0.5) / N), b); m += v[i]; }
        m /= N;
        for (int i = 0; i < N; ++i) s += (v[i] - m) * (v[i] - m);
        const double rmsOut = std::sqrt(s / N), rmsIn = a / std::sqrt(2.0);
        return rmsOut > 1e-9 ? (float)std::clamp(rmsIn / rmsOut, 0.25, 8.0) : 1.f;
    }
};

}  // namespace sat
}  // namespace dsp
