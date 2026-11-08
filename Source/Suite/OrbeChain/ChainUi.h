// Orbe Chain : couche JUCE commune (hôte audio + composants de face avant). Identité Orbe : graphite, lilas, glace, pêche.
#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>
#include "../../Kit/Kit.h"
#include "Looks.h"
#include "../../Kit/BypassFade.h"

namespace oc {

// Pont entre l'hôte et le moteur : latence, bypass compensé, compteurs pour l'interface.
struct Host {
    Chain chain; BypassDelay byp; bypassfx::Fader fader;
    std::atomic<float> peak{0.f}, level{0.f}, reduction{0.f}, shift{0.f};
    std::atomic<int> keyPc{-1}, keyScale{1};
    int prepare(double sr, int block) { chain.prepare(sr); byp.init(chain.latency()); fader.prepare(sr, chain.latency(), juce::jmax(4096, block)); return chain.latency(); }
    void bypassed(juce::AudioBuffer<float>& b, bool stereoIn) { if (b.getNumChannels() >= 2) byp.process(b.getWritePointer(0), b.getWritePointer(1), b.getNumSamples(), stereoIn); }
    // bypass : sortie = entrée retardée de la latence, fondu de 10 ms ; la chaîne continue de tourner (queues de reverb/delay intactes).
    void run(juce::AudioBuffer<float>& b, juce::AudioPlayHead* ph, const S& s, bool stereoIn, bool autoLevel, bool bypass = false) {
        if (b.getNumChannels() < 2) return;
        if (ph) if (auto pos = ph->getPosition()) if (auto bpm = pos->getBpm()) chain.setBpm(*bpm);
        chain.setAutoLevel(autoLevel); chain.set(s);
        float* l = b.getWritePointer(0); float* r = b.getWritePointer(1);
        fader.run(l, r, b.getNumSamples(), stereoIn, bypass, [&](float* cl, float* cr, int c) { chain.process(cl, stereoIn ? cr : nullptr, cl, cr, c); });
        peak.store(chain.outPeak()); level.store(std::sqrt(chain.outMeanSquare())); reduction.store(chain.gainReduction()); shift.store(chain.shiftSemis());
        keyPc.store(chain.keyKnown() ? chain.detectedKey() : -1); keyScale.store(chain.detectedScale());
    }
};

namespace ui {
using juce::Colour;
inline const Colour kLilac(0xFFCFC6FF), kIce(0xFFA9ECFF), kPeach(0xFFFFC4AB);
inline const char* const kNotes[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

inline kit::Palette palette(Colour accent = kLilac) {
    kit::Palette p;
    p.plateTop = Colour(0xFF2E2E36); p.plateBot = Colour(0xFF16161B);
    p.ink = Colour(0xFFE9E6F5); p.accent = accent; p.accent2 = kIce;
    p.mute = p.ink.withAlpha(0.55f); p.line = p.ink.withAlpha(0.14f);
    p.screen = Colour(0xFF0B0B10); p.screenInk = kIce; p.brush = 0.9f;
    return p;
}

// Potard dont le double-clic ramène à la valeur de référence (look ou preset).
class ResetSlider : public juce::Slider {
public:
    std::function<double()> resetTo;
    void mouseDoubleClick(const juce::MouseEvent&) override { if (resetTo) setValue(resetTo(), juce::sendNotificationSync); }
};

// Potard + nom gravé + afficheur de valeur. R = rayon du potard (unités de conception) ; k = échelle courante.
class Pot : public juce::Component {
public:
    Pot(juce::AudioProcessorValueTreeState& a, const juce::String& id, const juce::String& lab, const juce::String& tip, std::function<double()> reset, float radius, Colour accent = kLilac)
        : look(palette(accent)), label(lab), R(radius) {
        par = a.getParameter(id);
        s.setLookAndFeel(&look);
        s.setSliderStyle(juce::Slider::RotaryVerticalDrag); s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        s.setMouseDragSensitivity(240); s.setTooltip(tip + kit::u8(" Double-clic : valeur de référence."));
        s.resetTo = std::move(reset); s.onValueChange = [this] { repaint(); };
        addAndMakeVisible(s);
        att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(a, id, s);
    }
    ~Pot() override { s.setLookAndFeel(nullptr); }
    static float heightFor(float R) { return 3.f * R + 28.f; }
    void setScale(float u) { k = u; resized(); }
    void resized() override { s.setBounds(juce::Rectangle<float>((float)getWidth() * 0.5f - 1.5f * R * k, 0.f, 3.f * R * k, 3.f * R * k).toNearestInt()); }
    void paint(juce::Graphics& g) override {
        auto pal = palette(); const float w = (float)getWidth();
        kit::engraved(g, pal, label, juce::Rectangle<float>(0, (3.f * R + 2.f) * k, w, 11.f * k), juce::Justification::centred, 8.4f * k, 1.1f * k, true);
        const float vw = juce::jmin(w - 4 * k, 60.f * k);
        auto r = juce::Rectangle<float>(w / 2 - vw / 2, (3.f * R + 14.f) * k, vw, 11.f * k);
        g.setColour(juce::Colours::black.withAlpha(0.42f)); g.fillRoundedRectangle(r, 2.5f * k);
        g.setColour(juce::Colours::white.withAlpha(0.05f)); g.drawRoundedRectangle(r, 2.5f * k, 1.f);
        g.setColour(kIce.withAlpha(0.92f)); g.setFont(kit::mono(8.8f * k));
        g.drawText(par ? par->getText(par->getValue(), 0) : juce::String(), r, juce::Justification::centred);
    }
private:
    kit::Look look; ResetSlider s; juce::String label; juce::RangedAudioParameter* par = nullptr; float R, k = 1.f;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};

// Interrupteur « IN » à voyant : allumé = effet en service (le paramètre est « bypass »).
class Power : public juce::Button {
public:
    Power() : juce::Button("in"), P(palette()) { setClickingTogglesState(true); }
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
        kit::tracked(g, "In", juce::Rectangle<float>(0, r.getBottom() - 11, r.getWidth(), 11), juce::Justification::centred, 1.6f, kit::fnt(juce::jmax(7.f, rad * 0.55f), true));
    }
private:
    kit::Palette P;
};

// LookAndFeel Kit dont les textes de listes et les interrupteurs suivent l'échelle de la fenêtre (k).
class ScaledLook : public kit::Look {
public:
    explicit ScaledLook(kit::Palette p) : kit::Look(p) {}
    float k = 1.f;
    juce::Font getComboBoxFont(juce::ComboBox&) override { return kit::mono(11.f * k); }
    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& b, bool, bool) override {
        auto r = b.getLocalBounds().toFloat(); const float s = r.getHeight() / 18.f;
        kit::led(g, r.getX() + 9.f * s, r.getCentreY(), 4.f * s, b.getToggleState(), pal().accent);
        g.setColour(pal().ink.withAlpha(b.getToggleState() ? 0.98f : 0.6f));
        kit::tracked(g, b.getButtonText(), r.withTrimmedLeft(21.f * s), juce::Justification::centredLeft, 1.3f * s, kit::fnt(10.8f * s, true));
    }
};

// Barre de niveau segmentée (afficheur). db dans [lo, hi] ; bipolaire : zéro au centre.
inline void bar(juce::Graphics& g, juce::Rectangle<float> r, float v, float lo, float hi, Colour c, bool bipolar = false) {
    const int n = 28; const float sw = r.getWidth() / (float)n;
    for (int i = 0; i < n; ++i) {
        const float t0 = (float)i / (float)n, t1 = (float)(i + 1) / (float)n, pos = (v - lo) / (hi - lo);
        bool lit = bipolar ? (pos >= 0.5f ? (t1 > 0.5f && t0 < pos && t0 >= 0.5f - 1e-4f) : (t0 + 1e-4f >= pos && t1 <= 0.5f + 1e-4f)) : t0 < pos;
        g.setColour(lit ? c.withAlpha(0.95f) : c.withAlpha(0.12f));
        g.fillRoundedRectangle(r.getX() + (float)i * sw + 0.6f, r.getY(), sw - 1.4f, r.getHeight(), 1.f);
    }
}

}  // namespace ui
}  // namespace oc
