#pragma once
#include "AtelierParams.h"

class AtelierProcessor : public juce::AudioProcessor {
public:
    AtelierProcessor();
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>& b, juce::MidiBuffer&) override { host.bypassed(b, getTotalNumInputChannels() >= 2); }
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Orbe Atelier"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }
    juce::AudioProcessorParameter* getBypassParameter() const override { return bypass; }
    int getNumPrograms() override { return atl::numPresets(); }
    int getCurrentProgram() override { return currentPreset(); }
    void setCurrentProgram(int i) override { applyPreset(i); }
    const juce::String getProgramName(int i) override { return atl::presetName(i); }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    int currentPreset() const { return (int)apvts.getRawParameterValue("preset")->load(); }
    void applyPreset(int i);       // charge les réglages de tous les modules (la tonalité et le mix restent)
    bool matchesPreset() const;
    static double presetValue(const juce::String& id, int preset);
    oc::S settings() const;
    float flag(const char* id) const { return apvts.getRawParameterValue(id)->load(); }

    juce::AudioProcessorValueTreeState apvts;
    oc::Host host;
private:
    static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();
    juce::AudioParameterBool* bypass = nullptr;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AtelierProcessor)
};
