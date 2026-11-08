#pragma once
#include "DeEsserProcessor.h"

class DeScope;
class GrPanel;

class DeEsserEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit DeEsserEditor(DeEsserProcessor&);
    ~DeEsserEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    static constexpr float kW = 760.f, kH = 500.f;
private:
    void timerCallback() override;
    juce::Rectangle<int> Si(float x, float y, float w, float h) const { return juce::Rectangle<float>(x * u_, y * u_, w * u_, h * u_).toNearestInt(); }

    DeEsserProcessor& proc;
    kit::Palette pal;
    kit::Look look;
    juce::TooltipWindow tooltip{this, 600};
    juce::Image plate;
    float u_ = 1.f;
    std::unique_ptr<DeScope> scope;
    std::unique_ptr<GrPanel> grPanel;
    juce::OwnedArray<of::Pot> pots;
    of::Power power;
    of::Key listenKey;
    std::unique_ptr<of::Seg> modeSeg, styleSeg;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAtt, listenAtt;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DeEsserEditor)
};
