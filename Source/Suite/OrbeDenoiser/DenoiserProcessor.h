#pragma once
#include "../OrbeDsp/OrbeFace.h"
#include "DenoiserDsp.h"

class DenoiserProcessor : public of::FxProcessor, private juce::AudioProcessorValueTreeState::Listener, private juce::AsyncUpdater {
public:
    DenoiserProcessor();
    ~DenoiserProcessor() override;
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    dn::Params readParams() const;
    void learn() { engine.startLearn(); }
    dn::Engine engine;
protected:
    void beforeSave() override;
    void afterLoad() override;
private:
    static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();
    void parameterChanged(const juce::String&, float) override;
    void handleAsyncUpdate() override;
    int maxBlock = 2048;
    double curSr = 0.0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DenoiserProcessor)
};
