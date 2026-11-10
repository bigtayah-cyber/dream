#pragma once
#include "../OrbeChain/ChainUi.h"
#include "../../Kit/v3/NoteNames.h"

class LooksProcessor : public juce::AudioProcessor {
public:
    LooksProcessor();
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>& b, juce::MidiBuffer&) override { host.bypassed(b, getTotalNumInputChannels() >= 2); }
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Orbe Looks"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 6.0; }
    juce::AudioProcessorParameter* getBypassParameter() const override { return bypass; }
    int getNumPrograms() override { return oc::numLooks(); }
    int getCurrentProgram() override { return currentLook(); }
    void setCurrentProgram(int i) override { applyLook(i); }
    const juce::String getProgramName(int i) override { return lookName(i); }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    static juce::String lookName(int i) { return kit::u8(oc::looks()[(size_t)juce::jlimit(0, oc::numLooks() - 1, i)].name); }
    int currentLook() const { return (int)apvts.getRawParameterValue("look")->load(); }
    void applyLook(int i);        // choisit le look et ramène les 4 macros à leurs valeurs d'origine
    bool macrosMoved() const;     // vrai si une macro a quitté la valeur d'origine du look
    static double macroDefault(const juce::String& id, int look);
    oc::S settings() const;

    juce::AudioProcessorValueTreeState apvts;
    oc::Host host;
private:
    static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();
    juce::AudioParameterBool* bypass = nullptr;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LooksProcessor)
};
