// Vague 2B : mesures objectives des briques Source/Dsp (Saturation, Compressors, DeEssDyn). Sans JUCE.
// g++ -O2 -std=c++17 -I Source tests/dsp2b_test.cpp -o /tmp/dsp2b_test && /tmp/dsp2b_test
#include <cmath>
#include <cstdio>
#include <vector>
#include "Dsp/Saturation.h"
#include "Dsp/Compressors.h"
#include "Dsp/DeEssDyn.h"
#include "OneKnob/OneKnob.h"   // « avant » : onek::Comp / onek::DeEss (comparaison seulement)

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { ++fails; std::printf("ECHEC: " __VA_ARGS__); std::printf("\n"); } } while (0)
static const double PI = 3.14159265358979323846;
static double db(double x) { return 20.0 * std::log10(std::fabs(x) + 1e-30); }

// ---------------------------------------------------------------- outils de mesure
static void fft(std::vector<double>& re, std::vector<double>& im) {
    const size_t n = re.size();
    for (size_t i = 1, j = 0; i < n; ++i) { size_t bit = n >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; if (i < j) { std::swap(re[i], re[j]); std::swap(im[i], im[j]); } }
    for (size_t len = 2; len <= n; len <<= 1) {
        const double ang = -2.0 * PI / (double)len;
        for (size_t i = 0; i < n; i += len) for (size_t k = 0; k < len / 2; ++k) {
            const double wr = std::cos(ang * (double)k), wi = std::sin(ang * (double)k);
            const size_t a = i + k, b = a + len / 2;
            const double tr = re[b] * wr - im[b] * wi, ti = re[b] * wi + im[b] * wr;
            re[b] = re[a] - tr; im[b] = im[a] - ti; re[a] += tr; im[a] += ti;
        }
    }
}
// Spectre de puissance (Blackman-Harris, 32768 points) à partir de start.
static std::vector<double> power(const std::vector<float>& y, size_t start, size_t N = 32768) {
    std::vector<double> re(N), im(N, 0.0);
    for (size_t i = 0; i < N; ++i) { const double t = (double)i / (double)(N - 1), w = 0.35875 - 0.48829 * std::cos(2 * PI * t) + 0.14128 * std::cos(4 * PI * t) - 0.01168 * std::cos(6 * PI * t); re[i] = w * y[start + i]; }
    fft(re, im);
    std::vector<double> p(N / 2); for (size_t k = 0; k < N / 2; ++k) p[k] = re[k] * re[k] + im[k] * im[k];
    return p;
}
static double bandPow(const std::vector<double>& p, double sr, double f, size_t N = 32768) {
    const double bw = sr / (double)N; const long c = std::lround(f / bw); double s = 0;
    for (long k = c - 4; k <= c + 4; ++k) if (k > 0 && k < (long)p.size()) s += p[(size_t)k];
    return s;
}
// Harmonique h (dB re fondamental).
static double harmDb(const std::vector<double>& p, double sr, double f0, int h) { return 10 * std::log10(bandPow(p, sr, f0 * h) / bandPow(p, sr, f0) + 1e-30); }
static double thdDb(const std::vector<double>& p, double sr, double f0) {
    double s = 0; for (int h = 2; h * f0 < sr * 0.5 - 200; ++h) s += bandPow(p, sr, f0 * h);
    return 10 * std::log10(s / bandPow(p, sr, f0) + 1e-30);
}
// Pire raie hors harmoniques (repliement), dB re fondamental, 20 Hz..20 kHz.
static double aliasDb(const std::vector<double>& p, double sr, double f0, size_t N = 32768) {
    const double bw = sr / (double)N; double worst = 0;
    for (size_t k = 1; k < p.size(); ++k) {
        const double f = (double)k * bw; if (f < 20 || f > 20000) continue;
        const double hh = f / f0; if (std::fabs(hh - std::round(hh)) * f0 < 12 * bw) continue;
        worst = std::max(worst, p[k]);
    }
    return 10 * std::log10(worst / bandPow(p, sr, f0) + 1e-30);
}
static std::vector<float> sine(double sr, double f, double amp, size_t n) { std::vector<float> v(n); for (size_t i = 0; i < n; ++i) v[i] = (float)(amp * std::sin(2 * PI * f * (double)i / sr)); return v; }
static double rms(const std::vector<float>& v, size_t a, size_t b) { double s = 0; for (size_t i = a; i < b; ++i) s += (double)v[i] * v[i]; return std::sqrt(s / (double)(b - a)); }
static double peak(const std::vector<float>& v, size_t a, size_t b) { double s = 0; for (size_t i = a; i < b; ++i) s = std::max(s, (double)std::fabs(v[i])); return s; }
static bool finite(const std::vector<float>& v) { for (float x : v) if (!std::isfinite(x)) return false; return true; }

// ================================================================ 1. saturation
static void testSaturation() {
    using namespace dsp::sat;
    std::printf("\n=== Saturation (ADAA + surechantillonnage) ===\n");
    const char* names[4] = {"tanh", "arctan", "triode", "transfo"};
    std::printf("Courbe statique (drive 0 dB) x -> y :\n");
    for (int c = 0; c < 3; ++c) {
        std::printf("  %-7s", names[c]);
        for (double x : {-4.0, -2.0, -1.0, -0.5, 0.5, 1.0, 2.0, 4.0}) std::printf(" %+.3f", Saturator::staticCurve((Curve)c, x));
        std::printf("\n");
    }
    const double sr = 44100;
    for (int c = 0; c < 4; ++c) {
        for (int osf : {2, 4}) {
            for (float drv : {6.f, 18.f, 30.f}) {
                Saturator s; s.prepare(sr, osf); Params p; p.curve = (Curve)c; p.driveDb = drv; s.setParams(p);
                auto x = sine(sr, 7000, 0.5, 50000); s.process(x.data(), (int)x.size());
                const auto pw = power(x, 12000);
                const double al = aliasDb(pw, sr, 7000);
                std::printf("  %-7s x%d drive %2.0f dB : alias 7 kHz (crete -6 dBFS) %.1f dB\n", names[c], osf, drv, al);
                if (osf == 4 && drv <= 18.f) CHECK(al < -70.0, "alias %s x4 drive %.0f : %.1f dB", names[c], drv, al);
            }
        }
    }
    std::printf("THD a 1 kHz et niveau de sortie (compensation auto, sinus -18 dBFS RMS) :\n");
    for (int c = 0; c < 4; ++c) {
        for (float drv : {0.f, 12.f, 24.f}) {
            std::printf("  %-7s drive %2.0f :", names[c], drv);
            for (double lvl : {-30.0, -18.0, -6.0}) {
                Saturator s; s.prepare(sr, 4); Params p; p.curve = (Curve)c; p.driveDb = drv; s.setParams(p);
                const double amp = std::pow(10, lvl / 20) * std::sqrt(2.0);
                auto x = sine(sr, 1000, amp, 50000); s.process(x.data(), (int)x.size());
                const auto pw = power(x, 12000);
                const double g = db(rms(x, 12000, 44000)) - lvl;
                std::printf("  [%+.0f dB : THD %.1f dB, H2 %.1f, H3 %.1f, gain %+.2f dB]", lvl, thdDb(pw, sr, 1000), harmDb(pw, sr, 1000, 2), harmDb(pw, sr, 1000, 3), g);
                if (lvl == -18.0 && c != 3) CHECK(std::fabs(g) < 0.6, "compensation %s drive %.0f : %+.2f dB", names[c], drv, g);
            }
            std::printf("\n");
        }
    }
    {   // triode : harmoniques paires presentes (H2 > H3 a faible niveau) ; transfo : plus de distorsion a 50 Hz qu'a 1 kHz
        Saturator s; s.prepare(sr, 4); Params p; p.curve = Curve::Triode; p.driveDb = 12; s.setParams(p);
        auto x = sine(sr, 1000, 0.1, 50000); s.process(x.data(), (int)x.size());
        const auto pw = power(x, 12000);
        CHECK(harmDb(pw, sr, 1000, 2) > harmDb(pw, sr, 1000, 3), "triode : H2 %.1f <= H3 %.1f", harmDb(pw, sr, 1000, 2), harmDb(pw, sr, 1000, 3));
        double t50, t1k;
        { Saturator q; q.prepare(sr, 4); Params pp; pp.curve = Curve::Transformer; pp.driveDb = 12; q.setParams(pp); auto y = sine(sr, 50, 0.3, 50000); q.process(y.data(), (int)y.size()); t50 = thdDb(power(y, 12000), sr, 50); }
        { Saturator q; q.prepare(sr, 4); Params pp; pp.curve = Curve::Transformer; pp.driveDb = 12; q.setParams(pp); auto y = sine(sr, 1000, 0.3, 50000); q.process(y.data(), (int)y.size()); t1k = thdDb(power(y, 12000), sr, 1000); }
        std::printf("  transfo drive 12 dB, crete -10 dBFS : THD 50 Hz %.1f dB, 1 kHz %.1f dB\n", t50, t1k);
        CHECK(t50 > t1k + 15, "transfo : grave pas plus sature (%.1f / %.1f)", t50, t1k);
    }
    {   // emphase : à drive égal, 6 kHz plus compressé que 200 Hz (aigus « arrondis ») ; sans saturation, emphase transparente
        auto gainAt = [&](double f, float emph, float drv, double amp) {
            Saturator s; s.prepare(sr, 4); Params p; p.curve = Curve::Tanh; p.driveDb = drv; p.emphasisDb = emph; p.autoLevel = false; s.setParams(p);
            auto x = sine(sr, f, amp, 30000); s.process(x.data(), (int)x.size()); return db(rms(x, 10000, 30000) / (amp / std::sqrt(2.0))); };
        const double t = gainAt(6000, 9, 0, 0.001), c200 = gainAt(200, 9, 18, 0.5) - gainAt(200, 0, 18, 0.5), c6k = gainAt(6000, 9, 18, 0.5) - gainAt(6000, 0, 18, 0.5);
        std::printf("  emphase 9 dB : transparence petit signal 6 kHz %+.3f dB ; effet a fort niveau : 200 Hz %+.2f dB, 6 kHz %+.2f dB\n", t, c200, c6k);
        CHECK(std::fabs(t) < 0.05, "emphase non transparente (%.3f dB)", t);
        CHECK(c6k < c200 - 1.0, "emphase sans effet sur les aigus");
    }
    {   // latence, mix sec/traité aligné, robustesse
        Saturator s; s.prepare(sr, 4); Params p; p.driveDb = 0.f; p.mix = 1.f; s.setParams(p);
        std::vector<float> x(400, 0.f); x[10] = 0.01f; s.process(x.data(), 400);
        int arg = 0; for (int i = 0; i < 400; ++i) if (std::fabs(x[(size_t)i]) > std::fabs(x[(size_t)arg])) arg = i;
        std::printf("  latence declaree %d (x2 : %d), pic d'impulsion a %d\n", s.latency(), [] { Saturator q; q.prepare(44100, 2); return q.latency(); }(), arg - 10);
        CHECK(arg - 10 == s.latency(), "latence saturation");
        std::vector<float> z(20000); for (size_t i = 0; i < z.size(); ++i) z[i] = (i % 97 == 0) ? 1e6f : (i % 13 == 0 ? NAN : (float)std::sin(i * 0.3) * 3.f);
        for (int c = 0; c < 4; ++c) { Saturator q; q.prepare(sr, 4); Params pp; pp.curve = (Curve)c; pp.driveDb = 36; pp.dynBias = 1; pp.bias = 0.7f; pp.emphasisDb = 12; q.setParams(pp); auto zz = z; q.process(zz.data(), (int)zz.size()); CHECK(finite(zz), "NaN saturation %s", names[c]); }
    }
}


// ================================================================ 2. compresseurs
using dsp::comp::OptoComp; using dsp::comp::FetComp; using dsp::comp::VariMuComp;
template <class C> static std::vector<float> runComp(C& c, std::vector<float> x, std::vector<float>* grTrace = nullptr) {
    for (size_t i = 0; i < x.size(); i += 64) {
        const int n = (int)std::min<size_t>(64, x.size() - i);
        if (!grTrace) { c.process(x.data() + i, nullptr, n); continue; }
        for (int k = 0; k < n; ++k) { c.process(x.data() + i + k, nullptr, 1); grTrace->push_back(c.gainReductionDb()); }
    }
    return x;
}
// Signal par paliers de niveau (sinus 1 kHz) : (durée s, niveau dBFS crête)
static std::vector<float> steps(double sr, std::initializer_list<std::pair<double, double>> segs) {
    std::vector<float> v; size_t i = 0;
    for (auto& s : segs) { const double a = std::pow(10, s.second / 20); const size_t n = (size_t)(s.first * sr); for (size_t k = 0; k < n; ++k, ++i) v.push_back((float)(a * std::sin(2 * PI * 1000 * (double)i / sr))); }
    return v;
}
// Temps (ms) depuis t0 pour que la trace atteigne `frac` du chemin entre sa valeur en t0 et `target`.
static double timeTo(const std::vector<float>& tr, double sr, size_t t0, double target, double frac) {
    const double v0 = tr[t0], goal = v0 + frac * (target - v0);
    for (size_t i = t0; i < tr.size(); ++i) if ((target > v0 && tr[i] >= goal) || (target <= v0 && tr[i] <= goal)) return 1000.0 * (double)(i - t0) / sr;
    return -1;
}
template <class C, class P> static void staticCurve(const char* name, P p, double sr) {
    p.common.autoGain = false; p.common.sidechainHz = 0; p.common.mix = 1;
    std::printf("  %-7s (seuil %.0f) entree -> reduction mesuree / theorique (dB) :", name, p.common.thresholdDb);
    double worst = 0;
    for (double lvl = -40; lvl <= 0; lvl += 5) {
        C c; c.prepare(sr); c.setParams(p);
        auto y = runComp(c, steps(sr, {{2.5, lvl}}));
        const double gr = (lvl - 3.0103) - db(rms(y, (size_t)(2.0 * sr), (size_t)(2.5 * sr)));
        const double th = c.staticGrDb((float)lvl);
        std::printf(" %+.0f:%.1f/%.1f", lvl, gr, th); worst = std::max(worst, std::fabs(gr - th));
    }
    std::printf("\n");
    CHECK(worst < 1.0, "%s : courbe statique ecart %.2f dB", name, worst);
}

static void testCompressors() {
    std::printf("\n=== Compresseurs (latence nulle) ===\n");
    const double sr = 44100;
    { dsp::comp::OptoParams p; staticCurve<OptoComp>("opto", p, sr); p.limit = true; staticCurve<OptoComp>("opto-L", p, sr); }
    { dsp::comp::FetParams p; p.attackUs = 800; p.releaseMs = 1100; for (int r : {4, 20}) { p.ratio = r; staticCurve<FetComp>(r == 4 ? "fet4" : "fet20", p, sr); } p.allButtons = true; staticCurve<FetComp>("fetAB", p, sr); }
    { dsp::comp::VariMuParams p; p.timeConstant = 2; for (float c : {1.f, 3.f}) { p.curve = c; staticCurve<VariMuComp>(c < 2 ? "vmu p1" : "vmu p3", p, sr); } }

    std::printf("Constantes de temps (sinus 1 kHz, palier de seuil-10 a seuil+15 dB) :\n");
    {   // OPTO : attaque ~10 ms ; relâche 50 % ~60 ms ; mémoire : relâche 90 % court après une rafale, long après 5 s
        dsp::comp::OptoParams p; p.common.autoGain = false; p.common.sidechainHz = 0;
        double r50[2], r90[2], att = 0;
        for (int k = 0; k < 2; ++k) {
            const double dur = k == 0 ? 0.2 : 5.0;
            OptoComp c; c.prepare(sr); c.setParams(p); std::vector<float> tr;
            runComp(c, steps(sr, {{0.5, -34}, {dur, -9}, {6.0, -60}}), &tr);
            const size_t t1 = (size_t)(0.5 * sr), t2 = (size_t)((0.5 + dur) * sr);
            if (k == 1) att = timeTo(tr, sr, t1, tr[t2 - 1], 0.632);
            r50[k] = timeTo(tr, sr, t2, 0, 0.5); r90[k] = timeTo(tr, sr, t2, 0, 0.9);
        }
        std::printf("  opto : attaque 63%% %.1f ms (cible ~10) ; relache 50%% %.0f ms apres 0,2 s / %.0f ms apres 5 s (cible ~60) ; relache 90%% %.0f ms / %.0f ms (memoire)\n", att, r50[0], r50[1], r90[0], r90[1]);
        CHECK(att > 4 && att < 25, "opto attaque %.1f ms", att);
        CHECK(r50[0] > 30 && r50[0] < 150, "opto relache 50%% %.0f ms", r50[0]);
        CHECK(r90[1] > 2.5 * r90[0] && r90[1] > 1000, "opto sans memoire (%.0f / %.0f ms)", r90[0], r90[1]);
    }
    {   // FET
        for (float a : {20.f, 800.f}) for (float r : {50.f, 1100.f}) {
            dsp::comp::FetParams p; p.common.autoGain = false; p.common.sidechainHz = 0; p.attackUs = a; p.releaseMs = r; p.ratio = 4;
            double r63[2];
            double att = 0;
            for (int k = 0; k < 2; ++k) {
                const double dur = k == 0 ? 0.1 : 3.0;
                FetComp c; c.prepare(sr); c.setParams(p); std::vector<float> tr;
                runComp(c, steps(sr, {{0.3, -30}, {dur, -5}, {4.0, -60}}), &tr);
                const size_t t1 = (size_t)(0.3 * sr), t2 = (size_t)((0.3 + dur) * sr);
                if (k == 1) att = timeTo(tr, sr, t1, tr[t2 - 1], 0.632);
                r63[k] = timeTo(tr, sr, t2, 0, 0.632);
            }
            std::printf("  fet attaque %3.0f us / relache %4.0f ms : attaque 63%% mesuree %.2f ms ; relache 63%% %.0f ms apres 0,1 s, %.0f ms apres 3 s\n", a, r, att, r63[0], r63[1]);
            CHECK(r63[1] > r63[0] * 1.3, "fet : relache non dependante du programme");
            CHECK(att < a * 1e-3 * 1.5 + 0.1 && att > a * 1e-3 * 0.2, "fet : attaque %.2f ms loin du reglage", att);
        }
    }
    {   // VARIABLE-MU : position 2 (fixe) contre 5 (double)
        for (int tc : {2, 5}) {
            dsp::comp::VariMuParams p; p.common.autoGain = false; p.common.sidechainHz = 0; p.timeConstant = tc;
            double r90[2];
            for (int k = 0; k < 2; ++k) {
                const double dur = k == 0 ? 0.15 : 5.0;
                VariMuComp c; c.prepare(sr); c.setParams(p); std::vector<float> tr;
                runComp(c, steps(sr, {{0.3, -30}, {dur, -5}, {20.0, -70}}), &tr);
                r90[k] = timeTo(tr, sr, (size_t)((0.3 + dur) * sr), 0, 0.9);
            }
            std::printf("  vari-mu position %d : relache 90%% %.0f ms apres 0,15 s, %.0f ms apres 5 s\n", tc, r90[0], r90[1]);
            if (tc == 5) CHECK(r90[1] > 3 * r90[0], "vari-mu position 5 sans relache double");
        }
    }

    std::printf("THD selon la reduction (sinus 1 kHz, autoGain coupe) :\n");
    for (int m = 0; m < 3; ++m) {
        const char* nm[3] = {"opto", "fet", "vari-mu"};
        std::printf("  %-7s :", nm[m]);
        double h2lo = 0, h2hi = 0;
        for (double lvl : {-30.0, -14.0, -6.0, 0.0}) {
            std::vector<float> y; float gr = 0;
            auto x = sine(sr, 1000, std::pow(10, lvl / 20), (size_t)(3 * sr));
            if (m == 0) { OptoComp c; dsp::comp::OptoParams p; p.common.autoGain = false; p.common.thresholdDb = -30; c.prepare(sr); c.setParams(p); y = runComp(c, x); gr = c.gainReductionDb(); }
            if (m == 1) { FetComp c; dsp::comp::FetParams p; p.common.autoGain = false; p.common.thresholdDb = -30; p.attackUs = 800; p.releaseMs = 1100; c.prepare(sr); c.setParams(p); y = runComp(c, x); gr = c.gainReductionDb(); }
            if (m == 2) { VariMuComp c; dsp::comp::VariMuParams p; p.common.autoGain = false; p.common.thresholdDb = -30; p.timeConstant = 4; c.prepare(sr); c.setParams(p); y = runComp(c, x); gr = c.gainReductionDb(); }
            const auto pw = power(y, (size_t)(2 * sr));
            const double h2 = harmDb(pw, sr, 1000, 2);
            if (lvl == -30.0) h2lo = h2;
            if (lvl == 0.0) h2hi = h2;
            std::printf("  [GR %4.1f dB : THD %.1f, H2 %.1f, H3 %.1f]", gr, thdDb(pw, sr, 1000), h2, harmDb(pw, sr, 1000, 3));
        }
        std::printf("\n");
        if (m > 0) CHECK(h2hi > h2lo + 10, "%s : H2 ne croit pas avec la reduction", nm[m]);
    }

    std::printf("Repliement, sinus 7 kHz a 44,1 kHz, crete -6 dBFS, reglages les plus rapides :\n");
    {
        double a[3];
        { FetComp c; dsp::comp::FetParams p; p.common.thresholdDb = -30; p.attackUs = 20; p.releaseMs = 50; p.allButtons = true; c.prepare(sr); c.setParams(p); auto y = runComp(c, sine(sr, 7000, 0.5, 50000)); a[1] = aliasDb(power(y, 12000), sr, 7000); }
        { OptoComp c; dsp::comp::OptoParams p; p.common.thresholdDb = -30; p.limit = true; p.speed = 0.5f; c.prepare(sr); c.setParams(p); auto y = runComp(c, sine(sr, 7000, 0.5, 50000)); a[0] = aliasDb(power(y, 12000), sr, 7000); }
        { VariMuComp c; dsp::comp::VariMuParams p; p.common.thresholdDb = -30; p.timeConstant = 1; p.curve = 3; c.prepare(sr); c.setParams(p); auto y = runComp(c, sine(sr, 7000, 0.5, 50000)); a[2] = aliasDb(power(y, 12000), sr, 7000); }
        std::printf("  opto %.1f dB, fet (all-buttons 20 us) %.1f dB, vari-mu %.1f dB\n", a[0], a[1], a[2]);
        for (double v : a) CHECK(v < -70, "repliement compresseur %.1f dB", v);
    }

    std::printf("Niveaux sortie/entree avec gain auto (sinus 1 kHz, seuil -24) :\n");
    for (int m = 0; m < 3; ++m) {
        const char* nm[3] = {"opto", "fet", "vari-mu"};
        std::printf("  %-7s :", nm[m]);
        for (double lvl : {-36.0, -24.0, -12.0, -6.0, 0.0}) {
            auto x = sine(sr, 1000, std::pow(10, lvl / 20), (size_t)(3 * sr)); std::vector<float> y;
            if (m == 0) { OptoComp c; dsp::comp::OptoParams p; p.common.thresholdDb = -24; c.prepare(sr); c.setParams(p); y = runComp(c, x); }
            if (m == 1) { FetComp c; dsp::comp::FetParams p; p.common.thresholdDb = -24; p.attackUs = 800; p.releaseMs = 600; c.prepare(sr); c.setParams(p); y = runComp(c, x); }
            if (m == 2) { VariMuComp c; dsp::comp::VariMuParams p; p.common.thresholdDb = -24; p.timeConstant = 3; c.prepare(sr); c.setParams(p); y = runComp(c, x); }
            const double o = db(peak(y, (size_t)(2.5 * sr), (size_t)(3 * sr)));
            std::printf("  %+.0f -> %+.1f", lvl, o);
            if (lvl == -12.0) CHECK(std::fabs(o - lvl) < 1.0, "%s : gain auto, -12 dBFS sort a %.1f", nm[m], o);
        }
        std::printf("\n");
    }

    std::printf("Mix parallele : reduction de crete reelle (rafales type caisse, crete 0 dBFS, FET 20:1, seuil -30, makeup 0) :\n");
    {
        std::vector<float> x((size_t)(2 * sr)); uint32_t rng = 1;
        for (size_t i = 0; i < x.size(); ++i) { const double t = std::fmod((double)i / sr, 0.25); rng = rng * 1664525u + 1013904223u;
            x[i] = (float)(std::exp(-t / 0.05) * (0.7 * std::sin(2 * PI * 180 * t) + 0.3 * ((double)(int32_t)rng / 2147483648.0))); }
        const double pin = peak(x, (size_t)sr, x.size());
        for (float mix : {1.f, 0.75f, 0.5f}) {
            FetComp c; dsp::comp::FetParams p; p.common.thresholdDb = -30; p.common.autoGain = false; p.common.mix = mix; p.ratio = 20; p.attackUs = 20; p.releaseMs = 100; c.prepare(sr); c.setParams(p);
            auto y = runComp(c, x);
            const double red = db(peak(y, (size_t)sr, y.size()) / pin);
            std::printf("  mix %.2f : crete %+.1f dB", mix, red);
            if (mix == 1.f) CHECK(red < -15, "mix 1 : reduction de crete seulement %.1f dB", red);
        }
        // avant : un-potard, sortie = l·(1-0,6m) + l·g·0,6m -> plafond à m = 1 : 20·log10(0,4 + 0,6·g)
        std::printf("\n  avant (un-potard, m = 1, g -> 0) : plafond %.1f dB\n", db(0.4));
    }

    {   // lien stéréo et robustesse
        FetComp c; dsp::comp::FetParams p; p.common.autoGain = false; p.common.link = 1; c.prepare(sr); c.setParams(p);
        auto l = sine(sr, 1000, 1.0, 20000); std::vector<float> r(20000);
        for (size_t i = 0; i < r.size(); ++i) r[i] = (float)(0.01 * std::sin(2 * PI * 300 * (double)i / sr));
        const auto r0 = r;
        for (size_t i = 0; i < 20000; i += 64) c.process(l.data() + i, r.data() + i, (int)std::min<size_t>(64, 20000 - i));
        const double gR = db(rms(r, 15000, 20000) / rms(r0, 15000, 20000));
        std::printf("  lien stereo 1 : canal faible attenue de %.1f dB (canal fort reduit de %.1f dB)\n", gR, c.gainReductionDb());
        CHECK(gR < -10, "lien stereo inefficace");
        std::vector<float> z(30000); for (size_t i = 0; i < z.size(); ++i) z[i] = (i % 97 == 0) ? 1e6f : (i % 13 == 0 ? NAN : (i > 20000 ? 1e-38f : (float)std::sin(i * 0.3) * 3.f));
        { OptoComp q; q.prepare(sr); auto a = z, b = z; q.process(a.data(), b.data(), (int)a.size()); CHECK(finite(a) && finite(b), "NaN opto"); }
        { FetComp q; dsp::comp::FetParams pp; pp.allButtons = true; pp.attackUs = 20; q.prepare(sr); q.setParams(pp); auto a = z, b = z; q.process(a.data(), b.data(), (int)a.size()); CHECK(finite(a) && finite(b), "NaN fet"); }
        { VariMuComp q; q.prepare(sr); auto a = z, b = z; q.process(a.data(), b.data(), (int)a.size()); CHECK(finite(a) && finite(b), "NaN vari-mu"); }
    }
}

// ================================================================ 3. de-esser relatif + air dynamique
// Voix synthétique : voyelle (180 Hz, 50 harmoniques, pente -6 dB/oct au-delà de 1 kHz) 0,4 s, « s » (bruit 5-11 kHz) 0,2 s,
// voyelle 0,4 s, souffle (bruit -75 dBFS) 0,3 s. Mise à l'échelle à `peakDb`.
static std::vector<float> voice(double sr, double peakDb, size_t* seg) {
    std::vector<float> v; uint32_t rng = 7; dsp::deess::Svf hp, lp; hp.set(sr, 5000); lp.set(sr, 11000);
    auto nz = [&] { rng = rng * 1664525u + 1013904223u; return (float)((double)(int32_t)rng / 2147483648.0); };
    auto vowel = [&](double dur) { for (size_t i = 0, n = (size_t)(dur * sr); i < n; ++i) { double s = 0; for (int h = 1; h <= 50; ++h) { const double f = 180.0 * h; if (f > sr / 2) break; s += std::sin(2 * PI * f * (double)i / sr + h) / (f > 1000 ? f / 1000 : 1.0) / std::sqrt((double)h); } v.push_back((float)(0.15 * s)); } };
    seg[0] = 0; vowel(0.4); seg[1] = v.size();
    for (size_t i = 0, n = (size_t)(0.2 * sr); i < n; ++i) { hp.process(nz()); lp.process(hp.hp); v.push_back(1.2f * lp.lp); }
    seg[2] = v.size(); vowel(0.4); seg[3] = v.size();
    for (size_t i = 0, n = (size_t)(0.3 * sr); i < n; ++i) v.push_back(1.8e-4f * nz());
    seg[4] = v.size();
    const double pk = peak(v, 0, v.size()), g = std::pow(10, peakDb / 20) / pk;
    for (auto& x : v) x = (float)(x * g);
    return v;
}
static double hfDb(const std::vector<float>& y, size_t a, size_t b, double sr, double fLo) {   // énergie au-dessus de fLo
    const size_t N = 4096; double s = 0; int cnt = 0;
    for (size_t st = a; st + N <= b; st += N / 2, ++cnt) { std::vector<double> re(N), im(N, 0); for (size_t i = 0; i < N; ++i) re[i] = y[st + i] * (0.5 - 0.5 * std::cos(2 * PI * (double)i / (double)(N - 1))); fft(re, im);
        for (size_t k = (size_t)(fLo / sr * N); k < N / 2; ++k) s += re[k] * re[k] + im[k] * im[k]; }
    return 10 * std::log10(s / std::max(1, cnt) + 1e-30);
}
static void testDeEsser() {
    std::printf("\n=== De-esser relatif + air dynamique ===\n");
    const double sr = 44100;
    double redS[2], redV[2], oldS[2];
    for (int k = 0; k < 2; ++k) {
        const double lvl = k == 0 ? -8.0 : -30.0; size_t seg[5];
        const auto x = voice(sr, lvl, seg);
        dsp::deess::DeEsser d; d.prepare(sr); dsp::deess::Params p; p.thresholdDb = -6; d.setParams(p);
        auto y = x; double sumS = 0, maxV = 0;
        for (size_t i = 0; i < y.size(); ++i) { d.process(&y[i], nullptr, 1); if (i > seg[1] + 2000 && i < seg[2]) sumS += d.reductionDb(); if ((i > seg[0] + 4000 && i < seg[1]) || (i > seg[2] + (size_t)(0.3 * sr) && i < seg[3])) maxV = std::max(maxV, (double)d.reductionDb()); }
        redS[k] = hfDb(y, seg[1], seg[2], sr, 5000) - hfDb(x, seg[1], seg[2], sr, 5000); redV[k] = maxV;
        // avant : onek::DeEss (seuil absolu 0,05, amt 1)
        onek::DeEss o; o.prepare((float)sr); o.amt = 1.f; auto z = x; for (auto& v : z) v = o.process(v);
        oldS[k] = hfDb(z, seg[1], seg[2], sr, 5000) - hfDb(x, seg[1], seg[2], sr, 5000);
        (void)sumS;
    }
    std::printf("  \"s\" : attenuation des aigus a -8 dBFS %.1f dB, a -30 dBFS %.1f dB (avant, seuil absolu : %.1f / %.1f dB)\n", redS[0], redS[1], oldS[0], oldS[1]);
    std::printf("  voyelles (hors relache de 0,3 s apres le \"s\") : reduction max %.2f / %.2f dB\n", redV[0], redV[1]);
    CHECK(redS[0] < -4 && std::fabs(redS[0] - redS[1]) < 1.0, "de-esser dependant du niveau (%.1f / %.1f)", redS[0], redS[1]);
    CHECK(redV[0] < 1.0 && redV[1] < 1.0, "de-esser actif sur les voyelles");
    {   // air : +6 dB demandé
        size_t seg[5]; const auto x = voice(sr, -12, seg);
        dsp::deess::DeEsser d; d.prepare(sr); dsp::deess::Params p; p.thresholdDb = -6; p.airDb = 6; d.setParams(p);
        auto y = x; d.process(y.data(), nullptr, (int)y.size());
        dsp::deess::DeEsser d0; d0.prepare(sr); dsp::deess::Params p0 = p; p0.airDb = 0; d0.setParams(p0);
        auto y0 = x; d0.process(y0.data(), nullptr, (int)y0.size());
        const double v = hfDb(y, seg[2] + 4000, seg[3], sr, 7000) - hfDb(x, seg[2] + 4000, seg[3], sr, 7000);
        const double s = hfDb(y, seg[1] + 1000, seg[2], sr, 8000) - hfDb(y0, seg[1] + 1000, seg[2], sr, 8000);
        const double b = hfDb(y, seg[3] + 3000, seg[4], sr, 8000) - hfDb(x, seg[3] + 3000, seg[4], sr, 8000);
        std::printf("  air +6 dB : voyelle > 7 kHz %+.1f dB ; \"s\" > 8 kHz vs sans air %+.2f dB ; souffle %+.2f dB\n", v, s, b);
        CHECK(v > 3.0, "air absent sur la voyelle");
        CHECK(s < 0.5, "air ajoute du sifflement (%.2f dB)", s);
        CHECK(std::fabs(b) < 1.0, "air sur le souffle (%.2f dB)", b);
    }
    {   // transparence et robustesse
        dsp::deess::DeEsser d; d.prepare(sr); dsp::deess::Params p; p.thresholdDb = 12; d.setParams(p);
        auto x = sine(sr, 1000, 0.3, 20000), y = x; d.process(y.data(), nullptr, (int)y.size());
        double e = 0; for (size_t i = 10000; i < x.size(); ++i) e = std::max(e, (double)std::fabs(x[i] - y[i]));
        std::printf("  transparence au repos (ecart max) %.2e\n", e);
        CHECK(e < 1e-5, "de-esser non transparent");
        std::vector<float> z(20000); for (size_t i = 0; i < z.size(); ++i) z[i] = (i % 97 == 0) ? 1e6f : (i % 13 == 0 ? NAN : (float)std::sin(i * 2.3) * 3.f);
        dsp::deess::Params pa; pa.airDb = 8; d.setParams(pa); auto a = z, b2 = z; d.process(a.data(), b2.data(), (int)a.size()); CHECK(finite(a) && finite(b2), "NaN de-esser");
    }
}

int main() {
    testSaturation();
    testCompressors();
    testDeEsser();
    std::printf(fails ? "\ndsp2b_test : %d echec(s)\n" : "\ndsp2b_test : OK\n", fails);
    return fails ? 1 : 0;
}
