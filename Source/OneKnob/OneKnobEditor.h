#pragma once
#include "../Kit/Kit.h"
#include "OneKnobProcessor.h"

// Interrupteur à voyant : allumé = effet en service.
class InSwitch : public juce::Button {
public:
    explicit InSwitch(const kit::Palette& p) : juce::Button("in"), P(p) { setClickingTogglesState(true); }
    void paintButton(juce::Graphics&, bool, bool) override;
private:
    kit::Palette P;
};

// Look du kit, avec polices de listes proportionnelles à la taille de la fenêtre.
class KeyLook : public kit::Look {
public:
    using kit::Look::Look;
    float u = 1.f;
    juce::Font getComboBoxFont(juce::ComboBox&) override { return kit::mono(11.f * u); }
    juce::Font getPopupMenuFont() override { return kit::fnt(13.f * u); }
    void positionComboBoxText(juce::ComboBox& cb, juce::Label& l) override { l.setBounds(6, 1, cb.getWidth() - 22, cb.getHeight() - 2); l.setFont(getComboBoxFont(cb)); }
};

class OneKnobEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit OneKnobEditor(OneKnobProcessor&);
    ~OneKnobEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    static kit::Palette palette(int voicingIndex);

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAtt = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    void timerCallback() override;
    void paintVu(juce::Graphics&, juce::Rectangle<float>);
    juce::Rectangle<float> S(float x, float y, float w, float h) const { return {x * u_, y * u_, w * u_, h * u_}; }
    juce::Rectangle<int> Si(float x, float y, float w, float h) const { return S(x, y, w, h).toNearestInt(); }

    OneKnobProcessor& proc;
    kit::Palette pal;
    KeyLook look;
    juce::TooltipWindow tooltip{this, 600};
    juce::Image plate;
    float u_ = 1.f;
    float needle = -20.f;  // en VU
    bool usesPitch = false;
    int tick = 0;  // horloge d'animation (voyant clignotant en recherche)

    juce::Slider knobS;
    InSwitch inSw;
    juce::ComboBox keyBox, scaleBox;
    std::unique_ptr<ComboAtt> keyAtt, scaleAtt;
    std::unique_ptr<SliderAtt> knobAtt;
    std::unique_ptr<ButtonAtt> inAtt;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OneKnobEditor)
};
