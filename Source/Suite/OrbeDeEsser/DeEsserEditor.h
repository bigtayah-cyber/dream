#pragma once
#include "DeEsserProcessor.h"
#include "../../Kit/v3/Widgets3.h"

class DeScope;
class GrPanel;

class DeEsserEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit DeEsserEditor(DeEsserProcessor&);
    ~DeEsserEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    static constexpr float kW = 760.f, kH = 462.f, kW3 = 1000.f, kH3 = 600.f;
    // Thème (mémorisé hors apvts si persist) : Clair / Sombre (v3) ou Classique (face d'origine) ; échelle de fenêtre.
    void setThemeMode(kit::v3::Mode m, bool persist = true);
    void setUiScale(float s);
private:
    bool isV3() const { return mode != kit::v3::Mode::Classic; }
    void paintBackdrop3(juce::Graphics&, juce::Rectangle<float>);
    void timerCallback() override;
    juce::Rectangle<int> Si(float x, float y, float w, float h) const { return juce::Rectangle<float>(x * u_, y * u_, w * u_, h * u_).toNearestInt(); }

    DeEsserProcessor& proc;
    kit::Palette pal;
    kit::Look look;
    juce::TooltipWindow tooltip{this, 600};
    float u_ = 1.f;
    std::unique_ptr<DeScope> scope;
    std::unique_ptr<GrPanel> grPanel;
    juce::OwnedArray<of::Pot> pots;
    kit::v3::bridge::Power power;
    of::Key listenKey;
    std::unique_ptr<kit::v3::bridge::Seg> modeSeg, styleSeg;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAtt, listenAtt, listen3Att;
    kit::v3::bridge::HelpKey help;
    // --- v3
    kit::v3::Mode mode = kit::v3::Mode::Light;
    kit::v3::Theme T;
    kit::v3::Look3 look3{T};
    kit::v3::ArtCache art;
    kit::v3::ThemeMenu themeBtn;
    juce::OwnedArray<kit::v3::Knob3> k3;
    kit::v3::bridge::PushKey listen3;
    float scale_ = 1.f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DeEsserEditor)
};
