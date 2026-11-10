#pragma once
#include "AccrocProcessor.h"
#include "../../Kit/v3/Rack3.h"

class SeqView;
class StatusView;
class PadKey;

class AccrocEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit AccrocEditor(AccrocProcessor&);
    ~AccrocEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    static constexpr float kW = 1104.f, kH = 674.f;
    void setThemeMode(kit::v3::Mode m, bool persist = true);
    void setUiScale(float s);
private:
    static constexpr float kW3 = 1240.f, kH3 = 870.f;
    bool isV3() const { return skin.v3(); }
    void resizedClassic();
    void resized3();
    void paint3(juce::Graphics&);
    void timerCallback() override;
    void setBrush(int b);
    juce::Rectangle<int> Si(float x, float y, float w, float h) const { return juce::Rectangle<float>(x * u_, y * u_, w * u_, h * u_).toNearestInt(); }

    AccrocProcessor& proc;
    kit::Palette pal;
    kit::Look look;
    juce::TooltipWindow tooltip{this, 600};
    float u_ = 1.f, scale_ = 1.f;
    kit::v3::rk::Skin skin{"accroc", juce::Colour(0xFF7DB82F)};
    juce::OwnedArray<kit::v3::Knob3> k3;
    std::unique_ptr<SeqView> seqView;
    std::unique_ptr<StatusView> status;
    juce::OwnedArray<of::Pot> pots;
    std::unique_ptr<kit::v3::bridge::Seg> stepSeg, clockSeg;
    juce::OwnedArray<kit::v3::bridge::PushKey> brushes;
    juce::OwnedArray<PadKey> pads;
    kit::v3::bridge::PushKey reroll, clear;
    juce::ComboBox presets;
    kit::v3::bridge::Power power;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAtt;
    kit::v3::bridge::HelpKey help;
    int shownProgram = -1;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AccrocEditor)
};
