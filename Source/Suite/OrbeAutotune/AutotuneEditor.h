#pragma once
#include "../../Kit/Kit.h"
#include "AutotuneProcessor.h"
#include "../OrbeDsp/OrbeFace.h"

// Potard + nom gravé + afficheur de valeur. Double-clic : valeur d'origine.
class AtPot : public juce::Component {
public:
    AtPot(juce::AudioProcessorValueTreeState&, const juce::String& id, const juce::String& label, const juce::String& tip);
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    juce::Slider s;
    juce::String label;
    juce::RangedAudioParameter* par = nullptr;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};

// Interrupteur « IN » à voyant : allumé = effet en service.
class AtPower : public juce::Button {
public:
    explicit AtPower(const kit::Palette& p) : juce::Button("in"), P(p) { setClickingTogglesState(true); }
    void paintButton(juce::Graphics&, bool, bool) override;
private:
    kit::Palette P;
};

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
    juce::Image plate;
    float u_ = 1.f, viewC = 57.f, dispCorr = 0.f;
    int tMode = 2, tRoot = -1, tScale = 1, tFlips = 0, tMask = 0;
    float lastDet = 0.f, lastTgt = 0.f;
    juce::OwnedArray<AtPot> pots;
    juce::ComboBox keyBox, scaleBox, harmBox, presetBox;
    juce::ToggleButton formToggle;
    std::unique_ptr<of::Seg> latSeg;   // Latence : Direct / Qualité
    int lastLat = -1, lastLatDecl = -1;
    juce::TextButton resetNotes;
    AtKeys keys;
    AtPower power;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> keyAtt, scaleAtt, harmAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> formAtt, powerAtt;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AutotuneEditor)
};
