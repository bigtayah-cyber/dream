#include "OneKnobProcessor.h"
#include "OneKnobEditor.h"
#include "../Kit/Sanitize.h"

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
    // volet avancé
    auto fr = [](float v, int dec, const char* unit, bool sign) {
        const bool zero = std::abs(v) < 0.5f * std::pow(10.f, (float)-dec);
        juce::String t = dec == 0 ? juce::String(juce::roundToInt(v)) : juce::String(zero ? 0.f : v, dec).replaceCharacter('.', ',');
        if (sign && !zero && v > 0.f) t = "+" + t;
        return t + unit;
    };
    auto db = juce::AudioParameterFloatAttributes().withLabel("dB").withStringFromValueFunction([fr](float v, int) { return fr(v, 1, " dB", true); });
    auto pct = juce::AudioParameterFloatAttributes().withLabel("%").withStringFromValueFunction([fr](float v, int) { return fr(v, 0, " %", false); });
    l.add(std::make_unique<AudioParameterFloat>(ParameterID{"trim", 1}, juce::String::fromUTF8("Entrée"), NormalisableRange<float>(-12.f, 12.f, 0.1f), 0.f, db));
    l.add(std::make_unique<AudioParameterBool>(ParameterID{"autoin", 1}, juce::String::fromUTF8("Auto-entrée"), true));
    l.add(std::make_unique<AudioParameterFloat>(ParameterID{"tone", 1}, "Ton", NormalisableRange<float>(-6.f, 6.f, 0.1f), 0.f, db));
    l.add(std::make_unique<AudioParameterFloat>(ParameterID{"mix", 1}, "Mix", NormalisableRange<float>(0.f, 100.f, 1.f), 100.f, pct));
    l.add(std::make_unique<AudioParameterFloat>(ParameterID{"out", 1}, "Sortie", NormalisableRange<float>(-12.f, 12.f, 0.1f), 0.f, db));
    l.add(std::make_unique<AudioParameterFloat>(ParameterID{"spec", 1}, juce::String::fromUTF8(voicing().specName), NormalisableRange<float>(0.f, 100.f, 1.f),
                                                voicing().specDefault * 100.f, pct));
    l.add(std::make_unique<AudioParameterBool>(ParameterID{"autogain", 1}, "Auto-gain", true));
    // ajoutés en fin de liste (refonte, version d'état 2) : des choix, jamais des booléens
    l.add(std::make_unique<juce::AudioParameterChoice>(ParameterID{"opt", 1}, juce::String::fromUTF8(voicing().optName),
                                                       juce::StringArray{"Non", "Oui"}, voicing().optDefault > 0.5f ? 1 : 0));
    l.add(std::make_unique<juce::AudioParameterChoice>(ParameterID{"engine", 1}, "Moteur", juce::StringArray{"Actuel", "Classique"}, 0,
                                                       juce::AudioParameterChoiceAttributes().withAutomatable(false)));
    return l;
}

OneKnobProcessor::OneKnobProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "ONEKNOB", makeLayout()) {
    bypass = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter("bypass"));
    pChar = apvts.getRawParameterValue("char"); pKey = apvts.getRawParameterValue("key"); pScale = apvts.getRawParameterValue("scale");
    pTrim = apvts.getRawParameterValue("trim"); pAutoIn = apvts.getRawParameterValue("autoin"); pTone = apvts.getRawParameterValue("tone");
    pMix = apvts.getRawParameterValue("mix"); pOut = apvts.getRawParameterValue("out"); pSpec = apvts.getRawParameterValue("spec");
    pAutoGain = apvts.getRawParameterValue("autogain");
    pOpt = apvts.getRawParameterValue("opt"); pEngine = apvts.getRawParameterValue("engine");
}

bool OneKnobProcessor::isBusesLayoutSupported(const BusesLayout& l) const {
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() &&
           (l.getMainInputChannelSet() == juce::AudioChannelSet::mono() || l.getMainInputChannelSet() == juce::AudioChannelSet::stereo());
}

void OneKnobProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    maxBlock = juce::jmax(512, samplesPerBlock);
    sr = sampleRate;
    vu.reset();
    chain.select(index());
    chain.setClassic(pEngine->load() > 0.5f);
    chain.prepare(sampleRate, maxBlock);
    // retards dimensionnés pour la plus grande latence possible (moteur Classique ou actuel) : la bascule ne réalloue rien
    maxLat = chain.maxLatency();
    bypassL.init(maxLat + 8);
    bypassR.init(maxLat + 8);
    fader.prepare(sampleRate, maxLat, maxBlock);
    lastLat = chain.latency();
    fader.prepare(sampleRate, lastLat, maxBlock);
    setLatencySamples(lastLat);
}

void OneKnobProcessor::processBlockBypassed(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    if (buffer.getNumChannels() < 2) return;
    const bool stereoIn = getTotalNumInputChannels() >= 2;
    const float d = (float)lastLat;
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
    // G2 : un NaN/inf venu d'un plugin précédent est remplacé par 0 (sinon il empoisonne filtres, reverbs et compresseurs)
    for (int c = 0; c < 2; ++c) kit::sanitize(buffer.getWritePointer(c), buffer.getNumSamples());
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm()) chain.setBpm(*bpm);

    const float k = pChar->load() / 10.f;
    chain.setKeySel((int)std::lround(pKey->load()), (int)std::lround(pScale->load()));
    onek::Extras ex;
    ex.trimDb = pTrim->load(); ex.autoIn = pAutoIn->load() > 0.5f; ex.toneDb = pTone->load(); ex.mix = pMix->load() / 100.f;
    ex.outDb = pOut->load(); ex.special = pSpec->load() / 100.f; ex.autoGain = pAutoGain->load() > 0.5f;
    ex.option = pOpt->load() > 0.5f ? 1.f : 0.f;
    chain.setExtras(ex);
    chain.setClassic(pEngine->load() > 0.5f);
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
    if (chain.latency() != lastLat) {   // bascule Actuel <-> Classique terminée : nouvelle latence annoncée à l'hôte
        lastLat = chain.latency();
        fader.prepare(sr, lastLat, maxBlock);   // sans allocation : dimensionné pour maxLat au prepareToPlay
        setLatencySamples(lastLat);
    }
    {   // VU de sortie : RMS du bloc (après bypass), balistique 300 ms
        double s2 = 0;
        for (int i = 0; i < n; ++i) s2 += 0.5 * ((double)l[i] * l[i] + (double)r[i] * r[i]);
        vu.step(std::sqrt(s2 / juce::jmax(1, n)), n / sr);
        vuDb.store((float)vu.vu());
    }
    inDb.store(chain.inputDb());
    autoInDb.store(chain.autoInGainDb());
    level.store(peak);
    reduction.store(chain.gainReduction());
    keyPc.store(chain.keyKnown() ? chain.key() : -1);
    keyScale.store(chain.scale());
    keyState.store(chain.keyStatus());
}

void OneKnobProcessor::getStateInformation(juce::MemoryBlock& dest) {
    auto st = apvts.copyState();
    st.setProperty("stateVersion", onek::kStateVersion, nullptr);
    if (auto xml = st.createXml()) copyXmlToBinary(*xml, dest);
}
void OneKnobProcessor::setStateInformation(const void* data, int size) {
    if (auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(apvts.state.getType())) {
            const int version = xml->getIntAttribute("stateVersion", 0);
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
            // projet enregistré avant la refonte : on le rouvre avec l'ancien son (moteur Classique, ancienne latence)
            if (auto* e = apvts.getParameter("engine"))
                if (onek::classicForStateVersion(version)) e->setValueNotifyingHost(1.f);
        }
}

juce::AudioProcessorEditor* OneKnobProcessor::createEditor() { return new OneKnobEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new OneKnobProcessor(); }
