#pragma once
#include <functional>
#include "../Kit/Kit.h"
#include "../Kit/v3/Widgets3.h"
#include "OneKnobProcessor.h"

// Bouton dessiné par la face (levier IN, footswitch, poussoirs à LED du volet).
class FaceButton : public juce::Button {
public:
    using Painter = std::function<void(juce::Graphics&, FaceButton&, bool hover, bool down)>;
    FaceButton() : juce::Button({}) { setClickingTogglesState(true); }
    Painter painter;
    void paintButton(juce::Graphics& g, bool hover, bool down) override { if (painter) painter(g, *this, hover, down); }
};

// Look du kit : polices de listes proportionnelles à la taille de la fenêtre, couleur de capuchon par potard.
class KeyLook : public kit::Look {
public:
    using kit::Look::Look;
    float u = 1.f;
    juce::Font getComboBoxFont(juce::ComboBox&) override { return kit::lcdFont(12.5f * u); }
    juce::Font getPopupMenuFont() override { return kit::fnt(13.f * u); }
    void positionComboBoxText(juce::ComboBox& cb, juce::Label& l) override {
        l.setBounds(juce::roundToInt(7 * u), 1, cb.getWidth() - juce::roundToInt(24 * u), cb.getHeight() - 2);
        l.setFont(getComboBoxFont(cb));
    }
    void drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider& s) override {
        auto p = pal();
        if (const auto* c = s.getProperties().getVarPointer("cap")) p.accent = juce::Colour((juce::uint32)(juce::int64)*c);
        const auto st = (kit::KnobStyle)(int)s.getProperties().getWithDefault("kitKnob", (int)kit::KnobStyle::Auto);
        const float r = juce::jmin((float)w, (float)h) * 0.5f / 1.5f;
        kit::knob(g, p, (float)x + (float)w * 0.5f, (float)y + (float)h * 0.5f, r, pos, st, (bool)s.getProperties().getWithDefault("scale", true));
    }
};

class OneKnobEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit OneKnobEditor(OneKnobProcessor&);
    ~OneKnobEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    static kit::Palette palette(int voicingIndex);

    static constexpr float kW = 300.f, kH = 560.f, kPanel = 168.f;  // face fermée 300 × 560, volet ouvert + 140
    static constexpr float kW3 = 400.f, kH3 = 656.f, kPanel3 = 196.f; // v3 « hybride v2 » : face 400 × 670, volet + 196
    // Thème (mémorisé hors apvts si persist) : Clair / Sombre (v3) ou Classique (face d'origine) ; échelle 0,75 à 1,5 en v3.
    void setThemeMode(kit::v3::Mode m, bool persist = true);
    void setUiScale(float s);

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAtt = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    struct Layout { juce::Rectangle<float> hero, knob, label, bottom; };
    void timerCallback() override;
    void setPanel(bool open, bool resize);
    Layout layout() const;
    void paintHero(juce::Graphics&, juce::Rectangle<float>);
    void paintBottom(juce::Graphics&, juce::Rectangle<float>);
    void paintPanel(juce::Graphics&, juce::Rectangle<float>);
    void paintKeyLeds(juce::Graphics&, juce::Rectangle<float>);
    void serialPlate(juce::Graphics&, juce::Rectangle<float>, bool brass);
    juce::String keyText(bool seg7) const;
    bool isV3() const { return mode != kit::v3::Mode::Classic; }
    void applySize();
    void resizedClassic();
    void resized3();
    void paint3(juce::Graphics&);
    void paintBackdrop3(juce::Graphics&, juce::Rectangle<float>);
    void paintHero3(juce::Graphics&);
    juce::Colour lvlColour() const { return vi == 1 || vi == 3 ? T.sung : (vi == 0 ? juce::Colour(0xFF7FD1FF) : juce::Colour(0xFF5CE1E6)); }
    juce::Rectangle<float> S(float x, float y, float w, float h) const { return {x * u_, y * u_, w * u_, h * u_}; }
    juce::Rectangle<int> Si(float x, float y, float w, float h) const { return S(x, y, w, h).toNearestInt(); }

    OneKnobProcessor& proc;
    const int vi;
    kit::Palette pal;
    KeyLook look;
    juce::TooltipWindow tooltip{this, 600};
    float u_ = 1.f;
    float vu = -60.f, gr = 0.f, inDb = -100.f;
    bool usesPitch = false, panelOpen = false;
    int tick = 0;

    juce::Slider knobS, trimS, toneS, mixS, outS, specS;
    FaceButton inSw, advBtn, autoInB, autoGainB, optB, engineB;
    juce::ComboBox keyBox, scaleBox;
    std::unique_ptr<kit::v3::bridge::Seg> accSeg;   // # / b (modèles à tonalité)
    bool lastFlat = false;
    std::unique_ptr<ComboAtt> keyAtt, scaleAtt;
    std::unique_ptr<SliderAtt> knobAtt, trimAtt, toneAtt, mixAtt, outAtt, specAtt;
    std::unique_ptr<ButtonAtt> inAtt, autoInAtt, autoGainAtt, optAtt, engineAtt;

    // --- v3
    kit::v3::Mode mode = kit::v3::Mode::Light;
    kit::v3::Theme T;
    kit::v3::Look3 look3{T};
    kit::v3::ArtCache art;
    kit::v3::ThemeMenu themeBtn;
    std::unique_ptr<kit::v3::Knob3> big3, small3[5];
    std::unique_ptr<kit::v3::InSwitch> inSw3;
    kit::v3::Trace trLvl, trGr;
    static constexpr int kHist = 120;   // 4 s à 30 Hz
    float ringLvl[kHist] = {}, ringGr[kHist] = {}, bLvl[kHist] = {}, bGr[kHist] = {};
    int ringPos = 0;
    float scale_ = 1.f, vuSm = -60.f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OneKnobEditor)
};
