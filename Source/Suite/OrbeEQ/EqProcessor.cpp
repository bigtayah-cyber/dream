#include "EqProcessor.h"
#include "EqEditor.h"

using kit::u8;
using namespace of;

juce::AudioProcessorValueTreeState::ParameterLayout EqProcessor::makeLayout() {
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    const auto d = eq::factory();
    StringArray types{u8("Cloche"), u8("Plateau grave"), u8("Plateau aigu"), u8("Passe-haut"), u8("Passe-bas"), u8("Notch")};
    for (int i = 0; i < eq::kBands; ++i) {
        const String n = "b" + String(i + 1), nm = "Bande " + String(i + 1) + " ";
        l.add(fparam((n + "f").toRawUTF8(), nm + u8("Fr\xC3\xA9quence"), 20.f, 20000.f, 0.01f, d.b[i].freq, fmtHz, 1000.f, parseHz));
        l.add(fparam((n + "g").toRawUTF8(), nm + "Gain", -24.f, 24.f, 0.01f, 0.f, fmtDb));
        l.add(fparam((n + "q").toRawUTF8(), nm + "Q", 0.1f, 18.f, 0.01f, d.b[i].q, [](float v) { return String(v, 2); }, 1.f));
        l.add(std::make_unique<AudioParameterChoice>(ParameterID{(n + "t").toRawUTF8(), 1}, nm + "Type", types, d.b[i].type));
        // choix à deux états : se cale toujours sur 0 ou 1 (un booléen JUCE garde les valeurs intermédiaires d'un hôte)
        l.add(std::make_unique<AudioParameterChoice>(ParameterID{(n + "on").toRawUTF8(), 1}, nm + "Actif", StringArray{u8("Coup\xC3\xA9" "e"), "Active"}, d.b[i].on ? 1 : 0));
    }
    l.add(fparam("out", "Gain de sortie", -18.f, 18.f, 0.1f, 0.f, fmtDb));
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{"range", 1}, u8("\xC3\x89" "chelle"), StringArray{u8("\xC2\xB1" "12 dB"), u8("\xC2\xB1" "24 dB")}, 0));
    addBypass(l);
    return l;
}

EqProcessor::EqProcessor() : FxProcessor("Orbe EQ", "ORBEEQ", makeLayout()) {
    static const char* const kSuffix[5] = {"f", "g", "q", "t", "on"};
    for (int i = 0; i < eq::kBands; ++i)
        for (int k = 0; k < 5; ++k) rawPtr[(size_t)i][(size_t)k] = apvts.getRawParameterValue(("b" + juce::String(i + 1) + kSuffix[k]).toRawUTF8());
    outPtr = apvts.getRawParameterValue("out");
    for (auto* p : getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*>(p)) if (r->getParameterID() != "bypass" && r->getParameterID() != "range") abParams.push_back(r);
}

eq::Params EqProcessor::readParams() const {   // appelé dans processBlock : pointeurs préchargés, aucune chaîne ni allocation
    eq::Params p;
    for (int i = 0; i < eq::kBands; ++i) {
        auto& b = p.b[i]; const auto& q = rawPtr[(size_t)i];
        b.freq = q[0]->load(); b.gain = q[1]->load(); b.q = q[2]->load();
        b.type = juce::roundToInt(q[3]->load()); b.on = q[4]->load() > 0.5f;
    }
    p.out = outPtr->load();
    return p;
}

void EqProcessor::applyPreset(int k) {
    preset = juce::jlimit(0, eq::kPresets - 1, k);
    const auto p = eq::presetParams(preset);
    for (int i = 0; i < eq::kBands; ++i) {
        setReal(par(i, "f"), p.b[i].freq); setReal(par(i, "g"), p.b[i].gain); setReal(par(i, "q"), p.b[i].q);
        setReal(par(i, "t"), (float)p.b[i].type); setReal(par(i, "on"), p.b[i].on ? 1.f : 0.f);
    }
    setReal(apvts.getParameter("out"), p.out);
}

bool EqProcessor::matchesPreset() const {
    const auto a = readParams(), b = eq::presetParams(preset);
    for (int i = 0; i < eq::kBands; ++i) {
        const auto &x = a.b[i], &y = b.b[i];
        if (x.on != y.on || x.type != y.type) return false;
        if (std::fabs(std::log(x.freq / y.freq)) > 0.01f || std::fabs(x.q / y.q - 1.f) > 0.02f) return false;
        if (eq::hasGain(x.type) && std::fabs(x.gain - y.gain) > 0.06f) return false;
    }
    return std::fabs(a.out - b.out) < 0.06f;
}

void EqProcessor::resetBand(int i) {
    const auto d = eq::factory().b[i];
    setReal(par(i, "f"), d.freq); setReal(par(i, "g"), 0.f); setReal(par(i, "q"), d.q); setReal(par(i, "t"), (float)d.type); setReal(par(i, "on"), d.on ? 1.f : 0.f);
}

// Comparaison A/B : deux jeux complets de réglages ; la bascule échange le jeu courant et celui mis de côté.
void EqProcessor::switchAB(int to) {
    if (to == ab) return;
    auto& cur = stash[ab];
    cur.clear(); for (auto* p : abParams) cur.push_back(p->getValue());
    ab = to;
    if (stash[ab].size() == abParams.size()) for (size_t i = 0; i < abParams.size(); ++i) abParams[i]->setValueNotifyingHost(stash[ab][i]);
}

void EqProcessor::beforeSave() {
    auto& cur = stash[ab]; cur.clear(); for (auto* p : abParams) cur.push_back(p->getValue());
    for (int s = 0; s < 2; ++s) {
        juce::StringArray a; for (float v : stash[s]) a.add(juce::String(v, 5));
        apvts.state.setProperty("slot" + juce::String(s), a.joinIntoString(","), nullptr);
    }
    apvts.state.setProperty("ab", ab, nullptr); apvts.state.setProperty("preset", preset, nullptr);
}
void EqProcessor::afterLoad() {
    ab = juce::jlimit(0, 1, (int)apvts.state.getProperty("ab", 0)); preset = juce::jlimit(0, eq::kPresets - 1, (int)apvts.state.getProperty("preset", 0));
    for (int s = 0; s < 2; ++s) {
        juce::StringArray a; a.addTokens(apvts.state.getProperty("slot" + juce::String(s)).toString(), ",", "");
        stash[s].clear(); if (a.size() == (int)abParams.size()) for (auto& t : a) stash[s].push_back(juce::jlimit(0.f, 1.f, t.getFloatValue()));
    }
}

void EqProcessor::prepareToPlay(double sr, int block) {
    maxBlock = juce::jmax(512, block);
    engine.prepare(sr); mono.assign((size_t)maxBlock, 0.f);
    prepareBypass(sr, engine.latency(), maxBlock);
    setLatencySamples(engine.latency());
}

void EqProcessor::processBlock(juce::AudioBuffer<float>& b, juce::MidiBuffer&) {
    juce::ScopedNoDenormals nd;
    engine.set(readParams());
    stereo(b, maxBlock, [this](const float* il, const float* ir, float* ol, float* or_, int n) {
        for (int i = 0; i < n; ++i) mono[(size_t)i] = ir ? 0.5f * (il[i] + ir[i]) : il[i];
        analyzer.push(mono.data(), n);
        engine.process(il, ir, ol, or_, n);
    });
}

juce::AudioProcessorEditor* EqProcessor::createEditor() { return new EqEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new EqProcessor(); }
