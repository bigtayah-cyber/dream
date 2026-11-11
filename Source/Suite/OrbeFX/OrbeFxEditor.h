#pragma once
#include "OrbeFxProcessor.h"
#include "../../Kit/v3/Rack3.h"
#include "../../Kit/v3/Face3.h"

// Écran : réponse du filtre (courbe) et crêtes d'entrée / de sortie.
class FxScreen : public juce::Component {
public:
    explicit FxScreen(OrbeFxProcessor& p) : proc(p) { setTitle("Réponse du filtre et niveaux"); setInterceptsMouseClicks(false, false); }
    void paint(juce::Graphics&) override;
    void tick();
    const kit::v3::Theme* T = nullptr;
private:
    OrbeFxProcessor& proc;
    float inDb = -90.f, outDb = -90.f, inHold = -90.f, outHold = -90.f;
};

class OrbeFxEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit OrbeFxEditor(OrbeFxProcessor&);
    ~OrbeFxEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    static constexpr float kW = 1100.f, kH = 700.f;

private:
    using ComboAtt = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    void timerCallback() override;
    void paintBackdrop(juce::Graphics&, juce::Rectangle<float>);
    void gotoPreset(int i);
    juce::Rectangle<int> Si(float x, float y, float w, float h) const { return juce::Rectangle<float>(x * u_, y * u_, w * u_, h * u_).toNearestInt(); }
    void addKnob(const char* id, const juce::String& lab, const juce::String& tip, kit::v3::KnobSize sz, bool peach = false, bool bipolar = false);
    void addCombo(juce::ComboBox&, std::unique_ptr<ComboAtt>&, const char* id, const juce::String& tip);

    OrbeFxProcessor& proc;
    kit::Palette pal;
    kit::v3::Face face{"orbefx", kit::v3::accents::ice, kW, kH};
    juce::TooltipWindow tooltip{this, 600};
    kit::v3::bridge::HelpKey help;
    float u_ = 1.f;
    juce::OwnedArray<kit::v3::Knob3> k3;
    juce::StringArray knobIds;
    juce::OwnedArray<kit::v3::bridge::Seg> segs;   // on/off des 5 modules, puis ping-pong
    juce::ComboBox presetBox, modeBox, typeBox, timeBox;
    std::unique_ptr<ComboAtt> modeAtt, typeAtt, timeAtt;
    kit::v3::Chevron prevB{true}, nextB{false};
    std::unique_ptr<kit::v3::InSwitch> inSw;
    FxScreen screen;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OrbeFxEditor)
};
