// Rend la face avant d'Orbe Cabine dans un PNG. Usage : orbecabine_snapshot sortie.png [preset 0-10] [wav]
#include "CabineProcessor.h"
#include "CabineEditor.h"
int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI init;
    CabineProcessor proc;
    proc.prepareToPlay(48000, 512);
    if (argc > 2) proc.applyPreset(juce::String(argv[2]).getIntValue());
    juce::AudioBuffer<float> b(2, 512); juce::MidiBuffer m;
    for (int blk = 0; blk < 60; ++blk) { for (int i = 0; i < 512; ++i) { const float v = 0.18f * std::sin(0.06f * (float)(blk * 512 + i)) * (1.f + 2.f * std::sin(0.0004f * (float)(blk * 512 + i))); b.setSample(0, i, v); b.setSample(1, i, v); } proc.processBlock(b, m); }
    std::unique_ptr<juce::AudioProcessorEditor> ed(proc.createEditor());
    ed->setSize(400, 760);
    for (int i = 0; i < 12; ++i) juce::MessageManager::getInstance()->runDispatchLoopUntil(30);
    auto img = ed->createComponentSnapshot(ed->getLocalBounds(), true, 2.0f);
    juce::File f(argv[1]); f.deleteFile();
    juce::FileOutputStream os(f);
    juce::PNGImageFormat().writeImageToStream(img, os);
    return 0;
}
