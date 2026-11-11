#include "OrbeFxProcessor.h"
#include "OrbeFxEditor.h"
#include "../../Kit/Sanitize.h"
#include "../../Kit/StateVersion.h"
#include "../../Kit/v3/Format.h"

using juce::String;
using kit::u8;
namespace fmt = kit::v3::fmt;

namespace {
struct PV { const char* id; float v; };
struct Preset { const char* name; std::vector<PV> pv; };
// Un préréglage ne touche ni Mix, ni Sortie, ni le contournement : seulement les modules et les macros (le reste revient aux valeurs d'origine).
const std::vector<Preset>& presets() {
    static const std::vector<Preset> p = {
        {"Neutre", {}},
        {"Voix lo-fi", {{"fmode", 0}, {"fcut", 4500}, {"fres", 0.25f}, {"stype", 2}, {"sdrive", 8}, {"stone", 0.4f}, {"smix", 0.5f}, {"rsize", 0.3f}, {"rdecay", 1.0f}, {"rmix", 0.12f}}},
        {"Cassette", {{"fcut", 7000}, {"stype", 0}, {"sdrive", 10}, {"stone", 0.5f}, {"smix", 0.7f}, {"crate", 0.4f}, {"cdepth", 0.3f}, {"cmix", 0.3f}, {"dtime", 1}, {"dfb", 0.3f}, {"dtone", 0.35f}, {"dmix", 0.15f}}},
        {"Radio", {{"fmode", 1}, {"fcut", 1800}, {"fres", 0.45f}, {"stype", 1}, {"sdrive", 14}, {"stone", 0.6f}, {"smix", 0.6f}}},
        {"Chambre sombre", {{"fcut", 3000}, {"fres", 0.1f}, {"rsize", 0.35f}, {"rdecay", 1.2f}, {"rpre", 8}, {"rdamp", 0.7f}, {"rmix", 0.25f}}},
        {"Cathédrale", {{"fmode", 2}, {"fcut", 120}, {"rsize", 0.95f}, {"rdecay", 7.f}, {"rpre", 40}, {"rdamp", 0.4f}, {"rmix", 0.4f}}},
        {"Ensemble large", {{"crate", 0.35f}, {"cdepth", 0.7f}, {"cspread", 1.f}, {"cmix", 0.5f}, {"rsize", 0.4f}, {"rdecay", 1.5f}, {"rmix", 0.15f}}},
        {"Écho dub", {{"fcut", 5000}, {"dtime", 2}, {"dfb", 0.6f}, {"dtone", 0.4f}, {"dping", 1}, {"dmix", 0.4f}, {"rsize", 0.5f}, {"rdecay", 2.f}, {"rmix", 0.2f}}},
        {"Trap vocal", {{"fmode", 2}, {"fcut", 100}, {"stype", 0}, {"sdrive", 4}, {"smix", 0.3f}, {"dtime", 1}, {"dfb", 0.3f}, {"dmix", 0.25f}, {"rsize", 0.5f}, {"rdecay", 2.5f}, {"rpre", 30}, {"rmix", 0.2f}}},
        {"Bande tordue", {{"crate", 0.15f}, {"cdepth", 0.9f}, {"cmix", 0.5f}, {"stype", 2}, {"sdrive", 12}, {"smix", 0.6f}, {"dtime", 3}, {"dfb", 0.5f}, {"dping", 1}, {"dmix", 0.2f}}},
        {"Pad flottant", {{"fcut", 6000}, {"crate", 0.25f}, {"cdepth", 0.6f}, {"cmix", 0.4f}, {"rsize", 0.9f}, {"rdecay", 9.f}, {"rpre", 60}, {"rdamp", 0.3f}, {"rmix", 0.55f}}},
        {"Granuleux", {{"fmode", 1}, {"fcut", 900}, {"fres", 0.6f}, {"fenv", 0.6f}, {"stype", 1}, {"sdrive", 20}, {"stone", 0.6f}, {"smix", 0.8f}, {"dtime", 0}, {"dfb", 0.45f}, {"dmix", 0.25f}}},
    };
    return p;
}
// Identifiants remis à leur valeur d'origine avant d'appliquer un préréglage (tout sauf Mix, Sortie, contournement).
const char* const kModuleIds[] = {"fon", "fmode", "fcut", "fres", "fenv", "son", "stype", "sdrive", "stone", "smix", "con", "crate", "cdepth", "cspread", "cmix",
                                  "don", "dtime", "dfb", "dtone", "dping", "dmix", "ron", "rsize", "rdecay", "rpre", "rdamp", "rmix", "mcol", "mgrain", "mmove", "mspace"};

float parseHz(const String& s) { const double v = s.retainCharacters("0123456789.,").replace(",", ".").getDoubleValue(); const bool k = s.containsIgnoreCase("k"); return (float)((k || v < 20.0) ? v * 1000.0 : v); }
}  // namespace

int OrbeFxProcessor::numPresets() { return (int)presets().size(); }

juce::AudioProcessorValueTreeState::ParameterLayout OrbeFxProcessor::makeLayout() {
    using juce::AudioParameterChoice, juce::AudioParameterFloat, juce::NormalisableRange, juce::ParameterID;
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    const ofx::Params d;
    auto flt = [&](const char* id, const String& name, float lo, float hi, float def, std::function<String(float, int)> txt, float centre = 0.f, std::function<float(const String&)> parse = nullptr) {
        NormalisableRange<float> r(lo, hi); if (centre > 0.f) r.setSkewForCentre(centre);
        auto a = juce::AudioParameterFloatAttributes().withStringFromValueFunction(txt);
        if (parse) a = a.withValueFromStringFunction(parse);
        l.add(std::make_unique<AudioParameterFloat>(ParameterID{id, 1}, name, r, def, a));
    };
    auto chc = [&](const char* id, const String& name, juce::StringArray c, int def) { l.add(std::make_unique<AudioParameterChoice>(ParameterID{id, 1}, name, c, def)); };
    auto pct = [](float v, int) { return fmt::pct(v); };
    auto onOff = juce::StringArray{"Off", "On"};
    // filtre
    chc("fon", "Filtre", onOff, 1);
    chc("fmode", "Filtre : mode", {"Passe-bas", "Passe-bande", "Passe-haut"}, 0);
    flt("fcut", "Filtre : fr\xC3\xA9quence", 20.f, 20000.f, d.fCut, [](float v, int) { return fmt::hz(v); }, 1000.f, parseHz);
    flt("fres", "Filtre : r\xC3\xA9sonance", 0.f, 1.f, d.fRes, pct);
    flt("fenv", "Filtre : suivi", 0.f, 1.f, d.fEnv, pct);
    // saturation
    chc("son", "Saturation", onOff, 1);
    chc("stype", "Saturation : type", {"Doux", "Dur", "Asym."}, 0);
    flt("sdrive", "Saturation : drive", 0.f, 30.f, d.sDrive, [](float v, int) { return fmt::db(v); });
    flt("stone", "Saturation : ton", 0.f, 1.f, d.sTone, pct);
    flt("smix", "Saturation : mix", 0.f, 1.f, d.sMix, pct);
    // chorus
    chc("con", "Chorus", onOff, 1);
    flt("crate", "Chorus : vitesse", 0.05f, 6.f, d.cRate, [](float v, int) { return fmt::withUnit(fmt::num(v, 2), "Hz"); }, 0.6f);
    flt("cdepth", "Chorus : profondeur", 0.f, 1.f, d.cDepth, pct);
    flt("cspread", "Chorus : largeur", 0.f, 1.f, d.cSpread, pct);
    flt("cmix", "Chorus : mix", 0.f, 1.f, d.cMix, pct);
    // delay
    chc("don", "Delay", onOff, 1);
    chc("dtime", "Delay : dur\xC3\xA9" "e", {"1/16", "1/8", "1/8 pt", "1/4", "1/4 pt", "1/2", "1/2 pt"}, d.dTime);
    flt("dfb", "Delay : r\xC3\xA9" "injection", 0.f, 0.95f, d.dFb, pct);
    flt("dtone", "Delay : ton", 0.f, 1.f, d.dTone, pct);
    chc("dping", "Delay : mode", {u8("St\xC3\xA9r\xC3\xA9o"), "Ping-pong"}, 0);
    flt("dmix", "Delay : mix", 0.f, 1.f, d.dMix, pct);
    // réverbe
    chc("ron", u8("R\xC3\xA9verbe"), onOff, 1);
    flt("rsize", "R\xC3\xA9verbe : taille", 0.f, 1.f, d.rSize, pct);
    flt("rdecay", "R\xC3\xA9verbe : dur\xC3\xA9" "e", 0.3f, 12.f, d.rDecay, [](float v, int) { return fmt::withUnit(fmt::num(v, 1), "s"); }, 2.5f);
    flt("rpre", "R\xC3\xA9verbe : pr\xC3\xA9-retard", 0.f, 120.f, d.rPre, [](float v, int) { return fmt::ms(v); });
    flt("rdamp", "R\xC3\xA9verbe : amorti", 0.f, 1.f, d.rDamp, pct);
    flt("rmix", "R\xC3\xA9verbe : mix", 0.f, 1.f, d.rMix, pct);
    // macros (s'ajoutent aux modules, n'écrivent aucun paramètre)
    flt("mcol", "Macro : couleur", -1.f, 1.f, 0.f, [](float v, int) { return fmt::signedNum(v * 100.0, 0) + String::charToString((juce::juce_wchar)0x00A0) + "%"; });
    flt("mgrain", "Macro : grain", 0.f, 1.f, 0.f, pct);
    flt("mmove", "Macro : mouvement", 0.f, 1.f, 0.f, pct);
    flt("mspace", "Macro : espace", 0.f, 1.f, 0.f, pct);
    // sortie
    flt("mix", "Mix", 0.f, 1.f, 1.f, pct);
    flt("gain", "Sortie", -24.f, 12.f, 0.f, [](float v, int) { return fmt::db(v); });
    chc("bypass", "Bypass", onOff, 0);   // contournement en « choix » à deux états (aucun booléen) ; 0 = en service
    return l;
}

OrbeFxProcessor::OrbeFxProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true).withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "ORBEFX", makeLayout()) {
    bypass = apvts.getParameter("bypass");
    setLatencySamples(0);
}

ofx::Params OrbeFxProcessor::readParams() const {
    ofx::Params p;
    auto ch = [this](const char* id) { return juce::roundToInt(raw(id)); };
    p.fOn = ch("fon") == 1; p.fMode = ch("fmode"); p.fCut = raw("fcut"); p.fRes = raw("fres"); p.fEnv = raw("fenv");
    p.sOn = ch("son") == 1; p.sType = ch("stype"); p.sDrive = raw("sdrive"); p.sTone = raw("stone"); p.sMix = raw("smix");
    p.cOn = ch("con") == 1; p.cRate = raw("crate"); p.cDepth = raw("cdepth"); p.cSpread = raw("cspread"); p.cMix = raw("cmix");
    p.dOn = ch("don") == 1; p.dTime = ch("dtime"); p.dFb = raw("dfb"); p.dTone = raw("dtone"); p.dPing = ch("dping"); p.dMix = raw("dmix");
    p.rOn = ch("ron") == 1; p.rSize = raw("rsize"); p.rDecay = raw("rdecay"); p.rPre = raw("rpre"); p.rDamp = raw("rdamp"); p.rMix = raw("rmix");
    p.mCol = raw("mcol"); p.mGrain = raw("mgrain"); p.mMove = raw("mmove"); p.mSpace = raw("mspace");
    p.mix = raw("mix"); p.gainDb = raw("gain");
    const float b = hostBpm.load();
    p.bpm = b > 0.f ? (double)b : 120.0;
    return p;
}

int OrbeFxProcessor::getNumPrograms() { return numPresets(); }
const String OrbeFxProcessor::getProgramName(int i) { return i >= 0 && i < numPresets() ? u8(presets()[(size_t)i].name) : String(); }

void OrbeFxProcessor::setCurrentProgram(int i) {
    if (i < 0 || i >= numPresets()) return;
    program = i;
    auto put = [this](const char* id, float v) { if (auto* q = apvts.getParameter(id)) q->setValueNotifyingHost(q->convertTo0to1(v)); };
    for (auto id : kModuleIds) if (auto* q = apvts.getParameter(id)) q->setValueNotifyingHost(q->getDefaultValue());
    for (auto& pv : presets()[(size_t)i].pv) put(pv.id, pv.v);
}

bool OrbeFxProcessor::presetMatches(int i) const {
    if (i < 0 || i >= numPresets()) return false;
    for (auto id : kModuleIds) {
        auto* q = apvts.getParameter(id); if (q == nullptr) continue;
        float want = q->convertFrom0to1(q->getDefaultValue());
        for (auto& pv : presets()[(size_t)i].pv) if (String(pv.id) == id) want = pv.v;
        if (std::fabs(want - raw(id)) > 0.002f * std::max(1.f, std::fabs(want))) return false;
    }
    return true;
}

double OrbeFxProcessor::getTailLengthSeconds() const {
    const auto p = readParams();
    double t = 0.0;
    if (p.rOn && p.rMix + 0.6f * p.mSpace > 0.01f) t = std::max(t, (double)p.rDecay * (1.0 + 0.6 * p.mSpace) + 0.2);
    if (p.dOn && p.dMix + 0.45f * p.mSpace > 0.01f) t = std::max(t, 60.0 / p.bpm * ofx::delayBeats(p.dTime) * (1.0 + 6.0 * p.dFb));
    return std::min(t, 30.0);
}

void OrbeFxProcessor::prepareToPlay(double sampleRate, int maxBlock) {
    sr = sampleRate;
    engine.prepare(sampleRate);
    engine.set(readParams()); engine.snapAll();
    fader.prepare(sampleRate, 0, juce::jlimit(64, 8192, juce::jmax(512, maxBlock)));
    setLatencySamples(0);
}

void OrbeFxProcessor::processBlock(juce::AudioBuffer<float>& b, juce::MidiBuffer&) {
    juce::ScopedNoDenormals nd;
    const int n = b.getNumSamples();
    if (b.getNumChannels() < 2 || n <= 0) return;
    float* l = b.getWritePointer(0); float* r = b.getWritePointer(1);
    const bool stereoIn = getTotalNumInputChannels() >= 2;
    kit::sanitize(l, n);
    if (stereoIn) kit::sanitize(r, n); else std::copy(l, l + n, r);
    for (int i = 0; i < n; ++i) { l[i] = juce::jlimit(-32.f, 32.f, l[i]); r[i] = juce::jlimit(-32.f, 32.f, r[i]); }   // entrée démesurée : bornée (+30 dBFS)
    if (auto* ph = getPlayHead()) if (auto pos = ph->getPosition()) if (auto bpm = pos->getBpm()) hostBpm.store((float)*bpm);
    float pi = 0.f; for (int i = 0; i < n; ++i) pi = std::max(pi, std::max(std::fabs(l[i]), std::fabs(r[i])));
    engine.set(readParams());
    fader.run(l, r, n, true, raw("bypass") >= 0.5f, [this](float* cl, float* cr, int c) { engine.process(cl, cr, c); });
    float po = 0.f; for (int i = 0; i < n; ++i) po = std::max(po, std::max(std::fabs(l[i]), std::fabs(r[i])));
    inPeak.store(std::max(inPeak.load(), pi)); outPeak.store(std::max(outPeak.load(), po));
}

void OrbeFxProcessor::getStateInformation(juce::MemoryBlock& dest) {
    auto tree = apvts.copyState();
    kit::stampState(tree);
    tree.setProperty("program", program, nullptr);
    if (auto xml = tree.createXml()) copyXmlToBinary(*xml, dest);
}

// Un état de l'ancien « Orbe FX » (balise « ORBE », le moteur d'Orbe) n'a rien de commun avec cette chaîne : il est ignoré, le plugin garde ses réglages.
void OrbeFxProcessor::setStateInformation(const void* data, int size) {
    auto xml = getXmlFromBinary(data, size);
    if (!xml || !xml->hasTagName(apvts.state.getType())) return;
    const auto tree = juce::ValueTree::fromXml(*xml);
    apvts.replaceState(tree);
    program = juce::jlimit(0, numPresets() - 1, (int)tree.getProperty("program", 0));
    engine.set(readParams()); engine.snapAll();
}

juce::AudioProcessorEditor* OrbeFxProcessor::createEditor() { return new OrbeFxEditor(*this); }

#ifndef ORBE_NO_FACTORY
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new OrbeFxProcessor(); }
#endif
