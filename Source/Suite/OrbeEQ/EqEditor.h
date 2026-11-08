#pragma once
#include "EqProcessor.h"

class EqGraph;
class BandChips;
class EqField;

class EqEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit EqEditor(EqProcessor&);
    ~EqEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseUp(const juce::MouseEvent&) override;
    void select(int band);
    int selected() const { return sel; }
    static constexpr float kW = 900.f, kH = 500.f;
    static juce::Colour bandColour(int i);
private:
    void timerCallback() override;
    void refreshFields();
    void showPresetMenu();
    juce::Rectangle<int> Si(float x, float y, float w, float h) const { return juce::Rectangle<float>(x * u_, y * u_, w * u_, h * u_).toNearestInt(); }

    EqProcessor& proc;
    kit::Palette pal;
    kit::Look look;
    juce::TooltipWindow tooltip{this, 600};
    float u_ = 1.f;
    int sel = 2;
    bool modified = false;
    std::unique_ptr<EqGraph> graph;
    std::unique_ptr<BandChips> chips;
    std::unique_ptr<EqField> fFreq, fGain, fQ;
    juce::ComboBox typeBox;
    of::Key onKey, aKey, bKey;
    of::Power power;
    std::unique_ptr<of::Pot> outPot;
    std::unique_ptr<of::Seg> rangeSeg;
    juce::ArrowButton prev{"prev", 0.5f, kit::w::kSilk.withAlpha(0.7f)}, next{"next", 0.f, kit::w::kSilk.withAlpha(0.7f)};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAtt;
    kit::w::HelpKey help;
    bool updating = false;
    int boundBand = -1;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EqEditor)
};
