#pragma once
#include "../OrbeDsp/OrbeFace.h"
#include "EqDsp.h"

class EqProcessor : public of::FxProcessor {
public:
    EqProcessor();
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;

    int getNumPrograms() override { return eq::kPresets; }
    int getCurrentProgram() override { return preset; }
    void setCurrentProgram(int i) override { applyPreset(i); }
    const juce::String getProgramName(int i) override { return presetName(i); }
    static juce::String presetName(int i) { return juce::String::fromUTF8(eq::presets()[juce::jlimit(0, eq::kPresets - 1, i)].name); }

    eq::Params readParams() const;
    void applyPreset(int i);
    bool matchesPreset() const;
    int currentPreset() const { return preset; }
    void resetBand(int i);
    int abIndex() const { return ab; }
    void switchAB(int to);
    float rangeDb() const { return raw("range") > 0.5f ? 24.f : 12.f; }
    juce::RangedAudioParameter* par(int band, const char* what) const { return apvts.getParameter("b" + juce::String(band + 1) + what); }
    void setReal(juce::RangedAudioParameter* p, float v) const { p->setValueNotifyingHost(p->convertTo0to1(v)); }

    odsp::Analyzer analyzer;
protected:
    void beforeSave() override;
    void afterLoad() override;
private:
    static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();
    std::vector<juce::RangedAudioParameter*> abParams;
    std::vector<float> stash[2];
    int ab = 0, preset = 0, maxBlock = 2048;
    eq::Engine engine;
    std::array<std::array<std::atomic<float>*, 5>, eq::kBands> rawPtr{};   // f, g, q, type, on de chaque bande
    std::atomic<float>* outPtr = nullptr;
    std::vector<float> mono;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EqProcessor)
};
