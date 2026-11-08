#pragma once
#include "DenoiserProcessor.h"

class DnScope;

class DenoiserEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit DenoiserEditor(DenoiserProcessor&);
    ~DenoiserEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    static constexpr float kW = 808.f, kH = 468.f;
private:
    void timerCallback() override;
    juce::Rectangle<int> Si(float x, float y, float w, float h) const { return juce::Rectangle<float>(x * u_, y * u_, w * u_, h * u_).toNearestInt(); }

    DenoiserProcessor& proc;
    kit::Palette pal;
    kit::Look look;
    juce::TooltipWindow tooltip{this, 600};
    juce::Image plate;
    float u_ = 1.f;
    std::unique_ptr<DnScope> scope;
    juce::OwnedArray<of::Pot> pots;
    of::Power power;
    of::Key learnKey, adaptKey, listenKey;
    std::unique_ptr<of::Seg> humSeg;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAtt, adaptAtt, listenAtt;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DenoiserEditor)
};
