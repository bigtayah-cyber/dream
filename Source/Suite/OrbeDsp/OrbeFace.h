// Socle commun des effets Orbe (Denoiser, EQ, DeEsser) : processeur de base, composants de face avant « matériel de studio »
// et aides de tracé (grille fréquentielle log, courbes de spectre). Header-only ; dépend de JUCE et de Kit.h.
#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "../../Kit/Kit.h"
#include "OrbeDsp.h"
#include "../../Kit/BypassFade.h"

namespace of {
using juce::Colour, juce::Rectangle, juce::String;
inline const Colour kLilac(0xFFCFC6FF), kIce(0xFFA9ECFF), kPeach(0xFFFFC4AB);

inline kit::Palette palette() {
    kit::Palette p;
    p.plateTop = Colour(0xFF2E2E36); p.plateBot = Colour(0xFF16161B);
    p.ink = Colour(0xFFE9E6F5); p.accent = kLilac; p.accent2 = kIce;
    p.mute = p.ink.withAlpha(0.55f); p.line = p.ink.withAlpha(0.14f);
    p.screen = Colour(0xFF0B0B10); p.screenInk = kIce; p.brush = 0.9f;
    return p;
}

// ---------------------------------------------------------------- processeur de base
inline std::unique_ptr<juce::AudioParameterFloat> fparam(const char* id, const String& name, float lo, float hi, float step, float def,
                                                         std::function<String(float)> text, float centre = 0.f, std::function<float(const String&)> parse = nullptr) {
    juce::NormalisableRange<float> r(lo, hi, step);
    if (centre > 0.f) r.setSkewForCentre(centre);
    auto a = juce::AudioParameterFloatAttributes().withStringFromValueFunction([text](float v, int) { return text(v); });
    if (parse) a = a.withValueFromStringFunction([parse](const String& s) { return parse(s); });
    return std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{id, 1}, name, r, def, a);
}
inline String fmtHz(float v) { return v >= 1000.f ? String(v / 1000.f, v >= 10000.f ? 1 : 2) + " kHz" : String(juce::roundToInt(v)) + " Hz"; }
inline float parseHz(const String& s) {
    const double v = s.getDoubleValue(); const auto t = s.toLowerCase();
    return (float)((t.contains("k") ? v * 1000.0 : v));
}
inline String fmtDb(float v) { return (v > 0.04f ? "+" : "") + String(v, 1) + " dB"; }
inline String fmtPct(float v) { return String(juce::roundToInt(v * 100.f)) + " %"; }

class FxProcessor : public juce::AudioProcessor {
public:
    FxProcessor(const String& nm, const String& stateName, juce::AudioProcessorValueTreeState::ParameterLayout&& l)
        : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true).withOutput("Output", juce::AudioChannelSet::stereo(), true)),
          apvts(*this, nullptr, stateName, std::move(l)), pname(nm) {
        bypass = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter("bypass"));
    }
    static void addBypass(juce::AudioProcessorValueTreeState::ParameterLayout& l) { l.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"bypass", 1}, "Bypass", false)); }

    bool isBusesLayoutSupported(const BusesLayout& l) const override {
        return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() &&
               (l.getMainInputChannelSet() == juce::AudioChannelSet::mono() || l.getMainInputChannelSet() == juce::AudioChannelSet::stereo());
    }
    void releaseResources() override {}
    const String getName() const override { return pname; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    bool hasEditor() const override { return true; }
    juce::AudioProcessorParameter* getBypassParameter() const override { return bypass; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const String getProgramName(int) override { return {}; }
    void changeProgramName(int, const String&) override {}

    void processBlockBypassed(juce::AudioBuffer<float>& b, juce::MidiBuffer&) override {
        if (b.getNumChannels() < 2) return;
        byp.process(b.getWritePointer(0), b.getWritePointer(1), b.getNumSamples(), getTotalNumInputChannels() >= 2);
    }
    void getStateInformation(juce::MemoryBlock& dest) override {
        beforeSave();
        if (auto xml = apvts.copyState().createXml()) copyXmlToBinary(*xml, dest);
    }
    void setStateInformation(const void* data, int size) override {
        if (auto xml = getXmlFromBinary(data, size))
            if (xml->hasTagName(apvts.state.getType())) { apvts.replaceState(juce::ValueTree::fromXml(*xml)); afterLoad(); }
    }
    float raw(const char* id) const { return apvts.getRawParameterValue(id)->load(); }

    juce::AudioProcessorValueTreeState apvts;
protected:
    virtual void beforeSave() {}
    virtual void afterLoad() {}
    // Découpe un bloc en tranches de `maxBlock` et appelle f(inL, inR|nullptr, outL, outR, n) ; renvoie faux si la configuration est inutilisable.
    // Le bypass est honoré ici (le paramètre « bypass » est déclaré : JUCE n'appelle alors jamais processBlockBypassed) :
    // sortie = entrée retardée de la latence, fondu croisé de 10 ms ; le traitement continue en interne (queues et mémoires intactes).
    template <class F> bool stereo(juce::AudioBuffer<float>& b, int maxBlock, F f) {
        if (b.getNumChannels() < 2) return false;
        const bool st = getTotalNumInputChannels() >= 2;
        float* l = b.getWritePointer(0); float* r = b.getWritePointer(1);
        fader.run(l, r, b.getNumSamples(), st, bypass != nullptr && bypass->get(),
                  [&](float* cl, float* cr, int c) { for (int p = 0; p < c; p += maxBlock) { const int k = juce::jmin(maxBlock, c - p); f(cl + p, st ? cr + p : nullptr, cl + p, cr + p, k); } });
        return true;
    }
    // À appeler dans prepareToPlay à la place de byp.init : latence déclarée + tranche maximale.
    void prepareBypass(double sampleRate, int latency, int maxBlock) { byp.init(latency); fader.prepare(sampleRate, latency, maxBlock); }
    odsp::BypassDelay byp;
    bypassfx::Fader fader;
    juce::AudioParameterBool* bypass = nullptr;
private:
    String pname;
};

// ---------------------------------------------------------------- composants de face avant
// Potard + nom gravé + afficheur de valeur (double-clic : valeur par défaut du paramètre).
class Pot : public juce::Component {
public:
    static constexpr float kW = 117.f, kH = 78.f, kKnobR = 19.f;
    Pot(juce::AudioProcessorValueTreeState& a, const String& id, const String& lab, const String& tip) : label(lab) {
        par = a.getParameter(id);
        s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        s.setMouseDragSensitivity(240);
        s.setTooltip(tip + kit::u8(" Double-clic : valeur d'origine."));
        s.onValueChange = [this] { repaint(); };
        addAndMakeVisible(s);
        att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(a, id, s);
    }
    void resized() override {
        const float u = (float)getWidth() / kW;
        s.setBounds(Rectangle<float>(kW * u / 2 - 1.5f * kKnobR * u, (30.f - 1.5f * kKnobR) * u, 3 * kKnobR * u, 3 * kKnobR * u).toNearestInt());
    }
    void paint(juce::Graphics& g) override {
        const float u = (float)getWidth() / kW;
        auto pal = kit::Palette{}; pal.ink = Colour(0xFFE9E6F5);
        kit::engraved(g, pal, label, Rectangle<float>(0, 57 * u, kW * u, 11 * u), juce::Justification::centred, 8.6f * u, 1.3f * u, true);
        auto r = Rectangle<float>(kW * u / 2 - 31 * u, 68 * u, 62 * u, 11.5f * u);
        g.setColour(juce::Colours::black.withAlpha(0.42f)); g.fillRoundedRectangle(r, 2.5f * u);
        g.setColour(juce::Colours::white.withAlpha(0.05f)); g.drawRoundedRectangle(r, 2.5f * u, 1.f);
        g.setColour(kIce.withAlpha(0.92f)); g.setFont(kit::mono(9.2f * u));
        g.drawText(par ? par->getText(par->getValue(), 0) : String(), r, juce::Justification::centred);
    }
private:
    juce::Slider s; String label;
    juce::RangedAudioParameter* par = nullptr;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};

// Interrupteur rond « IN » à voyant (allumé = effet en service ; le paramètre est « bypass »).
class Power : public juce::Button {
public:
    explicit Power(const kit::Palette& p) : juce::Button("in"), P(p) { setClickingTogglesState(true); }
    void paintButton(juce::Graphics& g, bool hover, bool down) override {
        auto r = getLocalBounds().toFloat();
        const float cx = r.getCentreX(), cy = r.getCentreY() - 5, rad = juce::jmin(r.getWidth(), r.getHeight() - 12) * 0.42f;
        const bool on = !getToggleState();
        g.setColour(juce::Colours::black.withAlpha(0.55f)); g.fillEllipse(cx - rad - 2, cy - rad, (rad + 2) * 2, (rad + 2) * 2);
        g.setGradientFill(juce::ColourGradient(Colour(0xFF6A6A72), cx - rad * 0.6f, cy - rad, Colour(0xFF17171B), cx + rad * 0.6f, cy + rad, false));
        g.fillEllipse(cx - rad, cy - rad + (down ? 1.f : 0.f), rad * 2, rad * 2);
        g.setColour(juce::Colours::white.withAlpha(hover ? 0.3f : 0.14f)); g.drawEllipse(cx - rad, cy - rad, rad * 2, rad * 2, 1.f);
        kit::led(g, cx, cy, rad * 0.28f, on, P.accent);
        g.setColour(P.ink.withAlpha(0.8f));
        kit::tracked(g, "In", Rectangle<float>(0, r.getBottom() - 11, r.getWidth(), 11), juce::Justification::centred, 1.6f, kit::fnt(juce::jmax(7.f, rad * 0.55f), true));
    }
private:
    kit::Palette P;
};

// Touche rectangulaire biseautée à voyant, liée à un paramètre booléen (ou utilisée seule via onClick).
class Key : public juce::Button {
public:
    Key(const String& text, const kit::Palette& p, Colour led = kLilac) : juce::Button(text), P(p), ledColour(led) { setClickingTogglesState(true); }
    float fill = 0.f;  // 0..1 : barre de progression au bas de la touche
    void paintButton(juce::Graphics& g, bool hover, bool down) override {
        const float u = (float)getHeight() / 26.f;
        auto r = getLocalBounds().toFloat().reduced(1.f);
        g.setColour(juce::Colours::black.withAlpha(0.55f)); g.fillRoundedRectangle(r.translated(0, 1.5f * u), 4.f * u);
        auto top = Colour(0xFF54545A), bot = Colour(0xFF1D1D21);
        if (down) std::swap(top, bot);
        if (hover) top = top.brighter(0.12f);
        g.setGradientFill(juce::ColourGradient(top, r.getX(), r.getY(), bot, r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle(r, 4.f * u);
        g.setColour(juce::Colours::white.withAlpha(0.14f)); g.drawRoundedRectangle(r, 4.f * u, 1.f);
        const bool on = getToggleState();
        if (on) { g.setColour(ledColour.withAlpha(0.14f)); g.fillRoundedRectangle(r, 4.f * u); }
        if (fill > 0.f) { g.setColour(ledColour.withAlpha(0.85f)); g.fillRoundedRectangle(r.getX() + 3.f * u, r.getBottom() - 4.f * u, (r.getWidth() - 6.f * u) * juce::jlimit(0.f, 1.f, fill), 2.4f * u, 1.2f * u); }
        const float lx = r.getX() + 12.f * u;
        kit::led(g, lx, r.getCentreY() + (down ? 0.8f : 0.f), 3.6f * u, on, ledColour);
        g.setColour(on ? juce::Colours::white : P.ink.withAlpha(0.82f));
        kit::tracked(g, getButtonText(), r.withTrimmedLeft(22.f * u).translated(0, down ? 0.8f : 0.f), juce::Justification::centredLeft, 1.2f * u, kit::fnt(9.4f * u, true));
    }
private:
    kit::Palette P; Colour ledColour;
};

// Sélecteur à segments pour un paramètre « choix ».
class Seg : public juce::Component, public juce::SettableTooltipClient {
public:
    Seg(juce::AudioProcessorValueTreeState& a, const String& id, juce::StringArray names, const String& tip, const kit::Palette& p)
        : P(p), labels(std::move(names)), par(a.getParameter(id)),
          att(*par, [this](float v) { idx = juce::roundToInt(v); repaint(); }, nullptr) {
        setTooltip(tip); att.sendInitialUpdate();
    }
    void paint(juce::Graphics& g) override {
        const float u = (float)getHeight() / 24.f;
        auto r = getLocalBounds().toFloat().reduced(0.5f);
        g.setColour(juce::Colours::black.withAlpha(0.6f)); g.fillRoundedRectangle(r, 4.f * u);
        g.setColour(juce::Colours::white.withAlpha(0.12f)); g.drawRoundedRectangle(r, 4.f * u, 1.f);
        const float w = r.getWidth() / (float)labels.size();
        for (int i = 0; i < labels.size(); ++i) {
            auto c = Rectangle<float>(r.getX() + w * (float)i, r.getY(), w, r.getHeight()).reduced(2.f * u);
            const bool on = i == idx;
            if (on) { g.setGradientFill(juce::ColourGradient(P.accent.withAlpha(0.34f), c.getX(), c.getY(), P.accent.withAlpha(0.12f), c.getX(), c.getBottom(), false)); g.fillRoundedRectangle(c, 3.f * u);
                      g.setColour(P.accent.withAlpha(0.6f)); g.drawRoundedRectangle(c, 3.f * u, 1.f); }
            g.setColour(on ? juce::Colours::white : P.ink.withAlpha(0.6f));
            kit::tracked(g, labels[i], c, juce::Justification::centred, 1.1f * u, kit::fnt(8.8f * u, true));
        }
    }
    void mouseDown(const juce::MouseEvent& e) override {
        const int i = juce::jlimit(0, labels.size() - 1, (int)(e.position.x / ((float)getWidth() / (float)labels.size())));
        att.setValueAsCompleteGesture((float)i);
    }
private:
    kit::Palette P; juce::StringArray labels; juce::RangedAudioParameter* par; juce::ParameterAttachment att; int idx = 0;
};

// Face avant : plaque brossée, vis, flancs, bloc-titre.
inline void drawFace(juce::Graphics& g, const kit::Palette& pal, const juce::Image& plate, float u, float W, float H, const String& title, const String& tagline, const char* serial) {
    using namespace kit;
    g.drawImageAt(plate, 0, 0);
    for (float y : {14.f, H - 14.f}) for (float x : {14.f, W - 14.f}) screw(g, x * u, y * u, 5.f * u, (x + y) * 0.7f);
    g.setColour(juce::Colours::black.withAlpha(0.45f)); g.fillRect(Rectangle<float>(0, 0, 3 * u, H * u)); g.fillRect(Rectangle<float>((W - 3) * u, 0, 3 * u, H * u));
    g.setColour(juce::Colours::white.withAlpha(0.07f)); g.fillRect(Rectangle<float>(3 * u, 0, u, H * u)); g.fillRect(Rectangle<float>((W - 4) * u, 0, u, H * u));
    engraved(g, pal, "dreamaws", Rectangle<float>(28 * u, 24 * u, 120 * u, 12 * u), juce::Justification::centredLeft, 8.8f * u, 2.6f * u);
    engraved(g, pal, title, Rectangle<float>(28 * u, 38 * u, 340 * u, 30 * u), juce::Justification::centredLeft, 24.f * u, 3.6f * u, true);
    g.setColour(pal.mute); g.setFont(fnt(9.6f * u));
    g.drawText(tagline, Rectangle<float>(28 * u, 70 * u, 420 * u, 13 * u), juce::Justification::centredLeft);
    g.setFont(mono(8.4f * u)); g.drawText(serial, Rectangle<float>(300 * u, 24 * u, 70 * u, 12 * u), juce::Justification::centredLeft);
    g.setColour(kPeach.withAlpha(0.9f)); g.fillRect(Rectangle<float>(28 * u, 88 * u, 28 * u, 1.6f * u));
    g.setColour(kLilac.withAlpha(0.9f)); g.fillRect(Rectangle<float>(56 * u, 88 * u, 28 * u, 1.6f * u));
    g.setColour(kIce.withAlpha(0.9f)); g.fillRect(Rectangle<float>(84 * u, 88 * u, 28 * u, 1.6f * u));
}
inline void section(juce::Graphics& g, const kit::Palette& pal, float u, const String& title, float x, float y, float w) {
    kit::engraved(g, pal, title, Rectangle<float>(x * u, y * u, (w - 4) * u, 12 * u), juce::Justification::centredLeft, 8.8f * u, 2.2f * u, true);
    g.setColour(juce::Colours::black.withAlpha(0.5f)); g.fillRect(Rectangle<float>(x * u, (y + 15) * u, w * u, 1.2f * u));
    g.setColour(juce::Colours::white.withAlpha(0.07f)); g.fillRect(Rectangle<float>(x * u, (y + 16.2f) * u, w * u, u));
}

// ---------------------------------------------------------------- aides de tracé
namespace gfx {
inline float xOf(float f, Rectangle<float> r, float f0 = 20.f, float f1 = 20000.f) { return r.getX() + r.getWidth() * std::log(f / f0) / std::log(f1 / f0); }
inline float fOf(float x, Rectangle<float> r, float f0 = 20.f, float f1 = 20000.f) { return f0 * std::exp((x - r.getX()) / r.getWidth() * std::log(f1 / f0)); }
inline float yOfDb(float d, Rectangle<float> r, float top, float bot) { return r.getBottom() - r.getHeight() * (d - bot) / (top - bot); }

inline String hzLabel(float f) { return f >= 1000.f ? String(f / 1000.f, f == 1500.f ? 1 : 0) + "k" : String((int)f); }
// Repères fréquentiels log : lignes fines + repères gravés
inline void freqGrid(juce::Graphics& g, Rectangle<float> r, float u, float f0 = 20.f, float f1 = 20000.f, std::vector<float> marks = {20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000}, bool labels = true) {
    for (float dec = 10.f; dec <= 10000.f; dec *= 10.f)
        for (int m = 1; m < 10; ++m) {
            const float f = dec * (float)m; if (f < f0 || f > f1) continue;
            const bool major = std::find(marks.begin(), marks.end(), f) != marks.end();
            g.setColour(juce::Colours::white.withAlpha(major ? 0.10f : 0.035f));
            g.drawVerticalLine(juce::roundToInt(xOf(f, r, f0, f1)), r.getY(), r.getBottom());
        }
    if (!labels) return;
    g.setFont(kit::mono(7.6f * u));
    for (float f : marks) {
        if (f < f0 || f > f1) continue;
        const float x = xOf(f, r, f0, f1);
        g.setColour(Colour(0xFFE9E6F5).withAlpha(0.5f));
        g.drawText(hzLabel(f), Rectangle<float>(x - 14 * u, r.getBottom() + 2 * u, 28 * u, 10 * u), juce::Justification::centred);
    }
}
// Courbe de spectre (dB par bin) sur un axe log ; maximum des bins par colonne, interpolation quand un bin couvre plusieurs colonnes.
inline juce::Path specPath(const float* db, int bins, float sr, Rectangle<float> r, float topDb, float botDb, bool closed, float f0 = 20.f, float f1 = 20000.f) {
    juce::Path p; const float binHz = sr / (2.f * (float)(bins - 1));
    bool first = true;
    for (float x = r.getX(); x <= r.getRight(); x += 1.f) {
        const float fa = fOf(x - 0.5f, r, f0, f1), fb = fOf(x + 0.5f, r, f0, f1);
        float v;
        const int ia = (int)std::floor(fa / binHz), ib = (int)std::floor(fb / binHz);
        if (ib <= ia) { const float t = fa / binHz - (float)ia; const int i0 = juce::jlimit(0, bins - 1, ia), i1 = juce::jlimit(0, bins - 1, ia + 1); v = db[i0] + (db[i1] - db[i0]) * t; }
        else { v = -200.f; for (int i = juce::jmax(0, ia + 1); i <= juce::jmin(bins - 1, ib); ++i) v = juce::jmax(v, db[i]); }
        const float y = juce::jlimit(r.getY(), r.getBottom(), yOfDb(v, r, topDb, botDb));
        if (first) { if (closed) { p.startNewSubPath(x, r.getBottom()); p.lineTo(x, y); } else p.startNewSubPath(x, y); first = false; }
        else p.lineTo(x, y);
    }
    if (closed) { p.lineTo(r.getRight(), r.getBottom()); p.closeSubPath(); }
    return p;
}
}  // namespace gfx
}  // namespace of
