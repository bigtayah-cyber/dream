#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>
#include "CabineDsp.h"
#include "../../Kit/BypassFade.h"

class CabineProcessor : public juce::AudioProcessor {
public:
    CabineProcessor();
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Orbe Cabine"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }
    juce::AudioProcessorParameter* getBypassParameter() const override { return bypass; }

    int getNumPrograms() override { return cab::kPresets; }
    int getCurrentProgram() override { return currentPreset(); }
    void setCurrentProgram(int i) override { applyPreset(i); }
    const juce::String getProgramName(int i) override { return presetName(i); }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    static juce::String presetName(int i) { return juce::String::fromUTF8(cab::presets()[juce::jlimit(0, cab::kPresets - 1, i)].name); }
    int currentPreset() const { return (int)apvts.getRawParameterValue("preset")->load(); }
    void applyPreset(int i);            // fixe le choix de preset et tous les potards
    bool matchesPreset() const;         // vrai tant qu'aucun potard n'a bougé depuis le preset
    cab::Params readParams() const;

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float> level{0.f}, peak{0.f}, reduction{0.f};  // niveau efficace (VU), crête, réduction de gain (dB)
    static float presetValue(const juce::String& id, int preset);   // valeur d'un potard dans un preset

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();
    cab::Engine engine;
    cab::BypassDelay byp;
    bypassfx::Fader fader;
    juce::AudioParameterBool* bypass = nullptr;
    int maxBlock = 2048;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CabineProcessor)
};
