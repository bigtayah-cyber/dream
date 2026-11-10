#pragma once
#include "../../Kit/Widgets.h"
#include "CabineProcessor.h"
#include "../../Kit/v3/Rack3.h"

class CabineEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit CabineEditor(CabineProcessor&);
    ~CabineEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseUp(const juce::MouseEvent&) override;
    static kit::Palette palette();
    // Thème (mémorisé hors apvts si persist) et échelle de fenêtre.
    void setThemeMode(kit::v3::Mode m, bool persist = true);
    void setUiScale(float s);

private:
    bool isV3() const { return skin.v3(); }
    void resizedClassic();
    void resized3();
    void paint3(juce::Graphics&);
    void paintBackdrop3(juce::Graphics&, juce::Rectangle<float>);
    void paintLadder3(juce::Graphics&, juce::Rectangle<float>, const char* title, const float* thr, const char* const* lab, float value);
    juce::Rectangle<float> presetArea() const;
    void timerCallback() override;
    void paintLadder(juce::Graphics&, juce::Rectangle<float>, const char* title, const float* thr, const char* const* lab, float value, juce::Colour hot);
    void module(juce::Graphics&, int index, float y);
    void showMenu();
    void step(int d) { proc.applyPreset((proc.currentPreset() + d + cab::kPresets) % cab::kPresets); }
    static constexpr float kW3 = 720.f, kH3 = 836.f;
    juce::Rectangle<float> S(float x, float y, float w, float h) const { return {x * u_, y * u_, w * u_, h * u_}; }
    juce::Rectangle<int> Si(float x, float y, float w, float h) const { return S(x, y, w, h).toNearestInt(); }

    CabineProcessor& proc;
    kit::Palette pal;
    kit::Look look;
    juce::TooltipWindow tooltip{this, 600};
    float u_ = 1.f, grDisp = 0.f, pkDisp = 0.f;
    kit::VuBallistics vu;
    bool modified = false;
    kit::v3::rk::Skin skin{"cabine", kit::v3::accents::amber};
    float scale_ = 1.f;
    juce::OwnedArray<kit::w::Pot> pots;
    juce::OwnedArray<kit::v3::rk::RefKnob> k3;
    kit::v3::Chevron cPrev{true}, cNext{false};
    juce::ArrowButton prev{"prev", 0.5f, kit::w::kSilk.withAlpha(0.7f)}, next{"next", 0.f, kit::w::kSilk.withAlpha(0.7f)};
    kit::v3::bridge::Power power;
    kit::v3::bridge::HelpKey help;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAtt;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CabineEditor)
};
