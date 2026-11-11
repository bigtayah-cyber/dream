#pragma once
#include "EqProcessor.h"
#include "../../Kit/v3/Rack3.h"

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
    void setThemeMode(kit::v3::Mode m, bool persist = true);
    void setUiScale(float s);
private:
    static constexpr float kW3 = 1100.f, kH3 = 700.f;
    bool isV3() const { return skin.v3(); }
    void resizedClassic();
    void resized3();
    void paint3(juce::Graphics&);
    juce::Rectangle<float> presetArea() const;
    void timerCallback() override;
    void refreshFields();
    void showPresetMenu();
    juce::Rectangle<float> S(float x, float y, float w, float h) const { return {x * u_, y * u_, w * u_, h * u_}; }
    juce::Rectangle<int> Si(float x, float y, float w, float h) const { return juce::Rectangle<float>(x * u_, y * u_, w * u_, h * u_).toNearestInt(); }

    EqProcessor& proc;
    kit::Palette pal;
    kit::Look look;
    juce::TooltipWindow tooltip{this, 600};
    float u_ = 1.f, scale_ = 1.f;
    kit::v3::rk::Skin skin{"eq", kit::v3::accents::lilac};
    kit::v3::Chevron cPrev{true}, cNext{false};
    std::unique_ptr<kit::v3::Knob3> outK3;
    int sel = 2;
    bool modified = false;
    std::unique_ptr<EqGraph> graph;
    std::unique_ptr<BandChips> chips;
    std::unique_ptr<EqField> fFreq, fGain, fQ;
    juce::ComboBox typeBox, slopeBox;   // slopeBox : pente du coupe-bas / coupe-haut (lot E)
    kit::v3::rk::Key onKey, aKey, bKey;
    kit::v3::bridge::Power power;
    std::unique_ptr<of::Pot> outPot;
    std::unique_ptr<kit::v3::bridge::Seg> rangeSeg;
    juce::ArrowButton prev{"prev", 0.5f, kit::w::kSilk.withAlpha(0.7f)}, next{"next", 0.f, kit::w::kSilk.withAlpha(0.7f)};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAtt;
    kit::v3::bridge::HelpKey help;
    bool updating = false;
    int boundBand = -1;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EqEditor)
};
