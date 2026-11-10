#include "AccrocProcessor.h"
#include "AccrocEditor.h"

using kit::u8;
using namespace of;

namespace {
std::unique_ptr<juce::AudioParameterChoice> choice(const char* id, const juce::String& name, juce::StringArray items, int def) {
    return std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{id, 1}, name, items, def);
}
juce::String pct(float v) { return String(juce::roundToInt(v * 100.f)) + " %"; }
juce::String integer(float v) { return String(juce::roundToInt(v)); }

// Préréglages d'usine : motif (S R T F B C J G, minuscule = 50 %), durées en pas, réglages.
struct Preset { const char* name; const char* types; const char* lens; float inten; void (*tune)(acr::Params&); };
const Preset kPresets[] = {
    {"B\xC3\xA9gaiement trap", "....B.......B.b.", "1111111111112121", 0.9f, [](acr::Params&) {}},
    {"Stutter de fin de mesure", "............S...", "1111111111114111", 1.f, [](acr::Params& p) { p.stuDiv = 3; p.stuDecay = 0.25f; }},
    {"Rampe qui monte", "........S.......", "1111111181111111", 1.f, [](acr::Params& p) { p.stuDiv = 3; p.stuPitch = 2.f; p.stuDecay = 0.1f; }},
    {"Tape-stop de fin de phrase", "............T...", "1111111111114111", 1.f, [](acr::Params& p) { p.stepDiv = 0; p.tapeDepth = 1.f; p.tapeCurve = 0.4f; }},
    {"Dive de hauteur", "......T.......T.", "1111112111111121", 0.85f, [](acr::Params& p) { p.tapeDepth = 0.5f; p.tapeCurve = 0.3f; }},
    {"Gel de refrain", "F...............", "8111111111111111", 0.8f, [](acr::Params& p) { p.stepDiv = 0; p.frzMs = 120.f; p.frzJitter = 0.4f; }},
    {"Crush lo-fi", "C...............", "g111111111111111", 0.85f, [](acr::Params& p) { p.crBits = 7.f; p.crRate = 8000.f; p.crSmooth = 0.6f; }},
    {"Hoquet rythmique", "G...G...G.G.G...", "1111111111111111", 1.f, [](acr::Params& p) { p.gateDiv = 1; p.gateDens = 0.55f; }},
    {"Retour arri\xC3\xA8re", "....R.......R...", "1111211111112111", 0.9f, [](acr::Params& p) { p.revDiv = 1; }},
    {"Saut de lecture", "..J...J...J.J...", "1121112111212111", 0.9f, [](acr::Params& p) { p.skipRange = 4; }},
    {"Micro-coupures", "g.g.g.g.g.g.g.g.", "1111111111111111", 1.f, [](acr::Params& p) { p.gateDiv = 2; p.gateDens = 0.35f; }},
    {"Accident contr\xC3\xB4l\xC3\xA9", "................", nullptr, 0.9f, [](acr::Params& p) { p.chaos = 0.35f; p.seed = 7; }},
    {"Hachis 1/64", "..S...S...S.SSSS", "1111111111111111", 0.9f, [](acr::Params& p) { p.stuDiv = 4; p.stuDecay = 0.4f; p.stuPitch = -1.f; }},
    {"Radio cass\xC3\xA9" "e", "C...J...C...JJ..", "2111211121111111", 0.9f, [](acr::Params& p) { p.crBits = 4.f; p.crRate = 3000.f; p.crSmooth = 0.2f; p.skipRange = 6; }},
    {"Gel puis retour", "F.......R...R...", "4111111121112111", 0.9f, [](acr::Params& p) { p.frzMs = 90.f; p.revDiv = 2; }},
    {"Chaos total", "s.r.t.f.b.c.j.g.", "1111111111111111", 0.9f, [](acr::Params& p) { p.chaos = 0.7f; p.prob = 0.8f; p.seed = 23; }},
};
constexpr int kNumPresets = (int)(sizeof(kPresets) / sizeof(kPresets[0]));
}  // namespace

juce::AudioProcessorValueTreeState::ParameterLayout AccrocProcessor::makeLayout() {
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    const acr::Params d;
    l.add(fparam("mix", "Mix", 0.f, 1.f, 0.001f, d.mix, pct));
    l.add(fparam("out", "Sortie", -12.f, 12.f, 0.1f, d.outDb, fmtDb));
    l.add(fparam("chaos", "Chaos", 0.f, 1.f, 0.001f, d.chaos, pct));
    l.add(fparam("prob", u8("Probabilit\xC3\xA9"), 0.f, 1.f, 0.001f, d.prob, pct));
    l.add(fparam("seed", "Graine", 1.f, 999.f, 1.f, (float)d.seed, integer));
    l.add(choice("step", "Pas", {"1/8", "1/16", "1/32"}, d.stepDiv));
    l.add(choice("clock", "Horloge", {"Lecture", "Toujours"}, d.clock));
    l.add(choice("stu_div", "Stutter division", {"1/4", "1/8", "1/16", "1/32", "1/64"}, d.stuDiv));
    l.add(fparam("stu_decay", u8("Stutter d\xC3\xA9" "clin"), 0.f, 1.f, 0.001f, d.stuDecay, pct));
    l.add(fparam("stu_pitch", "Stutter rampe", -12.f, 12.f, 0.01f, d.stuPitch, [](float v) { return (v > 0.004f ? "+" : "") + String(v, 1) + " dt"; }));
    l.add(choice("rev_div", "Retour tranche", {"1/4", "1/8", "1/16", "1/32"}, d.revDiv));
    l.add(fparam("tape_depth", u8("Arr\xC3\xAAt profondeur"), 0.f, 1.f, 0.001f, d.tapeDepth, pct));
    l.add(fparam("tape_curve", u8("Arr\xC3\xAAt courbe"), 0.f, 1.f, 0.001f, d.tapeCurve, pct));
    l.add(fparam("frz_size", "Gel grain", 10.f, 250.f, 0.1f, d.frzMs, [](float v) { return String(juce::roundToInt(v)) + " ms"; }, 60.f));
    l.add(fparam("frz_jitter", "Gel flou", 0.f, 1.f, 0.001f, d.frzJitter, pct));
    l.add(choice("rep_div", u8("B\xC3\xA9gaiement syllabe"), {"1/4", "1/8", "1/16"}, d.repDiv));
    l.add(fparam("rep_gap", u8("B\xC3\xA9gaiement silence"), 0.f, 0.8f, 0.001f, d.repGap, pct));
    l.add(fparam("cr_bits", "Crush bits", 2.f, 16.f, 0.01f, d.crBits, [](float v) { return String(v, 1) + " bits"; }));
    l.add(fparam("cr_rate", u8("Crush fr\xC3\xA9quence"), 500.f, 24000.f, 1.f, d.crRate, fmtHz, 4000.f, parseHz));
    l.add(fparam("cr_smooth", "Crush lissage", 0.f, 1.f, 0.001f, d.crSmooth, pct));
    l.add(fparam("skip_range", u8("Saut port\xC3\xA9" "e"), 1.f, 8.f, 1.f, (float)d.skipRange, [](float v) { return String(juce::roundToInt(v)) + " x 1/16"; }));
    l.add(choice("gate_div", "Trous grille", {"1/16", "1/32", "1/64"}, d.gateDiv));
    l.add(fparam("gate_dens", u8("Trous densit\xC3\xA9"), 0.f, 1.f, 0.001f, d.gateDens, pct));
    l.add(choice("bypass", "Bypass", {"Off", "On"}, 0));   // contournement en « choix » à deux états (aucun booléen)
    return l;
}

AccrocProcessor::AccrocProcessor() : FxProcessor("Orbe Accroc", "ORBEACCROC", makeLayout()) {
    bypassChoice = apvts.getParameter("bypass");
    for (auto& a : uiAct) a.store(0.f);
    for (auto& a : padWant) a.store(0.f);
    seq_ = acr::pattern(kPresets[0].types, kPresets[0].lens, kPresets[0].inten);
    engine.pushSeq(seq_);
    setLatencySamples(48 * 4);
}

acr::Params AccrocProcessor::readParams() const {
    acr::Params p;
    auto ch = [this](const char* id) { return juce::roundToInt(raw(id)); };
    p.mix = raw("mix"); p.outDb = raw("out"); p.chaos = raw("chaos"); p.prob = raw("prob"); p.seed = ch("seed");
    p.stepDiv = ch("step"); p.clock = ch("clock");
    p.stuDiv = ch("stu_div"); p.stuDecay = raw("stu_decay"); p.stuPitch = raw("stu_pitch");
    p.revDiv = ch("rev_div"); p.tapeDepth = raw("tape_depth"); p.tapeCurve = raw("tape_curve");
    p.frzMs = raw("frz_size"); p.frzJitter = raw("frz_jitter");
    p.repDiv = ch("rep_div"); p.repGap = raw("rep_gap");
    p.crBits = raw("cr_bits"); p.crRate = raw("cr_rate"); p.crSmooth = raw("cr_smooth");
    p.skipRange = ch("skip_range"); p.gateDiv = ch("gate_div"); p.gateDens = raw("gate_dens");
    return p;
}

void AccrocProcessor::writeParams(const acr::Params& p) {
    auto put = [this](const char* id, float v) { if (auto* q = apvts.getParameter(id)) q->setValueNotifyingHost(q->convertTo0to1(v)); };
    put("mix", p.mix); put("out", p.outDb); put("chaos", p.chaos); put("prob", p.prob); put("seed", (float)p.seed);
    put("step", (float)p.stepDiv); put("clock", (float)p.clock);
    put("stu_div", (float)p.stuDiv); put("stu_decay", p.stuDecay); put("stu_pitch", p.stuPitch);
    put("rev_div", (float)p.revDiv); put("tape_depth", p.tapeDepth); put("tape_curve", p.tapeCurve);
    put("frz_size", p.frzMs); put("frz_jitter", p.frzJitter);
    put("rep_div", (float)p.repDiv); put("rep_gap", p.repGap);
    put("cr_bits", p.crBits); put("cr_rate", p.crRate); put("cr_smooth", p.crSmooth);
    put("skip_range", (float)p.skipRange); put("gate_div", (float)p.gateDiv); put("gate_dens", p.gateDens);
}

void AccrocProcessor::setSeq(const acr::Seq& s) {
    acr::Seq t = s; acr::sanitize(t);
    seq_ = t; ++seqVer;
    engine.pushSeq(t);
}

int AccrocProcessor::getNumPrograms() { return kNumPresets; }
const juce::String AccrocProcessor::getProgramName(int i) { return i >= 0 && i < kNumPresets ? u8(kPresets[i].name) : juce::String(); }
void AccrocProcessor::setCurrentProgram(int i) {
    if (i < 0 || i >= kNumPresets) return;
    program = i;
    const auto& pr = kPresets[i];
    acr::Params p; p.mix = raw("mix"); p.outDb = raw("out"); p.clock = juce::roundToInt(raw("clock"));   // mix, sortie et horloge restent à l'utilisateur
    pr.tune(p);
    writeParams(p);
    setSeq(acr::pattern(pr.types, pr.lens, pr.inten));
}

void AccrocProcessor::beforeSave() {
    apvts.state.setProperty("seq", juce::String(acr::toString(seq_)), nullptr);
    apvts.state.setProperty("stateVersion", 1, nullptr);
    apvts.state.setProperty("program", program, nullptr);
    apvts.state.setProperty("brush", brush, nullptr);
}
void AccrocProcessor::afterLoad() {
    acr::Seq s;
    if (acr::fromString(apvts.state.getProperty("seq").toString().toStdString(), s)) setSeq(s);
    program = juce::jlimit(0, kNumPresets - 1, (int)apvts.state.getProperty("program", 0));
    brush = juce::jlimit(-1, acr::kNumTypes - 1, (int)apvts.state.getProperty("brush", 0));
}

void AccrocProcessor::prepareToPlay(double sr, int block) {
    maxBlock = juce::jlimit(64, 8192, juce::jmax(512, block));
    engine.set(readParams());
    engine.prepare(sr, maxBlock);
    engine.pushSeq(seq_);
    for (auto& h : padHave) h = 0.f;
    prepareBypass(sr, engine.latency(), maxBlock);
    setLatencySamples(engine.latency());
}

void AccrocProcessor::processBlock(juce::AudioBuffer<float>& b, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals nd;
    if (b.getNumChannels() < 2 || maxBlock <= 0) return;
    int ne = 0;
    for (int k = 0; k < acr::kNumTypes; ++k) {   // pads de l'interface : début de bloc
        const float want = padWant[(size_t)k].load();
        if (want != padHave[(size_t)k]) { padHave[(size_t)k] = want; evs[(size_t)ne++] = {0, 60 + k, want}; }
    }
    for (const auto m : midi) {
        if (ne >= (int)evs.size()) break;
        const auto msg = m.getMessage();
        if (msg.isNoteOn()) evs[(size_t)ne++] = {m.samplePosition, msg.getNoteNumber(), msg.getFloatVelocity()};
        else if (msg.isNoteOff()) evs[(size_t)ne++] = {m.samplePosition, msg.getNoteNumber(), 0.f};
        else if (msg.isAllNotesOff() || msg.isAllSoundOff()) evs[(size_t)ne++] = {m.samplePosition, -1, 0.f};
    }
    acr::Host h;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition()) {
            h.playing = pos->getIsPlaying();
            if (auto bpm = pos->getBpm()) { h.hasBpm = true; h.bpm = *bpm; }
            if (auto ppq = pos->getPpqPosition()) { h.hasPos = true; h.ppq = *ppq; }
        }
    engine.set(readParams());
    engine.beginBlock(h, evs.data(), ne);
    const bool st = getTotalNumInputChannels() >= 2;
    const int n = b.getNumSamples();
    float* l = b.getWritePointer(0); float* r = b.getWritePointer(1);
    kit::sanitize(l, n); if (st) kit::sanitize(r, n);
    fader.run(l, r, n, st, raw("bypass") >= 0.5f, [&](float* cl, float* cr, int c) { engine.process(cl, st ? cr : nullptr, cl, cr, c); });
    uiStep.store(engine.stepNow()); uiBpm.store(engine.bpmNow());
    for (int k = 0; k < acr::kNumTypes; ++k) uiAct[(size_t)k].store(engine.activity(k));
}

juce::AudioProcessorEditor* AccrocProcessor::createEditor() { return new AccrocEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AccrocProcessor(); }
