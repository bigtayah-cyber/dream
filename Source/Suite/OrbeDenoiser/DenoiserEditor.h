#pragma once
#include "DenoiserProcessor.h"
#include "../../Kit/v3/Widgets3.h"

class DnScope;

class DenoiserEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit DenoiserEditor(DenoiserProcessor&);
    ~DenoiserEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    static constexpr float kW = 808.f, kH = 428.f, kW3 = 1000.f, kH3 = 600.f;
    // Thème (mémorisé hors apvts si persist) : Clair / Sombre (v3) ou Classique (face d'origine) ; échelle de fenêtre.
    void setThemeMode(kit::v3::Mode m, bool persist = true);
    void setUiScale(float s);
private:
    bool isV3() const { return mode != kit::v3::Mode::Classic; }
    void paintBackdrop3(juce::Graphics&, juce::Rectangle<float>);
    void timerCallback() override;
    juce::Rectangle<int> Si(float x, float y, float w, float h) const { return juce::Rectangle<float>(x * u_, y * u_, w * u_, h * u_).toNearestInt(); }

    DenoiserProcessor& proc;
    kit::Palette pal;
    kit::Look look;
    juce::TooltipWindow tooltip{this, 600};
    float u_ = 1.f;
    std::unique_ptr<DnScope> scope;
    juce::OwnedArray<of::Pot> pots;
    kit::v3::bridge::Power power;
    of::Key learnKey, adaptKey, listenKey;
    // v3 : réglages ajoutés (entre les phrases, réduction graves / aigus, mode Prise)
    juce::Slider extraS[3]; juce::Label extraL[3]; juce::ToggleButton liveBtn;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> extraAtt[3];
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> liveAtt;
    std::unique_ptr<kit::v3::bridge::Seg> humSeg;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAtt, adaptAtt, listenAtt, adapt3Att, listen3Att;
    kit::v3::bridge::HelpKey help;
    // --- v3
    kit::v3::Mode mode = kit::v3::Mode::Light;
    kit::v3::Theme T;
    kit::v3::Look3 look3{T};
    kit::v3::ArtCache art;
    kit::v3::ThemeMenu themeBtn;
    juce::OwnedArray<kit::v3::Knob3> k3;
    kit::v3::bridge::PushKey learn3, adapt3, listen3;
    float scale_ = 1.f, redSm = 0.f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DenoiserEditor)
};
