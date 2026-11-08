#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace {
struct Preset { const char* name; float tension, mordant, brume, halo, mix, out; int key, scale; };

const Preset kPresets[] = {
    {"Verre froid",  0.95f, 0.20f, 0.30f, 0.30f, 1.00f,  0.0f, 0, 0},
    {"Machoire",     0.85f, 0.65f, 0.15f, 0.20f, 1.00f, -1.0f, 0, 0},
    {"Salle vide",   0.60f, 0.10f, 0.70f, 0.25f, 0.90f,  0.0f, 0, 1},
    {"Halo rose",    0.75f, 0.15f, 0.45f, 0.80f, 1.00f, -1.5f, 0, 1},
    {"Tension pure", 1.00f, 0.00f, 0.10f, 0.00f, 1.00f,  0.0f, 0, 0},
    {"Vapeur",       0.40f, 0.30f, 0.85f, 0.45f, 0.85f,  0.0f, 0, 2},
    {"Casse",        0.90f, 0.95f, 0.25f, 0.30f, 1.00f, -3.0f, 0, 0},
    {"Cabine",       0.25f, 0.20f, 0.12f, 0.10f, 0.80f,  0.0f, 0, 1},
};
constexpr int kNumPresets = (int)(sizeof(kPresets) / sizeof(kPresets[0]));
}  // namespace

juce::AudioProcessorValueTreeState::ParameterLayout VoileProcessor::makeLayout() {
    using juce::AudioParameterChoice, juce::AudioParameterFloat, juce::NormalisableRange, juce::ParameterID;
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    auto pct = [](float v, int) { return juce::String(juce::roundToInt(v * 100.f)); };
    auto macro = [&](const char* id, const char* name, float def) {
        l.add(std::make_unique<AudioParameterFloat>(ParameterID{id, 1}, name, NormalisableRange<float>(0.f, 1.f), def,
                                                    juce::AudioParameterFloatAttributes().withStringFromValueFunction(pct)));
    };
    macro("tension", "Tension", 0.70f);
    macro("mordant", "Mordant", 0.30f);
    macro("brume", "Brume", 0.30f);
    macro("halo", "Halo", 0.30f);
    macro("mix", "Mix", 1.00f);
    l.add(std::make_unique<AudioParameterFloat>(ParameterID{"out", 1}, "Sortie", NormalisableRange<float>(-12.f, 12.f, 0.1f), 0.f,
                                                juce::AudioParameterFloatAttributes().withLabel("dB")));
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{"key", 1}, "Tonalite",
          juce::StringArray{"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"}, 0));
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{"scale", 1}, "Gamme",
          juce::StringArray{"Chromatique", "Majeur", "Mineur"}, 1));
    return l;
}

VoileProcessor::VoileProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "VOILE", makeLayout()) {}

bool VoileProcessor::isBusesLayoutSupported(const BusesLayout& l) const {
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() &&
           (l.getMainInputChannelSet() == juce::AudioChannelSet::mono() ||
            l.getMainInputChannelSet() == juce::AudioChannelSet::stereo());
}

void VoileProcessor::prepareToPlay(double sampleRate, int) {
    engine.prepare(sampleRate);
    bypassL.init(engine.latency() + 8);
    bypassR.init(engine.latency() + 8);
    setLatencySamples(engine.latency());
}

void VoileProcessor::processBlockBypassed(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    if (buffer.getNumChannels() < 2) return;
    const int nIn = getTotalNumInputChannels();
    const float d = (float)engine.latency();
    float* l = buffer.getWritePointer(0);
    float* r = buffer.getWritePointer(1);
    for (int i = 0; i < buffer.getNumSamples(); ++i) {
        const float x0 = l[i], x1 = nIn >= 2 ? r[i] : x0;
        bypassL.push(x0);
        bypassR.push(x1);
        l[i] = bypassL.read(d);
        r[i] = bypassR.read(d);
    }
}

void VoileProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int nIn = getTotalNumInputChannels();
    if (buffer.getNumChannels() < 2) return;

    auto get = [this](const char* id) { return apvts.getRawParameterValue(id)->load(); };
    voile::Params p;
    p.tension = get("tension");
    p.mordant = get("mordant");
    p.brume = get("brume");
    p.halo = get("halo");
    p.mix = get("mix");
    p.outDb = get("out");
    p.key = (int)get("key");
    p.scale = (int)get("scale");
    engine.set(p);

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm()) engine.setBpm(*bpm);

    float* l = buffer.getWritePointer(0);
    float* r = buffer.getWritePointer(1);
    engine.process(l, nIn >= 2 ? r : nullptr, l, r, n);
    level.store(engine.level());
    shift.store(engine.shiftSemis());
}

int VoileProcessor::getNumPrograms() { return kNumPresets; }
const juce::String VoileProcessor::getProgramName(int i) { return juce::isPositiveAndBelow(i, kNumPresets) ? kPresets[i].name : ""; }

void VoileProcessor::setParam(const char* id, float v) {
    if (auto* p = apvts.getParameter(id)) p->setValueNotifyingHost(p->convertTo0to1(v));
}

void VoileProcessor::setCurrentProgram(int i) {
    if (!juce::isPositiveAndBelow(i, kNumPresets)) return;
    current = i;
    const auto& p = kPresets[i];
    setParam("tension", p.tension);
    setParam("mordant", p.mordant);
    setParam("brume", p.brume);
    setParam("halo", p.halo);
    setParam("mix", p.mix);
    setParam("out", p.out);
    setParam("key", (float)p.key);
    setParam("scale", (float)p.scale);
}

void VoileProcessor::getStateInformation(juce::MemoryBlock& dest) {
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary(*xml, dest);
}

void VoileProcessor::setStateInformation(const void* data, int size) {
    if (auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(apvts.state.getType())) apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorEditor* VoileProcessor::createEditor() { return new VoileEditor(*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new VoileProcessor(); }
