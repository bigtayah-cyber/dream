// Garde d'entrée commune : un plugin placé avant peut envoyer un NaN ou un infini (glitch, plugin défaillant).
// Un seul de ces échantillons suffit à empoisonner pour toujours les mémoires des filtres, reverbs et compresseurs.
// sanitize() les remplace par 0 avant tout traitement. Test sur les bits (insensible à -ffast-math), sans JUCE, sans allocation.
#pragma once
#include <cstdint>
#include <cstring>

namespace kit {

inline bool finiteBits(float x) { std::uint32_t u; std::memcpy(&u, &x, sizeof u); return (u & 0x7f800000u) != 0x7f800000u; }

// Remplace sur place les échantillons non finis par 0. Renvoie vrai si au moins un échantillon a été remplacé.
inline bool sanitize(float* x, int n) {
    bool bad = false;
    if (x == nullptr) return false;
    for (int i = 0; i < n; ++i) if (!finiteBits(x[i])) { x[i] = 0.f; bad = true; }
    return bad;
}

}  // namespace kit
