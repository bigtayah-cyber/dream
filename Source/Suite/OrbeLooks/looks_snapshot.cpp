// Rend la face avant d'Orbe Looks dans un PNG. Usage : orbelooks_snapshot sortie.png [look 0-26] [largeur] [clair|sombre|classique]
#include "LooksProcessor.h"
#include "LooksEditor.h"
int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI init;
    LooksProcessor proc;
    proc.prepareToPlay(48000, 512);
    if (argc > 2) proc.applyLook(juce::String(argv[2]).getIntValue());
    juce::AudioBuffer<float> b(2, 512); juce::MidiBuffer m;
    for (int blk = 0; blk < 120; ++blk) { for (int i = 0; i < 512; ++i) { const float v = 0.18f * std::sin(0.06f * (float)(blk * 512 + i)) * (1.f + 2.f * std::sin(0.0004f * (float)(blk * 512 + i))); b.setSample(0, i, v); b.setSample(1, i, v); } proc.processBlock(b, m); }
    std::unique_ptr<juce::AudioProcessorEditor> ed(proc.createEditor());
    const int w = argc > 3 ? juce::String(argv[3]).getIntValue() : 460;
    const juce::String th = argc > 4 ? argv[4] : "classique";   // sans argument : rendu d'origine
    auto* le = dynamic_cast<LooksEditor*>(ed.get());
    le->setThemeMode(th == "sombre" ? kit::v3::Mode::Dark : th == "clair" ? kit::v3::Mode::Light : kit::v3::Mode::Classic, false);
    if (th == "classique") ed->setSize(w, juce::roundToInt(w * 560.0 / 460.0)); else le->setUiScale(1.f);
    for (int i = 0; i < 12; ++i) juce::MessageManager::getInstance()->runDispatchLoopUntil(30);
    auto img = ed->createComponentSnapshot(ed->getLocalBounds(), true, 2.0f);
    juce::File f(argv[1]); f.deleteFile();
    juce::FileOutputStream os(f);
    juce::PNGImageFormat().writeImageToStream(img, os);
    return 0;
}
