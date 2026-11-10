#pragma once
#include "../../Kit/v3/Analysis3.h"
#include "MixProcessor.h"

class MixEditor : public ou3::Base3 {
public:
    explicit MixEditor(MixProcessor&);
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
private:
    void layout() override;
    void layout3() override;
    void body(juce::Graphics&) override;
    void body3(juce::Graphics&) override;
    void backdrop3(juce::Graphics&, float k) override;
    void themeChanged(const kit::v3::Theme* tp) override;
    void timerCallback() override;
    void paintSpectrum(juce::Graphics&, float u);
    void paintAdvice(juce::Graphics&, float u);

    MixProcessor& proc;
    oa::MixSnap snap; oa::Report rep; bool ready = false, blank = false; int wantEpoch = 0, scroll = 0;
    juce::ComboBox styleBox;
    kit::v3::bridge::PushKey resetB;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> styleAtt;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MixEditor)
};
