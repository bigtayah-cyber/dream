#pragma once
#include "../../Kit/Kit.h"
#include "CabineProcessor.h"

// Potard dont le double-clic ramène à la valeur du preset en cours.
class CabSlider : public juce::Slider {
public:
    std::function<double()> resetTo;
    void mouseDoubleClick(const juce::MouseEvent&) override { if (resetTo) setValue(resetTo(), juce::sendNotificationSync); }
};

// Potard + nom gravé + afficheur de valeur.
class CabPot : public juce::Component {
public:
    CabPot(juce::AudioProcessorValueTreeState&, const juce::String& id, const juce::String& label, const juce::String& tip, std::function<double()> reset);
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    CabSlider s;
    juce::String label;
    juce::RangedAudioParameter* par = nullptr;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};

// Interrupteur « IN » à voyant : allumé = effet en service.
class CabPower : public juce::Button {
public:
    explicit CabPower(const kit::Palette& p) : juce::Button("in"), P(p) { setClickingTogglesState(true); }
    void paintButton(juce::Graphics&, bool, bool) override;
private:
    kit::Palette P;
};

class CabineEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit CabineEditor(CabineProcessor&);
    ~CabineEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseUp(const juce::MouseEvent&) override;
    static kit::Palette palette();

private:
    void timerCallback() override;
    void paintVu(juce::Graphics&, juce::Rectangle<float>);
    void paintLadder(juce::Graphics&, juce::Rectangle<float>, const char* title, const float* thr, const char* const* lab, float value, juce::Colour hot);
    void section(juce::Graphics&, const char* title, float y);
    void showMenu();
    void step(int d) { proc.applyPreset((proc.currentPreset() + d + cab::kPresets) % cab::kPresets); }
    juce::Rectangle<float> S(float x, float y, float w, float h) const { return {x * u_, y * u_, w * u_, h * u_}; }
    juce::Rectangle<int> Si(float x, float y, float w, float h) const { return S(x, y, w, h).toNearestInt(); }

    CabineProcessor& proc;
    kit::Palette pal;
    kit::Look look;
    juce::TooltipWindow tooltip{this, 600};
    juce::Image plate;
    float u_ = 1.f, needle = -20.f, grDisp = 0.f, pkDisp = 0.f;
    bool modified = false;
    juce::OwnedArray<CabPot> pots;
    juce::ArrowButton prev{"prev", 0.5f, juce::Colour(0xFFCFC6FF)}, next{"next", 0.f, juce::Colour(0xFFCFC6FF)};
    CabPower power;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAtt;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CabineEditor)
};
