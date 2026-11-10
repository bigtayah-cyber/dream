// Rend l'interface de Voile dans un PNG. Usage : snapshot_voile sortie.png [clair|sombre|classique] [échelle]
// Entrée : voix synthétique (mélodie avec vibrato et légers décalages de justesse) pour alimenter l'écran HAUTEUR.
#include "../Source/PluginEditor.h"
int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI init;
    VoileProcessor proc; const double sr = 48000.0; proc.prepareToPlay(sr, 512);
    const juce::String th = argc > 2 ? argv[2] : "clair";
    const float scale = argc > 3 ? (float)std::atof(argv[3]) : 1.f;
    juce::AudioBuffer<float> b(2, 512); juce::MidiBuffer m;
    const float mel[8] = {62, 64, 65, 64, 62, 64, 67, 64};
    double ph = 0; int note = 0, left = 0; float cur = 64.f;
    for (int blk = 0; blk < 420; ++blk) {
        for (int i = 0; i < 512; ++i) {
            if (--left <= 0) { cur = mel[note++ % 8] + (note % 3 - 1) * 0.22f; left = (int)(sr * 0.42); }
            const float vib = 0.15f * std::sin(0.00075f * (float)(blk * 512 + i) * 6.f);
            ph += 2 * 3.14159265358979 * 440.0 * std::pow(2.0, (cur + vib - 69.0) / 12.0) / sr;
            float v = 0; for (int k = 1; k <= 7; ++k) v += std::sin((float)(k * ph)) / (float)k;
            b.setSample(0, i, 0.08f * v); b.setSample(1, i, 0.08f * v);
        }
        proc.processBlock(b, m);
    }
    if (std::getenv("VOILE_SCOPE_DEBUG") != nullptr)
        for (int i = 0; i < VoileProcessor::Scope::N; i += 64) std::printf("%d sung=%.2f out=%.2f tgt=%.0f lvl=%.3f\n", i, proc.scope.sung[i].load(), proc.scope.out[i].load(), proc.scope.tgt[i].load(), proc.scope.lvl[i].load());
    std::unique_ptr<juce::AudioProcessorEditor> ed(proc.createEditor());
    auto* ve = dynamic_cast<VoileEditor*>(ed.get());
    ve->setThemeMode(th == "sombre" ? kit::v3::Mode::Dark : th == "classique" ? kit::v3::Mode::Classic : kit::v3::Mode::Light, false);
    ve->setUiScale(scale);
    for (int i = 0; i < 15; ++i) juce::MessageManager::getInstance()->runDispatchLoopUntil(30);
    auto img = ed->createComponentSnapshot(ed->getLocalBounds(), true, 2.0f);
    juce::File f(argv[1]); f.deleteFile(); juce::FileOutputStream os(f);
    juce::PNGImageFormat().writeImageToStream(img, os);
    return 0;
}
