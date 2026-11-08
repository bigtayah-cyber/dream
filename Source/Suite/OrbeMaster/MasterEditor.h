#pragma once
#include "../../Kit/Widgets.h"
#include "MasterProcessor.h"

class MasterEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit MasterEditor(MasterProcessor&);
    ~MasterEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    static kit::Palette palette();

private:
    void timerCallback() override;
    void paintCurve(juce::Graphics&, juce::Rectangle<float>);
    void paintMeters(juce::Graphics&, juce::Rectangle<float>);
    void paintLufs(juce::Graphics&, juce::Rectangle<float>);
    void module(juce::Graphics&, const char* title, float x, float w, float y);
    juce::Rectangle<float> S(float x, float y, float w, float h) const { return {x * u_, y * u_, w * u_, h * u_}; }
    juce::Rectangle<int> Si(float x, float y, float w, float h) const { return S(x, y, w, h).toNearestInt(); }

    MasterProcessor& proc;
    kit::Palette pal;
    kit::Look look;
    juce::TooltipWindow tooltip{this, 600};
    float u_ = 1.f;
    float dIn = -60.f, dOut = -60.f, dGc = 0.f, dGl = 0.f, lufsO = -70.f, lufsI = -70.f, holdOut = -60.f;
    float eqCache[mas::kBands] = {};
    juce::OwnedArray<kit::w::Pot> pots;
    kit::VuBallistics vu;
    juce::OwnedArray<juce::ToggleButton> toggles;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::ButtonAttachment> toggleAtts;
    kit::w::PushKey resetBtn;
    kit::w::Power power;
    kit::w::HelpKey help;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAtt;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MasterEditor)
};
