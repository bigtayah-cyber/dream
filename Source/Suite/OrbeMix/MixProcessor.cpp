#include "MixProcessor.h"
#include "MixEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout MixProcessor::makeLayout() {
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    juce::StringArray names; for (auto& p : oa::kProfiles) names.add(juce::String::fromUTF8(p.label));
    l.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"bypass", 1}, "Bypass", false));
    l.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"profile", 1}, "Style", names, 0));
    return l;
}

MixProcessor::MixProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true).withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "ORBEMIX", makeLayout()) {
    bypass = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter("bypass"));
    profileP = apvts.getRawParameterValue("profile");
    meter.prepare(sr);
}

bool MixProcessor::isBusesLayoutSupported(const BusesLayout& l) const {
    const auto o = l.getMainOutputChannelSet();
    return o == l.getMainInputChannelSet() && (o == juce::AudioChannelSet::mono() || o == juce::AudioChannelSet::stereo());
}

void MixProcessor::prepareToPlay(double rate, int) {
    if (!(rate > 0.0)) rate = 48000.0;
    const juce::SpinLock::ScopedLockType sl(prepLock);   // certains hôtes rappellent prepare pendant que le fil audio tourne
    sr = rate; meter.prepare(rate); sincePub = 0;
    oa::MixSnap s; meter.fill(s); pub.write(s);
}

void MixProcessor::processBlock(juce::AudioBuffer<float>& buf, juce::MidiBuffer&) {
    juce::ScopedNoDenormals nd;
    const int n = buf.getNumSamples(), nch = buf.getNumChannels();
    if (nch < 1 || n < 1) return;
    if (bypass != nullptr && bypass->get()) return;   // contourné : l'audio passe tel quel (latence nulle), plus de mesure
    const juce::SpinLock::ScopedTryLockType tl(prepLock);
    if (!tl.isLocked()) return;                        // préparation en cours : on saute la mesure de ce bloc
    if (resetReq.exchange(false)) { meter.reset(); oa::MixSnap s; meter.fill(s); pub.write(s); epoch.fetch_add(1); sincePub = 0; }
    const float* L = buf.getReadPointer(0);
    meter.process(L, nch > 1 ? buf.getReadPointer(1) : L, n);   // l'audio n'est jamais modifié
    sincePub += n;
    if (sincePub >= juce::jmax(1, (int)(sr / 15.0))) { oa::MixSnap s; meter.fill(s); pub.write(s); sincePub = 0; }
}

void MixProcessor::getStateInformation(juce::MemoryBlock& dest) { if (auto xml = apvts.copyState().createXml()) copyXmlToBinary(*xml, dest); }
void MixProcessor::setStateInformation(const void* data, int size) {
    if (auto xml = getXmlFromBinary(data, size)) if (xml->hasTagName(apvts.state.getType())) apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorEditor* MixProcessor::createEditor() { return new MixEditor(*this); }
#ifndef ORBE_SUITE_SNAPSHOT
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MixProcessor(); }
#endif
