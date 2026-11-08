// Rend la face d'Orbe Denoiser dans un PNG. Usage : orbedenoiser_snapshot sortie.png [voix.wav]
#include "DenoiserProcessor.h"
#include "DenoiserEditor.h"
#include "../OrbeDsp/snapshot_util.h"
int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI init;
    DenoiserProcessor proc; proc.prepareToPlay(48000, 512);
    proc.learn();
    snap::feed(proc, snap::noise(48000 * 3, -50.f, 1));  // bruit seul : apprentissage
    auto v = argc > 2 ? snap::loadMono(argv[2], 48000) : std::vector<float>();
    if (v.size() < 48000 * 4) v.assign(48000 * 4, 0.f);
    v.resize(48000 * 4); auto n = snap::noise(v.size(), -50.f, 2);
    for (size_t i = 0; i < v.size(); ++i) { v[i] += n[i] + 0.002f * std::sin(2 * 3.14159f * 50.f * (float)i / 48000.f); }
    std::unique_ptr<juce::AudioProcessorEditor> ed(proc.createEditor());
    ed->setSize(808, 428);
    const size_t half = v.size() / 2;
    snap::feed(proc, std::vector<float>(v.begin(), v.begin() + (long)half)); snap::settle(300);
    snap::feed(proc, std::vector<float>(v.begin() + (long)half, v.end())); snap::settle(200);
    return snap::savePng(*ed, argv[1]) ? 0 : 1;
}
