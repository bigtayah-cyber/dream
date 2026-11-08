#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>
#include "MasterDsp.h"
#include "../../Kit/BypassFade.h"

class MasterProcessor : public juce::AudioProcessor {
public:
    MasterProcessor();
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Orbe Master"; }
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

    mas::Params readParams() const;
    float raw(const char* id) const { return apvts.getRawParameterValue(id)->load(); }
    void resetAll();   // valeurs par défaut de mix.html (P0)

    juce::AudioProcessorValueTreeState apvts;
    // compteurs lus par l'interface (dB) : crête entrée/sortie, réduction compresseur/limiteur, LUFS momentané entrée/sortie
    std::atomic<float> inPk{0.f}, outPk{0.f}, grComp{0.f}, grLim{0.f}, lufsIn{-70.f}, lufsOut{-70.f};

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();
    mas::Engine engine;
    mas::BypassDelay byp;
    bypassfx::Fader fader;
    juce::AudioParameterBool* bypass = nullptr;
    int maxBlock = 2048;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MasterProcessor)
};
