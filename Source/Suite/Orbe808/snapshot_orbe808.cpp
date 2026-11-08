// Rend l'éditeur d'Orbe 808 dans un PNG (sans DAW). Usage : orbe808_snapshot sortie.png [largeur] [note]
#include "Orbe808Processor.h"
int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI init;
    Orbe808Processor proc;
    proc.prepareToPlay(48000, 512);
    if (argc > 3) {  // joue une note pour voir les voyants
        juce::MidiBuffer m; m.addEvent(juce::MidiMessage::noteOn(1, juce::String(argv[3]).getIntValue(), 0.9f), 0);
        juce::AudioBuffer<float> b(2, 512); proc.processBlock(b, m);
    }
    std::unique_ptr<juce::AudioProcessorEditor> ed(proc.createEditor());
    const int w = argc > 2 ? juce::String(argv[2]).getIntValue() : 1000;
    ed->setSize(w, (int)(w * 0.632));
    for (int i = 0; i < 5; ++i) juce::MessageManager::getInstance()->runDispatchLoopUntil(30);
    auto img = ed->createComponentSnapshot(ed->getLocalBounds(), true, 2.0f);
    juce::File f(argv[1]);
    f.deleteFile();
    juce::FileOutputStream os(f);
    juce::PNGImageFormat().writeImageToStream(img, os);
    return 0;
}
