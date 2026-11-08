#pragma once
#include "../OrbeAnalysis/OrbeUi.h"
#include "DetectProcessor.h"

class DetectEditor : public ou::Base {
public:
    explicit DetectEditor(DetectProcessor&);
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override { hover.clear(); repaint(); }
    void mouseDown(const juce::MouseEvent&) override;
private:
    void layout() override;
    void body(juce::Graphics&) override;
    void timerCallback() override;
    std::string wheelHit(juce::Point<float> p) const;
    juce::Rectangle<float> wheelRect() const { return R(680, 112, 288, 288); }
    std::string activeCode() const;
    void tap();

    DetectProcessor& proc;
    std::shared_ptr<const DetectResult> cur;
    double capSec = 0, tapBpm = 0; bool busy = false, full = false;
    float hostBpm = 0.f;
    std::string hover, focus;
    std::vector<double> taps;

    kit::w::PushKey captureB;
    kit::w::PushKey clearB{"Vider"}, analyzeB{"Analyser"}, halfB, dblB{juce::String::fromUTF8("×2")}, tapB{"Tap"}, applyB;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> capAtt;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DetectEditor)
};
