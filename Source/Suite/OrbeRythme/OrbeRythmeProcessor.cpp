#include "OrbeRythmeProcessor.h"
#include "OrbeRythmeEditor.h"
#include "../../Kit/StateVersion.h"
#include <algorithm>

using juce::String;

juce::AudioProcessorValueTreeState::ParameterLayout OrbeRythmeProcessor::makeLayout() {
    using juce::AudioParameterChoice, juce::AudioParameterFloat, juce::NormalisableRange, juce::ParameterID;
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    juce::StringArray styleNames;
    for (auto& s : orbe::styles()) styleNames.add(s.label);
    auto pct = [](float v, int) { return String(juce::roundToInt(v * 100.f)) + " %"; };
    auto attr = juce::AudioParameterFloatAttributes().withStringFromValueFunction(pct);
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{"style", 1}, "Style", styleNames, 0));
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{"bars", 1}, "Longueur", juce::StringArray{"4 mesures", "8 mesures"}, 0));
    l.add(std::make_unique<AudioParameterFloat>(ParameterID{"human", 1}, "Humanisation", NormalisableRange<float>(0.f, 1.f), 0.f, attr));
    l.add(std::make_unique<AudioParameterFloat>(ParameterID{"swing", 1}, "Swing", NormalisableRange<float>(0.f, 1.f), 0.f, attr));
    l.add(std::make_unique<AudioParameterFloat>(ParameterID{"level", 1}, "Niveau", NormalisableRange<float>(0.f, 1.f), 0.85f, attr));
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{"mute", 1}, "Muet", juce::StringArray{"Son", "Muet"}, 0));
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{"midiout", 1}, "Sortie MIDI", juce::StringArray{"Off", "On"}, 0));
    l.add(std::make_unique<AudioParameterFloat>(ParameterID{"master", 1}, "Master", NormalisableRange<float>(0.f, 1.f), 0.8f, attr));
    return l;
}

OrbeRythmeProcessor::OrbeRythmeProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "ORBERYTHME", makeLayout()) {
    for (auto id : {"style", "bars", "human", "swing"}) apvts.addParameterListener(id, this);
    prLevel = apvts.getRawParameterValue("level"); prMute = apvts.getRawParameterValue("mute");
    prMaster = apvts.getRawParameterValue("master"); prMidi = apvts.getRawParameterValue("midiout");
    gen.setSettings(settings());
    applyDirty(2);
    startTimerHz(30);
}

OrbeRythmeProcessor::~OrbeRythmeProcessor() {
    stopTimer();
    for (auto id : {"style", "bars", "human", "swing"}) apvts.removeParameterListener(id, this);
}

void OrbeRythmeProcessor::prepareToPlay(double sampleRate, int) {
    sr = sampleRate; synth.prepare(sampleRate);
    nPend = 0; internalPos = 0.0; wasRunning = false;
}

orbe::Settings OrbeRythmeProcessor::settings() const {
    orbe::Settings s;
    s.style = juce::jlimit(0, (int)orbe::styles().size() - 1, (int)p("style"));
    const auto& st = orbe::styles()[(size_t)s.style];
    s.key = ctx.key; s.scale = ctx.scale >= 0 ? ctx.scale : st.scale;
    s.bars = p("bars") > 0.5f ? 8 : 4;
    s.density = ctx.density; s.human = p("human");
    s.seventh = ctx.seventhSet ? ctx.seventh : st.seventh;
    s.add9 = ctx.add9; s.borrow = ctx.borrow; s.arp = ctx.arp;
    s.seed = seed;
    return s;
}

void OrbeRythmeProcessor::parameterChanged(const String& id, float) { dirty.fetch_or(id == "style" || id == "bars" ? 2 : 4); }

void OrbeRythmeProcessor::timerCallback() {
    if (const int f = dirty.exchange(0)) applyDirty(f);
    else if (publishReq) publish();
}

void OrbeRythmeProcessor::rebuildFromSeed() {
    gen.lockMask = 0; gen.editMask = 0; gen.clearHistory();
    gen.setSettings(settings());
    gen.reseed(seed);
    gen.generateChords({});
}

void OrbeRythmeProcessor::applyDirty(int flags) {
    const auto s = settings();
    if (s.style != lastStyle) { ctx.scale = -1; ctx.seventhSet = false; lastStyle = s.style; flags |= 2; }   // un style règle sa gamme et sa 7e
    if (flags & 2) rebuildFromSeed(); else gen.setSettings(settings());
    publish();
}

void OrbeRythmeProcessor::generate() {
    seed = (uint32_t)juce::Random::getSystemRandom().nextInt();
    gen.pushUndo();
    gen.setSettings(settings());
    gen.lockMask = 0;
    gen.reseed(seed);
    gen.generateChords({});   // refait aussi la batterie (les frappes éditées sont remplacées)
    publish();
}

int OrbeRythmeProcessor::findHit(int row, float step) const {
    const auto& v = gen.song().nDrums;
    int best = -1; float bd = 0.5f + 1e-4f;
    for (size_t i = 0; i < v.size(); ++i)
        if (oryt::rowOfNote(v[i].n) == row) { const float d = std::fabs(v[i].t - step); if (d < bd) { bd = d; best = (int)i; } }
    return best;
}

void OrbeRythmeProcessor::toggleHit(int row, int step) {
    if (row < 0 || row > 6 || step < 0 || step >= bars() * 16) return;   // beginEdit() est appelé une fois par geste par l'éditeur
    const int i = findHit(row, (float)step);
    if (i >= 0) gen.removeNote(3, i); else gen.addNote(3, (float)step, 0.5f, oryt::kRowNote[row], 0.8f);
    publishReq = true;
}

void OrbeRythmeProcessor::cycleVelocity(int row, int step) {
    const int i = findHit(row, (float)step);
    if (i < 0) return;
    auto& x = gen.song().nDrums[(size_t)i];
    x.v = x.v > 0.85f ? 0.65f : x.v > 0.5f ? 0.4f : 1.f;
    gen.editMask |= 8u;
    publishReq = true;
}

bool OrbeRythmeProcessor::undo() { if (!gen.undo()) return false; seed = gen.settings().seed; publish(); return true; }
bool OrbeRythmeProcessor::redo() { if (!gen.redo()) return false; seed = gen.settings().seed; publish(); return true; }

void OrbeRythmeProcessor::publish() {
    const auto& song = gen.song();
    const float human = p("human"), sw0 = gen.style().swing, swEff = sw0 + (1.f - sw0) * p("swing");
    auto r = std::make_shared<oryt::Rendered>();
    r->bars = juce::jmax(1, (int)song.prog.size());
    r->bpm = (float)gen.style().bpm;
    const float L = (float)r->bars * 16.f;
    auto hash = [](uint32_t x) { x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16; return (float)(x & 0xFFFF) / 32767.5f - 1.f; };
    const auto& v = song.nDrums;
    for (size_t i = 0; i < v.size(); ++i) {   // même décalage et même jitter que la piste 3 d'Orbe (swing en plus, nul par défaut)
        const auto& n = v[i];
        float t = (n.t == std::floor(n.t) && (int)n.t % 2 == 1) ? n.t + swEff * 0.42f : n.t;
        const float j = hash((uint32_t)(i * 31 + 3u * 977u)), j2 = hash((uint32_t)(i * 17 + 3u * 4421u + 5u));
        t += j * 0.07f * human;
        t = std::fmod(std::max(0.f, t) + L, L);
        r->ev.push_back({t, n.d, (uint8_t)n.n, juce::jlimit(0.05f, 1.f, n.v * (1.f + j2 * 0.18f * human))});
    }
    std::sort(r->ev.begin(), r->ev.end(), [](const oryt::Ev& a, const oryt::Ev& b) { return a.t < b.t; });
    publishReq = false;
    const auto now = juce::Time::getMillisecondCounter();
    if (!owners.empty()) owners.back().replacedMs = now;
    owners.push_back({r, 0});
    live.store(r.get(), std::memory_order_release);
    owners.erase(std::remove_if(owners.begin(), owners.end() - 1, [now](const Owned& o) { return now - o.replacedMs > 2000u; }), owners.end() - 1);   // jamais libéré côté audio
}

juce::File OrbeRythmeProcessor::writeMidiFile() {
    const auto& st = gen.style();
    String name = "Orbe Rythme " + String(st.label) + ".mid";
    name = name.replaceCharacters("#/\\:&", "s----");
    auto f = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("Orbe Rythme").getChildFile(name);
    f.getParentDirectory().createDirectory();
    const float bpm = hostBpm.load();
    const auto bytes = gen.toMidi(false, false, false, true, bpm > 0.f ? bpm : 0.f);
    f.replaceWithData(bytes.data(), bytes.size());
    return f;
}

// ---------------- état ----------------

void OrbeRythmeProcessor::getStateInformation(juce::MemoryBlock& dest) {
    auto tree = apvts.copyState();
    kit::stampState(tree);
    tree.setProperty("seed", (int)seed, nullptr);
    tree.setProperty("ctxKey", ctx.key, nullptr); tree.setProperty("ctxScale", ctx.scale, nullptr);
    tree.setProperty("ctxSeventh", ctx.seventhSet ? (ctx.seventh ? 1 : 0) : -1, nullptr);
    tree.setProperty("ctxAdd9", ctx.add9 ? 1 : 0, nullptr); tree.setProperty("ctxBorrow", ctx.borrow ? 1 : 0, nullptr);
    tree.setProperty("ctxArp", ctx.arp, nullptr); tree.setProperty("ctxDens", ctx.density, nullptr);
    String prog, p2, sp;
    for (int d : gen.song().prog) prog << d << ",";
    for (size_t i = 0; i < gen.song().prog2.size(); ++i) { p2 << gen.song().prog2[i] << ","; sp << gen.song().split[i] << ","; }
    tree.setProperty("prog", prog, nullptr); tree.setProperty("prog2", p2, nullptr); tree.setProperty("split", sp, nullptr);
    tree.setProperty("edited", (int)(gen.editMask >> 3 & 1u), nullptr);
    if (gen.editMask >> 3 & 1u) tree.setProperty("n3", String(orbe::Generator::pack(gen.song().nDrums)), nullptr);   // frappes modifiées à la main
    if (auto xml = tree.createXml()) copyXmlToBinary(*xml, dest);
}

// Ancien Orbe (balise « ORBE ») : paramètres de batterie repris (style, mesures, humain, niveau vol3, muet mute3, sortie MIDI, master),
// contexte harmonique et graine conservés pour que la batterie générée soit la même ; frappes éditées ou verrouillées (« n3 ») restaurées telles quelles.
// Les accords, la lead, la basse et le reste des réglages d'Orbe sont ignorés (ils restent dans Orbe).
void OrbeRythmeProcessor::loadLegacyOrbe(const juce::ValueTree& old) {
    auto setP = [this](const char* id, float v) { if (auto* q = apvts.getParameter(id)) q->setValueNotifyingHost(q->convertTo0to1(v)); };
    ctx = Ctx{};
    for (auto c : old) {
        if (!c.hasType("PARAM")) continue;
        const String id = c.getProperty("id").toString();
        const float v = (float)(double)c.getProperty("value", 0.0);
        if (id == "style") setP("style", v); else if (id == "bars") setP("bars", v); else if (id == "human") setP("human", v);
        else if (id == "vol3") setP("level", v); else if (id == "mute3") setP("mute", v > 0.5f ? 1.f : 0.f);
        else if (id == "midiout") setP("midiout", v > 0.5f ? 1.f : 0.f); else if (id == "master") setP("master", v);
        else if (id == "key") ctx.key = juce::jlimit(0, 11, (int)v);
        else if (id == "scale") ctx.scale = juce::jlimit(0, (int)orbe::scales().size() - 1, (int)v);
        else if (id == "seventh") { ctx.seventh = v > 0.5f; ctx.seventhSet = true; }
        else if (id == "add9") ctx.add9 = v > 0.5f; else if (id == "borrow") ctx.borrow = v > 0.5f;
        else if (id == "arp") ctx.arp = juce::jlimit(0, 3, (int)v); else if (id == "density") ctx.density = v;
    }
}

void OrbeRythmeProcessor::setStateInformation(const void* data, int size) {
    auto xml = getXmlFromBinary(data, size);
    if (!xml) return;
    juce::ValueTree tree = juce::ValueTree::fromXml(*xml);
    if (xml->hasTagName(apvts.state.getType())) {
        apvts.replaceState(tree);
        ctx = Ctx{};
        ctx.key = juce::jlimit(0, 11, (int)tree.getProperty("ctxKey", 9));
        ctx.scale = juce::jlimit(-1, (int)orbe::scales().size() - 1, (int)tree.getProperty("ctxScale", -1));
        const int sv = (int)tree.getProperty("ctxSeventh", -1);
        ctx.seventhSet = sv >= 0; ctx.seventh = sv > 0;
        ctx.add9 = (int)tree.getProperty("ctxAdd9", 0) != 0; ctx.borrow = (int)tree.getProperty("ctxBorrow", 0) != 0;
        ctx.arp = juce::jlimit(0, 3, (int)tree.getProperty("ctxArp", 0)); ctx.density = (float)(double)tree.getProperty("ctxDens", 0.55);
    } else if (xml->hasTagName("ORBE")) {
        loadLegacyOrbe(tree);
    } else return;
    seed = (uint32_t)(int)tree.getProperty("seed", (int)seed);
    auto ints = [&](const char* k) { std::vector<int> v; for (auto& t : juce::StringArray::fromTokens(tree.getProperty(k).toString(), ",", "")) if (t.isNotEmpty()) v.push_back(t.getIntValue()); return v; };
    const auto prog = ints("prog"), prog2 = ints("prog2"), split = ints("split");
    const auto s = settings();
    gen.lockMask = 0; gen.editMask = 0; gen.clearHistory();
    gen.setSettings(s);
    lastStyle = s.style;
    gen.reseed(seed);
    if ((int)prog.size() == s.bars) gen.setProgression(prog, prog2.size() == prog.size() ? prog2 : std::vector<int>{}, split.size() == prog.size() ? split : std::vector<int>{});
    else gen.generateChords({});
    if (tree.hasProperty("n3")) {   // frappes modifiées (Rythme) ou piste batterie modifiée / verrouillée (ancien Orbe)
        auto v = orbe::Generator::unpack(tree.getProperty("n3").toString().toStdString());
        if (!v.empty()) { gen.song().nDrums = v; gen.editMask |= 8u; }
    }
    publish();
    dirty = 0;
}

// ---------------- audio ----------------

void OrbeRythmeProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals nd;
    const int n = buffer.getNumSamples();
    buffer.clear();
    if (buffer.getNumChannels() < 2) return;
    const oryt::Rendered* r = live.load(std::memory_order_acquire);
    if (!r) return;

    bool hostPlaying = false;
    double hostStep = 0.0, bpm = r->bpm;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition()) {
            if (auto b = pos->getBpm()) { bpm = *b; hostBpm.store((float)*b); }
            if (pos->getIsPlaying()) if (auto ppq = pos->getPpqPosition()) { hostPlaying = true; hostStep = *ppq * 4.0; }
        }
    const bool running = hostPlaying || audition.load();
    const double L = (double)r->bars * 16.0;
    const double sps = std::max(1e-6, bpm / 60.0 * 4.0 / sr);
    if (running && !wasRunning && !hostPlaying) internalPos = 0.0;
    wasRunning = running;

    struct Trig { int off, note; float vel; };
    static thread_local Trig trigs[512];
    int nt = 0;
    double p0 = 0.0;
    if (running) {
        p0 = hostPlaying ? std::fmod(std::max(0.0, hostStep), L) : internalPos;
        const double p1 = p0 + (double)n * sps;
        const float vol = prLevel->load();
        const bool muted = prMute->load() > 0.5f;
        if (!muted)
            for (int k = 0; k < 2; ++k)
                for (const auto& e : r->ev) {
                    const double te = (double)e.t + k * L;
                    if (te < p0 || te >= p1 || nt >= 512) continue;
                    trigs[nt++] = {juce::jlimit(0, n - 1, (int)((te - p0) / sps)), e.n, e.v * vol};
                }
    }

    const bool midiOut = prMidi->load() > 0.5f;
    const float master = prMaster->load();
    float* Lc = buffer.getWritePointer(0);
    float* Rc = buffer.getWritePointer(1);
    int pos = 0;
    constexpr int ch = 10;
    for (int i = 0; i < nt; ++i) {
        if (trigs[i].off > pos) { synth.render(Lc + pos, Rc + pos, trigs[i].off - pos, master); pos = trigs[i].off; }
        synth.trigger(3, trigs[i].note, trigs[i].vel, std::max(1, (int)(0.1 * sr)), false);
        if (midiOut) {
            const int note = trigs[i].note;
            for (int j = 0; j < nPend; ++j)   // même frappe encore tenue : son note-off part avant le nouveau note-on
                if (pend[j].note == note) {
                    midi.addEvent(juce::MidiMessage::noteOff(ch, note), juce::jlimit(0, n - 1, std::min(pend[j].left, trigs[i].off)));
                    pend[j] = pend[--nPend];
                    break;
                }
            if (nPend < 128) {
                midi.addEvent(juce::MidiMessage::noteOn(ch, note, juce::jlimit(0.01f, 1.f, trigs[i].vel)), trigs[i].off);
                pend[nPend++] = {note, ch, trigs[i].off + (int)(0.1 * sr)};
            }
        }
    }
    if (pos < n) synth.render(Lc + pos, Rc + pos, n - pos, master);

    for (int i = 0; i < nPend;) {   // notes-off dus (y compris après l'arrêt de la lecture : rien ne reste tenu)
        if (pend[i].left < n) { midi.addEvent(juce::MidiMessage::noteOff(pend[i].ch, pend[i].note), std::max(0, pend[i].left)); pend[i] = pend[--nPend]; }
        else { pend[i].left -= n; ++i; }
    }

    if (running) {
        if (!hostPlaying) internalPos = std::fmod(internalPos + (double)n * sps, L);
        playStep.store((float)std::fmod(p0, L));
    } else playStep.store(-1.f);
}

juce::AudioProcessorEditor* OrbeRythmeProcessor::createEditor() { return new OrbeRythmeEditor(*this); }

#ifndef ORBE_NO_FACTORY
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new OrbeRythmeProcessor(); }
#endif
