#include "LooksProcessor.h"
#include "LooksEditor.h"

using kit::u8;
static const char* const kMacroIds[4] = {"int", "esp", "mor", "cou"};

juce::AudioProcessorValueTreeState::ParameterLayout LooksProcessor::makeLayout() {
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    StringArray names; for (int i = 0; i < oc::numLooks(); ++i) names.add(lookName(i));
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{"look", 1}, "Look", names, 0));
    const char* mn[4] = {"Intensité", "Espace", "Mordant", "Couleur"};
    for (int i = 0; i < 4; ++i)
        l.add(std::make_unique<AudioParameterFloat>(ParameterID{kMacroIds[i], 1}, u8(mn[i]), NormalisableRange<float>(0.f, 1.f, 0.001f), oc::looks()[0].def[i],
            AudioParameterFloatAttributes().withStringFromValueFunction([](float v, int) { return String(v * 10.f, 1); })));
    l.add(std::make_unique<AudioParameterFloat>(ParameterID{"mix", 1}, "Mix", NormalisableRange<float>(0.f, 1.f, 0.01f), 1.f,
        AudioParameterFloatAttributes().withStringFromValueFunction([](float v, int) { return String(roundToInt(v * 100.f)) + " %"; })));
    l.add(std::make_unique<AudioParameterFloat>(ParameterID{"out", 1}, "Sortie", NormalisableRange<float>(-24.f, 12.f, 0.1f), 0.f,
        AudioParameterFloatAttributes().withStringFromValueFunction([](float v, int) { return String(v, 1) + " dB"; })));
    StringArray keys; keys.add("Auto"); for (auto* n : oc::ui::kNotes) keys.add(n);
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{"key", 1}, u8("Tonalité"), keys, 0));
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{"scale", 1}, "Mode", StringArray{u8("Majeur"), u8("Mineur")}, 0));
    l.add(std::make_unique<AudioParameterBool>(ParameterID{"bypass", 1}, "Bypass", false));
    l.add(kit::v3::notes::makeAccidParam());   // ajouté en dernier : écriture dièses / bémols (noms seulement)
    return l;
}

LooksProcessor::LooksProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true).withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "ORBELOOKS", makeLayout()) {
    bypass = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter("bypass"));
}

bool LooksProcessor::isBusesLayoutSupported(const BusesLayout& l) const {
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() &&
           (l.getMainInputChannelSet() == juce::AudioChannelSet::mono() || l.getMainInputChannelSet() == juce::AudioChannelSet::stereo());
}

oc::S LooksProcessor::settings() const {
    auto v = [this](const char* id) { return apvts.getRawParameterValue(id)->load(); };
    oc::S s = oc::lookSettings(currentLook(), v("int"), v("esp"), v("mor"), v("cou"));
    s.key = v("key"); s.scale = v("scale"); s.mix = v("mix"); s.out = v("out");
    return s;
}

double LooksProcessor::macroDefault(const juce::String& id, int look) {
    for (int i = 0; i < 4; ++i) if (id == kMacroIds[i]) return oc::looks()[(size_t)juce::jlimit(0, oc::numLooks() - 1, look)].def[i];
    return 0.0;
}

void LooksProcessor::applyLook(int i) {
    i = juce::jlimit(0, oc::numLooks() - 1, i);
    if (auto* p = apvts.getParameter("look")) p->setValueNotifyingHost(p->convertTo0to1((float)i));
    for (int m = 0; m < 4; ++m) if (auto* p = apvts.getParameter(kMacroIds[m])) p->setValueNotifyingHost(p->convertTo0to1(oc::looks()[(size_t)i].def[m]));
}

bool LooksProcessor::macrosMoved() const {
    for (int m = 0; m < 4; ++m) if (std::fabs(apvts.getRawParameterValue(kMacroIds[m])->load() - oc::looks()[(size_t)currentLook()].def[m]) > 0.004f) return true;
    return false;
}

void LooksProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) { setLatencySamples(host.prepare(sampleRate, samplesPerBlock)); }

void LooksProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    juce::ScopedNoDenormals nd;
    host.run(buffer, getPlayHead(), settings(), getTotalNumInputChannels() >= 2, true, bypass != nullptr && bypass->get());
}

void LooksProcessor::getStateInformation(juce::MemoryBlock& dest) { if (auto xml = apvts.copyState().createXml()) copyXmlToBinary(*xml, dest); }
void LooksProcessor::setStateInformation(const void* data, int size) {
    if (auto xml = getXmlFromBinary(data, size)) if (xml->hasTagName(apvts.state.getType())) apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorEditor* LooksProcessor::createEditor() { return new LooksEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new LooksProcessor(); }
