// Moteur musical d'Orbe, porté depuis orbe.html. C++ pur, sans JUCE.
// Différence voulue avec la version web : tout l'aléatoire passe par une graine (reproductible).
#pragma once
#include <algorithm>
#include <cstdio>
#include <cmath>
#include <cstdint>
#include <random>
#include <set>
#include <string>
#include <vector>
#include "OrbeStyles.h"

namespace orbe {

struct Note { float t, d; int n; float v; int gl = 0; };  // t, d en pas de double-croche ; 16 pas = 1 mesure ; gl = 1 : glissade depuis la note précédente

struct Chord {
    int deg = 0;
    bool borrowed = false;
    int rootPc = 0;
    std::vector<int> pcs, voicing;
    std::string name, roman;
    int bar = 0;
    float t = 0, len = 16;  // début (pas absolu) et durée de l'accord : plusieurs accords par mesure possibles
};

struct Settings {
    int key = 9;       // 0 = C ... 11 = B
    int scale = 1;     // index dans scales()
    int style = 0;     // index dans styles()
    int bars = 4;      // 4 ou 8
    float density = 0.55f;
    bool seventh = false, add9 = false, borrow = false;
    int arp = 0;       // 0 plaqués, 1 montant, 2 aller-retour, 3 hasard
    float human = 0.f;  // 0..1
    uint32_t seed = 1;      // accords, basse, batterie
    uint32_t leadSeed = 2;  // mélodie
};

enum Track { TChords = 0, TLead = 1, TBass = 2, TDrums = 3 };

struct Song {
    std::vector<int> prog, prog2, split;  // 1er accord de chaque mesure ; 2e accord (-1 = aucun) ; pas où il commence
    std::vector<Chord> chords, slots;     // chords : 1er accord de chaque mesure ; slots : tous les accords dans l'ordre
    std::vector<Note> nChords, nLead, nBass, nDrums;
    std::vector<Note>& tr(int i) { return i == 0 ? nChords : i == 1 ? nLead : i == 2 ? nBass : nDrums; }
    const std::vector<Note>& tr(int i) const { return i == 0 ? nChords : i == 1 ? nLead : i == 2 ? nBass : nDrums; }
};

inline const char* kSharp[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
inline const char* kFlat[12] = {"C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"};
inline const int kMaj[7] = {0, 2, 4, 5, 7, 9, 11};

class Generator {
public:
    explicit Generator(Settings s = {}) : S(s), rng(s.seed) {}

    void setSettings(const Settings& s) { S = s; }
    const Settings& settings() const { return S; }
    void reseed(uint32_t seed) { S.seed = seed; rng.seed(seed); }
    void reseedLead(uint32_t seed) { S.leadSeed = seed; }
    const Song& song() const { return song_; }
    Song& song() { return song_; }
    const Style& style() const { return styles()[S.style]; }

    // ---------- théorie ----------
    bool useFlats() const {
        static const int off[7] = {0, 3, 10, 8, 3, 7, 5};  // maj min dor phr hmin mix lyd
        const int r = (S.key + off[S.scale]) % 12;
        return r == 5 || r == 10 || r == 3 || r == 8 || r == 1;
    }
    bool parallelIsMinor() const { return S.scale == 0 || S.scale == 5 || S.scale == 6; }
    std::vector<int> borrowList() const {
        return parallelIsMinor() ? std::vector<int>{13, 15, 16, 12} : std::vector<int>{13, 14};
    }

    Chord buildChord(int D) const {
        Chord c;
        const bool borrowed = D >= 10;
        const int deg = D % 10;
        const int* iv = scales()[borrowed ? (parallelIsMinor() ? 1 : 0) : S.scale].iv;
        const int k = S.seventh ? 4 : 3;
        std::vector<int> rel;
        for (int j = 0; j < k; ++j) { const int i = deg + 2 * j; rel.push_back(iv[i % 7] + 12 * (i / 7)); }
        const int i3 = rel[1] - rel[0], i5 = rel[2] - rel[0], i7 = k == 4 ? rel[3] - rel[0] : -1;
        char q = (i3 == 4 && i5 == 7) ? 'M' : (i3 == 3 && i5 == 7) ? 'm' : (i3 == 3 && i5 == 6) ? 'd' : (i3 == 4 && i5 == 8) ? 'a' : 'M';
        std::string suf = q == 'M' ? "" : q == 'm' ? "m" : q == 'd' ? "dim" : "aug";
        if (i7 >= 0) {
            if (q == 'M') suf = i7 == 11 ? "maj7" : "7";
            else if (q == 'm') suf = i7 == 10 ? "m7" : "m(maj7)";
            else if (q == 'd') suf = i7 == 10 ? "m7b5" : "dim7";
            else suf = i7 == 11 ? "maj7#5" : "aug7";
        }
        if (S.add9) {
            const int i = deg + 8, n9 = iv[i % 7] + 12 * (i / 7);
            if ((n9 - rel[0]) % 12 == 2) {
                rel.push_back(n9);
                if (i7 < 0) suf = q == 'm' ? "madd9" : q == 'M' ? "add9" : suf + "add9";
                else if (suf == "maj7") suf = "maj9";
                else if (suf == "7") suf = "9";
                else if (suf == "m7") suf = "m9";
                else if (suf == "m(maj7)") suf = "m(maj9)";
                else suf += "(9)";
            }
        }
        c.deg = D; c.borrowed = borrowed;
        c.rootPc = (S.key + rel[0]) % 12;
        static const char* R[7] = {"I", "II", "III", "IV", "V", "VI", "VII"};
        std::string roman = R[deg];
        if (q == 'm' || q == 'd') for (auto& ch : roman) ch = (char)std::tolower(ch);
        if (borrowed) { const int acc = rel[0] - kMaj[deg]; if (acc < 0) roman = "b" + roman; else if (acc > 0) roman = "#" + roman; }
        if (q == 'd') roman += suf == "m7b5" ? "ø" : "°";
        if (q == 'a') roman += "+";
        if (i7 >= 0 && q != 'd') roman += (suf.find("maj7") != std::string::npos || suf.find("maj9") != std::string::npos) ? "M7" : "7";
        if ((int)rel.size() > k) roman += "(9)";
        for (int x : rel) c.pcs.push_back((S.key + x) % 12);
        const bool flat = useFlats() || roman[0] == 'b';
        c.name = std::string((flat ? kFlat : kSharp)[c.rootPc]) + suf;
        c.roman = roman;
        return c;
    }

    // Voice leading : choisit renversement, position et disposition (0 serré, 1 drop 2 / ouvert, 2 large, 3 shell 1-7-3)
    // les plus proches de l'accord précédent. lo : note la plus grave visée (tessiture du style).
    std::vector<int> voice(const std::vector<int>& pcs, const std::vector<int>* prev, int mode = 0, int lo = 50) const {
        std::vector<int> best;
        float bs = 1e9f;
        const int np = (int)pcs.size();
        const bool has9 = np > (S.seventh ? 4 : 3);
        if (mode == 3 && np < 4) mode = 0;
        for (int inv = 0; inv < np; ++inv) {
            if (has9 && inv == np - 1) continue;  // jamais la 9e à la basse
            if (mode == 3 && inv > 0) break;
            std::vector<int> rot(pcs.begin() + inv, pcs.end());
            rot.insert(rot.end(), pcs.begin(), pcs.begin() + inv);
            if (mode == 3) rot = {pcs[0], pcs[3], pcs[1]};
            for (int low = lo; low < lo + 12; ++low) {
                if (low % 12 != rot[0]) continue;
                std::vector<int> v{low};
                for (size_t i = 1; i < rot.size(); ++i) { int n = v[i - 1] + 1; while (n % 12 != rot[i]) ++n; v.push_back(n); }
                const int m = (int)v.size();
                if (mode == 1) { if (m >= 4) v[(size_t)m - 2] -= 12; else if (m == 3) v[1] += 12; }
                else if (mode == 2 && m >= 3) { v[1] += 12; if (m >= 4) v[(size_t)m - 1] += 12; }
                std::sort(v.begin(), v.end());
                bool ok = v.front() >= 44 && v.back() <= 90;
                for (int i = 1; i < m; ++i) ok &= v[(size_t)i] > v[(size_t)i - 1];
                if (!ok) continue;
                float s;
                if (prev && !prev->empty()) {
                    auto nn = [](int x, const std::vector<int>& L) { int d = 999; for (int y : L) d = std::min(d, std::abs(x - y)); return d; };
                    s = 0;
                    for (int x : v) s += (float)nn(x, *prev);
                    for (int x : *prev) s += (float)nn(x, v);
                    s += (float)std::abs(v.back() - prev->back()) * 0.5f;
                } else {
                    float sum = 0; for (int x : v) sum += (float)x;
                    s = std::fabs(sum / (float)m - (float)(lo + 8));
                }
                if (s < bs) { bs = s; best = v; }
            }
        }
        if (best.empty() && mode != 0) return voice(pcs, prev, 0, lo);
        return best;
    }

    // Degrés conseillés pour une mesure : ceux que les grilles du style utilisent à cette place (+ cadence en fin de phrase).
    std::vector<int> suggest(int bar) const {
        std::vector<int> r;
        auto add = [&](int d) { if (std::find(r.begin(), r.end(), d) == r.end()) r.push_back(d); };
        for (auto& bk : style().banks) if (bar % 4 < (int)bk.size()) add(bk[(size_t)(bar % 4)]);
        if (bar % 4 == 3) { add(4); add(parallelIsMinor() ? 3 : 6); }
        return r;
    }

    const Chord& chordAt(float s) const {
        const auto& sl = song_.slots;
        for (size_t i = sl.size(); i-- > 1;) if (sl[i].t <= s + 1e-4f) return sl[i];
        return sl[0];
    }

    // ---------- génération ----------
    // locked : indices d'accords à conserver quand la longueur de la progression ne change pas.
    void generateChords(const std::set<int>& locked = {}) {
        rng.seed(S.seed);
        const Style& st = style();
        std::vector<int> p = pick(st.banks);
        if (rf() < 0.35f) { const int i = 1 + ri(3); const int d = ri(7); if (d != p[(size_t)i - 1]) p[(size_t)i] = d; }
        if (S.bars == 8) {
            std::vector<int> q = rf() < 0.5f ? p : pick(st.banks);
            if (q[0] != 0) q[0] = p[0];
            if (rf() < 0.6f) { std::vector<int> c; for (int x : {4, 5, 6, 3}) if (x != p[3]) c.push_back(x); q[3] = pick(c); }
            p.insert(p.end(), q.begin(), q.end());
        }
        if (S.borrow) {
            std::vector<int> cand;
            for (int i : {1, 2, 3, 5, 6, 7}) if (i < (int)p.size()) cand.push_back(i);
            for (int r = 0; r < (p.size() > 4 ? 2 : 1); ++r) if (rf() < 0.7f) p[(size_t)pick(cand)] = pick(borrowList());
        }
        // rythme harmonique : un 2e accord à la blanche, à la noire pointée ou en anticipation (croche avant la barre)
        const Prof pf = prof();
        std::vector<int> p2(p.size(), -1), sp(p.size(), 8);
        const std::vector<int> cad = (S.scale == 0 || S.scale == 5 || S.scale == 6) ? std::vector<int>{4, 3} : std::vector<int>{4, 6, 3};
        for (size_t b = 0; b < p.size(); ++b) {
            const float pr = std::min(0.6f, pf.hr * (b % 4 == 3 ? 2.2f : 1.f));
            if (rf() >= pr) continue;
            int d2 = b % 4 == 3 ? pick(cad) : pick(st.banks)[b % 4];
            if (d2 == p[b]) d2 = p[b] == 4 ? 3 : 4;
            p2[b] = d2; sp[b] = pick(pf.splits);
        }
        if (song_.prog.size() == p.size()) {
            fit();
            for (int i : locked) if (i < (int)p.size()) { p[(size_t)i] = song_.prog[(size_t)i]; p2[(size_t)i] = song_.prog2[(size_t)i]; sp[(size_t)i] = song_.split[(size_t)i]; }
        }
        song_.prog = p; song_.prog2 = p2; song_.split = sp;
        rebuild();
    }

    void generateLead() {
        if ((lockMask >> TLead & 1) && !song_.nLead.empty()) return;
        editMask &= ~(1u << TLead);
        rng.seed(S.leadSeed ^ 0x9e3779b9u);
        const Style& st = style();
        const int bars = (int)song_.chords.size();
        std::vector<Note> out;
        auto chA = [&](float s) -> const Chord& { return chordAt(s); };
        std::vector<Note> A = genSteps(0, 32, chA, 72 + st.lOct, nullptr);
        for (int p = 0; p < bars / 2; ++p) {
            auto chAt = [&](float s) -> const Chord& { return chordAt((float)(p * 32) + s); };
            std::vector<Note> ns = A;
            if (p > 0) {
                refit(ns, chAt);
                if (p % 2 == 1) {
                    std::vector<Note> kept;
                    for (auto x : ns) if (x.t < 24) { x.d = std::min(x.d, 24 - x.t); kept.push_back(x); }
                    const int last = kept.empty() ? 72 : kept.back().n;
                    int lastOut = 0;
                    auto tail = genSteps(24, 32, chAt, last, &lastOut);
                    kept.insert(kept.end(), tail.begin(), tail.end());
                    ns = kept;
                }
            }
            for (auto x : ns) { x.t += (float)(p * 32); out.push_back(x); }
        }
        bool anyBorrowed = false;
        for (auto& c : song_.slots) anyBorrowed |= c.borrowed;
        if (anyBorrowed) refit(out, chA);
        if (!out.empty()) {
            Note& last = out.back();
            const int tonic = S.key;
            int best = last.n;
            for (int d = 0; d < 7; ++d) {
                if ((last.n - d + 120) % 12 == tonic) { best = last.n - d; break; }
                if ((last.n + d) % 12 == tonic) { best = last.n + d; break; }
            }
            last.n = best;
            last.d = std::min(8.f, (float)bars * 16 - last.t - 0.1f);
            const Chord& lc = chA(last.t);
            auto has = [&](int pc) { return std::find(lc.pcs.begin(), lc.pcs.end(), pc) != lc.pcs.end(); };
            bool clash = false;
            for (int q : lc.pcs) clash |= ((tonic - q + 12) % 12 == 1) || ((q - tonic + 12) % 12 == 1);
            if (lc.borrowed && !has(tonic) && clash) last.n = nearestChordTone(last.n, lc.pcs);
        }
        for (auto& x : out) while (x.n > 93) x.n -= 12;  // pas de notes stridentes au-dessus de A6
        song_.nLead = out;
    }

    void generateAll(const std::set<int>& locked = {}) { generateChords(locked); generateLead(); }

    // Reconstruit accords, basse, batterie à partir de la progression (déterministe pour une graine donnée).
    void setProgression(const std::vector<int>& p) { song_.prog = p; fit(); rebuild(); }
    void setProgression(const std::vector<int>& p, const std::vector<int>& p2, const std::vector<int>& sp) { song_.prog = p; song_.prog2 = p2; song_.split = sp; fit(); rebuild(); }
    // slot 0 : 1er accord de la mesure ; slot 1 : 2e accord (deg < 0 : le retirer)
    void setChord(int bar, int slot, int deg) {
        fit();
        if (bar < 0 || bar >= (int)song_.prog.size()) return;
        const size_t b = (size_t)bar;
        if (slot == 0 && deg >= 0) song_.prog[b] = deg;
        else if (slot == 1) { song_.prog2[b] = deg; if (deg >= 0 && (song_.split[b] <= 0 || song_.split[b] >= 16)) song_.split[b] = 8; }
        rebuild(); generateLead();
    }

    void rebuild() {
        rng.seed(S.seed * 2654435761u + 7u);
        const Style& st = style();
        const Prof pf = prof();
        fit();
        const int bars = (int)song_.prog.size();
        std::vector<Note> sv[4];
        if (lastBars_ == bars) for (int t : {0, 2, 3}) if (lockMask >> t & 1) sv[t] = song_.tr(t);
        // accords (un ou deux par mesure) et voicings
        auto& sl = song_.slots; sl.clear();
        for (int b = 0; b < bars; ++b) {
            const bool two = song_.prog2[(size_t)b] >= 0;
            const int sp = song_.split[(size_t)b];
            Chord c = buildChord(song_.prog[(size_t)b]);
            c.bar = b; c.t = (float)b * 16; c.len = two ? (float)sp : 16.f; sl.push_back(c);
            if (two) { Chord d = buildChord(song_.prog2[(size_t)b]); d.bar = b; d.t = (float)(b * 16 + sp); d.len = 16.f - (float)sp; sl.push_back(d); }
        }
        std::vector<int> modes;
        for (size_t i = 0; i < sl.size(); ++i) {
            int m = pf.mode;
            if (rf() < 0.3f) { const int alt[4] = {0, 1, 2, S.seventh ? 3 : 0}; m = alt[ri(4)]; }
            modes.push_back(m);
        }
        std::vector<int> prev;
        for (size_t i = 0; i < sl.size(); ++i) { sl[i].voicing = voice(sl[i].pcs, prev.empty() ? nullptr : &prev, modes[i], pf.lo); prev = sl[i].voicing; }
        for (size_t i = 0; i < sl.size(); ++i) { sl[i].voicing = voice(sl[i].pcs, &prev, modes[i], pf.lo); prev = sl[i].voicing; }  // 2e passe : bouclage fluide
        song_.chords.clear();
        for (auto& c : sl) if (c.t == (float)c.bar * 16) song_.chords.push_back(c);

        auto& cn = song_.nChords; cn.clear();
        const float bpm = (float)st.bpm;
        for (auto& c : sl) {
            const float end = c.t + c.len;
            if (S.arp == 0) {
                std::vector<std::pair<float, float>> hits;
                for (auto& h : st.rhythm) { const float s = (float)c.bar * 16 + h.step; if (s >= c.t - 1e-4f && s < end - 1e-4f) hits.push_back({s, std::min(h.vel, end - s)}); }
                const bool starts = !hits.empty() && hits.front().first <= c.t + 1e-4f;
                if (!starts) hits.insert(hits.begin(), std::make_pair(c.t, std::min(hits.empty() ? c.len : hits.front().first - c.t + 0.1f, c.len)));
                for (size_t i = 0; i < hits.size(); ++i)
                    for (int n : c.voicing) cn.push_back({hits[i].first, std::max(0.5f, hits[i].second - 0.1f), n, i == 0 ? 0.85f : 0.7f});
            } else {
                std::vector<int> v = c.voicing; v.push_back(c.voicing[0] + 12);
                std::vector<int> seq;
                if (S.arp == 1) seq = v;
                else if (S.arp == 2) { seq = v; for (int i = (int)v.size() - 2; i >= 1; --i) seq.push_back(v[(size_t)i]); }
                const int rate = bpm >= 130 ? 2 : 1;
                for (int k = 0, s = 0; (float)s < c.len - 1e-4f; s += rate, ++k) {
                    const int n = seq.empty() ? v[(size_t)ri((int)v.size())] : seq[(size_t)k % seq.size()];
                    cn.push_back({c.t + (float)s, rate * 0.9f, n, s % 4 == 0 ? 0.85f : 0.66f});
                }
            }
        }
        genBass(pf);
        auto& dn = song_.nDrums; dn.clear();
        for (int b = 0; b < bars; ++b) drumBar(b, dn);
        for (int t : {0, 2, 3}) { if (lockMask >> t & 1) { if (!sv[t].empty()) song_.tr(t) = sv[t]; } else editMask &= ~(1u << t); }
        lastBars_ = bars;
    }

    // ---------- édition (pistes, verrous, historique) ----------
    unsigned lockMask = 0;  // pistes verrouillées : conservées telles quelles par toute régénération
    unsigned editMask = 0;  // pistes modifiées à la main

    struct Snap { std::vector<int> prog, prog2, split; uint32_t seed = 0, leadSeed = 0; unsigned lockMask = 0, editMask = 0; std::vector<Note> n[4]; };
    static constexpr int kUndoDepth = 20;

    Snap snap() const {
        Snap s; s.prog = song_.prog; s.prog2 = song_.prog2; s.split = song_.split; s.seed = S.seed; s.leadSeed = S.leadSeed; s.lockMask = lockMask; s.editMask = editMask;
        for (int t = 0; t < 4; ++t) s.n[t] = song_.tr(t);
        return s;
    }
    void restore(const Snap& s) {
        S.seed = s.seed; S.leadSeed = s.leadSeed;
        song_.prog = s.prog; song_.prog2 = s.prog2; song_.split = s.split;
        lockMask = 0; lastBars_ = -1; rebuild();
        for (int t = 0; t < 4; ++t) song_.tr(t) = s.n[t];
        lockMask = s.lockMask; editMask = s.editMask;
    }
    void pushUndo() { undo_.push_back(snap()); if ((int)undo_.size() > kUndoDepth) undo_.erase(undo_.begin()); redo_.clear(); }
    bool undo() { if (undo_.empty()) return false; redo_.push_back(snap()); restore(undo_.back()); undo_.pop_back(); return true; }
    bool redo() { if (redo_.empty()) return false; undo_.push_back(snap()); restore(redo_.back()); redo_.pop_back(); return true; }
    bool canUndo() const { return !undo_.empty(); }
    bool canRedo() const { return !redo_.empty(); }
    void clearHistory() { undo_.clear(); redo_.clear(); }

    int addNote(int tr, float t, float d, int n, float v = 0.8f) {
        auto& L = song_.tr(tr); const float len = (float)song_.prog.size() * 16;
        t = std::clamp(t, 0.f, len - 0.25f); d = std::clamp(d, 0.25f, len - t);
        L.push_back({t, d, std::clamp(n, 0, 127), v}); editMask |= 1u << tr;
        return (int)L.size() - 1;
    }
    void removeNote(int tr, int i) { auto& L = song_.tr(tr); if (i >= 0 && i < (int)L.size()) { L.erase(L.begin() + i); editMask |= 1u << tr; } }
    void setNote(int tr, int i, float t, float d, int n) {
        auto& L = song_.tr(tr); if (i < 0 || i >= (int)L.size()) return;
        const float len = (float)song_.prog.size() * 16;
        t = std::clamp(t, 0.f, len - 0.25f);
        Note& x = L[(size_t)i]; x.t = t; x.d = std::clamp(d, 0.25f, len - t); x.n = std::clamp(n, 0, 127); x.gl = 0;
        editMask |= 1u << tr;
    }

    static std::string pack(const std::vector<Note>& v) {
        std::string s; char b[96];
        for (auto& x : v) { std::snprintf(b, sizeof b, "%.4f,%.4f,%d,%.4f,%d;", x.t, x.d, x.n, x.v, x.gl); s += b; }
        return s;
    }
    static std::vector<Note> unpack(const std::string& s) {
        std::vector<Note> v; size_t p = 0;
        while (p < s.size()) {
            const size_t e = s.find(';', p); if (e == std::string::npos) break;
            Note n{}; if (std::sscanf(s.c_str() + p, "%f,%f,%d,%f,%d", &n.t, &n.d, &n.n, &n.v, &n.gl) >= 4) v.push_back(n);
            p = e + 1;
        }
        return v;
    }

    // ---------- MIDI ----------
    // Fichier MIDI standard (format 1, 480 ppq). 1 pas = 120 ticks. Canaux : accords 1, lead 2, basse 3, batterie 10.
    std::vector<uint8_t> toMidi(bool chords, bool lead, bool bass, bool drums, float bpmOverride = 0.f) {
        const float bpm = bpmOverride > 0 ? bpmOverride : (float)style().bpm;
        std::vector<uint8_t> out;
        auto w32 = [&](std::vector<uint8_t>& v, uint32_t x) { for (int s = 24; s >= 0; s -= 8) v.push_back((uint8_t)(x >> s)); };
        auto w16 = [&](std::vector<uint8_t>& v, uint16_t x) { v.push_back((uint8_t)(x >> 8)); v.push_back((uint8_t)x); };
        auto vlq = [&](std::vector<uint8_t>& v, uint32_t x) {
            uint8_t b[5]; int n = 0; b[n++] = x & 0x7f;
            while ((x >>= 7)) b[n++] = (uint8_t)((x & 0x7f) | 0x80);
            while (n--) v.push_back(b[n]);
        };
        std::vector<std::vector<uint8_t>> tracks;
        {   // piste de tempo
            std::vector<uint8_t> t; vlq(t, 0);
            const uint32_t us = (uint32_t)(60000000.0 / bpm);
            t.insert(t.end(), {0xFF, 0x51, 0x03, (uint8_t)(us >> 16), (uint8_t)(us >> 8), (uint8_t)us});
            vlq(t, 0); t.insert(t.end(), {0xFF, 0x58, 0x04, 4, 2, 24, 8});
            vlq(t, (uint32_t)(song_.chords.size() * 16 * 120)); t.insert(t.end(), {0xFF, 0x2F, 0x00});
            tracks.push_back(t);
        }
        auto add = [&](const std::vector<Note>& notes, int ch, float vol) {
            struct Ev { uint32_t tick; bool on; int n; int v; };
            std::vector<Ev> ev;
            for (auto x : notes) {
                const uint32_t t0 = (uint32_t)std::lround(swingT(x.t) * 120.f);
                const uint32_t t1 = t0 + std::max<uint32_t>(10, (uint32_t)std::lround(x.d * 120.f));
                const int vel = std::clamp((int)std::lround(x.v * vol / 0.8f * 105.f + 10.f), 1, 127);
                ev.push_back({t0, true, x.n, vel}); ev.push_back({t1, false, x.n, 0});
            }
            std::stable_sort(ev.begin(), ev.end(), [](const Ev& a, const Ev& b) { return a.tick != b.tick ? a.tick < b.tick : (!a.on && b.on); });
            std::vector<uint8_t> t; uint32_t last = 0;
            for (auto& e : ev) { vlq(t, e.tick - last); last = e.tick; t.push_back((uint8_t)((e.on ? 0x90 : 0x80) | (ch - 1))); t.push_back((uint8_t)e.n); t.push_back((uint8_t)e.v); }
            vlq(t, 0); t.insert(t.end(), {0xFF, 0x2F, 0x00});
            tracks.push_back(t);
        };
        if (chords) add(song_.nChords, 1, 0.8f);
        if (lead) add(song_.nLead, 2, 0.8f);
        if (bass) add(song_.nBass, 3, 0.85f);
        if (drums) add(song_.nDrums, 10, 0.85f);
        out.insert(out.end(), {'M', 'T', 'h', 'd'}); w32(out, 6); w16(out, 1); w16(out, (uint16_t)tracks.size()); w16(out, 480);
        for (auto& t : tracks) { out.insert(out.end(), {'M', 'T', 'r', 'k'}); w32(out, (uint32_t)t.size()); out.insert(out.end(), t.begin(), t.end()); }
        return out;
    }

    float swingT(float t) const {
        return (t == std::floor(t) && (int)t % 2 == 1) ? t + style().swing * 0.42f : t;
    }

    static int nearestChordTone(int n, const std::vector<int>& pcs) {
        auto has = [&](int pc) { return std::find(pcs.begin(), pcs.end(), pc) != pcs.end(); };
        for (int d = 0; d < 7; ++d) {
            if (has(((n - d) % 12 + 12) % 12)) return n - d;
            if (has((n + d) % 12)) return n + d;
        }
        return n;
    }

private:
    Settings S;
    std::mt19937 rng;
    Song song_;
    int lastBars_ = -1;
    std::vector<Snap> undo_, redo_;

    float rf() { return std::uniform_real_distribution<float>(0.f, 1.f)(rng); }
    int ri(int n) { return (int)(rf() * (float)n) % n; }
    template <class T> T pick(const std::vector<T>& v) { return v[(size_t)ri((int)v.size())]; }
    void fit() { const size_t n = song_.prog.size(); song_.prog2.resize(n, -1); song_.split.resize(n, 8); }

    // profil musical du style : rythme harmonique, tessiture et disposition des accords, glissades 808
    struct Prof { float hr = 0.25f; std::vector<int> splits{8}; int lo = 52, mode = 0; float slide = 0.f; };
    Prof prof() const {
        const Style& st = style();
        const std::string id = st.id, cs = st.cs, grp = st.group;
        Prof p;
        if (cs == "keys") { p.lo = 48; p.mode = 1; } else if (cs == "stab") p.lo = 55; else if (cs == "supersaw") { p.lo = 55; p.mode = 2; }
        auto in = [&](std::initializer_list<const char*> l) { for (auto* x : l) if (id == x) return true; return false; };
        if (grp == "Club") p.hr = 0.2f; else if (grp == "Monde") p.hr = 0.35f; else if (grp == "Pop & alt") p.hr = 0.3f;
        if (in({"techno", "trance", "hardstyle", "acid", "dnb", "dubstep", "ambient"})) p.hr = 0.08f;
        if (in({"trap", "drill", "phonk", "plugg"})) p.hr = 0.16f;
        if (in({"boombap", "lofi", "ukg", "deephouse", "rnb"})) p.hr = 0.38f;
        if (in({"house", "deephouse", "techhouse", "ukg", "afrobeats", "amapiano", "reggaeton", "rnb", "pop"})) p.splits = {8, 12, 14};
        else if (grp != "Rap") p.splits = {8, 12};
        p.slide = id == "drill" ? 0.75f : id == "trap" ? 0.5f : in({"phonk", "plugg", "rage", "hyperpop", "digicore", "futurebass"}) ? 0.28f : 0.f;
        return p;
    }

    std::vector<int> keyPool(int off) const {
        std::set<int> pcs;
        for (int i = 0; i < 7; ++i) pcs.insert((S.key + scales()[S.scale].iv[i]) % 12);
        std::vector<int> pool;
        for (int n = 60 + off; n <= std::min(86 + off, 91); ++n) if (pcs.count(n % 12)) pool.push_back(n);
        return pool;
    }
    static int nearestIdx(const std::vector<int>& pool, int n) {
        int bi = 0, bd = 1 << 30;
        for (size_t i = 0; i < pool.size(); ++i) { const int d = std::abs(pool[i] - n); if (d < bd) { bd = d; bi = (int)i; } }
        return bi;
    }
    bool inKey(int pc) const { for (int i = 0; i < 7; ++i) if ((S.key + scales()[S.scale].iv[i]) % 12 == pc % 12) return true; return false; }

    // Basse / 808 : rythme du style + variations par mesure, quinte, octave, notes de passage ; temps forts = notes de l'accord ;
    // glissades 808 (gl = 1) vers la note suivante quand le style s'y prête.
    void genBass(const Prof& pf) {
        const Style& st = style();
        auto& bn = song_.nBass; bn.clear();
        const std::string bs = st.bs;
        const bool low = bs == "808" || bs == "808d" || bs == "reese" || bs == "wobble";
        const bool rap = std::string(st.group) == "Rap";
        const int bars = (int)song_.prog.size();
        auto rootN = [&](const Chord& c) { return low ? 31 + ((c.rootPc - 7 + 12) % 12) : 36 + c.rootPc; };
        auto up = [&](const Chord& c, int pc) { return rootN(c) + ((pc - c.rootPc + 12) % 12); };
        for (int b = 0; b < bars; ++b) {
            std::vector<int> steps = st.bass;
            auto addStep = [&](int s) { if (std::find(steps.begin(), steps.end(), s) == steps.end()) steps.push_back(s); };
            for (auto& c : song_.slots) if (c.bar == b && c.t > (float)b * 16) addStep((int)(c.t - (float)b * 16));
            if (b % 2 == 1 && (rap || low) && rf() < 0.7f) addStep(pick(std::vector<int>{3, 7, 11, 14}));
            if (b % 4 == 3 && rf() < 0.65f) addStep(14);
            std::sort(steps.begin(), steps.end());
            for (size_t i = 0; i < steps.size(); ++i) {
                const int s = steps[i];
                const float abs = (float)(b * 16 + s);
                const Chord& c = chordAt(abs);
                const bool strong = s % 4 == 0, atStart = abs <= c.t + 1e-4f;
                const float end = std::min(c.t + c.len, (float)(b * 16 + (i + 1 < steps.size() ? steps[i + 1] : 16)));
                int n = rootN(c);
                const float r = rf();
                const bool pickup = s >= 14 && i + 1 == steps.size() && c.t + c.len > abs + 1.5f;
                if (pickup && b % 4 == 3 && !atStart) {  // note d'approche vers la fondamentale suivante (hors temps fort)
                    const Chord& nx = chordAt(std::fmod(abs + 2.f, (float)bars * 16));
                    const int tgt = nx.rootPc;
                    int pc = (tgt + (rf() < 0.5f ? 11 : 1)) % 12;
                    if (!inKey(pc)) pc = (tgt + (pc == (tgt + 1) % 12 ? 2 : 10)) % 12;
                    n = up(c, pc); while (n - rootN(c) > 6) n -= 12; while (rootN(c) - n > 6) n += 12;
                } else if (!atStart) {
                    if (strong) n = r < 0.62f ? n : r < 0.82f ? up(c, c.pcs[2]) : n + 12;
                    else n = r < 0.4f ? n : r < 0.58f ? n + 12 : r < 0.75f ? up(c, c.pcs[2]) : r < 0.88f ? up(c, (c.rootPc + (inKey(c.rootPc + 2) ? 2 : 1)) % 12) : n;
                }
                float d = end - abs - 0.15f;
                if (st.bShort) d = std::min(1.6f, d);
                bn.push_back({abs, std::max(0.4f, d), n, atStart && s == 0 ? 1.f : 0.85f});
            }
        }
        if (pf.slide <= 0.f) return;
        // glissades : une partie des notes glisse depuis la précédente (selon le style), dans la même mesure
        auto slideAt = [&](size_t j, bool force) {
            Note& cur = bn[j]; const Note& pr = bn[j - 1];
            if (cur.gl || (int)(cur.t / 16) != (int)(pr.t / 16)) return false;
            if (!force && rf() >= pf.slide) return false;
            const Chord& c = chordAt(cur.t);
            if (cur.n == pr.n) cur.n = (up(c, c.pcs[2]) != pr.n) ? up(c, c.pcs[2]) : rootN(c) + 12;
            if (cur.n == pr.n) cur.n = rootN(c);
            if (cur.n == pr.n) return false;
            cur.gl = 1; bn[j - 1].d = cur.t - bn[j - 1].t + 0.7f;
            return true;
        };
        for (size_t j = 1; j < bn.size(); ++j) slideAt(j, false);
        for (int g = 0; g < bars; g += 4) {  // au moins une glissade par groupe de 4 mesures
            bool any = false;
            for (auto& x : bn) any |= x.gl && x.t >= (float)(g * 16) && x.t < (float)((g + 4) * 16);
            for (size_t j = bn.size(); !any && j-- > 1;) if (bn[j].t >= (float)(g * 16) && bn[j].t < (float)((g + 4) * 16)) any = slideAt(j, true);
        }
    }

    template <class F>
    std::vector<Note> genSteps(int from, int to, F chordOf, int cur, int* lastOut) {
        const Style& st = style();
        const int off = st.lOct;
        const std::vector<int> pool = keyPool(off);
        const float dens = std::clamp(S.density + st.dBoost, 0.05f, 1.f);
        std::vector<int> on;
        for (int s = from; s < to; ++s) {
            const float str = s % 8 == 0 ? 1.f : s % 4 == 0 ? 0.72f : s % 2 == 0 ? 0.42f : 0.2f;
            float p = str * (0.22f + dens * 0.92f);
            if (s == from && s % 16 == 0) p = std::max(p, 0.85f);
            if (s >= to - 2 && to % 16 == 0) p *= 0.25f;  // respiration en fin de phrase
            if (rf() < p) on.push_back(s);
        }
        if (on.empty()) on.push_back(from);
        int idx = nearestIdx(pool, cur);
        std::vector<Note> notes;
        for (size_t i = 0; i < on.size(); ++i) {
            const int s = on[i];
            const int next = i + 1 < on.size() ? on[i + 1] : to;
            float d = std::min((float)(next - s), st.maxDur);
            if (rf() < st.stacc) d = std::min(d, rf() < 0.5f ? 1.f : 1.5f);
            if (next - s >= 3 && rf() < 0.35f) d = std::max(1.f, (float)(next - s) * 0.55f);  // silence après la note
            if (i + 1 == on.size() && to - s > 3) d = std::min(d, (float)(to - s) - 2.f);
            const Chord& ch = chordOf((float)s);
            int n;
            if (s % 4 == 0 || i == 0) {  // temps forts et note de départ : tons d'accord proches
                std::vector<int> cands;
                for (int x : pool) if (std::find(ch.pcs.begin(), ch.pcs.end(), x % 12) != ch.pcs.end() && std::abs(x - pool[(size_t)idx]) <= 7) cands.push_back(x);
                std::sort(cands.begin(), cands.end(), [&](int a, int b) { return std::abs(a - pool[(size_t)idx]) < std::abs(b - pool[(size_t)idx]); });
                if (!cands.empty()) {
                    n = cands[rf() < 0.6f ? 0 : std::min<size_t>(1, cands.size() - 1)];
                    idx = (int)(std::find(pool.begin(), pool.end(), n) - pool.begin());
                } else n = pool[(size_t)idx];
            } else {
                static const int small[9] = {-2, -1, -1, -1, 1, 1, 1, 2, 0};
                static const int big[4] = {-4, -3, 3, 4};
                int mv = small[ri(9)];
                if (rf() < 0.12f) mv = big[ri(4)];
                if (pool[(size_t)idx] > 81 + off) mv = -(std::abs(mv) ? std::abs(mv) : 1);
                if (pool[(size_t)idx] < 65 + off) mv = std::abs(mv) ? std::abs(mv) : 1;
                idx = std::clamp(idx + mv, 0, (int)pool.size() - 1);
                const size_t k = notes.size();
                if (k >= 2 && notes[k - 1].n == pool[(size_t)idx] && notes[k - 2].n == pool[(size_t)idx])
                    idx = std::clamp(idx + (rf() < 0.5f ? -1 : 1), 0, (int)pool.size() - 1);
                n = pool[(size_t)idx];
            }
            const float acc = s % 8 == 0 ? 0.95f : s % 4 == 0 ? 0.84f : 0.6f + rf() * 0.15f;  // accents
            notes.push_back({(float)s, std::max(0.5f, d - 0.05f), n, i + 1 == on.size() ? std::min(acc, 0.72f) : acc});
        }
        if (lastOut) *lastOut = pool[(size_t)idx];
        return notes;
    }

    template <class F>
    void refit(std::vector<Note>& ns, F chAt) {
        for (auto& x : ns) {
            const Chord& c = chAt(x.t);
            auto has = [&](int pc) { return std::find(c.pcs.begin(), c.pcs.end(), pc) != c.pcs.end(); };
            bool clash = false;
            for (int p : c.pcs) { const int a = ((x.n % 12) - p + 12) % 12; clash |= (a == 1 || a == 11); }
            const bool inChord = has(x.n % 12);
            if (((int)x.t % 4 == 0 && !inChord) || (clash && (x.d >= 2 || c.borrowed) && !inChord)) x.n = nearestChordTone(x.n, c.pcs);
        }
    }

    // Batterie : A (base), A' (ghost notes), A, fin de phrase (fill) ; crash au début de chaque phrase de 4 mesures.
    void drumBar(int b, std::vector<Note>& L) {
        const Style& st = style();
        const Drums& D = st.dr;
        const int bars = (int)song_.prog.size(), ph = b % 4;
        const bool fill = ph == 3, alt = ph == 1, lastBar = b == bars - 1;
        const std::string bs = st.bs, grp = st.group;
        const bool trapish = grp == "Rap" && (bs == "808" || bs == "808d");
        std::vector<Note> T;
        auto add = [&](float t, int n, float v) { T.push_back({(float)b * 16 + t, 0.5f, n, v}); };
        auto put = [&](const std::vector<Hit>& a, int n, float v) { for (auto& h : a) add(h.step, n, h.vel >= 0 ? h.vel : v); };
        put(D.K, 36, 1.f); put(D.HK, 35, 1.f); put(D.S, 38, 0.85f); put(D.C, 39, 0.9f); put(D.R, 37, 0.55f); put(D.O, 46, 0.42f);
        const float hv = D.hv;
        if (D.hGrid == 8) for (int t = 0; t < 16; t += 2) add((float)t, 42, t % 4 ? hv * 0.7f : hv);
        else if (D.hGrid == 16) for (int t = 0; t < 16; ++t) add((float)t, 42, t % 2 ? hv * 0.6f : hv);
        else put(D.H, 42, hv);
        for (auto& h : D.kR) if (rf() < (alt ? 0.8f : 0.4f)) add(h.step, 36, 0.75f);
        if (trapish) for (int t = 1; t < 16; t += 2) if (rf() < 0.22f) add((float)t, 42, 0.2f);  // ghost notes de charleston
        if (alt) {
            if (grp == "Club") add(6.f, 46, 0.35f);
            else add(pick(std::vector<float>{7.f, 11.f, 15.f}), 38, 0.2f);  // ghost de caisse claire
            if (D.roll || trapish) { const float rl[3][2] = {{12.f, 0.3f}, {12.667f, 0.4f}, {13.333f, 0.52f}}; for (auto& r : rl) add(r[0], 42, r[1]); }
        } else if (ph == 2 && (D.roll || trapish)) { add(15.f, 42, 0.35f); add(15.5f, 42, 0.45f); }
        if (fill) {
            T.erase(std::remove_if(T.begin(), T.end(), [&](const Note& n) { return n.t - (float)b * 16 >= 12.f && (n.n == 36 || n.n == 35 || n.n == 42 || n.n == 46); }), T.end());
            if (trapish || D.stut) for (float t : {14.f, 14.5f, 15.f, 15.25f, 15.5f, 15.75f}) add(t, 42, 0.3f + (t - 14.f) * 0.22f);
            const std::vector<float> sn = lastBar ? std::vector<float>{12.f, 12.5f, 13.f, 13.5f, 14.f, 14.5f, 15.f, 15.5f}
                                        : trapish ? std::vector<float>{14.f, 14.5f, 15.f, 15.5f} : std::vector<float>{12.f, 13.f, 14.f, 14.5f, 15.f, 15.5f};
            for (size_t i = 0; i < sn.size(); ++i) add(sn[i], 38, 0.45f + 0.5f * (float)i / (float)sn.size());
        }
        if (ph == 0) add(0.f, 49, 0.7f);  // crash
        L.insert(L.end(), T.begin(), T.end());
    }
};

}  // namespace orbe
