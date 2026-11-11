// Test console du moteur Orbe Cabine (sans JUCE). Cible : orbecabine_test (EXCLUDE_FROM_ALL).
#include <cstdio>
#include <cstring>
#include <random>
#include "../../Kit/BypassFadeTest.h"
#include "CabineDsp.h"

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { ++fails; std::printf("ECHEC: " __VA_ARGS__); std::printf("\n"); } } while (0)

static std::vector<float> loud(int n, float sr) {
    std::mt19937 rng(3); std::uniform_real_distribution<float> u(-1.f, 1.f);
    std::vector<float> x((size_t)n);
    for (int i = 0; i < n; ++i) x[(size_t)i] = 0.6f * std::sin(2 * cab::kPi * 220.f * (float)i / sr) * (0.5f + 0.5f * std::sin(2 * cab::kPi * 2.f * (float)i / sr)) + 0.3f * u(rng);
    return x;
}
static void run(cab::Engine& e, const std::vector<float>& x, std::vector<float>& l, std::vector<float>& r) {
    l.assign(x.size(), 0.f); r = l;
    for (size_t p = 0; p < x.size(); p += 480) { const int c = (int)std::min<size_t>(480, x.size() - p); e.process(x.data() + p, nullptr, l.data() + p, r.data() + p, c); }
}

int main() {
    const float sr = 48000.f; const int N = (int)(sr * 3);
    const auto x = loud(N, sr);
    std::vector<float> l, r;
    for (int k = 0; k < cab::kPresets; ++k) {
        cab::Engine e; e.prepare(sr, 480); e.set(cab::presets()[k].p); run(e, x, l, r);
        float pk = 0; bool ok = true;
        for (size_t i = 0; i < l.size(); ++i) { ok &= std::isfinite(l[i]) && std::isfinite(r[i]); pk = std::max(pk, std::max(std::fabs(l[i]), std::fabs(r[i]))); }
        std::printf("preset %-14s pic %.2f (%.1f dBFS)\n", cab::presets()[k].name, pk, 20 * std::log10(pk + 1e-9f));
        CHECK(ok, "NaN/inf preset %d", k); CHECK(pk < 6.f, "pic hors borne preset %d : %f", k, pk);
    }
    { // réglages extrêmes
        cab::Params p = cab::mk(800, 12, 12, 12, 1000, 10, -60, 10, 0.5f, 20, 12, 1, 1, 1); p.out = 12;
        cab::Engine e; e.prepare(sr, 480); e.set(p); run(e, x, l, r);
        bool ok = true; for (size_t i = 0; i < l.size(); ++i) ok &= std::isfinite(l[i]) && std::isfinite(r[i]);
        CHECK(ok, "NaN en réglages extrêmes");
        for (double sr2 : {44100.0, 96000.0}) { cab::Engine e2; e2.prepare(sr2, 480); e2.set(p); run(e2, x, l, r); ok = true; for (float v : l) ok &= std::isfinite(v); CHECK(ok, "NaN à %g Hz", sr2); }
    }
    { // latence exacte : chaîne neutre (Brut), corrélation croisée
        cab::Engine e; e.prepare(sr, 480); cab::Params p; p.hp = 5.f; e.set(p);
        std::vector<float> s((size_t)N); for (int i = 0; i < N; ++i) s[(size_t)i] = 0.3f * std::sin(2 * cab::kPi * 1000.f * (float)i / sr) + 0.2f * std::sin(2 * cab::kPi * 3777.f * (float)i / sr);
        run(e, s, l, r);
        int best = -1; double be = 1e9;
        for (int d = 0; d < 40; ++d) { double err = 0; for (int i = 20000; i < 40000; ++i) { const double v = l[(size_t)i] - s[(size_t)(i - d)]; err += v * v; } if (err < be) { be = err; best = d; } }
        std::printf("latence mesurée %d (déclarée %d), erreur résiduelle %.2e\n", best, e.latency(), be / 20000);
        CHECK(best == e.latency(), "latence %d != %d", best, e.latency()); CHECK(be / 20000 < 1e-5, "chaîne neutre pas transparente");
    }
    { // contournement : exactement l'entrée retardée
        cab::BypassDelay b; b.init(cab::Engine().latency());
        std::vector<float> a(x.begin(), x.begin() + 5000), c = a, ref = a;
        b.process(a.data(), c.data(), 5000, false);
        bool ok = true; for (int i = 0; i < 5000; ++i) ok &= a[(size_t)i] == (i >= 16 ? ref[(size_t)(i - 16)] : 0.f) && c[(size_t)i] == a[(size_t)i];
        CHECK(ok, "bypass non transparent");
    }
    { // bypass honoré (même Fader que processBlock) : sortie = entrée retardée de la latence (< -120 dB), saut au basculement < -60 dBFS
        cab::Engine e; cab::Params p; p.rev = 0.4f; p.dly = 0.3f; p.sat = 0.5f; p.pres = 3.f; std::vector<float> sc(4096);
        auto m = bypassfx::measure(48000.0, e.latency(), 512, [&] { e.prepare(48000.0, 512); e.set(p); }, [&](float* l, float* r, int n) { e.process(l, r, l, r ? r : sc.data(), n); });
        std::printf("bypass %s : latence %d, erreur %.1f dB, saut au basculement %.1f dBFS (signal seul : %.1f)\n", "OrbeCabine", e.latency(), m.exactDb, m.jumpDb, m.plainDb);
        CHECK(m.exactDb < -120.0, "bypass non exact : %.1f dB", m.exactDb); CHECK(m.jumpDb < -60.0, "clic au basculement : %.1f dBFS", m.jumpDb);
    }
    { // lot E : presets tous différents (plus de doublon Beat/Brut)
        int same = 0;
        for (int a = 0; a < cab::kPresets; ++a) for (int b = a + 1; b < cab::kPresets; ++b) {
            const auto &x = cab::presets()[a].p, &y = cab::presets()[b].p;
            if (!std::memcmp(&x, &y, sizeof(x))) { ++same; std::printf("presets identiques : %s / %s\n", cab::presets()[a].name, cab::presets()[b].name); }
        }
        CHECK(same == 0, "%d paires de presets identiques", same);
    }
    { // lot E : de-esser en tête. Voyelle (220 Hz + harmoniques) puis sifflante (bruit 7 kHz) ; réduit la sifflante, pas la voyelle ; 0 = exact
        std::mt19937 rng(9); std::uniform_real_distribution<float> u(-1.f, 1.f);
        const int M = (int)(sr * 2); std::vector<float> v((size_t)M); float hpS = 0, z1 = 0;
        for (int i = 0; i < M; ++i) {
            const float t = (float)i / sr; float vow = 0; for (int h = 1; h <= 8; ++h) vow += std::sin(2 * cab::kPi * 220.f * (float)h * t) / (float)h;
            const float n = u(rng); const float hp = n - hpS; hpS += 0.45f * hp; (void)z1;   // bruit passe-haut (~ >4 kHz)
            const bool sib = (i / (int)(sr * 0.25f)) % 2 == 1;
            v[(size_t)i] = sib ? 0.5f * hp : 0.15f * vow;
        }
        auto rmsSeg = [&](const std::vector<float>& y, bool sib) { double e = 0; int c = 0; for (int i = 24000; i < M; ++i) if (((i / (int)(sr * 0.25f)) % 2 == 1) == sib && (i % 12000) > 3000 && (i % 12000) < 9000) { e += y[(size_t)i] * y[(size_t)i]; ++c; } return 10 * std::log10(e / c + 1e-20); };
        cab::Params p0; p0.hp = 5.f; cab::Engine a; a.prepare(sr, 480); a.set(p0); run(a, v, l, r); const std::vector<float> off = l;
        cab::Params p1 = p0; p1.des = 0.8f; cab::Engine b; b.prepare(sr, 480); b.set(p1); run(b, v, l, r);
        const double dS = rmsSeg(l, true) - rmsSeg(off, true), dV = rmsSeg(l, false) - rmsSeg(off, false);
        std::printf("de-esser 0.8 : sifflante %+.1f dB, voyelle %+.1f dB\n", dS, dV);
        CHECK(dS < -4.0, "sifflante pas réduite (%.1f dB)", dS); CHECK(dV > -0.5, "voyelle touchée (%.1f dB)", dV);
        cab::Engine c; c.prepare(sr, 480); c.set(p0); run(c, v, l, r); bool same = true; for (size_t i = 0; i < l.size(); ++i) same &= l[i] == off[i];
        CHECK(same, "de-esser à 0 pas identique");
        // gain d'entrée : +6 dB devant la chaîne neutre = +6 dB en sortie, latence inchangée
        cab::Params p2 = p0; p2.in = 6.f; cab::Engine g; g.prepare(sr, 480); g.set(p2); run(g, v, l, r);
        const double dG = rmsSeg(l, false) - rmsSeg(off, false); std::printf("gain d'entrée +6 dB : %+.2f dB\n", dG);
        CHECK(std::fabs(dG - 6.0) < 0.2, "gain d'entrée %.2f dB", dG);
    }
    std::printf(fails ? "%d ECHEC(S)\n" : "TOUT EST BON\n", fails);
    return fails ? 1 : 0;
}
