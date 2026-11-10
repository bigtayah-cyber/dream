#pragma once
#include "GaletProcessor.h"

class ShapeView;
class TransferView;

class GaletEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit GaletEditor(GaletProcessor&);
    ~GaletEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    static constexpr float kW = 960.f, kH = 694.f;
private:
    void timerCallback() override;
    juce::Rectangle<int> Si(float x, float y, float w, float h) const { return juce::Rectangle<float>(x * u_, y * u_, w * u_, h * u_).toNearestInt(); }

    GaletProcessor& proc;
    kit::Palette pal;
    kit::Look look;
    juce::TooltipWindow tooltip{this, 600};
    float u_ = 1.f;
    std::unique_ptr<ShapeView> shapeView;
    std::unique_ptr<TransferView> transfer;
    juce::OwnedArray<of::Pot> pots;
    std::unique_ptr<of::Seg> syncSeg, retrigSeg, feelSeg, interpSeg;
    juce::OwnedArray<of::Key> modKeys;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::ButtonAttachment> modAtts;
    of::Key snapKey;
    juce::OwnedArray<kit::w::PushKey> starts;
    juce::ComboBox presets;
    of::Power power;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAtt;
    kit::w::HelpKey help;
    int shownProgram = -1;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GaletEditor)
};
