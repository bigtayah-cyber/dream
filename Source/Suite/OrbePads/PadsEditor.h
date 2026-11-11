#pragma once
#include "PadsProcessor.h"
#include "../../Kit/v3/InstrumentSkin.h"
#include "../../Kit/v3/NoteNames.h"

class PadsEditor : public juce::AudioProcessorEditor, public juce::FileDragAndDropTarget, private juce::Timer {
public:
    explicit PadsEditor(PadsProcessor&);
    ~PadsEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    bool isInterestedInFileDrag(const juce::StringArray& f) override;
    void filesDropped(const juce::StringArray& f, int, int) override { if (f.size()) loadFile(juce::File(f[0])); }
    static kit::Palette palette();
    static juce::Colour bankColour(int bank);
    void selectPad(int i);
    PadsProcessor& proc;
    float u_ = 1.f;
    float flash[16] = {};
    std::shared_ptr<const osamp::Bank> snap;
    std::vector<float> padPk[16];

private:
    class PadButton;
    kit::v3::UiAccidental acc{"orbepads"};   // dièses / bémols : réglage d'interface (Prefs)
    std::unique_ptr<kit::v3::UiSeg> accSeg;
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    void timerCallback() override;
    void loadFile(const juce::File&);
    void refreshBank();
    void paintWaveOver(juce::Graphics&, osamp::WaveView&);
    void waveDown(const juce::MouseEvent&);
    void waveDrag(const juce::MouseEvent&);
    void panel(juce::Graphics&, juce::Rectangle<float>, const juce::String& title);
    juce::Rectangle<float> S(float x, float y, float w, float h) const { return {x * u_, y * u_, w * u_, h * u_}; }
    juce::Rectangle<int> Si(float x, float y, float w, float h) const { return S(x, y, w, h).toNearestInt(); }

    kit::Palette pal;
    kit::Look look;
    juce::TooltipWindow tooltip{this, 600};
    kit::v3::bridge::HelpKey help;
    kit::v3::Skin skin{"orbepads", kit::v3::accents::ice};
    juce::OwnedArray<kit::v3::Knob3> k3;
    std::unique_ptr<kit::v3::Knob3> masterK3;
    void resized3(); void paint3(juce::Graphics&); void paintBackdrop3(juce::Graphics&, juce::Rectangle<float>);
    osamp::WaveView wave;
    kit::v3::GridSeg modeSeg, divSeg, playSeg, dltSeg, outSeg;
    std::unique_ptr<kit::v3::GridSeg> grpSeg;
    juce::Slider sens;
    std::unique_ptr<SliderAtt> sensAtt;
    kit::v3::bridge::PushKey importB{kit::u8("Importer")}, demoB{kit::u8("Démo")}, clearB{kit::u8("Effacer les repères")};
    juce::ToggleButton revB;
    std::unique_ptr<ButtonAtt> revAtt;
    std::unique_ptr<osamp::Knob> k[9];
    std::unique_ptr<osamp::Knob> masterK;
    std::unique_ptr<PadButton> pads[16];
    std::unique_ptr<juce::FileChooser> chooser;
    const osamp::Bank* snapRaw = nullptr;
    int lastRev = -1, dragMark = -1, tick = 0, statusTicks = 0, lastMode = -1;
    juce::String status;
    bool anyActive = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PadsEditor)
};
