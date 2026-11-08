#pragma once
#include "../OrbeAnalysis/OrbeUi.h"
#include "MixProcessor.h"

class MixEditor : public ou::Base {
public:
    explicit MixEditor(MixProcessor&);
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
private:
    void layout() override;
    void body(juce::Graphics&) override;
    void timerCallback() override;
    void paintSpectrum(juce::Graphics&, float u);
    void paintAdvice(juce::Graphics&, float u);

    MixProcessor& proc;
    oa::MixSnap snap; oa::Report rep; bool ready = false, blank = false; int wantEpoch = 0, scroll = 0;
    juce::ComboBox styleBox;
    kit::w::PushKey resetB;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> styleAtt;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MixEditor)
};
