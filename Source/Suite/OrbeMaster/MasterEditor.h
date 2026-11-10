#pragma once
#include "../../Kit/Widgets.h"
#include "MasterProcessor.h"
#include "../../Kit/v3/Rack3.h"

class MasterEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit MasterEditor(MasterProcessor&);
    ~MasterEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    static kit::Palette palette();
    void setThemeMode(kit::v3::Mode m, bool persist = true);
    void setUiScale(float s);

private:
    static constexpr float kW3 = 1180.f, kH3 = 712.f;
    bool isV3() const { return skin.v3(); }
    void resizedClassic();
    void resized3();
    void paint3(juce::Graphics&);
    void paintBackdrop3(juce::Graphics&, juce::Rectangle<float>);
    void paintCurve3(juce::Graphics&, juce::Rectangle<float>);
    void paintMeters3(juce::Graphics&, juce::Rectangle<float>);
    void paintLufs3(juce::Graphics&, juce::Rectangle<float>);
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
    kit::v3::rk::Skin skin{"master", kit::v3::accents::ice};
    float scale_ = 1.f;
    kit::v3::Trace trEq;
    float eqVals[200] = {};
    juce::OwnedArray<kit::w::Pot> pots;
    juce::OwnedArray<kit::v3::Knob3> k3;
    kit::VuBallistics vu;
    juce::OwnedArray<juce::ToggleButton> toggles;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::ButtonAttachment> toggleAtts;
    kit::v3::bridge::PushKey resetBtn;
    kit::v3::bridge::Power power;
    kit::v3::bridge::HelpKey help;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAtt;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MasterEditor)
};
