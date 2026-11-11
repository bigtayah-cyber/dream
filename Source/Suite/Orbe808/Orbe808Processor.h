#pragma once
#include <atomic>
#include <juce_audio_utils/juce_audio_utils.h>
#include "Orbe808Engine.h"

class Orbe808Processor : public juce::AudioProcessor,
                         private juce::AudioProcessorValueTreeState::Listener,
                         private juce::Timer {
public:
    Orbe808Processor();
    ~Orbe808Processor() override;

    void prepareToPlay(double sampleRate, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Orbe 808"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }
    int getNumPrograms() override { return o808::kNumPresets; }
    int getCurrentProgram() override;
    void setCurrentProgram(int) override;
    const juce::String getProgramName(int) override;
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    static juce::String presetLabel(int i);
    static o808::Params paramsFrom(const juce::AudioProcessorValueTreeState&, int stateVersion = 3);

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keys;           // clavier de l'interface
    std::atomic<int> sounding{-1};          // note audible (-1 = aucune)
    std::atomic<int> lastNote{33};
    std::atomic<float> meter{0.f};          // crête du dernier bloc
    std::atomic<int> loadedVersion{3};      // version d'état lue (nouvelle instance : 3) ; < 3 : saturation « Repliée » d'origine

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();
    void parameterChanged(const juce::String& id, float) override;
    void timerCallback() override;
    void applyPreset(int idx);

    o808::Engine engine;
    std::vector<float> mono;
    std::atomic<bool> presetDirty{false}, knobTouched{false};
    bool applying = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Orbe808Processor)
};
