#include "SamplerCommon.h"

using namespace kit;
using juce::Rectangle, juce::String;

namespace v3 = kit::v3;
namespace osamp {

// ---------------- échantillon + analyse ----------------
SampleHolder::SampleHolder() : juce::Thread("Orbe analyse") { startThread(juce::Thread::Priority::low); }
SampleHolder::~SampleHolder() { signalThreadShouldExit(); notify(); stopThread(15000); }

void SampleHolder::set(std::shared_ptr<const Sample> s, const String& name) {
    {
        std::lock_guard<std::mutex> l(m);
        smp = s; todo = s; nm = name;
    }
    bpm = 0.f; bpmConf = 0.f; pc = -1; minor = 0;
    state = s ? 1 : 0;
    ++revision;
    notify();
}

void SampleHolder::run() {
    while (!threadShouldExit()) {
        std::shared_ptr<const Sample> job;
        { std::lock_guard<std::mutex> l(m); job = todo; todo.reset(); }
        if (!job) { wait(-1); continue; }
        const auto t = tempo(*job);
        const auto k = detectKey(*job);
        { std::lock_guard<std::mutex> l(m); if (smp != job) continue; }  // un autre son est arrivé entre-temps
        bpm = (float)t.bpm; bpmConf = (float)t.conf; pc = k.pc; minor = k.minor ? 1 : 0; state = 2;
        ++revision;
        if (onAnalysed) onAnalysed();
    }
}

std::shared_ptr<Sample> SampleHolder::loadFile(const juce::File& f, double maxSeconds, bool* truncated) {
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> rd(fm.createReaderFor(f));
    if (!rd || rd->lengthInSamples < 64 || rd->sampleRate < 8000.0) return nullptr;
    const int n = (int)std::min<juce::int64>(rd->lengthInSamples, (juce::int64)(maxSeconds * rd->sampleRate));
    if (truncated) *truncated = rd->lengthInSamples > n;
    juce::AudioBuffer<float> b((int)std::min<unsigned>(2u, rd->numChannels), n);
    if (!rd->read(&b, 0, n, 0, true, true)) return nullptr;
    auto s = std::make_shared<Sample>();
    s->sr = rd->sampleRate;
    for (int c = 0; c < 2; ++c) {
        const float* src = b.getReadPointer(juce::jmin(c, b.getNumChannels() - 1));
        s->ch[c].assign(src, src + n);
    }
    for (int c = 0; c < 2; ++c) for (auto& v : s->ch[c]) if (!std::isfinite(v)) v = 0.f;
    return s;
}

void SampleHolder::writeTo(juce::MemoryOutputStream& mo) const {
    const auto s = get();
    mo.writeString(name());
    mo.writeInt(s ? s->size() : 0);
    if (!s) return;
    mo.writeDouble(s->sr);
    const bool st = s->ch[0] != s->ch[1];
    mo.writeByte((char)st);
    std::vector<int16_t> pcm((size_t)s->size());
    for (int c = 0; c < (st ? 2 : 1); ++c) {
        for (int i = 0; i < s->size(); ++i) pcm[(size_t)i] = (int16_t)juce::jlimit(-32767, 32767, juce::roundToInt(s->ch[c][(size_t)i] * 32767.f));
        mo.write(pcm.data(), pcm.size() * sizeof(int16_t));
    }
}

bool SampleHolder::readFrom(juce::MemoryInputStream& mi) {
    const String name = mi.readString();
    const int n = mi.readInt();
    if (n < 0 || n > 40000000) return false;
    if (n == 0) { set(nullptr, {}); return true; }
    const double sr = mi.readDouble();
    const bool st = mi.readByte() != 0;
    if (sr < 8000.0 || sr > 400000.0 || mi.getNumBytesRemaining() < (juce::int64)n * 2 * (st ? 2 : 1)) return false;
    auto s = std::make_shared<Sample>();
    s->sr = sr;
    std::vector<int16_t> pcm((size_t)n);
    for (int c = 0; c < (st ? 2 : 1); ++c) {
        mi.read(pcm.data(), (int)(pcm.size() * sizeof(int16_t)));
        s->ch[c].resize((size_t)n);
        for (int i = 0; i < n; ++i) s->ch[c][(size_t)i] = (float)pcm[(size_t)i] / 32767.f;
    }
    if (!st) s->ch[1] = s->ch[0];
    set(s, name);
    return true;
}

void writeState(juce::MemoryOutputStream& mo, juce::AudioProcessorValueTreeState& apvts, const SampleHolder& h) {
    mo.writeInt(0x4F534D50);  // « OSMP »
    mo.writeInt(1);
    if (auto xml = apvts.copyState().createXml()) mo.writeString(xml->toString(juce::XmlElement::TextFormat().singleLine())); else mo.writeString({});
    h.writeTo(mo);
}

bool readState(juce::MemoryInputStream& mi, juce::AudioProcessorValueTreeState& apvts, SampleHolder& h) {
    if (mi.readInt() != 0x4F534D50 || mi.readInt() != 1) return false;
    if (auto xml = juce::parseXML(mi.readString())) {
        auto tree = juce::ValueTree::fromXml(*xml);
        if (tree.hasType(apvts.state.getType())) apvts.replaceState(tree);
    }
    return h.readFrom(mi);
}

String noteName(int midi) { return String(noteLetter(midi)) + String(midi / 12 - 2); }
String keyName(int pc, bool minor) { return pc < 0 ? String(u8("\xE2\x80\x94")) : String(noteLetter(pc)) + (minor ? " min" : " maj"); }
String durText(double s) { return fr(s, 2) + " s"; }

// ---------------- Seg ----------------
Knob::Knob(juce::AudioProcessorValueTreeState& a, const String& id, const String& caption, const Palette& pal) : apvts(a), P(pal), cap(caption) {
    s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    s.setMouseDragSensitivity(180);
    addAndMakeVisible(s);
    bind(id);
}
void Knob::bind(const String& id) { att.reset(); att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, id, s); repaint(); }
void Knob::resized() {
    const int side = juce::jmin(getWidth(), (int)(getHeight() * 0.68f));
    s.setBounds((getWidth() - side) / 2, 0, side, side);
}
void Knob::paint(juce::Graphics& g) {
    const float h = (float)getHeight(), w = (float)getWidth();
    auto vr = Rectangle<float>(w * 0.08f, h * 0.70f, w * 0.84f, h * 0.15f);
    g.setColour(juce::Colours::black.withAlpha(0.55f));
    g.fillRoundedRectangle(vr, 3.f);
    g.setColour(juce::Colours::white.withAlpha(0.07f));
    g.drawRoundedRectangle(vr, 3.f, 1.f);
    g.setColour(P.screenInk);
    g.setFont(kit::mono(vr.getHeight() * 0.72f));
    const String ov = textOverride ? textOverride() : String();
    g.drawFittedText(ov.isNotEmpty() ? ov : s.getTextFromValue(s.getValue()), vr.toNearestInt(), juce::Justification::centred, 1, 0.8f);
    engraved(g, P, cap, Rectangle<float>(0, h * 0.87f, w, h * 0.13f), juce::Justification::centred, juce::jmax(8.f, h * 0.105f), 1.f, true);
}

// ---------------- WaveView ----------------
void WaveView::rebuild() {
    mn.clear(); mx.clear();
    if (!smp || smp->size() < 2) return;
    const int cols = juce::jmax(1, (int)inner().getWidth()), n = smp->size();
    mn.assign((size_t)cols, 0.f); mx.assign((size_t)cols, 0.f);
    for (int x = 0; x < cols; ++x) {
        const int a = (int)((juce::int64)n * x / cols), b = juce::jmax(a + 1, (int)((juce::int64)n * (x + 1) / cols)), step = juce::jmax(1, (b - a) / 256);
        float lo = 0, hi = 0;
        for (int i = a; i < b && i < n; i += step) {
            const float l = smp->ch[0][(size_t)i], r = smp->ch[1][(size_t)i];
            lo = juce::jmin(lo, l, r); hi = juce::jmax(hi, l, r);
        }
        mn[(size_t)x] = lo; mx[(size_t)x] = hi;
    }
    // enveloppe lissée (v3) : deux courbes, haut et bas
    const auto r = inner();
    const float cy = r.getCentreY(), hh = r.getHeight() * 0.5f * 0.92f;
    const v3::Trace::Opts o{cols, 0.9f, 1e9f, -2.f, 0};
    auto X = [&](float i) { return r.getX() + i * r.getWidth() / (float)cols; };
    trTop.build(mx.data(), cols, X, [&](float v) { return cy - juce::jlimit(0.f, 1.f, v) * hh; }, cy, o);
    trBot.build(mn.data(), cols, X, [&](float v) { return cy - juce::jlimit(-1.f, 0.f, v) * hh; }, cy, o);
}

void WaveView::paint3(juce::Graphics& g) {
    const auto& t = *T;
    auto well = getLocalBounds().toFloat().reduced(3.f);
    const float rad = juce::jmin(16.f, well.getHeight() * 0.12f);
    g.setGradientFill(juce::ColourGradient(t.scrTop, well.getX(), well.getY(), t.scrBot, well.getX(), well.getBottom(), false)); g.fillRoundedRectangle(well, rad);
    g.setColour(juce::Colours::white.withAlpha(0.07f)); g.drawRoundedRectangle(well.reduced(0.5f), rad, 1.f);
    auto r = inner();
    g.setColour(t.scrGrid);
    for (int i = 1; i < 4; ++i) g.fillRect(r.getX(), r.getY() + r.getHeight() * (float)i / 4.f, r.getWidth(), 1.f);
    g.setColour(juce::Colours::white.withAlpha(0.14f)); g.fillRect(r.getX(), r.getCentreY() - 0.5f, r.getWidth(), 1.f);
    if (!smp || mn.empty()) { g.setColour(t.scrMute); v3::caps(g, emptyText, r, juce::jmax(11.f, getHeight() * 0.09f), juce::Justification::centred); }
    else {
        const float cy = r.getCentreY();
        trTop.draw(g, col, 1.5f, 0.8f, 0.30f, cy);
        trBot.draw(g, col, 1.5f, 0.8f, 0.30f, r.getBottom());
    }
    if (paintOver) paintOver(g, *this);
}

void WaveView::paint(juce::Graphics& g) {
    if (v3::bridge::v3(T)) { paint3(g); return; }
    auto well = getLocalBounds().toFloat().reduced(3.f);
    screenWell(g, P, well);
    auto r = inner();
    g.setColour(juce::Colours::white.withAlpha(0.06f));
    for (int i = 1; i < 4; ++i) g.fillRect(r.getX(), r.getY() + r.getHeight() * (float)i / 4.f, r.getWidth(), 1.f);
    g.setColour(juce::Colours::white.withAlpha(0.14f));
    g.fillRect(r.getX(), r.getCentreY() - 0.5f, r.getWidth(), 1.f);
    if (!smp || mn.empty()) {
        g.setColour(P.mute);
        tracked(g, emptyText, r, juce::Justification::centred, 1.6f, fnt(juce::jmax(10.f, getHeight() * 0.09f), true));
    } else {
        const float hh = r.getHeight() * 0.5f * 0.92f, cy = r.getCentreY();
        for (size_t x = 0; x < mn.size(); ++x) {
            const float top = cy - juce::jlimit(0.f, 1.f, mx[x]) * hh, bot = cy - juce::jlimit(-1.f, 0.f, mn[x]) * hh;
            const float px = r.getX() + (float)x * r.getWidth() / (float)mn.size();
            g.setColour(col.withAlpha(0.22f));
            g.fillRect(px, top - 1.f, 1.f, bot - top + 2.f);
            g.setColour(col.withAlpha(0.88f));
            g.fillRect(px, top, 1.f, juce::jmax(1.f, bot - top));
        }
    }
    if (paintOver) paintOver(g, *this);
}

std::vector<float> miniPeaks(const Sample& s, int a, int b, int cols) {
    std::vector<float> o((size_t)cols, 0.f);
    const int n = juce::jmax(1, b - a);
    float m = 1e-6f;
    for (int x = 0; x < cols; ++x) {
        const int i0 = a + (int)((juce::int64)n * x / cols), i1 = juce::jmin(s.size(), a + (int)((juce::int64)n * (x + 1) / cols) + 1), step = juce::jmax(1, (i1 - i0) / 64);
        float p = 0;
        for (int i = i0; i < i1; i += step) p = juce::jmax(p, std::fabs(s.ch[0][(size_t)i]), std::fabs(s.ch[1][(size_t)i]));
        o[(size_t)x] = p; m = juce::jmax(m, p);
    }
    for (auto& v : o) v /= m;
    return o;
}

void paintBrand(juce::Graphics& g, const Palette& p, Rectangle<float> r, const String& title, const String& sub) {
    engraved(g, p, title, r.removeFromTop(r.getHeight() * 0.62f), juce::Justification::centredLeft, r.getHeight() * 0.5f, 3.2f, true);
    g.setColour(p.mute);
    tracked(g, sub, r, juce::Justification::centredLeft, 2.2f, fnt(juce::jmax(8.f, r.getHeight() * 0.8f), true));
}

}  // namespace osamp
