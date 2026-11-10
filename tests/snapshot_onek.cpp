// Rend la face avant d'un plugin à un potard dans un PNG (×2). Usage : snapshot_onek sortie.png indice(0-3) [potard 0-10] [volet 0/1] [largeur] [clair|sombre|classique] [échelle v3]
#include "../Source/OneKnob/OneKnobProcessor.h"
#include "../Source/OneKnob/OneKnobEditor.h"
int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI init;
    OneKnobProcessor::index() = argc > 2 ? juce::String(argv[2]).getIntValue() : 0;
    OneKnobProcessor proc;
    proc.prepareToPlay(48000, 512);
    if (argc > 3) if (auto* p = proc.apvts.getParameter("char")) p->setValueNotifyingHost(p->convertTo0to1(juce::String(argv[3]).getFloatValue()));
    // un peu de signal pour animer le VU
    juce::AudioBuffer<float> b(2, 512); juce::MidiBuffer m;
    for (int blk = 0; blk < 60; ++blk) { for (int i = 0; i < 512; ++i) { const float v = 0.22f * std::sin(0.06f * (float)(blk * 512 + i)); b.setSample(0, i, v); b.setSample(1, i, v); } proc.processBlock(b, m); }
    if (argc > 4) proc.apvts.state.setProperty("adv", juce::String(argv[4]).getIntValue() != 0, nullptr);
    if (argc > 5) proc.apvts.state.setProperty("uiW", juce::String(argv[5]).getIntValue(), nullptr);
    std::unique_ptr<juce::AudioProcessorEditor> ed(proc.createEditor());
    if (auto* oe = dynamic_cast<OneKnobEditor*>(ed.get())) {   // sans argument : rendu d'origine (comparaisons existantes)
        const juce::String th = argc > 6 ? argv[6] : "classique";
        oe->setThemeMode(th == "sombre" ? kit::v3::Mode::Dark : th == "clair" ? kit::v3::Mode::Light : kit::v3::Mode::Classic, false);
        if (th != "classique") oe->setUiScale(argc > 7 ? (float)std::atof(argv[7]) : 1.f);
        if (std::getenv("SNAP_DEBUG")) std::printf("taille %d x %d\n", ed->getWidth(), ed->getHeight());
    }
    for (int i = 0; i < 12; ++i) juce::MessageManager::getInstance()->runDispatchLoopUntil(30);
    if (std::getenv("SNAP_DEBUG")) std::printf("taille finale %d x %d\n", ed->getWidth(), ed->getHeight());
    auto img = ed->createComponentSnapshot(ed->getLocalBounds(), true, 2.0f);
    juce::File f(argv[1]); f.deleteFile();
    juce::FileOutputStream os(f);
    juce::PNGImageFormat().writeImageToStream(img, os);
    return 0;
}
