// Rend la face d'Orbe Relance dans un PNG. Usage : orberelance_snapshot sortie.png [clair|sombre|classique] [échelle]
// Voix rap synthétique (phrases et silences), tête de lecture à 120 BPM, préréglage « Backeur trap » : relances visibles.
#include "RelanceProcessor.h"
#include "RelanceEditor.h"
struct FakeHead : juce::AudioPlayHead {
    juce::int64 pos = 0;
    juce::Optional<PositionInfo> getPosition() const override {
        PositionInfo p; p.setTimeInSamples(pos); p.setIsPlaying(true); p.setBpm(120.0); p.setPpqPosition((double)pos / 48000.0 * 2.0); return p;
    }
};
int main(int argc, char** argv) {
    if (argc < 2) return 2;
    juce::ScopedJuceInitialiser_GUI init;
    RelanceProcessor proc;
    const double sr = 48000.0;
    proc.prepareToPlay(sr, 512);
    proc.setCurrentProgram(8);
    FakeHead head; proc.setPlayHead(&head);
    juce::AudioBuffer<float> b(2, 512); juce::MidiBuffer m;
    double ph = 0.0;
    const double cyc[4] = {1.1, 0.7, 1.4, 0.9};
    for (int blk = 0; blk < 560; ++blk) {
        for (int i = 0; i < 512; ++i) {
            const double t = (double)(blk * 512 + i) / sr;
            double acc = 0.0, tp = 0.0; int c = 0; bool on = false;
            while (true) { const double len = cyc[c % 4]; if (t < acc + len) { tp = t - acc; on = tp < len - 0.45; break; } acc += len; ++c; }
            float v = 0.f;
            if (on) {
                ph += 2.0 * 3.14159265 * (130.0 + 25.0 * std::sin(t * 2.1)) / sr;
                double s = 0.0; for (int k = 1; k <= 8; ++k) s += std::sin(k * ph) / k;
                v = (float)(0.3 * s * (0.5 + 0.5 * std::sin(3.14159265 * std::fmod(tp, 0.11) / 0.11)));
            }
            b.setSample(0, i, v); b.setSample(1, i, v);
        }
        head.pos = (juce::int64)blk * 512;
        proc.processBlock(b, m);
    }
    std::unique_ptr<juce::AudioProcessorEditor> ed(proc.createEditor());
    auto* re = dynamic_cast<RelanceEditor*>(ed.get());
    const juce::String th = argc > 2 ? argv[2] : "clair";
    re->setThemeMode(th == "sombre" ? kit::v3::Mode::Dark : th == "classique" ? kit::v3::Mode::Classic : kit::v3::Mode::Light, false);
    re->setUiScale(argc > 3 ? (float)std::atof(argv[3]) : 1.f);
    for (int i = 0; i < 12; ++i) juce::MessageManager::getInstance()->runDispatchLoopUntil(30);
    auto img = ed->createComponentSnapshot(ed->getLocalBounds(), true, 2.0f);
    juce::File f(argv[1]); f.deleteFile();
    juce::FileOutputStream os(f);
    return juce::PNGImageFormat().writeImageToStream(img, os) ? 0 : 1;
}
