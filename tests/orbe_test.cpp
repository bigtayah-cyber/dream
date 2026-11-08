#include <cstdio>
#include <cstdlib>
#include <set>
#include "../Source/Orbe/OrbeCore.h"
using namespace orbe;
static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { ++fails; std::printf("ECHEC: " __VA_ARGS__); std::printf("\n"); } } while (0)

static bool inScale(const Settings& s, int n) {
    for (int i = 0; i < 7; ++i) if ((s.key + scales()[s.scale].iv[i]) % 12 == n % 12) return true;
    return false;
}
// lit un fichier MIDI : renvoie le nombre de pistes et de notes ; faux si malformé
static bool parseMidi(const std::vector<uint8_t>& b, int& tracks, int& notes) {
    tracks = notes = 0;
    if (b.size() < 14 || b[0] != 'M' || b[1] != 'T' || b[2] != 'h' || b[3] != 'd') return false;
    tracks = (b[10] << 8) | b[11];
    size_t p = 14;
    for (int t = 0; t < tracks; ++t) {
        if (p + 8 > b.size() || b[p] != 'M' || b[p + 1] != 'T' || b[p + 2] != 'r' || b[p + 3] != 'k') return false;
        const uint32_t len = (b[p + 4] << 24) | (b[p + 5] << 16) | (b[p + 6] << 8) | b[p + 7];
        p += 8;
        if (p + len > b.size()) return false;
        size_t q = p; const size_t end = p + len; uint8_t run = 0;
        while (q < end) {
            while (b[q] & 0x80) ++q;
            ++q;  // delta
            uint8_t st = b[q];
            if (st == 0xFF) { q += 2 + b[q + 2] + 1; continue; }
            if (st & 0x80) { run = st; ++q; }
            if ((run & 0xF0) == 0x90 && b[q + 1] > 0) ++notes;
            q += 2;
        }
        p = end;
    }
    return p == b.size();
}

int main() {
    // 1. reproductibilité : même graine => mêmes notes
    {
        Settings s; s.style = 0; s.seed = 42;
        Generator a(s), b(s);
        a.generateAll(); b.generateAll();
        bool same = a.song().nLead.size() == b.song().nLead.size() && a.song().prog == b.song().prog;
        for (size_t i = 0; same && i < a.song().nLead.size(); ++i) same = a.song().nLead[i].n == b.song().nLead[i].n && a.song().nLead[i].t == b.song().nLead[i].t;
        CHECK(same, "[1] meme graine, resultat different");
        b.reseed(43); b.generateAll();
        CHECK(a.song().prog != b.song().prog || a.song().nLead.size() != b.song().nLead.size() || a.song().nLead[0].n != b.song().nLead[0].n, "[1] graines differentes, resultat identique");
        std::printf("[1] reproductibilite OK\n");
    }
    // 2. tous styles x gammes x tonalités : bornes et cohérence
    int combos = 0, leadOut = 0, chordOut = 0, empty = 0;
    for (int st = 0; st < (int)styles().size(); ++st)
        for (int sc = 0; sc < (int)scales().size(); ++sc)
            for (int key = 0; key < 12; ++key)
                for (int bars : {4, 8}) {
                    Settings s; s.style = st; s.scale = sc; s.key = key; s.bars = bars; s.seed = (uint32_t)(st * 131 + sc * 17 + key);
                    s.seventh = styles()[st].seventh; s.add9 = (key % 3 == 0); s.borrow = (key % 2 == 0);
                    Generator g(s); g.generateAll();
                    const Song& so = g.song();
                    ++combos;
                    if (so.prog.size() != (size_t)bars || so.nChords.empty() || so.nBass.empty() || so.nDrums.empty() || so.nLead.empty()) { ++empty; continue; }
                    for (auto& c : so.chords) {
                        if (c.voicing.empty()) { ++empty; continue; }
                        for (int n : c.voicing) if (n < 44 || n > 90) ++chordOut;
                        for (size_t i = 1; i < c.voicing.size(); ++i) if (c.voicing[i] <= c.voicing[i - 1]) ++chordOut;
                    }
                    bool anyBorrow = false; for (auto& c : so.chords) anyBorrow |= c.borrowed;
                    for (auto& n : so.nLead) {
                        if (n.n > 93 || n.n < 28 || n.d <= 0 || n.t < 0 || n.t >= bars * 16) { ++leadOut; std::printf("  borne: style %d n=%d d=%.2f t=%.1f\n", st, n.n, n.d, n.t); }
                        if (!anyBorrow && !inScale(s, n.n)) {  // hors gamme possible seulement via refit sur accord emprunté
                            ++leadOut; std::printf("  gamme: style %d sc %d key %d n=%d t=%.1f\n", st, sc, key, n.n, n.t);
                        }
                    }
                }
    CHECK(empty == 0, "[2] %d generations vides", empty);
    CHECK(chordOut == 0, "[2] %d notes d'accord hors bornes ou desordonnees", chordOut);
    CHECK(leadOut == 0, "[2] %d notes de lead hors bornes/hors gamme", leadOut);
    std::printf("[2] %d combinaisons style/gamme/tonalite/longueur verifiees\n", combos);
    // 3. nommage connu : La mineur, progression i-VI-III-VII
    {
        Settings s; s.key = 9; s.scale = 1; s.style = 0;
        Generator g(s);
        CHECK(g.buildChord(0).name == "Am", "[3] i devrait etre Am, obtenu %s", g.buildChord(0).name.c_str());
        CHECK(g.buildChord(5).name == "F", "[3] VI devrait etre F, obtenu %s", g.buildChord(5).name.c_str());
        CHECK(g.buildChord(6).name == "G", "[3] VII devrait etre G, obtenu %s", g.buildChord(6).name.c_str());
        s.seventh = true; g.setSettings(s);
        CHECK(g.buildChord(0).name == "Am7" && g.buildChord(0).roman == "i7", "[3] Am7 / i7, obtenu %s %s", g.buildChord(0).name.c_str(), g.buildChord(0).roman.c_str());
        s.key = 5; s.seventh = false; g.setSettings(s);  // Fa mineur => bémols
        CHECK(g.buildChord(5).name == "Db", "[3] en Fm, VI = Db, obtenu %s", g.buildChord(5).name.c_str());
        std::printf("[3] noms d'accords OK\n");
    }
    // 4. verrous : un accord verrouillé survit à la régénération
    {
        Settings s; s.style = 0; s.seed = 7; Generator g(s); g.generateChords();
        const int keep = g.song().prog[2];
        bool ok = true;
        for (int i = 0; i < 30; ++i) { g.reseed(1000 + i); g.generateChords({2}); ok &= g.song().prog[2] == keep; }
        CHECK(ok, "[4] accord verrouille modifie");
        std::printf("[4] verrous OK\n");
    }
    // 5. MIDI valide, nombre de notes cohérent
    {
        Settings s; s.style = 5; s.seed = 9; Generator g(s); g.generateAll();
        auto m = g.toMidi(true, true, true, true);
        int tr, nn;
        CHECK(parseMidi(m, tr, nn), "[5] MIDI malforme");
        const size_t expect = g.song().nChords.size() + g.song().nLead.size() + g.song().nBass.size() + g.song().nDrums.size();
        CHECK(tr == 5, "[5] attendu 5 pistes, obtenu %d", tr);
        CHECK((size_t)nn == expect, "[5] notes MIDI %d, attendu %zu", nn, expect);
        FILE* f = std::fopen("/tmp/orbe_demo.mid", "wb"); if (f) { std::fwrite(m.data(), 1, m.size(), f); std::fclose(f); }
        std::printf("[5] MIDI OK (%zu octets, %d notes)\n", m.size(), nn);
    }
    // 6. voice leading : distance moyenne de chaque note au plus proche de l'accord précédent (et inversement), boucle comprise
    {
        double tot = 0; int cnt = 0;
        auto nn = [](int x, const std::vector<int>& L) { int m = 999; for (int y : L) m = std::min(m, std::abs(x - y)); return m; };
        for (int st = 0; st < (int)styles().size(); ++st)
            for (int bars : {4, 8}) {
                Settings s; s.style = st; s.seed = 100 + st; s.bars = bars; Generator g(s); g.generateChords();
                auto& ch = g.song().slots;
                for (size_t i = 0; i < ch.size(); ++i) {
                    const auto& a = ch[i].voicing; const auto& b = ch[(i + 1) % ch.size()].voicing;
                    double d = 0; for (int x : b) d += nn(x, a); for (int x : a) d += nn(x, b);
                    tot += d / (double)(a.size() + b.size()); ++cnt;
                }
            }
        std::printf("[6] mouvement moyen au plus proche voisin : %.2f demi-tons (%d transitions)\n", tot / cnt, cnt);
        CHECK(tot / cnt <= 1.5, "[6] voice leading trop saute (%.2f)", tot / cnt);
    }
    // 7. glissades 808 en trap et drill (4 et 8 mesures, plusieurs graines), et seulement entre notes de hauteurs différentes
    {
        for (int st : {0, 1}) {
            int groups = 0, withSlide = 0, same = 0;
            for (uint32_t seed = 1; seed <= 20; ++seed)
                for (int bars : {4, 8}) {
                    Settings s; s.style = st; s.seed = seed; s.bars = bars; Generator g(s); g.generateChords();
                    const auto& b = g.song().nBass; int sl = 0;
                    for (size_t i = 0; i < b.size(); ++i) if (b[i].gl) { ++sl; if (i == 0 || b[i].n == b[i - 1].n) ++same; }
                    ++groups; withSlide += sl > 0;
                }
            std::printf("[7] %s : glissades dans %d/%d generations\n", styles()[st].label, withSlide, groups);
            CHECK(withSlide == groups, "[7] %s : generation sans glissade 808", styles()[st].label);
            CHECK(same == 0, "[7] %s : glissade entre deux notes identiques", styles()[st].label);
        }
    }
    // 8. batterie : au moins 2 variantes sur 8 mesures, fill en fin de phrase, crash au début, charleys fantômes en trap/drill
    {
        int bad = 0;
        for (int st = 0; st < (int)styles().size(); ++st) {
            Settings s; s.style = st; s.seed = 5 + st; s.bars = 8; Generator g(s); g.generateChords();
            std::set<std::string> sig;
            for (int b = 0; b < 8; ++b) {
                std::vector<std::string> v;
                for (auto& n : g.song().nDrums) if (n.t >= b * 16 && n.t < b * 16 + 16) { char c[48]; std::snprintf(c, sizeof c, "%.2f/%d/%.2f", n.t - b * 16, n.n, n.v); v.push_back(c); }
                std::sort(v.begin(), v.end()); std::string k; for (auto& x : v) k += x + ";"; sig.insert(k);
            }
            if (sig.size() < 2) { ++bad; std::printf("  style %d : %zu variante(s)\n", st, sig.size()); }
        }
        CHECK(bad == 0, "[8] %d styles avec moins de 2 variantes de batterie sur 8 mesures", bad);
        Settings s; s.style = 0; s.seed = 3; s.bars = 8; Generator g(s); g.generateChords();
        int crash = 0, ghost = 0, snFill = 0;
        for (auto& n : g.song().nDrums) { crash += n.n == 49; ghost += n.n == 42 && n.v < 0.25f; snFill += n.n == 38 && std::fmod(n.t, 16.f) >= 12.f; }
        CHECK(crash >= 2 && ghost > 0 && snFill >= 4, "[8] trap : crash %d, ghosts %d, fill %d", crash, ghost, snFill);
        std::printf("[8] variantes de batterie OK (trap : %d crashs, %d ghosts de charley, %d coups de fill)\n", crash, ghost, snFill);
    }
    // 9. basse : aucune note hors de l'accord sur un temps fort, dans toutes les combinaisons ; rythme harmonique présent
    {
        int viol = 0, tot = 0, two = 0, slots = 0;
        for (int st = 0; st < (int)styles().size(); ++st)
            for (int sc = 0; sc < 7; ++sc)
                for (int key = 0; key < 12; key += 3)
                    for (int bars : {4, 8}) {
                        Settings s; s.style = st; s.scale = sc; s.key = key; s.bars = bars; s.seed = (uint32_t)(st * 7 + sc * 3 + key);
                        s.seventh = sc % 2; s.add9 = key == 6; s.borrow = key == 3;
                        Generator g(s); g.generateChords();
                        for (auto& c : g.song().slots) { ++slots; two += c.t > c.bar * 16; }
                        for (auto& n : g.song().nBass) {
                            if (std::fmod(n.t, 4.f) != 0.f) continue;
                            ++tot; const auto& c = g.chordAt(n.t);
                            if (std::find(c.pcs.begin(), c.pcs.end(), n.n % 12) == c.pcs.end()) { ++viol; if (viol < 4) std::printf("  basse hors accord : style %d t=%.1f n=%d %s\n", st, n.t, n.n, c.name.c_str()); }
                        }
                    }
        CHECK(viol == 0, "[9] %d notes de basse hors accord sur temps fort (sur %d)", viol, tot);
        CHECK(two > 20, "[9] rythme harmonique : seulement %d accords en 2e moitie de mesure", two);
        std::printf("[9] basse : %d notes sur temps forts toutes dans l'accord ; %d accords sur %d en milieu de mesure\n", tot, two, slots);
    }
    // 10. reproductibilité complète : mêmes réglages + graine = mêmes notes sur les 4 pistes
    {
        bool same = true;
        for (int st = 0; st < (int)styles().size(); ++st) {
            Settings s; s.style = st; s.seed = 77 + st; s.leadSeed = 5 + st; s.bars = st % 2 ? 8 : 4; s.arp = st % 4;
            Generator a(s), b(s); a.generateAll(); b.generateAll();
            for (int t = 0; t < 4; ++t) same &= Generator::pack(a.song().tr(t)) == Generator::pack(b.song().tr(t));
        }
        CHECK(same, "[10] meme graine, notes differentes");
        std::printf("[10] reproductibilite sur les 4 pistes et 28 styles OK\n");
    }
    // 11. édition + annuler/rétablir : l'état est restauré à l'identique, 20 niveaux, verrous de piste
    {
        Settings s; s.style = 0; s.seed = 11; Generator g(s); g.generateAll();
        auto dump = [&] { std::string r; for (int t = 0; t < 4; ++t) r += Generator::pack(g.song().tr(t)) + "|"; for (int d : g.song().prog) r += std::to_string(d) + ","; return r; };
        const std::string s0 = dump();
        g.pushUndo(); g.addNote(TLead, 4, 2, 72); g.removeNote(TBass, 0);
        const std::string s1 = dump();
        CHECK(s0 != s1, "[11] edition sans effet");
        g.pushUndo(); g.setChord(1, 0, 3); g.setChord(2, 1, 4);
        const std::string s2 = dump();
        CHECK(g.canUndo() && g.undo() && dump() == s1, "[11] annuler 1 ne restaure pas");
        CHECK(g.undo() && dump() == s0, "[11] annuler 2 ne restaure pas");
        CHECK(!g.undo(), "[11] annuler sans historique");
        CHECK(g.redo() && dump() == s1 && g.redo() && dump() == s2 && !g.redo(), "[11] retablir");
        for (int i = 0; i < 25; ++i) { g.pushUndo(); g.addNote(TLead, (float)(i % 60), 1, 60 + i % 12); }
        int n = 0; while (g.undo()) ++n;
        CHECK(n == 20, "[11] profondeur d'annulation %d au lieu de 20", n);
        // verrou : la piste lead modifiée survit à une régénération complète
        g.reseed(999); g.generateChords(); g.generateLead();
        g.addNote(TLead, 0, 1, 90); g.lockMask = 1u << TLead;
        const std::string lead = Generator::pack(g.song().nLead);
        g.reseed(1234); g.reseedLead(4321); g.generateAll();
        CHECK(Generator::pack(g.song().nLead) == lead, "[11] piste verrouillee modifiee par la regeneration");
        auto p = Generator::unpack(Generator::pack(g.song().nBass));
        CHECK(p.size() == g.song().nBass.size() && !p.empty() && std::fabs(p[1].t - g.song().nBass[1].t) < 1e-3, "[11] serialisation des notes");
        std::printf("[11] edition, annuler/retablir (20 niveaux) et verrous de piste OK\n");
    }
    // 12. MIDI relu = notes modifiées (lead et basse), glissades en notes qui se chevauchent
    {
        Settings s; s.style = 0; s.seed = 21; Generator g(s); g.generateAll();
        g.addNote(TLead, 8, 3, 81, 0.9f); g.addNote(TBass, 4, 2, 40, 0.9f); g.removeNote(TLead, 0); g.setNote(TBass, 1, 6, 2, 38);
        auto m = g.toMidi(true, true, true, true);
        // extrait (tick, note) des note-on par canal
        std::multiset<std::pair<int, int>> got[16];
        size_t p = 14;
        for (int t = 0; t < 5; ++t) {
            const uint32_t len = (m[p + 4] << 24) | (m[p + 5] << 16) | (m[p + 6] << 8) | m[p + 7]; p += 8;
            size_t q = p; uint32_t tick = 0; uint8_t run = 0;
            while (q < p + len) {
                uint32_t dt = 0; while (m[q] & 0x80) dt = (dt << 7) | (m[q++] & 0x7f); dt = (dt << 7) | m[q++]; tick += dt;
                uint8_t st = m[q];
                if (st == 0xFF) { q += 3 + m[q + 2]; continue; }
                if (st & 0x80) { run = st; ++q; }
                if ((run & 0xF0) == 0x90 && m[q + 1] > 0) got[run & 15].insert({(int)tick, m[q]});
                q += 2;
            }
            p += len;
        }
        auto want = [&](const std::vector<Note>& v) { std::multiset<std::pair<int, int>> w; for (auto& n : v) w.insert({(int)std::lround(g.swingT(n.t) * 120.f), n.n}); return w; };
        CHECK(got[1] == want(g.song().nLead), "[12] MIDI lead different des notes modifiees");
        CHECK(got[2] == want(g.song().nBass), "[12] MIDI basse different des notes modifiees");
        CHECK(got[0] == want(g.song().nChords) && got[9] == want(g.song().nDrums), "[12] MIDI accords/batterie");
        CHECK(got[1].count({8 * 120, 81}) == 1 && got[2].count({4 * 120, 40}) == 1 && got[2].count({6 * 120, 38}) == 1, "[12] notes ajoutees absentes du MIDI");
        std::printf("[12] fichier MIDI relu coherent avec les notes modifiees (%zu lead, %zu basse)\n", got[1].size(), got[2].size());
    }
    std::printf(fails ? "\n%d ECHEC(S)\n" : "\nTOUS LES TESTS ORBE PASSENT\n", fails);
    return fails ? 1 : 0;
}
