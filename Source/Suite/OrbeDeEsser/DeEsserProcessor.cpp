#include "DeEsserProcessor.h"
#include "DeEsserEditor.h"

using kit::u8;
using namespace of;

juce::AudioProcessorValueTreeState::ParameterLayout DeEsserProcessor::makeLayout() {
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    const des::Params d;
    l.add(fparam("freq", u8("Fr\xC3\xA9quence"), 4000.f, 10000.f, 1.f, d.freq, fmtHz, 0.f, parseHz));
    l.add(fparam("width", "Largeur", 0.5f, 3.f, 0.01f, d.width, [](float v) { return String(v, 1) + " oct"; }));
    l.add(fparam("thr", "Seuil", -60.f, 0.f, 0.1f, d.thr, [](float v) { return String(v, 1) + " dB"; }));
    l.add(fparam("range", "Plage", 0.f, 24.f, 0.1f, d.range, [](float v) { return String(v, 1) + " dB"; }));
    l.add(fparam("atk", "Attaque", 0.05f, 20.f, 0.01f, d.atk, [](float v) { return String(v, v < 1.f ? 2 : 1) + " ms"; }, 1.f));
    l.add(fparam("rel", u8("Rel\xC3\xA2" "che"), 5.f, 200.f, 0.1f, d.rel, [](float v) { return String(juce::roundToInt(v)) + " ms"; }, 40.f));
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{"mode", 1}, "Mode", StringArray{"Split", "Large bande"}, 0));
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{"style", 1}, "Style", StringArray{"Doux", "Net"}, 0));
    l.add(std::make_unique<AudioParameterBool>(ParameterID{"listen", 1}, u8("\xC3\x89" "couter la sibilance"), false));
    addBypass(l);
    return l;
}

DeEsserProcessor::DeEsserProcessor() : FxProcessor("Orbe DeEsser", "ORBEDEESSER", makeLayout()) {}

des::Params DeEsserProcessor::readParams() const {
    des::Params p;
    p.freq = raw("freq"); p.width = raw("width"); p.thr = raw("thr"); p.range = raw("range"); p.atk = raw("atk"); p.rel = raw("rel");
    p.split = raw("mode") < 0.5f; p.sharp = raw("style") > 0.5f; p.listen = raw("listen") > 0.5f;
    return p;
}

void DeEsserProcessor::prepareToPlay(double sr, int block) {
    maxBlock = juce::jmax(512, block);
    engine.prepare(sr); monoIn.assign((size_t)maxBlock, 0.f); monoOut.assign((size_t)maxBlock, 0.f);
    prepareBypass(sr, engine.latency(), maxBlock);
    setLatencySamples(engine.latency());
}

void DeEsserProcessor::processBlock(juce::AudioBuffer<float>& b, juce::MidiBuffer&) {
    juce::ScopedNoDenormals nd;
    engine.set(readParams()); engine.resetMeter();
    stereo(b, maxBlock, [this](const float* il, const float* ir, float* ol, float* or_, int n) {
        for (int i = 0; i < n; ++i) monoIn[(size_t)i] = ir ? 0.5f * (il[i] + ir[i]) : il[i];
        inAn.push(monoIn.data(), n);
        engine.process(il, ir, ol, or_, n);
        for (int i = 0; i < n; ++i) monoOut[(size_t)i] = 0.5f * (ol[i] + or_[i]);
        outAn.push(monoOut.data(), n);
    });
    gr.store(engine.reduction());
}

juce::AudioProcessorEditor* DeEsserProcessor::createEditor() { return new DeEsserEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new DeEsserProcessor(); }
