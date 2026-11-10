#include "MasterProcessor.h"
#include "MasterEditor.h"
#include "../../Kit/Kit.h"

using kit::u8;
using juce::String;

juce::AudioProcessorValueTreeState::ParameterLayout MasterProcessor::makeLayout() {
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    const mas::Params d;
    auto flt = [&](const char* id, const char* name, float lo, float hi, float step, float def, std::function<String(float)> fmt, float centre = 0.f) {
        NormalisableRange<float> r(lo, hi, step); if (centre > 0.f) r.setSkewForCentre(centre);
        l.add(std::make_unique<AudioParameterFloat>(ParameterID{id, 1}, u8(name), r, def, AudioParameterFloatAttributes().withStringFromValueFunction([fmt](float v, int) { return fmt(v); })));
    };
    auto db = [](float v) { return String(v, 1) + " dB"; };
    auto pct = [](float v) { return String(roundToInt(v * 100.f)) + " %"; };
    auto sw = [&](const char* id, const char* name, bool def) { l.add(std::make_unique<AudioParameterBool>(ParameterID{id, 1}, u8(name), def)); };
    for (int i = 0; i < mas::kBands; ++i) {
        const auto& b = mas::bands();
        flt(("eq" + String(i)).toRawUTF8(), (std::string(b[i].name) + " " + b[i].tag).c_str(), -6.f, 6.f, 0.1f, 0.f, db);
    }
    sw("eqOn", "EQ actif", d.eqOn);
    sw("compOn", "Compresseur actif", d.compOn);
    flt("cAmt", "Compresseur quantit\xC3\xA9", 0.f, 1.f, 0.01f, d.cAmt, pct);
    flt("cRatio", "Compresseur ratio", 1.f, 6.f, 0.1f, d.cRatio, [](float v) { return String(v, 1) + ":1"; });
    flt("cAtt", "Compresseur attaque", 1.f, 80.f, 0.5f, d.cAtt, [](float v) { return String(v, 0) + " ms"; }, 20.f);
    flt("cGain", "Compresseur gain", -6.f, 12.f, 0.1f, d.cGain, db);
    sw("satOn", "Saturation active", d.satOn);
    flt("drive", "Saturation drive", 0.f, 1.f, 0.01f, d.drive, pct);
    flt("satMix", "Saturation m\xC3\xA9lange", 0.f, 1.f, 0.01f, d.satMix, pct);
    sw("stOn", "St\xC3\xA9r\xC3\xA9o actif", d.stOn);
    flt("width", "Largeur", 0.f, 1.6f, 0.01f, d.width, pct);
    sw("mono", "Basses en mono", d.mono);
    flt("monoHz", "Mono sous", 40.f, 250.f, 1.f, d.monoHz, [](float v) { return String(roundToInt(v)) + " Hz"; }, 100.f);
    sw("limOn", "Limiteur actif", d.limOn);
    flt("lGain", "Limiteur gain d'entr\xC3\xA9" "e", -3.f, 18.f, 0.1f, d.lGain, db);
    flt("ceil", "Plafond", -3.f, 0.f, 0.1f, d.ceil, [](float v) { return String(v, 1) + " dBTP"; });
    flt("lRel", "Limiteur rel\xC3\xA2" "chement", 20.f, 500.f, 1.f, d.lRel, [](float v) { return String(roundToInt(v)) + " ms"; }, 100.f);
    l.add(std::make_unique<AudioParameterBool>(ParameterID{"bypass", 1}, "Bypass", false));
    return l;
}

MasterProcessor::MasterProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true).withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "ORBEMASTER", makeLayout()) {
    bypass = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter("bypass"));
    for (int i = 0; i < mas::kBands; ++i) pp[(size_t)i] = apvts.getRawParameterValue("eq" + String(i));
    const char* ids[] = {"eqOn", "compOn", "cAmt", "cRatio", "cAtt", "cGain", "satOn", "drive", "satMix", "stOn", "width", "mono", "monoHz", "limOn", "lGain", "ceil", "lRel"};
    static_assert(sizeof(ids) / sizeof(ids[0]) == kNumP - kEqOn, "liste des identifiants");
    for (int i = kEqOn; i < kNumP; ++i) { pp[(size_t)i] = apvts.getRawParameterValue(ids[i - kEqOn]); jassert(pp[(size_t)i] != nullptr); }
}

bool MasterProcessor::isBusesLayoutSupported(const BusesLayout& l) const {
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() &&
           (l.getMainInputChannelSet() == juce::AudioChannelSet::mono() || l.getMainInputChannelSet() == juce::AudioChannelSet::stereo());
}

mas::Params MasterProcessor::readParams() const {
    mas::Params p;
    auto v = [this](int i) { return pp[(size_t)i]->load(); };
    for (int i = 0; i < mas::kBands; ++i) p.eq[i] = v(i);
    p.eqOn = v(kEqOn) > 0.5f; p.compOn = v(kCompOn) > 0.5f; p.cAmt = v(kCAmt); p.cRatio = v(kCRatio); p.cAtt = v(kCAtt); p.cGain = v(kCGain);
    p.satOn = v(kSatOn) > 0.5f; p.drive = v(kDrive); p.satMix = v(kSatMix);
    p.stOn = v(kStOn) > 0.5f; p.width = v(kWidth); p.mono = v(kMono) > 0.5f; p.monoHz = v(kMonoHz);
    p.limOn = v(kLimOn) > 0.5f; p.lGain = v(kLGain); p.ceil = v(kCeil); p.lRel = v(kLRel);
    return p;
}

void MasterProcessor::resetAll() {
    for (auto* p : getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*>(p)) if (r->paramID != "bypass") r->setValueNotifyingHost(r->getDefaultValue());
}

void MasterProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    maxBlock = juce::jmax(512, samplesPerBlock);
    engine.prepare(sampleRate, maxBlock);
    byp.init(engine.latency());
    fader.prepare(sampleRate, engine.latency(), maxBlock);
    setLatencySamples(engine.latency());
}

void MasterProcessor::processBlockBypassed(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    if (buffer.getNumChannels() < 2) return;
    byp.process(buffer.getWritePointer(0), buffer.getWritePointer(1), buffer.getNumSamples(), getTotalNumInputChannels() >= 2);
}

void MasterProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    juce::ScopedNoDenormals nd;
    if (buffer.getNumChannels() < 2) return;
    engine.set(readParams());
    float* l = buffer.getWritePointer(0);
    float* r = buffer.getWritePointer(1);
    const int n = buffer.getNumSamples();
    kit::sanitize(l, n); kit::sanitize(r, n);   // NaN/inf d'un plugin précédent : remplacés par 0 avant tout état interne
    if (getTotalNumInputChannels() < 2) juce::FloatVectorOperations::copy(r, l, n);
    float ip = 0.f, op = 0.f, gc = 0.f, gl = 0.f;
    fader.run(l, r, n, true, bypass != nullptr && bypass->get(), [&](float* cl, float* cr, int c) {   // bypass honoré : entrée retardée + fondu de 10 ms
        engine.process(cl, cr, c);
        ip = juce::jmax(ip, engine.inPeak()); op = juce::jmax(op, engine.outPeak()); gc = juce::jmax(gc, engine.compReduction()); gl = juce::jmax(gl, engine.limReduction());
    });
    inPk.store(ip); outPk.store(op); grComp.store(gc); grLim.store(gl); lufsIn.store(engine.lufsIn()); lufsOut.store(engine.lufsOut());
}

void MasterProcessor::getStateInformation(juce::MemoryBlock& dest) {
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary(*xml, dest);
}
void MasterProcessor::setStateInformation(const void* data, int size) {
    if (auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(apvts.state.getType())) apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorEditor* MasterProcessor::createEditor() { return new MasterEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MasterProcessor(); }
