// Contournement honoré par processBlock : sortie = entrée retardée de la latence déclarée, fondu croisé de 10 ms au basculement.
// Header-only, sans JUCE. Le traitement continue de tourner pendant le contournement (les queues de reverb/delay, les enveloppes
// et les mémoires de filtre restent à jour : pas de trou ni de claquement quand on réactive l'effet).
#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

namespace bypassfx {

class Fader {
public:
    // latency : latence déclarée au PluginProcessor ; maxBlock : taille de tranche (aucune allocation ensuite) ; fadeSec : durée du fondu.
    void prepare(double sampleRate, int latency, int maxBlock, double fadeSec = 0.010) {
        lat = std::max(0, latency); cap = std::max(1, maxBlock);
        size_t sz = 1; while (sz < (size_t)lat + 1) sz <<= 1;
        mask = sz - 1; w = 0;
        for (auto& r : ring) r.assign(sz, 0.f);
        for (auto& d : dry) d.assign((size_t)cap, 0.f);
        step = (float)(1.0 / std::max(1.0, fadeSec * sampleRate));
        m = 0.f; primed = false;
    }
    int latency() const { return lat; }
    bool settledBypassed() const { return primed && m >= 1.f; }

    // Traite `n` échantillons sur place. l/r : voies du tampon ; r peut être nul (une seule voie). stereoIn faux : l'entrée est mono,
    // la voie droite du tampon est ignorée (le contournement la remplace par la gauche). process(float* l, float* r, int n) fait le
    // travail normal d'un bloc d'au plus `maxBlock` échantillons, sur place.
    template <class F> void run(float* l, float* r, int n, bool stereoIn, bool bypassed, F&& process) {
        if (cap == 0 || ring[0].empty()) { process(l, r, n); return; }   // non préparé : traitement direct
        for (int p = 0; p < n; p += cap) chunk(l + p, r ? r + p : nullptr, std::min(cap, n - p), stereoIn, bypassed, process);
    }

private:
    template <class F> void chunk(float* l, float* r, int n, bool stereoIn, bool bypassed, F& process) {
        float* dl = dry[0].data(); float* dr = dry[1].data();
        for (int i = 0; i < n; ++i) {   // entrée retardée de `lat` échantillons (exact), capturée avant que le traitement ne l'écrase
            const float x0 = l[i], x1 = (stereoIn && r) ? r[i] : x0;
            ring[0][w] = x0; ring[1][w] = x1;
            const size_t rd = (w - (size_t)lat) & mask;
            dl[i] = ring[0][rd]; dr[i] = ring[1][rd];
            w = (w + 1) & mask;
        }
        const float target = bypassed ? 1.f : 0.f;
        if (!primed) { m = target; primed = true; }   // état initial restauré par l'hôte : pas de fondu au chargement
        process(l, r, n);                              // toujours : les mémoires du traitement restent chaudes
        if (m == 0.f && target == 0.f) return;
        if (m == 1.f && target == 1.f) { std::copy(dl, dl + n, l); if (r) std::copy(dr, dr + n, r); return; }
        for (int i = 0; i < n; ++i) {
            m = m < target ? std::min(target, m + step) : std::max(target, m - step);
            if (m >= 1.f) { l[i] = dl[i]; if (r) r[i] = dr[i]; }
            else { l[i] = l[i] * (1.f - m) + dl[i] * m; if (r) r[i] = r[i] * (1.f - m) + dr[i] * m; }
        }
    }

    std::vector<float> ring[2], dry[2];
    size_t mask = 0, w = 0; int lat = 0, cap = 0; float step = 0.001f, m = 0.f; bool primed = false;
};

}  // namespace bypassfx
