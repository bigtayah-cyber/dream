#pragma once
#include "../OrbeDsp/OrbeFace.h"
#include "AccrocDsp.h"

class AccrocProcessor : public of::FxProcessor {
public:
    AccrocProcessor();
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool acceptsMidi() const override { return true; }
    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassChoice; }
    int getNumPrograms() override;
    int getCurrentProgram() override { return program; }
    void setCurrentProgram(int) override;
    const juce::String getProgramName(int) override;

    acr::Params readParams() const;
    void writeParams(const acr::Params&);
    // Séquence : modifiée par l'interface (fil de message), transmise au moteur sans attente côté audio.
    const acr::Seq& seq() const { return seq_; }
    void setSeq(const acr::Seq&);
    int seqVersion() const { return seqVer; }
    // Pads de l'interface : vélocité (0 = relâché), lue au bloc suivant.
    void pad(int type, float vel) { if (type >= 0 && type < acr::kNumTypes) padWant[(size_t)type].store(vel); }
    int brush = acr::Stutter;   // pinceau du séquenceur (interface), sauvegardé dans l'état

    int stepNow() const { return uiStep.load(); }
    float activity(int t) const { return uiAct[(size_t)juce::jlimit(0, acr::kNumTypes - 1, t)].load(); }
    double bpmNow() const { return uiBpm.load(); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();
    void beforeSave() override;
    void afterLoad() override;
    acr::Engine engine;
    acr::Seq seq_;
    juce::RangedAudioParameter* bypassChoice = nullptr;
    int seqVer = 0, program = 0, maxBlock = 0;
    std::array<acr::Ev, acr::Engine::kMaxEv> evs{};
    std::array<std::atomic<float>, acr::kNumTypes> padWant{};
    std::array<float, acr::kNumTypes> padHave{};
    std::atomic<int> uiStep{-1};
    std::array<std::atomic<float>, acr::kNumTypes> uiAct{};
    std::atomic<double> uiBpm{120.0};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AccrocProcessor)
};
