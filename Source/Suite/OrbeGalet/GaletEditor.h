#pragma once
#include "GaletProcessor.h"
#include "../../Kit/v3/Rack3.h"

class ShapeView;
class TransferView;

class GaletEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit GaletEditor(GaletProcessor&);
    ~GaletEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    static constexpr float kW = 960.f, kH = 694.f;
    void setThemeMode(kit::v3::Mode m, bool persist = true);
    void setUiScale(float s);
private:
    static constexpr float kW3 = 1180.f, kH3 = 884.f;
    bool isV3() const { return skin.v3(); }
    void resizedClassic();
    void resized3();
    void paint3(juce::Graphics&);
    void timerCallback() override;
    juce::Rectangle<int> Si(float x, float y, float w, float h) const { return juce::Rectangle<float>(x * u_, y * u_, w * u_, h * u_).toNearestInt(); }

    GaletProcessor& proc;
    kit::Palette pal;
    kit::Look look;
    juce::TooltipWindow tooltip{this, 600};
    float u_ = 1.f, scale_ = 1.f;
    kit::v3::rk::Skin skin{"galet", kit::v3::accents::mint};
    juce::OwnedArray<kit::v3::Knob3> k3;
    std::unique_ptr<ShapeView> shapeView;
    std::unique_ptr<TransferView> transfer;
    juce::OwnedArray<of::Pot> pots;
    std::unique_ptr<kit::v3::bridge::Seg> syncSeg, retrigSeg, feelSeg, interpSeg;
    juce::OwnedArray<kit::v3::rk::Key> modKeys;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::ButtonAttachment> modAtts;
    kit::v3::rk::Key snapKey;
    juce::OwnedArray<kit::v3::bridge::PushKey> starts;
    juce::ComboBox presets;
    kit::v3::bridge::Power power;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAtt;
    kit::v3::bridge::HelpKey help;
    int shownProgram = -1;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GaletEditor)
};
