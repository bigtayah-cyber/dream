#pragma once
#include "Kit/Widgets.h"
#include "PluginProcessor.h"

// Voile : unité de traitement vocal en largeur, même socle matériel que les autres plugins dreamaws.
class VoileEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit VoileEditor(VoileProcessor&);
    ~VoileEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    static kit::Palette palette();

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAtt = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    void timerCallback() override;
    void stepPreset(int d);
    juce::Rectangle<float> S(float x, float y, float w, float h) const { return {x * u_, y * u_, w * u_, h * u_}; }
    juce::Rectangle<int> Si(float x, float y, float w, float h) const { return S(x, y, w, h).toNearestInt(); }

    VoileProcessor& proc;
    kit::Palette pal;
    kit::Look look;
    juce::TooltipWindow tooltip{this, 600};
    kit::VuBallistics vu;
    float u_ = 1.f, shiftSm = 0.f, lvlSm = 0.f;

    juce::Slider knob[4], mixS, outS;
    juce::ComboBox keyBox, scaleBox, presetBox;
    juce::ArrowButton prevB{"prev", 0.5f, kit::w::kSilk.withAlpha(0.7f)}, nextB{"next", 0.f, kit::w::kSilk.withAlpha(0.7f)};
    kit::w::HelpKey help;
    std::unique_ptr<SliderAtt> knobAtt[4], mixAtt, outAtt;
    std::unique_ptr<ComboAtt> keyAtt, scaleAtt;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VoileEditor)
};
