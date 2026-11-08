// Rend la face avant d'Orbe Master dans un PNG. Usage : orbemaster_snapshot sortie.png [eq] (eq = 1 : courbe d'exemple)
#include "MasterProcessor.h"
#include "MasterEditor.h"
int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI init;
    MasterProcessor proc;
    proc.prepareToPlay(48000, 512);
    if (argc > 2) {
        const float g[7] = {2.f, 1.5f, -2.f, 0.f, 1.f, 2.5f, 3.f};
        for (int i = 0; i < 7; ++i) if (auto* p = proc.apvts.getParameter("eq" + juce::String(i))) p->setValueNotifyingHost(p->convertTo0to1(g[i]));
        for (auto* id : {"satOn"}) if (auto* p = proc.apvts.getParameter(id)) p->setValueNotifyingHost(1.f);
        if (auto* p = proc.apvts.getParameter("lGain")) p->setValueNotifyingHost(p->convertTo0to1(6.f));
    }
    juce::AudioBuffer<float> b(2, 512); juce::MidiBuffer m;
    for (int blk = 0; blk < 120; ++blk) { for (int i = 0; i < 512; ++i) { const float t = (float)(blk * 512 + i); const float v = 0.3f * std::sin(0.03f * t) * (0.6f + 0.5f * std::sin(0.0007f * t)) + 0.1f * std::sin(0.4f * t); b.setSample(0, i, v); b.setSample(1, i, 0.9f * v); } proc.processBlock(b, m); }
    std::unique_ptr<juce::AudioProcessorEditor> ed(proc.createEditor());
    ed->setSize(1000, 490);
    for (int i = 0; i < 12; ++i) juce::MessageManager::getInstance()->runDispatchLoopUntil(30);
    auto img = ed->createComponentSnapshot(ed->getLocalBounds(), true, 2.0f);
    juce::File f(argv[1]); f.deleteFile();
    juce::FileOutputStream os(f);
    juce::PNGImageFormat().writeImageToStream(img, os);
    return 0;
}
