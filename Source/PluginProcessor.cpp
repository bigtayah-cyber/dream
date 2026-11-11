#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace {
// Un preset ne règle jamais la tonalité ni la gamme (elles dépendent du morceau, pas du son) : l'artiste garde les siennes.
// Sortie à 0 dB partout : Mordant est maintenant compensé en niveau, plus besoin de baisser la sortie pour comparer à niveau égal.
struct Preset { const char* name; float tension, mordant, brume, halo, mix, out; };

const Preset kPresets[] = {
    {"Verre froid",  0.95f, 0.20f, 0.30f, 0.30f, 1.00f,  0.0f},
    {"Machoire",     0.85f, 0.65f, 0.15f, 0.20f, 1.00f,  0.0f},
    {"Salle vide",   0.60f, 0.10f, 0.70f, 0.25f, 0.90f,  0.0f},
    {"Halo rose",    0.75f, 0.15f, 0.45f, 0.80f, 1.00f,  0.0f},
    {"Tension pure", 1.00f, 0.00f, 0.10f, 0.00f, 1.00f,  0.0f},
    {"Vapeur",       0.40f, 0.30f, 0.85f, 0.45f, 0.85f,  0.0f},
    {"Casse",        0.90f, 0.95f, 0.25f, 0.30f, 1.00f,  0.0f},
    {"Cabine",       0.25f, 0.20f, 0.12f, 0.10f, 0.80f,  0.0f},
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
    // ajouté en dernier : les identifiants existants ne changent pas
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{"bypass", 1}, "Bypass", juce::StringArray{"Off", "On"}, 0));
    l.add(kit::v3::notes::makeAccidParam());   // écriture des notes (dièses / bémols), nom seulement : jamais le son
    return l;
}

VoileProcessor::VoileProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "VOILE", makeLayout()) {
    bypass = apvts.getParameter("bypass");
    apvts.state.setProperty("dspv", kDspVersion, nullptr);   // nouvelle instance : son v2 (Mordant compensé, Halo stéréo)
}

bool VoileProcessor::isBusesLayoutSupported(const BusesLayout& l) const {
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() &&
           (l.getMainInputChannelSet() == juce::AudioChannelSet::mono() ||
            l.getMainInputChannelSet() == juce::AudioChannelSet::stereo());
}

void VoileProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    engine.prepare(sampleRate);
    scopeSr = sampleRate > 1000.0 ? sampleRate : 48000.0;
    fader.prepare(sampleRate, engine.latency(), juce::jmax(4096, samplesPerBlock));
    setLatencySamples(engine.latency());
}

// Le paramètre « bypass » est déclaré : JUCE passe alors par processBlock. Si un hôte appelle tout de même processBlockBypassed,
// le moteur continue de tourner (sortie sèche retardée) : au retour, pas de vieille queue de reverb ou de delay figée.
void VoileProcessor::processBlockBypassed(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) { run(buffer, true); }

void VoileProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    run(buffer, bypass != nullptr && bypass->getValue() >= 0.5f);
}

void VoileProcessor::run(juce::AudioBuffer<float>& buffer, bool bypassed) {
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
    p.mordComp = p.haloStereo = dspVersion.load() >= 2;   // v1 (projet d'avant) : ancien son à l'identique
    engine.set(p);

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm()) engine.setBpm(*bpm);

    float* l = buffer.getWritePointer(0);
    float* r = buffer.getWritePointer(1);
    kit::sanitize(l, n);   // un NaN/inf venu d'un plugin précédent n'entre jamais dans le moteur (sinon NaN pour toujours)
    if (nIn >= 2) kit::sanitize(r, n);
    fader.run(l, r, n, nIn >= 2, bypassed, [&](float* cl, float* cr, int c) { engine.process(cl, nIn >= 2 ? cr : nullptr, cl, cr, c); });
    level.store(engine.level());
    shift.store(engine.shiftSemis());

    // historique d'affichage : lecture de l'état existant du moteur, rien n'est recalculé ni modifié
    float sung = 0.f; int tgt = -1;
    const bool ok = !bypassed && engine.voiceReadout(sung, tgt);
    const float out = ok ? sung + engine.shiftSemis() : 0.f;
    scopeAcc += (double)n / scopeSr;
    for (int guard = 0; scopeAcc >= Scope::period && guard < Scope::N; ++guard) {
        scopeAcc -= Scope::period;
        const int w = scope.wr.load(std::memory_order_relaxed);
        scope.sung[w].store(ok ? sung : 0.f, std::memory_order_relaxed);
        scope.out[w].store(out, std::memory_order_relaxed);
        scope.tgt[w].store(ok ? (float)tgt : 0.f, std::memory_order_relaxed);
        scope.lvl[w].store(engine.level(), std::memory_order_relaxed);
        scope.wr.store((w + 1) % Scope::N, std::memory_order_release);
    }
    scope.key.store(engine.detectedKey(), std::memory_order_relaxed);
    scope.scale.store(engine.detectedScale(), std::memory_order_relaxed);
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
}

void VoileProcessor::getStateInformation(juce::MemoryBlock& dest) {
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary(*xml, dest);
}

void VoileProcessor::setStateInformation(const void* data, int size) {
    if (auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(apvts.state.getType())) {
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
            // état sans version = projet enregistré avant la v2 : on garde son ancien rendu
            if (!apvts.state.hasProperty("dspv")) apvts.state.setProperty("dspv", 1, nullptr);
            dspVersion.store((int)apvts.state.getProperty("dspv", 1));
        }
}

juce::AudioProcessorEditor* VoileProcessor::createEditor() { return new VoileEditor(*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new VoileProcessor(); }
