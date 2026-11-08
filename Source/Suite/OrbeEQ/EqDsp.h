// Moteur d'Orbe EQ : 8 bandes biquads en phase minimale (double précision), latence nulle. Sans JUCE.
// Conception « appariée » (matched) : pôles par z = exp(sT) du prototype analogique RBJ, zéros ajustés pour que le module numérique
// égale le module analogique en DC, à Nyquist et à la fréquence centrale (Vicanek, « Matched Second Order Digital Filters »).
// Contrairement à la transformation bilinéaire, les aigus ne sont pas comprimés près de Nyquist (cloches, plateaux, coupe-haut à 44,1 kHz).
// Les paramètres (fréquence, gain, Q, activation, gain de sortie) sont lissés sur ~20 ms : plus de bruit de zipper sous automation.
#pragma once
#include "../OrbeDsp/OrbeDsp.h"

namespace eq {
constexpr int kBands = 8, kPresets = 9;
// Types de bande (ordre = choix du paramètre)
enum Type { Bell, LowShelf, HighShelf, LowCut, HighCut, Notch, kTypes };
inline odsp::Kind kind(int t) {
    static const odsp::Kind k[kTypes] = {odsp::Kind::Peak, odsp::Kind::LowShelf, odsp::Kind::HighShelf, odsp::Kind::LowCut, odsp::Kind::HighCut, odsp::Kind::Notch};
    return k[std::clamp(t, 0, kTypes - 1)];
}
inline bool hasGain(int t) { return t <= HighShelf; }

struct Band { float freq = 1000.f, gain = 0.f, q = 0.707f; int type = Bell; bool on = true; };
struct Params {
    Band b[kBands]; float out = 0.f;
    bool operator==(const Params& o) const {
        for (int i = 0; i < kBands; ++i) if (b[i].freq != o.b[i].freq || b[i].gain != o.b[i].gain || b[i].q != o.b[i].q || b[i].type != o.b[i].type || b[i].on != o.b[i].on) return false;
        return out == o.out;
    }
};

// ---- prototype analogique (RBJ) : H(s) = (n2 s² + n1 s + n0) / (d2 s² + d1 s + d0), s normalisé par w0
struct Analog { double n[3], d[3]; };
inline Analog analogProto(odsp::Kind k, double gDb, double q) {
    q = std::max(q, 0.05);
    const double A = std::pow(10.0, gDb / 40.0), sA = std::sqrt(A);
    switch (k) {
        case odsp::Kind::Peak:      return {{1, A / q, 1}, {1, 1 / (A * q), 1}};            // n0, n1, n2 rangés dans l'ordre s^0, s^1, s^2 ci-dessous
        case odsp::Kind::LowShelf:  return {{A * A, A * sA / q, A}, {1, sA / q, A}};
        case odsp::Kind::HighShelf: return {{A, A * sA / q, A * A}, {A, sA / q, 1}};
        case odsp::Kind::LowCut:    return {{0, 0, 1}, {1, 1 / q, 1}};
        case odsp::Kind::HighCut:   return {{1, 0, 0}, {1, 1 / q, 1}};
        case odsp::Kind::Notch:     return {{1, 0, 1}, {1, 1 / q, 1}};
        default:                    return {{0, 1 / q, 0}, {1, 1 / q, 1}};
    }
}
// |H|² à la pulsation normalisée W (= w / w0)
inline double analogMag2(const Analog& a, double W) {
    const double W2 = W * W, nr = a.n[0] - a.n[2] * W2, ni = a.n[1] * W, dr = a.d[0] - a.d[2] * W2, di = a.d[1] * W;
    return (nr * nr + ni * ni) / (dr * dr + di * di);
}
// Réponse analogique de référence (dB) d'une bande à la fréquence f

// exp(x) - 1 sans perte de précision (x complexe)
inline std::complex<double> cexpm1(std::complex<double> x) {
    const double a = x.real(), b = x.imag(), s = std::sin(b / 2);
    return {std::expm1(a) * std::cos(b) - 2 * s * s, std::exp(a) * std::sin(b)};
}
// Biquad apparié : module numérique = module analogique en DC, à w0 et à Nyquist.
inline odsp::Coef designMatched(odsp::Kind k, double sr, double f0, double gDb, double q) {
    const double nyq = 0.49 * sr;
    f0 = std::clamp(f0, 5.0, nyq);
    const Analog an = analogProto(k, gDb, q);
    const double w0T = 2 * odsp::kPi * f0 / sr;
    // pôles : racines du dénominateur analogique (s normalisé), puis z = exp(s w0T) ; angle plafonné pour rester sous Nyquist
    using C = std::complex<double>;
    const C disc = std::sqrt(C(an.d[1] * an.d[1] - 4 * an.d[2] * an.d[0], 0.0));
    C s1 = (-an.d[1] + disc) / (2 * an.d[2]), s2 = (-an.d[1] - disc) / (2 * an.d[2]);
    const double maxIm = 0.98 * odsp::kPi / w0T;   // pôle analogique au-delà de Nyquist : angle plafonné
    if (std::fabs(s1.imag()) > maxIm) { s1 = C(s1.real(), std::copysign(maxIm, s1.imag())); s2 = std::conj(s1); }
    const C e1 = cexpm1(s1 * w0T), e2 = cexpm1(s2 * w0T), z1 = e1 + 1.0, z2 = e2 + 1.0;   // z = exp(sT) ; e = z - 1 sans perte de précision
    const double a1 = -(z1 + z2).real(), a2 = (z1 * z2).real();
    const double D0 = ((-e1) * (-e2)).real(), D1 = ((1.0 + z1) * (1.0 + z2)).real();
    const double phi1 = std::pow(std::sin(w0T / 2), 2), phi0 = 1 - phi1, phi2 = 4 * phi0 * phi1;
    const double Dm = D0 * D0 * phi0 + D1 * D1 * phi1 - 4 * a2 * phi2;      // |D(w0)|²
    const double A0 = analogMag2(an, 0.0) * D0 * D0;
    const double A1 = analogMag2(an, odsp::kPi / w0T) * D1 * D1;
    const double A2 = (analogMag2(an, 1.0) * Dm - A0 * phi0 - A1 * phi1) / phi2;
    if (k == odsp::Kind::Notch) {   // zéros exactement sur le cercle unité en w0 (creux profond garanti) ; gain = compromis DC / Nyquist
        const double c = std::cos(w0T), gdc = std::sqrt(A0) / (2 - 2 * c), gny = std::sqrt(A1) / (2 + 2 * c), g = std::sqrt(gdc * gny);
        return {g, -2 * c * g, g, a1, a2};
    }
    const double W = 0.5 * (std::sqrt(A0) + std::sqrt(A1));
    const double b1 = 0.5 * (std::sqrt(A0) - std::sqrt(A1));
    const double b0 = 0.5 * (W + std::sqrt(std::max(0.0, W * W + A2)));
    const double b2 = b0 != 0 ? -A2 / (4 * b0) : 0.0;
    const odsp::Coef c{b0, b1, b2, a1, a2};
    if (!std::isfinite(b0 + b1 + b2 + a1 + a2) || std::fabs(a2) >= 1.0) return odsp::design(k, sr, f0, gDb, q);   // garde-fou : RBJ classique
    if (f0 < 0.15 * sr) return c;   // loin de Nyquist les deux conceptions coïncident
    // Près de Nyquist, quand un pôle analogique est au-delà de Nyquist, l'appariement peut creuser un défaut ; on garde la meilleure des deux
    // conceptions (appariée / bilinéaire) selon l'écart maximal à la courbe analogique sur 16 points log de 200 Hz à 0,49 fs (plancher -30 dB).
    const odsp::Coef rb = odsp::design(k, sr, f0, gDb, q);
    double e1m = 0, e2m = 0;
    for (int i = 0; i < 16; ++i) {
        const double f = 200.0 * std::pow(nyq / 200.0, i / 15.0), ref = std::max(-30.0, 10 * std::log10(analogMag2(an, f / f0) + 1e-24));
        e1m = std::max(e1m, std::fabs(std::max(-30.0, 20 * std::log10(c.mag(f, sr) + 1e-12)) - ref));
        e2m = std::max(e2m, std::fabs(std::max(-30.0, 20 * std::log10(rb.mag(f, sr) + 1e-12)) - ref));
    }
    return e2m < e1m ? rb : c;
}
inline odsp::Coef coefOf(const Band& b, double sr) { return designMatched(kind(b.type), sr, b.freq, hasGain(b.type) ? b.gain : 0.0, b.q); }
inline double analogDb(const Band& b, double f) {
    const double f0 = std::clamp((double)b.freq, 5.0, 1e9);
    return 10 * std::log10(analogMag2(analogProto(kind(b.type), hasGain(b.type) ? b.gain : 0.0, b.q), f / f0) + 1e-24);
}
// Réponse théorique (dB) de la chaîne à la fréquence f
inline double responseDb(const Params& p, double f, double sr) {
    double m = 1;
    for (auto& b : p.b) if (b.on) m *= coefOf(b, sr).mag(f, sr);
    return 20 * std::log10(m + 1e-12) + p.out;
}
inline double bandDb(const Band& b, double f, double sr) { return 20 * std::log10(coefOf(b, sr).mag(f, sr) + 1e-12); }

// Valeurs d'usine des 8 bandes
inline Band defaultBand(int i) {
    static const Band d[kBands] = {{80.f, 0.f, 0.707f, LowCut, true}, {120.f, 0.f, 0.707f, LowShelf, true}, {250.f, 0.f, 1.f, Bell, true}, {800.f, 0.f, 1.f, Bell, true},
                                   {2500.f, 0.f, 1.f, Bell, true}, {5000.f, 0.f, 1.f, Bell, true}, {9000.f, 0.f, 0.707f, HighShelf, true}, {16000.f, 0.f, 0.707f, HighCut, true}};
    return d[i];
}
inline Params factory() { Params p; for (int i = 0; i < kBands; ++i) { p.b[i] = defaultBand(i); p.b[i].on = (i != 0 && i != 7); } return p; }  // coupe-bas/haut éteints

struct Preset { const char* name; Band b[kBands]; float out; };
// Presets voix : chaque bande {Hz, dB, Q, type, actif}
inline const Preset* presets() {
    static const Preset P[kPresets] = {
        {"Neutre", {}, 0.f},
        {"Rap \xC2\xB7 pr\xC3\xA9sence", {{70, 0, .707f, LowCut, 1}, {120, 1.5f, .7f, LowShelf, 1}, {300, -3.f, 1.2f, Bell, 1}, {900, 0, 1, Bell, 0}, {3200, 3.f, 1.1f, Bell, 1}, {5500, 1.5f, 1.4f, Bell, 1}, {10000, 2.5f, .7f, HighShelf, 1}, {18000, 0, .7f, HighCut, 0}}, 0.f},
        {"Voix claire pop", {{90, 0, .707f, LowCut, 1}, {150, 0, .7f, LowShelf, 0}, {250, -2.5f, 1.f, Bell, 1}, {1200, -1.f, 1.f, Bell, 1}, {3500, 2.f, 1.f, Bell, 1}, {7000, 1.f, 1.f, Bell, 0}, {12000, 3.5f, .7f, HighShelf, 1}, {18000, 0, .7f, HighCut, 0}}, 0.f},
        {"Nettoyage boue", {{100, 0, .707f, LowCut, 1}, {150, 0, .7f, LowShelf, 0}, {220, -3.f, 1.4f, Bell, 1}, {420, -2.f, 1.8f, Bell, 1}, {2500, 0, 1, Bell, 0}, {5000, 0, 1, Bell, 0}, {9000, 0, .7f, HighShelf, 0}, {18000, 0, .7f, HighCut, 0}}, 0.f},
        {"Chaleur", {{60, 0, .707f, LowCut, 1}, {180, 3.f, .7f, LowShelf, 1}, {400, 1.5f, .9f, Bell, 1}, {1500, 0, 1, Bell, 0}, {3500, -1.5f, 1.f, Bell, 1}, {6000, 0, 1, Bell, 0}, {9000, -2.f, .7f, HighShelf, 1}, {14000, 0, .707f, HighCut, 1}}, 0.f},
        {"Air et brillance", {{80, 0, .707f, LowCut, 1}, {150, 0, .7f, LowShelf, 0}, {300, 0, 1, Bell, 0}, {1000, 0, 1, Bell, 0}, {4000, 1.5f, .9f, Bell, 1}, {8000, 2.f, 1.f, Bell, 1}, {13000, 4.f, .6f, HighShelf, 1}, {20000, 0, .7f, HighCut, 0}}, -1.f},
        {"Anti-nasal", {{80, 0, .707f, LowCut, 1}, {150, 0, .7f, LowShelf, 0}, {250, -1.f, 1, Bell, 1}, {1000, -3.5f, 2.2f, Bell, 1}, {3000, 1.f, 1, Bell, 1}, {5000, 0, 1, Bell, 0}, {9000, 1.5f, .7f, HighShelf, 1}, {18000, 0, .7f, HighCut, 0}}, 0.f},
        {"Voix t\xC3\xA9l\xC3\xA9phone", {{400, 0, .9f, LowCut, 1}, {150, 0, .7f, LowShelf, 0}, {900, 4.f, 1.2f, Bell, 1}, {1800, 3.f, 1.2f, Bell, 1}, {2500, 0, 1, Bell, 0}, {5000, 0, 1, Bell, 0}, {9000, 0, .7f, HighShelf, 0}, {3800, 0, .9f, HighCut, 1}}, 0.f},
        {"Anti-sifflant", {{90, 0, .707f, LowCut, 1}, {150, 0, .7f, LowShelf, 0}, {250, 0, 1, Bell, 0}, {1000, 0, 1, Bell, 0}, {3000, 0, 1, Bell, 0}, {7200, -4.f, 3.f, Bell, 1}, {10000, -1.5f, .7f, HighShelf, 1}, {18000, 0, .7f, HighCut, 0}}, 0.f},
    };
    return P;
}

inline Params presetParams(int k) {
    if (k <= 0) return factory();
    Params p; for (int i = 0; i < kBands; ++i) p.b[i] = presets()[std::min(k, kPresets - 1)].b[i];
    p.out = presets()[std::min(k, kPresets - 1)].out; return p;
}

class Engine {
public:
    // Lissage : les paramètres (log fréquence, gain dB, log Q) glissent en ~20 ms (pas de coefficients recalculés tous les kCtl échantillons) ;
    // activation de bande et gain de sortie en fondu ; changement de type = fondu enchaîné entre l'ancien et le nouveau filtre.
    static constexpr int kCtl = 16;
    void prepare(double sampleRate) {
        sr = sampleRate;
        aCtl = 1.0 - std::exp(-(double)kCtl / (0.005 * sr));   // constante de temps 5 ms : 98 % en ~20 ms
        aOut = 1.0 - std::exp(-1.0 / (0.005 * sr));
        stepAct = 1.0 / (0.010 * sr); stepXf = 1.0 / (0.020 * sr);
        for (auto& b : st) { for (auto& f : b.f) f.reset(); for (auto& f : b.prev) f.reset(); b.xf = 1.0; }
        snap = true;
    }
    void set(const Params& np) {
        if (np == p && !snap) return;
        p = np;
        for (int i = 0; i < kBands; ++i) {
            auto& b = st[(size_t)i]; const Band& t = p.b[i];
            b.tf = std::log(std::clamp((double)t.freq, 5.0, 0.49 * sr)); b.tg = hasGain(t.type) ? (double)t.gain : 0.0; b.tq = std::log(std::max((double)t.q, 0.05));
            b.tact = t.on ? 1.0 : 0.0;
            if (snap) { b.lf = b.tf; b.lg = b.tg; b.lq = b.tq; b.act = b.tact; b.type = t.type; b.dirty = true; }
            else if (t.type != b.type) {   // changement de type : l'ancien filtre (coefficients figés, état conservé) s'efface en fondu
                for (int c = 0; c < 2; ++c) b.prev[c] = b.f[c];
                b.xf = 0.0; b.type = t.type; b.lf = b.tf; b.lg = b.tg; b.lq = b.tq; b.dirty = true;
            }
        }
        tgtGain = odsp::db2lin(p.out);
        if (snap) { gain = tgtGain; snap = false; }
        for (int i = 0; i < kBands; ++i) if (st[(size_t)i].dirty) refresh(st[(size_t)i]);
    }
    int latency() const { return 0; }
    void process(const float* inL, const float* inR, float* outL, float* outR, int n) {
        for (int i0 = 0; i0 < n; i0 += kCtl) {
            const int c = std::min(kCtl, n - i0);
            for (auto& b : st) glide(b);
            for (int i = i0; i < i0 + c; ++i) {
                float l = inL[i], r = inR ? inR[i] : l;
                for (auto& b : st) {
                    if (b.rampLeft > 0) stepCoef(b);
                    const float xl = l;
                    float yl = b.f[0].process(xl);
                    if (b.xf < 1.0) yl = mixXf(b, b.prev[0].process(xl), yl);
                    l = shape(b, xl, yl);
                    if (inR) {
                        const float xr = r; float yr = b.f[1].process(xr);
                        if (b.xf < 1.0) yr = mixXf(b, b.prev[1].process(xr), yr);
                        r = shape(b, xr, yr);
                    }
                    ramp(b);
                }
                if (!inR) r = l;
                if (gain != tgtGain) { gain += (tgtGain - gain) * aOut; if (std::fabs(tgtGain - gain) < 1e-7 * std::max(1.0, tgtGain)) gain = tgtGain; }
                outL[i] = l * (float)gain; outR[i] = r * (float)gain;
            }
        }
    }
    const Params& params() const { return p; }
private:
    struct BandState {
        odsp::Bq f[2], prev[2];
        double tf = 0, tg = 0, tq = 0, lf = 0, lg = 0, lq = 0, act = 0, tact = 0, xf = 1.0;
        odsp::Coef goal, dc; int rampLeft = 0;
        int type = Bell; bool dirty = true;
    };
    void refresh(BandState& b) {
        const auto cf = designMatched(kind(b.type), sr, std::exp(b.lf), hasGain(b.type) ? b.lg : 0.0, std::exp(b.lq));
        b.f[0].c = cf; b.f[1].c = cf; b.goal = cf; b.rampLeft = 0; b.dirty = false;
    }
    // Glissement : les coefficients sont recalculés toutes les kCtl échantillons, puis interpolés linéairement échantillon par échantillon
    // (des marches de coefficients toutes les 16 échantillons seraient audibles comme un souffle dans les aigus).
    void retarget(BandState& b) {
        if (b.rampLeft > 0) { b.f[0].c = b.goal; b.f[1].c = b.goal; b.rampLeft = 0; }
        const auto cf = designMatched(kind(b.type), sr, std::exp(b.lf), hasGain(b.type) ? b.lg : 0.0, std::exp(b.lq));
        const auto& c0 = b.f[0].c; const double k = 1.0 / kCtl;
        b.dc = {(cf.b0 - c0.b0) * k, (cf.b1 - c0.b1) * k, (cf.b2 - c0.b2) * k, (cf.a1 - c0.a1) * k, (cf.a2 - c0.a2) * k};
        b.goal = cf; b.rampLeft = kCtl;
    }
    void stepCoef(BandState& b) {
        if (--b.rampLeft == 0) { b.f[0].c = b.goal; b.f[1].c = b.goal; return; }
        for (auto& f : b.f) { f.c.b0 += b.dc.b0; f.c.b1 += b.dc.b1; f.c.b2 += b.dc.b2; f.c.a1 += b.dc.a1; f.c.a2 += b.dc.a2; }
    }
    void glide(BandState& b) {
        auto adv = [&](double& cur, double tgt, double eps) {
            const double d = tgt - cur;
            if (d == 0.0) return false;
            cur = std::fabs(d) < eps ? tgt : cur + d * aCtl; return true;
        };
        bool ch = adv(b.lf, b.tf, 1e-6);
        ch |= adv(b.lg, b.tg, 1e-4);
        ch |= adv(b.lq, b.tq, 1e-6);
        if (ch) retarget(b);
    }
    float mixXf(BandState& b, float old, float cur) const { return old + (cur - old) * (float)b.xf; }
    float shape(const BandState& b, float x, float y) const { return b.act >= 1.0 ? y : (b.act <= 0.0 ? x : x + (y - x) * (float)b.act); }
    void ramp(BandState& b) {
        if (b.act != b.tact) b.act = b.act < b.tact ? std::min(b.tact, b.act + stepAct) : std::max(b.tact, b.act - stepAct);
        if (b.xf < 1.0) b.xf = std::min(1.0, b.xf + stepXf);
    }
    double sr = 48000.0, aCtl = 0.06, aOut = 0.004, stepAct = 0.002, stepXf = 0.001, gain = 1.0, tgtGain = 1.0;
    Params p = factory(); bool snap = true;
    std::array<BandState, kBands> st;
};
}  // namespace eq
