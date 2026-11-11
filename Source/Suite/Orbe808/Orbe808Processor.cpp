#include "Orbe808Processor.h"
#include "../../Kit/StateVersion.h"
#include "Orbe808Editor.h"
#include "SuiteUi.h"

using juce::String;

namespace {
const char* const kKnobIds[8] = {"len", "punch", "pt", "drive", "sat", "tone", "cut", "click"};
const float kBendValues[4] = {1.f, 2.f, 5.f, 12.f};
}

String Orbe808Processor::presetLabel(int i) { return i >= 0 && i < o808::kNumPresets ? kit::u8(o808::presets()[i].label) : String("Perso"); }

juce::AudioProcessorValueTreeState::ParameterLayout Orbe808Processor::makeLayout() {
    using juce::AudioParameterChoice, juce::AudioParameterFloat, juce::AudioParameterInt, juce::NormalisableRange, juce::ParameterID;
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    const auto& d = o808::presets()[1];
    juce::StringArray names;
    for (int i = 0; i <= o808::kNumPresets; ++i) names.add(presetLabel(i));
    auto fl = [&](const char* id, const String& name, float lo, float hi, float def, float step, std::function<String(float, int)> txt) {
        l.add(std::make_unique<AudioParameterFloat>(ParameterID{id, 1}, name, NormalisableRange<float>(lo, hi, step), def,
                                                    juce::AudioParameterFloatAttributes().withStringFromValueFunction(std::move(txt))));
    };
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{"preset", 1}, "Preset", names, 1));
    fl("len", "Longueur", 0.2f, 4.f, d.len, 0.05f, [](float v, int) { return suiteui::fr1(v) + " s"; });
    fl("punch", "Punch", 0.f, 24.f, d.punch, 1.f, [](float v, int) { return "+" + String(juce::roundToInt(v)) + " dt"; });
    fl("pt", "Vitesse du punch", 0.01f, 0.2f, d.pt, 0.005f, [](float v, int) { return String(juce::roundToInt(v * 1000.f)) + " ms"; });
    fl("drive", "Saturation", 0.f, 1.f, d.drive, 0.02f, [](float v, int) { return String(juce::roundToInt(v * 100.f)) + " %"; });
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{"sat", 1}, "Type de saturation", juce::StringArray{kit::u8("Douce"), kit::u8("Écrêtée"), kit::u8("Pliée")}, d.sat));
    fl("tone", "Harmoniques", 0.f, 1.f, d.tone, 0.02f, [](float v, int) { return String(juce::roundToInt(v * 100.f)) + " %"; });
    fl("cut", "Filtre", 0.f, 1.f, d.cut, 0.01f, [](float v, int) { return String(juce::roundToInt(120.f * std::pow(100.f, v))) + " Hz"; });
    fl("click", kit::u8("Attaque (clic)"), 0.f, 1.f, d.click, 0.02f, [](float v, int) { return String(juce::roundToInt(v * 100.f)) + " %"; });
    fl("glide", kit::u8("Durée des glissés"), 0.03f, 0.4f, 0.12f, 0.01f, [](float v, int) { return String(juce::roundToInt(v * 1000.f)) + " ms"; });
    fl("level", "Niveau", 0.f, 1.f, 0.8f, 0.01f, [](float v, int) { return String(juce::roundToInt(v * 100.f)) + " %"; });
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{"bend", 1}, "Pitch bend", juce::StringArray{kit::u8("±1"), kit::u8("±2"), kit::u8("±5"), kit::u8("±12")}, 1));
    return l;
}

Orbe808Processor::Orbe808Processor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "ORBE808", makeLayout()) {
    apvts.addParameterListener("preset", this);
    for (auto id : kKnobIds) apvts.addParameterListener(id, this);
    startTimerHz(20);
}

Orbe808Processor::~Orbe808Processor() {
    stopTimer();
    apvts.removeParameterListener("preset", this);
    for (auto id : kKnobIds) apvts.removeParameterListener(id, this);
}

bool Orbe808Processor::isBusesLayoutSupported(const BusesLayout& l) const {
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void Orbe808Processor::prepareToPlay(double sr, int maxBlock) {
    engine.prepare(sr);
    setLatencySamples(engine.latency());   // saturation suréchantillonnée (phase linéaire)
    mono.assign((size_t)juce::jmax(512, maxBlock), 0.f);
}

o808::Params Orbe808Processor::paramsFrom(const juce::AudioProcessorValueTreeState& a, int stateVersion) {
    auto g = [&](const char* id) { return a.getRawParameterValue(id)->load(); };
    o808::Params p;
    p.len = g("len"); p.punch = g("punch"); p.pt = g("pt"); p.drive = g("drive"); p.sat = (int)g("sat"); p.tone = g("tone");
    p.cut = g("cut"); p.click = g("click"); p.glide = g("glide"); p.level = g("level");
    p.bendRange = kBendValues[juce::jlimit(0, 3, (int)g("bend"))];
    p.foldScale = stateVersion >= 3 ? 0.5f : 1.f;   // « Pliée » : k réduit (repliement ÷100) ; un projet d'avant garde la « Repliée » d'origine
    return p;
}

void Orbe808Processor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals nd;
    const int n = buffer.getNumSamples();
    buffer.clear();
    if (buffer.getNumChannels() < 2) return;
    if ((int)mono.size() < n) mono.resize((size_t)n);  // hôte qui dépasse la taille annoncée
    keys.processNextMidiBuffer(midi, 0, n, true);
    engine.setParams(paramsFrom(apvts, loadedVersion.load()));

    int pos = 0;
    auto renderTo = [&](int end) { if (end > pos) { engine.render(mono.data() + pos, end - pos); pos = end; } };
    for (const auto meta : midi) {
        const auto m = meta.getMessage();
        renderTo(juce::jlimit(0, n, meta.samplePosition));
        if (m.isNoteOn()) engine.noteOn(m.getNoteNumber(), m.getFloatVelocity());
        else if (m.isNoteOff()) engine.noteOff(m.getNoteNumber());
        else if (m.isPitchWheel()) engine.setBend(((float)m.getPitchWheelValue() - 8192.f) / 8192.f);
        else if (m.isAllNotesOff() || m.isAllSoundOff()) engine.allOff();
    }
    renderTo(n);

    float pk = 0.f;
    for (int i = 0; i < n; ++i) pk = juce::jmax(pk, std::fabs(mono[(size_t)i]));
    buffer.copyFrom(0, 0, mono.data(), n);
    buffer.copyFrom(1, 0, mono.data(), n);
    meter.store(pk);
    const int cur = engine.currentNote();
    sounding.store(cur);
    if (cur >= 0) lastNote.store(cur);
}

// ---------------- presets (thread message) ----------------

void Orbe808Processor::parameterChanged(const String& id, float) {
    if (applying) return;
    if (id == "preset") presetDirty = true; else knobTouched = true;
}

void Orbe808Processor::applyPreset(int idx) {
    if (idx < 0 || idx >= o808::kNumPresets) return;
    const auto& q = o808::presets()[idx];
    applying = true;
    auto set = [&](const char* id, float v) { if (auto* p = apvts.getParameter(id)) p->setValueNotifyingHost(p->convertTo0to1(v)); };
    set("len", q.len); set("punch", q.punch); set("pt", q.pt); set("drive", q.drive); set("sat", (float)q.sat);
    set("tone", q.tone); set("cut", q.cut); set("click", q.click);
    applying = false;
}

void Orbe808Processor::timerCallback() {
    const int idx = (int)apvts.getRawParameterValue("preset")->load();
    if (presetDirty.exchange(false)) { knobTouched = false; applyPreset(idx); }
    else if (knobTouched.exchange(false) && idx != o808::kNumPresets) {
        applying = true;
        if (auto* p = apvts.getParameter("preset")) p->setValueNotifyingHost(p->convertTo0to1((float)o808::kNumPresets));
        applying = false;
    }
}

int Orbe808Processor::getCurrentProgram() { return juce::jmin(o808::kNumPresets - 1, (int)apvts.getRawParameterValue("preset")->load()); }
void Orbe808Processor::setCurrentProgram(int i) {
    if (i < 0 || i >= o808::kNumPresets) return;
    if (auto* p = apvts.getParameter("preset")) p->setValueNotifyingHost(p->convertTo0to1((float)i));
}
const String Orbe808Processor::getProgramName(int i) { return presetLabel(i); }

void Orbe808Processor::getStateInformation(juce::MemoryBlock& dest) {
    auto st = apvts.copyState();
    kit::stampState(st, loadedVersion.load());
    if (auto xml = st.createXml()) copyXmlToBinary(*xml, dest);
}
void Orbe808Processor::setStateInformation(const void* data, int size) {
    auto xml = getXmlFromBinary(data, size);
    if (!xml || !xml->hasTagName(apvts.state.getType())) return;
    applying = true;  // les valeurs restaurées font foi : le preset ne doit pas les écraser
    auto tree = juce::ValueTree::fromXml(*xml);
    loadedVersion = kit::stateVersionOf(tree);   // avant replaceState : un projet d'avant garde son ancien son de saturation « Repliée »
    apvts.replaceState(tree);
    applying = false;
    presetDirty = false; knobTouched = false;
}

juce::AudioProcessorEditor* Orbe808Processor::createEditor() { return new Orbe808Editor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new Orbe808Processor(); }
