#include "PadsProcessor.h"
#include "PadsEditor.h"

using namespace kit;
using namespace osamp;
using juce::String;

static const char* kPN[11] = {"pitch", "vol", "pan", "cut", "att", "rel", "crush", "rv", "dl", "rev", "grp"};
const char* const* PadsProcessor::padParamNames() { return kPN; }

juce::AudioProcessorValueTreeState::ParameterLayout PadsProcessor::makeLayout() {
    using juce::AudioParameterBool, juce::AudioParameterChoice, juce::AudioParameterFloat, juce::NormalisableRange, juce::ParameterID;
    using A = juce::AudioParameterFloatAttributes;
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    auto txt = [](std::function<String(float)> f) { return A().withStringFromValueFunction([f](float v, int) { return f(v); }); };
    auto pct = [](float v) { return String(juce::roundToInt(v * 100.f)) + " %"; };
    for (int i = 0; i < 16; ++i) {
        const String pre = "p" + String(i) + "_", nm = "Pad " + String(i + 1) + " ";
        auto id = [&](int k) { return ParameterID{pre + kPN[k], 1}; };
        l.add(std::make_unique<AudioParameterFloat>(id(0), nm + "Hauteur", NormalisableRange<float>(-24.f, 24.f, 0.01f), 0.f,
                                                    txt([](float v) { return (v > 0.f ? "+" : "") + fr(v, 1) + " dt"; })));
        l.add(std::make_unique<AudioParameterFloat>(id(1), nm + "Volume", NormalisableRange<float>(-24.f, 6.f, 0.1f), 0.f, txt([](float v) { return fr(v, 1) + " dB"; })));
        l.add(std::make_unique<AudioParameterFloat>(id(2), nm + "Pan", NormalisableRange<float>(-100.f, 100.f, 1.f), 0.f,
                                                    txt([](float v) { return v == 0.f ? String("C") : (v < 0.f ? "G " : "D ") + String(juce::roundToInt(std::fabs(v))); })));
        l.add(std::make_unique<AudioParameterFloat>(id(3), nm + "Filtre", NormalisableRange<float>(0.f, 1.f), 1.f, txt([](float v) {
            if (v > 0.98f) return u8("ouvert");
            const double hz = osamp::cutHz(v);
            return hz >= 1000.0 ? fr(hz / 1000.0, 1) + " kHz" : String(juce::roundToInt(hz)) + " Hz"; })));
        l.add(std::make_unique<AudioParameterFloat>(id(4), nm + "Attaque", NormalisableRange<float>(0.f, 500.f, 1.f, 0.4f), 0.f, txt([](float v) { return String(juce::roundToInt(v)) + " ms"; })));
        l.add(std::make_unique<AudioParameterFloat>(id(5), u8("Pad ") + String(i + 1) + u8(" Relâche"), NormalisableRange<float>(0.f, 2000.f, 1.f, 0.35f), 30.f,
                                                    txt([](float v) { return String(juce::roundToInt(v)) + " ms"; })));
        l.add(std::make_unique<AudioParameterFloat>(id(6), nm + "Bits", NormalisableRange<float>(0.f, 1.f), 0.f,
                                                    txt([](float v) { return v < 0.001f ? String("net") : String(juce::jmax(3, juce::roundToInt(13.f - v * 10.f))) + " bits"; })));
        l.add(std::make_unique<AudioParameterFloat>(id(7), nm + "Reverb", NormalisableRange<float>(0.f, 1.f), 0.f, txt(pct)));
        l.add(std::make_unique<AudioParameterFloat>(id(8), nm + "Delay", NormalisableRange<float>(0.f, 1.f), 0.f, txt(pct)));
        l.add(std::make_unique<AudioParameterBool>(id(9), nm + "Reverse", false));
        l.add(std::make_unique<AudioParameterChoice>(id(10), u8("Pad ") + String(i + 1) + u8(" Groupe de coupure"), juce::StringArray{"Non", "A", "B", "C"}, 0));
    }
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{"mode", 1}, u8("Découpe"), juce::StringArray{"Transitoires", "Grille", "Manuel"}, 0));
    l.add(std::make_unique<AudioParameterFloat>(ParameterID{"sens", 1}, u8("Sensibilité"), NormalisableRange<float>(0.f, 1.f), 0.55f, txt(pct)));
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{"div", 1}, "Grille", juce::StringArray{"4", "8", "16"}, 2));
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{"play", 1}, "Lecture", juce::StringArray{"Un coup", "Tenue"}, 0));
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{"dlt", 1}, "Delay temps", juce::StringArray{"1/16", "1/8", "1/8 pointee", "1/4"}, 2));
    l.add(std::make_unique<AudioParameterFloat>(ParameterID{"master", 1}, "Master", NormalisableRange<float>(0.f, 1.f), 0.85f, txt(pct)));
    return l;
}

PadsProcessor::PadsProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "ORBEPADS", makeLayout()) {
    for (int i = 0; i < 16; ++i) {
        for (int k = 0; k < 11; ++k) pp[i][k] = apvts.getRawParameterValue("p" + String(i) + "_" + kPN[k]);
        padPos[i] = -1.f; uiOn[i] = 0; uiOff[i] = 0;
    }
    pPlay = apvts.getRawParameterValue("play"); pDlt = apvts.getRawParameterValue("dlt"); pMaster = apvts.getRawParameterValue("master");
    for (auto id : {"mode", "sens", "div"}) apvts.addParameterListener(id, this);
    holder.maxSeconds = 60.0;
    loadDemo();
    startTimerHz(30);
}

PadsProcessor::~PadsProcessor() {
    stopTimer();
    for (auto id : {"mode", "sens", "div"}) apvts.removeParameterListener(id, this);
}

bool PadsProcessor::isBusesLayoutSupported(const BusesLayout& l) const { return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo(); }

void PadsProcessor::prepareToPlay(double sampleRate, int) { sr = sampleRate; engine.prepare(sampleRate); }

osamp::PadParams PadsProcessor::read(int i) const {
    osamp::PadParams q;
    q.pitch = pp[i][0]->load(); q.volDb = pp[i][1]->load(); q.pan = pp[i][2]->load(); q.cut = pp[i][3]->load(); q.attMs = pp[i][4]->load();
    q.relMs = pp[i][5]->load(); q.crush = pp[i][6]->load(); q.rv = pp[i][7]->load(); q.dl = pp[i][8]->load();
    q.rev = pp[i][9]->load() > 0.5f; q.grp = (int)pp[i][10]->load();
    return q;
}

// ---------------- audio ----------------
void PadsProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals nd;
    buffer.clear();
    if (buffer.getNumChannels() < 2) return;
    const int n = buffer.getNumSamples();
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm()) { engine.setTempo(*b); hostBpm.store((float)*b); }
    static const float kBeats[4] = {0.25f, 0.5f, 0.75f, 1.f};
    engine.setDelayBeats(kBeats[juce::jlimit(0, 3, (int)pDlt->load())]);
    const osamp::Bank* bk = bank.get();
    const bool gate = pPlay->load() > 0.5f;
    float* L = buffer.getWritePointer(0);
    float* R = buffer.getWritePointer(1);
    auto trig = [&](int pad, float vel) { if (pad >= 0 && pad < 16) engine.trigger(bk, pad, vel, read(pad), gate); };
    for (int i = 0; i < 16; ++i) {
        if (const int v = uiOn[i].exchange(0)) trig(i, (float)v / 127.f);
        if (uiOff[i].exchange(0)) engine.release(i);
    }
    int cur = 0;
    for (const auto meta : midi) {
        const auto m = meta.getMessage();
        const int pos = juce::jlimit(0, n, meta.samplePosition);
        if (pos > cur) { engine.render(L + cur, R + cur, pos - cur); cur = pos; }
        if (m.isNoteOn()) trig(m.getNoteNumber() - kFirstNote, juce::jmax(0.05f, m.getFloatVelocity()));
        else if (m.isNoteOff()) { const int pad = m.getNoteNumber() - kFirstNote; if (pad >= 0 && pad < 16) engine.release(pad); }
        else if (m.isAllNotesOff() || m.isAllSoundOff()) engine.allOff();
    }
    if (cur < n) engine.render(L + cur, R + cur, n - cur);
    buffer.applyGain(pMaster->load());
    for (int i = 0; i < 16; ++i) padPos[i].store(engine.position(i));
}

// ---------------- échantillon et repères ----------------
bool PadsProcessor::loadFile(const juce::File& f) {
    bool cut = false;
    auto s = osamp::SampleHolder::loadFile(f, holder.maxSeconds, &cut);
    if (!s) return false;
    truncated = cut;
    holder.set(s, f.getFileNameWithoutExtension());
    rechop();
    return true;
}

void PadsProcessor::loadDemo() {
    truncated = false;
    holder.set(osamp::demoSample(44100.0, 92.0), u8("Démo 92 BPM"));
    rechop();
}

void PadsProcessor::rechop() {
    const auto s = holder.get();
    chopDirty = false;
    {
        std::lock_guard<std::mutex> l(markLock);
        const int mode = (int)p("mode");
        if (!s) marks = {0};
        else if (mode == 0) {
            if (monoFor != s.get()) { monoCache = osamp::mono(*s); monoKeep = s; monoFor = s.get(); }
            marks.clear();
            for (double t : osamp::onsets(monoCache, s->sr, p("sens"), 16)) marks.push_back(juce::jlimit(0, s->size() - 1, (int)(t * s->sr)));
        } else if (mode == 1) {
            static const int d[3] = {4, 8, 16};
            const int div = d[juce::jlimit(0, 2, (int)p("div"))];
            marks.clear();
            for (int i = 0; i < div; ++i) marks.push_back((int)((juce::int64)s->size() * i / div));
        }
        if (marks.empty()) marks = {0};
        std::sort(marks.begin(), marks.end());
        marks[0] = 0;
    }
    publishBank();
}

void PadsProcessor::publishBank() {
    const auto s = holder.get();
    auto b = std::make_shared<osamp::Bank>();
    b->pcm = s;
    if (s) {
        std::lock_guard<std::mutex> l(markLock);
        b->nsl = juce::jmin(16, (int)marks.size());
        for (int i = 0; i < b->nsl; ++i) { b->a[i] = marks[(size_t)i]; b->b[i] = i + 1 < (int)marks.size() ? marks[(size_t)i + 1] : s->size(); }
    }
    bank.publish(std::move(b));
}

void PadsProcessor::timerCallback() {
    if (chopDirty.exchange(false)) rechop();
    else if (bankDirty.exchange(false)) publishBank();
}

static void toManual(juce::AudioProcessorValueTreeState& a) {
    if (auto* m = a.getParameter("mode")) if (a.getRawParameterValue("mode")->load() != 2.f) m->setValueNotifyingHost(m->convertTo0to1(2.f));
}
void PadsProcessor::addMark(int frame) {
    const auto s = holder.get();
    if (!s) return;
    toManual(apvts);
    const int gap = (int)(0.02 * s->sr);
    std::lock_guard<std::mutex> l(markLock);
    if (marks.size() >= 16) return;
    for (int m : marks) if (std::abs(m - frame) < gap) return;
    marks.push_back(juce::jlimit(gap, s->size() - gap, frame));
    std::sort(marks.begin(), marks.end());
    bankDirty = true;
}
void PadsProcessor::moveMark(int idx, int frame) {
    const auto s = holder.get();
    if (!s) return;
    toManual(apvts);
    const int gap = (int)(0.01 * s->sr);
    std::lock_guard<std::mutex> l(markLock);
    if (idx <= 0 || idx >= (int)marks.size()) return;
    const int hi = idx + 1 < (int)marks.size() ? marks[(size_t)idx + 1] - gap : s->size() - gap;
    marks[(size_t)idx] = juce::jlimit(marks[(size_t)idx - 1] + gap, juce::jmax(marks[(size_t)idx - 1] + gap, hi), frame);
    bankDirty = true;
}
void PadsProcessor::removeMark(int idx) {
    toManual(apvts);
    std::lock_guard<std::mutex> l(markLock);
    if (idx <= 0 || idx >= (int)marks.size()) return;
    marks.erase(marks.begin() + idx);
    bankDirty = true;
}
void PadsProcessor::clearMarks() {
    toManual(apvts);
    std::lock_guard<std::mutex> l(markLock);
    marks = {0};
    bankDirty = true;
}

// ---------------- état ----------------
void PadsProcessor::getStateInformation(juce::MemoryBlock& dest) {
    juce::MemoryOutputStream mo(dest, false);
    osamp::writeState(mo, apvts, holder);
    const auto m = marksCopy();
    mo.writeInt((int)m.size());
    for (int v : m) mo.writeInt(v);
    mo.writeInt(sel.load());
}

void PadsProcessor::setStateInformation(const void* data, int size) {
    juce::MemoryInputStream mi(data, (size_t)size, false);
    if (!osamp::readState(mi, apvts, holder)) return;
    const int nm = mi.isExhausted() ? 0 : mi.readInt();
    if (nm > 0 && nm <= 16) {
        std::vector<int> m;
        for (int i = 0; i < nm; ++i) m.push_back(mi.readInt());
        if (!mi.isExhausted()) sel = juce::jlimit(0, 15, mi.readInt());
        const auto s = holder.get();
        const int total = s ? s->size() : 0;
        {
            std::lock_guard<std::mutex> l(markLock);
            marks = m;
            for (auto& v : marks) v = juce::jlimit(0, juce::jmax(0, total - 1), v);
            std::sort(marks.begin(), marks.end());
            marks[0] = 0;
        }
        chopDirty = false;
        publishBank();
        chopDirty = false;
    } else rechop();
}

juce::AudioProcessorEditor* PadsProcessor::createEditor() { return new PadsEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PadsProcessor(); }
