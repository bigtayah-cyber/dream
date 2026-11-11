// Comportement d'Orbe Autotune (phases P0 à P2 de l'audit 4) : presets, peigne, attaque, warble, stabilisateur, transitions, formants en descente.
// Voix de synthèse source-filtre (glotte de Rosenberg, jitter, shimmer, souffle, scoops), mesures sur atn::Engine (chemin complet du plugin).
// Cible : orbeautotune_behavior_test (EXCLUDE_FROM_ALL), sans JUCE. Seuils = valeurs mesurées avec une marge ; voir docs/audit4.
#include <chrono>
#include <complex>
#include <cstdio>
#include <random>
#include "AutotuneDsp.h"
using V = std::vector<float>;
static const double SR = 44100.0;

struct Reso { double a1 = 0, a2 = 0, g = 1, y1 = 0, y2 = 0;
    void set(double f, double bw) { const double r = std::exp(-M_PI * bw / SR), w = 2 * M_PI * f / SR; a1 = 2 * r * std::cos(w); a2 = -r * r; g = 1 - a1 - a2; }
    double run(double x) { const double y = g * x + a1 * y1 + a2 * y2; y2 = y1; y1 = y; return y; } };
// vowel formants
static const double VA[4] = {730, 1090, 2440, 3400}, VI[4] = {300, 2200, 3000, 3600}, BW[4] = {80, 100, 140, 200};
// Génère une voix : midiAt(t) (<=0 : silence / sifflante si fric(t)), voyelle mélangée a->i selon vow(t).
template <class F, class G, class H> V voice(F midiAt, G fric, H vow, double dur, double jit = 0.006, double shim = 0.04, double breath = 0.06, unsigned seed = 1) {
    std::mt19937 r(seed); std::normal_distribution<double> g(0, 1); std::uniform_real_distribution<double> u(-1, 1);
    Reso fo[4]; V x; const size_t n = (size_t)(dur * SR); double ph = 1.0, perJ = 0, ampJ = 1, lp = 0, hp = 0, pn = 0, env = 0;
    for (size_t i = 0; i < n; ++i) {
        const double t = i / SR, m = midiAt(t);
        const double w = vow(t); for (int k = 0; k < 4; ++k) fo[k].set(VA[k] + w * (VI[k] - VA[k]), BW[k]);
        double src = 0; const bool on = m > 0;
        env += ((on ? 1.0 : 0.0) - env) * (on ? 0.004 : 0.003);
        if (env > 1e-4) {
            const double f0 = 440 * std::pow(2, (m - 69) / 12) * (1 + perJ);
            ph += f0 / SR; if (ph >= 1) { ph -= 1; perJ = jit * g(r); ampJ = 1 + shim * g(r); }
            const double oq = 0.6; double gl = ph < oq ? 0.5 * (1 - std::cos(M_PI * ph / oq)) : std::cos(0.5 * M_PI * (ph - oq) / (1 - oq)); if (ph > oq + (1 - oq)) gl = 0;
            const double dg = gl - pn; pn = gl;   // dérivée du débit glottique
            const double asp = breath * g(r) * (ph < oq ? 1.0 : 0.3);
            src = env * (ampJ * dg * 30.0 + asp);
        }
        double y = src; for (int k = 0; k < 4; ++k) y = fo[k].run(y);
        y *= 0.5;
        if (fric(t)) { const double z = g(r); hp = 0.6 * (hp + z - lp); lp = z; y += 0.05 * hp; }
        x.push_back((float)y);
    }
    double pk = 0; for (float v : x) pk = std::max(pk, (double)std::fabs(v)); for (auto& v : x) v = (float)(v * 0.5 / (pk + 1e-12));
    return x;
}
// YIN plein débit hors-ligne (référence de mesure), fenêtre Wn, pas hop.
static V yin(const V& x, int Wn = 1024, int hop = 128, double thr = 0.2) {
    const int tmax = (int)(SR / 60.0), tmin = (int)(SR / 1000.0); V out; std::vector<double> d((size_t)tmax + 2);
    for (size_t s = (size_t)(Wn + tmax); s < x.size(); s += (size_t)hop) {
        const float* a = &x[s - (size_t)Wn]; double e = 0; for (int j = 0; j < Wn; ++j) e += a[j] * a[j];
        if (e / Wn < 1e-5) { out.push_back(0.f); continue; }
        double cum = 0; d[0] = 1; float res = 0.f; int bt = -1; double bv = 9;
        for (int t = 1; t <= tmax; ++t) { double s2 = 0; for (int j = 0; j < Wn; ++j) { const double v = a[j] - a[j - t]; s2 += v * v; } cum += s2; d[(size_t)t] = cum > 0 ? s2 * t / cum : 1; }
        for (int t = tmin + 1; t < tmax; ++t) { if (d[(size_t)t] < bv) { bv = d[(size_t)t]; bt = t; } if (d[(size_t)t] < thr && d[(size_t)t] <= d[(size_t)t - 1] && d[(size_t)t] < d[(size_t)t + 1]) { bt = t; break; } }
        if (bt > 0 && d[(size_t)bt] < 0.35) { const double a0 = d[(size_t)bt - 1], b0 = d[(size_t)bt], c0 = d[(size_t)bt + 1], den = a0 - 2 * b0 + c0;
            const double tf = bt + (den > 1e-12 ? 0.5 * (a0 - c0) / den : 0.0); res = (float)(69 + 12 * std::log2(SR / tf / 440)); }
        out.push_back(res);
    }
    return out;   // trame k centrée vers s - Wn/2 - T/2
}
static double tOf(size_t k, int Wn = 1024, int hop = 128) { return ((double)(Wn + (int)(SR / 60.0)) + (double)k * hop - Wn / 2.0) / SR; }

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { ++fails; std::printf("  ECHEC: "); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

static V run(const atn::Params& p, const V& in, int* lat) {
    static atn::Engine e; e.prepare(SR); e.set(p); *lat = e.latency();
    V l = in, r = in;
    for (size_t q = 0; q < in.size();) { const size_t c = std::min<size_t>(in.size() - q, 512); e.process(l.data() + q, r.data() + q, (int)c); q += c; }
    return V(l.begin() + std::min<size_t>(l.size(), (size_t)*lat), l.end());
}
// nouvelle instance : valeurs par défaut de atn::Params + préréglage ; chromatique, Orbe
static atn::Params base(bool direct, int render, float speed) {
    atn::Params p; p.amt = 1; p.tol = 0; p.dbl = 0; p.harm = 0; p.mix = 1; p.form = 0; p.vd = 0; p.trans = 0; p.key = 0; p.scale = 8; p.expr = 1;
    p.lowLat = direct; p.orbe = true; p.render = render; p.speed = speed; p.glide = 25; return p;
}
static double flipsPerSec(const V& to) {
    int sw = 0, last = -1; double T = 0;
    for (size_t k = 0; k < to.size(); ++k) { const double t = tOf(k, 512, 64); if (t < 0.5 || t > 2.6 || to[k] <= 0) continue; T += 64 / SR; const int nt = (int)std::lround(to[k]); if (last >= 0 && nt != last) ++sw; last = nt; }
    return sw / std::max(T, 1e-9);
}
static double irregularPct(const V& to, const V& ti, double a, double b) {
    int n = 0, k0 = 0;
    for (size_t k = 0; k < std::min(to.size(), ti.size()); ++k) { const double t = tOf(k, 512, 64); if (t < a || t > b || ti[k] <= 0) continue; ++k0; if (to[k] < 50) ++n; }
    return 100.0 * n / std::max(1, k0);
}

int main() {
    // ---------------------------------------------------------------- P0 : presets, instance neuve, peigne
    std::printf("[P0] presets et peigne\n");
    {
        const atn::Params fresh;   // instance neuve
        CHECK(fresh.render == 1, "une instance neuve doit s'ouvrir en Precis (render %d)", fresh.render);
        for (int i = 0; i < atn::kPresets; ++i) CHECK(atn::presets()[i].render >= 0, "preset %d ne fixe pas son rendu", i);
        CHECK(std::string(atn::presets()[3].name) == "Robot" && atn::presets()[3].render == 2, "le preset Robot doit fixer le rendu Robot");
        struct N { double t0, t1, m, off; };
        const std::vector<N> mel = {{0.2, 0.55, 57, .30}, {0.62, 0.95, 60, -.25}, {1.05, 1.4, 62, .35}, {2.0, 2.3, 64, -.40}, {2.4, 2.75, 62, .20}, {2.85, 3.3, 60, -.30}};
        const V x = voice([&](double t) { for (auto& n : mel) if (t >= n.t0 && t < n.t1) return n.m + n.off - 0.8 * std::exp(-(t - n.t0) / 0.03); return 0.0; }, [](double) { return false; }, [](double) { return 0.5; }, 3.5);
        atn::Params p = base(true, atn::presets()[3].render, atn::presets()[3].speed);   // preset Robot, Direct
        int lat; const V to = yin(run(p, x, &lat), 512, 64); int ok = 0, n = 0;
        for (size_t k = 0; k < to.size(); ++k) { const double t = tOf(k, 512, 64); bool in = false; for (auto& q : mel) in |= t > q.t0 + 0.06 && t < q.t1 - 0.03; if (!in || to[k] <= 0) continue; ++n; if (std::fabs(to[k] - std::round(to[k])) < 0.1) ++ok; }
        const double pct = 100.0 * ok / std::max(1, n);
        std::printf("  preset Robot sur instance neuve (Direct) : %.1f %% des trames a <=10 ct (avant : Satin 28 %%)\n", pct);
        CHECK(pct >= 85.0, "preset Robot : %.1f %% < 85 %%", pct);
        // peigne : voix deja juste a +2 ct, Precis : ecart d'enveloppe des harmoniques 200-4000 Hz
        auto harm = [&](const V& y, double f0) { std::vector<double> a; const size_t s0 = (size_t)(0.8 * SR), N = (size_t)(0.8 * SR);
            for (int h = 1; h * f0 < 4000; ++h) { if (h * f0 < 200) continue; double best = 0; for (double df : {-3.0, -1.5, 0.0, 1.5, 3.0}) { const double f = h * f0 + df; double re = 0, im = 0;
                for (size_t i = 0; i < N; ++i) { const double w = 0.5 - 0.5 * std::cos(2 * M_PI * i / N); re += w * y[s0 + i] * std::cos(2 * M_PI * f * i / SR); im += w * y[s0 + i] * std::sin(2 * M_PI * f * i / SR); }
                best = std::max(best, std::hypot(re, im)); } a.push_back(20 * std::log10(best + 1e-12)); }
            return a; };
        const double f57 = 440 * std::pow(2, (57.0 - 69) / 12);
        auto ctl = [](double m) { return [m](double t) { return t < 0.1 || t > 1.9 ? 0.0 : m; }; };
        const V xs = voice(ctl(57.02), [](double) { return false; }, [](double) { return 0.0; }, 2.0, 0.0, 0.0, 0.0), xr = voice(ctl(57.0), [](double) { return false; }, [](double) { return 0.0; }, 2.0, 0.0, 0.0, 0.0);
        const auto hi = harm(xr, f57);
        for (bool direct : {true, false}) {
            const V y = run(base(direct, 1, 0.f), xs, &lat); const auto ho = harm(y, f57);
            double mu = 0; for (size_t i = 0; i < ho.size(); ++i) mu += ho[i] - hi[i]; mu /= ho.size(); double s = 0; for (size_t i = 0; i < ho.size(); ++i) s += (ho[i] - hi[i] - mu) * (ho[i] - hi[i] - mu);
            const double rms = std::sqrt(s / ho.size());
            std::printf("  +2 ct, Precis %s : ondulation spectrale %.2f dB RMS (avant : 2,5)\n", direct ? "Direct " : "Qualite", rms);
            CHECK(rms <= 0.3, "peigne %.2f dB > 0.3", rms);
        }
    }
    // ---------------------------------------------------------------- P1 : attaque, bascules, transitions
    std::printf("[P1] attaque accordee, warble, stabilisateur, transitions\n");
    {
        struct N { double t0, t1, m, off; };
        const std::vector<N> mel = {{0.2, 0.55, 57, .30}, {0.62, 0.95, 60, -.25}, {1.05, 1.4, 62, .35}, {2.0, 2.3, 64, -.40}, {2.4, 2.75, 62, .20}, {2.85, 3.3, 60, -.30}, {4.0, 4.3, 57, .15}, {4.38, 4.8, 55, -.35}};
        const V x = voice([&](double t) { for (auto& n : mel) if (t >= n.t0 && t < n.t1) return n.m + n.off - 0.8 * std::exp(-(t - n.t0) / 0.03); return 0.0; }, [](double) { return false; }, [](double) { return 0.5; }, 5.0);
        auto lockMed = [&](const atn::Params& p) { int lat; const V to = yin(run(p, x, &lat), 512, 64); V L;
            for (auto& n : mel) { double lk = -1; for (size_t k = 0; k < to.size(); ++k) { const double t = tOf(k, 512, 64); if (t < n.t0 || t > n.t1 - 0.03) continue; if (to[k] > 0 && std::fabs(to[k] - n.m) < 0.1) { lk = t - n.t0; break; } } L.push_back(lk < 0 ? 999.f : (float)(lk * 1000)); }
            std::sort(L.begin(), L.end()); return (double)L[L.size() / 2]; };
        atn::Params pa = base(true, 2, 0.f), pn = pa; pn.attack = false;
        const double lockOn = lockMed(pa), lockOff = lockMed(pn);
        std::printf("  accroche mediane en Direct, vitesse 0 : %.1f ms avec attaque accordee, %.1f ms sans (avant : 33,3)\n", lockOn, lockOff);
        CHECK(lockOn <= 20.0, "accroche Direct %.1f ms > 20", lockOn);
        CHECK(lockOn <= lockOff + 1.0, "l'attaque accordee ne doit pas retarder l'accroche");
        // warble / stabilisateur : voix tenue a mi-chemin (A3 + 50 ct), vibrato 5,5 Hz +-25 ct
        const V xv = voice([](double t) { return t < 0.2 || t > 2.8 ? 0.0 : 57.5 + 0.25 * std::sin(2 * M_PI * 5.5 * t); }, [](double) { return false; }, [](double) { return 0.0; }, 3.0, 0.004, 0.02, 0.03);
        const V ti = yin(xv, 512, 64); int lat;
        double f1 = 0, f06 = 0, fs80 = 0, fs0 = 0;
        { atn::Params p = base(true, 2, 0.f); f1 = flipsPerSec(yin(run(p, xv, &lat), 512, 64)); p.warb = 0.6f; f06 = flipsPerSec(yin(run(p, xv, &lat), 512, 64)); }
        { atn::Params p = base(false, 2, 0.f); fs0 = flipsPerSec(yin(run(p, xv, &lat), 512, 64)); p.stab = 80; fs80 = flipsPerSec(yin(run(p, xv, &lat), 512, 64)); }
        std::printf("  bascules par seconde : warble 100 %% %.1f, 60 %% %.1f | Qualite stabilisateur off %.1f, 80 ms %.1f\n", f1, f06, fs0, fs80);
        CHECK(f1 >= 8.0, "warble libre : %.1f bascules/s < 8", f1);
        CHECK(f06 <= 1.0, "warble 60 %% : %.1f bascules/s > 1", f06);
        CHECK(fs0 >= 8.0 && fs80 <= 1.0, "stabilisateur 80 ms : %.1f -> %.1f", fs0, fs80);
        // raccords aux changements de note : sauts nets de 2 dt toutes les 150 ms
        const V xs = voice([](double t) { return t < 0.2 || t > 2.8 ? 0.0 : 57.3 + 2 * (int)(std::fmod(t, 0.3) >= 0.15); }, [](double) { return false; }, [](double) { return 0.0; }, 3.0, 0.003, 0.02, 0.03);
        const V ts = yin(xs, 512, 64);
        for (bool direct : {true, false}) {
            const double irr = irregularPct(yin(run(base(direct, 2, 0.f), xs, &lat), 512, 64), ts, 0.5, 2.6);
            std::printf("  sauts de note, %s : %.1f %% de trames irregulieres (avant : 4,0 / 2,2)\n", direct ? "Direct " : "Qualite", irr);
            CHECK(irr <= 3.0, "trames irregulieres %.1f %% > 3", irr);
        }
        // glissando A3 -> C#4 : la vitesse de transition ralentit l'escalier (trames entre deux notes)
        const V xg = voice([](double t) { return t < 0.3 ? 0.0 : t < 0.5 ? 57.0 : t < 0.9 ? 57 + 4 * (t - 0.5) / 0.4 : t < 1.4 ? 61.0 : 0.0; }, [](double) { return false; }, [](double) { return 0.0; }, 1.5, 0.003, 0.02, 0.03);
        auto mid = [&](atn::Params p) { const V to = yin(run(p, xg, &lat), 512, 64); int m = 0, n = 0; for (size_t k = 0; k < to.size(); ++k) { const double t = tOf(k, 512, 64); if (t < 0.52 || t > 0.9 || to[k] <= 0) continue; ++n; if (std::fabs(to[k] - std::round(to[k])) > 0.25f) ++m; } return 100.0 * m / std::max(1, n); };
        atn::Params pt = base(false, 2, 0.f); const double m0 = mid(pt); pt.tspd = 120.f; const double m1 = mid(pt);
        std::printf("  trames entre deux notes dans un glissando : transition = vitesse %.1f %%, transition 120 ms %.1f %%\n", m0, m1);
        CHECK(m1 > m0 + 10.0, "la vitesse de transition ne ralentit pas l'escalier (%.1f -> %.1f)", m0, m1);
    }
    // ---------------------------------------------------------------- P2 : formants en descente
    std::printf("[P2] pente spectrale en transposition (voyelle /a/, D3)\n");
    {
        auto envTrue = [](double f) { std::complex<double> h = 1; for (int k = 0; k < 4; ++k) { const double r = std::exp(-M_PI * BW[k] / SR), w = 2 * M_PI * VA[k] / SR, a1 = 2 * r * std::cos(w), a2 = -r * r;
                const std::complex<double> z = std::polar(1.0, -2 * M_PI * f / SR); h *= (1 - a1 - a2) / (1.0 - a1 * z - a2 * z * z); } return 20 * std::log10(std::abs(h)); };
        const V xv = voice([](double t) { return t < 0.1 || t > 1.9 ? 0.0 : 50.0; }, [](double) { return false; }, [](double) { return 0.0; }, 2.0, 0.0, 0.0, 0.0);
        auto slopeOf = [&](const V& y, double f0) { std::vector<double> X, Y; const size_t s0 = (size_t)(0.7 * SR), N = (size_t)(0.6 * SR);
            for (int h = 1; h * f0 < 3500; ++h) { if (h * f0 < 250) continue; const double f = h * f0; double re = 0, im = 0; for (size_t i = 0; i < N; ++i) { const double w = 0.5 - 0.5 * std::cos(2 * M_PI * i / N); re += w * y[s0 + i] * std::cos(2 * M_PI * f * i / SR); im += w * y[s0 + i] * std::sin(2 * M_PI * f * i / SR); }
                X.push_back(std::log2(f / 500)); Y.push_back(20 * std::log10(std::hypot(re, im) + 1e-12) - envTrue(f)); }
            double mx = 0, my = 0; for (size_t i = 0; i < X.size(); ++i) { mx += X[i]; my += Y[i]; } mx /= X.size(); my /= X.size(); double sxy = 0, sxx = 0; for (size_t i = 0; i < X.size(); ++i) { sxy += (X[i] - mx) * (Y[i] - my); sxx += (X[i] - mx) * (X[i] - mx); } return sxy / sxx; };
        const double s0 = slopeOf(xv, 440 * std::pow(2, (50.0 - 69) / 12));
        const struct { int tr; double lim; } cases[] = {{-7, 2.2}, {-3, 1.0}, {3, 0.8}, {7, 1.0}};
        for (auto c : cases) { atn::Params p = base(true, 1, 0.f); p.amt = 0; p.trans = (float)c.tr; int lat; const V y = run(p, xv, &lat);
            const double e = slopeOf(y, 440 * std::pow(2, (50.0 + c.tr - 69) / 12)) - s0;
            std::printf("  %+d dt Direct : ecart de pente %+.1f dB/oct (avant : -9,6 a -7 dt, -2,7 a -3 dt)\n", c.tr, e);
            CHECK(std::fabs(e) <= c.lim, "pente %+d dt : %+.1f dB/oct", c.tr, e); }
    }
    std::printf(fails ? "%d ECHEC(S)\n" : "tout est bon\n", fails);
    return fails ? 1 : 0;
}
