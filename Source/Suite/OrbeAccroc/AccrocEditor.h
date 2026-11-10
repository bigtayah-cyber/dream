#pragma once
#include "AccrocProcessor.h"

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
private:
    void timerCallback() override;
    void setBrush(int b);
    juce::Rectangle<int> Si(float x, float y, float w, float h) const { return juce::Rectangle<float>(x * u_, y * u_, w * u_, h * u_).toNearestInt(); }

    AccrocProcessor& proc;
    kit::Palette pal;
    kit::Look look;
    juce::TooltipWindow tooltip{this, 600};
    float u_ = 1.f;
    std::unique_ptr<SeqView> seqView;
    std::unique_ptr<StatusView> status;
    juce::OwnedArray<of::Pot> pots;
    std::unique_ptr<of::Seg> stepSeg, clockSeg;
    juce::OwnedArray<kit::w::PushKey> brushes;
    juce::OwnedArray<PadKey> pads;
    kit::w::PushKey reroll, clear;
    juce::ComboBox presets;
    of::Power power;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAtt;
    kit::w::HelpKey help;
    int shownProgram = -1;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AccrocEditor)
};
