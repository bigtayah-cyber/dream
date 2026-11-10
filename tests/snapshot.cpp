// Rend l'éditeur d'un plugin dans un PNG (sans DAW) pour contrôle visuel. Usage : snapshot sortie.png [largeur] [aleatoire]
#include "../Source/Orbe/OrbeProcessor.h"
#include "../Source/Kit/v3/InstrumentSkin.h"
#include "../Source/Orbe/OrbeEditor.h"
int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI init;
    kit::v3::snapshotTheme("orbe");
    OrbeProcessor proc;
    if (argc > 3) proc.generate(OrbeProcessor::Gen::All);
    proc.prepareToPlay(48000, 512);
    std::unique_ptr<juce::AudioProcessorEditor> ed(proc.createEditor());
    const int w = argc > 2 ? juce::String(argv[2]).getIntValue() : 1000;
    ed->setSize(w, (int)(w * 0.64));
    for (int i = 0; i < 5; ++i) juce::MessageManager::getInstance()->runDispatchLoopUntil(30);
    auto img = ed->createComponentSnapshot(ed->getLocalBounds(), true, 2.0f);
    juce::File f(argv[1]);
    f.deleteFile();
    juce::FileOutputStream os(f);
    juce::PNGImageFormat().writeImageToStream(img, os);
    return 0;
}
