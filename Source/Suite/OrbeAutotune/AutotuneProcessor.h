#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "AutotuneDsp.h"
#include "../../Kit/BypassFade.h"

class AutotuneProcessor : public juce::AudioProcessor {
public:
    AutotuneProcessor();
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Orbe Autotune"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    juce::AudioProcessorParameter* getBypassParameter() const override { return bypass; }

    int getNumPrograms() override { return atn::kPresets; }
    int getCurrentProgram() override { return prog; }
    void setCurrentProgram(int i) override;
    const juce::String getProgramName(int i) override { return juce::String::fromUTF8(atn::presets()[juce::jlimit(0, atn::kPresets - 1, i)].name); }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    atn::Params readParams() const;
    float raw(const char* id) const { return apvts.getRawParameterValue(id)->load(); }
    bool presetMatches(int i) const;   // les réglages actuels sont-ils ceux du preset i ?
    void toggleNote(int pc);           // inverse la note par rapport à la gamme (masque éditable)
    void clearNotes();
    const atn::Telemetry& tel() const { return engine.tel; }

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();
    void setPlain(const char* id, float v);
    atn::Engine engine;
    atn::BypassDelay byp;
    bypassfx::Fader fader, faderIdle;   // l'un pour le mode actif, l'autre (latence de l'autre mode) tenu chaud pour basculer sans trou
    bool faderIsLow = false;
    int latLow = 0, latQual = 0;
    juce::AudioParameterBool* bypass = nullptr;
    juce::AudioBuffer<float> scratch;
    int prog = 2;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AutotuneProcessor)
};
