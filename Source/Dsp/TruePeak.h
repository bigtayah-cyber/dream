// Pic vrai (true peak) : détecteur unique, partagé par Orbe Master (limiteur) et Orbe Mix (mesure). C++17 pur, sans JUCE.
//
// Suréchantillonnage ×4 polyphasé (3 phases intermédiaires de 48 coefficients, fenêtre de Kaiser), puis affinage parabolique
// autour de chaque maximum local de la grille ×4. Erreur mesurée contre un sinc ×16 : sinus de 1 à 22 kHz, toutes phases,
// au plus -0,04 dB (sous-estimation) ; bruit blanc à pleine bande (cas extrême, irréaliste en musique) au plus -0,3 dB.
// L'ancienne mesure de Mix (interpolation cubique) sous-estimait jusqu'à 1,1 dB vers 12 kHz.
#pragma once
#include <algorithm>
#include <cmath>

namespace dsp {

struct TruePeak {
    static constexpr int T = 48, kLag = T / 2;   // la crête du segment [c, c+1] est connue kLag échantillons après c
    static constexpr int kRing = 64;             // puissance de 2 >= T
    float co[3][T] = {};
    float x[2][kRing] = {}; int w = 0; float prevIp[2] = {0, 0};
    TruePeak() {
        constexpr double kPi = 3.14159265358979323846, beta = 9.0;
        auto i0 = [](double v) { double s = 1, t = 1; for (int k = 1; k < 40; ++k) { t *= (v / (2 * k)) * (v / (2 * k)); s += t; } return s; };
        for (int p = 0; p < 3; ++p) {
            const double f = (p + 1) / 4.0; double sum = 0;
            for (int j = 0; j < T; ++j) {
                const double d = (j - (T / 2 - 1)) - f, s = std::sin(kPi * d) / (kPi * d), r = d / (T / 2.0);
                co[p][j] = (float)(s * i0(beta * std::sqrt(std::max(0.0, 1 - r * r))) / i0(beta)); sum += co[p][j];
            }
            for (int j = 0; j < T; ++j) co[p][j] = (float)(co[p][j] / sum);
        }
    }
    void reset() { for (auto& c : x) for (auto& v : c) v = 0; w = 0; prevIp[0] = prevIp[1] = 0; }
    // Entrée : échantillon courant (deux voies ; pour une source mono, passer deux fois le même).
    // Sortie : crête vraie (valeur absolue) de l'échantillon c = n - kLag, voies liées.
    float push(float a, float b) {
        const float in[2] = {a, b}; float best = 0.f;
        for (int ch = 0; ch < 2; ++ch) {
            x[ch][w & (kRing - 1)] = in[ch];
            float v[5] = {x[ch][(w - kLag) & (kRing - 1)], 0, 0, 0, x[ch][(w - kLag + 1) & (kRing - 1)]};
            for (int p = 0; p < 3; ++p) { float s = 0.f; for (int j = 0; j < T; ++j) s += co[p][j] * x[ch][(w - (T - 1) + j) & (kRing - 1)]; v[p + 1] = s; }
            float ip = 0.f;
            for (int k = 0; k < 5; ++k) ip = std::max(ip, std::fabs(v[k]));
            for (int k = 1; k < 4; ++k) {
                const float sg = v[k] < 0 ? -1.f : 1.f, lo = sg * v[k - 1], mid = sg * v[k], hi = sg * v[k + 1], den = 2 * mid - lo - hi;
                if (mid >= lo && mid >= hi && den > 1e-9f) ip = std::max(ip, std::min(mid * 1.1f, mid + (hi - lo) * (hi - lo) / (8 * den)));
            }
            best = std::max(best, std::max(std::fabs(v[0]), std::max(ip, prevIp[ch])));
            prevIp[ch] = ip;
        }
        ++w; return best;
    }
};

}  // namespace dsp
