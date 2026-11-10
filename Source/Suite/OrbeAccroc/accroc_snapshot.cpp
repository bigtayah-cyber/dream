// Rend la face d'Orbe Accroc dans deux PNG. Usage : orbeaccroc_snapshot a.png b.png
// a : préréglage « Chaos total » en cours de jeu ; b : « Radio cassée », pinceau Crush, pad Gel maintenu.
#include "AccrocProcessor.h"
#include "AccrocEditor.h"
#include "../OrbeDsp/snapshot_util.h"
int main(int argc, char** argv) {
    if (argc < 3) return 2;
    juce::ScopedJuceInitialiser_GUI init;
    AccrocProcessor proc; proc.prepareToPlay(48000, 512);
    std::vector<float> v(48000 * 2);
    for (size_t i = 0; i < v.size(); ++i) v[i] = 0.4f * (float)std::sin(2.0 * 3.14159265 * 180.0 * (double)i / 48000.0) * (0.6f + 0.4f * (float)std::sin(2.0 * 3.14159265 * 3.0 * (double)i / 48000.0));
    proc.setCurrentProgram(15);
    std::unique_ptr<juce::AudioProcessorEditor> ed(proc.createEditor());
    ed->setSize(1104, 674);
    snap::feed(proc, std::vector<float>(v.begin(), v.begin() + 48000 * 3 / 4)); snap::settle(200);
    if (!snap::savePng(*ed, argv[1])) return 1;
    proc.setCurrentProgram(13); proc.brush = acr::Crush; proc.pad(acr::Freeze, 1.f);
    snap::feed(proc, std::vector<float>(v.begin(), v.begin() + 48000 * 7 / 10)); snap::settle(200);
    return snap::savePng(*ed, argv[2]) ? 0 : 1;
}
