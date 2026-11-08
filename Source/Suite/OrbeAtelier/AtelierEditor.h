#pragma once
#include "AtelierProcessor.h"

class AtelierEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit AtelierEditor(AtelierProcessor&);
    ~AtelierEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseUp(const juce::MouseEvent&) override;
private:
    struct Mod { const char* title; const char* on; std::vector<juce::String> ids; int row; juce::Rectangle<float> panel; int firstPot = 0, nPots = 0; std::vector<juce::Rectangle<float>> labels; };
    void timerCallback() override;
    void showMenu();
    void step(int d) { proc.applyPreset((proc.currentPreset() + d + atl::numPresets()) % atl::numPresets()); }
    juce::Rectangle<float> S(float x, float y, float w, float h) const { return {x * u_, y * u_, w * u_, h * u_}; }
    juce::Rectangle<int> Si(float x, float y, float w, float h) const { return S(x, y, w, h).toNearestInt(); }

    AtelierProcessor& proc;
    kit::Palette pal;
    oc::ui::ScaledLook look;
    juce::TooltipWindow tooltip{this, 600};
    juce::Image plate;
    float u_ = 1.f, outDb = -60.f, grDisp = 0.f;
    bool modified = false;
    std::vector<Mod> mods;
    juce::OwnedArray<oc::ui::Pot> pots;
    juce::OwnedArray<juce::ToggleButton> toggles; juce::OwnedArray<juce::ComboBox> combos;
    juce::ArrowButton prev{"prev", 0.5f, oc::ui::kLilac}, next{"next", 0.f, oc::ui::kLilac};
    oc::ui::Power power;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAtt;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> togAtt;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> cmbAtt;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AtelierEditor)
};
