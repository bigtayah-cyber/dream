// Orbe Mix : analyseur de mix temps réel, passthrough. LUFS BS.1770, LRA, pic vrai, corrélation, 7 bandes contre une cible de style.
#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>
#include "../OrbeAnalysis/OrbeAnalysis.h"

class MixProcessor : public juce::AudioProcessor {
public:
    MixProcessor();
    void prepareToPlay(double sampleRate, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Orbe Mix"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    juce::AudioProcessorParameter* getBypassParameter() const override { return bypass; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    void requestReset() { resetReq = true; }
    int resetEpoch() const { return epoch.load(); }
    oa::MixSnap snapshot() const { return pub.read(); }
    int profile() const { return (int)profileP->load(); }

    juce::AudioProcessorValueTreeState apvts;
private:
    static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();
    juce::AudioParameterBool* bypass = nullptr;
    std::atomic<float>* profileP = nullptr;
    oa::MixMeter meter;
    oa::SeqPub<oa::MixSnap> pub;
    std::atomic<bool> resetReq{false};
    std::atomic<int> epoch{0};
    double sr = 48000.0;
    int sincePub = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MixProcessor)
};
