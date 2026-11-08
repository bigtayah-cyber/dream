#pragma once
#include "../../Kit/Kit.h"
#include "MasterProcessor.h"

// Potard + nom gravé + afficheur de valeur. Double-clic : valeur d'origine (mix.html, P0).
class MasPot : public juce::Component {
public:
    MasPot(juce::AudioProcessorValueTreeState&, const juce::String& id, const juce::String& label, const juce::String& tip);
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    juce::Slider s;
    juce::String label;
    juce::RangedAudioParameter* par = nullptr;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};

// Interrupteur « IN » à voyant : allumé = effet en service.
class MasPower : public juce::Button {
public:
    explicit MasPower(const kit::Palette& p) : juce::Button("in"), P(p) { setClickingTogglesState(true); }
    void paintButton(juce::Graphics&, bool, bool) override;
private:
    kit::Palette P;
};

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
    juce::Image plate;
    float u_ = 1.f;
    float dIn = -60.f, dOut = -60.f, dGc = 0.f, dGl = 0.f, lufsO = -70.f, lufsI = -70.f, holdOut = -60.f;
    float eqCache[mas::kBands] = {};
    juce::OwnedArray<MasPot> pots;
    juce::OwnedArray<juce::ToggleButton> toggles;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::ButtonAttachment> toggleAtts;
    juce::TextButton resetBtn;
    MasPower power;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAtt;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MasterEditor)
};
