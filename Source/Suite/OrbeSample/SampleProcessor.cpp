#include "SampleProcessor.h"
#include "SampleEditor.h"

using namespace kit;
using namespace osamp;
using juce::String;

// ---------------- thread de rendu ----------------
void RenderThread::poke() { lastChange = juce::Time::getMillisecondCounterHiRes(); dirty = true; cancel = true; }

void RenderThread::run() {
    while (!threadShouldExit()) {
        wait(25);
        if (!dirty.load() || juce::Time::getMillisecondCounterHiRes() - lastChange.load() < 110.0) continue;
        dirty = false; cancel = false; busy = true;
        auto r = owner.renderNow(tf, cancel);
        busy = false;
        if (r) owner.publish(std::move(r));
        else if (cancel.load()) dirty = true;
    }
}

// ---------------- paramètres ----------------
namespace {
struct Pre { const char* name; float semi, cut, bits, rate, crackle, wow, verb; bool rev; };
const float kNone = 99.f;
const Pre kPre[9] = {
    {"Brut", 0, 1, 16, 1, 0, 0, 0, false},
    {"Vinyle", kNone, .78f, 14, 1, .45f, .18f, 0, false},
    {"Cassette", kNone, .6f, 12, .6f, .08f, .5f, 0, false},
    {"Memphis", -2, .55f, 8, .3f, .15f, 0, 0, false},
    {"Soul acc\xC3\xA9l\xC3\xA9r\xC3\xA9""e", 5, 1, 16, 1, 0, 0, 0, false},
    {"Slowed + reverb", -3, .85f, 16, 1, 0, 0, .5f, false},
    {"Nightcore", 4, 1, 16, 1, 0, 0, 0, false},
    {"Mont\xC3\xA9""e \xC3\xA0 l'envers", kNone, 1, 16, 1, 0, 0, .6f, true},
    {"Radio", kNone, .42f, 10, .5f, 0, 0, 0, false},
};
}  // namespace
String SampleProcessor::presetName(int i) { return u8(kPre[juce::jlimit(0, 8, i)].name); }

juce::AudioProcessorValueTreeState::ParameterLayout SampleProcessor::makeLayout() {
    using juce::AudioParameterBool, juce::AudioParameterChoice, juce::AudioParameterFloat, juce::NormalisableRange, juce::ParameterID;
    using A = juce::AudioParameterFloatAttributes;
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    auto txt = [](std::function<String(float)> f) { return A().withStringFromValueFunction([f](float v, int) { return f(v); }); };
    auto pct = [](float v) { return String(juce::roundToInt(v * 100.f)) + " %"; };
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{"mode", 1}, u8("Hauteur et vitesse"), juce::StringArray{u8("Liés (vinyle)"), u8("Indépendants")}, 0));
    l.add(std::make_unique<AudioParameterFloat>(ParameterID{"semi", 1}, "Hauteur", NormalisableRange<float>(-12.f, 12.f, 1.f), 0.f,
                                                txt([](float v) { return (v > 0.f ? "+" : "") + String(juce::roundToInt(v)) + " dt"; })));
    l.add(std::make_unique<AudioParameterFloat>(ParameterID{"speed", 1}, "Vitesse", NormalisableRange<float>(50.f, 200.f, 0.1f), 100.f, txt([](float v) { return fr(v, 1) + " %"; })));
    l.add(std::make_unique<AudioParameterBool>(ParameterID{"sync", 1}, "Sync BPM hote", false));
    l.add(std::make_unique<AudioParameterFloat>(ParameterID{"cut", 1}, "Filtre", NormalisableRange<float>(0.f, 1.f), 1.f, txt([](float v) {
        if (v > 0.98f) return u8("ouvert");
        const double hz = cutHz(v);
        return hz >= 1000.0 ? fr(hz / 1000.0, 1) + " kHz" : String(juce::roundToInt(hz)) + " Hz"; })));
    l.add(std::make_unique<AudioParameterFloat>(ParameterID{"bits", 1}, "Bits", NormalisableRange<float>(4.f, 16.f, 1.f), 16.f,
                                                txt([](float v) { return v >= 15.5f ? u8("16 \xC2\xB7 net") : String(juce::roundToInt(v)) + " bits"; })));
    l.add(std::make_unique<AudioParameterFloat>(ParameterID{"rate", 1}, u8("Échantillonnage"), NormalisableRange<float>(0.f, 1.f), 1.f,
                                                txt([](float v) { return v > 0.995f ? String("44,1 k") : fr(rateHz(v) / 1000.0, 1) + " k"; })));
    l.add(std::make_unique<AudioParameterFloat>(ParameterID{"crackle", 1}, "Craquements", NormalisableRange<float>(0.f, 1.f), 0.f, txt(pct)));
    l.add(std::make_unique<AudioParameterFloat>(ParameterID{"wow", 1}, "Pleurage", NormalisableRange<float>(0.f, 1.f), 0.f, txt(pct)));
    l.add(std::make_unique<AudioParameterFloat>(ParameterID{"verb", 1}, u8("Réverbe"), NormalisableRange<float>(0.f, 1.f), 0.f, txt(pct)));
    l.add(std::make_unique<AudioParameterBool>(ParameterID{"rev", 1}, "Reverse", false));
    l.add(std::make_unique<AudioParameterBool>(ParameterID{"tape", 1}, u8("Arrêt de bande"), false));
    l.add(std::make_unique<AudioParameterBool>(ParameterID{"loop", 1}, "Boucle", true));
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{"chop", 1}, u8("Découpes"), juce::StringArray{"Aucune", "4", "8", "16", "Attaques"}, 2));
    l.add(std::make_unique<AudioParameterFloat>(ParameterID{"zoneA", 1}, "Zone debut", NormalisableRange<float>(0.f, 1.f), 0.f, txt(pct)));
    l.add(std::make_unique<AudioParameterFloat>(ParameterID{"zoneB", 1}, "Zone fin", NormalisableRange<float>(0.f, 1.f), 1.f, txt(pct)));
    l.add(std::make_unique<AudioParameterBool>(ParameterID{"follow", 1}, u8("Suivre l'hôte"), true));
    l.add(std::make_unique<AudioParameterFloat>(ParameterID{"master", 1}, "Volume", NormalisableRange<float>(0.f, 1.f), 0.9f, txt(pct)));
    return l;
}

static const char* kRenderIds[] = {"mode", "semi", "speed", "sync", "cut", "bits", "rate", "crackle", "wow", "verb", "rev", "tape", "loop", "chop", "zoneA", "zoneB"};

SampleProcessor::SampleProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "ORBESAMPLE", makeLayout()), worker(*this) {
    pMaster = apvts.getRawParameterValue("master");
    pFollow = apvts.getRawParameterValue("follow");
    for (auto id : kRenderIds) apvts.addParameterListener(id, this);
    holder.maxSeconds = 120.0;
    holder.onAnalysed = [this] { worker.poke(); };
    loadDemo();
    worker.startThread(juce::Thread::Priority::normal);
    startTimerHz(20);
}

SampleProcessor::~SampleProcessor() {
    stopTimer();
    for (auto id : kRenderIds) apvts.removeParameterListener(id, this);
    holder.onAnalysed = nullptr;
    worker.signalThreadShouldExit();
    worker.cancel = true;
    worker.stopThread(20000);
}

bool SampleProcessor::isBusesLayoutSupported(const BusesLayout& l) const { return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo(); }

void SampleProcessor::prepareToPlay(double sampleRate, int) {
    sr = sampleRate;
    cur = {}; old = {}; fadeLeft = 0; sv[0] = {}; sv[1] = {}; loopLevel = 0.f;
    out.quiesce();  // plus aucun pointeur tenu par le thread audio : les anciens rendus peuvent partir
}

void SampleProcessor::parameterChanged(const String& id, float) {
    if (!applying && !restoring && id != "chop" && id != "zoneA" && id != "zoneB" && id != "loop") presetIdx = -1;
    worker.poke();
}

void SampleProcessor::timerCallback() {
    // en synchro hôte, un changement de tempo relance le rendu
    if (p("sync") > 0.5f && p("mode") > 0.5f) {
        const double b = hostBpm.load();
        if (b > 20.0 && std::fabs(b - lastHostUsed.load()) > 0.05) worker.poke();
    }
}

// ---------------- rendu (thread de travail) ----------------
std::shared_ptr<Rendered> SampleProcessor::renderNow(Transformer& tf, const std::atomic<bool>& cancel) {
    const auto src = holder.get();
    if (!src) return nullptr;
    TParams t;
    t.mode = (int)p("mode"); t.semi = p("semi"); t.speed = p("speed"); t.sync = p("sync") > 0.5f;
    t.hostBpm = hostBpm.load(); lastHostUsed = t.hostBpm;
    t.cut = p("cut"); t.bits = p("bits"); t.rate = p("rate"); t.crackle = p("crackle"); t.wow = p("wow"); t.verb = p("verb");
    t.rev = p("rev") > 0.5f; t.tape = p("tape") > 0.5f; t.loop = p("loop") > 0.5f;
    static const int ch[5] = {0, 4, 8, 16, -1};
    t.chop = ch[juce::jlimit(0, 4, (int)p("chop"))];
    t.tp = loadedVersion.load() >= 3;   // étirement à transitoires préservés (un projet d'avant garde l'ancien WSOLA)
    t.za = p("zoneA"); t.zb = juce::jmax(t.za + 0.002f, p("zoneB"));
    const bool ok = holder.state.load() == 2;
    return tf.render(src, t, ok ? holder.bpm.load() : 0.0, ok ? holder.pc.load() : -1, holder.minor.load() != 0, &cancel);
}

// ---------------- audio ----------------
static inline void readLin(const Rendered* r, double pos, float& l, float& rr) {
    const int n = r->pcm->size();
    const int i0 = (int)pos;
    const float f = (float)(pos - i0);
    const int i1 = i0 + 1 >= n ? 0 : i0 + 1;
    l = r->pcm->ch[0][(size_t)i0] * (1.f - f) + r->pcm->ch[0][(size_t)i1] * f;
    rr = r->pcm->ch[1][(size_t)i0] * (1.f - f) + r->pcm->ch[1][(size_t)i1] * f;
}

void SampleProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals nd;
    buffer.clear();
    if (buffer.getNumChannels() < 2) return;
    const int n = buffer.getNumSamples();
    float* L = buffer.getWritePointer(0);
    float* R = buffer.getWritePointer(1);

    bool hostPlaying = false;
    double ppq = 0.0, bpm = 0.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition()) {
            if (auto b = pos->getBpm()) { bpm = *b; hostBpm.store(*b); }
            if (pos->getIsPlaying()) if (auto q = pos->getPpqPosition()) { hostPlaying = true; ppq = *q; }
        }

    // échange sans coupure : l'ancienne boucle s'efface pendant que la nouvelle entre à la même position relative
    const Rendered* nr = out.get();
    if (nr != cur.r) {
        if (cur.r && nr) {
            old = cur; fadeTotal = fadeLeft = juce::jmax(1, (int)(0.02 * sr));
            cur.pos = cur.pos / (double)cur.r->pcm->size() * (double)nr->pcm->size();
        } else cur.pos = 0;
        cur.r = nr;
    }
    const bool follow = hostPlaying && pFollow->load() > 0.5f && bpm > 0.0;
    if (cur.r) {
        const double len = (double)cur.r->pcm->size();
        if (follow) {
            const double want = std::fmod(ppq * 60.0 / bpm * cur.r->pcm->sr, len);
            double d = std::fabs(want - cur.pos);
            d = juce::jmin(d, len - d);
            if (!wasHostPlaying || d > 0.03 * cur.r->pcm->sr) cur.pos = want < 0 ? want + len : want;
        } else if (cur.pos >= len) cur.pos = 0;
    }
    wasHostPlaying = follow;
    const float loopTarget = (audition.load() || follow) && cur.r ? 1.f : 0.f;
    const float step = 1.f / (float)(0.008 * sr);

    // déclenchements : interface, puis MIDI
    auto startSlice = [&](int idx) {
        const Rendered* r = out.get();
        if (!r || idx < 0 || idx >= r->nsl) return;
        Slice& prev = sv[svCur];
        if (prev.on) prev.dec = 1.f / (float)(0.006 * sr);
        svCur ^= 1;
        sv[svCur] = {r, (double)r->a[idx], (double)r->b[idx], 1.f, 0.f, idx, true};
    };
    if (const int u = uiSlice.exchange(-1); u >= 0) startSlice(u);
    int done = 0;
    auto render = [&](int a, int b) {
        for (int i = a; i < b; ++i) {
            float l = 0, rr = 0;
            loopLevel += juce::jlimit(-step, step, loopTarget - loopLevel);
            if (loopLevel > 0.f && cur.r) {
                float x, y; readLin(cur.r, cur.pos, x, y);
                l += x * loopLevel; rr += y * loopLevel;
                cur.pos += cur.r->pcm->sr / sr;
                if (cur.pos >= (double)cur.r->pcm->size()) cur.pos -= (double)cur.r->pcm->size();
            }
            if (fadeLeft > 0 && old.r) {
                float x, y; readLin(old.r, old.pos, x, y);
                const float g = (float)fadeLeft / (float)fadeTotal;
                l += x * g * loopLevel; rr += y * g * loopLevel;
                old.pos += old.r->pcm->sr / sr;
                if (old.pos >= (double)old.r->pcm->size()) old.pos -= (double)old.r->pcm->size();
                --fadeLeft;
            }
            for (auto& s : sv) {
                if (!s.on) continue;
                if (s.pos >= s.end - 1) { s.on = false; continue; }
                float x, y; readLin(s.r, s.pos, x, y);
                const double left = s.end - s.pos;
                float g = left < 0.006 * s.r->pcm->sr ? (float)(left / (0.006 * s.r->pcm->sr)) : 1.f;
                g = juce::jmin(g, juce::jmin(1.f, (float)((s.pos - s.r->a[s.idx]) / (0.002 * s.r->pcm->sr + 1))));
                if (s.dec > 0.f) { s.fade -= s.dec; if (s.fade <= 0.f) { s.on = false; continue; } g *= s.fade; }
                l += x * g; rr += y * g;
                s.pos += s.r->pcm->sr / sr;
            }
            L[i] += l; R[i] += rr;
        }
    };
    for (const auto meta : midi) {
        const auto m = meta.getMessage();
        const int pos = juce::jlimit(0, n, meta.samplePosition);
        if (pos > done) { render(done, pos); done = pos; }
        if (m.isNoteOn()) startSlice(m.getNoteNumber() - kFirstNote);
        else if (m.isAllNotesOff() || m.isAllSoundOff()) { sv[0].on = sv[1].on = false; }
    }
    if (done < n) render(done, n);
    const float mg = pMaster->load();
    for (int i = 0; i < n; ++i) { L[i] = softClip(L[i] * mg); R[i] = softClip(R[i] * mg); }
    playPos.store(loopLevel > 0.f && cur.r ? (float)(cur.pos / (double)cur.r->pcm->size()) : -1.f);
    slicePlaying.store(sv[svCur].on ? sv[svCur].idx : -1);
    // les rendus encore lus (boucle, fondu, tranches) restent vivants tant qu'ils servent, quelle que soit leur durée
    std::uint64_t oldest = out.currentGen();
    if (cur.r) oldest = juce::jmin(oldest, cur.r->gen);
    if (fadeLeft > 0 && old.r) oldest = juce::jmin(oldest, old.r->gen);
    for (const auto& s : sv) if (s.on && s.r) oldest = juce::jmin(oldest, s.r->gen);
    out.reportInUse(oldest);
}

// ---------------- échantillon, zone, préréglages ----------------
bool SampleProcessor::loadFile(const juce::File& f) {
    bool cut = false;
    auto s = SampleHolder::loadFile(f, holder.maxSeconds, &cut);
    if (!s) return false;
    truncated = cut;
    setZone(0.f, 1.f);
    holder.set(s, f.getFileNameWithoutExtension());
    worker.poke();
    return true;
}

void SampleProcessor::loadDemo() {
    truncated = false;
    setZone(0.f, 1.f);
    holder.set(demoSample(44100.0, 92.0), u8("Démo 92 BPM"));
    worker.poke();
}

void SampleProcessor::setZone(float a, float b) {
    applying = true;
    for (auto [id, v] : {std::pair<const char*, float>{"zoneA", a}, {"zoneB", b}})
        if (auto* q = apvts.getParameter(id)) q->setValueNotifyingHost(q->convertTo0to1(v));
    applying = false;
}

void SampleProcessor::applyPreset(int i) {
    i = juce::jlimit(0, kPresets - 1, i);
    const Pre& P = kPre[i];
    applying = true;
    auto set = [&](const char* id, float v) { if (auto* q = apvts.getParameter(id)) q->setValueNotifyingHost(q->convertTo0to1(v)); };
    set("cut", P.cut); set("bits", P.bits); set("rate", P.rate); set("crackle", P.crackle); set("wow", P.wow); set("verb", P.verb);
    set("rev", P.rev ? 1.f : 0.f); set("tape", 0.f);
    if (P.semi != kNone) { set("semi", P.semi); set("mode", 0.f); set("speed", 100.f); }
    applying = false;
    presetIdx = i;
    worker.poke();
}

void SampleProcessor::getStateInformation(juce::MemoryBlock& dest) {
    juce::MemoryOutputStream mo(dest, false);
    writeState(mo, apvts, holder, loadedVersion.load());
    mo.writeInt(presetIdx.load());
}

void SampleProcessor::setStateInformation(const void* data, int size) {
    juce::MemoryInputStream mi(data, (size_t)size, false);
    restoring = true;
    int ver = 0;
    if (readState(mi, apvts, holder, &ver)) { loadedVersion = ver; presetIdx = mi.isExhausted() ? 0 : mi.readInt(); }   // version lue avant les paramètres : < 3 = ancien WSOLA
    restoring = false;
    worker.poke();
}

juce::AudioProcessorEditor* SampleProcessor::createEditor() { return new SampleEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SampleProcessor(); }
