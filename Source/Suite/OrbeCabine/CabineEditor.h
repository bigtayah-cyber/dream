#pragma once
#include "../../Kit/Widgets.h"
#include "CabineProcessor.h"

class CabineEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit CabineEditor(CabineProcessor&);
    ~CabineEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseUp(const juce::MouseEvent&) override;
    static kit::Palette palette();

private:
    void timerCallback() override;
    void paintLadder(juce::Graphics&, juce::Rectangle<float>, const char* title, const float* thr, const char* const* lab, float value, juce::Colour hot);
    void module(juce::Graphics&, int index, float y);
    void showMenu();
    void step(int d) { proc.applyPreset((proc.currentPreset() + d + cab::kPresets) % cab::kPresets); }
    juce::Rectangle<float> S(float x, float y, float w, float h) const { return {x * u_, y * u_, w * u_, h * u_}; }
    juce::Rectangle<int> Si(float x, float y, float w, float h) const { return S(x, y, w, h).toNearestInt(); }

    CabineProcessor& proc;
    kit::Palette pal;
    kit::Look look;
    juce::TooltipWindow tooltip{this, 600};
    float u_ = 1.f, grDisp = 0.f, pkDisp = 0.f;
    kit::VuBallistics vu;
    bool modified = false;
    juce::OwnedArray<kit::w::Pot> pots;
    juce::ArrowButton prev{"prev", 0.5f, kit::w::kSilk.withAlpha(0.7f)}, next{"next", 0.f, kit::w::kSilk.withAlpha(0.7f)};
    kit::w::Power power;
    kit::w::HelpKey help;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAtt;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CabineEditor)
};
