// Aides communes des outils de capture (contrôle visuel hors écran) des effets Orbe. Non inclus dans les plugins.
#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <random>

namespace snap {
// Lit un WAV en mono (moyenne des voies), rééchantillonné à `sr` par interpolation linéaire.
inline std::vector<float> loadMono(const juce::String& path, double sr) {
    juce::AudioFormatManager fm; fm.registerBasicFormats();
    std::vector<float> out;
    if (auto rd = std::unique_ptr<juce::AudioFormatReader>(fm.createReaderFor(juce::File(path)))) {
        juce::AudioBuffer<float> b((int)rd->numChannels, (int)rd->lengthInSamples); rd->read(&b, 0, (int)rd->lengthInSamples, 0, true, true);
        const double ratio = rd->sampleRate / sr; const int n = (int)((double)rd->lengthInSamples / ratio);
        for (int i = 0; i < n; ++i) { const double p = i * ratio; const int i0 = (int)p; const float t = (float)(p - i0); float v = 0;
            for (int c = 0; c < b.getNumChannels(); ++c) v += b.getSample(c, i0) * (1 - t) + b.getSample(c, juce::jmin(i0 + 1, b.getNumSamples() - 1)) * t;
            out.push_back(v / (float)b.getNumChannels()); }
    }
    return out;
}
inline void feed(juce::AudioProcessor& p, const std::vector<float>& x, int block = 512) {
    juce::AudioBuffer<float> b(2, block); juce::MidiBuffer m;
    for (size_t pos = 0; pos + (size_t)block <= x.size(); pos += (size_t)block) {
        for (int i = 0; i < block; ++i) { b.setSample(0, i, x[pos + (size_t)i]); b.setSample(1, i, x[pos + (size_t)i]); }
        p.processBlock(b, m);
    }
}
inline std::vector<float> noise(size_t n, float dbfs, unsigned seed = 1) {
    std::mt19937 r(seed); std::normal_distribution<float> d(0.f, 1.f); const float a = std::pow(10.f, dbfs / 20.f);
    std::vector<float> v(n); for (auto& s : v) s = a * d(r); return v;
}
inline void settle(int ms = 400) { for (int i = 0; i < ms / 30; ++i) juce::MessageManager::getInstance()->runDispatchLoopUntil(30); }
inline bool savePng(juce::Component& c, const char* path, float scale = 2.f) {
    auto img = c.createComponentSnapshot(c.getLocalBounds(), true, scale);
    juce::File f(path); f.deleteFile(); juce::FileOutputStream os(f);
    return os.openedOk() && juce::PNGImageFormat().writeImageToStream(img, os);
}
}  // namespace snap
