#include "OrbeBeatProcessor.h"
#include "../../Kit/StateVersion.h"
#include "OrbeBeatEditor.h"

using juce::String;
using namespace obeat;

namespace {
const float kVolDefault[kLanes] = {.9f, .8f, .75f, .55f, .5f, .6f, .6f, .85f};
// Carte GM : -1 = pas une note de batterie
int gmLane(int n) {
    switch (n) {
        case 35: case 36: return K;
        case 38: case 40: return S;
        case 39: return C;
        case 42: case 44: return H;
        case 46: return O;
        case 37: return R;
        case 41: case 43: case 45: case 47: case 48: case 50: return P;
        default: return -1;
    }
}
}  // namespace

juce::AudioProcessorValueTreeState::ParameterLayout OrbeBeatProcessor::makeLayout() {
    using juce::AudioParameterBool, juce::AudioParameterChoice, juce::AudioParameterFloat, juce::NormalisableRange, juce::ParameterID;
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    juce::StringArray kn, kk{"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    for (int i = 0; i < kKits; ++i) kn.add(kits()[i].label);
    auto pct = [](float v, int) { return String(juce::roundToInt(v * 100.f)) + " %"; };
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{"kit", 1}, "Kit", kn, 0));
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{"pat", 1}, "Pattern", juce::StringArray{"A", "B", "C", "D"}, 0));
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{"steps", 1}, "Pas", juce::StringArray{"16", "32"}, 0));
    l.add(std::make_unique<AudioParameterFloat>(ParameterID{"swing", 1}, "Swing", NormalisableRange<float>(0.f, 0.5f), 0.f, juce::AudioParameterFloatAttributes().withStringFromValueFunction([](float v, int) { return String(juce::roundToInt(v * 200.f)) + " %"; })));
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{"chain", 1}, kit::u8("Enchaînement"), juce::StringArray{"Aucun", "A B", "A B C D"}, 0));
    l.add(std::make_unique<AudioParameterBool>(ParameterID{"seq", 1}, kit::u8("Séquenceur"), true));
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{"midimap", 1}, kit::u8("Entrée MIDI"), juce::StringArray{"GM + 808", kit::u8("Canal 10 = batterie")}, 0));
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{"key", 1}, "Tonique 808", kk, 9));
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{"mode", 1}, "Gamme 808", juce::StringArray{"Mineur", "Majeur"}, 0));
    l.add(std::make_unique<AudioParameterFloat>(ParameterID{"master", 1}, "Master", NormalisableRange<float>(0.f, 1.f), 0.85f, juce::AudioParameterFloatAttributes().withStringFromValueFunction(pct)));
    for (int i = 0; i < kLanes; ++i) {
        const String n = kit::u8(lanes()[i].label);
        l.add(std::make_unique<AudioParameterFloat>(ParameterID{"vol" + String(i), 1}, n + " volume", NormalisableRange<float>(0.f, 1.f), kVolDefault[i], juce::AudioParameterFloatAttributes().withStringFromValueFunction(pct)));
        l.add(std::make_unique<AudioParameterBool>(ParameterID{"mute" + String(i), 1}, n + " muet", false));
        l.add(std::make_unique<AudioParameterBool>(ParameterID{"solo" + String(i), 1}, n + " solo", false));
    }
    l.add(kit::v3::notes::makeAccidParam());   // ajouté en dernier : écriture dièses / bémols (noms seulement)
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{"outs", 1}, "Sorties", juce::StringArray{"Mix seul", "Mix + sorties", "Sorties seules"}, 0));
    return l;
}

OrbeBeatProcessor::OrbeBeatProcessor()
    : AudioProcessor(kit::aux::withAux(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true), kLanes, "Piste")),
      apvts(*this, nullptr, "ORBEBEAT", makeLayout()) {
    for (auto& c : cells) c.store(0);
    for (auto& f : flash) f.store(0.f);
    for (auto& r : ring) r.store(0);
    // motif de départ : le générateur du kit Trap, graine fixe (le même démarrage à chaque instance)
    Rng rng(20260101u);
    Pattern pt; generate(pt, 0, 16, .5f, rng);
    writePattern(0, pt);
}

bool OrbeBeatProcessor::isBusesLayoutSupported(const BusesLayout& l) const { return kit::aux::layoutOk(l); }

void OrbeBeatProcessor::prepareToPlay(double sampleRate, int maxBlock) {
    sr = sampleRate;
    lastKit = (int)p("kit");
    engine.setKit(lastKit);
    engine.prepare(sampleRate);
    mono.assign((size_t)juce::jmax(512, maxBlock), 0.f);
    for (auto& s : stemBuf) s.assign((size_t)juce::jmax(512, maxBlock), 0.f);
    lastValid = false; internalPpq = 0.0; wasAudition = false;
}

bool OrbeBeatProcessor::audible(int lane) const {
    bool anySolo = false;
    for (int i = 0; i < kLanes; ++i) anySolo |= p(("solo" + String(i)).toRawUTF8()) > 0.5f;
    return p(("mute" + String(lane)).toRawUTF8()) < 0.5f && (!anySolo || p(("solo" + String(lane)).toRawUTF8()) > 0.5f);
}

int OrbeBeatProcessor::chainPattern(long long bar) const {
    const int c = (int)p("chain");
    return c == 0 ? currentPattern() : (int)(bar % (c == 1 ? 2 : 4));
}

// ---------------- audio ----------------

void OrbeBeatProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals nd;
    const int n = buffer.getNumSamples();
    buffer.clear();
    if (buffer.getNumChannels() < 2) return;
    if ((int)mono.size() < n) { mono.resize((size_t)n); for (auto& s : stemBuf) s.resize((size_t)n); }   // hôte qui dépasse la taille annoncée
    if (const int k = (int)p("kit"); k != lastKit) { lastKit = k; engine.setKit(k); }
    engine.setLevel(p("master"));

    float vol[kLanes]; bool aud[kLanes], anySolo = false;
    for (int i = 0; i < kLanes; ++i) { vol[i] = p(("vol" + String(i)).toRawUTF8()); anySolo |= p(("solo" + String(i)).toRawUTF8()) > 0.5f; }
    for (int i = 0; i < kLanes; ++i) aud[i] = p(("mute" + String(i)).toRawUTF8()) < 0.5f && (!anySolo || p(("solo" + String(i)).toRawUTF8()) > 0.5f);

    nev = 0;
    auto push = [&](int off, int lane, int note, float v, bool rel) { if (nev < (int)evs.size()) evs[(size_t)nev++] = {juce::jlimit(0, juce::jmax(0, n - 1), off), lane, note, v, rel}; };

    // 1. MIDI de l'hôte
    const bool strict = p("midimap") > 0.5f;
    for (const auto meta : midi) {
        const auto m = meta.getMessage();
        const int note = m.getNoteNumber();
        int lane = -1;
        if (m.isNoteOnOrOff()) {
            const bool drumCh = m.getChannel() == 10;
            lane = drumCh ? gmLane(note) : (strict ? B : (gmLane(note) >= 0 ? gmLane(note) : B));
            if (drumCh && lane < 0) continue;
        }
        if (m.isNoteOn() && lane >= 0) { if (aud[lane]) push(meta.samplePosition, lane, note, m.getFloatVelocity() * vol[lane], false); }
        else if (m.isNoteOff() && lane == B) push(meta.samplePosition, B, note, 0.f, true);
        else if (m.isAllNotesOff() || m.isAllSoundOff()) push(meta.samplePosition, B, -1, 0.f, true);
    }
    // aperçus de l'interface
    for (uint32_t r = ringR.load(); r != ringW.load(); ++r) {
        const uint32_t w = ring[r % ring.size()].load();
        const int lane = (int)(w & 15);
        push(0, lane, (int)((w >> 4) & 255), (float)((w >> 12) & 255) / 255.f, false);
        ringR.store(r + 1);
    }

    // 2. séquenceur sur le transport de l'hôte (ou l'horloge interne)
    bool hostPlaying = false;
    double hostPpq = 0.0, bpm = (double)obeat::kits()[lastKit].bpm;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition()) {
            if (auto b = pos->getBpm()) { bpm = *b; hostBpm.store((float)*b); }
            if (pos->getIsPlaying()) if (auto q = pos->getPpqPosition()) { hostPlaying = true; hostPpq = *q; }
        }
    if (bpm < 20.0 || bpm > 400.0) bpm = 120.0;
    const bool useInternal = !hostPlaying && audition.load();
    const bool running = (hostPlaying || useInternal) && p("seq") > 0.5f;
    const double perSample = bpm / 60.0 / sr;
    float showStep = -1.f; int showPat = currentPattern();
    if (useInternal && !wasAudition) internalPpq = 0.0;
    wasAudition = useInternal;
    if (running) {
        const double want = hostPlaying ? hostPpq : internalPpq;
        const double ppq0 = (lastValid && std::fabs(want - lastEnd) < 1e-3) ? lastEnd : want;
        const double ppq1 = ppq0 + (double)n * perSample;
        const int st = steps();
        const float sw = p("swing");
        auto at = [&](long long s, int li) -> Cell { const long long bar = s / st; return cell(chainPattern(bar), li, (int)(s % st)); };
        scan(ppq0, ppq1, sw, at, [&](double t, int li, Cell c, int k) {
            if (!aud[li]) return;
            const int note = li == B ? noteOfDegree(degOf(c)) : lanes()[li].midi;
            push((int)((t - ppq0) / perSample), li, note, velOf(c) * vol[li] * (k ? .75f : 1.f), false);
        });
        lastEnd = ppq1; lastValid = true;
        const long long gs = (long long)std::floor(std::max(0.0, ppq0) * 4.0);
        showStep = (float)(gs % st) + (float)(std::max(0.0, ppq0) * 4.0 - (double)gs);
        showPat = chainPattern(gs / st);
        if (useInternal) internalPpq = ppq1;
    } else lastValid = false;

    // 3. tri stable par position puis rendu par segments
    for (int i = 1; i < nev; ++i) { auto e = evs[(size_t)i]; int j = i - 1; while (j >= 0 && evs[(size_t)j].off > e.off) { evs[(size_t)j + 1] = evs[(size_t)j]; --j; } evs[(size_t)j + 1] = e; }
    // sorties auxiliaires : une piste par bus stéréo activé par l'hôte (mono dupliqué en G et D)
    const int outs = (int)p("outs");
    const auto tp = kit::aux::taps(*this, buffer, kLanes);
    float* stems[kLanes] = {};
    bool useStems = false;
    if (outs > 0 && tp.any) for (int k = 0; k < kLanes; ++k) if (tp.l[k]) { stems[k] = stemBuf[k].data(); useStems = true; }
    auto renderSeg = [&](int from, int len) {
        float* sp[kLanes];
        for (int k = 0; k < kLanes; ++k) sp[k] = stems[k] ? stems[k] + from : nullptr;
        engine.render(mono.data() + from, len, useStems ? sp : nullptr);
    };
    int pos = 0;
    for (int i = 0; i < nev; ++i) {
        const auto& e = evs[(size_t)i];
        if (e.off > pos) { renderSeg(pos, e.off - pos); pos = e.off; }
        if (e.release) engine.release808(e.note);
        else { engine.trigger(e.lane, e.v, e.note); flash[e.lane].store(juce::jmin(1.f, e.v)); }
    }
    if (pos < n) renderSeg(pos, n - pos);

    float pk = 0.f;
    for (int i = 0; i < n; ++i) pk = juce::jmax(pk, std::fabs(mono[(size_t)i]));
    if (outs == 2 && tp.any) { buffer.clear(0, 0, n); buffer.clear(1, 0, n); }   // sorties seules : le bus principal se tait
    else { buffer.copyFrom(0, 0, mono.data(), n); buffer.copyFrom(1, 0, mono.data(), n); }
    if (useStems) for (int k = 0; k < kLanes; ++k) if (stems[k]) { std::copy(stems[k], stems[k] + n, tp.l[k]); std::copy(stems[k], stems[k] + n, tp.r[k]); }
    meter.store(pk);
    playStep.store(showStep); playPat.store(showPat);
}

// ---------------- motifs ----------------

Pattern OrbeBeatProcessor::readPattern(int pat) const {
    Pattern pt;
    for (int l = 0; l < kLanes; ++l) for (int s = 0; s < kMaxSteps; ++s) pt.c[l][s] = cell(pat, l, s);
    return pt;
}
void OrbeBeatProcessor::writePattern(int pat, const Pattern& pt) {
    for (int l = 0; l < kLanes; ++l) for (int s = 0; s < kMaxSteps; ++s) setCell(pat, l, s, pt.c[l][s]);
}
void OrbeBeatProcessor::generateCurrent() {
    Rng rng((uint32_t)juce::Random::getSystemRandom().nextInt());
    Pattern pt; generate(pt, (int)p("kit"), steps(), .5f, rng);
    writePattern(currentPattern(), pt);
}
int OrbeBeatProcessor::varyCurrent() {
    Rng rng((uint32_t)juce::Random::getSystemRandom().nextInt());
    const int src = currentPattern(), dst = (src + 1) % kPats;
    Pattern out; vary(readPattern(src), out, rng);
    writePattern(dst, out);
    if (auto* q = apvts.getParameter("pat")) q->setValueNotifyingHost(q->convertTo0to1((float)dst));
    return dst;
}
void OrbeBeatProcessor::clearCurrent() { writePattern(currentPattern(), Pattern{}); }

void OrbeBeatProcessor::preview(int lane, float v, int note) {
    const uint32_t w = (uint32_t)lane | ((uint32_t)note << 4) | ((uint32_t)juce::jlimit(1, 255, juce::roundToInt(v * 255.f)) << 12);
    const uint32_t wi = ringW.load();
    if (wi - ringR.load() >= ring.size()) return;
    ring[wi % ring.size()].store(w);
    ringW.store(wi + 1);
}

juce::File OrbeBeatProcessor::writeMidiFile() {
    const int c = (int)p("chain"), st = steps();
    std::vector<Pattern> pats;
    const int n = c == 0 ? 1 : c == 1 ? 2 : 4;
    for (int i = 0; i < n; ++i) pats.push_back(readPattern(c == 0 ? currentPattern() : i));
    std::vector<const Pattern*> list;
    const int reps = c == 0 ? 64 / st : 1;
    for (int r = 0; r < reps; ++r) for (auto& q : pats) list.push_back(&q);
    const auto bytes = midiFile(list, st, p("swing"), tempo(), (int)p("key"), (int)p("mode"));
    String name = "Orbe Beat " + String(kits()[(int)p("kit")].label) + (c == 0 ? String(" ") + "ABCD"[currentPattern()] : c == 1 ? " AB" : " ABCD") + ".mid";
    auto f = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("Orbe").getChildFile(name);
    f.getParentDirectory().createDirectory();
    f.replaceWithData(bytes.data(), bytes.size());
    return f;
}

// ---------------- état ----------------

void OrbeBeatProcessor::getStateInformation(juce::MemoryBlock& dest) {
    auto tree = apvts.copyState();
    kit::stampState(tree);
    String hex;
    hex.preallocateBytes(cells.size() * 4 + 8);
    for (auto& c : cells) hex << String::toHexString((int)c.load()).paddedLeft('0', 4);
    tree.setProperty("cells", hex, nullptr);
    if (auto xml = tree.createXml()) copyXmlToBinary(*xml, dest);
}
void OrbeBeatProcessor::setStateInformation(const void* data, int size) {
    auto xml = getXmlFromBinary(data, size);
    if (!xml || !xml->hasTagName(apvts.state.getType())) return;
    auto tree = juce::ValueTree::fromXml(*xml);
    apvts.replaceState(tree);
    const String hex = tree.getProperty("cells").toString();
    if (hex.length() == (int)cells.size() * 4)
        for (size_t i = 0; i < cells.size(); ++i) cells[i].store((Cell)(hex.substring((int)i * 4, (int)i * 4 + 4).getHexValue32() & 0x0FFF));
}

juce::AudioProcessorEditor* OrbeBeatProcessor::createEditor() { return new OrbeBeatEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new OrbeBeatProcessor(); }
