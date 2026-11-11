#include "CabineProcessor.h"
#include "../../Kit/StateVersion.h"
#include "CabineEditor.h"
#include "../../Kit/Kit.h"
#include "../../Kit/Sanitize.h"

using kit::u8;
namespace {
struct Def { const char* id; const char* name; float lo, hi, step, centre; const char* unit; float cab::Params::*field; };
const Def kDefs[] = {
    {"hp",   "Passe-haut",  20.f, 800.f, 1.f, 120.f, " Hz", &cab::Params::hp},
    {"mud",  "Boue 300 Hz", -12.f, 12.f, 0.1f, 0.f, " dB", &cab::Params::mud},
    {"pres", "Pr\xC3\xA9sence 4 kHz", -12.f, 12.f, 0.1f, 0.f, " dB", &cab::Params::pres},
    {"air",  "Air 10 kHz",  -12.f, 12.f, 0.1f, 0.f, " dB", &cab::Params::air},
    {"lp",   "Passe-bas",   1000.f, 20000.f, 10.f, 5000.f, " Hz", &cab::Params::lp},
    {"sat",  "Saturation",  0.f, 10.f, 0.01f, 0.f, "", &cab::Params::sat},
    {"thr",  "Seuil",       -60.f, 0.f, 0.1f, 0.f, " dB", &cab::Params::thr},
    {"ratio","Ratio",       1.f, 10.f, 0.1f, 3.f, ":1", &cab::Params::ratio},
    {"atk",  "Attaque",     0.5f, 100.f, 0.1f, 8.f, " ms", &cab::Params::atk},
    {"rel",  "Rel\xC3\xA2" "che", 20.f, 500.f, 1.f, 120.f, " ms", &cab::Params::rel},
    {"mk",   "Makeup",      -6.f, 12.f, 0.1f, 0.f, " dB", &cab::Params::mk},
    {"dbl",  "Doubleur",    0.f, 1.f, 0.01f, 0.f, " %", &cab::Params::dbl},
    {"rev",  "Reverb",      0.f, 1.f, 0.01f, 0.f, " %", &cab::Params::rev},
    {"dly",  "Delay",       0.f, 1.f, 0.01f, 0.f, " %", &cab::Params::dly},
    {"out",  "Sortie",      -24.f, 12.f, 0.1f, 0.f, " dB", &cab::Params::out},
    // Lot E (ajoutés en fin : identifiants existants inchangés). Neutres par défaut ; un état d'avant la v3 les remet à 0 au chargement.
    {"ingain", "Entr\xC3\xA9" "e",  -24.f, 24.f, 0.1f, 0.f, " dB", &cab::Params::in},
    {"deess",  "Sifflantes", 0.f, 1.f, 0.01f, 0.f, " %", &cab::Params::des},
};
}  // namespace

juce::AudioProcessorValueTreeState::ParameterLayout CabineProcessor::makeLayout() {
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    StringArray names; for (int i = 0; i < cab::kPresets; ++i) names.add(presetName(i));
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{"preset", 1}, "Preset", names, cab::kDefaultPreset));
    const auto& def = cab::presets()[cab::kDefaultPreset].p;
    for (auto& d : kDefs) {
        NormalisableRange<float> r(d.lo, d.hi, d.step);
        if (d.centre > 0.f) r.setSkewForCentre(d.centre);
        const bool pct = String(d.unit) == " %";
        const float dv = String(d.id) == "ingain" ? 0.f : def.*d.field;   // gain d'entrée neutre ; sifflantes = celles du preset par défaut (v3)
        l.add(std::make_unique<AudioParameterFloat>(ParameterID{d.id, 1}, u8(d.name), r, dv,
            AudioParameterFloatAttributes().withStringFromValueFunction([d, pct](float v, int) {
                if (pct) return String(juce::roundToInt(v * 100.f)) + " %";
                if (String(d.id) == "lp" && v >= 19900.f) return u8("coup\xC3\xA9");
                const bool big = v >= 1000.f && String(d.unit) == " Hz";
                const String s = big ? String(v / 1000.f, 1) + " k" : String(v, d.step < 0.5f ? 1 : 0);
                return s + (big ? String("Hz") : String(d.unit)); })));
    }
    l.add(std::make_unique<AudioParameterBool>(ParameterID{"bypass", 1}, "Bypass", false));
    return l;
}

CabineProcessor::CabineProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true).withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "ORBECABINE", makeLayout()) {
    bypass = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter("bypass"));
}

bool CabineProcessor::isBusesLayoutSupported(const BusesLayout& l) const {
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() &&
           (l.getMainInputChannelSet() == juce::AudioChannelSet::mono() || l.getMainInputChannelSet() == juce::AudioChannelSet::stereo());
}

cab::Params CabineProcessor::readParams() const {
    cab::Params p;
    for (auto& d : kDefs) p.*d.field = apvts.getRawParameterValue(d.id)->load();
    return p;
}

float CabineProcessor::presetValue(const juce::String& id, int i) {
    for (auto& d : kDefs) if (id == d.id) return id == "out" ? 0.f : cab::presets()[juce::jlimit(0, cab::kPresets - 1, i)].p.*d.field;
    return 0.f;
}

void CabineProcessor::applyPreset(int i) {
    i = juce::jlimit(0, cab::kPresets - 1, i);
    const auto& p = cab::presets()[i].p;
    if (auto* pc = apvts.getParameter("preset")) pc->setValueNotifyingHost(pc->convertTo0to1((float)i));
    for (auto& d : kDefs) if (auto* a = apvts.getParameter(d.id)) {
        if (std::string(d.id) == "out") { a->setValueNotifyingHost(a->convertTo0to1(0.f)); continue; }
        a->setValueNotifyingHost(a->convertTo0to1(p.*d.field));
    }
}

bool CabineProcessor::matchesPreset() const {
    const auto& p = cab::presets()[currentPreset()].p; const auto cur = readParams();
    for (auto& d : kDefs) {
        const float want = std::string(d.id) == "out" ? 0.f : p.*d.field, tol = (d.hi - d.lo) * 0.004f;
        if (std::fabs(cur.*d.field - want) > tol) return false;
    }
    return true;
}

void CabineProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    maxBlock = juce::jmax(512, samplesPerBlock);
    engine.prepare(sampleRate, maxBlock);
    byp.init(engine.latency());
    fader.prepare(sampleRate, engine.latency(), maxBlock);
    setLatencySamples(engine.latency());
}

void CabineProcessor::processBlockBypassed(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    if (buffer.getNumChannels() < 2) return;
    byp.process(buffer.getWritePointer(0), buffer.getWritePointer(1), buffer.getNumSamples(), getTotalNumInputChannels() >= 2);
}

void CabineProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    juce::ScopedNoDenormals nd;
    if (buffer.getNumChannels() < 2) return;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm()) engine.setBpm(*bpm);
    engine.set(readParams());
    const bool stereoIn = getTotalNumInputChannels() >= 2;
    float* l = buffer.getWritePointer(0);
    float* r = buffer.getWritePointer(1);
    const int n = buffer.getNumSamples();
    kit::sanitize(l, n); if (stereoIn) kit::sanitize(r, n);   // NaN/inf d'un plugin précédent : remplacés par 0
    float pk = 0.f, gr = 0.f, ms = 0.f;
    fader.run(l, r, n, stereoIn, bypass != nullptr && bypass->get(), [&](float* cl, float* cr, int c) {   // bypass honoré : entrée retardée + fondu de 10 ms
        engine.process(cl, stereoIn ? cr : nullptr, cl, cr, c);
        pk = juce::jmax(pk, engine.outPeak()); gr = juce::jmax(gr, engine.gainReduction()); ms = juce::jmax(ms, engine.outMeanSquare());
    });
    peak.store(pk); level.store(std::sqrt(ms)); reduction.store(gr);
}

void CabineProcessor::getStateInformation(juce::MemoryBlock& dest) {
    auto st = apvts.copyState();
    kit::stampState(st, kit::kStateVersionDsp);
    if (auto xml = st.createXml()) copyXmlToBinary(*xml, dest);
}
void CabineProcessor::setStateInformation(const void* data, int size) {
    if (auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(apvts.state.getType())) {
            auto tree = juce::ValueTree::fromXml(*xml);
            const int ver = kit::stateVersionOf(tree);
            apvts.replaceState(tree);
            if (ver < kit::kStateVersionDsp)   // projet d'avant : sans de-esser ni gain d'entrée, son inchangé
                for (const char* id : {"ingain", "deess"}) if (auto* a = apvts.getParameter(id)) a->setValueNotifyingHost(a->convertTo0to1(0.f));
        }
}

juce::AudioProcessorEditor* CabineProcessor::createEditor() { return new CabineEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new CabineProcessor(); }
