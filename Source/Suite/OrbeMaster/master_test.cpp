// Test console du moteur Orbe Master (sans JUCE). Cible : orbemaster_test (EXCLUDE_FROM_ALL).
#include <cstdio>
#include <random>
#include "../../Kit/BypassFadeTest.h"
#include "MasterDsp.h"

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { ++fails; std::printf("ECHEC: " __VA_ARGS__); std::printf("\n"); } } while (0)
using V = std::vector<float>;

static void run(mas::Engine& e, V& l, V& r) {
    for (size_t p = 0; p < l.size(); p += 480) e.process(l.data() + p, r.data() + p, (int)std::min<size_t>(480, l.size() - p));
}
// Crête vraie de référence, indépendante du limiteur : sinc long fenêtré Kaiser, 16 phases.
static double bessel0(double x) { double s = 1, t = 1; for (int k = 1; k < 40; ++k) { t *= (x / (2 * k)) * (x / (2 * k)); s += t; } return s; }
static float truePeakDb(const V& x, size_t from, size_t to = 0) {
    double best = 0; const int R = 32; if (!to || to > x.size() - R - 2) to = x.size() - R - 2;
    static double ker[16][2 * R]; static bool init = false;
    if (!init) { for (int k = 0; k < 16; ++k) for (int j = -R + 1; j <= R; ++j) { const double d = j - k / 16.0, sn = std::fabs(d) < 1e-9 ? 1.0 : std::sin(mas::kPi * d) / (mas::kPi * d), r = d / R; ker[k][j + R - 1] = sn * bessel0(8.0 * std::sqrt(std::max(0.0, 1 - r * r))) / bessel0(8.0); } init = true; }
    for (size_t i = std::max<size_t>(from, R); i < to; ++i)
        for (int k = 0; k < 16; ++k) { double s = 0; for (int j = 0; j < 2 * R; ++j) s += x[i + (size_t)j - (R - 1)] * ker[k][j]; best = std::max(best, std::fabs(s)); }
    return (float)(20 * std::log10(best + 1e-12));
}
// Passe-bas FIR (63 coefficients, Kaiser) : les signaux de test sont limités en bande comme une vraie musique (<= 0,36 fs).
static V bandLimit(const V& x, float peak) {
    V h(63); double sum = 0;
    for (int i = 0; i < 63; ++i) { const double m = i - 31.0, s = m == 0 ? 0.72 : std::sin(mas::kPi * 0.72 * m) / (mas::kPi * m), r = m / 32.0; h[(size_t)i] = (float)(s * bessel0(7.0 * std::sqrt(1 - r * r))); sum += h[(size_t)i]; }
    V y(x.size()); float pk = 0;
    for (size_t i = 0; i < x.size(); ++i) { double s = 0; for (int j = 0; j < 63; ++j) if (i >= (size_t)j) s += x[i - (size_t)j] * h[(size_t)j] / sum; y[i] = (float)s; pk = std::max(pk, std::fabs(y[i])); }
    for (auto& v : y) v *= peak / pk;
    return y;
}
static V make(int kind, int n, float sr) {
    std::mt19937 rng(11); std::uniform_real_distribution<float> u(-1.f, 1.f); V x((size_t)n);
    for (int i = 0; i < n; ++i) {
        const float t = (float)i / sr;
        if (kind == 0) x[(size_t)i] = u(rng);
        else if (kind == 1) x[(size_t)i] = 0.9f * std::sin(mas::kPi * 0.5f * (float)i + 0.7854f);       // fs/4, phase 45° : crête entre les échantillons
        else x[(size_t)i] = (i % 6000 < 40 ? 1.f : 0.f) * (i % 2 ? 1.f : -1.f) + 0.5f * std::sin(2 * mas::kPi * 60.f * t) + 0.2f * u(rng);
    }
    return kind == 1 ? x : bandLimit(x, 0.95f);
}

int main() {
    for (double sr : {48000.0, 44100.0, 96000.0}) {
        const int N = (int)(sr * 0.8);
        for (int kind = 0; kind < 3; ++kind) for (float ceil : {-3.f, -1.f, 0.f}) {
            mas::Engine e; e.prepare(sr, 480); mas::Params p; p.compOn = false; p.stOn = false; p.lGain = 12.f; p.ceil = ceil; p.lRel = 120.f; e.set(p);
            V l = make(kind, N, (float)sr), r = l; run(e, l, r);
            bool ok = true; for (size_t i = 0; i < l.size(); ++i) ok &= std::isfinite(l[i]) && std::isfinite(r[i]);
            const float tp = truePeakDb(l, (size_t)e.latency() + 200);
            std::printf("sr %6.0f signal %d plafond %+.1f dBTP -> crete vraie %+.3f dBTP, reduction max %.1f dB\n", sr, kind, ceil, tp, e.limReduction());
            CHECK(ok, "NaN"); CHECK(tp <= ceil + 0.02f, "plafond depasse : %f > %f", tp, ceil);   // dépassement : avant 0,32 dB (audit 4), maintenant <= 0,02 dB
        }
    }
    { // limiteur : distorsion des graves (audit 4 : DHT 1,16 % à 40 Hz pour 8 dB de réduction) et pas de modulation du gain
        auto thd = [](const V& y, double f0, double sr, size_t a, size_t n) {
            auto mag = [&](double f) { double re = 0, im = 0; for (size_t i = 0; i < n; ++i) { const double w = 0.5 - 0.5 * std::cos(2 * mas::kPi * (double)i / (double)n); re += y[a + i] * w * std::cos(2 * mas::kPi * f * (double)i / sr); im += y[a + i] * w * std::sin(2 * mas::kPi * f * (double)i / sr); } return std::hypot(re, im); };
            double hs = 0; for (int k = 2; k <= 9; ++k) hs += std::pow(mag(k * f0), 2); return std::sqrt(hs) / mag(f0);
        };
        for (double sr : {48000.0, 44100.0, 96000.0}) for (double f : {25.0, 30.0, 40.0, 60.0, 100.0}) {
            mas::Params p; p.eqOn = false; p.compOn = false; p.satOn = false; p.stOn = false; p.lGain = 8.f; p.ceil = -1.f; p.lRel = 90.f;
            mas::Engine e; e.prepare(sr, 512); e.set(p); const size_t N = (size_t)(sr * 3); V l(N), r(N);
            for (size_t i = 0; i < N; ++i) l[i] = r[i] = (float)(0.9 * std::sin(2 * mas::kPi * f * (double)i / sr));
            run(e, l, r);
            const double d = 100 * thd(l, f, sr, N - (size_t)sr, (size_t)sr);
            if (sr == 48000.0) std::printf("limiteur %3.0f Hz, +8 dB : DHT %.3f %%, GR max %.1f dB\n", f, d, e.limReduction());
            CHECK(d < (f < 30 ? 0.6 : 0.3), "DHT limiteur %.0f Hz a %.0f Hz d'echantillonnage : %.3f %%", f, sr, d);
        }
    }
    { // limiteur : le maintien ne retient pas le gain apres une crete isolee (retour a 0 dB de GR en moins de 600 ms)
        mas::Engine e; e.prepare(48000, 480); mas::Params p; p.eqOn = false; p.compOn = false; p.stOn = false; p.lGain = 0.f; p.ceil = -1.f; p.lRel = 90.f; e.set(p);
        V l(48000 * 2, 0.f), r;
        for (size_t i = 0; i < l.size(); ++i) l[i] = 0.2f * std::sin(2 * mas::kPi * 440.f * (float)i / 48000.f);
        for (size_t i = 24000; i < 24200; ++i) l[i] = 1.6f * std::sin(2 * mas::kPi * 440.f * (float)i / 48000.f);
        r = l; run(e, l, r);
        // pic : le dernier bloc entièrement après 600 ms doit avoir rendu le niveau d'origine
        float pk = 0; for (size_t i = 24000 + 28800 + 4000; i < 24000 + 28800 + 8000; ++i) pk = std::max(pk, std::fabs(l[i]));
        std::printf("apres une crete isolee, niveau revenu a %.3f (attendu 0.2)\n", pk);
        CHECK(pk > 0.19f, "gain non rendu apres 600 ms : %.3f", pk);
    }
    { // réglages extrêmes + chaîne complète
        mas::Params p; for (auto& v : p.eq) v = 6.f; p.cAmt = 1; p.cRatio = 6; p.cAtt = 1; p.cGain = 12; p.satOn = true; p.drive = 1; p.satMix = 1; p.width = 1.6f; p.monoHz = 250; p.lGain = 14; p.ceil = -3;
        mas::Engine e; e.prepare(48000, 480); e.set(p); V l = make(0, 96000, 48000), r = make(2, 96000, 48000); run(e, l, r);
        bool ok = true; for (size_t i = 0; i < l.size(); ++i) ok &= std::isfinite(l[i]) && std::isfinite(r[i]);
        CHECK(ok, "NaN en réglages extrêmes"); CHECK(std::max(truePeakDb(l, 500), truePeakDb(r, 500)) <= -2.9f, "plafond extrême");
        std::printf("extrême : crete %.2f dBTP\n", std::max(truePeakDb(l, 500), truePeakDb(r, 500)));
    }
    { // latence exacte et transparence (chaîne neutre, limiteur sans action)
        mas::Engine e; e.prepare(48000, 480); mas::Params p; p.compOn = false; p.stOn = false; e.set(p);
        V s(72000); for (size_t i = 0; i < s.size(); ++i) s[i] = 0.2f * std::sin(2 * mas::kPi * 1000.f * (float)i / 48000.f) + 0.1f * std::sin(2 * mas::kPi * 3777.f * (float)i / 48000.f);
        V l = s, r = s; run(e, l, r);
        int best = -1; double be = 1e9;
        for (int d = 0; d < 400; ++d) { double err = 0; for (int i = 30000; i < 50000; ++i) { const double v = l[(size_t)i] - s[(size_t)(i - d)]; err += v * v; } if (err < be) { be = err; best = d; } }
        std::printf("latence mesurée %d (déclarée %d), erreur résiduelle %.2e\n", best, e.latency(), be / 20000);
        CHECK(best == e.latency(), "latence %d != %d", best, e.latency()); CHECK(be / 20000 < 1e-8, "chaîne neutre pas transparente");
    }
    { // LUFS : sinusoïde 1 kHz, -20 dBFS crête sur L et R = -20 LUFS
        mas::Engine e; e.prepare(48000, 480); mas::Params p; p.compOn = false; p.stOn = false; e.set(p);
        V l(96000); for (size_t i = 0; i < l.size(); ++i) l[i] = 0.1f * std::sin(2 * mas::kPi * 1000.f * (float)i / 48000.f); V r = l; run(e, l, r);
        std::printf("LUFS entrée %.2f sortie %.2f\n", e.lufsIn(), e.lufsOut());
        CHECK(std::fabs(e.lufsIn() + 20.f) < 0.3f && std::fabs(e.lufsOut() + 20.f) < 0.3f, "LUFS faux");
    }
    { // LUFS court terme et intégré (EBU Tech 3341/3342, BS.1770-4)
        auto sine = [](size_t n, float amp, size_t off = 0) { V v(n); for (size_t i = 0; i < n; ++i) v[i] = amp * std::sin(2 * mas::kPi * 997.f * (float)(i + off) / 48000.f); return v; };
        { mas::Engine e; e.prepare(48000, 480); mas::Params p; p.compOn = false; p.stOn = false; e.set(p);
          V l = sine(48000 * 10, 0.1f), r = l; run(e, l, r);
          std::printf("LUFS -20 dBFS 10 s : M %.2f  S %.2f  I %.2f\n", e.lufsOut(), e.lufsShortOut(), e.lufsIntegratedOut());
          CHECK(std::fabs(e.lufsShortOut() + 20.f) < 0.1f && std::fabs(e.lufsIntegratedOut() + 20.f) < 0.1f, "S/I d'une sinusoide a -20 LUFS"); }
        { // 3341 cas 2 : 20 s a -36 LUFS puis 20 s a -23 : l'intégré est -23,0 (porte absolue de -70, relative de -10 LU : le premier palier tombe)
          mas::Engine e; e.prepare(48000, 480); mas::Params p; p.compOn = false; p.stOn = false; p.limOn = false; e.set(p);
          V l = sine(48000 * 20, 0.0158f), l2 = sine(48000 * 20, 0.0708f); l.insert(l.end(), l2.begin(), l2.end()); V r = l; run(e, l, r);
          std::printf("3341 palier -36 puis -23 : I %.2f (attendu -23.0)\n", e.lufsIntegratedOut());
          CHECK(std::fabs(e.lufsIntegratedOut() + 23.f) < 0.15f, "integre avec porte relative : %.2f", e.lufsIntegratedOut()); }
        { // la remise a zero repart de zero : l'intégré ne garde rien des 3 s à -20 LUFS
          mas::Engine e; e.prepare(48000, 480); mas::Params p; p.compOn = false; p.stOn = false; e.set(p);
          V l = sine(48000 * 3, 0.1f), r = l; run(e, l, r);
          const float before = e.lufsIntegratedOut(); e.requestLufsReset(); V z = sine(48000 * 3, 0.00316f), z2 = z; run(e, z, z2);   // -50 LUFS après la remise à zéro
          std::printf("integre avant remise a zero %.2f, apres (3 s a -50 LUFS) %.2f\n", before, e.lufsIntegratedOut());
          CHECK(before > -21.f && std::fabs(e.lufsIntegratedOut() + 50.f) < 0.5f, "remise a zero de l'integre"); }
    }
    { // contournement : exactement l'entrée retardée
        mas::BypassDelay b; mas::Engine e0; e0.prepare(48000, 480); b.init(e0.latency());
        V a = make(0, 5000, 48000), c = a, ref = a; b.process(a.data(), c.data(), 5000, false);
        bool ok = true; for (int i = 0; i < 5000; ++i) ok &= a[(size_t)i] == (i >= e0.latency() ? ref[(size_t)(i - e0.latency())] : 0.f) && c[(size_t)i] == a[(size_t)i];
        CHECK(ok, "bypass non transparent");
    }
    { // bypass honoré (même Fader que processBlock) : sortie = entrée retardée de la latence (< -120 dB), saut au basculement < -60 dBFS
        mas::Engine e; mas::Params p; p.compOn = true; p.satOn = true; p.lGain = 0.f;
        auto m = bypassfx::measure(48000.0, e.latency(), 480, [&] { e.prepare(48000.0, 480); e.set(p); }, [&](float* l, float* r, int n) { if (!r) { r = l; } e.process(l, r, n); });
        std::printf("bypass %s : latence %d, erreur %.1f dB, saut au basculement %.1f dBFS (signal seul : %.1f)\n", "OrbeMaster", e.latency(), m.exactDb, m.jumpDb, m.plainDb);
        CHECK(m.exactDb < -120.0, "bypass non exact : %.1f dB", m.exactDb); CHECK(m.jumpDb < -60.0, "clic au basculement : %.1f dBFS", m.jumpDb);
    }
    { // lot E : cible LUFS, écoute à niveau égal, delta
        const float sr = 48000.f; const int N = (int)(sr * 45);
        V base((size_t)N); { std::mt19937 rng(5); std::uniform_real_distribution<float> u(-1.f, 1.f); float lp = 0; for (int i = 0; i < N; ++i) { lp += 0.35f * (u(rng) - lp); base[(size_t)i] = 0.35f * lp * (0.7f + 0.3f * std::sin(2 * mas::kPi * 0.5f * (float)i / sr)); } }
        auto go = [&](mas::Params p, V& l, V& r, mas::Engine& e) { e.prepare(sr, 480); e.set(p); l = base; r = base; run(e, l, r); };
        mas::Params p0; p0.compOn = false; p0.satOn = false; p0.stOn = false; p0.eqOn = false;
        { mas::Engine e; V l, r; mas::Params p = p0; p.target = 1; go(p, l, r, e);
          std::printf("cible -14 LUFS : court terme final %.2f LUFS (entrée %.2f)\n", e.lufsShortOut(), e.lufsShortIn()); CHECK(std::fabs(e.lufsShortOut() + 14.f) < 1.0f, "cible -14 non atteinte (%.2f)", e.lufsShortOut());
          CHECK(truePeakDb(l, 24000) < -0.95f, "plafond dépassé avec la cible"); }
        { mas::Engine e; V l, r; mas::Params p = p0; p.lGain = 9.f; p.match = true; go(p, l, r, e);
          mas::Lufs mm; mm.prepare(sr); for (size_t i = 0; i < l.size(); ++i) mm.push(l[i], r[i]);   // niveau réellement entendu (après atténuation)
          std::printf("niveau égal : entendu %.2f LUFS pour une entrée à %.2f (sortie du limiteur %.2f)\n", mm.shortTerm, e.lufsShortIn(), e.lufsShortOut());
          mas::Engine e2; V l2, r2; p.match = false; go(p, l2, r2, e2);
          CHECK(e2.lufsShortOut() - e2.lufsShortIn() > 4.f, "banc : le limiteur ne grossit pas");
          CHECK(std::fabs(mm.shortTerm - e.lufsShortIn()) < 1.0f, "niveau égal : entendu %.2f != entrée %.2f", mm.shortTerm, e.lufsShortIn()); }
        { mas::Engine e; V l, r; mas::Params p = p0; p.limOn = false; p.delta = true; go(p, l, r, e);
          double ee = 0, ei = 0; for (size_t i = 24000; i < l.size(); ++i) { ee += (double)l[i] * l[i]; ei += (double)base[i] * base[i]; }
          std::printf("delta, chaîne neutre : résiduel %.1f dB sous l'entrée\n", 10 * std::log10(ei / (ee + 1e-30))); CHECK(10 * std::log10(ei / (ee + 1e-30)) > 60.0, "delta d'une chaîne neutre non nul"); }
        { mas::Engine e; V l, r; mas::Params p = p0; go(p, l, r, e); mas::Engine e2; V l2, r2; go(p0, l2, r2, e2); bool same = true; for (size_t i = 0; i < l.size(); ++i) same &= l[i] == l2[i]; CHECK(same, "réglages lot E neutres : sortie modifiée"); }
    }
    std::printf(fails ? "%d ECHEC(S)\n" : "TOUT EST BON\n", fails);
    return fails ? 1 : 0;
}
