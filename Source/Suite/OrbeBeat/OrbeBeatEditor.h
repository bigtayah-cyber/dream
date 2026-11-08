#pragma once
#include "../Orbe808/SuiteUi.h"
#include "OrbeBeatProcessor.h"

// Grille de pas : 8 pistes × 16 ou 32 pas. Outils : dessiner, roll, vélocité, note (808).
class OrbeBeatGrid : public juce::Component {
public:
    explicit OrbeBeatGrid(OrbeBeatProcessor& p) : proc(p) {}
    enum Tool { Draw = 0, Roll, Vel, Note };
    Tool tool = Draw;
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override { paintMode = 0; }
private:
    bool locate(juce::Point<float>, int& lane, int& step) const;
    void apply(int lane, int step, bool first, bool alt);
    OrbeBeatProcessor& proc;
    int paintMode = 0;  // 0 rien, 1 pose, 2 efface
};

class OrbeBeatEditor : public juce::AudioProcessorEditor, public juce::DragAndDropContainer, private juce::Timer {
public:
    explicit OrbeBeatEditor(OrbeBeatProcessor&);
    ~OrbeBeatEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    static kit::Palette palette();

    static constexpr float kW = 1100.f, kH = 640.f, kGridX = 316.f, kGridY = 154.f, kGridW = 760.f, kRow = 34.f;

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAtt = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    void timerCallback() override;
    void paintHeader(juce::Graphics&, float u);
    juce::Rectangle<float> S(float x, float y, float w, float h) const { return {x * u_, y * u_, w * u_, h * u_}; }
    juce::Rectangle<int> Si(float x, float y, float w, float h) const { return S(x, y, w, h).toNearestInt(); }

    OrbeBeatProcessor& proc;
    kit::Palette pal;
    kit::Look look;
    juce::TooltipWindow tooltip{this, 600};
    kit::w::HelpKey help;
    float u_ = 1.f;

    OrbeBeatGrid grid;
    juce::ComboBox kitBox, keyBox;
    std::unique_ptr<suiteui::Segmented> patSeg, stepsSeg, chainSeg, midiSeg, modeSeg;
    kit::w::PushKey genB{kit::u8("Générer")}, varyB{"Varier"}, clearB{"Vider"}, auditionB{kit::u8("Écouter")};
    kit::w::PushKey toolB[4];
    juce::ToggleButton seqB;
    juce::Slider swingK, masterK, vol[obeat::kLanes];
    kit::w::PushKey muteB[obeat::kLanes], soloB[obeat::kLanes];
    std::unique_ptr<suiteui::DragHandle> handle;
    std::unique_ptr<ComboAtt> kitAtt, keyAtt;
    std::unique_ptr<SliderAtt> swingAtt, masterAtt, volAtt[obeat::kLanes];
    std::unique_ptr<ButtonAtt> muteAtt[obeat::kLanes], soloAtt[obeat::kLanes], seqAtt;
    float meterShown = 0.f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OrbeBeatEditor)
};
