// Rend la face d'Orbe Galet dans deux PNG. Usage : orbegalet_snapshot a.png b.png
// a : préréglage « Pompe saturée » (3 modules) ; b : « Wobble lent » en tracé En S.
#include "GaletProcessor.h"
#include "GaletEditor.h"
#include "../OrbeDsp/snapshot_util.h"
int main(int argc, char** argv) {
    if (argc < 3) return 2;
    juce::ScopedJuceInitialiser_GUI init;
    GaletProcessor proc; proc.prepareToPlay(48000, 512);
    std::vector<float> v(48000 * 2);
    for (size_t i = 0; i < v.size(); ++i) v[i] = 0.4f * (float)std::sin(2.0 * 3.14159265 * 110.0 * (double)i / 48000.0) + 0.1f * (float)std::sin(2.0 * 3.14159265 * 1320.0 * (double)i / 48000.0);
    proc.setCurrentProgram(13);
    std::unique_ptr<juce::AudioProcessorEditor> ed(proc.createEditor());
    if (auto* ge = dynamic_cast<GaletEditor*>(ed.get())) { ge->setThemeMode(kit::v3::rk::envMode(), false); ge->setUiScale(kit::v3::rk::envScale()); }
    snap::feed(proc, std::vector<float>(v.begin(), v.begin() + 48000 * 3 / 4)); snap::settle(200);
    if (!snap::savePng(*ed, argv[1])) return 1;
    proc.setCurrentProgram(4);
    snap::feed(proc, std::vector<float>(v.begin(), v.begin() + 48000 * 7 / 10)); snap::settle(200);
    return snap::savePng(*ed, argv[2]) ? 0 : 1;
}
