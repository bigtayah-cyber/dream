// Test console de la carte des fausses notes et du collage de tonalité (sans JUCE). Cible : orbedetect_test (EXCLUDE_FROM_ALL).
#include <cstdio>
#include "FalseNotes.h"
#include "../../Kit/KeyParse.h"
static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { ++fails; std::printf("ECHEC: " __VA_ARGS__); std::printf("\n"); } } while (0)
int main() {
    const double sr = 48000.0;
    // voix : Do4 Mi4 Sol4 (justes, 0,6 s chacun), Fa#4 (hors gamme de Do majeur, 0,6 s), Mi4 à -35 cents (1,2 s), silence 0,3 s, Do4 avec vibrato centré (0,9 s)
    struct Seg { double midi, dur, vib; }; const Seg segs[] = {{60, .6, 0}, {64, .6, 0}, {67, .6, 0}, {66, .6, 0}, {63.65, 1.2, 0}, {-1, .3, 0}, {60, .9, 0.3}};
    std::vector<float> x; double ph = 0;
    for (auto& s : segs) for (int i = 0; i < (int)(s.dur * sr); ++i) {
        float v = 0; if (s.midi > 0) { const double t = (double)i / sr, m = s.midi + s.vib * std::sin(2 * 3.14159265 * 5.5 * t), f = 440.0 * std::pow(2.0, (m - 69) / 12.0); ph += 2 * 3.14159265 * f / sr;
            const double env = std::min(1.0, std::min(t * 40.0, (s.dur - t) * 40.0)); for (int h = 1; h <= 6; ++h) v += (float)(std::sin(h * ph) / h * env); }
        x.push_back(0.3f * v);
    }
    const auto M = fn::build(x.data(), (int)x.size(), sr, 0, false);
    const double offPct = M.offPct(), badPct = M.badPct();
    std::printf("carte : voix %.1f s, hors gamme %.0f %%, pas juste %.0f %%\n", M.voicedSec, offPct, badPct);
    // hors gamme : Fa# 0,6 s sur ~4,8 s chantées = ~12 % ; pas juste : Fa# (juste en accordage mais hors gamme : non) et Mi -35 cents 1,2 s = ~25 %
    CHECK(M.valid && offPct > 8.0 && offPct < 18.0, "hors gamme : %.1f %%", offPct);
    CHECK(badPct > 17.0 && badPct < 32.0, "pas juste : %.1f %%", badPct);
    const auto w = fn::worstNotes(M, 3);
    std::printf("  notes en cause :"); for (int p : w) std::printf(" %d (hors %.1f s, faux %.1f s, %+.0f ct)", p, M.pc[p].offSec, M.pc[p].badSec, M.pc[p].badMean); std::printf("\n");
    bool hasFs = false, hasE = false; for (int p : w) { hasFs |= p == 6; hasE |= p == 3; }   // Fa# ; Mi bémol (63,65 arrondi à 64 : Mi)
    CHECK(M.pc[6].offSec > 0.4, "Fa# non vu hors gamme"); CHECK(M.pc[4].badSec > 0.8 && M.pc[4].badMean < -25 && M.pc[4].badMean > -45, "Mi a -35 ct : %.1f s, %.0f ct", M.pc[4].badSec, M.pc[4].badMean);
    CHECK(M.pc[0].badSec < 0.25, "le vibrato centré ne doit pas compter faux : %.2f s", M.pc[0].badSec); (void)hasFs; (void)hasE;
    // même voix jugée en Fa# mineur : changement de verdict
    const auto M2 = fn::build(x.data(), (int)x.size(), sr, 6, true); std::printf("  mêmes notes en Fa# mineur : hors gamme %.0f %%\n", M2.offPct()); CHECK(M2.offPct() > offPct + 20.0, "verdict identique dans une autre tonalité");
    // collage de tonalité
    struct C { const char* s; int pc; bool minor; bool ok; };
    const C cases[] = {{"Fm", 5, true, true}, {"F minor", 5, true, true}, {"F mineur", 5, true, true}, {"F# minor", 6, true, true}, {"Bb", 10, false, true}, {"Bbm", 10, true, true}, {"Bm", 11, true, true}, {"B", 11, false, true},
                       {"A\xE2\x99\xAD majeur", 8, false, true}, {"Eb Major", 3, false, true}, {"  c", 0, false, true}, {"Fm +23 ct", 5, true, true}, {"Fa", 0, false, false}, {"", 0, false, false}, {"hello", 0, false, false}, {"Do", 0, false, false}};
    for (auto& c : cases) { kit::keyparse::Key k; const bool ok = kit::keyparse::parse(c.s, k); CHECK(ok == c.ok && (!ok || (k.pc == c.pc && k.minor == c.minor)), "parse \"%s\" : ok=%d pc=%d min=%d", c.s, ok, k.pc, k.minor); }
    for (int pc = 0; pc < 12; ++pc) for (bool mn : {false, true}) { kit::keyparse::Key k; CHECK(kit::keyparse::parse(kit::keyparse::format(pc, mn), k) && k.pc == pc && k.minor == mn, "aller-retour %d %d", pc, mn); }
    std::printf(fails ? "orbedetect_test : %d ECHEC(S)\n" : "orbedetect_test : OK\n", fails);
    return fails ? 1 : 0;
}
