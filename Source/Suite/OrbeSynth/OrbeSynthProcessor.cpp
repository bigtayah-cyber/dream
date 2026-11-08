#include "OrbeSynthProcessor.h"
#include "OrbeSynthEditor.h"
#include "../../Kit/Kit.h"

using juce::String;

namespace {
String pct(float x, int) { return String(juce::roundToInt(x * 100.f)) + " %"; }
String secs(float x, int) { return x < 1.f ? String(juce::roundToInt(x * 1000.f)) + " ms" : String(x, 2) + " s"; }
String hz(float x, int) {
    const float f = 60.f * std::pow(300.f, x);
    return f >= 1000.f ? String(f / 1000.f, 1) + " kHz" : String(juce::roundToInt(f)) + " Hz";
}
}  // namespace

juce::AudioProcessorValueTreeState::ParameterLayout OrbeSynthProcessor::makeLayout() {
    using namespace juce;
    using kit::u8;
    AudioProcessorValueTreeState::ParameterLayout l;
    auto F = [&](const char* id, String name, float lo, float hi, float def, float skew, String (*txt)(float, int)) {
        l.add(std::make_unique<AudioParameterFloat>(ParameterID{id, 1}, name, NormalisableRange<float>(lo, hi, 0.f, skew), def,
                                                    AudioParameterFloatAttributes().withStringFromValueFunction(txt)));
    };
    auto C = [&](const char* id, String name, StringArray items, int def) {
        l.add(std::make_unique<AudioParameterChoice>(ParameterID{id, 1}, name, items, def));
    };
    const StringArray waves{"Sinus", "Triangle", "Scie", u8("Carré")};
    C("w1", "Osc 1 forme", waves, osyn::Saw);
    C("w2", "Osc 2 forme", waves, osyn::Square);
    l.add(std::make_unique<AudioParameterInt>(ParameterID{"semi", 1}, "Osc 2 transposition", -24, 24, 0, AudioParameterIntAttributes().withStringFromValueFunction([](int x, int) { return String(x > 0 ? "+" : "") + String(x) + " dt"; })));
    F("mix", "Osc 2 niveau", 0.f, 1.f, .3f, 1.f, pct);
    F("fm", "FM", 0.f, 1.f, 0.f, 1.f, pct);
    l.add(std::make_unique<AudioParameterInt>(ParameterID{"uni", 1}, "Unisson", 1, 7, 3, AudioParameterIntAttributes().withStringFromValueFunction([](int x, int) { return String(x) + " voix"; })));
    l.add(std::make_unique<AudioParameterInt>(ParameterID{"det", 1}, u8("Désaccord"), 0, 50, 12, AudioParameterIntAttributes().withStringFromValueFunction([](int x, int) { return String(x) + " ct"; })));
    F("sub", "Sub", 0.f, 1.f, 0.f, 1.f, pct);
    F("cut", "Coupure", 0.f, 1.f, .55f, 1.f, hz);
    F("res", u8("Résonance"), 0.f, 1.f, .2f, 1.f, pct);
    F("famt", u8("Quantité d'enveloppe"), 0.f, 1.f, .4f, 1.f, pct);
    F("fdec", u8("Déclin filtre"), .02f, 2.f, .4f, .4f, secs);
    F("a", "Attaque", .001f, 2.f, .005f, .3f, secs);
    F("d", u8("Déclin"), .02f, 3.f, .4f, .4f, secs);
    F("s", "Sustain", 0.f, 1.f, .4f, 1.f, pct);
    F("r", u8("Relâche"), .02f, 4.f, .3f, .4f, secs);
    F("drive", "Saturation", 0.f, 1.f, 0.f, 1.f, pct);
    F("cho", "Chorus", 0.f, 1.f, 0.f, 1.f, pct);
    F("dly", "Delay", 0.f, 1.f, .15f, 1.f, pct);
    C("dlydiv", "Delay valeur", {"1/16", "1/8", u8("1/8 pointée"), "1/4", u8("1/4 pointée"), "1/2"}, 2);
    F("rev", "Reverb", 0.f, 1.f, .25f, 1.f, pct);
    F("vol", "Volume", 0.f, 1.f, .7f, 1.f, pct);
    return l;
}

OrbeSynthProcessor::OrbeSynthProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "ORBESYNTH", makeLayout()) {}

bool OrbeSynthProcessor::isBusesLayoutSupported(const BusesLayout& l) const {
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() && l.getMainInputChannelSet().isDisabled();
}

void OrbeSynthProcessor::prepareToPlay(double sampleRate, int) {
    sr = sampleRate;
    engine.prepare(sampleRate);
    setLatencySamples(engine.latency());   // saturation suréchantillonnée (phase linéaire)
    engine.setParams(readParams());
    keys.reset();
}

osyn::Params OrbeSynthProcessor::readParams() const {
    osyn::Params p;
    p.w1 = (int)v("w1"); p.w2 = (int)v("w2"); p.semi = v("semi"); p.mix = v("mix"); p.fm = v("fm"); p.uni = (int)v("uni"); p.det = v("det");
    p.sub = v("sub"); p.cut = v("cut"); p.res = v("res"); p.famt = v("famt"); p.fdec = v("fdec");
    p.a = v("a"); p.d = v("d"); p.s = v("s"); p.r = v("r");
    p.drive = v("drive"); p.cho = v("cho"); p.dly = v("dly"); p.dlyDiv = (int)v("dlydiv"); p.rev = v("rev"); p.vol = v("vol");
    return p;
}

void OrbeSynthProcessor::handle(const juce::MidiMessage& m) {
    if (m.isNoteOn()) engine.noteOn(m.getNoteNumber(), m.getFloatVelocity());
    else if (m.isNoteOff()) engine.noteOff(m.getNoteNumber());
    else if (m.isAllNotesOff() || m.isAllSoundOff()) engine.allNotesOff();
    else if (m.isPitchWheel()) { bend = (float)(m.getPitchWheelValue() - 8192) / 8192.f; }
    else if (m.isControllerOfType(1)) { mod = (float)m.getControllerValue() / 127.f; }
    else if (m.isControllerOfType(64)) engine.sustain(m.getControllerValue() >= 64);
}

void OrbeSynthProcessor::processBlock(juce::AudioBuffer<float>& buf, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals nd;
    const int n = buf.getNumSamples();
    buf.clear();
    keys.processNextMidiBuffer(midi, 0, n, true);
    engine.setParams(readParams());
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm()) { engine.setTempo(*bpm); hostBpm = (float)*bpm; }
    int pos = 0;
    auto render = [&](int from, int len) {
        if (len <= 0) return;
        engine.setBend(bend.load()); engine.setMod(mod.load());
        engine.render(buf.getWritePointer(0, from), buf.getWritePointer(1, from), len);
    };
    for (const auto meta : midi) {
        const int t = juce::jlimit(pos, n, meta.samplePosition);
        render(pos, t - pos); pos = t;
        handle(meta.getMessage());
    }
    render(pos, n - pos);
    voices = engine.activeVoices();
}

void OrbeSynthProcessor::setCurrentProgram(int i) {
    i = juce::jlimit(0, getNumPrograms() - 1, i);
    program = i;
    const auto& p = osyn::presets()[(size_t)i].p;
    auto set = [&](const char* id, float x) { if (auto* prm = apvts.getParameter(id)) prm->setValueNotifyingHost(prm->convertTo0to1(x)); };
    set("w1", (float)p.w1); set("w2", (float)p.w2); set("semi", p.semi); set("mix", p.mix); set("fm", p.fm); set("uni", (float)p.uni); set("det", p.det);
    set("sub", p.sub); set("cut", p.cut); set("res", p.res); set("famt", p.famt); set("fdec", p.fdec);
    set("a", p.a); set("d", p.d); set("s", p.s); set("r", p.r);
    set("drive", p.drive); set("cho", p.cho); set("dly", p.dly); set("dlydiv", (float)p.dlyDiv); set("rev", p.rev); set("vol", p.vol);
}

const juce::String OrbeSynthProcessor::getProgramName(int i) {
    return i >= 0 && i < getNumPrograms() ? kit::u8(osyn::presets()[(size_t)i].name) : juce::String();
}

bool OrbeSynthProcessor::presetModified() const {
    const auto a = readParams(), b = osyn::presets()[(size_t)program].p;
    auto d = [](float x, float y) { return std::fabs(x - y) > 1e-3f * (1.f + std::fabs(y)); };
    return a.w1 != b.w1 || a.w2 != b.w2 || a.uni != b.uni || a.dlyDiv != b.dlyDiv || d(a.semi, b.semi) || d(a.mix, b.mix) || d(a.fm, b.fm) || d(a.det, b.det) || d(a.sub, b.sub)
        || d(a.cut, b.cut) || d(a.res, b.res) || d(a.famt, b.famt) || d(a.fdec, b.fdec) || d(a.a, b.a) || d(a.d, b.d) || d(a.s, b.s) || d(a.r, b.r)
        || d(a.drive, b.drive) || d(a.cho, b.cho) || d(a.dly, b.dly) || d(a.rev, b.rev) || d(a.vol, b.vol);
}

void OrbeSynthProcessor::getStateInformation(juce::MemoryBlock& dest) {
    auto st = apvts.copyState();
    st.setProperty("program", program, nullptr);
    if (auto xml = st.createXml()) copyXmlToBinary(*xml, dest);
}

void OrbeSynthProcessor::setStateInformation(const void* data, int size) {
    auto xml = getXmlFromBinary(data, size);
    if (!xml || !xml->hasTagName(apvts.state.getType())) return;
    auto tree = juce::ValueTree::fromXml(*xml);
    apvts.replaceState(tree);
    program = juce::jlimit(0, getNumPrograms() - 1, (int)tree.getProperty("program", 0));
}

juce::AudioProcessorEditor* OrbeSynthProcessor::createEditor() { return new OrbeSynthEditor(*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new OrbeSynthProcessor(); }
