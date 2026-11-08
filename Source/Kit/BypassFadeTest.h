// Mesures communes des tests console du contournement (sans JUCE). Utilise le même Fader que les processBlock.
#pragma once
#include <cmath>
#include <functional>
#include <random>
#include <vector>
#include "BypassFade.h"

namespace bypassfx {
struct Measure { double exactDb = -400, jumpDb = 0, plainDb = 0; };

// reset() ramène le moteur à zéro ; engine(l, r|nullptr, n) traite un bloc sur place comme le fait processBlock (r nul si entrée mono).
// exactDb : erreur max (dBFS) entre la sortie en contournement et l'entrée retardée de `latency` (blocs de tailles variées, stéréo puis mono).
// jumpDb  : plus grand écart d'un échantillon au suivant (dBFS) autour des deux basculements, sinus lent de -26 dBFS.
inline Measure measure(double sr, int latency, int maxBlock, const std::function<void()>& reset, const std::function<void(float*, float*, int)>& engine) {
    Measure out;
    const int N = (int)sr;
    std::mt19937 rng(11); std::uniform_real_distribution<float> u(-0.5f, 0.5f);
    for (int mono = 0; mono < 2; ++mono) {
        reset(); Fader f; f.prepare(sr, latency, maxBlock);
        std::vector<float> xl((size_t)N), xr((size_t)N), l, r;
        for (int i = 0; i < N; ++i) { xl[(size_t)i] = u(rng); xr[(size_t)i] = u(rng); }
        l = xl; r = xr;
        const int sizes[4] = {333, 480, 64, 1000}; int k = 0;
        for (int p = 0; p < N; k++) {
            const int c = std::min(sizes[k & 3], N - p);
            f.run(l.data() + p, r.data() + p, c, !mono, true, [&](float* a, float* b, int n) { engine(a, mono ? nullptr : b, n); if (mono) std::copy(a, a + n, b); });
            p += c;
        }
        double err = 0;
        for (int i = 0; i < N; ++i) {
            const float eL = i >= latency ? xl[(size_t)(i - latency)] : 0.f, eR = i >= latency ? (mono ? xl[(size_t)(i - latency)] : xr[(size_t)(i - latency)]) : 0.f;
            err = std::max({err, (double)std::fabs(l[(size_t)i] - eL), (double)std::fabs(r[(size_t)i] - eR)});
        }
        out.exactDb = std::max(out.exactDb, 20 * std::log10(err + 1e-30));
    }
    {   // basculements pendant un signal lent : actif -> contourné (0,5 s) -> actif (1,0 s)
        reset(); Fader f; f.prepare(sr, latency, maxBlock);
        const int T = (int)(1.5 * sr); std::vector<float> l((size_t)T), r;
        for (int i = 0; i < T; ++i) l[(size_t)i] = 0.05f * (float)std::sin(2 * 3.14159265358979 * 50.0 * i / sr);
        r = l;
        double worst = 0, wp = 0;
        for (int p = 0; p < T;) {
            const int c = std::min(256, T - p), cur = p;
            const bool by = (cur >= (int)(0.5 * sr) && cur < (int)(1.0 * sr));
            f.run(l.data() + p, r.data() + p, c, true, by, [&](float* a, float* b, int n) { engine(a, b, n); });
            p += c;
        }
        for (int i = (int)(0.3 * sr); i < T; ++i) {
            const double d = std::fabs(l[(size_t)i] - l[(size_t)i - 1]);
            if (i >= (int)(0.5 * sr) - 4 && i < (int)(0.55 * sr)) worst = std::max(worst, d);
            if (i >= (int)(1.0 * sr) - 4 && i < (int)(1.05 * sr)) worst = std::max(worst, d);
            wp = std::max(wp, d);
        }
        out.jumpDb = 20 * std::log10(worst + 1e-30); out.plainDb = 20 * std::log10(wp + 1e-30);
    }
    return out;
}
}  // namespace bypassfx
