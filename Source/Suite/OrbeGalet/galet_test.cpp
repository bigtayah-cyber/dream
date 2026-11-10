// Test console du moteur Orbe Galet (sans JUCE). Cible : orbegalet_test (EXCLUDE_FROM_ALL).
#include <cstdio>
#include <cstdlib>
#include <new>
#include <random>
#include "../../Kit/BypassFadeTest.h"
#include "../../Kit/Sanitize.h"
#include "GaletDsp.h"

static long g_allocs = 0; static bool g_count = false;   // allocateur instrumenté
void* operator new(std::size_t n) { if (g_count) ++g_allocs; if (void* p = std::malloc(n ? n : 1)) return p; throw std::bad_alloc(); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { ++fails; std::printf("ECHEC: " __VA_ARGS__); std::printf("\n"); } } while (0)
using namespace gal;

static Shape flat(float y) { Shape s; s.n = 1; s.p[0] = {0, y, 0}; return s; }
// Traite x (mono dupliqué en stéréo) par blocs de `blk` ; hôte figé (arrêté) sauf `host` fourni.
static void run(Engine& e, const std::vector<float>& x, std::vector<float>& yl, std::vector<float>& yr, int blk, Host h = {}) {
    yl.assign(x.size(), 0.f); yr.assign(x.size(), 0.f);
    for (size_t p = 0; p < x.size(); p += (size_t)blk) {
        const int c = (int)std::min<size_t>((size_t)blk, x.size() - p);
        e.beginBlock(h, nullptr, 0); e.process(x.data() + p, x.data() + p, yl.data() + p, yr.data() + p, c);
    }
}
static double db(double v) { return 20 * std::log10(v + 1e-30); }

// DFT d'un bin (fenêtre rectangulaire : signal exactement périodique sur N)
static double binPow(const std::vector<float>& v, size_t a, size_t N, size_t k) {
    double re = 0, im = 0; const double w = 2 * kPi * (double)k / (double)N;
    for (size_t i = 0; i < N; ++i) { re += v[a + i] * std::cos(w * (double)i); im -= v[a + i] * std::sin(w * (double)i); }
    return (re * re + im * im) / ((double)N * N);
}
// Puissance de chaque bin par FFT radix-2 (N puissance de 2)
static std::vector<double> spectrum(const std::vector<float>& v, size_t a, size_t N) {
    std::vector<double> re(N), im(N, 0.0);
    for (size_t i = 0; i < N; ++i) re[i] = v[a + i];
    for (size_t i = 1, j = 0; i < N; ++i) { size_t bit = N >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; if (i < j) { std::swap(re[i], re[j]); } }
    for (size_t len = 2; len <= N; len <<= 1) {
        const double ang = -2 * kPi / (double)len;
        for (size_t i = 0; i < N; i += len) for (size_t k = 0; k < len / 2; ++k) {
            const double wr = std::cos(ang * (double)k), wi = std::sin(ang * (double)k);
            const double ur = re[i + k], ui = im[i + k], vr = re[i + k + len / 2] * wr - im[i + k + len / 2] * wi, vi = re[i + k + len / 2] * wi + im[i + k + len / 2] * wr;
            re[i + k] = ur + vr; im[i + k] = ui + vi; re[i + k + len / 2] = ur - vr; im[i + k + len / 2] = ui - vi;
        }
    }
    std::vector<double> pw(N / 2 + 1); for (size_t k = 0; k <= N / 2; ++k) pw[k] = (re[k] * re[k] + im[k] * im[k]) / ((double)N * N);
    return pw;
}
// Alias / signal (dB) : sinus calé sur un bin, harmoniques légitimes (< Nyquist) = signal, tout le reste (hors continu) = alias.
static double aliasDb(double sr, int type, float drive, float color, size_t bin, double* fOut = nullptr, double* audible = nullptr) {
    const size_t N = 65536; const double f0 = (double)bin * sr / (double)N; if (fOut) *fOut = f0;
    Engine e; e.prepare(sr, 512); Params p; p.volOn = false; p.drvOn = true; p.drvType = type; p.drive = drive; p.color = color; p.drvDepth = 1.f; e.set(p); e.pushShape(flat(1.f));
    std::vector<float> x(N + 16384), yl, yr; for (size_t i = 0; i < x.size(); ++i) x[i] = 0.5f * (float)std::sin(2 * kPi * f0 * (double)i / sr);
    run(e, x, yl, yr, 512);
    const auto pw = spectrum(yl, 16384, N);
    double sig = 0, ali = 0, aud = 0; const size_t k20 = (size_t)(20000.0 * (double)N / sr);
    for (size_t k = 8; k <= N / 2; ++k) { const bool harm = k % bin == 0; (harm ? sig : ali) += pw[k]; if (!harm && k <= k20) aud += pw[k]; }
    if (audible) *audible = 10 * std::log10(aud / sig + 1e-30);
    return 10 * std::log10(ali / sig + 1e-30);
}

int main() {
    const double sr = 48000.0;
    std::mt19937 rng(7); std::uniform_real_distribution<float> un(-0.5f, 0.5f);
    std::vector<float> noise(48000); for (auto& v : noise) v = un(rng);
    std::vector<float> yl, yr;
    const int L = Engine::kLatency;
    std::printf("latence déclarée : %d échantillons (%.2f ms à 48 kHz)\n", L, L / 48.0);

    { // 1. courbe à 0 : signal neutre (tous les modules actifs, profondeurs pleines)
        Engine e; e.prepare(sr, 512); Params p; p.volOn = p.fltOn = p.drvOn = p.panOn = true; p.volDepth = p.fltDepth = p.drvDepth = 1; p.pan = 1; p.width = 1; p.drive = 36; p.color = 1; p.fltRes = 1; e.set(p); e.pushShape(flat(0.f));
        run(e, noise, yl, yr, 480);
        double err = 0; for (size_t i = (size_t)L; i < noise.size(); ++i) err = std::max(err, (double)std::max(std::fabs(yl[i] - noise[i - (size_t)L]), std::fabs(yr[i] - noise[i - (size_t)L])));
        std::printf("courbe à 0, 4 modules à fond : erreur max %.1f dB (sortie = entrée retardée de %d)\n", db(err), L);
        CHECK(err < 1e-6, "courbe à 0 non neutre (%.1f dB)", db(err));
    }
    { // 2. pompage : profondeur exacte (forme constante 1 puis 0,4)
        for (float lvl : {1.f, 0.4f}) {
            Engine e; e.prepare(sr, 512); Params p; p.volDepth = 0.75f; e.set(p); e.pushShape(flat(lvl));
            std::vector<float> dc(24000, 0.5f); run(e, dc, yl, yr, 256);
            const double g = yl.back() / 0.5, want = 1.0 - 0.75 * lvl;
            std::printf("pompage : forme %.1f, profondeur 75 %% -> gain %.6f (attendu %.6f)\n", lvl, g, want);
            CHECK(std::fabs(g - want) < 1e-5, "profondeur inexacte");
        }
    }
    { // 3. aucun clic : saut de position PPQ, puis de tempo, porte à fronts raides (lissage par défaut 4 ms et minimal 1 ms)
        for (float smooth : {4.f, 1.f}) {
            Engine e; e.prepare(sr, 512); Params p; p.volDepth = 1.f; p.div = 4; p.smoothMs = smooth; e.set(p); e.pushShape(startShape(5));
            std::vector<float> dc(256 * 560, 0.5f); yl.assign(dc.size(), 0.f); yr = yl;
            Host h; h.playing = h.hasPos = h.hasBpm = true; h.bpm = 120; double ppq = 0;
            for (size_t pos = 0; pos < dc.size(); pos += 256) {
                if (pos == 256 * 180) ppq = 3.6;      // saut de position (boucle de l'hôte)
                if (pos == 256 * 290) ppq = 0.23;
                if (pos == 256 * 400) h.bpm = 87;     // saut de tempo
                h.ppq = ppq; e.beginBlock(h, nullptr, 0); e.process(dc.data() + pos, dc.data() + pos, yl.data() + pos, yr.data() + pos, 256);
                ppq += 256.0 / sr * h.bpm / 60.0;
            }
            double step = 0; for (size_t i = 1000; i < dc.size(); ++i) step = std::max(step, (double)std::fabs(yl[i] - yl[i - 1]));
            std::printf("clic : lissage %.0f ms, plus grand saut d'un échantillon %.5f (%.1f dBFS) pour un saut brut de 0,5\n", smooth, step, db(step));
            CHECK(step < (smooth >= 4.f ? 0.003 : 0.01), "discontinuité %.4f", step);
        }
        // changement de forme en cours de lecture : fondu sans marche
        Engine e; e.prepare(sr, 512); Params p; p.volDepth = 1.f; p.smoothMs = 1.f; e.set(p); e.pushShape(flat(0.f));
        std::vector<float> dc(128 * 188, 0.5f); yl.assign(dc.size(), 0.f); yr = yl; double step = 0;
        for (size_t pos = 0; pos < dc.size(); pos += 128) { if (pos == 12032) e.pushShape(flat(1.f)); e.beginBlock({}, nullptr, 0); e.process(dc.data() + pos, dc.data() + pos, yl.data() + pos, yr.data() + pos, 128); }
        for (size_t i = 400; i < dc.size(); ++i) step = std::max(step, (double)std::fabs(yl[i] - yl[i - 1]));
        std::printf("forme 0 -> 1 d'un coup : plus grand saut %.5f\n", step); CHECK(step < 0.001, "zipper au changement de forme");
    }
    { // 4. alias de la distorsion (sinus haut, 44,1 kHz) : objectif < -70 dB
        struct Case { int type; float drive, color; const char* nm; };
        const Case cs[] = {{0, 6, 0, "tanh 6 dB"}, {0, 12, 0, "tanh 12 dB"}, {0, 18, 0, "tanh 18 dB"}, {0, 24, 0, "tanh 24 dB"}, {0, 36, 0, "tanh 36 dB"},
                           {1, 18, 0, "arctan 18 dB"}, {0, 18, 0.6f, "tanh 18 dB + couleur 0,6"}, {2, 18, 0, "pli doux 18 dB"}};
        for (auto& c : cs) {
            for (size_t bin : {(size_t)10403, (size_t)5003}) {
                double f0, aud; const double a = aliasDb(44100, c.type, c.drive, c.color, bin, &f0, &aud);
                std::printf("alias %-26s sinus %5.0f Hz -> %6.1f dB (0-20 kHz : %6.1f dB)\n", c.nm, f0, a, aud);
                if (c.type == 0 && c.drive <= 18 && c.color == 0) CHECK(aud < -70, "alias %s : %.1f dB", c.nm, aud);
            }
        }
    }
    { // 5. latence mesurée = déclarée (chemin sur-échantillonné quasi linéaire, drive 0,5 dB)
        Engine e; e.prepare(sr, 512); Params p; p.volOn = false; p.drvOn = true; p.drive = 0.5f; e.set(p); e.pushShape(flat(1.f));
        run(e, noise, yl, yr, 480);
        int best = -1; double bc = -1;
        for (int d = 0; d < 200; ++d) { double c = 0; for (size_t i = 10000; i < 40000; ++i) c += (double)yl[i] * noise[i - (size_t)d]; if (c > bc) { bc = c; best = d; } }
        std::printf("latence mesurée (distorsion active) : %d (déclarée %d)\n", best, L); CHECK(best == L, "latence %d != %d", best, L);
    }
    { // 6. mono / stéréo : entrée mono = entrée stéréo identique
        Params p; p.fltOn = p.drvOn = p.panOn = true; p.pan = 0.7f; p.width = 0.5f; p.div = 5;
        Engine a, b; a.prepare(sr, 512); b.prepare(sr, 512); a.set(p); b.set(p); a.pushShape(startShape(0)); b.pushShape(startShape(0));
        std::vector<float> ml(noise.size()), mr(noise.size()), sl(noise.size()), sr_(noise.size()); double d = 0;
        for (size_t pos = 0; pos < noise.size(); pos += 400) {
            const int c = (int)std::min<size_t>(400, noise.size() - pos);
            a.beginBlock({}, nullptr, 0); a.process(noise.data() + pos, nullptr, ml.data() + pos, mr.data() + pos, c);
            b.beginBlock({}, nullptr, 0); b.process(noise.data() + pos, noise.data() + pos, sl.data() + pos, sr_.data() + pos, c);
        }
        for (size_t i = 0; i < noise.size(); ++i) d = std::max(d, (double)std::max(std::fabs(ml[i] - sl[i]), std::fabs(mr[i] - sr_[i])));
        std::printf("mono vs stéréo identique : écart max %.2e\n", d); CHECK(d < 1e-7, "mono != stéréo");
    }
    { // 7. tailles de bloc 1..8192 : résultat identique (moteur sans état de bloc)
        Params p; p.fltOn = p.drvOn = p.panOn = true; p.pan = -0.5f; p.div = 6; p.feel = 1;
        std::vector<float> x(48000 * 2); for (size_t i = 0; i < x.size(); ++i) x[i] = noise[i % noise.size()];
        std::vector<float> ref, rr; double worst = 0;
        for (int blk : {512, 1, 7, 64, 333, 4096, 8192}) {
            Engine e; e.prepare(sr, blk); e.set(p); e.pushShape(startShape(7)); run(e, x, yl, yr, blk);
            if (blk == 512) { ref = yl; continue; }
            double d = 0; for (size_t i = 0; i < x.size(); ++i) d = std::max(d, (double)std::fabs(yl[i] - ref[i]));
            worst = std::max(worst, d);
        }
        std::printf("blocs 1/7/64/333/4096/8192 vs 512 : écart max %.2e\n", worst); CHECK(worst < 1e-5, "dépend de la taille de bloc");
    }
    { // 8. zéro NaN : extrêmes, automation brutale, entrée énorme et dénormaux, retrig MIDI
        bool ok = true;
        for (double s2 : {22050.0, 44100.0, 96000.0, 192000.0}) {
            Engine e; e.prepare(s2, 512); std::mt19937 r(3); std::uniform_real_distribution<float> u(0, 1);
            std::vector<float> x(4096), l(4096), rr(4096);
            for (int b = 0; b < 200; ++b) {
                Params p; p.sync = u(r) < 0.5f; p.div = (int)(u(r) * 8); p.feel = (int)(u(r) * 3); p.hz = 0.02f + u(r) * 40; p.retrig = (int)(u(r) * 3);
                p.smoothMs = 1 + u(r) * 60; p.interp = (int)(u(r) * 3); p.fltOn = p.drvOn = p.panOn = true; p.fltMode = (int)(u(r) * 3); p.fltFreq = 20 + u(r) * 20000;
                p.fltRes = u(r); p.drvType = (int)(u(r) * 3); p.drive = u(r) * 36; p.color = u(r); p.pan = u(r) * 2 - 1; p.width = u(r) * 2 - 1; p.mix = u(r); p.outDb = u(r) * 24 - 12;
                e.set(p); if (b % 7 == 0) { Shape s = startShape(b % 8); s.p[0].c = 1; e.pushShape(s); }
                for (int i = 0; i < 4096; ++i) x[(size_t)i] = b % 3 == 0 ? (u(r) - 0.5f) * 20.f : b % 3 == 1 ? 1e-38f : (i == 0 ? 1.f : 0.f);
                const int notes[3] = {0, 100, 4000}; Host h; h.playing = b % 2; h.hasPos = true; h.hasBpm = true; h.bpm = 20 + u(r) * 300; h.ppq = u(r) * 1000;
                e.beginBlock(h, notes, 3); e.process(x.data(), b % 4 ? x.data() : nullptr, l.data(), rr.data(), 4096);
                for (int i = 0; i < 4096; ++i) ok &= std::isfinite(l[(size_t)i]) && std::isfinite(rr[(size_t)i]) && std::fabs(l[(size_t)i]) < 1e4f;
            }
        }
        std::printf("NaN / Inf / emballement : %s\n", ok ? "aucun" : "PRÉSENTS"); CHECK(ok, "NaN");
    }
    { // 9. zéro allocation en temps réel : set + beginBlock + process + nouvelle forme
        Engine e; e.prepare(sr, 512); Params p; p.fltOn = p.drvOn = p.panOn = true; std::vector<float> x(48000), l(48000), r(48000); for (size_t i = 0; i < x.size(); ++i) x[i] = 0.3f * std::sin(0.05f * (float)i);
        const Shape s1 = startShape(1), s2 = startShape(6);
        g_allocs = 0; g_count = true;
        for (int s = 0; s + 480 <= 48000; s += 480) { p.interp = (s / 480) % 3; p.fltMode = (s / 960) % 3; e.set(p); if (s % 4800 == 0) e.pushShape((s / 4800) % 2 ? s1 : s2); const int nt = 10; e.beginBlock({}, &nt, 1); e.process(x.data() + s, x.data() + s, l.data() + s, r.data() + s, 480); }
        g_count = false;
        std::printf("allocations dans le chemin temps réel : %ld\n", g_allocs); CHECK(g_allocs == 0, "%ld allocation(s)", g_allocs);
    }
    { // 10. retrig par note MIDI : la phase repart à 0 à l'échantillon de la note
        Engine e; e.prepare(sr, 512); Params p; p.retrig = 1; p.sync = false; p.hz = 0.5f; p.smoothMs = 1; e.set(p); e.pushShape(startShape(3));   // rampe montante
        std::vector<float> dc(9600, 0.5f), l(9600), r(9600);
        for (int b = 0; b < 15; ++b) { const int nt = 200; e.beginBlock({}, b == 10 ? &nt : nullptr, b == 10 ? 1 : 0); e.process(dc.data() + b * 640, dc.data() + b * 640, l.data() + b * 640, r.data() + b * 640, 640); }
        const float before = e.phaseNow(), want = (float)(3000 * 0.5 / sr);   // 440 + 4×640 échantillons depuis la note
        std::printf("retrig note : phase %.5f, 3000 échantillons après la note (attendu %.5f)\n", before, want);
        CHECK(std::fabs(before - want) < 1e-4, "retrig MIDI");
    }
    { // 11. état : texte de forme aller-retour et tolérance
        Shape s = startShape(7), t; const bool ok = fromString(toString(s), t);
        bool same = ok && t.n == s.n; for (int i = 0; same && i < s.n; ++i) same = std::fabs(t.p[i].x - s.p[i].x) < 1e-4f && std::fabs(t.p[i].y - s.p[i].y) < 1e-4f && std::fabs(t.p[i].c - s.p[i].c) < 1e-3f;
        Shape g; const bool bad = fromString("G1;3;0.1,abc", g) || fromString("xx", g) || fromString("G1;99;", g);
        std::printf("état de forme : aller-retour %s, textes invalides refusés %s\n", same ? "exact" : "FAUX", bad ? "NON" : "oui");
        CHECK(same && !bad, "sérialisation");
    }
    { // 12. bypass honoré (même Fader que processBlock)
        Engine e; Params p; p.drvOn = true; std::vector<float> sc(8192);
        auto m = bypassfx::measure(sr, L, 512, [&] { e.prepare(sr, 512); e.set(p); e.pushShape(startShape(2)); }, [&](float* l, float* r, int n) { e.beginBlock({}, nullptr, 0); e.process(l, r, l, r ? r : sc.data(), n); });
        std::printf("bypass : erreur %.1f dB, saut au basculement %.1f dBFS (signal seul %.1f)\n", m.exactDb, m.jumpDb, m.plainDb);
        CHECK(m.exactDb < -120.0, "bypass non exact"); CHECK(m.jumpDb < -60.0, "clic au basculement");
    }
    { // 13. NaN / infini en entree : l'entree est nettoyee comme dans processBlock (kit::sanitize), la sortie redevient finie en moins de 100 ms
        Engine e; e.prepare(sr, 512); Params p; p.drvOn = true; p.fltOn = true; e.set(p); e.pushShape(startShape(2));
        const int blk = 480, nb = 40; const float nan = std::nanf(""), inf = INFINITY;
        std::vector<float> x(blk), l(blk), r(blk); double eAfter = 0; int lastBad = -1; bool finiteEnd = true;
        for (int b = 0; b < nb; ++b) {
            for (int i = 0; i < blk; ++i) x[(size_t)i] = 0.3f * std::sin(0.05f * (float)(b * blk + i));
            if (b == 2) { x[10] = nan; x[11] = inf; x[12] = -inf; x[200] = nan; }
            kit::sanitize(x.data(), blk);
            e.beginBlock({}, nullptr, 0); e.process(x.data(), x.data(), l.data(), r.data(), blk);
            for (int i = 0; i < blk; ++i) {
                if (!std::isfinite(l[(size_t)i]) || !std::isfinite(r[(size_t)i])) lastBad = b * blk + i;
                if (b >= nb - 10) { eAfter += (double)l[(size_t)i] * l[(size_t)i]; if (!std::isfinite(l[(size_t)i])) finiteEnd = false; }
            }
        }
        const double msBad = lastBad < 0 ? 0.0 : (lastBad - 2 * blk) * 1000.0 / sr;
        std::printf("NaN/inf en entree : derniere sortie non finie a +%.1f ms apres le debut du bloc fautif, energie finale %.3f\n", msBad, eAfter);
        CHECK(lastBad < 0 || msBad < 100.0, "sortie non finie plus de 100 ms"); CHECK(finiteEnd && eAfter > 0.0, "NaN permanent ou silence");
    }
    (void)binPow;
    std::printf(fails ? "%d ECHEC(S)\n" : "TOUT EST BON\n", fails);
    return fails ? 1 : 0;
}
