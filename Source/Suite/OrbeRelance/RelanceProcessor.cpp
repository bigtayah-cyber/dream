#include "RelanceProcessor.h"
#include "RelanceEditor.h"

using kit::u8;
using namespace of;

namespace {
std::unique_ptr<juce::AudioParameterChoice> choice(const char* id, const juce::String& name, juce::StringArray items, int def) {
    return std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{id, 1}, name, items, def);
}
juce::String pct(float v) { return String(juce::roundToInt(v * 100.f)) + " %"; }
juce::String integer(float v) { return String(juce::roundToInt(v)); }
juce::String msTxt(float v) { return String(juce::roundToInt(v)) + " ms"; }

// Préréglages d'usine. Mix, sortie (voix + relances / seules) et verrou de graine restent à l'utilisateur.
struct Preset { const char* name; void (*tune)(rel::Params&); };
const Preset kPresets[] = {
    {"M\xC3\xA9lange discret", [](rel::Params&) {}},
    {"\xC3\x89" "cho de fin de phrase", [](rel::Params& p) { p.character = 1; p.density = 0.6f; p.audace = 0.3f; p.delayMode = 0; p.lenMs = 300.f; }},
    {"Octave basse menace", [](rel::Params& p) { p.character = 2; p.density = 0.45f; p.audace = 0.5f; p.tone = 0.35f; p.levelDb = -1.f; }},
    {"T\xC3\xA9l\xC3\xA9phone", [](rel::Params& p) { p.character = 3; p.density = 0.5f; p.width = 0.8f; p.levelDb = -2.f; }},
    {"R\xC3\xA9ponse invers\xC3\xA9" "e", [](rel::Params& p) { p.character = 4; p.density = 0.4f; p.lenMs = 450.f; p.delayMode = 1; }},
    {"B\xC3\xA9gaiement trap", [](rel::Params& p) { p.character = 5; p.density = 0.5f; p.audace = 0.7f; p.lenMs = 350.f; }},
    {"Harmonie quinte", [](rel::Params& p) { p.character = 6; p.density = 0.5f; p.audace = 0.2f; p.lenMs = 500.f; p.tone = 0.6f; }},
    {"Harmonie octave haute", [](rel::Params& p) { p.character = 6; p.density = 0.4f; p.audace = 1.f; p.lenMs = 400.f; p.tone = 0.7f; p.levelDb = -6.f; }},
    {"Backeur trap", [](rel::Params& p) { p.density = 0.7f; p.audace = 0.55f; p.lenMs = 300.f; p.width = 0.8f; }},
    {"Drill sombre", [](rel::Params& p) { p.density = 0.6f; p.audace = 0.6f; p.tone = 0.25f; p.delayMode = 0; p.lenMs = 280.f; }},
    {"Cloud rap lointain", [](rel::Params& p) { p.density = 0.5f; p.audace = 0.3f; p.tone = 0.2f; p.width = 1.f; p.levelDb = -8.f; p.lenMs = 550.f; p.delayMode = 1; }},
    {"Ad-libs serr\xC3\xA9s", [](rel::Params& p) { p.density = 0.85f; p.audace = 0.45f; p.delayMode = 0; p.lenMs = 220.f; p.width = 0.5f; }},
    {"Flow dense", [](rel::Params& p) { p.density = 0.4f; p.threshDb = -34.f; p.lenMs = 250.f; p.delayMode = 0; }},
    {"Prise calme", [](rel::Params& p) { p.density = 0.5f; p.threshDb = -50.f; p.lenMs = 450.f; }},
    {"Audace maximale", [](rel::Params& p) { p.density = 0.9f; p.audace = 1.f; p.width = 1.f; p.seed = 23; }},
    {"Une r\xC3\xA9ponse sur deux", [](rel::Params& p) { p.density = 0.5f; p.audace = 0.4f; p.seed = 7; p.delayMode = 1; }},
};
constexpr int kNumPresets = (int)(sizeof(kPresets) / sizeof(kPresets[0]));
static_assert(kNumPresets == 16, "16 préréglages");
}  // namespace

int RelanceProcessor::numPresets() { return kNumPresets; }

juce::AudioProcessorValueTreeState::ParameterLayout RelanceProcessor::makeLayout() {
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    const rel::Params d;
    l.add(fparam("dens", u8("Densit\xC3\xA9"), 0.f, 1.f, 0.001f, d.density, pct));
    l.add(fparam("audace", "Audace", 0.f, 1.f, 0.001f, d.audace, pct));
    l.add(choice("carac", u8("Caract\xC3\xA8re"), {u8("M\xC3\xA9lange"), u8(rel::typeName(0)), u8(rel::typeName(1)), u8(rel::typeName(2)), u8(rel::typeName(3)), u8(rel::typeName(4)), u8(rel::typeName(5))}, d.character));
    l.add(choice("delay", "Retard", {"1 croche", "2 croches", "1 ou 2"}, d.delayMode));
    l.add(fparam("len", "Longueur", 200.f, 600.f, 1.f, d.lenMs, msTxt));
    l.add(fparam("thresh", "Seuil", -60.f, -20.f, 0.1f, d.threshDb, fmtDb));
    l.add(fparam("width", "Largeur", 0.f, 1.f, 0.001f, d.width, pct));
    l.add(fparam("tone", "Couleur", 0.f, 1.f, 0.001f, d.tone, pct));
    l.add(fparam("level", "Niveau", -24.f, 6.f, 0.1f, d.levelDb, fmtDb));
    l.add(fparam("seed", "Graine", 1.f, 999.f, 1.f, (float)d.seed, integer));
    l.add(choice("lock", "Verrou", {"Libre", u8("Verrouill\xC3\xA9" "e")}, d.lock));
    l.add(choice("outmode", "Sortie", {"Voix + relances", "Relances seules"}, d.outMode));
    l.add(choice("bypass", "Bypass", {"Off", "On"}, 0));   // contournement en « choix » à deux états (aucun booléen)
    // ajoutés en fin de liste : tonalité et gamme de l'harmonie (Auto : déduite des notes chantées)
    l.add(choice("hkey", u8("Tonalit\xC3\xA9 harmonie"), {"Auto", "Do", "Do#", "R\xC3\xA9", "R\xC3\xA9#", "Mi", "Fa", "Fa#", "Sol", "Sol#", "La", "La#", "Si"}, 0));
    l.add(choice("hscale", "Mode harmonie", {"Majeur", "Mineur"}, 0));
    return l;
}

RelanceProcessor::RelanceProcessor() : FxProcessor("Orbe Relance", "ORBERELANCE", makeLayout()) {
    bypassChoice = apvts.getParameter("bypass");
    setLatencySamples(0);
}

rel::Params RelanceProcessor::readParams() const {
    rel::Params p;
    auto ch = [this](const char* id) { return juce::roundToInt(raw(id)); };
    p.density = raw("dens"); p.audace = raw("audace"); p.character = ch("carac"); p.delayMode = ch("delay");
    p.lenMs = raw("len"); p.threshDb = raw("thresh"); p.width = raw("width"); p.tone = raw("tone"); p.levelDb = raw("level");
    p.seed = ch("seed"); p.lock = ch("lock"); p.outMode = ch("outmode");
    p.key = ch("hkey"); p.scale = ch("hscale"); p.diatonic = stateVer >= 2;   // état d'avant la v2 : quinte juste fixe d'origine
    return p;
}

void RelanceProcessor::writeParams(const rel::Params& p) {
    auto put = [this](const char* id, float v) { if (auto* q = apvts.getParameter(id)) q->setValueNotifyingHost(q->convertTo0to1(v)); };
    put("dens", p.density); put("audace", p.audace); put("carac", (float)p.character); put("delay", (float)p.delayMode);
    put("len", p.lenMs); put("thresh", p.threshDb); put("width", p.width); put("tone", p.tone); put("level", p.levelDb);
    put("seed", (float)p.seed); put("lock", (float)p.lock); put("outmode", (float)p.outMode);
    put("hkey", (float)p.key); put("hscale", (float)p.scale);
}

static rel::Params presetParams(int i, const rel::Params& cur) {
    rel::Params p; p.lock = cur.lock; p.outMode = cur.outMode; p.key = cur.key; p.scale = cur.scale;   // tonalité : jamais dans un préréglage
    kPresets[i].tune(p);
    return p;
}

bool RelanceProcessor::presetMatches(int i) const {
    if (i < 0 || i >= kNumPresets) return false;
    const auto c = readParams(), p = presetParams(i, c);
    auto eq = [](float a, float b, float tol) { return std::fabs(a - b) <= tol; };
    return eq(c.density, p.density, 0.002f) && eq(c.audace, p.audace, 0.002f) && c.character == p.character && c.delayMode == p.delayMode
        && eq(c.lenMs, p.lenMs, 1.f) && eq(c.threshDb, p.threshDb, 0.1f) && eq(c.width, p.width, 0.002f) && eq(c.tone, p.tone, 0.002f)
        && eq(c.levelDb, p.levelDb, 0.1f) && c.seed == p.seed;
}

int RelanceProcessor::getNumPrograms() { return kNumPresets; }
const juce::String RelanceProcessor::getProgramName(int i) { return i >= 0 && i < kNumPresets ? u8(kPresets[i].name) : juce::String(); }
void RelanceProcessor::setCurrentProgram(int i) {
    if (i < 0 || i >= kNumPresets) return;
    program = i;
    writeParams(presetParams(i, readParams()));
}

void RelanceProcessor::beforeSave() {
    rel::Meta m; m.program = program;
    apvts.state.setProperty("rel", juce::String(rel::toString(m)), nullptr);
    apvts.state.setProperty("stateVersion", 2, nullptr);   // 2 : harmonie diatonique
}
void RelanceProcessor::afterLoad() {
    stateVer = (int)apvts.state.getProperty("stateVersion", 1);
    rel::Meta m;
    if (rel::fromString(apvts.state.getProperty("rel").toString().toStdString(), m)) program = juce::jlimit(0, kNumPresets - 1, m.program);
}

void RelanceProcessor::prepareToPlay(double sr, int block) {
    maxBlock = juce::jlimit(64, 8192, juce::jmax(512, block));
    engine.set(readParams());
    engine.prepare(sr, maxBlock);
    prepareBypass(sr, engine.latency(), maxBlock);
    setLatencySamples(engine.latency());
}

void RelanceProcessor::processBlock(juce::AudioBuffer<float>& b, juce::MidiBuffer&) {
    juce::ScopedNoDenormals nd;
    if (b.getNumChannels() < 2 || maxBlock <= 0) return;
    rel::Host h;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition()) {
            h.playing = pos->getIsPlaying();
            if (auto bpm = pos->getBpm()) { h.hasBpm = true; h.bpm = *bpm; }
            if (auto ppq = pos->getPpqPosition()) { h.hasPos = true; h.ppq = *ppq; }
        }
    engine.set(readParams());
    const bool st = getTotalNumInputChannels() >= 2;
    const int n = b.getNumSamples();
    float* l = b.getWritePointer(0); float* r = b.getWritePointer(1);
    kit::sanitize(l, n); if (st) kit::sanitize(r, n);
    // position de chaque tranche : la PPQ avance avec les échantillons déjà traités
    int done = 0;
    const double ppqPerSample = h.hasBpm ? h.bpm / 60.0 / getSampleRate() : 2.0 / getSampleRate();
    fader.run(l, r, n, st, raw("bypass") >= 0.5f, [&](float* cl, float* cr, int c) {
        rel::Host hc = h; hc.ppq = h.ppq + done * ppqPerSample;
        engine.beginBlock(hc);
        engine.process(cl, st ? cr : nullptr, cl, cr, c);
        done += c;
    });
}

juce::AudioProcessorEditor* RelanceProcessor::createEditor() { return new RelanceEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new RelanceProcessor(); }
