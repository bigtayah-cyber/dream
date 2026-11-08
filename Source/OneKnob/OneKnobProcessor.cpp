#include "OneKnobProcessor.h"
#include "OneKnobEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout OneKnobProcessor::makeLayout() {
    using juce::AudioParameterBool, juce::AudioParameterFloat, juce::NormalisableRange, juce::ParameterID;
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    l.add(std::make_unique<AudioParameterFloat>(
        ParameterID{"char", 1}, juce::String::fromUTF8("Caractère"), NormalisableRange<float>(0.f, 10.f, 0.01f), voicing().defaultK * 10.f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction([](float v, int) { return juce::String(v, 1); })));
    l.add(std::make_unique<AudioParameterBool>(ParameterID{"bypass", 1}, "Bypass", false));
    l.add(std::make_unique<juce::AudioParameterChoice>(ParameterID{"key", 1}, juce::String::fromUTF8("Tonalité"),
                                                       juce::StringArray{"Auto", "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"}, 0));
    l.add(std::make_unique<juce::AudioParameterChoice>(ParameterID{"scale", 1}, "Gamme",
                                                       juce::StringArray{"Auto", "Majeur", "Mineur"}, 0));
    return l;
}

OneKnobProcessor::OneKnobProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "ONEKNOB", makeLayout()) {
    bypass = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter("bypass"));
}

bool OneKnobProcessor::isBusesLayoutSupported(const BusesLayout& l) const {
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() &&
           (l.getMainInputChannelSet() == juce::AudioChannelSet::mono() || l.getMainInputChannelSet() == juce::AudioChannelSet::stereo());
}

void OneKnobProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    maxBlock = juce::jmax(512, samplesPerBlock);
    chain.prepare(sampleRate, maxBlock);
    bypassL.init(chain.latency() + 8);
    bypassR.init(chain.latency() + 8);
    fader.prepare(sampleRate, chain.latency(), maxBlock);
    setLatencySamples(chain.latency());
}

void OneKnobProcessor::processBlockBypassed(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    if (buffer.getNumChannels() < 2) return;
    const bool stereoIn = getTotalNumInputChannels() >= 2;
    const float d = (float)chain.latency();
    float* l = buffer.getWritePointer(0);
    float* r = buffer.getWritePointer(1);
    for (int i = 0; i < buffer.getNumSamples(); ++i) {
        const float x0 = l[i], x1 = stereoIn ? r[i] : x0;
        bypassL.push(x0); bypassR.push(x1);
        l[i] = bypassL.read(d); r[i] = bypassR.read(d);
    }
}

void OneKnobProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    juce::ScopedNoDenormals nd;
    if (buffer.getNumChannels() < 2) return;
    const bool stereoIn = getTotalNumInputChannels() >= 2;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm()) chain.setBpm(*bpm);

    const float k = apvts.getRawParameterValue("char")->load() / 10.f;
    chain.setKeySel((int)std::lround(apvts.getRawParameterValue("key")->load()), (int)std::lround(apvts.getRawParameterValue("scale")->load()));
    float* l = buffer.getWritePointer(0);
    float* r = buffer.getWritePointer(1);
    const int n = buffer.getNumSamples();
    float peak = 0.f;
    // bypass honoré ici (JUCE n'appelle pas processBlockBypassed quand un paramètre de bypass est déclaré) : entrée retardée + fondu de 10 ms
    fader.run(l, r, n, stereoIn, bypass != nullptr && bypass->get(), [&](float* cl, float* cr, int c) {
        chain.setKnob(k, voicing());
        chain.process(cl, stereoIn ? cr : nullptr, cl, cr, c);
        peak = juce::jmax(peak, chain.outLevel());
    });
    level.store(peak);
    reduction.store(chain.gainReduction());
    keyPc.store(chain.keyKnown() ? chain.key() : -1);
    keyScale.store(chain.scale());
    keyState.store(chain.keyStatus());
}

void OneKnobProcessor::getStateInformation(juce::MemoryBlock& dest) {
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary(*xml, dest);
}
void OneKnobProcessor::setStateInformation(const void* data, int size) {
    if (auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(apvts.state.getType())) apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorEditor* OneKnobProcessor::createEditor() { return new OneKnobEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new OneKnobProcessor(); }
