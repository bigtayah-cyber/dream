#pragma once
// Orbe FX : effet audio (chaîne filtre / saturation / chorus / delay / réverbe, 4 macros, 12 préréglages). DSP : OrbeFxDsp.h.
// Remplace l'ancien « Orbe FX » (le moteur d'Orbe déclaré comme effet, qui ignorait l'audio). Mêmes identifiants de plugin (OrbF, com.dreamaws.orbefx).
#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>
#include "OrbeFxDsp.h"
#include "../../Kit/BypassFade.h"

class OrbeFxProcessor : public juce::AudioProcessor {
public:
    OrbeFxProcessor();
    void prepareToPlay(double sampleRate, int maxBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& l) const override {
        return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() && (l.getMainInputChannelSet() == juce::AudioChannelSet::stereo() || l.getMainInputChannelSet() == juce::AudioChannelSet::mono());
    }
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}   // latence nulle : l'entrée traverse telle quelle
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Orbe FX"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override;
    juce::AudioProcessorParameter* getBypassParameter() const override { return bypass; }
    int getNumPrograms() override;
    int getCurrentProgram() override { return program; }
    void setCurrentProgram(int) override;
    const juce::String getProgramName(int) override;
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    static int numPresets();
    bool presetMatches(int i) const;
    ofx::Params readParams() const;
    float raw(const char* id) const { return apvts.getRawParameterValue(id)->load(); }

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float> inPeak{0.f}, outPeak{0.f};   // crêtes depuis la dernière lecture de l'éditeur (remises à zéro par lui)
    std::atomic<float> hostBpm{0.f};

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();
    ofx::Engine engine;
    bypassfx::Fader fader;
    juce::RangedAudioParameter* bypass = nullptr;
    int program = 0;
    double sr = 48000.0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OrbeFxProcessor)
};
