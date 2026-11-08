#pragma once
#include "SampleProcessor.h"

class SampleEditor : public juce::AudioProcessorEditor, public juce::FileDragAndDropTarget, private juce::Timer {
public:
    explicit SampleEditor(SampleProcessor&);
    ~SampleEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    bool isInterestedInFileDrag(const juce::StringArray& f) override;
    void filesDropped(const juce::StringArray& f, int, int) override { if (f.size()) loadFile(juce::File(f[0])); }
    static kit::Palette palette();
    SampleProcessor& proc;

private:
    class Chip;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    void timerCallback() override;
    void loadFile(const juce::File&);
    void panel(juce::Graphics&, juce::Rectangle<float>, const juce::String& title);
    void paintSrcOver(juce::Graphics&, osamp::WaveView&);
    void paintResOver(juce::Graphics&, osamp::WaveView&);
    void srcDrag(const juce::MouseEvent&, bool start);
    juce::Rectangle<float> S(float x, float y, float w, float h) const { return {x * u_, y * u_, w * u_, h * u_}; }
    juce::Rectangle<int> Si(float x, float y, float w, float h) const { return S(x, y, w, h).toNearestInt(); }

    float u_ = 1.f;
    kit::Palette pal;
    kit::Look look;
    juce::TooltipWindow tooltip{this, 600};
    juce::Image plate;
    osamp::WaveView srcWave, resWave;
    osamp::Seg modeSeg, chopSeg;
    std::unique_ptr<osamp::Knob> k[9];
    std::unique_ptr<Chip> chip[9];
    juce::ToggleButton revB, tapeB, loopB, syncB, followB;
    std::unique_ptr<ButtonAtt> aRev, aTape, aLoop, aSync, aFollow;
    juce::TextButton importB{kit::u8("Importer")}, demoB{kit::u8("Démo")}, playB{kit::u8("Lecture")};
    std::unique_ptr<juce::FileChooser> chooser;
    std::shared_ptr<const osamp::Rendered> res;
    const osamp::Rendered* resRaw = nullptr;
    int lastRev = -1, drag = 0, tick = 0, statusTicks = 0, lastPreset = -2, lastSlice = -2;
    float anchor = 0.f;
    bool wasPlaying = false;
    juce::String status;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SampleEditor)
};
