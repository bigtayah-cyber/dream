#pragma once
#include "../OrbeDsp/OrbeFace.h"
#include "RelanceDsp.h"

class RelanceProcessor : public of::FxProcessor {
public:
    RelanceProcessor();
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassChoice; }
    double getTailLengthSeconds() const override { return 2.5; }   // relances encore en cours après la dernière phrase
    int getNumPrograms() override;
    int getCurrentProgram() override { return program; }
    void setCurrentProgram(int) override;
    const juce::String getProgramName(int) override;

    rel::Params readParams() const;
    void writeParams(const rel::Params&);
    bool presetMatches(int i) const;
    const rel::Telemetry& tel() const { return engine.tel(); }
    static int numPresets();

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();
    void beforeSave() override;
    void afterLoad() override;
    rel::Engine engine;
    juce::RangedAudioParameter* bypassChoice = nullptr;
    int program = 0, maxBlock = 0, stateVer = 2;   // stateVer < 2 : projet d'avant l'harmonie diatonique
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RelanceProcessor)
};
