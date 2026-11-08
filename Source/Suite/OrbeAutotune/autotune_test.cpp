// Test console du moteur Orbe Autotune (sans JUCE). Cible : orbeautotune_test (EXCLUDE_FROM_ALL).
// Usage : orbeautotune_test [prise.wav]
#include <cstdio>
#include <cstdint>
#include <random>
#include "../../Kit/BypassFadeTest.h"
#include "AutotuneDsp.h"

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { ++fails; std::printf("ECHEC: " __VA_ARGS__); std::printf("\n"); } } while (0)
using V = std::vector<float>;
constexpr float SR = 44100.f;

static V voice(const std::vector<std::pair<float, float>>& notes /* midi, secondes */, float detCents = 0.f, float sr = SR) {
    V x; double ph = 0;
    for (auto& nt : notes) {
        const double f = 440.0 * std::pow(2.0, (nt.first - 69.0 + detCents / 100.0) / 12.0);
        for (int i = 0, n = (int)(nt.second * sr); i < n; ++i) {
            ph += 2 * 3.14159265358979 * f / sr; float v = 0;
            for (int k = 1; k <= 8; ++k) v += std::sin((float)(k * ph)) / (float)k;
            const float env = std::min(1.f, std::min((float)i, (float)(n - i)) / (0.02f * sr));
            x.push_back(0.3f * v * env);
        }
    }
    return x;
}
static void run(atn::Engine& e, const V& in, V& l, V& r, bool stereoSame = true) {
    l = in; r = stereoSame ? in : V(in.size(), 0.f);
    std::mt19937 rng(5);
    for (size_t p = 0; p < in.size();) {
        const size_t c = std::min<size_t>(in.size() - p, 1 + rng() % 700);
        e.process(l.data() + p, r.data() + p, (int)c); p += c;
    }
}
// Hauteur (MIDI, 0 = non voisé) toutes les 256 échantillons ; `delay` aligne sur l'entrée.
static V track(const V& x, int delay = 0) {
    atn::Tracker t; t.init(SR); V o;
    for (size_t i = 0; i < x.size() + (size_t)delay; ++i) {
        t.det.push(i < (size_t)delay ? 0.f : x[i - (size_t)delay]);
        if ((i + 1) % 256 == 0) o.push_back(t.step() ? t.midi : 0.f);
    }
    return o;
}
static float median(V v) { if (v.empty()) return 0; std::sort(v.begin(), v.end()); return v[v.size() / 2]; }
// Écart de hauteur en cents sur [t0,t1] s entre sortie et référence MIDI fixe.
static float centsTo(const V& tr, float ref, float t0, float t1) {
    V d; for (size_t h = (size_t)(t0 * SR / 256); h < std::min(tr.size(), (size_t)(t1 * SR / 256)); ++h) if (tr[h] > 0) d.push_back((tr[h] - ref) * 100.f);
    return d.empty() ? 9999.f : median(d);
}
static bool gLow = false;   // la batterie de tests tourne deux fois : mode Qualité puis mode Direct
static atn::Params base() { atn::Params p; p.amt = 1; p.tol = 0; p.speed = 0; p.dbl = 0; p.harm = 0; p.mix = 1; p.form = 0; p.vd = 0; p.trans = 0; p.lowLat = gLow; return p; }
struct Wav { int n = 0; V x; };
static bool readWav(const char* path, Wav& w) {
    FILE* f = std::fopen(path, "rb"); if (!f) return false;
    std::vector<uint8_t> b; uint8_t t[65536]; size_t r; while ((r = std::fread(t, 1, sizeof t, f)) > 0) b.insert(b.end(), t, t + r);
    std::fclose(f);
    size_t pos = 12; int nch = 2; uint16_t bits = 16;
    while (pos + 8 <= b.size()) {
        uint32_t sz; std::memcpy(&sz, &b[pos + 4], 4);
        if (!std::memcmp(&b[pos], "fmt ", 4)) { uint16_t c; std::memcpy(&c, &b[pos + 10], 2); nch = c; std::memcpy(&bits, &b[pos + 22], 2); }
        else if (!std::memcmp(&b[pos], "data", 4) && bits == 16) {
            const size_t n = std::min<size_t>(sz, b.size() - pos - 8) / (2 * (size_t)nch);
            for (size_t i = 0; i < n; ++i) { int16_t s; std::memcpy(&s, &b[pos + 8 + i * 2 * (size_t)nch], 2); w.x.push_back(s / 32768.f); }
            w.n = (int)n; return true;
        }
        pos += 8 + sz + (sz & 1);
    }
    return false;
}

static void direct_tests();
static void suite(int argc, char** argv) {
    std::printf("=== mode %s ===\n", gLow ? "Direct" : "Qualite");
    // --- 1. latence mesurée = déclarée, bypass transparent
    {
        atn::Engine e; e.prepare(SR); auto p = base(); p.amt = 0; p.key = 0; e.set(p);
        V in(20000, 0.f); in[3000] = 1.f; V l, r; run(e, in, l, r);
        int pk = 0; for (size_t i = 0; i < l.size(); ++i) if (std::fabs(l[i]) > std::fabs(l[(size_t)pk])) pk = (int)i;
        std::printf("latence declaree %d, mesuree %d (%.1f ms)\n", e.latency(), pk - 3000, 1000.f * e.latency() / SR);
        CHECK(pk - 3000 == e.latency(), "latence mesuree %d != declaree %d", pk - 3000, e.latency());
        CHECK(std::fabs(l[(size_t)pk] - 1.f) < 0.02f, "impulsion deformee : %f", l[(size_t)pk]);
        atn::BypassDelay bd; bd.init(e.latency()); V a = in, b2 = in; bd.process(a.data(), b2.data(), (int)a.size(), true);
        CHECK(std::fabs(a[(size_t)(3000 + e.latency())] - 1.f) < 1e-6f && std::fabs(a[3000]) < 1e-6f, "bypass non transparent");
    }
    // --- 2. sinus +30 cents ramené sur la note (A4), Vitesse 0 et 12 ms
    for (float sp : {0.f, 12.f}) {
        atn::Engine e; e.prepare(SR); auto p = base(); p.key = 9; p.scale = 1; p.speed = sp; e.set(p);
        V l, r; run(e, voice({{69.f, 2.5f}}, 30.f), l, r);
        const float c = centsTo(track(l), 69.f, 1.2f, 2.4f);
        std::printf("sinus +30 ct, vitesse %.0f ms -> %+.1f ct de la note\n", sp, c);
        CHECK(std::fabs(c) <= 8.f, "sinus +30 non corrige (vitesse %.0f) : %+.1f", sp, c);
    }
    // --- 3. mauvaise tonalité manuelle : la sortie se range sur la grille (F# majeur) quelle que soit la mélodie (C majeur)
    {
        atn::Engine e; e.prepare(SR); auto p = base(); p.key = 6; p.scale = 0; p.speed = 5; e.set(p);
        const float cm[7] = {60, 62, 64, 65, 67, 69, 71};
        std::vector<std::pair<float, float>> mel; for (float n : cm) mel.push_back({n, 0.5f});
        V l, r; run(e, voice(mel, 18.f), l, r);
        const auto tr = track(l, 0); const int mask = atn::scaleMask(6, 0); float worst = 0.f;
        for (int i = 0; i < 7; ++i) {
            const float t0 = 0.5f * (float)i + 0.3f + (float)e.latency() / SR, t1 = t0 + 0.12f;
            V d; for (size_t h = (size_t)(t0 * SR / 256); h < (size_t)(t1 * SR / 256) && h < tr.size(); ++h) if (tr[h] > 0) d.push_back(tr[h]);
            const float m = median(d); float best = 1e9f;
            for (int n = 40; n < 90; ++n) if ((mask >> atn::pcOf(n)) & 1) best = std::min(best, std::fabs(m - (float)n) * 100.f);
            worst = std::max(worst, best);
        }
        std::printf("tonalite manuelle F# maj, melodie C maj : ecart max a la grille %.1f ct\n", worst);
        CHECK(worst <= 10.f, "sortie hors grille : %.1f ct", worst);
    }
    // --- 4. Auto non fiable (note unique, glissando) : hauteur inchangée
    {
        atn::Engine e; e.prepare(SR); auto p = base(); p.key = -1; p.scale = 1; e.set(p);
        V l, r; run(e, voice({{57.f, 4.f}}, 30.f), l, r);
        const float c = centsTo(track(l), 57.3f, 2.f, 3.9f);
        std::printf("Auto, note unique +30 ct : sortie %+.1f ct de l'entree, mode=%d\n", c, e.tel.mode.load());
        CHECK(std::fabs(c) <= 3.f, "Auto non fiable a deplace la hauteur : %+.1f", c);
        CHECK(e.tel.mode.load() == 2, "Auto aurait du rester en recherche");
        atn::Engine g; g.prepare(SR); g.set(p);
        std::vector<std::pair<float, float>> gl; for (int i = 0; i < 48; ++i) gl.push_back({50.f + 0.37f * (float)i, 0.2f});
        const V in = voice(gl, 0.f); run(g, in, l, r);
        std::printf("Auto, glissando chromatique : mode=%d\n", g.tel.mode.load());
        CHECK(g.tel.mode.load() == 2, "glissando : verrouillage abusif");
        const auto ti = track(in, g.latency()), to = track(l); V d;
        for (size_t h = 0; h < std::min(ti.size(), to.size()); ++h) if (ti[h] > 0 && to[h] > 0) d.push_back(std::fabs(ti[h] - to[h]) * 100.f);
        CHECK(median(d) <= 6.f, "glissando deplace : %.1f ct", median(d));
    }
    // --- 5. Auto fiable : mélodie en A mineur légèrement fausse -> verrouillage sur A, sortie sur la grille
    {
        atn::Engine e; e.prepare(SR); auto p = base(); p.key = -1; p.scale = 1; p.speed = 10; e.set(p);
        const float am[8] = {69, 72, 76, 74, 71, 67, 72, 69};
        std::vector<std::pair<float, float>> mel; std::mt19937 rg(2);
        for (int rep = 0; rep < 9; ++rep) for (float n : am) mel.push_back({n + ((float)(rg() % 5) - 2.f) * 0.1f, 0.35f});
        mel.push_back({76.f, 1.f});
        V in = voice(mel, 0.f), l, r; run(e, in, l, r);
        std::printf("Auto, melodie A min : mode=%d racine=%d\n", e.tel.mode.load(), e.tel.root.load());
        CHECK(e.tel.mode.load() == 1 && e.tel.root.load() == 9, "verrouillage attendu sur A (9), obtenu mode %d racine %d", e.tel.mode.load(), e.tel.root.load());
    }
    // --- 6. transposition, formants, harmonie : hauteur attendue, pas de NaN, pic borné
    {
        atn::Engine e; e.prepare(SR); auto p = base(); p.key = 0; p.scale = 8; p.amt = 0; p.trans = 5; e.set(p);
        V l, r; run(e, voice({{57.f, 2.f}}), l, r);
        const float c = centsTo(track(l), 62.f, 1.f, 1.9f);
        std::printf("Transposer +5 dt : %+.1f ct\n", c);
        CHECK(std::fabs(c) <= 6.f, "transposition : %+.1f", c);
        float pk = 0; bool ok = true;
        for (int pr = 0; pr < atn::kPresets; ++pr) {
            const auto& q = atn::presets()[pr]; atn::Engine g; g.prepare(SR);
            atn::Params pp; pp.speed = q.speed; pp.tol = q.tol; pp.amt = q.amt; pp.trans = q.trans; pp.form = q.form; pp.vd = q.vd; pp.vr = q.vr; pp.harm = q.harm; pp.hv = q.hv; pp.dbl = q.dbl; pp.key = 9; pp.scale = 1; pp.lowLat = gLow; g.set(pp);
            V a, b; run(g, voice({{57.f, 1.f}, {60.f, 1.f}, {64.f, 1.f}}, 25.f), a, b, false);
            float pp2 = 0; for (size_t i = 0; i < a.size(); ++i) { ok &= std::isfinite(a[i]) && std::isfinite(b[i]); pp2 = std::max(pp2, std::max(std::fabs(a[i]), std::fabs(b[i]))); }
            pk = std::max(pk, pp2); std::printf("preset %-13s pic %.2f\n", q.name, pp2);
        }
        CHECK(ok, "NaN/inf"); CHECK(pk < 2.5f, "pic hors borne : %f", pk);
    }
    // --- 7. prise réelle : accordée, Vitesse lente, Quantité faible -> à ≤ 10 cents de l'entrée
    {
        Wav w; const char* path = argc > 1 ? argv[1] : "/tmp/claude-0/-home-claude/12ae9892-af71-5ca8-a97b-93f9fd958b0b/scratchpad/prise.wav";
        if (readWav(path, w)) {
            for (int mode = 0; mode < 3; ++mode) {
                atn::Engine e; e.prepare(SR); auto p = base();
                p.speed = mode == 2 ? 12.f : 400.f; p.amt = mode == 0 ? 0.2f : 1.f; p.tol = mode == 1 ? 50.f : 0.f;
                p.key = mode == 1 ? -1 : 0; p.scale = mode == 1 ? 1 : 8; e.set(p);   // 0/2 : manuel chromatique ; 1 : Auto
                V l, r; run(e, w.x, l, r);
                const auto ti = track(w.x, e.latency()), to = track(l); V d; int onGrid = 0, nv = 0;
                for (size_t h = 0; h < std::min(ti.size(), to.size()); ++h) if (ti[h] > 0 && to[h] > 0) { d.push_back(std::fabs(ti[h] - to[h]) * 100.f); ++nv; onGrid += std::fabs(to[h] - std::round(to[h])) < 0.25f; }
                std::sort(d.begin(), d.end());
                float pk = 0; for (float v : l) pk = std::max(pk, std::fabs(v));
                const char* nm[3] = {"chrom, vit 400, quant .2", "Auto, tol 50", "chrom, vit 12, quant 1"};
                std::printf("prise (%s) : %d trames, ecart median %.1f ct, p90 %.1f ct, a moins de 25 ct de la grille %d%%, mode=%d pic %.2f\n", nm[mode], nv, median(d), d.empty() ? 0 : d[d.size() * 9 / 10], nv ? 100 * onGrid / nv : 0, e.tel.mode.load(), pk);
                if (mode < 2) CHECK(median(d) <= 10.f, "prise deplacee : median %.1f ct", median(d));
                else CHECK(onGrid * 10 >= nv * 7, "prise : seulement %d/%d trames sur la grille", onGrid, nv);
            }
        } else std::printf("prise.wav absente : test saute\n");
    }
    // --- 8. formants préservés : le centroïde spectral suit la hauteur sans la préservation, pas avec (mode Qualité seulement)
    if (!gLow) {
        auto centroid = [](const V& x, size_t a, size_t b) { double sw = 0, s = 0; atn::Fft f; f.init(4096); V re(4096), im(4096, 0.f);
            for (int i = 0; i < 4096; ++i) re[(size_t)i] = x[a + (size_t)i] * (0.5f - 0.5f * std::cos(2 * atn::kPi * (float)i / 4096.f));
            f.run(re.data(), im.data(), false);
            for (int k = 1; k < 1500; ++k) { const double m = std::hypot(re[(size_t)k], im[(size_t)k]); sw += m * k; s += m; } return sw / s * SR / 4096.0; };
        // voix à formants : harmoniques de 150 Hz pondérées par une bosse à 700 Hz
        V in; { double ph = 0; for (int i = 0; i < (int)(3 * SR); ++i) { ph += 2 * 3.14159265358979 * 150.0 / SR; float v = 0; for (int k = 1; k <= 40; ++k) { const float fk = 150.f * (float)k, g = std::exp(-std::pow((fk - 700.f) / 350.f, 2.f)); v += g * std::sin((float)(k * ph)); } in.push_back(0.12f * v); } }
        double cs[3];
        for (int mode = 0; mode < 3; ++mode) {
            atn::Engine e; e.prepare(SR); auto p = base(); p.key = 0; p.scale = 8; p.amt = 0; p.trans = mode == 0 ? 0.f : 5.f; p.formPres = mode != 2; e.set(p);
            V l, r; run(e, in, l, r); cs[mode] = centroid(l, (size_t)(1.5 * SR), 0) ;
        }
        std::printf("centroide : original %.0f Hz, +5 dt formants preserves %.0f Hz, +5 dt sans %.0f Hz\n", cs[0], cs[1], cs[2]);
        CHECK(cs[1] < cs[0] * 1.12 && cs[2] > cs[0] * 1.2, "formants : preservation inefficace");
    }
    { // bypass honoré (même Fader que processBlock) : sortie = entrée retardée de la latence (< -120 dB), saut au basculement < -60 dBFS
        atn::Engine e; auto p = base(); std::vector<float> sc(4096);
        auto m = bypassfx::measure(SR, e.latency(), 512, [&] { e.prepare(SR); e.set(p); }, [&](float* l, float* r, int n) { if (!r) { std::copy(l, l + n, sc.data()); r = sc.data(); } e.process(l, r, n); });
        std::printf("bypass %s : latence %d, erreur %.1f dB, saut au basculement %.1f dBFS (signal seul : %.1f)\n", "OrbeAutotune", e.latency(), m.exactDb, m.jumpDb, m.plainDb);
        CHECK(m.exactDb < -120.0, "bypass non exact : %.1f dB", m.exactDb); CHECK(m.jumpDb < -60.0, "clic au basculement : %.1f dBFS", m.jumpDb);
    }
}

int main(int argc, char** argv) {
    gLow = false; suite(argc, argv);
    gLow = true; suite(argc, argv);
    direct_tests();
    std::printf(fails ? "%d ECHEC(S)\n" : "tout est bon\n", fails);
    return fails ? 1 : 0;
}

// ---------------------------------------------------------------- chantier 13 : mode Direct (basse latence)
static double fftSpec(const V& y, size_t a, double sr, int f0bin, int nh, double guardHz, double* tot) {
    const size_t N = 65536; std::vector<double> re(N), im(N, 0.0);
    for (size_t i = 0; i < N; ++i) { const double w = 0.35875 - 0.48829 * std::cos(2 * M_PI * i / (N - 1)) + 0.14128 * std::cos(4 * M_PI * i / (N - 1)) - 0.01168 * std::cos(6 * M_PI * i / (N - 1)); re[i] = w * y[a + i]; }
    // FFT récursive-itérative simple (double)
    for (size_t i = 1, j = 0; i < N; ++i) { size_t bit = N >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; if (i < j) { std::swap(re[i], re[j]); std::swap(im[i], im[j]); } }
    for (size_t len = 2; len <= N; len <<= 1) {
        const double ang = -2 * M_PI / (double)len;
        for (size_t i = 0; i < N; i += len) for (size_t k = 0; k < len / 2; ++k) {
            const double wr = std::cos(ang * (double)k), wi = std::sin(ang * (double)k);
            const size_t a0 = i + k, b0 = a0 + len / 2;
            const double tr = re[b0] * wr - im[b0] * wi, ti = re[b0] * wi + im[b0] * wr;
            re[b0] = re[a0] - tr; im[b0] = im[a0] - ti; re[a0] += tr; im[a0] += ti;
        }
    }
    (void)f0bin;
    double all = 0, sig = 0; const double bw = sr / (double)N, f0 = (double)f0bin;
    for (size_t k = 1; k < N / 2; ++k) {
        const double f = (double)k * bw, p = re[k] * re[k] + im[k] * im[k]; all += p;
        const int h = (int)std::lround(f / f0);
        if (h >= 1 && h <= nh && std::fabs(f - h * f0) < guardHz) sig += p;
    }
    *tot = all; return 10 * std::log10((all - sig) / all + 1e-30);
}
static void direct_tests() {
    std::printf("=== chantier 13 : mode Direct ===\n");
    // latence annoncée <= 256 échantillons à toutes les fréquences d'échantillonnage ; Qualité inchangée (plus de 1000)
    for (double sr : {44100.0, 48000.0, 88200.0, 96000.0}) {
        atn::Engine e; e.prepare(sr); auto p = base(); p.lowLat = true; e.set(p);
        std::printf("sr %.0f : latence Direct %d (%.2f ms), Qualite %d\n", sr, e.latency(), 1000.0 * e.latency() / sr, e.latencyQuality());
        CHECK(e.latency() <= 256 && e.latency() == e.latencyDirect(), "latence Direct %d > 256", e.latency());
        CHECK(e.latencyQuality() > 1000, "latence Qualite %d", e.latencyQuality());
        p.lowLat = false; e.set(p); CHECK(e.latency() == e.latencyQuality(), "retour en Qualite");
    }
    // THD+N d'une voix corrigée (+30 ct ramené sur la note) : le mode Direct fait au moins aussi bien que l'ancien décaleur à deux têtes
    // (valeurs de l'ancien moteur, mesurées avant ce chantier : sinus -73,3 dB, voix 110 Hz -60,6 dB, 196 Hz -66,2 dB, 330 Hz -58,2 dB ; Qualité à 48 kHz)
    const double sr = 48000.0;
    struct Cs { const char* name; double f0; int nh; double oldDb; };
    const Cs cs[4] = {{"sinus 440 Hz", 440.0, 1, -73.3}, {"voix 110 Hz", 110.0, 8, -60.6}, {"voix 196 Hz", 196.0, 8, -66.2}, {"voix 330 Hz", 329.63, 8, -58.2}};
    for (int mode = 0; mode < 2; ++mode) for (const auto& c : cs) {
        atn::Engine e; e.prepare(sr); auto p = base(); p.key = 9; p.scale = 8; p.lowLat = mode == 1; e.set(p);
        V in; { double ph = 0; const double f = c.f0 * std::pow(2.0, 30.0 / 1200.0); const size_t n = (size_t)(4.0 * sr);
                for (size_t i = 0; i < n; ++i) { ph += 2 * M_PI * f / sr; float v = 0; for (int k = 1; k <= c.nh; ++k) v += (float)(std::sin((double)k * ph) / (c.nh > 1 ? (double)k : 1.0));
                    in.push_back(0.3f * v * std::min(1.f, std::min((float)i, (float)(n - i)) / (0.02f * (float)sr))); } }
        V l, r; run(e, in, l, r); double tot; const double db = fftSpec(l, (size_t)(2.0 * sr), sr, (int)std::lround(c.f0), c.nh, 30.0, &tot);
        std::printf("THD+N %s, %s : %.1f dB (avant : %.1f dB), latence %d\n", mode ? "Direct " : "Qualite", c.name, db, c.oldDb, e.latency());
        CHECK(db <= c.oldDb + 0.5, "THD+N %s %s : %.1f dB, pire que l'ancien moteur (%.1f dB)", mode ? "Direct" : "Qualite", c.name, db, c.oldDb);
    }
    // pas de saut en changeant de mode en cours de route ; sortie bornée
    { atn::Engine e; e.prepare(sr); auto p = base(); p.key = 9; p.scale = 1; e.set(p); V in = voice({{57.f, 1.f}, {60.f, 1.f}, {64.f, 1.f}}, 25.f, (float)sr), l = in, r = in; float pk = 0; bool ok = true;
      for (size_t q = 0; q < in.size(); q += 480) { p.lowLat = ((q / 480) / 20) % 2 == 1; e.set(p); const int c = (int)std::min<size_t>(480, in.size() - q); e.process(l.data() + q, r.data() + q, c); }
      for (size_t i = 0; i < l.size(); ++i) { ok &= std::isfinite(l[i]); pk = std::max(pk, std::fabs(l[i])); }
      CHECK(ok && pk < 2.5f, "changement de mode : pic %f", pk); }
}
