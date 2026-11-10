#include "GaletProcessor.h"
#include "GaletEditor.h"
#include "../../Kit/Sanitize.h"

using kit::u8;
using namespace of;

namespace {
juce::StringArray onOff() { return {"Off", "On"}; }
std::unique_ptr<juce::AudioParameterChoice> choice(const char* id, const juce::String& name, juce::StringArray items, int def) {
    return std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{id, 1}, name, items, def);
}
juce::String pct(float v) { return String(juce::roundToInt(v * 100.f)) + " %"; }

// Préréglages d'usine : réglages + forme (forme de départ ou motif à pas).
struct Preset { const char* name; int start; const char* steps; void (*tune)(gal::Params&); };
const Preset kPresets[] = {
    {"Pompe rythmique", 2, nullptr, [](gal::Params&) {}},
    {"Pompe douce", 2, nullptr, [](gal::Params& p) { p.volDepth = 0.5f; p.smoothMs = 12.f; }},
    {"Trance gate 1/16", -1, "x.x..x.xx.x..x.x", [](gal::Params& p) { p.div = 2; p.volDepth = 1.f; p.smoothMs = 2.5f; }},
    {"Hachoir 1/32", 5, nullptr, [](gal::Params& p) { p.div = 6; p.volDepth = 1.f; p.smoothMs = 1.5f; }},
    {"Wobble lent", 0, nullptr, [](gal::Params& p) { p.div = 3; p.volOn = false; p.fltOn = true; p.fltFreq = 260.f; p.fltRes = 0.6f; p.interp = gal::Sym; }},
    {"Wobble triolet", 0, nullptr, [](gal::Params& p) { p.div = 5; p.feel = 1; p.volOn = false; p.fltOn = true; p.fltFreq = 420.f; p.fltRes = 0.72f; p.interp = gal::Sym; }},
    {"Slice rythmique", -1, "..x.xx..x...x.x.", [](gal::Params& p) { p.div = 1; p.volDepth = 1.f; p.smoothMs = 3.f; }},
    {"Saturation respirante", 0, nullptr, [](gal::Params& p) { p.div = 3; p.volOn = false; p.drvOn = true; p.drive = 18.f; p.color = 0.2f; p.interp = gal::Sym; }},
    {"Harmoniques paires", 3, nullptr, [](gal::Params& p) { p.div = 2; p.volOn = false; p.drvOn = true; p.drvType = 1; p.drive = 15.f; p.color = 0.7f; p.smoothMs = 10.f; }},
    {"Pli doux rythmique", 5, nullptr, [](gal::Params& p) { p.div = 5; p.volOn = false; p.drvOn = true; p.drvType = 2; p.drive = 18.f; p.smoothMs = 6.f; p.mix = 0.7f; }},
    {"Balayage passe-haut", 3, nullptr, [](gal::Params& p) { p.div = 0; p.volOn = false; p.fltOn = true; p.fltMode = 2; p.fltFreq = 1400.f; p.fltRes = 0.3f; p.smoothMs = 20.f; }},
    {"Balancier", 0, nullptr, [](gal::Params& p) { p.div = 3; p.volOn = false; p.panOn = true; p.pan = 0.8f; p.interp = gal::Sym; }},
    {"Respiration st\xC3\xA9r\xC3\xA9o", 0, nullptr, [](gal::Params& p) { p.div = 1; p.volOn = false; p.panOn = true; p.pan = 0.f; p.width = 1.f; p.interp = gal::Spline; }},
    {"Pompe satur\xC3\xA9" "e", 2, nullptr, [](gal::Params& p) { p.volDepth = 0.6f; p.drvOn = true; p.drive = 12.f; p.drvDepth = 0.7f; p.fltOn = true; p.fltFreq = 2000.f; p.fltDepth = 0.5f; }},
    {"Escalier filtrant", 6, nullptr, [](gal::Params& p) { p.div = 2; p.volOn = false; p.fltOn = true; p.fltMode = 1; p.fltFreq = 1500.f; p.fltRes = 0.6f; p.smoothMs = 3.f; }},
    {"Rebond", 7, nullptr, [](gal::Params& p) { p.div = 3; p.volDepth = 0.9f; p.smoothMs = 2.f; }},
};
constexpr int kNumPresets = (int)(sizeof(kPresets) / sizeof(kPresets[0]));
}  // namespace

juce::AudioProcessorValueTreeState::ParameterLayout GaletProcessor::makeLayout() {
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    const gal::Params d;
    l.add(choice("sync", "Synchro", {"Tempo", "Hz"}, 0));
    l.add(choice("div", "Division", {"4 mes.", "2 mes.", "1/1", "1/2", "1/4", "1/8", "1/16", "1/32"}, d.div));
    l.add(choice("feel", "Valeur", {"Droit", "Triolet", u8("Point\xC3\xA9")}, 0));
    l.add(fparam("hz", u8("Fr\xC3\xA9quence libre"), 0.02f, 40.f, 0.001f, d.hz, [](float v) { return String(v, v < 1.f ? 3 : 2) + " Hz"; }, 1.f));
    l.add(fparam("phase", "Phase", 0.f, 360.f, 1.f, d.phase, [](float v) { return String(juce::roundToInt(v)) + juce::String::fromUTF8("\xC2\xB0"); }));
    l.add(choice("retrig", u8("D\xC3\xA9part"), {u8("H\xC3\xB4te"), "Note", "Libre"}, 0));
    l.add(fparam("smooth", "Lissage", 1.f, 60.f, 0.1f, d.smoothMs, [](float v) { return String(v, v < 10.f ? 1 : 0) + " ms"; }, 8.f));
    l.add(choice("interp", u8("Trac\xC3\xA9"), {"Libre", "En S", "Spline"}, 0));
    l.add(choice("grid", "Grille", {"1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/8 T", "1/16 T"}, 4));
    l.add(choice("vol_on", "Volume", onOff(), 1));
    l.add(fparam("vol_depth", "Profondeur volume", 0.f, 1.f, 0.001f, d.volDepth, pct));
    l.add(choice("flt_on", "Filtre", onOff(), 0));
    l.add(choice("flt_mode", "Type de filtre", {"Passe-bas", "Passe-bande", "Passe-haut"}, 0));
    l.add(fparam("flt_freq", u8("Fr\xC3\xA9quence filtre"), 40.f, 16000.f, 1.f, d.fltFreq, fmtHz, 800.f, parseHz));
    l.add(fparam("flt_res", u8("R\xC3\xA9sonance"), 0.f, 1.f, 0.001f, d.fltRes, pct));
    l.add(fparam("flt_depth", "Profondeur filtre", 0.f, 1.f, 0.001f, d.fltDepth, pct));
    l.add(choice("drv_on", "Distorsion", onOff(), 0));
    l.add(choice("drv_type", "Type de saturation", {"Tanh", "Arctan", "Pli doux"}, 0));
    l.add(fparam("drv_drive", "Drive", 0.f, 24.f, 0.1f, 12.f, [](float v) { return "+" + String(v, 1) + " dB"; }));
    l.add(fparam("drv_color", "Couleur", 0.f, 1.f, 0.001f, d.color, pct));
    l.add(fparam("drv_depth", "Profondeur distorsion", 0.f, 1.f, 0.001f, d.drvDepth, pct));
    l.add(choice("pan_on", "Pan", onOff(), 0));
    l.add(fparam("pan_pan", "Pan", -1.f, 1.f, 0.001f, 0.6f, [](float v) { const int k = juce::roundToInt(std::fabs(v) * 100.f); return k == 0 ? String("C") : (v < 0 ? "G " : "D ") + String(k) + " %"; }));
    l.add(fparam("pan_width", "Largeur", -1.f, 1.f, 0.001f, 0.f, [](float v) { const int k = juce::roundToInt(v * 100.f); return (k > 0 ? "+" : "") + String(k) + " %"; }));
    l.add(fparam("mix", "Mix", 0.f, 1.f, 0.001f, d.mix, pct));
    l.add(fparam("out", "Sortie", -12.f, 12.f, 0.1f, d.outDb, fmtDb));
    addBypass(l);
    return l;
}

GaletProcessor::GaletProcessor() : FxProcessor("Orbe Galet", "ORBEGALET", makeLayout()) {
    for (auto& e : uiEnv) e.store(0.f);
    shape_ = gal::startShape(2);
    engine.pushShape(shape_);
    setLatencySamples(gal::Engine::kLatency);
}

gal::Params GaletProcessor::readParams() const {
    gal::Params p;
    auto ch = [this](const char* id) { return juce::roundToInt(raw(id)); };
    p.sync = ch("sync") == 0; p.div = ch("div"); p.feel = ch("feel"); p.hz = raw("hz"); p.phase = raw("phase"); p.retrig = ch("retrig");
    p.smoothMs = raw("smooth"); p.interp = ch("interp");
    p.volOn = ch("vol_on") == 1; p.volDepth = raw("vol_depth");
    p.fltOn = ch("flt_on") == 1; p.fltMode = ch("flt_mode"); p.fltFreq = raw("flt_freq"); p.fltRes = raw("flt_res"); p.fltDepth = raw("flt_depth");
    p.drvOn = ch("drv_on") == 1; p.drvType = ch("drv_type"); p.drive = raw("drv_drive"); p.color = raw("drv_color"); p.drvDepth = raw("drv_depth");
    p.panOn = ch("pan_on") == 1; p.pan = raw("pan_pan"); p.width = raw("pan_width");
    p.mix = raw("mix"); p.outDb = raw("out");
    return p;
}

void GaletProcessor::writeParams(const gal::Params& p) {
    auto put = [this](const char* id, float v) { if (auto* q = apvts.getParameter(id)) q->setValueNotifyingHost(q->convertTo0to1(v)); };
    put("sync", p.sync ? 0.f : 1.f); put("div", (float)p.div); put("feel", (float)p.feel); put("hz", p.hz); put("phase", p.phase); put("retrig", (float)p.retrig);
    put("smooth", p.smoothMs); put("interp", (float)p.interp);
    put("vol_on", p.volOn ? 1.f : 0.f); put("vol_depth", p.volDepth);
    put("flt_on", p.fltOn ? 1.f : 0.f); put("flt_mode", (float)p.fltMode); put("flt_freq", p.fltFreq); put("flt_res", p.fltRes); put("flt_depth", p.fltDepth);
    put("drv_on", p.drvOn ? 1.f : 0.f); put("drv_type", (float)p.drvType); put("drv_drive", p.drive); put("drv_color", p.color); put("drv_depth", p.drvDepth);
    put("pan_on", p.panOn ? 1.f : 0.f); put("pan_pan", p.pan); put("pan_width", p.width);
    put("mix", p.mix); put("out", p.outDb);
}

void GaletProcessor::setShape(const gal::Shape& s) {
    gal::Shape t = s; gal::sanitize(t);
    shape_ = t; ++shapeVer;
    engine.pushShape(t);
}

int GaletProcessor::getNumPrograms() { return kNumPresets; }
const juce::String GaletProcessor::getProgramName(int i) { return i >= 0 && i < kNumPresets ? u8(kPresets[i].name) : juce::String(); }
void GaletProcessor::setCurrentProgram(int i) {
    if (i < 0 || i >= kNumPresets) return;
    program = i;
    const auto& pr = kPresets[i];
    gal::Params p; int ip = gal::Free;
    const gal::Shape s = pr.steps ? gal::stepShape(pr.steps) : gal::startShape(pr.start, &ip);
    p.interp = ip; p.drive = 12.f; p.pan = 0.6f;
    pr.tune(p);
    writeParams(p);
    setShape(s);
}

void GaletProcessor::beforeSave() {
    apvts.state.setProperty("shape", juce::String(gal::toString(shape_)), nullptr);
    apvts.state.setProperty("snap", snap ? 1 : 0, nullptr);
    apvts.state.setProperty("program", program, nullptr);
}
void GaletProcessor::afterLoad() {
    gal::Shape s;
    if (gal::fromString(apvts.state.getProperty("shape").toString().toStdString(), s)) setShape(s);
    snap = (int)apvts.state.getProperty("snap", 1) != 0;
    program = juce::jlimit(0, kNumPresets - 1, (int)apvts.state.getProperty("program", 0));
}

void GaletProcessor::prepareToPlay(double sr, int block) {
    maxBlock = juce::jlimit(64, 8192, juce::jmax(512, block));
    engine.prepare(sr, maxBlock);
    engine.pushShape(shape_);
    prepareBypass(sr, engine.latency(), maxBlock);
    setLatencySamples(engine.latency());
}

void GaletProcessor::processBlock(juce::AudioBuffer<float>& b, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals nd;
    if (b.getNumChannels() < 2 || maxBlock <= 0) return;
    int nn = 0;
    for (const auto m : midi) if (m.getMessage().isNoteOn() && nn < (int)notes.size()) notes[(size_t)nn++] = m.samplePosition;
    gal::Host h;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition()) {
            h.playing = pos->getIsPlaying();
            if (auto bpm = pos->getBpm()) { h.hasBpm = true; h.bpm = *bpm; }
            if (auto ppq = pos->getPpqPosition()) { h.hasPos = true; h.ppq = *ppq; }
        }
    engine.set(readParams());
    engine.beginBlock(h, notes.data(), nn);
    const bool st = getTotalNumInputChannels() >= 2;
    for (int c = getTotalNumInputChannels(); c < 2; ++c) if (c > 0) b.clear(c, 0, b.getNumSamples());
    kit::sanitize(b.getWritePointer(0), b.getNumSamples()); if (st) kit::sanitize(b.getWritePointer(1), b.getNumSamples());   // NaN/inf d'un plugin precedent : remplaces par 0
    fader.run(b.getWritePointer(0), b.getWritePointer(1), b.getNumSamples(), st, bypass != nullptr && bypass->get(),
              [&](float* l, float* r, int n) { engine.process(l, st ? r : nullptr, l, r, n); });
    uiPhase.store(engine.phaseNow()); uiMod.store(engine.modNow()); uiDrive.store(engine.driveDbNow()); uiBpm.store(h.hasBpm ? h.bpm : 120.0);
    for (int k = 0; k < 4; ++k) uiEnv[(size_t)k].store(engine.env(k));
}

juce::AudioProcessorEditor* GaletProcessor::createEditor() { return new GaletEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new GaletProcessor(); }
