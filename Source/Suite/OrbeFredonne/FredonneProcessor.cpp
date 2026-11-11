#include "FredonneProcessor.h"
#include "FredonneEditor.h"

using juce::String;

juce::AudioProcessorValueTreeState::ParameterLayout FredonneProcessor::makeLayout() {
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    auto pct = [](float v, int) { return String(roundToInt(v * 100.f)) + " %"; };
    auto ms = [](float v, int) { return String(roundToInt(v)) + " ms"; };
    StringArray keys{"Auto"}; for (auto* k : {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"}) keys.add(k);
    l.add(std::make_unique<AudioParameterFloat>(ParameterID{"sens", 1}, "Sensibilite", NormalisableRange<float>(0.f, 1.f), 0.5f, AudioParameterFloatAttributes().withStringFromValueFunction(pct)));
    l.add(std::make_unique<AudioParameterFloat>(ParameterID{"minlen", 1}, "Note la plus courte", NormalisableRange<float>(40.f, 300.f, 1.f), 90.f, AudioParameterFloatAttributes().withStringFromValueFunction(ms)));
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{"grid", 1}, "Grille", StringArray{"Libre", "1/4", "1/8", "1/16"}, 3));
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{"oct", 1}, "Octave", StringArray{"-2", "-1", "0", "+1"}, 2));
    l.add(std::make_unique<AudioParameterBool>(ParameterID{"snap", 1}, "Calage sur gamme", true));
    l.add(std::make_unique<AudioParameterBool>(ParameterID{"tune", 1}, "Justesse", true));
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{"key", 1}, "Tonalite", keys, 0));
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{"mode", 1}, "Mode", StringArray{"Auto", "Mineur", "Majeur", "Chromatique"}, 0));
    l.add(std::make_unique<AudioParameterFloat>(ParameterID{"bpm", 1}, "Tempo", NormalisableRange<float>(50.f, 200.f, 0.1f), 100.f));
    l.add(std::make_unique<AudioParameterBool>(ParameterID{"follow", 1}, "Suivre l'hote", true));
    l.add(std::make_unique<AudioParameterBool>(ParameterID{"play", 1}, "Jouer les notes", true));
    l.add(std::make_unique<AudioParameterBool>(ParameterID{"monitor", 1}, "Ecoute directe", false));
    l.add(kit::v3::notes::makeAccidParam());   // ajouté en dernier : écriture dièses / bémols (noms seulement)
    return l;
}

FredonneProcessor::FredonneProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true).withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "ORBEFREDONNE", makeLayout()) {
    for (auto id : {"sens", "minlen", "grid", "oct", "snap", "tune", "key", "mode", "bpm", "follow"}) apvts.addParameterListener(id, this);
    pPlay = apvts.getRawParameterValue("play"); pMonitor = apvts.getRawParameterValue("monitor"); pFollow = apvts.getRawParameterValue("follow"); pBpm = apvts.getRawParameterValue("bpm");
    takeP = std::make_shared<const Take>();
    startTimerHz(15);
}
FredonneProcessor::~FredonneProcessor() {
    stopTimer(); worker.stopThread(30000);
    for (auto id : {"sens", "minlen", "grid", "oct", "snap", "tune", "key", "mode", "bpm", "follow"}) apvts.removeParameterListener(id, this);
}

bool FredonneProcessor::isBusesLayoutSupported(const BusesLayout& l) const {
    const auto in = l.getMainInputChannelSet();
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() && (in.isDisabled() || in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo());
}

void FredonneProcessor::prepareToPlay(double rate, int) {
    // Un changement de fréquence (ou de tampon) de l'hôte ne doit jamais effacer une prise : une prise en cours est close et
    // analysée à sa propre fréquence (recSr) ; le tampon n'est redimensionné que lorsqu'il ne contient rien à analyser.
    sr = rate;
    int s = Recording;
    if (st.compare_exchange_strong(s, Captured)) {}   // prise en cours : on la garde telle quelle
    const int want = (int)(rate * kMaxSec) + 4096;
    if ((s == Idle || s == Armed) && (int)rec.size() != want) {
        rec.assign((size_t)want, 0.f); recMax = want; recLen = 0; recSr = rate;
    }
    yin.init(rate); yDec = yin.dec; yAcc = 0.f; yCount = 0; std::fill(pre, pre + 4096, 0.f); preIdx = 0; lastP1 = -1.0;
}

double FredonneProcessor::effectiveBpm() const { const float h = hostBpm.load(); return (pFollow->load() > .5f && h > 0.f) ? (double)h : (double)pBpm->load(); }

oa::SegParams FredonneProcessor::segParams() const {
    oa::SegParams s; static const int grids[4] = {0, 4, 2, 1};
    s.sens = p("sens"); s.minlen = p("minlen") / 1000.0; s.grid = grids[juce::jlimit(0, 3, (int)p("grid"))]; s.oct = (int)p("oct") - 2;
    s.snap = p("snap") > .5f; s.tune = p("tune") > .5f; s.key = (int)p("key") - 1; s.mode = (int)p("mode") - 1; s.bpm = effectiveBpm();
    return s;
}

void FredonneProcessor::processBlock(juce::AudioBuffer<float>& buf, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals nd;
    const int n = buf.getNumSamples(), nch = juce::jmin(2, getTotalNumInputChannels());
    double ppq = 0.0; bool playing = false;
    if (auto* ph = getPlayHead()) if (auto pos = ph->getPosition()) {
        if (auto b = pos->getBpm()) hostBpm.store((float)*b);
        if (pos->getIsPlaying()) if (auto q = pos->getPpqPosition()) { playing = true; ppq = *q; }
    }
    float pk = 0.f;
    if (nch > 0) {
        const float* L = buf.getReadPointer(0); const float* R = buf.getReadPointer(nch > 1 ? 1 : 0);
        const int s0 = st.load();
        for (int i = 0; i < n; ++i) {
            const float m = (L[i] + R[i]) * .5f; pk = juce::jmax(pk, std::abs(m));
            yAcc += m; if (++yDec >= yin.dec) { yin.push(yAcc / (float)yin.dec); yAcc = 0.f; yDec = 0; if (++yCount >= 512) { yCount = 0; liveHz.store(yin.analyze()); } }
        }
        level.store(juce::jmax(pk, level.load() * 0.9f));
        if (s0 == Armed) {
            if (pk > 0.01f) {   // le premier son déclenche l'enregistrement ; 4096 échantillons de pré-roll évitent de couper l'attaque
                for (int i = 0; i < 4096; ++i) rec[(size_t)i] = pre[(preIdx + i) & 4095];
                recSr = sr; recLen.store(4096); takeStart.store(playing ? ppq - 4096.0 / sr * effectiveBpm() / 60.0 : 0.0);
                st.store(Recording);
            } else for (int i = 0; i < n; ++i) { pre[preIdx] = (L[i] + R[i]) * .5f; preIdx = (preIdx + 1) & 4095; }
        }
        if (st.load() == Recording) {
            int len = recLen.load(std::memory_order_relaxed); const int k = juce::jmin(n, recMax - len);
            for (int i = 0; i < k; ++i) rec[(size_t)(len + i)] = (L[i] + R[i]) * .5f;
            recLen.store(len + k, std::memory_order_release);
            if (len + k >= recMax) { int e = Recording; st.compare_exchange_strong(e, Captured); }
        }
    }
    if (pMonitor->load() < .5f || nch == 0) buf.clear();
    else for (int c = 2; c < buf.getNumChannels(); ++c) buf.clear(c, 0, n);

    // lecture des notes dans l'hôte, calée sur la position de lecture
    auto notes = std::atomic_load(&playNotes);
    const bool go = playing && pPlay->load() > .5f && notes && !notes->empty();
    if (go) {
        const double bpm = effectiveBpm(), p0 = ppq, p1 = p0 + (double)n * bpm / 60.0 / sr, t0 = takeStart.load();
        if (lastP1 < 0 || p0 < lastP1 - 1e-6 || p0 > lastP1 + 0.05) for (int k = 0; k < 128; ++k) if (active[k]) { midi.addEvent(juce::MidiMessage::noteOff(1, k), 0); active[k] = false; }
        for (auto& x : *notes) {
            const double on = t0 + x.t, off = on + x.d;
            auto at = [&](double b) { return juce::jlimit(0, n - 1, (int)((b - p0) / (p1 - p0) * (double)n)); };
            const int note = juce::jlimit(0, 127, x.n);
            // vélocité : la même que dans le fichier .MID (tirée du niveau de la note)
            if (on >= p0 && on < p1) { midi.addEvent(juce::MidiMessage::noteOn(1, note, (juce::uint8)juce::jlimit(1, 127, (int)std::floor(x.v * 115.0 + 0.5) + 8)), at(on)); active[note] = true; }
            if (off >= p0 && off < p1 && active[note]) { midi.addEvent(juce::MidiMessage::noteOff(1, note), at(off)); active[note] = false; }
        }
        lastP1 = p1;
    } else if (wasPlaying || lastP1 >= 0) {
        for (int k = 0; k < 128; ++k) if (active[k]) { midi.addEvent(juce::MidiMessage::noteOff(1, k), 0); active[k] = false; }
        lastP1 = -1.0;
    }
    wasPlaying = go;
}

void FredonneProcessor::pressRecord() {
    const int s = st.load();
    if (s == Idle) {
        const int want = (int)(sr * kMaxSec) + 4096;   // tampon resté à l'ancienne fréquence (prise analysée entre-temps) : remis à la taille
        if ((int)rec.size() != want) { rec.assign((size_t)want, 0.f); recMax = want; }
        preIdx = 0; std::fill(pre, pre + 4096, 0.f); recLen = 0; st.store(Armed);
    }
    else if (s == Armed) st.store(Idle);
    else if (s == Recording) { int e = Recording; st.compare_exchange_strong(e, Captured); }
}

void FredonneProcessor::clearTake() {
    if (st.load() == Analysing || st.load() == Captured) return;
    st.store(Idle); recLen = 0; trackP.reset(); std::atomic_store(&takeP, std::make_shared<const Take>()); std::atomic_store(&playNotes, std::shared_ptr<const std::vector<oa::MidiNote>>());
}

void FredonneProcessor::loadAudio(const float* mono, int n, double rate) {
    if (st.load() == Analysing) return;
    if ((int)rec.size() < n) { rec.assign((size_t)n, 0.f); recMax = n; }
    n = juce::jmin(n, recMax); std::copy(mono, mono + n, rec.begin()); recSr = rate; recLen = n; st.store(Captured);
}

void FredonneProcessor::timerCallback() {
    if (st.load() == Captured) {
        if (recLen.load() < (int)(recSr * 0.4)) { st.store(Idle); recLen = 0; }
        else { st.store(Analysing); analysisDone = false; worker.startThread(); }
    }
    if (analysisDone.exchange(false)) { st.store(Idle); dirty = true; }
    if (!dirty.load() && takeP->track && p("follow") > .5f && hostBpm.load() > 0.f && std::abs(hostBpm.load() - takeP->bpm) > 0.05) dirty = true;
    if (dirty.exchange(false)) resegment();
}

void FredonneProcessor::Worker::run() {
    auto t = std::make_shared<oa::PitchTrack>(oa::analyzePitch(owner.rec.data(), owner.recLen.load(), owner.recSr));
    owner.trackP = t; owner.analysisDone = true;
}

void FredonneProcessor::resegment() {
    if (!trackP) return;
    auto tk = std::make_shared<Take>(); tk->track = trackP; tk->bpm = effectiveBpm();
    auto sp = segParams(); sp.bpm = tk->bpm; tk->seg = oa::segment(*trackP, sp);
    auto mn = std::make_shared<std::vector<oa::MidiNote>>(); const double bps = tk->bpm / 60.0;
    for (auto& x : tk->seg.notes) mn->push_back({x.s * bps, (x.e - x.s) * bps, x.n, 0.85});
    std::atomic_store(&takeP, std::shared_ptr<const Take>(tk)); std::atomic_store(&playNotes, std::shared_ptr<const std::vector<oa::MidiNote>>(mn));
}

juce::File FredonneProcessor::writeMidiFile() {
    const auto tk = take(); if (tk->seg.notes.empty()) return {};
    static const char* asc[12] = {"C", "Cs", "D", "Eb", "E", "F", "Fs", "G", "Ab", "A", "Bb", "B"};
    const char* md[3] = {"min", "maj", "chr"};
    auto f = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("Orbe").getChildFile(
        String("Orbe Fredonne - ") + asc[tk->seg.key % 12] + " " + md[juce::jlimit(0, 2, tk->seg.mode)] + " " + String(juce::roundToInt(tk->bpm)) + " BPM.mid");
    f.getParentDirectory().createDirectory();
    const double bps = tk->bpm / 60.0; std::vector<oa::MidiNote> mn;
    for (auto& x : tk->seg.notes) mn.push_back({x.s * bps, (x.e - x.s) * bps, x.n, 0.85});
    const auto bytes = oa::writeMidi(mn, tk->bpm);
    f.replaceWithData(bytes.data(), bytes.size());
    return f;
}

// L'état garde la prise analysée (piste de hauteur : 16 octets par trame de 10 ms, soit ~200 Ko pour 2 min) et sa position dans le
// morceau : les notes sont recalculées au chargement avec les réglages sauvegardés, comme après un réglage dans l'interface.
void FredonneProcessor::getStateInformation(juce::MemoryBlock& dest) {
    auto tree = apvts.copyState();
    const auto tk = take();
    if (tk && tk->track && !tk->track->frames.empty()) {
        const auto& fr = tk->track->frames;
        static_assert(sizeof(oa::PitchFrame) == 4 * sizeof(float), "trame de hauteur : 4 flottants");
        juce::MemoryBlock mb(fr.data(), fr.size() * sizeof(oa::PitchFrame));
        tree.setProperty("takeFrames", mb.toBase64Encoding(), nullptr);
        tree.setProperty("takeMaxR", (double)tk->track->maxR, nullptr);
        tree.setProperty("takeDur", tk->track->dur, nullptr);
        tree.setProperty("takeStart", takeStart.load(), nullptr);
    }
    if (auto xml = tree.createXml()) copyXmlToBinary(*xml, dest);
}
void FredonneProcessor::setStateInformation(const void* data, int size) {
    auto xml = getXmlFromBinary(data, size);
    if (!xml || !xml->hasTagName(apvts.state.getType())) return;
    auto tree = juce::ValueTree::fromXml(*xml);
    apvts.replaceState(tree);
    if (!tree.hasProperty("takeFrames") || st.load() == Analysing || st.load() == Captured) return;
    juce::MemoryBlock mb;
    if (!mb.fromBase64Encoding(tree.getProperty("takeFrames").toString()) || mb.getSize() % sizeof(oa::PitchFrame) != 0) return;
    auto t = std::make_shared<oa::PitchTrack>();
    t->frames.resize(mb.getSize() / sizeof(oa::PitchFrame));
    std::memcpy(t->frames.data(), mb.getData(), mb.getSize());
    t->maxR = (float)(double)tree.getProperty("takeMaxR", 0.0);
    t->dur = (double)tree.getProperty("takeDur", 0.0);
    takeStart.store((double)tree.getProperty("takeStart", 0.0));
    trackP = t;
    resegment();
    dirty = false;
}

juce::AudioProcessorEditor* FredonneProcessor::createEditor() { return new FredonneEditor(*this); }
#ifndef ORBE_SUITE_SNAPSHOT
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new FredonneProcessor(); }
#endif
