#include "OrbeProcessor.h"
#include "OrbeEditor.h"
#include <algorithm>

using juce::String;

static const char* kKeys[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

juce::AudioProcessorValueTreeState::ParameterLayout OrbeProcessor::makeLayout() {
    using juce::AudioParameterBool, juce::AudioParameterChoice, juce::AudioParameterFloat, juce::NormalisableRange, juce::ParameterID;
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    juce::StringArray styleNames, scaleNames;
    for (auto& s : orbe::styles()) styleNames.add(s.label);
    for (auto& s : orbe::scales()) scaleNames.add(s.label);
    auto pct = [](float v, int) { return String(juce::roundToInt(v * 100.f)) + " %"; };
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{"style", 1}, "Style", styleNames, 0));
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{"key", 1}, "Tonique", juce::StringArray(kKeys, 12), 9));
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{"scale", 1}, "Gamme", scaleNames, 1));
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{"bars", 1}, "Longueur", juce::StringArray{"4 mesures", "8 mesures"}, 0));
    l.add(std::make_unique<AudioParameterFloat>(ParameterID{"density", 1}, "Densite", NormalisableRange<float>(0.f, 1.f), 0.55f,
                                                juce::AudioParameterFloatAttributes().withStringFromValueFunction(pct)));
    l.add(std::make_unique<AudioParameterFloat>(ParameterID{"human", 1}, "Humanisation", NormalisableRange<float>(0.f, 1.f), 0.f,
                                                juce::AudioParameterFloatAttributes().withStringFromValueFunction(pct)));
    l.add(std::make_unique<AudioParameterBool>(ParameterID{"seventh", 1}, "7e", false));
    l.add(std::make_unique<AudioParameterBool>(ParameterID{"add9", 1}, "Add9", false));
    l.add(std::make_unique<AudioParameterBool>(ParameterID{"borrow", 1}, "Emprunts", false));
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{"arp", 1}, "Jeu des accords", juce::StringArray{"Plaques", "Montant", "Aller-retour", "Hasard"}, 0));
    const char* tn[4] = {"Accords", "Lead", "Basse", "Batterie"};
    const float tv[4] = {0.8f, 0.8f, 0.85f, 0.85f};
    for (int i = 0; i < 4; ++i) {
        l.add(std::make_unique<AudioParameterFloat>(ParameterID{"vol" + String(i), 1}, String(tn[i]) + " volume", NormalisableRange<float>(0.f, 1.f), tv[i]));
        l.add(std::make_unique<AudioParameterBool>(ParameterID{"mute" + String(i), 1}, String(tn[i]) + " muet", false));
    }
    l.add(std::make_unique<AudioParameterBool>(ParameterID{"midiout", 1}, "Sortie MIDI", false));
    l.add(std::make_unique<AudioParameterFloat>(ParameterID{"master", 1}, "Master", NormalisableRange<float>(0.f, 1.f), 0.8f));
    l.add(kit::v3::notes::makeAccidParam());   // ajouté en dernier : écriture dièses / bémols (noms seulement)
    return l;
}

juce::AudioProcessor::BusesProperties OrbeProcessor::makeBuses() {
    auto b = BusesProperties();
#ifdef ORBE_FX
    b = b.withInput("Input", juce::AudioChannelSet::stereo(), true);
#endif
    return b.withOutput("Output", juce::AudioChannelSet::stereo(), true);
}

OrbeProcessor::OrbeProcessor()
    : AudioProcessor(makeBuses()),
      apvts(*this, nullptr, "ORBE", makeLayout()) {
    for (auto id : {"style", "key", "scale", "bars", "density", "human", "seventh", "add9", "borrow", "arp"}) apvts.addParameterListener(id, this);
    for (int i = 0; i < 4; ++i) { prVol[i] = apvts.getRawParameterValue("vol" + String(i)); prMute[i] = apvts.getRawParameterValue("mute" + String(i)); }
    prMaster = apvts.getRawParameterValue("master"); prMidi = apvts.getRawParameterValue("midiout");
    gen.setSettings(settingsFromParams());
    applyDirty(2);
    startTimerHz(30);
}

OrbeProcessor::~OrbeProcessor() {
    stopTimer();
    for (auto id : {"style", "key", "scale", "bars", "density", "human", "seventh", "add9", "borrow", "arp"}) apvts.removeParameterListener(id, this);
}

bool OrbeProcessor::isBusesLayoutSupported(const BusesLayout& l) const {
#ifdef ORBE_FX
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() && l.getMainInputChannelSet() == juce::AudioChannelSet::stereo();
#else
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() && l.getMainInputChannelSet().isDisabled();
#endif
}

void OrbeProcessor::prepareToPlay(double sampleRate, int) {
    sr = sampleRate;
    synth.prepare(sampleRate);
    nPend = 0;
    internalPos = 0.0;
    wasRunning = false;
}

orbe::Settings OrbeProcessor::settingsFromParams() const {
    orbe::Settings s;
    s.style = (int)p("style");
    s.key = (int)p("key");
    s.scale = (int)p("scale");
    s.bars = p("bars") > 0.5f ? 8 : 4;
    s.density = p("density");
    s.human = p("human");
    s.seventh = p("seventh") > 0.5f;
    s.add9 = p("add9") > 0.5f;
    s.borrow = p("borrow") > 0.5f;
    s.arp = (int)p("arp");
    s.seed = seed;
    s.leadSeed = leadSeed;
    return s;
}

void OrbeProcessor::parameterChanged(const String& id, float) {
    int f = 0;
    if (id == "style" || id == "bars" || id == "borrow") f = 2;
    else if (id == "human") f = 4;
    else f = 1;
    dirty.fetch_or(f);
}

void OrbeProcessor::timerCallback() {
    if (const int f = dirty.exchange(0)) applyDirty(f);
    else if (publishReq) publish();
}

void OrbeProcessor::setStateInformation(const void* data, int size) {
    auto xml = getXmlFromBinary(data, size);
    if (!xml || !xml->hasTagName(apvts.state.getType())) return;
    auto tree = juce::ValueTree::fromXml(*xml);
    apvts.replaceState(tree);
    seed = (uint32_t)(int)tree.getProperty("seed", (int)seed);
    leadSeed = (uint32_t)(int)tree.getProperty("leadSeed", (int)leadSeed);
    locks.clear();
    for (auto& t : juce::StringArray::fromTokens(tree.getProperty("locks").toString(), ",", "")) if (t.isNotEmpty()) locks.insert(t.getIntValue());
    auto ints = [&](const char* k) { std::vector<int> v; for (auto& t : juce::StringArray::fromTokens(tree.getProperty(k).toString(), ",", "")) if (t.isNotEmpty()) v.push_back(t.getIntValue()); return v; };
    const auto prog = ints("prog"), prog2 = ints("prog2"), split = ints("split");
    auto s = settingsFromParams();
    gen.lockMask = 0; gen.editMask = 0; gen.clearHistory();
    gen.setSettings(s);
    lastStyle = s.style;
    gen.reseed(seed);
    gen.reseedLead(leadSeed);
    if ((int)prog.size() == s.bars) gen.setProgression(prog, prog2.size() == prog.size() ? prog2 : std::vector<int>{}, split.size() == prog.size() ? split : std::vector<int>{});
    else gen.generateChords(locks);
    gen.generateLead();
    for (int t = 0; t < 4; ++t)  // pistes modifiées ou verrouillées : notes restaurées telles quelles
        if (tree.hasProperty("n" + String(t))) { auto v = orbe::Generator::unpack(tree.getProperty("n" + String(t)).toString().toStdString()); if (!v.empty()) gen.song().tr(t) = v; }
    gen.lockMask = (unsigned)(int)tree.getProperty("tlock", 0) & 15u;
    gen.editMask = (unsigned)(int)tree.getProperty("edited", 0) & 15u;
    publish();
    dirty = 0;  // replaceState a déclenché les écouteurs : l'état restauré est déjà à jour
}

void OrbeProcessor::getStateInformation(juce::MemoryBlock& dest) {
    auto tree = apvts.copyState();
    tree.setProperty("seed", (int)seed, nullptr);
    tree.setProperty("leadSeed", (int)leadSeed, nullptr);
    String prog, lk;
    for (int d : gen.song().prog) prog << d << ",";
    for (int i : locks) lk << i << ",";
    tree.setProperty("prog", prog, nullptr);
    tree.setProperty("locks", lk, nullptr);
    String p2, sp;
    for (size_t i = 0; i < gen.song().prog2.size(); ++i) { p2 << gen.song().prog2[i] << ","; sp << gen.song().split[i] << ","; }
    tree.setProperty("prog2", p2, nullptr);
    tree.setProperty("split", sp, nullptr);
    tree.setProperty("tlock", (int)gen.lockMask, nullptr);
    tree.setProperty("edited", (int)gen.editMask, nullptr);
    for (int t = 0; t < 4; ++t)  // notes des pistes modifiées à la main ou verrouillées
        if ((gen.lockMask | gen.editMask) >> t & 1) tree.setProperty("n" + String(t), String(orbe::Generator::pack(gen.song().tr(t))), nullptr);
    if (auto xml = tree.createXml()) copyXmlToBinary(*xml, dest);
}

// ---------------- génération (thread message) ----------------

void OrbeProcessor::applyDirty(int flags) {
    auto s = settingsFromParams();
    const bool styleChanged = s.style != lastStyle;
    if (styleChanged) {
        // un style règle aussi la gamme et la 7e, comme dans Orbe web
        const auto& st = orbe::styles()[(size_t)s.style];
        auto set = [&](const char* id, float v) { if (auto* q = apvts.getParameter(id)) q->setValueNotifyingHost(q->convertTo0to1(v)); };
        set("scale", (float)st.scale);
        set("seventh", st.seventh ? 1.f : 0.f);
        lastStyle = s.style;
        s = settingsFromParams();
        dirty = 0;
        flags |= 2;
    }
    gen.setSettings(s);
    if (flags & 2) {
        if (gen.song().prog.size() != (size_t)s.bars) locks.clear();
        gen.clearHistory();
        gen.reseed(seed);
        gen.generateChords(locks);
        gen.generateLead();
    } else if (flags & 1) {
        gen.pushUndo();
        gen.setProgression(gen.song().prog);
        gen.generateLead();
    }
    publish();
}

void OrbeProcessor::generate(Gen kind) {
    auto& rnd = juce::Random::getSystemRandom();
    auto s = settingsFromParams();
    if (kind != Gen::Lead) seed = (uint32_t)rnd.nextInt();
    if (kind != Gen::Chords) leadSeed = (uint32_t)rnd.nextInt();
    s.seed = seed; s.leadSeed = leadSeed;
    gen.pushUndo();
    gen.setSettings(s);
    if (kind != Gen::Lead) {
        if (gen.song().prog.size() != (size_t)s.bars) locks.clear();
        gen.reseed(seed);
        gen.generateChords(locks);
    }
    gen.reseedLead(leadSeed);
    gen.generateLead();
    publish();
}

void OrbeProcessor::toggleLock(int c) {
    if (!locks.erase(c)) locks.insert(c);
}

void OrbeProcessor::rerollChord(int c) {
    auto prog = gen.song().prog;
    if (c < 0 || c >= (int)prog.size()) return;
    auto& rnd = juce::Random::getSystemRandom();
    auto sug = gen.suggest(c);  // degrés que le style utilise à cette place, plutôt qu'un tirage uniforme
    sug.erase(std::remove(sug.begin(), sug.end(), prog[(size_t)c]), sug.end());
    if (sug.empty()) sug = {(prog[(size_t)c] + 1 + rnd.nextInt(6)) % 7};
    gen.pushUndo();
    gen.setSettings(settingsFromParams());
    gen.setChord(c, 0, sug[(size_t)rnd.nextInt((int)sug.size())]);
    publish();
}

void OrbeProcessor::setChord(int bar, int slot, int deg) {
    gen.pushUndo();
    gen.setSettings(settingsFromParams());
    gen.setChord(bar, slot, deg);
    publish();
}

bool OrbeProcessor::undo() { if (!gen.undo()) return false; syncSeeds(); publish(); return true; }
bool OrbeProcessor::redo() { if (!gen.redo()) return false; syncSeeds(); publish(); return true; }

void OrbeProcessor::publish() {
    const auto& song = gen.song();
    const float human = p("human");
    auto r = std::make_shared<OrbeRendered>();
    r->bars = (int)song.prog.size();
    r->bpm = (float)gen.style().bpm;
    const float L = (float)r->bars * 16.f;
    auto hash = [](uint32_t x) { x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16; return (float)(x & 0xFFFF) / 32767.5f - 1.f; };
    auto add = [&](const std::vector<orbe::Note>& v, int track) {
        for (size_t i = 0; i < v.size(); ++i) {
            const auto& n = v[i];
            float t = gen.swingT(n.t);
            const float j = hash((uint32_t)(i * 31 + (size_t)track * 977)), j2 = hash((uint32_t)(i * 17 + (size_t)track * 4421 + 5));
            t += j * (track == 3 ? 0.07f : 0.13f) * human;
            t = std::fmod(std::max(0.f, t) + L, L);
            r->ev.push_back({t, n.d, (uint8_t)n.n, (uint8_t)track, juce::jlimit(0.05f, 1.f, n.v * (1.f + j2 * 0.18f * human)), (uint8_t)n.gl});
        }
    };
    add(song.nChords, 0); add(song.nLead, 1); add(song.nBass, 2); add(song.nDrums, 3);
    std::sort(r->ev.begin(), r->ev.end(), [](const OrbeEv& a, const OrbeEv& b) { return a.t < b.t; });
    publishReq = false;
    const auto now = juce::Time::getMillisecondCounter();
    if (!owners.empty()) owners.back().replacedMs = now;
    owners.push_back({r, 0});
    live.store(r.get(), std::memory_order_release);
    // libère les instantanés remplacés depuis plus de 2 s (le dernier, en service, est toujours gardé)
    owners.erase(std::remove_if(owners.begin(), owners.end() - 1, [now](const Owned& o) { return now - o.replacedMs > 2000u; }), owners.end() - 1);
}

juce::File OrbeProcessor::writeMidiFile(bool c, bool l, bool b, bool d) {
    const auto& st = gen.style();
    const auto s = gen.settings();
    String name = "Orbe " + String(st.label) + " " + kKeys[s.key] + " " + String(orbe::scales()[(size_t)s.scale].id) + ".mid";
    name = name.replaceCharacters("#/\\:", "s---");
    auto f = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("Orbe").getChildFile(name);
    f.getParentDirectory().createDirectory();
    const float bpm = hostBpm.load();
    const auto bytes = gen.toMidi(c, l, b, d, bpm > 0.f ? bpm : 0.f);
    f.replaceWithData(bytes.data(), bytes.size());
    return f;
}

// ---------------- audio ----------------

void OrbeProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals nd;
    const int n = buffer.getNumSamples();
#ifndef ORBE_FX
    buffer.clear();  // version effet : l'audio entrant passe tel quel, l'aperçu s'y ajoute
#endif
    if (buffer.getNumChannels() < 2) return;

    const OrbeRendered* r = live.load(std::memory_order_acquire);
    if (!r || r->ev.empty()) return;

    bool hostPlaying = false;
    double hostStep = 0.0, bpm = r->bpm;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition()) {
            if (auto b = pos->getBpm()) { bpm = *b; hostBpm.store((float)*b); }
            if (pos->getIsPlaying()) if (auto ppq = pos->getPpqPosition()) { hostPlaying = true; hostStep = *ppq * 4.0; }
        }
    const bool running = hostPlaying || audition.load();
    const double L = (double)r->bars * 16.0;
    const double sps = bpm / 60.0 * 4.0 / sr;
    if (running && !wasRunning && !hostPlaying) internalPos = 0.0;
    wasRunning = running;

    struct Trig { int off, track, note, gate; float vel; bool gl; };
    static thread_local Trig trigs[512];
    int nt = 0;
    double p0 = 0.0;
    if (running) {
        p0 = hostPlaying ? std::fmod(std::max(0.0, hostStep), L) : internalPos;
        const double p1 = p0 + (double)n * sps;
        float vol[4]; bool mute[4];
        for (int i = 0; i < 4; ++i) { vol[i] = prVol[i]->load(); mute[i] = prMute[i]->load() > 0.5f; }
        for (int k = 0; k < 2; ++k)
            for (const auto& e : r->ev) {
                const double te = (double)e.t + k * L;
                if (te < p0 || te >= p1 || nt >= 512 || mute[e.track]) continue;
                trigs[nt++] = {juce::jlimit(0, n - 1, (int)((te - p0) / sps)), e.track, e.n, std::max(1, (int)((double)e.d / sps)), e.v * vol[e.track], e.gl != 0};
            }
    }

    const bool midiOut = prMidi->load() > 0.5f;
    int pos = 0;
    float* Lc = buffer.getWritePointer(0);
    float* Rc = buffer.getWritePointer(1);
    // MASTER ne règle que l'aperçu généré : dans Orbe FX, l'audio entrant traverse inchangé
    const float master = prMaster->load();
    static const int chans[4] = {1, 2, 3, 10};
    for (int i = 0; i < nt; ++i) {
        if (trigs[i].off > pos) { synth.render(Lc + pos, Rc + pos, trigs[i].off - pos, master); pos = trigs[i].off; }
        synth.trigger(trigs[i].track, trigs[i].note, trigs[i].vel, trigs[i].gate, trigs[i].gl);
        if (midiOut) {
            const int ch = chans[trigs[i].track], note = trigs[i].note;
            // même note encore tenue sur ce canal (note commune entre deux accords, legato, chevauchement d'humanisation) :
            // son note-off part d'abord, au plus tard sur l'échantillon du nouveau note-on, et AVANT lui dans le tampon
            for (int j = 0; j < nPend; ++j)
                if (pend[j].note == note && pend[j].ch == ch) {
                    midi.addEvent(juce::MidiMessage::noteOff(ch, note), juce::jlimit(0, n - 1, std::min(pend[j].left, trigs[i].off)));
                    pend[j] = pend[--nPend];
                    break;
                }
            if (nPend < 128) {
                const int g = trigs[i].track == 3 ? (int)(0.1 * sr) : trigs[i].gate;
                midi.addEvent(juce::MidiMessage::noteOn(ch, note, juce::jlimit(0.01f, 1.f, trigs[i].vel)), trigs[i].off);
                pend[nPend++] = {note, ch, trigs[i].off + g};
            }
        }
    }
    if (pos < n) synth.render(Lc + pos, Rc + pos, n - pos, master);

    for (int i = 0; i < nPend;) {
        if (pend[i].left < n) {
            midi.addEvent(juce::MidiMessage::noteOff(pend[i].ch, pend[i].note), std::max(0, pend[i].left));
            pend[i] = pend[--nPend];
        } else { pend[i].left -= n; ++i; }
    }

    if (running) {
        if (!hostPlaying) { internalPos = std::fmod(internalPos + (double)n * sps, L); }
        playStep.store((float)std::fmod(p0, L));
    } else playStep.store(-1.f);
}

juce::AudioProcessorEditor* OrbeProcessor::createEditor() { return new OrbeEditor(*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new OrbeProcessor(); }
