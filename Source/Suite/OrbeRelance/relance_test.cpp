// Test console du moteur Orbe Relance (sans JUCE). Cible : orbe_relance_test (EXCLUDE_FROM_ALL).
// Finitude (NaN/inf, silence, plein niveau), niveaux, déclenchement en fin de phrase, latence (sec intact), invariance à la taille
// de bloc (1 à 8192), 44,1 / 48 / 96 kHz, graine verrouillée, aucune allocation dans process, état « REL1 ».
#include <cstdio>
#include <cstdlib>
#include <new>
#include <random>
#include "RelanceDsp.h"

static long g_allocs = 0; static bool g_count = false, g_arm = false;   // allocateur instrumenté
void* operator new(std::size_t n) { if (g_count) ++g_allocs; if (void* p = std::malloc(n ? n : 1)) return p; throw std::bad_alloc(); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { ++fails; std::printf("ECHEC: " __VA_ARGS__); std::printf("\n"); } } while (0)
using namespace rel;

struct Out { std::vector<float> l, r; };
static Out run(Engine& e, const std::vector<float>& x, int blk, double sr, bool playing = true, double bpm = 120.0) {
    Out o; o.l.assign(x.size(), 0.f); o.r.assign(x.size(), 0.f);
    g_count = g_arm;
    for (size_t p = 0; p < x.size(); p += (size_t)blk) {
        const int c = (int)std::min<size_t>((size_t)blk, x.size() - p);
        Host h; h.playing = playing; h.hasPos = h.hasBpm = true; h.bpm = bpm; h.ppq = (double)p * bpm / 60.0 / sr;
        e.beginBlock(h);
        e.process(x.data() + p, nullptr, o.l.data() + p, o.r.data() + p, c);
    }
    g_count = false;
    return o;
}
// « voix » rap synthétique : phrases voisées (harmoniques, 140 Hz légèrement modulé, syllabes de 120 ms) séparées de silences
static std::vector<float> voice(double sr, double sec, double phraseS = 0.9, double gapS = 0.5, float amp = 0.3f) {
    std::vector<float> v((size_t)(sr * sec), 0.f);
    double ph = 0.0;
    for (size_t i = 0; i < v.size(); ++i) {
        const double t = (double)i / sr, tp = std::fmod(t, phraseS + gapS);
        if (tp >= phraseS) continue;
        const double f0 = 140.0 * (1.0 + 0.04 * std::sin(2.0 * 3.14159265 * 1.3 * t));
        ph += 2.0 * 3.14159265 * f0 / sr;
        double s = 0.0; for (int k = 1; k <= 8; ++k) s += std::sin(k * ph) / k;
        const double syl = 0.55 + 0.45 * std::sin(3.14159265 * std::fmod(tp, 0.12) / 0.12);
        const double edge = std::min(1.0, std::min(tp / 0.01, (phraseS - tp) / 0.01));
        v[i] = (float)(amp * s * syl * edge);
    }
    return v;
}
static float peak(const std::vector<float>& v, size_t a = 0, size_t b = (size_t)-1) { float m = 0.f; for (size_t i = a; i < std::min(b, v.size()); ++i) m = std::max(m, std::fabs(v[i])); return m; }
static bool finite(const Out& o) { for (size_t i = 0; i < o.l.size(); ++i) if (!std::isfinite(o.l[i]) || !std::isfinite(o.r[i])) return false; return true; }

int main() {
    for (double sr : {44100.0, 48000.0, 96000.0}) {
        const auto x = voice(sr, 6.0);
        // 1) déclenchement : densité 1, chaque fin de phrase -> une relance dans le silence suivant
        for (int ch = 0; ch <= kNumTypes; ++ch) {
            Engine e; Params p; p.density = 1.f; p.character = ch; p.outMode = 1; p.levelDb = 0.f; e.set(p); e.prepare(sr, 512);
            g_allocs = 0; g_arm = true;
            const auto o = run(e, x, 512, sr);
            g_arm = false;
            CHECK(g_allocs == 0, "%.0f Hz car %d : %ld allocations dans process", sr, ch, g_allocs);
            CHECK(finite(o), "%.0f Hz car %d : sortie non finie", sr, ch);
            CHECK(e.phrasesSeen() == 4, "%.0f Hz car %d : %d fins de phrase (attendu 4)", sr, ch, e.phrasesSeen());
            CHECK(e.relancesFired() == e.phrasesSeen(), "%.0f Hz car %d : %d relances pour %d phrases", sr, ch, e.relancesFired(), e.phrasesSeen());
            // relances seules : rien avant la première fin de phrase (0,9 s + 150 ms), du son ensuite, niveau raisonnable
            const float early = peak(o.l, 0, (size_t)(0.9 * sr)), late = std::max(peak(o.l), peak(o.r));
            CHECK(early < 1e-6f, "%.0f Hz car %d : relance avant la fin de phrase (%g)", sr, ch, early);
            CHECK(late > 0.03f && late < 1.2f, "%.0f Hz car %d : niveau de relance %g hors plage", sr, ch, late);
            if (sr == 48000.0) std::printf("  car %d : %d relances, crete %.2f\n", ch, e.relancesFired(), late);
        }
        // 2) voie sèche intacte (latence 0) : densité 0 -> sortie = entrée, bit à bit
        {
            Engine e; Params p; p.density = 0.f; e.set(p); e.prepare(sr, 256);
            const auto o = run(e, x, 256, sr);
            float d = 0.f; for (size_t i = 0; i < x.size(); ++i) d = std::max(d, std::max(std::fabs(o.l[i] - x[i]), std::fabs(o.r[i] - x[i])));
            CHECK(e.latency() == 0 && d == 0.f, "%.0f Hz : voie seche modifiee (ecart %g)", sr, d);
            CHECK(e.phrasesSeen() == 4 && e.relancesFired() == 0, "%.0f Hz : densite 0 (%d/%d)", sr, e.relancesFired(), e.phrasesSeen());
        }
        // 3) invariance à la taille de bloc (1 à 8192)
        {
            Params p; p.density = 0.8f; p.audace = 0.7f; p.delayMode = 2;
            Engine e0; e0.set(p); e0.prepare(sr, 8192); const auto ref = run(e0, x, 512, sr);
            for (int blk : {1, 7, 64, 333, 2048, 8192}) {
                Engine e; e.set(p); e.prepare(sr, 8192); const auto o = run(e, x, blk, sr);
                float d = 0.f; for (size_t i = 0; i < x.size(); ++i) d = std::max(d, std::max(std::fabs(o.l[i] - ref.l[i]), std::fabs(o.r[i] - ref.r[i])));
                CHECK(d == 0.f, "%.0f Hz : bloc %d different (ecart %g)", sr, blk, d);
            }
        }
    }
    const double sr = 48000.0;
    const auto x = voice(sr, 8.0, 0.7, 0.45);
    // 4) graine verrouillée : deux lectures -> mêmes relances ; graine différente -> autres choix
    {
        Params p; p.density = 0.6f; p.lock = 1; p.seed = 7;
        Engine a; a.set(p); a.prepare(sr, 512); const auto oa = run(a, x, 512, sr);
        Engine b; b.set(p); b.prepare(sr, 512); run(b, std::vector<float>(48000, 0.f), 512, sr, false); const auto ob = run(b, x, 512, sr);
        float d = 0.f; for (size_t i = 0; i < x.size(); ++i) d = std::max(d, std::fabs(oa.l[i] - ob.l[i]));
        CHECK(d == 0.f, "graine verrouillee : relances differentes apres relance de la lecture (%g)", d);
        p.seed = 8; Engine c; c.set(p); c.prepare(sr, 512); const auto oc = run(c, x, 512, sr);
        float dc = 0.f; for (size_t i = 0; i < x.size(); ++i) dc = std::max(dc, std::fabs(oa.l[i] - oc.l[i]));
        CHECK(dc > 0.f, "graine : 7 et 8 donnent la meme sortie");
        std::printf("  graine 7 : %d/%d relances ; graine 8 : %d/%d\n", a.relancesFired(), a.phrasesSeen(), c.relancesFired(), c.phrasesSeen());
    }
    // 5) débit dense sans silence : aucune fausse fin de phrase ; voisement détecté
    {
        Engine e; Params p; p.density = 1.f; e.set(p); e.prepare(sr, 512);
        const auto o = run(e, voice(sr, 4.0, 10.0, 0.1), 512, sr);
        CHECK(e.phrasesSeen() == 0, "debit continu : %d fausses fins de phrase", e.phrasesSeen());
        CHECK(e.voicedState() && std::fabs(e.period() - sr / 140.0) < sr / 140.0 * 0.08, "YIN : periode %g (attendu ~%g)", e.period(), sr / 140.0);
        (void)o;
    }
    // 6) finitude : NaN/inf en entrée, plein niveau, silence ; paramètres extrêmes
    {
        Engine e; Params p; p.density = 1.f; p.audace = 1.f; p.levelDb = 6.f; p.tone = 1.f; p.lenMs = 600.f; p.threshDb = -60.f; e.set(p); e.prepare(sr, 512);
        auto y = voice(sr, 5.0, 0.6, 0.3, 1.0f);
        y[1000] = std::nanf(""); y[30000] = INFINITY; y[60000] = -INFINITY;
        const auto o = run(e, y, 512, sr);
        CHECK(finite(o), "NaN/inf en entree : sortie non finie");
        CHECK(peak(o.l) <= peak(y) + 2.001f && peak(o.r) <= peak(y) + 2.001f, "plein niveau : crete %g", std::max(peak(o.l), peak(o.r)));
        Engine s; s.set(Params{}); s.prepare(sr, 512);
        const auto os = run(s, std::vector<float>(96000, 0.f), 512, sr);
        CHECK(finite(os) && peak(os.l) == 0.f, "silence : sortie non nulle");
        Params bad; bad.density = NAN; bad.lenMs = 1e9f; bad.levelDb = -INFINITY; bad.character = 99; sanitize(bad);
        CHECK(bad.density == Params{}.density && bad.lenMs == 600.f && bad.levelDb == Params{}.levelDb && bad.character == kNumTypes, "sanitize des parametres");
    }
    // 7) état versionné
    {
        Meta m; m.program = 11; Meta r;
        CHECK(fromString(toString(m), r) && r.program == 11 && toString(m) == "REL1;prog=11", "etat REL1");
        CHECK(!fromString("XYZ;prog=3", r) && fromString("REL1;prog=99", r) && r.program == 15, "etat REL1 tolerant");
    }
    std::printf(fails ? "orbe_relance_test : %d ECHEC(S)\n" : "orbe_relance_test : OK\n", fails);
    return fails ? 1 : 0;
}
