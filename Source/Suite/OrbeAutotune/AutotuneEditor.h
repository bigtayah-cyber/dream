#pragma once
#include "../../Kit/Widgets.h"
#include "AutotuneProcessor.h"
#include "../OrbeDsp/OrbeFace.h"

// Clavier d'une octave : le masque de notes de la gamme, éditable à la main (clic = inverser la note).
class AtKeys : public juce::Component {
public:
    explicit AtKeys(AutotuneProcessor& p) : proc(p) {}
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override { setMouseCursor(juce::MouseCursor::PointingHandCursor); }
    int sung = -1, root = -1;
    int mask = 0, mode = 2;
private:
    int keyAt(juce::Point<float>) const;
    AutotuneProcessor& proc;
};

class AutotuneEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit AutotuneEditor(AutotuneProcessor&);
    ~AutotuneEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    static kit::Palette palette();

private:
    void timerCallback() override;
    void paintScreen(juce::Graphics&, juce::Rectangle<float>);
    void paintGauge(juce::Graphics&, juce::Rectangle<float>);
    void paintReadout(juce::Graphics&, juce::Rectangle<float>);
    void module(juce::Graphics&, const char* title, float x, float w, float y);
    juce::String tonText() const;
    juce::Rectangle<float> S(float x, float y, float w, float h) const { return {x * u_, y * u_, w * u_, h * u_}; }
    juce::Rectangle<int> Si(float x, float y, float w, float h) const { return S(x, y, w, h).toNearestInt(); }

    AutotuneProcessor& proc;
    kit::Palette pal;
    kit::Look look;
    juce::TooltipWindow tooltip{this, 600};
    float u_ = 1.f, viewC = 57.f, dispCorr = 0.f;
    int tMode = 2, tRoot = -1, tScale = 1, tFlips = 0, tMask = 0;
    float lastDet = 0.f, lastTgt = 0.f;
    juce::OwnedArray<kit::w::Pot> pots;
    juce::ComboBox keyBox, scaleBox, harmBox, presetBox;
    juce::ToggleButton formToggle;
    std::unique_ptr<of::Seg> latSeg;   // Latence : Direct / Qualité
    int lastLat = -1, lastLatDecl = -1;
    kit::w::PushKey resetNotes;
    AtKeys keys;
    kit::w::Power power;
    kit::w::HelpKey help;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> keyAtt, scaleAtt, harmAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> formAtt, powerAtt;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AutotuneEditor)
};
