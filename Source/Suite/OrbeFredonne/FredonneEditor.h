#pragma once
#include "../OrbeAnalysis/OrbeUi.h"
#include "FredonneProcessor.h"
#include "../../Kit/v3/InstrumentSkin.h"

// Poignée pour glisser le fichier .mid vers une piste de l'hôte.
class MidiHandle : public juce::Component {
public:
    MidiHandle(FredonneProcessor& p, const kit::Palette& pal) : proc(p), P(pal) { setMouseCursor(juce::MouseCursor::DraggingHandCursor); }
    void paint(juce::Graphics&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override { dragging = false; repaint(); }
    bool enabledLook = false;
    const kit::v3::Theme* T = nullptr;   // nul ou Classique : rendu d'origine
private:
    FredonneProcessor& proc; kit::Palette P; bool dragging = false;
};

// L'éditeur ne dérive plus de ou::Base (dont paint et resized sont figés) : même boîtier en Classique, rendu v3 sinon.
class FredonneEditor : public juce::AudioProcessorEditor, public juce::DragAndDropContainer, private juce::Timer {
public:
    explicit FredonneEditor(FredonneProcessor&);
    ~FredonneEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    using Colour = juce::Colour; using String = juce::String;
    static constexpr float kHeader = 50.f, kSide = 16.f;
    float W = 1000.f, H = 640.f, u_ = 1.f;
    kit::Palette pal; kit::w::ScaledLook look;
    kit::Display disp; Colour scr;
    juce::TooltipWindow tooltip{this, 600};
    juce::Rectangle<float> content;
    kit::v3::bridge::HelpKey help;
    kit::v3::Skin skin{"orbefredonne", kit::v3::accents::lilac};
    juce::OwnedArray<kit::v3::Knob3> k3;
    void well(juce::Graphics& g, juce::Rectangle<float> r) const { kit::screenWell(g, pal, r, disp); }
    juce::Rectangle<float> R(float x, float y, float w, float h) const { return {x * u_, y * u_, w * u_, h * u_}; }
    juce::Rectangle<int> Ri(float x, float y, float w, float h) const { return R(x, y, w, h).toNearestInt(); }
    void layout3(); void paint3(juce::Graphics&); void paintBackdrop3(juce::Graphics&, juce::Rectangle<float>); void paintRoll3(juce::Graphics&, juce::Rectangle<float>, float u);
    using SliderAtt0 = juce::AudioProcessorValueTreeState::SliderAttachment;
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAtt = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    void layout();
    void body(juce::Graphics&);
    void timerCallback() override;
    void paintRoll(juce::Graphics&, juce::Rectangle<float> r, float u);

    FredonneProcessor& proc;
    std::shared_ptr<const FredonneProcessor::Take> take;
    int state = 0; double recSec = 0; float level = 0.f, cents = 0.f; int liveNote = -1; double liveHz = 0, lastLive = 0;
    juce::Slider sensK, minK, bpmK;
    juce::ComboBox keyBox, modeBox;
    std::unique_ptr<kit::v3::bridge::Seg> accSeg;   // # / b
    bool flat_ = false;
    std::unique_ptr<kit::v3::GridSeg> gridSeg, octSeg;
    juce::ToggleButton snapT, tuneT, followT, playT, monT;
    kit::v3::bridge::PushKey recB, clearB;
    std::unique_ptr<MidiHandle> handle;
    std::unique_ptr<SliderAtt> sensA, minA, bpmA;
    std::unique_ptr<ComboAtt> keyA, modeA;
    std::unique_ptr<ButtonAtt> snapA, tuneA, followA, playA, monA;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FredonneEditor)
};
