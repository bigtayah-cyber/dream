#pragma once
#include "../../Kit/Widgets.h"
#include "RelanceProcessor.h"
#include "../../Kit/v3/Widgets3.h"

// Face d'Orbe Relance, directement au design v3 (thèmes Clair par défaut, Sombre, Classique = graphite du rack).
// Écran héros : enveloppe de la voix, voisement, seuil, fins de phrase et relances programmées (pastilles par caractère),
// reliées par un arc de la phrase à sa réponse. Panneau de droite : compteurs, dernier caractère, voix de relance actives.
class RelanceEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit RelanceEditor(RelanceProcessor&);
    ~RelanceEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void setThemeMode(kit::v3::Mode m, bool persist = true);
    void setUiScale(float s);
    static juce::Colour typeColour(int t);

private:
    static constexpr float kW3 = 1100.f, kH3 = 706.f;
    void timerCallback() override;
    void paintBackdrop(juce::Graphics&, juce::Rectangle<float>);
    void paintScreen(juce::Graphics&, juce::Rectangle<float>);
    void paintReadout(juce::Graphics&, juce::Rectangle<float>);
    juce::Rectangle<float> S(float x, float y, float w, float h) const { return {x * u_, y * u_, w * u_, h * u_}; }
    juce::Rectangle<int> Si(float x, float y, float w, float h) const { return S(x, y, w, h).toNearestInt(); }

    RelanceProcessor& proc;
    kit::Palette pal;
    kit::v3::Mode mode = kit::v3::Mode::Light;
    kit::v3::Theme T;
    kit::v3::Look3 look3{T};
    kit::v3::ArtCache art;
    juce::TooltipWindow tooltip{this, 600};
    float scale_ = 1.f, u_ = 1.f;
    int lastProg = -1;
    juce::OwnedArray<kit::v3::Knob3> k3;
    kit::v3::Trace trIn, trWet;
    float sIn[rel::Telemetry::N] = {}, sWet[rel::Telemetry::N] = {};
    char sVo[rel::Telemetry::N] = {};
    juce::ComboBox presetBox, caracBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> caracAtt;
    std::unique_ptr<kit::v3::bridge::Seg> delaySeg, lockSeg, outSeg;
    std::unique_ptr<kit::v3::InSwitch> inSw;
    kit::v3::ThemeMenu themeBtn;
    kit::v3::bridge::HelpKey help;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RelanceEditor)
};
