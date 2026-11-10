#pragma once
#include "../../Kit/v3/Analysis3.h"
#include "../../Kit/v3/NoteNames.h"
#include "DetectProcessor.h"

class DetectEditor : public ou3::Base3 {
public:
    explicit DetectEditor(DetectProcessor&);
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override { hover.clear(); repaint(); }
    void mouseDown(const juce::MouseEvent&) override;
private:
    void layout() override;
    void layout3() override;
    void body(juce::Graphics&) override;
    void body3(juce::Graphics&) override;
    void backdrop3(juce::Graphics&, float k) override;
    void themeChanged(const kit::v3::Theme* tp) override;
    void timerCallback() override;
    std::string wheelHit(juce::Point<float> p) const;
    juce::Rectangle<float> wheelRect() const { return isV3() ? R(780, 118, 340, 340) : R(680, 112, 288, 288); }
    std::string activeCode() const;
    void tap();

    DetectProcessor& proc;
    std::shared_ptr<const DetectResult> cur;
    double capSec = 0, tapBpm = 0; bool busy = false, full = false;
    float hostBpm = 0.f;
    std::string hover, focus;
    std::vector<double> taps;

    kit::v3::UiAccidental acc{"detect"};            // dièses / bémols : réglage d'interface (Prefs)
    std::unique_ptr<kit::v3::UiSeg> accSeg;
    bool flat_() const { return acc.flat(); }
    kit::v3::bridge::PushKey captureB;
    kit::v3::bridge::PushKey clearB{"Vider"}, analyzeB{"Analyser"}, halfB, dblB{juce::String::fromUTF8("×2")}, tapB{"Tap"}, applyB;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> capAtt;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DetectEditor)
};
