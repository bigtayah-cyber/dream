// Test console du moteur Orbe Denoiser (sans JUCE). Cible : orbedenoiser_test (EXCLUDE_FROM_ALL).
#include <cstdio>
#include <random>
#include <limits>
#include "../../Kit/BypassFadeTest.h"
#include <cstdlib>
#include <new>
#include "DenoiserDsp.h"

static long g_allocs = 0; static bool g_count = false;   // allocateur instrumenté
void* operator new(std::size_t n) { if (g_count) ++g_allocs; if (void* p = std::malloc(n ? n : 1)) return p; throw std::bad_alloc(); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { ++fails; std::printf("ECHEC: " __VA_ARGS__); std::printf("\n"); } } while (0)
static const float sr = 48000.f;

static void run(dn::Engine& e, const std::vector<float>& x, std::vector<float>& y) {
    y.assign(x.size(), 0.f); std::vector<float> r(480);
    for (size_t p = 0; p < x.size(); p += 480) { const int c = (int)std::min<size_t>(480, x.size() - p); e.process(x.data() + p, nullptr, y.data() + p, r.data(), c); }
}
static double rms(const std::vector<float>& v, size_t a, size_t b) { double s = 0; for (size_t i = a; i < b; ++i) s += (double)v[i] * v[i]; return std::sqrt(s / (double)(b - a)); }
static double db(double x) { return 20 * std::log10(x + 1e-12); }
// amplitude de la composante à f (démodulation)
static double tone(const std::vector<float>& v, size_t a, size_t b, double f) {
    double c = 0, s = 0; for (size_t i = a; i < b; ++i) { const double w = 2 * odsp::kPi * f * (double)i / sr; c += v[i] * std::cos(w); s += v[i] * std::sin(w); }
    return 2 * std::sqrt(c * c + s * s) / (double)(b - a);
}

int main() {
    std::mt19937 rng(7); std::normal_distribution<float> nd(0.f, 1.f);
    const float nAmp = std::pow(10.f, -40.f / 20.f);  // bruit blanc gaussien de -40 dBFS efficace
    auto noise = [&](size_t n) { std::vector<float> v(n); for (auto& s : v) s = nAmp * nd(rng); return v; };
    dn::Params prm;  // réglages par défaut

    { // bruit seul, profil appris : réduction >= 12 dB
        dn::Engine e; e.prepare(sr); e.set(prm);
        auto x = noise((size_t)(sr * 8)); std::vector<float> y;
        e.startLearn(); run(e, x, y);
        CHECK(e.profileKnown(), "profil non appris");
        const double rin = rms(x, (size_t)(sr * 4), (size_t)(sr * 8)), rout = rms(y, (size_t)(sr * 4), (size_t)(sr * 8));
        std::printf("bruit seul (profil appris) : %.1f dB de réduction\n", db(rin) - db(rout));
        CHECK(db(rin) - db(rout) >= 12.0, "réduction insuffisante %.1f", db(rin) - db(rout));
        bool ok = true; for (float v : y) ok &= std::isfinite(v); CHECK(ok, "NaN");
    }
    { // bruit seul, mode adaptatif sans profil
        dn::Engine e; e.prepare(sr); e.set(prm);
        auto x = noise((size_t)(sr * 10)); std::vector<float> y; run(e, x, y);
        const double r = db(rms(x, (size_t)(sr * 6), (size_t)(sr * 10))) - db(rms(y, (size_t)(sr * 6), (size_t)(sr * 10)));
        std::printf("bruit seul (adaptatif) : %.1f dB de réduction\n", r);
        CHECK(r >= 8.0, "adaptatif trop faible %.1f", r);
    }
    { // sinus 1 kHz -20 dBFS dans le bruit : sinus préservé (< 1 dB)
        dn::Engine e; e.prepare(sr); e.set(prm);
        auto n0 = noise((size_t)(sr * 2.5)); std::vector<float> y; run(e, n0, y);  // silence : apprentissage
        e.startLearn(); run(e, n0, y);
        auto x = noise((size_t)(sr * 6)); for (size_t i = 0; i < x.size(); ++i) x[i] += 0.1f * std::sin(2 * (float)odsp::kPi * 1000.f * (float)i / sr);
        run(e, x, y);
        const size_t a = (size_t)(sr * 3), b = (size_t)(sr * 6);
        const double din = db(tone(x, a, b, 1000)), dout = db(tone(y, a + dn::kLatency, b + dn::kLatency > y.size() ? y.size() : b, 1000));
        // la sortie est retardée : on compare sur une fenêtre décalée de la latence
        std::vector<float> yy(y.begin() + dn::kLatency, y.end()); yy.resize(x.size());
        const double dout2 = db(tone(yy, a, b - 1000, 1000));
        std::printf("sinus 1 kHz : entrée %.2f dBFS, sortie %.2f dBFS (écart %.2f dB)\n", din, dout2, dout2 - din);
        CHECK(std::fabs(dout2 - din) < 1.0, "sinus altéré de %.2f dB", dout2 - din); (void)dout;
        std::vector<float> nn(yy.begin() + a, yy.begin() + b); (void)nn;
    }
    { // latence mesurée = latence déclarée (réduction 0 dB => sortie = entrée retardée)
        dn::Params p0 = prm; p0.reduction = 0.f; p0.adaptive = false;
        dn::Engine e; e.prepare(sr); e.set(p0);
        std::vector<float> x((size_t)(sr * 2)), y;
        for (size_t i = 0; i < x.size(); ++i) x[i] = 0.3f * std::sin(2 * (float)odsp::kPi * 1000.f * (float)i / sr) + 0.2f * std::sin(2 * (float)odsp::kPi * 3777.f * (float)i / sr) + 0.05f * nd(rng);
        run(e, x, y);
        int best = -1; double be = 1e30;
        for (int d = 0; d < 4000; ++d) { double err = 0; for (size_t i = 20000; i < 30000; ++i) { const double v = y[i] - x[i - (size_t)d]; err += v * v; } if (err < be) { be = err; best = d; } }
        std::printf("latence mesurée %d (déclarée %d), erreur %.2e\n", best, e.latency(), be / 10000);
        CHECK(best == e.latency(), "latence %d != %d", best, e.latency()); CHECK(be / 10000 < 1e-8, "reconstruction imparfaite");
    }
    { // bypass transparent (exactement l'entrée retardée)
        odsp::BypassDelay b; b.init(dn::kLatency);
        auto x = noise(6000); auto a = x, c = x;
        b.process(a.data(), c.data(), 6000, true);
        bool ok = true; for (size_t i = 0; i < 6000; ++i) ok &= a[i] == (i >= (size_t)dn::kLatency ? x[i - (size_t)dn::kLatency] : 0.f) && c[i] == a[i];
        CHECK(ok, "bypass non transparent");
    }
    { // réglages extrêmes, ronflement, écoute du bruit retiré, autres fréquences d'échantillonnage
        for (double s : {44100.0, 96000.0}) for (int hum : {0, 1, 2}) {
            dn::Engine e; e.prepare(s); dn::Params p; p.reduction = 24; p.sens = 1; p.smooth = 1; p.air = 1; p.hum = hum; p.listen = hum == 2;
            e.set(p); auto x = noise(100000); std::vector<float> y; run(e, x, y); e.startLearn(); run(e, x, y);
            bool ok = true; for (float v : y) ok &= std::isfinite(v) && std::fabs(v) < 4.f; CHECK(ok, "NaN/hors borne à %g Hz hum %d", s, hum);
        }
    }
    { // bypass honoré (même Fader que processBlock) : sortie = entrée retardée de la latence (< -120 dB), saut au basculement < -60 dBFS
        dn::Engine e; dn::Params p; p.reduction = 18.f; std::vector<float> sc(4096);
        auto m = bypassfx::measure(sr, e.latency(), 512, [&] { e.prepare(sr); e.set(p); }, [&](float* l, float* r, int n) { e.process(l, r, l, r ? r : sc.data(), n); });
        std::printf("bypass %s : latence %d, erreur %.1f dB, saut au basculement %.1f dBFS (signal seul : %.1f)\n", "OrbeDenoiser", e.latency(), m.exactDb, m.jumpDb, m.plainDb);
        CHECK(m.exactDb < -120.0, "bypass non exact : %.1f dB", m.exactDb); CHECK(m.jumpDb < -60.0, "clic au basculement : %.1f dBFS", m.jumpDb);
    }
    { // zéro allocation dans le temps réel : set + process (apprentissage, hum, écoute, adaptatif), après prepare
        dn::Engine e; e.prepare(sr); dn::Params p; std::vector<float> x(48000), y(48000), z(48000); for (auto& s : x) s = 0.01f * nd(rng);
        std::vector<float> prof((size_t)dn::kBins, 1e-6f);
        g_allocs = 0; g_count = true;
        e.startLearn(); e.setProfile(prof);
        for (int pass = 0; pass < 4; ++pass) { p.hum = pass % 3; p.listen = pass == 2; p.adaptive = pass != 1; e.set(p); if (pass == 2) e.startLearn(); for (int s = 0; s + 480 <= 48000; s += 480) e.process(x.data() + s, x.data() + s, y.data() + s, z.data() + s, 480); }
        g_count = false;
        std::printf("allocations dans set/process du Denoiser (apprentissage, hum, écoute) : %ld\n", g_allocs); CHECK(g_allocs == 0, "%ld allocation(s) dans le chemin temps réel", g_allocs);
    }
    { // audit 3 : un NaN isolé en entrée ne doit pas rendre la sortie NaN pour toujours (avant : 100 % NaN après le glitch)
        dn::Engine e; e.prepare(sr); e.set(prm);
        auto x = noise((size_t)(sr * 3)); for (size_t i = 0; i < x.size(); ++i) x[i] += 0.1f * std::sin(2 * (float)odsp::kPi * 440.f * (float)i / sr);
        x[5000] = std::numeric_limits<float>::quiet_NaN(); x[5001] = std::numeric_limits<float>::infinity(); std::vector<float> y; run(e, x, y);
        int bad = 0; for (size_t i = (size_t)sr; i < y.size(); ++i) bad += !std::isfinite(y[i]);
        std::printf("NaN isolé en entrée : %d échantillon(s) non fini(s) après 1 s\n", bad); CHECK(bad == 0, "sortie NaN après un NaN d'entrée (%d)", bad);
    }
    { // audit 3 (M4) : l'apprentissage moyenne les deux voies (gauche muette, bruit à droite => profil non nul)
        dn::Engine e; e.prepare(sr); e.set(prm);
        const size_t n = (size_t)(sr * 3); std::vector<float> l(n, 0.f), r = noise(n), ol(n), orr(n);
        e.startLearn(); for (size_t p = 0; p < n; p += 480) e.process(l.data() + p, r.data() + p, ol.data() + p, orr.data() + p, (int)std::min<size_t>(480, n - p));
        std::vector<float> prof; e.getProfile(prof); double m = 0; for (float v : prof) m += v; m /= (double)prof.size();
        std::printf("apprentissage stéréo (gauche muette) : profil moyen %.3g\n", m); CHECK(e.profileKnown() && m > 0.0, "profil appris sur la seule voie gauche");
    }
    { // B2 (non-régression demandée par 09-validation) : La 440 Hz -20 dBFS tenu 8 s dans un bruit -50 dBFS, réglages d'usine
        dn::Engine e; e.prepare(sr); e.set(prm);
        const size_t n = (size_t)(sr * 8); std::vector<float> x(n), y; for (size_t i = 0; i < n; ++i) x[i] = 0.1f * std::sin(2 * (float)odsp::kPi * 440.f * (float)i / sr) + 0.1f * nAmp * nd(rng);
        run(e, x, y); std::vector<float> yy(y.begin() + dn::kLatency, y.end());
        const double d = db(tone(yy, (size_t)(sr * 6.5), (size_t)(sr * 7.5), 440)) - db(tone(x, (size_t)(sr * 6.5), (size_t)(sr * 7.5), 440));
        std::printf("note tenue 7 s : %.2f dB\n", d); CHECK(std::fabs(d) < 1.0, "note tenue avalée (%.1f dB)", d);
    }
    { // audit 3 : bruit de pièce grave (brownien, passe-bas 400 Hz) : la garde de tonalité ne doit pas sous-estimer le bruit en pente
        // (avant : -3,2 dB de réduction mesurée de 80 Hz à 16 kHz ; sans garde : -17,8 dB)
        dn::Engine e; e.prepare(sr); e.set(prm);
        const size_t n = (size_t)(sr * 8); std::vector<float> x(n), y; double br = 0; odsp::Bq lp; lp.c = odsp::design(odsp::Kind::HighCut, sr, 400, 0, 0.6);
        for (auto& v : x) { br = 0.995 * br + 0.05 * nd(rng); v = 0.01f * (lp.process((float)br) * 3.f + 0.3f * nd(rng)); }
        run(e, x, y);
        auto hp80 = [&](std::vector<float> v) { odsp::Bq a, b; a.c = b.c = odsp::design(odsp::Kind::LowCut, sr, 80, 0, 0.7071); for (auto& s : v) s = b.process(a.process(s)); return v; };
        const auto xb = hp80(x), yb = hp80(y);
        const double r = db(rms(xb, (size_t)(sr * 4), (size_t)(sr * 7.9))) - db(rms(yb, (size_t)(sr * 4) + dn::kLatency, (size_t)(sr * 7.9) + dn::kLatency));
        std::printf("bruit de pièce grave (adaptatif, 80 Hz-16 kHz) : %.1f dB de réduction\n", r); CHECK(r >= 12.0, "bruit grave mal estimé : %.1f dB", r);
    }
    { // mode Prise : latence 512 mesurée = déclarée, réduction >= 8 dB sur un bruit blanc, voix conservée
        dn::Engine e; dn::Params lp = prm; lp.live = true; e.prepare(sr, true); e.set(lp);
        CHECK(e.latency() == dn::kLatencyLive && e.liveActive(), "latence du mode Prise : %d", e.latency());
        std::vector<float> x(48000, 0.f); x[5000] = 1.f; std::vector<float> y; dn::Params p0 = lp; p0.reduction = 0.f; e.set(p0); run(e, x, y);
        size_t best = 0; for (size_t i = 0; i < y.size(); ++i) if (std::fabs(y[i]) > std::fabs(y[best])) best = i;
        std::printf("mode Prise : latence mesurée %d (déclarée %d)\n", (int)best - 5000, e.latency()); CHECK((int)best - 5000 == e.latency(), "latence Prise mesurée %d", (int)best - 5000);
        dn::Engine g; g.prepare(sr, true); g.set(lp);
        const auto nz = noise((size_t)(sr * 6)); std::vector<float> yn; run(g, nz, yn);
        const double r = db(rms(nz, (size_t)(sr * 3), (size_t)(sr * 5.9))) - db(rms(yn, (size_t)(sr * 3) + 512, (size_t)(sr * 5.9) + 512));
        std::printf("mode Prise : bruit blanc adaptatif, réduction %.1f dB (plafond 12 dB)\n", r); CHECK(r >= 6.0 && r <= 14.5, "réduction du mode Prise : %.1f dB", r);
        // profil appris en mode Prise, puis lu en mode normal : même ordre de grandeur de puissance par bin
        dn::Engine a; a.prepare(sr, true); a.set(lp); a.startLearn(); std::vector<float> ya; run(a, noise((size_t)(sr * 3)), ya);
        std::vector<float> pa; a.getProfile(pa); dn::Engine b; b.prepare(sr); b.set(prm); b.startLearn(); std::vector<float> yb; run(b, noise((size_t)(sr * 3)), yb);
        std::vector<float> pb; b.getProfile(pb); double sa = 0, sb = 0; for (int k = 50; k < 900; ++k) { sa += pa[(size_t)k]; sb += pb[(size_t)k]; }
        std::printf("profil Prise / normal : %.2f dB d'écart\n", 10 * std::log10(sa / sb)); CHECK(std::fabs(10 * std::log10(sa / sb)) < 1.5, "profil du mode Prise hors échelle (%.1f dB)", 10 * std::log10(sa / sb));
    }
    { // « entre les phrases » : le silence baisse de gapDb de plus, la voix (note tenue + bruit) reste intacte
        const size_t n = (size_t)(sr * 10); std::vector<float> x(n);
        for (size_t i = 0; i < n; ++i) { const bool voice = i > (size_t)(sr * 4) && i < (size_t)(sr * 6); x[i] = (voice ? 0.2f * std::sin(2 * (float)odsp::kPi * 220.f * (float)i / sr) : 0.f) + 0.1f * nAmp * nd(rng) * 3.f; }
        double sil[2], vo[2];
        for (int c = 0; c < 2; ++c) { dn::Engine e; dn::Params q = prm; q.gapDb = c ? 15.f : 0.f; e.prepare(sr); e.set(q); std::vector<float> y; run(e, x, y);
            sil[c] = db(rms(y, (size_t)(sr * 8) + 2048, (size_t)(sr * 9.8) + 2048)); vo[c] = db(tone(y, (size_t)(sr * 4.5) + 2048, (size_t)(sr * 5.5) + 2048, 220)); }
        std::printf("entre les phrases 15 dB : silence %.1f -> %.1f dB, voix %.1f -> %.1f dB\n", sil[0], sil[1], vo[0], vo[1]);
        CHECK(sil[0] - sil[1] > 8.0, "gap : silence baisse de %.1f dB seulement", sil[0] - sil[1]); CHECK(std::fabs(vo[0] - vo[1]) < 0.5, "gap : la voix change de %.1f dB", vo[1] - vo[0]);
    }
    { // 3 bandes : bandHi = +12 dB retire davantage d'aigus (bruit blanc) et ne touche pas le grave ; 0 = identique à l'ancien
        dn::Engine e0, e1; dn::Params q = prm; e0.prepare(sr); e0.set(q); q.bandHi = 12.f; e1.prepare(sr); e1.set(q);
        const auto nz = noise((size_t)(sr * 6)); std::vector<float> y0, y1; run(e0, nz, y0); run(e1, nz, y1);
        auto band = [&](const std::vector<float>& v, double f0, double f1) { odsp::Bq a, b; a.c = odsp::design(odsp::Kind::LowCut, sr, f0, 0, 0.7071); b.c = odsp::design(odsp::Kind::HighCut, sr, f1, 0, 0.7071); std::vector<float> o(v); for (auto& s : o) s = b.process(a.process(s)); return db(rms(o, (size_t)(sr * 3), (size_t)(sr * 5.9))); };
        const double hi = band(y0, 7000, 12000) - band(y1, 7000, 12000), lo = band(y0, 300, 3000) - band(y1, 300, 3000);
        std::printf("bandHi +12 dB : aigus -%.1f dB de plus, médium %.1f dB\n", hi, lo); CHECK(hi > 4.0 && std::fabs(lo) < 1.0, "bandes : aigus %.1f, médium %.1f", hi, lo);
    }
    { // profil enregistré à 44,1 kHz relu à 48 kHz : les bins sont recalés en Hz
        dn::Engine a; a.prepare(44100.f); a.set(prm); std::vector<float> pr((size_t)dn::kBins, 0.f); for (int k = 0; k < dn::kBins; ++k) pr[(size_t)k] = k < 500 ? 1e-3f : 1e-6f;
        dn::Engine b; b.prepare(sr); b.set(prm); b.setProfile(pr, 44100.f); std::vector<float> z(4096, 0.f), zy; run(b, z, zy); std::vector<float> got; b.getProfile(got);
        const int edge = (int)std::lround(500.0 * 48000.0 / 44100.0); std::printf("profil 44,1 -> 48 kHz : arête du profil au bin %d (attendu %d)\n", [&] { int k = 0; while (k < dn::kBins && got[(size_t)k] > 1e-4f) ++k; return k; }(), edge);
        int k = 0; while (k < dn::kBins && got[(size_t)k] > 1e-4f) ++k; CHECK(std::abs(k - edge) <= 2, "profil non recalé : arête %d (attendu %d)", k, edge); (void)a;
    }
    std::printf(fails ? "%d ECHEC(S)\n" : "TOUT EST BON\n", fails);
    return fails ? 1 : 0;
}
