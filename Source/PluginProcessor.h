#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "Engine.h"

class VoileProcessor : public juce::AudioProcessor {
public:
    VoileProcessor();

    void prepareToPlay(double sampleRate, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Voile"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 3.5; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return current; }
    void setCurrentProgram(int i) override;
    const juce::String getProgramName(int i) override;
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float> level{0.f}, shift{0.f};  // pour l'interface

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();
    void setParam(const char* id, float v);

    voile::Engine engine;
    voile::Delay bypassL, bypassR;  // garde la même latence quand le plugin est en bypass
    int current = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VoileProcessor)
};
