// Rend la face d'Orbe DeEsser dans un PNG. Usage : orbedeesser_snapshot sortie.png [voix.wav|-] [clair|sombre|classique]
#include "DeEsserProcessor.h"
#include "DeEsserEditor.h"
#include "../OrbeDsp/snapshot_util.h"
int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI init;
    DeEsserProcessor proc; proc.prepareToPlay(48000, 512);
    auto v = argc > 2 ? snap::loadMono(argv[2], 48000) : std::vector<float>();
    if (v.size() < 48000 * 3) v.assign(48000 * 3, 0.f);
    v.resize(48000 * 3);
    // sibilances synthétiques (bruit filtré 5-9 kHz) ajoutées à la prise
    auto n = snap::noise(v.size(), -22.f, 4);
    float lp = 0, hp = 0;
    for (size_t i = 0; i < v.size(); ++i) { lp += 0.55f * (n[i] - lp); hp += 0.25f * (lp - hp); const float env = (std::fmod((float)i / 48000.f, 0.6f) < 0.18f) ? 1.f : 0.f; v[i] += env * (lp - hp) * 2.2f; }
    std::unique_ptr<juce::AudioProcessorEditor> ed(proc.createEditor());
    const juce::String th = argc > 3 ? argv[3] : "classique";   // sans argument : rendu d'origine
    auto* de = dynamic_cast<DeEsserEditor*>(ed.get());
    de->setThemeMode(th == "sombre" ? kit::v3::Mode::Dark : th == "clair" ? kit::v3::Mode::Light : kit::v3::Mode::Classic, false);
    if (th == "classique") ed->setSize(760, 462); else de->setUiScale(1.f);
    snap::feed(proc, v); snap::settle(300);
    snap::feed(proc, std::vector<float>(v.begin() + 48000 / 2, v.begin() + 48000 / 2 + 48000 / 2)); snap::settle(100);
    return snap::savePng(*ed, argv[1]) ? 0 : 1;
}
