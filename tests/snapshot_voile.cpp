// Rend l'interface de Voile dans un PNG. Usage : snapshot_voile sortie.png
#include "../Source/PluginEditor.h"
int main(int, char** argv) {
    juce::ScopedJuceInitialiser_GUI init;
    VoileProcessor proc; proc.prepareToPlay(48000, 512);
    juce::AudioBuffer<float> b(2, 512); juce::MidiBuffer m;
    for (int blk = 0; blk < 80; ++blk) { for (int i = 0; i < 512; ++i) { const float v = 0.2f * std::sin(0.058f * (float)(blk * 512 + i)); b.setSample(0, i, v); b.setSample(1, i, v); } proc.processBlock(b, m); }
    std::unique_ptr<juce::AudioProcessorEditor> ed(proc.createEditor());
    ed->setSize(760, 360);
    for (int i = 0; i < 15; ++i) juce::MessageManager::getInstance()->runDispatchLoopUntil(30);
    auto img = ed->createComponentSnapshot(ed->getLocalBounds(), true, 2.0f);
    juce::File f(argv[1]); f.deleteFile(); juce::FileOutputStream os(f);
    juce::PNGImageFormat().writeImageToStream(img, os);
    return 0;
}
