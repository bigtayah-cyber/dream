#pragma once
#include "../OrbeDsp/OrbeFace.h"
#include "DeEsserDsp.h"

class DeEsserProcessor : public of::FxProcessor {
public:
    DeEsserProcessor();
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    des::Params readParams() const;
    odsp::Analyzer inAn, outAn;          // spectres d'entrée et de sortie (lus par l'interface)
    std::atomic<float> gr{0.f};          // réduction de gain maximale du dernier bloc (dB, positive)
private:
    static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();
    des::Engine engine;
    std::vector<float> monoIn, monoOut;
    int maxBlock = 2048;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DeEsserProcessor)
};
