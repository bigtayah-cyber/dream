#pragma once
#include "LooksProcessor.h"

class LooksEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit LooksEditor(LooksProcessor&);
    ~LooksEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseUp(const juce::MouseEvent&) override;
private:
    void timerCallback() override;
    void showMenu();
    void step(int d) { proc.applyLook((proc.currentLook() + d + oc::numLooks()) % oc::numLooks()); }
    void section(juce::Graphics&, const char* title, float y);
    juce::Rectangle<float> S(float x, float y, float w, float h) const { return {x * u_, y * u_, w * u_, h * u_}; }
    juce::Rectangle<int> Si(float x, float y, float w, float h) const { return S(x, y, w, h).toNearestInt(); }

    LooksProcessor& proc;
    kit::Palette pal;
    oc::ui::ScaledLook look;
    juce::TooltipWindow tooltip{this, 600};
    float u_ = 1.f, outDb = -60.f, grDisp = 0.f, shiftDisp = 0.f;
    int shownKey = -2;
    bool moved = false;
    float modLevel[8] = {};
    juce::OwnedArray<oc::ui::Pot> pots;
    juce::ComboBox keyBox, scaleBox;
    juce::ArrowButton prev{"prev", 0.5f, kit::w::kSilk.withAlpha(0.7f)}, next{"next", 0.f, kit::w::kSilk.withAlpha(0.7f)};
    oc::ui::Power power;
    kit::w::HelpKey help;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> keyAtt, scaleAtt;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LooksEditor)
};
