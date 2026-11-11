#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>
#include "OrbeSynthEngine.h"

class OrbeSynthProcessor : public juce::AudioProcessor {
public:
    OrbeSynthProcessor();

    void prepareToPlay(double sampleRate, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Orbe Synth"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

    int getNumPrograms() override { return (int)osyn::presets().size(); }
    int getCurrentProgram() override { return program; }
    void setCurrentProgram(int) override;
    const juce::String getProgramName(int i) override;
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    osyn::Params readParams() const;
    bool presetModified() const;  // vrai si un réglage s'écarte du preset courant

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keys;         // clavier à l'écran, et reflet des notes reçues de l'hôte
    std::atomic<float> bend{0.f}, mod{0.f};  // molettes (écran ou MIDI), -1..1 et 0..1
    std::atomic<int> voices{0};
    std::atomic<float> hostBpm{0.f};
    std::atomic<int> loadedVersion{3};   // version d'état lue (nouvelle instance : 3) ; < 3 : ancien son des oscillateurs

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();
    void handle(const juce::MidiMessage&);
    float v(const char* id) const { return apvts.getRawParameterValue(id)->load(); }

    osyn::Engine engine;
    int program = 0;
    double sr = 48000.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OrbeSynthProcessor)
};
