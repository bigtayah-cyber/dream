#include "AutotuneProcessor.h"
#include "AutotuneEditor.h"
#include "../../Kit/Kit.h"

using kit::u8;
using juce::String;

juce::AudioProcessorValueTreeState::ParameterLayout AutotuneProcessor::makeLayout() {
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    const atn::Params d;
    auto flt = [&](const char* id, const char* name, float lo, float hi, float step, float def, std::function<String(float)> fmt, float centre = 0.f) {
        NormalisableRange<float> r(lo, hi, step); if (centre > 0.f) r.setSkewForCentre(centre);
        l.add(std::make_unique<AudioParameterFloat>(ParameterID{id, 1}, u8(name), r, def, AudioParameterFloatAttributes().withStringFromValueFunction([fmt](float v, int) { return fmt(v); })));
    };
    auto pct = [](float v) { return String(roundToInt(v * 100.f)) + " %"; };
    auto sgn = [](float v, int dec) { return (v > 0.f ? "+" : "") + String(v, dec); };
    flt("speed", "Vitesse", 0.f, 400.f, 1.f, d.speed, [](float v) { return v < 0.5f ? u8("0 \xC2\xB7 robot") : String(roundToInt(v)) + " ms"; }, 60.f);
    flt("tol", "Tolérance", 0.f, 50.f, 1.f, d.tol, [](float v) { return String(roundToInt(v)) + " ct"; });
    flt("amt", "Quantité", 0.f, 1.f, 0.01f, d.amt, pct);
    flt("trans", "Transposer", -12.f, 12.f, 1.f, d.trans, [sgn](float v) { return sgn(v, 0) + " dt"; });
    flt("form", "Formants", -6.f, 6.f, 0.5f, d.form, [sgn](float v) { return sgn(v, 1) + " dt"; });
    l.add(std::make_unique<AudioParameterBool>(ParameterID{"formPres", 1}, u8("Formants préservés"), d.formPres));
    flt("vd", "Vibrato", 0.f, 60.f, 1.f, d.vd, [](float v) { return v < 0.5f ? String("off") : String(roundToInt(v)) + " ct"; });
    flt("vr", "Vitesse du vibrato", 3.f, 8.f, 0.1f, d.vr, [](float v) { return String(v, 1) + " Hz"; });
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{"harm", 1}, "Harmonie", StringArray{u8("Aucune"), u8("Tierce au-dessus"), "Quinte", u8("Tierce en dessous"), u8("Octave au-dessus"), u8("Octave en dessous")}, d.harm));
    flt("hv", "Volume d'harmonie", 0.f, 1.f, 0.01f, d.hv, pct);
    flt("dbl", "Doublage", 0.f, 1.f, 0.01f, d.dbl, [](float v) { return v < 0.005f ? String("off") : String(roundToInt(v * 100.f)) + " %"; });
    flt("mix", "Mix", 0.f, 1.f, 0.01f, d.mix, pct);
    StringArray keys{"Auto"}; for (int i = 0; i < 12; ++i) keys.add(atn::noteNames()[i]);
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{"key", 1}, u8("Tonalité"), keys, 0));
    StringArray sc; for (int i = 0; i < atn::kScales; ++i) sc.add(atn::scales()[i].name);
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{"scale", 1}, "Gamme", sc, 1));
    for (int i = 0; i < 12; ++i) l.add(std::make_unique<AudioParameterChoice>(ParameterID{"x" + String(i), 1}, u8("Note ") + atn::noteNames()[i] + u8(" inversée"), StringArray{"Non", "Oui"}, 0));   // choix à 2 valeurs : valeur toujours calée sur 0/1 (restauration d'état fiable)
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{"latmode", 1}, u8("Latence"), StringArray{"Direct", u8("Qualité")}, 0));   // Direct : <= 256 éch. ; Qualité : formants, latence d'origine
    l.add(std::make_unique<AudioParameterBool>(ParameterID{"bypass", 1}, "Bypass", false));
    return l;
}

AutotuneProcessor::AutotuneProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true).withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "ORBEAUTOTUNE", makeLayout()) {
    bypass = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter("bypass"));
}

bool AutotuneProcessor::isBusesLayoutSupported(const BusesLayout& l) const {
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    const bool inOk = in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
    return inOk && (out == in || (in == juce::AudioChannelSet::mono() && out == juce::AudioChannelSet::stereo()));
}

atn::Params AutotuneProcessor::readParams() const {
    atn::Params p;
    p.speed = raw("speed"); p.tol = raw("tol"); p.amt = raw("amt"); p.trans = raw("trans"); p.form = raw("form"); p.formPres = raw("formPres") > 0.5f;
    p.vd = raw("vd"); p.vr = raw("vr"); p.harm = juce::roundToInt(raw("harm")); p.hv = raw("hv"); p.dbl = raw("dbl"); p.mix = raw("mix");
    p.lowLat = juce::roundToInt(raw("latmode")) == 0;
    p.key = juce::roundToInt(raw("key")) - 1; p.scale = juce::roundToInt(raw("scale"));
    for (int i = 0; i < 12; ++i) if (raw(("x" + String(i)).toRawUTF8()) > 0.5f) p.flips |= 1 << i;
    return p;
}

void AutotuneProcessor::setPlain(const char* id, float v) {
    if (auto* p = apvts.getParameter(id)) { p->beginChangeGesture(); p->setValueNotifyingHost(p->convertTo0to1(v)); p->endChangeGesture(); }
}
void AutotuneProcessor::setCurrentProgram(int i) {
    prog = juce::jlimit(0, atn::kPresets - 1, i);
    const auto& q = atn::presets()[prog];
    setPlain("speed", q.speed); setPlain("tol", q.tol); setPlain("amt", q.amt); setPlain("trans", q.trans); setPlain("form", q.form);
    setPlain("vd", q.vd); setPlain("vr", q.vr); setPlain("harm", (float)q.harm); setPlain("hv", q.hv); setPlain("dbl", q.dbl);
}
bool AutotuneProcessor::presetMatches(int i) const {
    const auto& q = atn::presets()[juce::jlimit(0, atn::kPresets - 1, i)];
    auto eq = [](float a, float b) { return std::fabs(a - b) < 0.011f * juce::jmax(1.f, std::fabs(b)); };
    return eq(raw("speed"), q.speed) && eq(raw("tol"), q.tol) && eq(raw("amt"), q.amt) && eq(raw("trans"), q.trans) && eq(raw("form"), q.form) && eq(raw("vd"), q.vd) &&
           eq(raw("vr"), q.vr) && juce::roundToInt(raw("harm")) == q.harm && eq(raw("hv"), q.hv) && eq(raw("dbl"), q.dbl);
}
void AutotuneProcessor::toggleNote(int pc) {
    if (auto* p = apvts.getParameter("x" + String(pc))) { p->beginChangeGesture(); p->setValueNotifyingHost(p->getValue() > 0.5f ? 0.f : 1.f); p->endChangeGesture(); }
}
void AutotuneProcessor::clearNotes() { for (int i = 0; i < 12; ++i) if (auto* p = apvts.getParameter("x" + String(i))) if (p->getValue() > 0.5f) { p->beginChangeGesture(); p->setValueNotifyingHost(0.f); p->endChangeGesture(); } }

void AutotuneProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    engine.prepare(sampleRate);
    engine.set(readParams());   // mode de latence choisi : la latence déclarée en dépend
    latLow = engine.latencyDirect(); latQual = engine.latencyQuality();
    byp.init(juce::jmax(latLow, latQual));
    byp.lat = engine.latency();
    scratch.setSize(1, juce::jmax(512, samplesPerBlock));
    faderIsLow = engine.latency() == latLow && juce::roundToInt(raw("latmode")) == 0;
    fader.prepare(sampleRate, engine.latency(), juce::jmax(4096, samplesPerBlock));
    faderIdle.prepare(sampleRate, faderIsLow ? latQual : latLow, juce::jmax(4096, samplesPerBlock));
    setLatencySamples(engine.latency());
}

void AutotuneProcessor::processBlockBypassed(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    const int n = buffer.getNumSamples(), nc = buffer.getNumChannels();
    if (nc == 0) return;
    if (scratch.getNumSamples() < n) scratch.setSize(1, n);
    float* l = buffer.getWritePointer(0);
    float* r = nc > 1 ? buffer.getWritePointer(1) : scratch.getWritePointer(0);
    byp.process(l, r, n, getTotalNumInputChannels() >= 2);
}

void AutotuneProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    juce::ScopedNoDenormals nd;
    const int n = buffer.getNumSamples(), nc = buffer.getNumChannels();
    if (nc == 0) return;
    if (scratch.getNumSamples() < n) scratch.setSize(1, n);
    const atn::Params prm = readParams();
    engine.set(prm);
    if (prm.lowLat != faderIsLow) {   // changement de mode : nouvelle latence déclarée, les deux fondus de contournement échangent leur rôle
        std::swap(fader, faderIdle);
        faderIsLow = prm.lowLat; byp.lat = engine.latency();
        setLatencySamples(engine.latency());
    }
    float* l = buffer.getWritePointer(0);
    float* r = nc > 1 ? buffer.getWritePointer(1) : scratch.getWritePointer(0);
    const bool stereoIn = getTotalNumInputChannels() >= 2 && nc >= 2;
    // le fondu inactif suit l'entrée sans la modifier (mémoire de l'entrée retardée prête pour la bascule)
    faderIdle.run(l, nc > 1 ? r : nullptr, n, stereoIn, false, [](float*, float*, int) {});
    // bypass honoré : entrée retardée + fondu de 10 ms ; le moteur continue de tourner
    fader.run(l, nc > 1 ? r : nullptr, n, stereoIn, bypass != nullptr && bypass->get(), [&](float* cl, float* cr, int c) {
        float* rr = cr ? cr : r + (cl - l);   // une seule voie : tampon de travail
        if (!stereoIn) juce::FloatVectorOperations::copy(rr, cl, c);   // entrée mono : même signal des deux côtés
        engine.process(cl, rr, c);
    });
}

void AutotuneProcessor::getStateInformation(juce::MemoryBlock& dest) {
    auto st = apvts.copyState(); st.setProperty("prog", prog, nullptr);
    if (auto xml = st.createXml()) copyXmlToBinary(*xml, dest);
}
void AutotuneProcessor::setStateInformation(const void* data, int size) {
    if (auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(apvts.state.getType())) { auto st = juce::ValueTree::fromXml(*xml); prog = juce::jlimit(0, atn::kPresets - 1, (int)st.getProperty("prog", prog)); apvts.replaceState(st); }
}

juce::AudioProcessorEditor* AutotuneProcessor::createEditor() { return new AutotuneEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AutotuneProcessor(); }
