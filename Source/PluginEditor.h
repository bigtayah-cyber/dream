#pragma once
#include "Kit/Widgets.h"
#include "Kit/KeyClipboard.h"
#include "Kit/v3/Widgets3.h"
#include "PluginProcessor.h"

// Voile : unité de traitement vocal en largeur.
// Trois apparences : Clair (défaut) et Sombre = kit v3 « hybride v2 » avec écran HAUTEUR ; Classique = face d'origine, inchangée.
class VoileEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit VoileEditor(VoileProcessor&);
    ~VoileEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    static kit::Palette palette();
    // Thème (mémorisé hors apvts si persist) et échelle de fenêtre (0,75 à 1,5 ; jusqu'à 2 en Classique).
    void setThemeMode(kit::v3::Mode m, bool persist = true);
    void setUiScale(float s);
    kit::v3::Mode themeMode() const { return mode; }

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAtt = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    static constexpr float kClassicW = 760.f, kClassicH = 360.f, kW3 = 1000.f, kH3 = 596.f;
    void timerCallback() override;
    void stepPreset(int d);
    void paintClassic(juce::Graphics&);
    void resizedClassic();
    void paint3(juce::Graphics&);
    void resized3();
    void paintBackdrop3(juce::Graphics&, juce::Rectangle<float>);
    void paintHero(juce::Graphics&);
    bool isV3() const { return mode != kit::v3::Mode::Classic; }
    juce::Rectangle<float> S(float x, float y, float w, float h) const { return {x * u_, y * u_, w * u_, h * u_}; }
    juce::Rectangle<int> Si(float x, float y, float w, float h) const { return S(x, y, w, h).toNearestInt(); }

    VoileProcessor& proc;
    kit::Palette pal;
    kit::Look look;
    kit::v3::Mode mode = kit::v3::Mode::Light;
    kit::v3::Theme T;
    kit::v3::Look3 look3{T};
    kit::v3::ArtCache art;
    juce::TooltipWindow tooltip{this, 600};
    kit::VuBallistics vu;
    float u_ = 1.f, shiftSm = 0.f, lvlSm = 0.f, scale_ = 1.f, viewC = 64.f;
    int lastProg = -1;
    bool lastFlat = false;   // écriture dièses / bémols (paramètre « accid »)
    std::unique_ptr<kit::v3::bridge::Seg> accSeg;

    // --- Classique
    juce::Slider knob[4], mixS, outS;
    juce::ComboBox keyBox, scaleBox, presetBox;
    kit::KeyPaster keyPaster;   // clic droit sur Tonalité : coller la tonalité copiée depuis Orbe Détect
    juce::ArrowButton prevB{"prev", 0.5f, kit::w::kSilk.withAlpha(0.7f)}, nextB{"next", 0.f, kit::w::kSilk.withAlpha(0.7f)};
    kit::v3::bridge::HelpKey help;
    std::unique_ptr<SliderAtt> knobAtt[4], mixAtt, outAtt;
    std::unique_ptr<ComboAtt> keyAtt, scaleAtt;

    // --- v3
    std::unique_ptr<kit::v3::Knob3> k3[4], mix3, out3;
    kit::v3::Chevron prev3{true}, next3{false};
    std::unique_ptr<kit::v3::InSwitch> inSw;
    kit::v3::ThemeMenu themeBtn;
    kit::v3::Trace trSung, trOut, trTgt, trLvl;
    float bSung[VoileProcessor::Scope::N], bOut[VoileProcessor::Scope::N], bTgt[VoileProcessor::Scope::N], bLvl[VoileProcessor::Scope::N];
    int nView = 0, valid = 0, lastNote = -1;
    float lastSung = 0.f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VoileEditor)
};
