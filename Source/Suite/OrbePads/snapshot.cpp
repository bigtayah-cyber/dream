// Rend l'éditeur d'Orbe Pads dans un PNG (sans DAW). Usage : orbe_pads_snapshot sortie.png [largeur] [pad_selectionne]
#include "PadsEditor.h"
#include "../../Kit/v3/InstrumentSkin.h"
int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI init;
    kit::v3::snapshotTheme("orbepads");
    kit::v3::Prefs::setFlats("orbepads", std::getenv("FLATS") != nullptr);   // bémols (réglage d'interface)
    PadsProcessor proc;
    proc.prepareToPlay(48000, 512);
    if (argc > 1 && juce::String(argv[1]) == "--etat") {  // aller-retour de l'état : l'audio embarqué doit revenir à l'identique
        juce::MemoryBlock mb; proc.getStateInformation(mb);
        PadsProcessor q; q.setStateInformation(mb.getData(), (int)mb.getSize());
        const auto a = proc.holder.get(), b = q.holder.get();
        const bool same = a && b && a->size() == b->size() && std::fabs(a->ch[0][1000] - b->ch[0][1000]) < 1e-3f && q.holder.name() == proc.holder.name();
        std::printf("etat : %d octets, echantillon %s, reperes %s\n", (int)mb.getSize(), same ? "identique" : "DIFFERENT", (proc.marksCopy().size() == q.marksCopy().size()) ? "identiques" : "DIFFERENTS");
        return same ? 0 : 1;
    }
    std::unique_ptr<juce::AudioProcessorEditor> ed(proc.createEditor());
    const int w = argc > 2 ? juce::String(argv[2]).getIntValue() : 1000;
    ed->setSize(w, (int)(w * 0.66));
    if (argc > 3) static_cast<PadsEditor*>(ed.get())->selectPad(juce::String(argv[3]).getIntValue());
    proc.padDown(5, 0.9f);  // un pad actif pour voir les voyants
    juce::AudioBuffer<float> b(2, 512); juce::MidiBuffer m;
    for (int i = 0; i < 12; ++i) { proc.processBlock(b, m); juce::MessageManager::getInstance()->runDispatchLoopUntil(40); }
    auto img = ed->createComponentSnapshot(ed->getLocalBounds(), true, 2.0f);
    juce::File f(argv[1]);
    f.deleteFile();
    juce::FileOutputStream os(f);
    juce::PNGImageFormat().writeImageToStream(img, os);
    return 0;
}
