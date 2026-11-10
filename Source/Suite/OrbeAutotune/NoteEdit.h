// Orbe Autotune : édition note par note (sans JUCE). Capture de la hauteur pendant la lecture, découpage en notes,
// liste d'éditions consultée par le moteur à chaque trame (recherche dichotomique, aucune allocation côté audio).
// Les positions sont des positions de la timeline de l'hôte, en échantillons, de l'audio analysé (entrée du plugin).
#pragma once
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace ned {

struct Note {
    double start = 0.0, end = 0.0;   // positions (échantillons de timeline)
    float center = 0.f;              // hauteur médiane détectée (MIDI fractionnaire)
    float shift = 0.f;               // déplacement demandé (cents) : cible = center + shift / 100
    float amount = 1.f;              // force de correction de la note
    float expr = 1.f;                // vibrato / expression conservés (0..1)
    float speedMs = 30.f;            // vitesse d'accrochage de la note
    int edited = 0;                  // 0 : la note suit le réglage global ; 1 : la note est imposée
    float target() const { return center + shift * 0.01f; }
};

constexpr int kMaxNotes = 1024;

// Copie consultée par le fil audio (taille fixe).
struct EditList {
    int n = 0;
    Note notes[kMaxNotes];
    // Note éditée qui couvre `pos`, ou nul.
    const Note* at(double pos) const {
        int lo = 0, hi = n - 1, k = -1;
        while (lo <= hi) { const int m = (lo + hi) / 2; if (notes[m].start <= pos) { k = m; lo = m + 1; } else hi = m - 1; }
        if (k < 0) return nullptr;
        const Note& q = notes[k];
        return (pos < q.end && q.edited) ? &q : nullptr;
    }
    void assign(const std::vector<Note>& v) {
        n = (int)std::min<size_t>(v.size(), kMaxNotes);
        for (int i = 0; i < n; ++i) notes[i] = v[(size_t)i];
    }
};

struct Frame { double pos; float midi; };   // midi = 0 : non voisé

// Tampon de capture (alloué une fois). Écrit par le fil audio quand `armed`, lu par l'interface quand `armed` retombe.
struct Capture {
    static constexpr int kCap = 32768;
    std::vector<Frame> f = std::vector<Frame>((size_t)kCap);
    std::atomic<int> n{0};
    std::atomic<int> armed{0};   // 0 repos, 1 armé (attend la lecture), 2 enregistre, 3 terminé (à découper)
    double hop = 128.0;
    void push(double pos, float midi) {
        const int a = armed.load(std::memory_order_relaxed);
        if (a != 1 && a != 2) return;
        int k = n.load(std::memory_order_relaxed);
        if (a == 2 && k > 0 && pos < f[(size_t)k - 1].pos - 0.5 * hop) { armed.store(3, std::memory_order_release); return; }   // boucle : un passage complet
        if (k >= kCap) { armed.store(3, std::memory_order_release); return; }
        f[(size_t)k] = {pos, midi};
        n.store(k + 1, std::memory_order_release);
        if (a == 1) armed.store(2, std::memory_order_relaxed);
    }
};

inline float medianOf(std::vector<float> v) {
    if (v.empty()) return 0.f;
    std::nth_element(v.begin(), v.begin() + (long)(v.size() / 2), v.end());
    return v[v.size() / 2];
}

// Retire les pointes isolées de la courbe (erreurs d'octave d'une ou deux trames aux changements de note) : médiane de 5.
inline void despike(std::vector<Frame>& f) {
    std::vector<Frame> o = f;
    for (size_t i = 2; i + 2 < f.size(); ++i) {
        if (f[i].midi <= 0.f) continue;
        float w[5]; int n = 0; for (size_t k = i - 2; k <= i + 2; ++k) if (f[k].midi > 0.f) w[n++] = f[k].midi;
        if (n < 3) continue;
        std::sort(w, w + n);
        if (std::fabs(f[i].midi - w[n / 2]) > 1.f) o[i].midi = w[n / 2];
    }
    f.swap(o);
}

// Découpe la courbe capturée en notes : segments voisés contigus, coupés quand la hauteur lissée s'écarte durablement
// (>= 0,75 demi-ton pendant >= 4 trames) de la médiane de la note en cours. Notes de moins de 40 ms fusionnées.
inline std::vector<Note> segment(const Frame* fr, int n, double hop, double sr) {
    std::vector<Note> out;
    const double minLen = 0.04 * sr;
    int i = 0;
    while (i < n) {
        while (i < n && fr[i].midi <= 0.f) ++i;
        if (i >= n) break;
        int j = i;   // fin du segment voisé (trous de 2 trames tolérés, positions contiguës)
        while (j + 1 < n) {
            if (fr[j + 1].pos - fr[j].pos > 1.5 * hop) break;
            if (fr[j + 1].midi > 0.f) { ++j; continue; }
            int g = j + 1; while (g < n && fr[g].midi <= 0.f && g - j <= 3) ++g;
            if (g < n && g - j <= 3 && fr[g].midi > 0.f && fr[g].pos - fr[j].pos < 4.5 * hop) j = g; else break;
        }
        int s = i, dev = 0;
        std::vector<float> cur;
        auto close = [&](int a, int b) {
            std::vector<float> v; for (int k = a; k <= b; ++k) if (fr[k].midi > 0.f) v.push_back(fr[k].midi);
            if (v.empty()) return;
            Note q; q.start = fr[a].pos; q.end = fr[b].pos + hop; q.center = medianOf(v);
            if (!out.empty() && (q.end - q.start < minLen) && q.start - out.back().end < 2 * hop) { out.back().end = q.end; return; }
            out.push_back(q);
        };
        for (int k = i; k <= j; ++k) {
            if (fr[k].midi <= 0.f) continue;
            float sm[5]; int ns = 0;
            for (int q = std::max(s, k - 2); q <= std::min(j, k + 2); ++q) if (fr[q].midi > 0.f) sm[ns++] = fr[q].midi;
            std::sort(sm, sm + ns);
            const float smooth = sm[ns / 2];
            const float med = cur.empty() ? smooth : medianOf(cur);
            if (cur.size() >= 4 && std::fabs(smooth - med) >= 0.75f) {
                if (++dev >= 4) { const int cut = k - dev + 1; close(s, cut - 1); s = cut; cur.clear(); dev = 0; for (int q = cut; q <= k; ++q) if (fr[q].midi > 0.f) cur.push_back(fr[q].midi); }
            } else { dev = 0; cur.push_back(smooth); }
        }
        close(s, j);
        i = j + 1;
    }
    std::sort(out.begin(), out.end(), [](const Note& a, const Note& b) { return a.start < b.start; });
    return out;
}

// Noms de notes par lettres, dièses ou bémols.
inline const char* noteName(int pc, bool flats) {
    static const char* const sh[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    static const char* const fl[12] = {"C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"};
    pc = ((pc % 12) + 12) % 12;
    return flats ? fl[pc] : sh[pc];
}
// Écriture cohérente avec l'armure : tonique majeure relative (gammes de atn::scales()), F/Bb/Eb/Ab/Db = bémols,
// G/D/A/E/B = dièses ; C et F#/Gb (armure ambiguë) ou gamme chromatique : préférence de l'utilisateur.
inline bool useFlats(int root, int scale, bool pref) {
    if (root < 0 || scale == 8) return pref;
    static const int relMaj[9] = {0, 3, 3, 10, 8, 3, 0, 3, 0};   // Majeur, Mineur, Min. harm., Dorien, Phrygien, Penta min, Penta maj, Blues, Chrom.
    const int r = ((root + relMaj[std::clamp(scale, 0, 8)]) % 12 + 12) % 12;
    switch (r) {
        case 5: case 10: case 3: case 8: case 1: return true;
        case 7: case 2: case 9: case 4: case 11: return false;
        default: return pref;
    }
}

// Sérialisation binaire compacte (notes ; courbe en suites contiguës de cents int16).
inline void putBytes(std::vector<uint8_t>& b, const void* p, size_t n) { const auto* c = (const uint8_t*)p; b.insert(b.end(), c, c + n); }
inline std::vector<uint8_t> packNotes(const std::vector<Note>& v) {
    std::vector<uint8_t> b; const uint32_t ver = 1, n = (uint32_t)v.size(); putBytes(b, &ver, 4); putBytes(b, &n, 4);
    for (const auto& q : v) { putBytes(b, &q.start, 8); putBytes(b, &q.end, 8); float f[5] = {q.center, q.shift, q.amount, q.expr, q.speedMs}; putBytes(b, f, sizeof f); int32_t e = q.edited; putBytes(b, &e, 4); }
    return b;
}
inline bool unpackNotes(const uint8_t* p, size_t n, std::vector<Note>& v) {
    v.clear(); if (n < 8) return false;
    uint32_t ver, cnt; std::memcpy(&ver, p, 4); std::memcpy(&cnt, p + 4, 4);
    const size_t rec = 8 + 8 + 20 + 4; if (ver != 1 || cnt > (uint32_t)kMaxNotes || n < 8 + cnt * rec) return false;
    for (uint32_t i = 0; i < cnt; ++i) {
        const uint8_t* r = p + 8 + i * rec; Note q; float f[5]; int32_t e;
        std::memcpy(&q.start, r, 8); std::memcpy(&q.end, r + 8, 8); std::memcpy(f, r + 16, 20); std::memcpy(&e, r + 36, 4);
        q.center = f[0]; q.shift = f[1]; q.amount = f[2]; q.expr = f[3]; q.speedMs = f[4]; q.edited = e ? 1 : 0;
        if (!std::isfinite(q.start) || !std::isfinite(q.end) || !std::isfinite(q.center) || !std::isfinite(q.shift)) continue;
        v.push_back(q);
    }
    return true;
}
inline std::vector<uint8_t> packCurve(const std::vector<Frame>& f, double hop) {
    std::vector<uint8_t> b; const uint32_t ver = 1; putBytes(b, &ver, 4); putBytes(b, &hop, 8);
    size_t i = 0;
    while (i < f.size()) {
        size_t j = i + 1; while (j < f.size() && std::fabs(f[j].pos - f[j - 1].pos - hop) < 0.5) ++j;
        const uint32_t cnt = (uint32_t)(j - i); putBytes(b, &f[i].pos, 8); putBytes(b, &cnt, 4);
        for (size_t k = i; k < j; ++k) { const int16_t c = (int16_t)std::lround(std::clamp(f[k].midi, 0.f, 127.f) * 100.f); putBytes(b, &c, 2); }
        i = j;
    }
    return b;
}
inline bool unpackCurve(const uint8_t* p, size_t n, std::vector<Frame>& f) {
    f.clear(); if (n < 12) return false;
    uint32_t ver; double hop; std::memcpy(&ver, p, 4); std::memcpy(&hop, p + 4, 8); if (ver != 1 || !(hop > 0.0)) return false;
    size_t o = 12;
    while (o + 12 <= n && f.size() < (size_t)Capture::kCap) {
        double s; uint32_t cnt; std::memcpy(&s, p + o, 8); std::memcpy(&cnt, p + o + 8, 4); o += 12;
        if (o + (size_t)cnt * 2 > n) return false;
        for (uint32_t k = 0; k < cnt && f.size() < (size_t)Capture::kCap; ++k) { int16_t c; std::memcpy(&c, p + o + k * 2, 2); f.push_back({s + hop * k, c * 0.01f}); }
        o += (size_t)cnt * 2;
    }
    return true;
}

}  // namespace ned
