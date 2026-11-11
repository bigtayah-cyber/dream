// Carte des fausses notes (Orbe Détect, mode voix) : la hauteur chantée (YIN de l'analyse partagée) est jugée par rapport à une tonalité.
// Sortie : part du temps chanté hors gamme, justesse par note (écart moyen en cents quand elle est mal accordée), et une ligne de temps par cases de 100 ms.
// Pur C++, testable hors-ligne ; aucune allocation côté fil audio (appelé depuis le fil d'analyse).
#pragma once
#include <algorithm>
#include <cmath>
#include <vector>
#include "../OrbeAnalysis/OrbeAnalysis.h"

namespace fn {

constexpr double kCell = 0.1;       // s par case de la ligne de temps
constexpr double kBadCents = 20.0;  // au-delà, une case est jugée « pas juste » (le vibrato centré ne compte pas : la case moyenne ~100 ms)

struct Cell { signed char pc = -1; signed char cents = 0; unsigned char off = 0, bad = 0; };   // pc -1 : pas de voix
struct PcStat { double sec = 0, offSec = 0, badSec = 0, badMean = 0; };

struct Map {
    bool valid = false; int key = 0; bool minor = false;
    double voicedSec = 0, offSec = 0, badSec = 0;
    PcStat pc[12];
    std::vector<Cell> cells;
    double offPct() const { return voicedSec > 0 ? 100.0 * offSec / voicedSec : 0.0; }
    double badPct() const { return voicedSec > 0 ? 100.0 * badSec / voicedSec : 0.0; }
};

inline bool inScale(int pc, int key, bool minor) {
    static const bool maj[12] = {1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1}, mnr[12] = {1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 0};
    return (minor ? mnr : maj)[((pc - key) % 12 + 12) % 12];
}

inline Map build(const float* mono, int n, double sr, int key, bool minor) {
    Map M; M.key = ((key % 12) + 12) % 12; M.minor = minor;
    const auto T = oa::analyzePitch(mono, n, sr);
    const int nc = (int)std::ceil(T.dur / kCell) + 1; M.cells.assign((size_t)std::max(1, nc), Cell{});
    std::vector<double> sum((size_t)M.cells.size(), 0.0); std::vector<int> cnt((size_t)M.cells.size(), 0);
    for (const auto& f : T.frames) {
        if (!(f.f > 0.f) || f.q > 0.25f || f.rms < T.maxR * 0.05f) continue;
        const size_t c = std::min(M.cells.size() - 1, (size_t)std::max(0.0, f.t / kCell));
        sum[c] += oa::ftom(f.f); ++cnt[c];
    }
    for (size_t c = 0; c < M.cells.size(); ++c) {
        if (cnt[c] < 3) continue;   // case trop peu voisée : ignorée
        const double m = sum[c] / cnt[c], nn = std::floor(m + 0.5), dev = (m - nn) * 100.0; const int pc = ((int)nn % 12 + 12) % 12;
        Cell& k = M.cells[c]; k.pc = (signed char)pc; k.cents = (signed char)std::max(-50.0, std::min(50.0, std::floor(dev + 0.5)));
        k.off = inScale(pc, M.key, M.minor) ? 0 : 1; k.bad = std::fabs(dev) > kBadCents ? 1 : 0;
        auto& s = M.pc[pc]; s.sec += kCell; M.voicedSec += kCell;
        if (k.off) { s.offSec += kCell; M.offSec += kCell; }
        if (k.bad) { s.badSec += kCell; s.badMean += dev * kCell; M.badSec += kCell; }
    }
    for (auto& s : M.pc) if (s.badSec > 0) s.badMean /= s.badSec;
    M.valid = M.voicedSec > 0.5;
    return M;
}

// Les notes les plus fausses : (hors gamme) ou (pas juste), triées par temps concerné. Renvoie au plus `maxN` classes de hauteur.
inline std::vector<int> worstNotes(const Map& M, int maxN = 3) {
    std::vector<int> v; for (int i = 0; i < 12; ++i) if (M.pc[i].sec >= 0.3 && std::max(M.pc[i].offSec, M.pc[i].badSec) >= 0.2) v.push_back(i);
    std::sort(v.begin(), v.end(), [&](int a, int b) { return std::max(M.pc[a].offSec, M.pc[a].badSec) > std::max(M.pc[b].offSec, M.pc[b].badSec); });
    if ((int)v.size() > maxN) v.resize((size_t)maxN);
    return v;
}

}  // namespace fn
