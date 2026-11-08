// Test du moteur Orbe Beat sans JUCE : pas de NaN, pic borné, silence, reproductibilité, motifs, balayage du transport, export MIDI.
#include <cstdio>
#include <vector>
#include "OrbeBeatEngine.h"

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { ++fails; std::printf("ECHEC: " __VA_ARGS__); std::printf("\n"); } } while (0)
static float peakOf(const std::vector<float>& v, size_t a, size_t b) { float m = 0; for (size_t i = a; i < b && i < v.size(); ++i) m = std::max(m, std::fabs(v[i])); return m; }
static bool finite(const std::vector<float>& v) { for (float x : v) if (!std::isfinite(x)) return false; return true; }

static std::vector<float> hit(int kit, int lane, double sr, bool chord = false) {
    obeat::Engine e; e.setKit(kit); e.prepare(sr);
    std::vector<float> out((size_t)(sr * 3.0));
    if (chord) for (int l = 0; l < obeat::kLanes; ++l) e.trigger(l, 1.f, 33);
    else e.trigger(lane, 1.f, 33);
    e.render(out.data(), (int)sr);
    if (lane == obeat::B || chord) e.release808();
    e.render(out.data() + (size_t)sr, (int)(sr * 2));
    CHECK(!e.active(), "voix encore active apres 3 s (kit %d piste %d)", kit, lane);
    return out;
}

int main() {
    using namespace obeat;
    for (double sr : {44100.0, 48000.0})
        for (int k = 0; k < kKits; ++k) {
            for (int l = 0; l < kLanes; ++l) {
                auto a = hit(k, l, sr), b = hit(k, l, sr);
                CHECK(finite(a), "NaN kit %d piste %d", k, l);
                CHECK(peakOf(a, 0, a.size()) <= 1.f && peakOf(a, 0, a.size()) > 0.01f, "pic hors borne kit %d piste %d (%.3f)", k, l, peakOf(a, 0, a.size()));
                CHECK(peakOf(a, (size_t)(sr * 2.5), a.size()) < 1e-4f, "residu apres le coup, kit %d piste %d", k, l);
                CHECK(a == b, "non reproductible kit %d piste %d", k, l);
            }
            auto all = hit(k, 0, sr, true);
            CHECK(finite(all) && peakOf(all, 0, all.size()) <= 1.f, "somme des 8 pistes non bornee, kit %d", k);
        }
    {   // motifs : reproductibles, kick sur le premier temps, variation différente
        for (int k = 0; k < kKits; ++k) {
            Pattern a, b, v; Rng r1(42 + k), r2(42 + k), r3(7);
            generate(a, k, 32, .5f, r1); generate(b, k, 32, .5f, r2);
            bool same = true; for (int l = 0; l < kLanes; ++l) for (int s = 0; s < kMaxSteps; ++s) same &= a.c[l][s] == b.c[l][s];
            CHECK(same, "generate non reproductible, kit %d", k);
            CHECK(k == 5 || filled(a.c[K][0]), "pas de kick au temps 1 (kit %d)", k);
            CHECK(filled(a.c[B][2]) || filled(a.c[B][0]), "pas de 808 (kit %d)", k);
            vary(a, v, r3);
            bool diff = false; for (int l = 0; l < kLanes; ++l) for (int s = 0; s < kMaxSteps; ++s) diff |= a.c[l][s] != v.c[l][s];
            CHECK(diff, "vary identique (kit %d)", k);
        }
        CHECK(rollOf(mk(.5f, 3, 5)) == 3 && degOf(mk(.5f, 3, 5)) == 5 && std::fabs(velOf(mk(.5f)) - .5f) < .01f, "codage des cases");
        CHECK(degNote(9, 0, 0) == 33 && degNote(9, 0, 7) == 45, "degNote A mineur");
    }
    {   // balayage du transport : lire par blocs ou d'un coup donne les mêmes coups, sans doublon ni trou, swing compris
        Pattern p; Rng r(3); generate(p, 6, 32, .8f, r);
        auto at = [&](long long s, int l) { return p.c[l][s % 32]; };
        for (float sw : {0.f, .3f}) {
            int whole = 0, parts = 0; double ppq = 0, step = 0.0731;
            scan(0.0, 16.0, sw, at, [&](double, int, Cell, int) { ++whole; });
            double end = 16.0 - 1e-9; (void)end;
            while (ppq < 16.0) { const double n = std::min(16.0, ppq + step); scan(ppq, n, sw, at, [&](double, int, Cell, int) { ++parts; }); ppq = n; }
            CHECK(whole == parts && whole > 20, "scan : %d d'un coup contre %d par blocs (swing %.1f)", whole, parts, sw);
        }
    }
    {   // export MIDI
        Pattern p; Rng r(9); generate(p, 0, 16, .5f, r);
        auto m = midiFile({&p, &p}, 16, .2f, 142.f, 9, 0);
        CHECK(m.size() > 100 && m[0] == 'M' && m[1] == 'T' && m[2] == 'h' && m[3] == 'd' && m[10] == 0 && m[11] == 3, "en-tete MIDI");
        int ons = 0; for (size_t i = 0; i + 2 < m.size(); ++i) if (m[i] == 0x99 && m[i + 2] > 0 && m[i + 1] >= 35 && m[i + 1] <= 47) ++ons;
        int expect = 0; for (int l = 0; l < kLanes - 1; ++l) for (int s = 0; s < 16; ++s) if (filled(p.c[l][s])) expect += rollOf(p.c[l][s]);
        CHECK(ons >= expect * 2 - 4 && ons <= expect * 2 + 8, "notes batterie MIDI : %d pour %d attendues", ons, expect * 2);
    }
    std::printf(fails ? "orbebeat_test : %d echec(s)\n" : "orbebeat_test : OK\n", fails);
    return fails ? 1 : 0;
}
