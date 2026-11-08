// Rend l'éditeur d'Orbe Synth dans un PNG (sans DAW). Usage : orbesynth_snapshot sortie.png [largeur] [preset 0-9]
#include "OrbeSynthProcessor.h"
#include "OrbeSynthEditor.h"
int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI init;
    OrbeSynthProcessor proc;
    proc.prepareToPlay(48000, 512);
    if (argc > 3) proc.setCurrentProgram(juce::String(argv[3]).getIntValue());
    proc.mod = .35f;
    juce::AudioBuffer<float> b(2, 512); juce::MidiBuffer m;
    for (int n : {48, 55, 60, 64, 67}) m.addEvent(juce::MidiMessage::noteOn(1, n, (juce::uint8)100), 0);
    for (int blk = 0; blk < 20; ++blk) { proc.processBlock(b, m); m.clear(); }
    std::unique_ptr<juce::AudioProcessorEditor> ed(proc.createEditor());
    const int w = argc > 2 ? juce::String(argv[2]).getIntValue() : 1100;
    ed->setSize(w, (int)(w * 660.0 / 1100.0));
    for (int i = 0; i < 10; ++i) juce::MessageManager::getInstance()->runDispatchLoopUntil(30);
    auto img = ed->createComponentSnapshot(ed->getLocalBounds(), true, 1.5f);
    juce::File f(argv[1]); f.deleteFile();
    juce::FileOutputStream os(f);
    juce::PNGImageFormat().writeImageToStream(img, os);
    return 0;
}
