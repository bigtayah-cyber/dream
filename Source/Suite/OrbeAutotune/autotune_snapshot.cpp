// Rend la face avant d'Orbe Autotune dans un PNG. Usage : orbeautotune_snapshot sortie.png [auto|manuel|recherche]
#include "AutotuneProcessor.h"
#include "AutotuneEditor.h"
int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI init;
    AutotuneProcessor proc;
    const double sr = 48000.0;
    proc.prepareToPlay(sr, 512);
    const juce::String mode = argc > 2 ? argv[2] : "manuel";
    auto set = [&](const char* id, float v) { if (auto* p = proc.apvts.getParameter(id)) p->setValueNotifyingHost(p->convertTo0to1(v)); };
    if (mode == "manuel") { set("key", 10); set("scale", 1); set("harm", 1); set("x6", 1); }   // A min (key 10 = A), note F# ajoutée
    if (mode == "recherche") set("key", 0);
    const float mel[10] = {69, 72, 76, 74, 71, 67, 72, 69, 76, 72};
    juce::AudioBuffer<float> b(2, 512); juce::MidiBuffer m;
    double ph = 0; int note = 0, left = 0; float cur = 69.f;
    for (int blk = 0; blk < 560; ++blk) {
        for (int i = 0; i < 512; ++i) {
            if (--left <= 0) { cur = mel[note++ % 10] + (note % 3 - 1) * 0.28f; left = (int)(sr * 0.32); }
            const float vib = 0.18f * std::sin(0.00075f * (float)(blk * 512 + i) * 6.f);
            ph += 2 * 3.14159265358979 * 440.0 * std::pow(2.0, (cur + vib - 69.0) / 12.0) / sr;
            float v = 0; for (int k = 1; k <= 7; ++k) v += std::sin((float)(k * ph)) / (float)k;
            b.setSample(0, i, 0.22f * v); b.setSample(1, i, 0.22f * v);
        }
        proc.processBlock(b, m);
    }
    std::unique_ptr<juce::AudioProcessorEditor> ed(proc.createEditor());
    ed->setSize(1000, 566);
    for (int i = 0; i < 12; ++i) juce::MessageManager::getInstance()->runDispatchLoopUntil(30);
    auto img = ed->createComponentSnapshot(ed->getLocalBounds(), true, 1.5f);
    juce::File f(argv[1]); f.deleteFile();
    juce::FileOutputStream os(f);
    juce::PNGImageFormat().writeImageToStream(img, os);
    return 0;
}
