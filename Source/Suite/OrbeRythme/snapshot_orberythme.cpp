// Rend l'éditeur d'Orbe Rythme dans un PNG (sans DAW). Usage : orberythme_snapshot sortie.png [largeur] [style] [8]
#include "OrbeRythmeProcessor.h"
#include "../../Kit/v3/InstrumentSkin.h"
int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI init;
    kit::v3::snapshotTheme("orberythme");
    OrbeRythmeProcessor proc;
    proc.prepareToPlay(48000, 512);
    auto setP = [&](const char* id, float v) { if (auto* p = proc.apvts.getParameter(id)) p->setValueNotifyingHost(p->convertTo0to1(v)); };
    if (argc > 3) setP("style", (float)juce::String(argv[3]).getIntValue());
    if (argc > 4) setP("bars", 1.f);
    for (int i = 0; i < 4; ++i) juce::MessageManager::getInstance()->runDispatchLoopUntil(40);   // le tirage suit le style au timer
    proc.audition = true;   // quelques blocs pour placer la tête de lecture
    juce::AudioBuffer<float> b(2, 512); juce::MidiBuffer m;
    for (int i = 0; i < 60; ++i) { m.clear(); proc.processBlock(b, m); }
    std::unique_ptr<juce::AudioProcessorEditor> ed(proc.createEditor());
    const int w = argc > 2 ? juce::String(argv[2]).getIntValue() : 1000;
    ed->setSize(w, (int)(w * 580.0 / 1000.0));
    for (int i = 0; i < 5; ++i) juce::MessageManager::getInstance()->runDispatchLoopUntil(30);
    auto img = ed->createComponentSnapshot(ed->getLocalBounds(), true, 2.0f);
    juce::File f(argv[1]);
    f.deleteFile();
    juce::FileOutputStream os(f);
    juce::PNGImageFormat().writeImageToStream(img, os);
    return 0;
}
