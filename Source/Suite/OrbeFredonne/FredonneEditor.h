#pragma once
#include "../OrbeAnalysis/OrbeUi.h"
#include "FredonneProcessor.h"

// Poignée pour glisser le fichier .mid vers une piste de l'hôte.
class MidiHandle : public juce::Component {
public:
    MidiHandle(FredonneProcessor& p, const kit::Palette& pal) : proc(p), P(pal) { setMouseCursor(juce::MouseCursor::DraggingHandCursor); }
    void paint(juce::Graphics&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override { dragging = false; repaint(); }
    bool enabledLook = false;
private:
    FredonneProcessor& proc; kit::Palette P; bool dragging = false;
};

class FredonneEditor : public ou::Base {
public:
    explicit FredonneEditor(FredonneProcessor&);
private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAtt = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    void layout() override;
    void body(juce::Graphics&) override;
    void timerCallback() override;
    void paintRoll(juce::Graphics&, juce::Rectangle<float> r, float u);

    FredonneProcessor& proc;
    std::shared_ptr<const FredonneProcessor::Take> take;
    int state = 0; double recSec = 0; float level = 0.f, cents = 0.f; int liveNote = -1; double liveHz = 0, lastLive = 0;
    juce::Slider sensK, minK, bpmK;
    juce::ComboBox keyBox, modeBox;
    std::unique_ptr<ou::Seg> gridSeg, octSeg;
    juce::ToggleButton snapT, tuneT, followT, playT, monT;
    juce::TextButton recB, clearB;
    std::unique_ptr<MidiHandle> handle;
    std::unique_ptr<SliderAtt> sensA, minA, bpmA;
    std::unique_ptr<ComboAtt> keyA, modeA;
    std::unique_ptr<ButtonAtt> snapA, tuneA, followA, playA, monA;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FredonneEditor)
};
