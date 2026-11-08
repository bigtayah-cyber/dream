#pragma once
#include "../OrbeDsp/OrbeFace.h"
#include "DenoiserDsp.h"

class DenoiserProcessor : public of::FxProcessor {
public:
    DenoiserProcessor();
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
    int maxBlock = 2048;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DenoiserProcessor)
};
