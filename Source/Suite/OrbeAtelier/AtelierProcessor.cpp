#include "AtelierProcessor.h"
#include "AtelierEditor.h"

using kit::u8;

juce::AudioProcessorValueTreeState::ParameterLayout AtelierProcessor::makeLayout() {
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    StringArray names; for (int i = 0; i < atl::numPresets(); ++i) names.add(atl::presetName(i));
    // « preset » : navigation, pas un réglage à automatiser (une automation rechargerait toute la chaîne en pleine lecture)
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{"preset", 1}, "Preset", names, 0, AudioParameterChoiceAttributes().withAutomatable(false)));
    const oc::S def = atl::presetSettings(0);
    for (auto& d : atl::kP) {
        const ParameterID pid{d.id, 1};
        if (d.kind == atl::B) l.add(std::make_unique<AudioParameterChoice>(pid, u8(d.name), StringArray{u8("Coupé"), "Actif"}, def.*d.field > 0.5f ? 1 : 0));  // choix 2 valeurs : s'accroche à 0/1 à la restauration d'état
        else if (d.kind == atl::C) l.add(std::make_unique<AudioParameterChoice>(pid, u8(d.name), atl::choices(d.id), (int)(def.*d.field)));
        else {
            NormalisableRange<float> r(d.lo, d.hi, d.step);
            if (d.centre > 0.f) r.setSkewForCentre(d.centre);
            const atl::PDef* dp = &d;
            l.add(std::make_unique<AudioParameterFloat>(pid, u8(d.name), r, def.*d.field,
                AudioParameterFloatAttributes().withStringFromValueFunction([dp](float v, int) { return atl::fmt(*dp, v); })));
        }
    }
    l.add(std::make_unique<AudioParameterBool>(ParameterID{"bypass", 1}, "Bypass", false));
    l.add(kit::v3::notes::makeAccidParam());   // ajouté en dernier : écriture dièses / bémols (noms seulement)
    l.add(std::make_unique<AudioParameterBool>(ParameterID{"dsFirst", 1}, u8("De-ess avant comp"), false));   // ordre de la chaîne : réglage de session, hors presets
    return l;
}

AtelierProcessor::AtelierProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true).withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "ORBEATELIER", makeLayout()) {
    bypass = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter("bypass"));
}

bool AtelierProcessor::isBusesLayoutSupported(const BusesLayout& l) const {
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() &&
           (l.getMainInputChannelSet() == juce::AudioChannelSet::mono() || l.getMainInputChannelSet() == juce::AudioChannelSet::stereo());
}

oc::S AtelierProcessor::settings() const {
    oc::S s; for (auto& d : atl::kP) s.*d.field = apvts.getRawParameterValue(d.id)->load();
    s.dsFirst = apvts.getRawParameterValue("dsFirst")->load();
    return s;
}

double AtelierProcessor::presetValue(const juce::String& id, int preset) {
    if (auto* d = atl::find(id)) return atl::presetSettings(preset).*d->field;
    return 0.0;
}

void AtelierProcessor::applyPreset(int i) {
    i = juce::jlimit(0, atl::numPresets() - 1, i);
    if (auto* p = apvts.getParameter("preset")) p->setValueNotifyingHost(p->convertTo0to1((float)i));
    const oc::S s = atl::presetSettings(i);
    for (auto& d : atl::kP) if (!atl::presetSkips(d.id)) if (auto* a = apvts.getParameter(d.id)) a->setValueNotifyingHost(a->convertTo0to1(s.*d.field));
}

bool AtelierProcessor::matchesPreset() const {
    const oc::S s = atl::presetSettings(currentPreset());
    for (auto& d : atl::kP) if (!atl::presetSkips(d.id)) if (std::fabs(apvts.getRawParameterValue(d.id)->load() - s.*d.field) > std::max((d.hi - d.lo) * 0.004f, d.step * 0.55f) + 1e-4f) return false;
    return true;
}

void AtelierProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) { setLatencySamples(host.prepare(sampleRate, samplesPerBlock)); }

void AtelierProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    juce::ScopedNoDenormals nd;
    host.run(buffer, getPlayHead(), settings(), getTotalNumInputChannels() >= 2, false, bypass != nullptr && bypass->get());
}

void AtelierProcessor::getStateInformation(juce::MemoryBlock& dest) { if (auto xml = apvts.copyState().createXml()) copyXmlToBinary(*xml, dest); }
void AtelierProcessor::setStateInformation(const void* data, int size) {
    if (auto xml = getXmlFromBinary(data, size)) if (xml->hasTagName(apvts.state.getType())) apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorEditor* AtelierProcessor::createEditor() { return new AtelierEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AtelierProcessor(); }
