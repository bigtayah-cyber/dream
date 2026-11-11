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
    // ajoutés en fin de liste (identifiants existants inchangés)
    l.add(fparam("link", "Liaison st\xC3\xA9r\xC3\xA9o", 0.f, 1.f, 0.01f, d.link, fmtPct));
    l.add(fparam("chamt", "CH (2,5-5 kHz)", 0.f, 1.f, 0.01f, d.chAmt, [](float v) { return v < 0.005f ? String(u8("Coup\xC3\xA9")) : fmtPct(v); }));
    l.add(std::make_unique<AudioParameterFloat>(ParameterID{"look", 1}, "Lookahead", NormalisableRange<float>(0.f, des::kLookMaxMs, 0.1f), 1.f,
        AudioParameterFloatAttributes().withAutomatable(false).withStringFromValueFunction([](float v, int) { return String(v, 1) + " ms"; })));
    return l;
}

DeEsserProcessor::DeEsserProcessor() : FxProcessor("Orbe DeEsser", "ORBEDEESSER", makeLayout()) { apvts.addParameterListener("look", this); }
DeEsserProcessor::~DeEsserProcessor() { apvts.removeParameterListener("look", this); cancelPendingUpdate(); }

// Lookahead = latence : changement refait sous suspendProcessing (fil audio à l'arrêt), puis l'hôte est prévenu.
void DeEsserProcessor::parameterChanged(const juce::String&, float) { triggerAsyncUpdate(); }
void DeEsserProcessor::handleAsyncUpdate() {
    if (curSr <= 0.0 || (int)std::lround((double)juce::jlimit(0.f, des::kLookMaxMs, raw("look")) * 0.001 * curSr) == engine.latency()) return;   // latence inchangée : rien à refaire
    suspendProcessing(true);
    engine.prepare(curSr, raw("look"));
    prepareBypass(curSr, engine.latency(), maxBlock);
    setLatencySamples(engine.latency());
    suspendProcessing(false);
    updateHostDisplay(ChangeDetails().withLatencyChanged(true));
}

float DeEsserProcessor::pollAutoThreshold() { return engine.autoStatus() == 2 ? engine.takeAutoThreshold() : 1.f; }

des::Params DeEsserProcessor::readParams() const {
    des::Params p;
    p.freq = raw("freq"); p.width = raw("width"); p.thr = raw("thr"); p.range = raw("range"); p.atk = raw("atk"); p.rel = raw("rel");
    p.link = raw("link"); p.chAmt = raw("chamt"); p.split = raw("mode") < 0.5f; p.sharp = raw("style") > 0.5f; p.listen = raw("listen") > 0.5f;
    return p;
}

void DeEsserProcessor::prepareToPlay(double sr, int block) {
    maxBlock = juce::jmax(512, block);
    curSr = sr;
    engine.prepare(sr, raw("look")); monoIn.assign((size_t)maxBlock, 0.f); monoOut.assign((size_t)maxBlock, 0.f);
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
