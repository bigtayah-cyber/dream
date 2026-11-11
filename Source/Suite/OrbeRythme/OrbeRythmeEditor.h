#pragma once
#include "OrbeRythmeProcessor.h"
#include "../../Kit/v3/Rack3.h"
#include "../../Kit/v3/Face3.h"

// Grille des frappes : 7 lignes de batterie x (mesures x 16) pas. Clic : pose / retire (glisser pour peindre) ; Maj + clic : vélocité.
class RythmeGrid : public juce::Component {
public:
    explicit RythmeGrid(OrbeRythmeProcessor& p) : proc(p) { setTitle("Grille de batterie"); setWantsKeyboardFocus(true); }
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override { mode = 0; lastRow = lastStep = -1; }
    const kit::v3::Theme* T = nullptr;
private:
    juce::Rectangle<float> cells() const;
    bool locate(juce::Point<float>, int& row, int& step) const;
    void apply(int row, int step);
    OrbeRythmeProcessor& proc;
    int mode = 0, lastRow = -1, lastStep = -1;   // mode : 1 pose, 2 efface
};

// Poignée de glisser-déposer du fichier MIDI (batterie) vers une piste du DAW.
class RythmeHandle : public juce::Component {
public:
    explicit RythmeHandle(OrbeRythmeProcessor& p) : proc(p) { setMouseCursor(juce::MouseCursor::DraggingHandCursor); setTitle("Glisser le MIDI"); }
    void paint(juce::Graphics&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override { dragging = false; repaint(); }
    const kit::v3::Theme* T = nullptr;
private:
    OrbeRythmeProcessor& proc;
    bool dragging = false;
};

class OrbeRythmeEditor : public juce::AudioProcessorEditor, public juce::DragAndDropContainer, private juce::Timer {
public:
    explicit OrbeRythmeEditor(OrbeRythmeProcessor&);
    ~OrbeRythmeEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress&) override;
    static constexpr float kW = 1000.f, kH = 580.f;

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAtt = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    void timerCallback() override;
    void paintBackdrop(juce::Graphics&, juce::Rectangle<float>);
    juce::Rectangle<float> S(float x, float y, float w, float h) const { return {x * u_, y * u_, w * u_, h * u_}; }
    juce::Rectangle<int> Si(float x, float y, float w, float h) const { return S(x, y, w, h).toNearestInt(); }

    OrbeRythmeProcessor& proc;
    kit::Palette pal;
    kit::v3::Face face{"orberythme", kit::v3::accents::peach, kW, kH};
    juce::TooltipWindow tooltip{this, 600};
    kit::v3::bridge::HelpKey help;
    float u_ = 1.f;
    juce::OwnedArray<kit::v3::Knob3> k3;
    juce::ComboBox styleBox;
    std::unique_ptr<ComboAtt> styleAtt;
    std::unique_ptr<kit::v3::bridge::Seg> barsSeg, muteSeg, midiSeg;
    kit::v3::bridge::PushKey auditionB{kit::u8("Écouter")}, genB{kit::u8("Générer")}, undoB{"Annuler"}, redoB{kit::u8("Rétablir")};
    RythmeGrid grid;
    RythmeHandle handle;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OrbeRythmeEditor)
};
