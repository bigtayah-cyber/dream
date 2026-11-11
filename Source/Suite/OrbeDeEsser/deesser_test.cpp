// Test console du moteur Orbe DeEsser (sans JUCE). Cible : orbedeesser_test (EXCLUDE_FROM_ALL).
#include <cstdio>
#include <limits>
#include <random>
#include "../../Kit/BypassFadeTest.h"
#include "DeEsserDsp.h"

#include <cstdlib>
#include <new>
static long g_allocs = 0; static bool g_count = false;   // allocateur instrumenté
void* operator new(std::size_t n) { if (g_count) ++g_allocs; if (void* p = std::malloc(n ? n : 1)) return p; throw std::bad_alloc(); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { ++fails; std::printf("ECHEC: " __VA_ARGS__); std::printf("\n"); } } while (0)
static const double sr = 48000.0;
static void run(des::Engine& e, const std::vector<float>& x, std::vector<float>& y) {
    y.assign(x.size(), 0.f); std::vector<float> r(480);
    for (size_t p = 0; p < x.size(); p += 480) { const int c = (int)std::min<size_t>(480, x.size() - p); e.process(x.data() + p, x.data() + p, y.data() + p, r.data(), c); }
}
// énergie dans la bande [f0, f1] par filtre passe-bande
static double bandRms(const std::vector<float>& v, size_t a, size_t b, double f0, double f1) {
    odsp::Bq h, l; h.c = odsp::design(odsp::Kind::LowCut, sr, f0, 0, 0.707); l.c = odsp::design(odsp::Kind::HighCut, sr, f1, 0, 0.707);
    double s = 0; for (size_t i = 0; i < b; ++i) { const float y = l.process(h.process(v[i])); if (i >= a) s += (double)y * y; }
    return std::sqrt(s / (double)(b - a));
}
static double db(double x) { return 20 * std::log10(x + 1e-12); }

int main() {
    std::mt19937 rng(11); std::normal_distribution<float> nd(0.f, 1.f);
    const size_t N = (size_t)(sr * 3);
    // « voix » : harmoniques graves de 150 Hz jusqu'à 3 kHz, sans sibilance ; salve de sibilance 6-8 kHz entre 1,5 s et 2,2 s
    std::vector<float> voice(N), sib(N, 0.f), x(N);
    for (size_t i = 0; i < N; ++i) { double v = 0; for (int h = 1; h <= 12; ++h) v += std::sin(2 * odsp::kPi * 150.0 * h * (double)i / sr) / h; voice[i] = 0.12f * (float)v; }
    odsp::Bq hp, lp; hp.c = odsp::design(odsp::Kind::LowCut, sr, 6000, 0, 0.9); lp.c = odsp::design(odsp::Kind::HighCut, sr, 8000, 0, 0.9);
    for (size_t i = 0; i < N; ++i) { const float n = lp.process(hp.process(nd(rng))); if (i > (size_t)(sr * 1.5) && i < (size_t)(sr * 2.2)) sib[i] = 0.5f * n; }
    for (size_t i = 0; i < N; ++i) x[i] = voice[i] + sib[i];
    des::Params p; p.freq = 7000.f; p.width = 2.0f; p.thr = -30.f; p.range = 12.f;
    des::Engine e; e.prepare(sr); e.set(p);
    std::vector<float> y; run(e, x, y);
    const int L = e.latency();
    const size_t sa = (size_t)(sr * 1.7), sb = (size_t)(sr * 2.1);
    const double sIn = db(bandRms(x, sa, sb, 6000, 8000)), sOut = db(bandRms(y, sa + (size_t)L, sb + (size_t)L, 6000, 8000));
    std::printf("sibilance 6-8 kHz : %.1f -> %.1f dB (réduction %.1f dB), latence %d\n", sIn, sOut, sIn - sOut, L);
    CHECK(sIn - sOut >= 6.0, "sibilance insuffisamment réduite (%.1f dB)", sIn - sOut);
    const size_t va = (size_t)(sr * 0.3), vb = (size_t)(sr * 1.2);
    const double vIn = db(bandRms(x, va, vb, 100, 4000)), vOut = db(bandRms(y, va + (size_t)L, vb + (size_t)L, 100, 4000));
    std::printf("voix sans sibilance : %.2f -> %.2f dB (écart %.3f dB)\n", vIn, vOut, vOut - vIn);
    CHECK(std::fabs(vOut - vIn) < 0.5, "voix altérée de %.2f dB", vOut - vIn);
    // mode large-bande : la sibilance est réduite aussi
    p.split = false; e.prepare(sr); e.set(p); run(e, x, y);
    const double wOut = db(bandRms(y, sa + (size_t)L, sb + (size_t)L, 6000, 8000)); std::printf("large-bande : réduction %.1f dB\n", sIn - wOut);
    CHECK(sIn - wOut >= 6.0, "large-bande insuffisant");
    // latence mesurée = déclarée, transparence sans dépassement de seuil
    p.split = true; p.thr = 0.f; e.prepare(sr); e.set(p); run(e, voice, y);
    int best = -1; double be = 1e30;
    for (int d = 0; d < 200; ++d) { double err = 0; for (size_t i = 20000; i < 30000; ++i) { const double v = y[i] - voice[i - (size_t)d]; err += v * v; } if (err < be) { be = err; best = d; } }
    std::printf("latence mesurée %d (déclarée %d), erreur %.2e\n", best, L, be / 10000);
    CHECK(best == L, "latence %d != %d", best, L); CHECK(be == 0.0 || be / 10000 < 1e-12, "non transparent sous le seuil");
    { // bypass exact + extrêmes
        odsp::BypassDelay b; b.init(L); auto a = x, c = x; b.process(a.data(), c.data(), (int)N, true);
        bool ok = true; for (size_t i = 0; i < N; ++i) ok &= a[i] == (i >= (size_t)L ? x[i - (size_t)L] : 0.f); CHECK(ok, "bypass non transparent");
        for (double s2 : {44100.0, 96000.0}) { des::Params q; q.freq = 10000; q.width = 3; q.thr = -60; q.range = 24; q.atk = 0.05f; q.rel = 5; q.sharp = true; des::Engine f; f.prepare(s2); f.set(q); run(f, x, y); ok = true; for (float v : y) ok &= std::isfinite(v); CHECK(ok, "NaN"); }
    }
    { // bypass honoré (même Fader que processBlock) : sortie = entrée retardée de la latence (< -120 dB), saut au basculement < -60 dBFS
        des::Engine e; des::Params p; p.thr = -50.f; p.range = 12.f; std::vector<float> sc(4096);
        auto m = bypassfx::measure(sr, e.latency(), 512, [&] { e.prepare(sr); e.set(p); }, [&](float* l, float* r, int n) { e.process(l, r, l, r ? r : sc.data(), n); });
        std::printf("bypass %s : latence %d, erreur %.1f dB, saut au basculement %.1f dBFS (signal seul : %.1f)\n", "OrbeDeEsser", e.latency(), m.exactDb, m.jumpDb, m.plainDb);
        CHECK(m.exactDb < -120.0, "bypass non exact : %.1f dB", m.exactDb); CHECK(m.jumpDb < -60.0, "clic au basculement : %.1f dBFS", m.jumpDb);
    }
    { // zéro allocation dans le temps réel : set + process (réglages variables, écoute)
        des::Engine e; e.prepare(sr); des::Params p; std::vector<float> x(48000), y(48000), z(48000); for (size_t i = 0; i < x.size(); ++i) x[i] = 0.3f * std::sin(0.9f * (float)i);
        g_allocs = 0; g_count = true;
        for (int s = 0; s + 480 <= 48000; s += 480) { p.freq = 4000.f + (float)s * 0.1f; p.width = 1.f + (float)(s % 3); p.listen = (s / 480) % 5 == 0; p.split = (s / 480) % 2 == 0; e.set(p); e.process(x.data() + s, x.data() + s, y.data() + s, z.data() + s, 480); }
        g_count = false;
        std::printf("allocations dans set/process du De-esser : %ld\n", g_allocs); CHECK(g_allocs == 0, "%ld allocation(s) dans le chemin temps réel", g_allocs);
    }
    { // audit 3 (G2) : un NaN isolé ne doit pas rendre la sortie NaN pour toujours (avant : 100 % NaN après le glitch)
        des::Engine e; e.prepare(sr); des::Params p; e.set(p);
        std::vector<float> x(48000), y(48000), z(48000); for (size_t i = 0; i < x.size(); ++i) x[i] = 0.3f * std::sin(0.37f * (float)i);
        x[1000] = std::numeric_limits<float>::quiet_NaN(); x[1001] = -std::numeric_limits<float>::infinity();
        e.process(x.data(), x.data(), y.data(), z.data(), (int)x.size());
        int bad = 0; for (size_t i = 2000; i < y.size(); ++i) bad += !std::isfinite(y[i]) || !std::isfinite(z[i]);
        std::printf("NaN isolé en entrée : %d échantillon(s) non fini(s)\n", bad); CHECK(bad == 0, "sortie NaN après un NaN d'entrée (%d)", bad);
    }
    { // lookahead réglable : latence = 0, 5 ms, 10 ms mesurée = déclarée (impulsion) ; 0 ms = sans retard
        for (float ms : {0.f, 5.f, 10.f}) {
            des::Engine f; des::Params q; q.thr = 0.f; f.prepare(sr, ms); f.set(q);
            std::vector<float> imp(8000, 0.f), o; imp[1000] = 1.f; run(f, imp, o);
            size_t best = 0; for (size_t i = 0; i < o.size(); ++i) if (std::fabs(o[i]) > std::fabs(o[best])) best = i;
            std::printf("lookahead %.0f ms : latence déclarée %d, mesurée %d\n", ms, f.latency(), (int)best - 1000);
            CHECK(f.latency() == (int)std::lround(ms * 0.001 * sr) && (int)best - 1000 == f.latency(), "latence du lookahead %.0f ms", ms);
        }
    }
    { // liaison stéréo : sibilance à gauche seulement ; 100 % : la droite baisse aussi, 0 % : la droite est intacte
        std::vector<float> xl(N), xr(N), yl(N), yr(N); for (size_t i = 0; i < N; ++i) { xl[i] = voice[i] + sib[i]; xr[i] = voice[i]; }
        double rr[2];
        for (int c = 0; c < 2; ++c) {
            des::Params q; q.freq = 7000.f; q.width = 2.f; q.thr = -30.f; q.range = 12.f; q.link = c ? 0.f : 1.f; q.split = false;   // large bande : visible sur tout le signal
            des::Engine f; f.prepare(sr); f.set(q);
            for (size_t o = 0; o < N; o += 480) { const int k = (int)std::min<size_t>(480, N - o); f.process(xl.data() + o, xr.data() + o, yl.data() + o, yr.data() + o, k); }
            const int L2 = f.latency(); const size_t a2 = (size_t)(sr * 1.7), b2 = (size_t)(sr * 2.1);
            rr[c] = db(bandRms(xr, a2, b2, 100, 4000)) - db(bandRms(yr, a2 + (size_t)L2, b2 + (size_t)L2, 100, 4000));
        }
        std::printf("liaison stéréo : voie droite (sans sibilance) baisse de %.1f dB à 100 %%, %.1f dB à 0 %%\n", rr[0], rr[1]);
        CHECK(rr[0] > 3.0 && std::fabs(rr[1]) < 1.0, "liaison stéréo : %.1f / %.1f", rr[0], rr[1]);
    }
    { // « CH » : consonne non voisée à 3,6 kHz réduite, voyelle brillante (harmoniques 2-4 kHz AVEC bas du spectre) intacte ; coupé : sans effet
        std::vector<float> xc(N, 0.f), yc;
        odsp::Bq h2, l2; h2.c = odsp::design(odsp::Kind::LowCut, sr, 2800, 0, 0.9); l2.c = odsp::design(odsp::Kind::HighCut, sr, 4600, 0, 0.9);
        for (size_t i = 0; i < N; ++i) { const float n = l2.process(h2.process(nd(rng))); xc[i] = voice[i] * 0.05f + ((i > (size_t)(sr * 1.5) && i < (size_t)(sr * 2.2)) ? 0.5f * n : 0.f); }
        std::vector<float> xv(N); for (size_t i = 0; i < N; ++i) { double v = 0; for (int h = 1; h <= 40; ++h) v += std::sin(2 * odsp::kPi * 150.0 * h * (double)i / sr) / (double)h; xv[i] = 0.3f * (float)v; }   // voyelle brillante : 40 harmoniques, -6 dB/octave
        double red[3];
        for (int t = 0; t < 3; ++t) {
            des::Params q; q.freq = 7000.f; q.thr = -30.f; q.range = 12.f; q.chAmt = t == 0 ? 0.f : 1.f; des::Engine f; f.prepare(sr); f.set(q);
            const auto& in = t == 2 ? xv : xc; run(f, in, yc);
            const size_t a2 = t == 2 ? (size_t)(sr * 1.0) : (size_t)(sr * 1.7), b2 = t == 2 ? (size_t)(sr * 2.0) : (size_t)(sr * 2.1); const int L2 = f.latency();
            red[t] = db(bandRms(in, a2, b2, 2800, 4600)) - db(bandRms(yc, a2 + (size_t)L2, b2 + (size_t)L2, 2800, 4600));
        }
        std::printf("CH : consonne -%.1f dB (CH coupé : -%.1f dB), voyelle riche -%.1f dB\n", red[1], red[0], red[2]);
        CHECK(red[1] - red[0] > 3.0 && red[2] < 1.5, "CH : consonne %.1f, coupé %.1f, voyelle %.1f", red[1], red[0], red[2]);
    }
    { // auto-seuil : 5 s de voix avec sifflantes de 70 ms toutes les 600 ms ; le seuil proposé tombe près du niveau des sifflantes
        std::vector<float> xa((size_t)(sr * 6), 0.f); odsp::Bq h2, l2; h2.c = odsp::design(odsp::Kind::LowCut, sr, 6000, 0, 0.9); l2.c = odsp::design(odsp::Kind::HighCut, sr, 8000, 0, 0.9);
        for (size_t i = 0; i < xa.size(); ++i) { const float n = l2.process(h2.process(nd(rng))); const double ph = std::fmod((double)i / sr, 0.6); xa[i] = (ph < 0.07 ? 0.3f * n : 0.f) + 0.1f * voice[i % N]; }
        des::Engine f; des::Params q; q.freq = 7000.f; q.thr = 0.f; f.prepare(sr); f.set(q); f.startAuto(); std::vector<float> yo; run(f, xa, yo);
        CHECK(f.autoStatus() == 2, "auto-seuil non terminé"); const float thr = f.takeAutoThreshold();
        const double sibDb = db(bandRms(xa, (size_t)(sr * 0.01), (size_t)(sr * 0.06), 6000, 8000)); std::printf("auto-seuil : %.1f dBFS (sifflante ~%.1f dB en bande)\n", thr, sibDb);
        CHECK(thr < -5.f && thr > -50.f && f.autoStatus() == 0, "auto-seuil aberrant : %.1f", thr);
    }
    std::printf(fails ? "%d ECHEC(S)\n" : "TOUT EST BON\n", fails);
    return fails ? 1 : 0;
}
