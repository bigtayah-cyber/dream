// Rend l'éditeur d'Orbe Beat dans un PNG (sans DAW). Usage : orbebeat_snapshot sortie.png [largeur] [kit] [32]
#include "OrbeBeatProcessor.h"
int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI init;
    OrbeBeatProcessor proc;
    proc.prepareToPlay(48000, 512);
    auto setP = [&](const char* id, float v) { if (auto* p = proc.apvts.getParameter(id)) p->setValueNotifyingHost(p->convertTo0to1(v)); };
    if (argc > 3) { setP("kit", (float)juce::String(argv[3]).getIntValue()); }
    if (argc > 4) setP("steps", 1.f);
    if (argc > 3) { proc.generateCurrent(); }
    proc.audition = true;  // joue quelques blocs pour allumer les voyants et la tête de lecture
    juce::AudioBuffer<float> b(2, 512); juce::MidiBuffer m;
    for (int i = 0; i < 40; ++i) { m.clear(); proc.processBlock(b, m); }
    std::unique_ptr<juce::AudioProcessorEditor> ed(proc.createEditor());
    const int w = argc > 2 ? juce::String(argv[2]).getIntValue() : 1100;
    ed->setSize(w, (int)(w * 640.0 / 1100.0));
    for (int i = 0; i < 5; ++i) juce::MessageManager::getInstance()->runDispatchLoopUntil(30);
    auto img = ed->createComponentSnapshot(ed->getLocalBounds(), true, 2.0f);
    juce::File f(argv[1]);
    f.deleteFile();
    juce::FileOutputStream os(f);
    juce::PNGImageFormat().writeImageToStream(img, os);
    return 0;
}
