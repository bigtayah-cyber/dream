// Test console du moteur Orbe EQ (sans JUCE). Cible : orbeeq_test (EXCLUDE_FROM_ALL).
#include <cstdio>
#include <cstdlib>
#include <new>
#include <random>
#include "EqDsp.h"
#include "../../Kit/BypassFadeTest.h"

// Allocateur instrumenté : compte les new/delete pendant les sections « temps réel ».
static long g_allocs = 0; static bool g_count = false;
void* operator new(std::size_t n) { if (g_count) ++g_allocs; if (void* p = std::malloc(n ? n : 1)) return p; throw std::bad_alloc(); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { ++fails; std::printf("ECHEC: " __VA_ARGS__); std::printf("\n"); } } while (0)

static float measureDb(const eq::Params& p, double f, double sr) {
    eq::Engine e; e.prepare(sr); e.set(p);
    const int n = (int)(sr * 1.5); std::vector<float> x((size_t)n), y((size_t)n), r((size_t)n);
    for (int i = 0; i < n; ++i) x[(size_t)i] = 0.25f * (float)std::sin(2 * odsp::kPi * f * i / sr);
    for (int s = 0; s < n; s += 480) { const int c = std::min(480, n - s); e.process(x.data() + s, x.data() + s, y.data() + s, r.data() + s, c); }
    double a = 0, b = 0; const int from = n / 2;  // régime établi, nombre entier de périodes non requis grâce à la longueur
    for (int i = from; i < n; ++i) { a += (double)x[(size_t)i] * x[(size_t)i]; b += (double)y[(size_t)i] * y[(size_t)i]; }
    return (float)(10 * std::log10(b / a));
}

int main() {
    const double sr = 48000.0;
    std::mt19937 rng(5); std::uniform_real_distribution<float> u(-1.f, 1.f);
    int tested = 0; double worst = 0;
    for (int k = 0; k < eq::kPresets; ++k) for (double sr2 : {44100.0, 48000.0, 96000.0}) {
        eq::Params p; for (int i = 0; i < eq::kBands; ++i) p.b[i] = eq::presets()[k].b[i]; p.out = eq::presets()[k].out;
        for (int i = 0; i < eq::kBands; ++i) if (p.b[i].freq <= 0) p.b[i] = eq::defaultBand(i);
        if (k == 0) p = eq::factory();
        for (double f : {60.0, 100.0, 250.0, 500.0, 1000.0, 2000.0, 4000.0, 7000.0, 10000.0, 15000.0}) {
            if (f > sr2 * 0.4) continue;
            const double th = eq::responseDb(p, f, sr2), me = measureDb(p, f, sr2), d = std::fabs(th - me);
            worst = std::max(worst, d); ++tested;
            CHECK(d < 0.2, "preset %d sr %g f %g : théorie %.3f mesure %.3f", k, sr2, f, th, me);
        }
    }
    std::printf("réponse mesurée vs théorique : %d points, écart max %.3f dB\n", tested, worst);
    { // bande unique à +12 dB, 2 kHz, Q 2 : vérifie le gain au centre
        eq::Params p = eq::factory(); p.b[4].gain = 12.f; p.b[4].q = 2.f; p.b[4].freq = 2000.f;
        const double me = measureDb(p, 2000.0, sr); std::printf("cloche +12 dB à 2 kHz : %.2f dB\n", me); CHECK(std::fabs(me - 12.0) < 0.2, "gain cloche");
    }
    { // pas de NaN en réglages extrêmes ; latence 0 ; bypass (moteur neutre) transparent
        eq::Params p; for (int i = 0; i < eq::kBands; ++i) { p.b[i].freq = i % 2 ? 20.f : 20000.f; p.b[i].gain = i % 2 ? 24.f : -24.f; p.b[i].q = i % 3 ? 18.f : 0.1f; p.b[i].type = i % eq::kTypes; }
        eq::Engine e; e.prepare(sr); e.set(p); std::vector<float> x(48000), y(48000), r(48000); for (auto& s : x) s = u(rng);
        e.process(x.data(), x.data(), y.data(), r.data(), 48000); bool ok = true; for (float v : y) ok &= std::isfinite(v); CHECK(ok, "NaN en réglages extrêmes");
        eq::Engine n; n.prepare(sr); n.set(eq::factory()); n.process(x.data(), x.data(), y.data(), r.data(), 48000);
        double err = 0; for (size_t i = 0; i < x.size(); ++i) err = std::max(err, (double)std::fabs(x[i] - y[i]));
        std::printf("latence déclarée %d ; EQ à plat : écart max %.2e\n", n.latency(), err); CHECK(err < 1e-6, "EQ à plat non transparent");
        // latence mesurée : impulsion
        std::vector<float> imp(256, 0.f), o(256), o2(256); imp[10] = 1.f; n.prepare(sr); n.set(eq::factory()); n.process(imp.data(), imp.data(), o.data(), o2.data(), 256);
        int pk = (int)(std::max_element(o.begin(), o.end()) - o.begin()); CHECK(pk == 10 + n.latency(), "latence mesurée %d", pk - 10);
    }
    { // lissage : balayage de 100 Hz à 10 kHz en 50 ms (cloche +12 dB, Q 2), consignes mises à jour par blocs de 64 comme une automation d'hôte
        // critère de clic : la dérivée seconde du signal de sortie reste celle d'une sinusoïde (pas de discontinuité) ; l'ancien comportement (coefficients
        // changés d'un coup, filtre en direct) est mesuré en comparaison.
        const double sr2 = 48000.0; const int N = (int)(sr2 * 0.6), sw0 = (int)(sr2 * 0.2), swN = (int)(sr2 * 0.05);
        std::vector<float> x((size_t)N);
        for (int i = 0; i < N; ++i) x[(size_t)i] = 0.2f * (float)std::sin(2 * odsp::kPi * 200.0 * i / sr2);   // 200 Hz : la cloche la traverse pendant le balayage
        auto freqAt = [&](int i) { const double t = std::clamp((i - sw0) / (double)swN, 0.0, 1.0); return 100.0 * std::pow(100.0, t); };
        auto clickOf = [&](const std::vector<float>& y) {   // max |d²y| (sur la zone du balayage et 40 ms après) / borne d'une sinusoïde de même crête
            double d2 = 0, pk = 0;
            for (int i = sw0; i < sw0 + swN + (int)(0.04 * sr2); ++i) { d2 = std::max(d2, (double)std::fabs(y[(size_t)i] - 2 * y[(size_t)i - 1] + y[(size_t)i - 2])); pk = std::max(pk, (double)std::fabs(y[(size_t)i])); }
            const double w = 2 * odsp::kPi * 200.0 / sr2; return d2 / (pk * w * w);
        };
        std::vector<float> ys((size_t)N), yd((size_t)N);
        { eq::Engine e; e.prepare(sr2); eq::Params p = eq::factory(); p.b[4].gain = 12.f; p.b[4].q = 2.f; e.set(p);
          for (int s = 0; s < N; s += 64) { const int c = std::min(64, N - s); p.b[4].freq = (float)freqAt(s); e.set(p); e.process(x.data() + s, x.data() + s, ys.data() + s, ys.data() + s, c); } }
        { // version « d'avant » : un biquad dont les coefficients sont remplacés d'un coup à chaque bloc (conception RBJ)
          odsp::Bq f; for (int s = 0; s < N; s += 64) { const int c = std::min(64, N - s); f.c = odsp::design(odsp::Kind::Peak, sr2, freqAt(s), 12.0, 2.0); for (int i = s; i < s + c; ++i) yd[(size_t)i] = f.process(x[(size_t)i]); } }
        const double cs = clickOf(ys), cd = clickOf(yd);
        std::printf("balayage 100 Hz -> 10 kHz en 50 ms : clic lissé %.3f, ancien %.3f (1.0 = sinusoïde pure)\n", cs, cd);
        CHECK(cs < 3.0, "balayage : discontinuité mesurée %.3f", cs);
        CHECK(cd > 3.0 * cs, "le test doit distinguer l'ancien comportement (%.3f contre %.3f)", cd, cs);
        // saut de gain de sortie et activation de bande : pas de marche
        eq::Engine e; e.prepare(sr2); eq::Params p = eq::factory(); p.b[4].gain = 6.f; p.b[4].on = true; e.set(p);
        std::vector<float> a((size_t)N), o((size_t)N); for (int i = 0; i < N; ++i) a[(size_t)i] = 0.2f * (float)std::sin(2 * odsp::kPi * 100.0 * i / sr2);
        double worst = 0, pkAfter = 0;
        for (int s = 0; s < N; s += 64) { const int c = std::min(64, N - s); if (s == 8192) { p.out = 12.f; p.b[4].on = false; p.b[2].type = eq::HighShelf; p.b[2].gain = 9.f; e.set(p); }
            e.process(a.data() + s, a.data() + s, o.data() + s, o.data() + s, c); }
        for (int i = 8192; i < N; ++i) { worst = std::max(worst, (double)std::fabs(o[(size_t)i] - o[(size_t)i - 1])); if (i > N / 2) pkAfter = std::max(pkAfter, (double)std::fabs(o[(size_t)i])); }
        const double w100 = 2 * odsp::kPi * 100.0 / sr2; std::printf("saut de réglages (sortie +12 dB, bande coupée, type changé) : pas max %.5f (limite sinusoïde finale %.5f)\n", worst, pkAfter * w100);
        CHECK(worst < 1.3 * pkAfter * w100, "marche audible après un saut de réglages (%.5f)", worst);
    }
    { // conception près de Nyquist : écart avec la courbe analogique (prototype RBJ) à 16 kHz, 44,1 kHz ; plancher à -20 dB (zone coupée : une erreur sous -20 dB est inaudible)
        const double sr2 = 44100.0; auto cl = [](double v) { return std::max(v, -20.0); };
        auto errAt = [&](const eq::Band& b, double f, bool old) {
            const double dig = old ? 20 * std::log10(odsp::design(eq::kind(b.type), sr2, b.freq, eq::hasGain(b.type) ? b.gain : 0.0, b.q).mag(f, sr2) + 1e-12) : eq::bandDb(b, f, sr2);
            return std::fabs(cl(dig) - cl(eq::analogDb(b, f)));
        };
        // (1) réglages d'usage : les 9 presets (chaîne complète) + bande AIR (plateau aigu 18 kHz) + cloches/plateaux usuels
        double wp = 0, wpOld = 0;
        for (int k = 0; k < eq::kPresets; ++k) {
            const eq::Params p = eq::presetParams(k); double ana = p.out, old = p.out;
            for (auto& b : p.b) if (b.on) { ana += eq::analogDb(b, 16000.0); old += 20 * std::log10(odsp::design(eq::kind(b.type), sr2, b.freq, eq::hasGain(b.type) ? b.gain : 0.0, b.q).mag(16000.0, sr2) + 1e-12); }
            wp = std::max(wp, std::fabs(cl(eq::responseDb(p, 16000.0, sr2)) - cl(ana))); wpOld = std::max(wpOld, std::fabs(cl(old) - cl(ana)));
        }
        eq::Band air; air.freq = 18000.f; air.gain = 6.f; air.q = 0.707f; air.type = eq::HighShelf;
        eq::Band lp16; lp16.freq = 16000.f; lp16.q = 0.707f; lp16.type = eq::HighCut;
        eq::Band bell; bell.freq = 6000.f; bell.gain = 6.f; bell.q = 1.4f; bell.type = eq::Bell;
        eq::Band sh; sh.freq = 9000.f; sh.gain = 4.f; sh.q = 0.707f; sh.type = eq::HighShelf;
        const double wu = std::max({errAt(lp16, 12000.0, false), errAt(lp16, 16000.0, false), errAt(bell, 16000.0, false), errAt(sh, 16000.0, false)});
        const double wuOld = std::max({errAt(lp16, 12000.0, true), errAt(lp16, 16000.0, true), errAt(bell, 16000.0, true), errAt(sh, 16000.0, true)});
        std::printf("44,1 kHz, écart à 16 kHz avec la courbe analogique : presets %.3f dB (RBJ avant %.3f), bandes d'usage %.3f dB (RBJ avant %.3f)\n", wp, wpOld, wu, wuOld);
        std::printf("limite connue : plateau aigu a 18 kHz (+6 dB) : ecart %.2f dB a 16 kHz (RBJ avant %.2f) - un pole analogique au-dela de Nyquist n'est pas reproductible par un biquad\n", errAt(air, 16000.0, false), errAt(air, 16000.0, true));
        CHECK(wp < 0.5, "presets : écart %.3f dB à 16 kHz", wp); CHECK(wu < 0.5, "bandes d'usage : écart %.3f dB à 16 kHz", wu);
        // (2) grille complète (informatif) : part des bandes à moins de 0,5 dB, pire cas
        int cnt = 0, good = 0, goodOld = 0; double worst = 0;
        for (int t = 0; t < eq::kTypes; ++t) for (double f0 : {100.0, 500.0, 2000.0, 5000.0, 9000.0, 13000.0, 16000.0}) for (double g : {-12.0, 6.0, 12.0}) for (double q : {0.5, 0.707, 1.0, 2.0, 4.0}) {
            if (!eq::hasGain(t) && g != 6.0) continue;
            eq::Band b; b.freq = (float)f0; b.gain = (float)g; b.q = (float)q; b.type = t;
            const double d = errAt(b, 16000.0, false), o = errAt(b, 16000.0, true); ++cnt; good += d < 0.5; goodOld += o < 0.5; worst = std::max(worst, d);
        }
        std::printf("grille de %d bandes (tous types, 100 Hz - 16 kHz, Q 0,5 - 4, +/-12 dB) : %d a moins de 0,5 dB a 16 kHz (RBJ avant : %d), pire cas %.2f dB\n", cnt, good, goodOld, worst);
        CHECK(good > goodOld + cnt / 10, "la conception appariée doit nettement améliorer la grille");
        // la courbe affichée = la réponse réelle : mesure sur le moteur d'une bande cloche à 12 kHz +9 dB, 44,1 kHz
        eq::Params p = eq::factory(); p.b[5].freq = 12000.f; p.b[5].gain = 9.f; p.b[5].q = 1.f;
        for (double f : {6000.0, 12000.0, 16000.0}) { const double th = eq::responseDb(p, f, sr2), me = measureDb(p, f, sr2); CHECK(std::fabs(th - me) < 0.2, "courbe affichée / mesure à %g Hz : %.3f / %.3f", f, th, me); }
    }
    { // zéro allocation dans le temps réel : set + process (stéréo et mono), analyseur, fondu de bypass
        eq::Engine e; e.prepare(48000.0); std::vector<float> x(4096), y(4096), z(4096); for (auto& s : x) s = u(rng);
        odsp::Analyzer an; bypassfx::Fader fd; fd.prepare(48000.0, e.latency(), 512);
        eq::Params p = eq::presetParams(2);
        g_allocs = 0; g_count = true;
        for (int s = 0; s < 4096; s += 512) {
            p.b[3].freq = 800.f + (float)s; p.b[3].gain = 4.f; p.b[3].type = (s / 512) % eq::kTypes; p.out = (float)(s / 512);
            e.set(p); an.push(x.data() + s, 512);
            fd.run(x.data() + s, z.data() + s, 512, true, (s / 512) % 2 == 0, [&](float* l, float* r, int n) { e.process(l, r, l, r, n); });
            e.process(x.data() + s, nullptr, y.data() + s, z.data() + s, 512);
        }
        g_count = false;
        std::printf("allocations dans set/process/analyseur/bypass : %ld\n", g_allocs); CHECK(g_allocs == 0, "%ld allocation(s) dans le chemin temps réel", g_allocs);
    }
    { // bypass honoré (même Fader que processBlock) : sortie = entrée retardée (< -120 dB), saut au basculement < -60 dBFS
        eq::Engine e; const double sr2 = 48000.0; eq::Params p = eq::presetParams(1);
        auto m = bypassfx::measure(sr2, e.latency(), 512, [&] { e.prepare(sr2); e.set(p); }, [&](float* l, float* r, int n) { e.process(l, r, l, r ? r : l, n); });
        std::printf("bypass : erreur %.1f dB, saut au basculement %.1f dBFS (signal seul : %.1f)\n", m.exactDb, m.jumpDb, m.plainDb);
        CHECK(m.exactDb < -120.0, "bypass non exact : %.1f dB", m.exactDb); CHECK(m.jumpDb < -60.0, "clic au basculement : %.1f dBFS", m.jumpDb);
        // cas dur : traitement très différent du signal sec (gain x2 + retard de 100 échantillons) ; un basculement sec ferait un saut de -26 dBFS
        std::vector<float> dl(100, 0.f); std::vector<float> hist;
        auto m2 = bypassfx::measure(sr2, 100, 512, [&] { hist.assign(100, 0.f); }, [&](float* l, float* r, int n) {
            for (int i = 0; i < n; ++i) { hist.push_back(l[i]); l[i] = 2.f * hist[hist.size() - 101]; } if (r) std::copy(l, l + n, r); if (hist.size() > 8192) hist.erase(hist.begin(), hist.end() - 128); });
        std::printf("bypass, traitement très différent du sec : erreur %.1f dB, saut au basculement %.1f dBFS\n", m2.exactDb, m2.jumpDb);
        CHECK(m2.exactDb < -120.0, "bypass (cas dur) non exact : %.1f dB", m2.exactDb); CHECK(m2.jumpDb < -60.0, "clic (cas dur) : %.1f dBFS", m2.jumpDb);
    }
    { // lot E : pentes 12 / 24 / 48 dB/oct (coupe-bas 200 Hz, une octave et deux octaves plus bas) ; la courbe affichée suit la mesure
        for (int hpLp = 0; hpLp < 2; ++hpLp) for (int sl = 0; sl < 3; ++sl) {
            eq::Params p; for (int i = 0; i < eq::kBands; ++i) { p.b[i] = eq::defaultBand(i); p.b[i].on = false; }
            p.b[0].on = true; p.b[0].type = hpLp ? eq::HighCut : eq::LowCut; p.b[0].freq = hpLp ? 2000.f : 200.f; p.b[0].q = 0.7071f; p.b[0].slope = sl;
            const double f1 = hpLp ? 4000.0 : 100.0, f2 = hpLp ? 8000.0 : 50.0;
            const double m1 = measureDb(p, f1, sr), m2 = measureDb(p, f2, sr), t1 = eq::responseDb(p, f1, sr), t2 = eq::responseDb(p, f2, sr);
            std::printf("%s %d dB/oct : %.1f / %.1f dB à 1 et 2 octaves (théorie %.1f / %.1f)\n", hpLp ? "passe-bas" : "passe-haut", 12 << sl, m1, m2, t1, t2);
            CHECK(std::fabs(m1 - t1) < 0.3 && std::fabs(m2 - t2) < 0.5, "pente %d : mesure != courbe", sl);
            if (sl == 2) CHECK(m2 < m1 - 30.0, "pente 48 dB/oct trop molle (%.1f / %.1f)", m1, m2);
            if (sl == 0) { eq::Params q = p; q.b[0].slope = 0; CHECK(m1 > -20.0, "12 dB/oct trop raide"); }
        }
    }
    std::printf(fails ? "%d ECHEC(S)\n" : "TOUT EST BON\n", fails);
    return fails ? 1 : 0;
}
