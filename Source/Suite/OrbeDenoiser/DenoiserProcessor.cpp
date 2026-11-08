#include "DenoiserProcessor.h"
#include "DenoiserEditor.h"

using kit::u8;
using namespace of;

juce::AudioProcessorValueTreeState::ParameterLayout DenoiserProcessor::makeLayout() {
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    const dn::Params d;
    l.add(fparam("red", u8("R\xC3\xA9" "duction"), 0.f, 24.f, 0.1f, d.reduction, [](float v) { return String(v, 1) + " dB"; }));
    l.add(fparam("sens", "Seuil", 0.f, 1.f, 0.01f, d.sens, fmtPct));
    l.add(fparam("smooth", "Lissage", 0.f, 1.f, 0.01f, d.smooth, fmtPct));
    l.add(fparam("air", "Aigus", 0.f, 1.f, 0.01f, d.air, fmtPct));
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{"hum", 1}, "Anti-ronflement", StringArray{u8("Coup\xC3\xA9"), "50 Hz", "60 Hz"}, 0));
    l.add(std::make_unique<AudioParameterBool>(ParameterID{"adapt", 1}, "Adaptatif", d.adaptive));
    l.add(std::make_unique<AudioParameterBool>(ParameterID{"listen", 1}, u8("\xC3\x89" "couter le bruit retir\xC3\xA9"), false));
    addBypass(l);
    return l;
}

DenoiserProcessor::DenoiserProcessor() : FxProcessor("Orbe Denoiser", "ORBEDENOISER", makeLayout()) {}

dn::Params DenoiserProcessor::readParams() const {
    dn::Params p;
    p.reduction = raw("red"); p.sens = raw("sens"); p.smooth = raw("smooth"); p.air = raw("air");
    p.hum = juce::roundToInt(raw("hum")); p.adaptive = raw("adapt") > 0.5f; p.listen = raw("listen") > 0.5f;
    return p;
}

void DenoiserProcessor::prepareToPlay(double sr, int block) {
    maxBlock = juce::jmax(512, block);
    engine.prepare(sr);
    prepareBypass(sr, engine.latency(), maxBlock);
    setLatencySamples(engine.latency());
}

void DenoiserProcessor::processBlock(juce::AudioBuffer<float>& b, juce::MidiBuffer&) {
    juce::ScopedNoDenormals nd;
    engine.set(readParams());
    stereo(b, maxBlock, [this](const float* il, const float* ir, float* ol, float* or_, int n) { engine.process(il, ir, ol, or_, n); });
}

// Le profil de bruit appris est enregistré avec la session (base64 des 1025 bins).
void DenoiserProcessor::beforeSave() {
    std::vector<float> v; engine.getProfile(v);
    if (engine.profileKnown()) apvts.state.setProperty("profile", juce::MemoryBlock(v.data(), v.size() * sizeof(float)).toBase64Encoding(), nullptr);
    else apvts.state.removeProperty("profile", nullptr);
}
void DenoiserProcessor::afterLoad() {
    juce::MemoryBlock mb;
    if (mb.fromBase64Encoding(apvts.state.getProperty("profile").toString()) && mb.getSize() == dn::kBins * sizeof(float)) {
        std::vector<float> v((size_t)dn::kBins); std::memcpy(v.data(), mb.getData(), mb.getSize());
        for (float& x : v) if (!std::isfinite(x) || x < 0.f) x = 0.f;
        engine.setProfile(v);
    }
}

juce::AudioProcessorEditor* DenoiserProcessor::createEditor() { return new DenoiserEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new DenoiserProcessor(); }
