// Test console du moteur Orbe Accroc (sans JUCE). Cible : orbeaccroc_test (EXCLUDE_FROM_ALL).
#include <cstdio>
#include <cstdlib>
#include <new>
#include <random>
#include "AccrocDsp.h"

static long g_allocs = 0; static bool g_count = false;   // allocateur instrumenté
void* operator new(std::size_t n) { if (g_count) ++g_allocs; if (void* p = std::malloc(n ? n : 1)) return p; throw std::bad_alloc(); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { ++fails; std::printf("ECHEC: " __VA_ARGS__); std::printf("\n"); } } while (0)
using namespace acr;

struct Out { std::vector<float> l, r; };
// Traite x (stéréo l/r ; r vide = entrée mono) par blocs de `blk`, hôte en lecture à `bpm` depuis la PPQ 0.
static Out run(Engine& e, const std::vector<float>& xl, const std::vector<float>& xr, int blk, double sr, double bpm = 120.0,
               const std::vector<Ev>& midi = {}, bool playing = true) {
    Out o; o.l.assign(xl.size(), 0.f); o.r.assign(xl.size(), 0.f);
    std::vector<Ev> local; local.reserve(64);
    for (size_t p = 0; p < xl.size(); p += (size_t)blk) {
        const int c = (int)std::min<size_t>((size_t)blk, xl.size() - p);
        Host h; h.playing = playing; h.hasPos = h.hasBpm = true; h.bpm = bpm; h.ppq = (double)p * bpm / 60.0 / sr;
        local.clear();
        for (const auto& m : midi) if (m.offset >= (int)p && m.offset < (int)p + c) local.push_back({m.offset - (int)p, m.note, m.vel});
        e.beginBlock(h, local.data(), (int)local.size());
        e.process(xl.data() + p, xr.empty() ? nullptr : xr.data() + p, o.l.data() + p, o.r.data() + p, c);
    }
    return o;
}
static double db(double v) { return 20 * std::log10(v + 1e-30); }
static std::vector<float> noise(size_t n, unsigned seed, float amp = 0.5f) {
    std::mt19937 r(seed); std::uniform_real_distribution<float> u(-amp, amp); std::vector<float> v(n); for (auto& s : v) s = u(r); return v;
}
static std::vector<float> sine(size_t n, double f, double sr, float amp = 0.5f) {
    std::vector<float> v(n); for (size_t i = 0; i < n; ++i) v[i] = amp * (float)std::sin(2 * kPi * f * (double)i / sr); return v;
}
static Seq all(int type, float inten, int len = 1, float prob = 1.f) {
    Seq q; for (auto& s : q.s) s = {type, prob, inten, len}; return q;
}
static double maxJump(const Out& o, size_t from) {
    double m = 0; for (size_t i = from + 1; i < o.l.size(); ++i) m = std::max(m, (double)std::max(std::fabs(o.l[i] - o.l[i - 1]), std::fabs(o.r[i] - o.r[i - 1])));
    return m;
}

int main() {
    const double rates[3] = {44100.0, 48000.0, 96000.0};
    std::printf("Orbe Accroc : test du moteur\n");

    for (double sr : rates) {
        const size_t N = (size_t)(sr * 4);
        // 1. latence déclarée = mesurée (impulsion, séquence vide)
        { Engine e; Params p; e.set(p); e.prepare(sr, 512); e.pushSeq(Seq{});
          std::vector<float> x(4096, 0.f); x[100] = 0.5f;
          auto o = run(e, x, {}, 256, sr);
          size_t pk = 0; for (size_t i = 0; i < x.size(); ++i) if (std::fabs(o.l[i]) > std::fabs(o.l[pk])) pk = i;
          std::printf("[%5.1f kHz] latence déclarée %d, mesurée %d (%.2f ms)\n", sr / 1000, e.latency(), (int)pk - 100, e.latency() * 1000.0 / sr);
          CHECK((int)pk - 100 == e.latency() && std::fabs(o.l[pk] - 0.5f) < 1e-6f, "latence mesurée %d != %d", (int)pk - 100, e.latency()); }

        // 2. intensité 0 : signal neutre (retardé de la latence), pour chaque type d'accident, chaos 0
        double worst0 = 0;
        auto xl = noise(N, 3), xr = noise(N, 4);
        for (int ty = 0; ty < kNumTypes; ++ty) {
            Engine e; Params p; p.stuPitch = 12.f; p.stuDiv = 4; e.set(p); e.prepare(sr, 512); e.pushSeq(all(ty, 0.f, 3));
            auto o = run(e, xl, xr, 480, sr);
            std::vector<Ev> pad{{1000, 60 + ty, 0.f}};   // une note de vélocité nulle = relâchement : sans effet
            const int L = e.latency(); double err = 0;
            for (size_t i = (size_t)L; i < N; ++i) err = std::max(err, (double)std::max(std::fabs(o.l[i] - xl[i - (size_t)L]), std::fabs(o.r[i] - xr[i - (size_t)L])));
            worst0 = std::max(worst0, err);
            CHECK(err < 1e-6, "%.0f Hz, %s à intensité 0 non neutre (%.1f dB)", sr, kTypeNames[ty], db(err));
        }
        std::printf("[%5.1f kHz] intensité 0, 8 types : écart max %.1f dB (sortie = entrée retardée)\n", sr / 1000, db(worst0));

        // 3. aucune discontinuité parasite : sinus 220 Hz, 0,5 ; chaque accident à fond, réglages extrêmes
        auto s = sine(N, 220.0, sr);
        { Engine e; Params p; e.set(p); e.prepare(sr, 512); e.pushSeq(Seq{});
          const double nat = maxJump(run(e, s, s, 512, sr), 0);
          std::printf("[%5.1f kHz] saut naturel du sinus : %.4f\n", sr / 1000, nat);
          for (int ty = 0; ty < kNumTypes; ++ty) {
              for (int var = 0; var < 2; ++var) {
                  Engine f; Params q;
                  if (var == 1) { q.stuDiv = 4; q.stuPitch = 12.f; q.stuDecay = 0.f; q.revDiv = 3; q.tapeCurve = 1.f; q.frzMs = 10.f; q.frzJitter = 1.f;
                                  q.repDiv = 2; q.repGap = 0.8f; q.skipRange = 8; q.gateDiv = 2; q.gateDens = 0.6f; q.crBits = 4.f; q.crRate = 3000.f; q.crSmooth = 1.f; q.stepDiv = 2; }
                  f.set(q); f.prepare(sr, 512);
                  Seq sq; for (int i = 0; i < 16; i += 3) sq.s[(size_t)i] = {ty, 1.f, 1.f, 1 + i % 4};
                  f.pushSeq(sq);
                  const double j = maxJump(run(f, s, s, 512, sr, 140.0), 0);
                  const bool crush = ty == Crush;   // la réduction de bits est volontairement en marches : seuil propre
                  // critère : pente max <= 1,5 x pente naturelle x vitesse de lecture max (stutter extrême : rampe jusqu'à x4 = 2 octaves)
                  const double th = crush ? 0.25 : 1.5 * nat * (ty == Stutter && var == 1 ? 4.0 : 1.0);
                  std::printf("  %-8s %s : saut max %.4f (x%.1f du naturel, seuil %.3f)\n", kTypeNames[ty], var ? "extrême" : "défaut ", j, j / nat, th);
                  CHECK(j < th, "%.0f Hz : discontinuité %s (%.4f)", sr, kTypeNames[ty], j);
              }
          }
        }
    }

    const double sr = 48000.0; const size_t N = 48000 * 6;
    auto xl = noise(N, 11), xr = noise(N, 12);
    Seq rnd; { std::mt19937 g(5); std::uniform_real_distribution<float> u(0, 1);
               for (auto& st : rnd.s) st = {u(g) < 0.75f ? (int)(u(g) * 8) % 8 : -1, 0.3f + 0.7f * u(g), 0.4f + 0.6f * u(g), 1 + (int)(u(g) * 4)}; }
    Params pr; pr.chaos = 0.5f; pr.seed = 42; pr.prob = 0.9f; pr.stuPitch = 5.f;

    // 4. reproductible : même séquence + même graine = même sortie, blocs de 1 à 8192
    { std::vector<Ev> pads{{30000, 60, 0.8f}, {52000, 60, 0.f}, {90000, 67, 1.f}, {140000, 67, 0.f}};
      Engine ref; ref.set(pr); ref.prepare(sr, 8192); ref.pushSeq(rnd);
      const auto o0 = run(ref, xl, xr, 512, sr, 128.0, pads);
      double changed = 0; for (size_t i = 0; i < N; ++i) changed = std::max(changed, (double)std::fabs(o0.l[i] - xl[i >= 192 ? i - 192 : 0]));
      for (int blk : {1, 7, 64, 333, 4096, 8192}) {
          Engine e; e.set(pr); e.prepare(sr, 8192); e.pushSeq(rnd);
          const auto o = run(e, xl, xr, blk, sr, 128.0, pads);
          double d = 0; for (size_t i = 0; i < N; ++i) d = std::max(d, (double)std::max(std::fabs(o.l[i] - o0.l[i]), std::fabs(o.r[i] - o0.r[i])));
          std::printf("reproductible, blocs de %4d : écart max %.3g\n", blk, d);
          CHECK(d == 0.0, "blocs de %d : sortie différente (%.3g)", blk, d);
      }
      CHECK(changed > 0.1, "la séquence aléatoire n'a rien fait");
      Params p2 = pr; p2.seed = 43; Engine e2; e2.set(p2); e2.prepare(sr, 8192); e2.pushSeq(rnd);
      const auto o2 = run(e2, xl, xr, 512, sr, 128.0, pads);
      double d = 0; for (size_t i = 0; i < N; ++i) d = std::max(d, (double)std::fabs(o2.l[i] - o0.l[i]));
      std::printf("autre graine : écart %.3f (doit différer)\n", d);
      CHECK(d > 0.01, "la graine ne change rien"); }

    // 5. zéro NaN, sortie bornée (limiteur doux), entrée hostile et réglages extrêmes
    { std::mt19937 g(9); std::uniform_real_distribution<float> u(0, 1);
      double peak = 0; bool finite = true;
      for (int trial = 0; trial < 12; ++trial) {
          Params q; q.mix = u(g); q.outDb = 12.f * u(g); q.chaos = u(g); q.prob = u(g); q.seed = 1 + (int)(u(g) * 999); q.stepDiv = trial % 3;
          q.stuDiv = trial % 5; q.stuPitch = -12.f + 24.f * u(g); q.stuDecay = u(g); q.tapeDepth = u(g); q.tapeCurve = u(g); q.frzMs = 10.f + 240.f * u(g);
          q.crBits = 1.f + 15.f * u(g); q.crRate = 500.f + 23000.f * u(g); q.crSmooth = u(g); q.gateDens = u(g); q.repGap = 0.8f * u(g); q.skipRange = 1 + trial % 8;
          Engine e; e.set(q); e.prepare(sr, 512); e.pushSeq(rnd);
          auto x = noise(48000 * 2, 100 + (unsigned)trial, 4.f);
          x[1000] = NAN; x[2000] = INFINITY; x[3000] = -INFINITY; x[4000] = 1e30f; for (size_t i = 5000; i < 6000; ++i) x[i] = 1e-39f;
          const auto o = run(e, x, x, 64 + trial * 37, sr, 60.0 + 20.0 * trial);
          for (size_t i = 0; i < o.l.size(); ++i) { finite &= std::isfinite(o.l[i]) && std::isfinite(o.r[i]); peak = std::max(peak, (double)std::max(std::fabs(o.l[i]), std::fabs(o.r[i]))); }
      }
      std::printf("entrée hostile (NaN, inf, 1e30, +24 dB) : finie %s, crête %.4f\n", finite ? "oui" : "NON", peak);
      CHECK(finite && peak <= 1.0, "sortie non finie ou au-delà de 0 dBFS"); }

    // 6. zéro allocation pendant le traitement
    { Engine e; e.set(pr); e.prepare(sr, 8192); e.pushSeq(rnd);
      std::vector<Ev> pads{{100, 61, 1.f}, {9000, 61, 0.f}};
      std::vector<float> ol(8192), orr(8192); Host h; h.playing = h.hasPos = h.hasBpm = true;
      g_count = true; g_allocs = 0;
      for (int b = 0; b < 60; ++b) { h.ppq = b * 4096.0 * 2.0 / sr; e.beginBlock(h, b == 0 ? pads.data() : nullptr, b == 0 ? 2 : 0); e.pushSeq(rnd);
                                     e.process(xl.data() + (size_t)b * 4096, xr.data() + (size_t)b * 4096, ol.data(), orr.data(), 4096); }
      g_count = false;
      std::printf("allocations pendant le traitement : %ld\n", g_allocs);
      CHECK(g_allocs == 0, "%ld allocations en temps réel", g_allocs); }

    // 7. mono / stéréo : entrée mono (voie droite nulle) = deux voies identiques à l'entrée dupliquée
    { Engine a, b; a.set(pr); b.set(pr); a.prepare(sr, 512); b.prepare(sr, 512); a.pushSeq(rnd); b.pushSeq(rnd);
      const auto om = run(a, xl, {}, 512, sr, 128.0), os = run(b, xl, xl, 512, sr, 128.0);
      double d = 0; for (size_t i = 0; i < N; ++i) d = std::max(d, (double)std::max(std::fabs(om.l[i] - om.r[i]), std::fabs(om.l[i] - os.l[i])));
      std::printf("mono / stéréo : écart %.3g\n", d);
      CHECK(d == 0.0, "mono incohérent (%.3g)", d); }

    // 8. note MIDI : l'accident démarre à la note et s'arrête au relâchement (retour exact au signal sec)
    { Engine e; Params p; e.set(p); e.prepare(sr, 512); e.pushSeq(Seq{});
      std::vector<Ev> m{{24000, 60 + Stutter, 1.f}, {48000, 60 + Stutter, 0.f}};
      const auto o = run(e, xl, xr, 512, sr, 120.0, m, false);
      const int L = e.latency(); double during = 0, after = 0;
      for (size_t i = 30000; i < 47000; ++i) during = std::max(during, (double)std::fabs(o.l[i] - xl[i - (size_t)L]));
      for (size_t i = 48000 + 4 * (size_t)L; i < N; ++i) after = std::max(after, (double)std::fabs(o.l[i] - xl[i - (size_t)L]));
      std::printf("note MIDI C : écart pendant %.3f, après relâchement %.3g\n", during, after);
      CHECK(during > 0.1 && after < 1e-6, "pad MIDI : pendant %.3f, après %.3g", during, after); }

    // 9. horloge « Lecture » : à l'arrêt, la séquence ne joue pas
    { Engine e; Params p; p.clock = 0; e.set(p); e.prepare(sr, 512); e.pushSeq(all(Stutter, 1.f));
      const auto o = run(e, xl, xr, 512, sr, 120.0, {}, false); const int L = e.latency(); double d = 0;
      for (size_t i = (size_t)L; i < N; ++i) d = std::max(d, (double)std::fabs(o.l[i] - xl[i - (size_t)L]));
      std::printf("horloge Lecture, hôte arrêté : écart %.3g\n", d);
      CHECK(d < 1e-6, "la séquence joue à l'arrêt"); }

    // 10. calage : un stutter au pas 4 (1/16, 120 bpm) commence à 0,5 s (logique), la première répétition à 0,5 s + tranche
    { Engine e; Params p; p.stuDiv = 2; e.set(p); e.prepare(sr, 512); Seq q; q.s[4] = {Stutter, 1.f, 1.f, 4}; e.pushSeq(q);
      auto x = sine(N, 997.0, sr); const auto o = run(e, x, x, 256, sr, 120.0); const int L = e.latency();
      int first = -1; for (size_t i = (size_t)L; i < N && first < 0; ++i) if (std::fabs(o.l[i] - x[i - (size_t)L]) > 1e-4) first = (int)i - L;
      std::printf("calage : premier écart à %d (attendu %d + 1/16 = %d)\n", first, 24000, 24000 + 6000);
      CHECK(first >= 24000 + 6000 - L && first <= 24000 + 6000 + 2, "calage du pas incorrect (%d)", first); }

    // 11. état de la séquence : aller-retour et refus d'une version inconnue
    { Seq a = rnd, b; const auto s = toString(a); const bool ok = fromString(s, b) && toString(b) == s;
      Seq c = rnd; const bool rej = !fromString("ACR9;1,2,3", c) && toString(c) == toString(rnd);
      std::printf("état : aller-retour %s, version inconnue refusée %s (%zu octets)\n", ok ? "oui" : "NON", rej ? "oui" : "NON", s.size());
      CHECK(ok && rej, "sérialisation de la séquence"); }

    std::printf(fails ? "\n%d ECHEC(S)\n" : "\nTOUT EST BON\n", fails);
    return fails ? 1 : 0;
}
