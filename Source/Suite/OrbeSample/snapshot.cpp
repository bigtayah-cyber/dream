// Rend l'éditeur d'Orbe Sample dans un PNG (sans DAW). Usage : orbe_sample_snapshot sortie.png [largeur] [préréglage]
#include "SampleEditor.h"
int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI init;
    SampleProcessor proc;
    proc.prepareToPlay(48000, 512);
    if (argc > 1 && juce::String(argv[1]) == "--etat") {  // aller-retour de l'état : l'audio embarqué doit revenir à l'identique
        juce::MemoryBlock mb; proc.getStateInformation(mb);
        SampleProcessor q; q.setStateInformation(mb.getData(), (int)mb.getSize());
        const auto a = proc.holder.get(), b = q.holder.get();
        const bool same = a && b && a->size() == b->size() && std::fabs(a->ch[0][1000] - b->ch[0][1000]) < 1e-3f && q.holder.name() == proc.holder.name();
        std::printf("etat : %d octets, echantillon %s, reperes %s\n", (int)mb.getSize(), same ? "identique" : "DIFFERENT", (true) ? "identiques" : "DIFFERENTS");
        return same ? 0 : 1;
    }
    std::unique_ptr<juce::AudioProcessorEditor> ed(proc.createEditor());
    const int w = argc > 2 ? juce::String(argv[2]).getIntValue() : 1000;
    ed->setSize(w, (int)(w * 0.65));
    if (argc > 3) proc.applyPreset(juce::String(argv[3]).getIntValue());
    proc.audition = true;
    juce::AudioBuffer<float> b(2, 512); juce::MidiBuffer m;
    for (int i = 0; i < 40; ++i) { proc.processBlock(b, m); juce::MessageManager::getInstance()->runDispatchLoopUntil(60); }
    auto img = ed->createComponentSnapshot(ed->getLocalBounds(), true, 2.0f);
    juce::File f(argv[1]);
    f.deleteFile();
    juce::FileOutputStream os(f);
    juce::PNGImageFormat().writeImageToStream(img, os);
    return 0;
}
