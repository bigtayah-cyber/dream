#pragma once
#include "../OrbeDsp/OrbeFace.h"
#include "DeEsserDsp.h"

class DeEsserProcessor : public of::FxProcessor, private juce::AudioProcessorValueTreeState::Listener, private juce::AsyncUpdater {
public:
    DeEsserProcessor();
    ~DeEsserProcessor() override;
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    des::Params readParams() const;
    odsp::Analyzer inAn, outAn;          // spectres d'entrée et de sortie (lus par l'interface)
    void startAuto() { engine.startAuto(); }
    float autoProgress() const { return engine.autoProgress(); }
    float pollAutoThreshold();           // seuil proposé quand l'écoute de 5 s est finie, sinon 1 (valeur impossible)
    std::atomic<float> gr{0.f};          // réduction de gain maximale du dernier bloc (dB, positive)
private:
    static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();
    des::Engine engine;
    std::vector<float> monoIn, monoOut;
    void parameterChanged(const juce::String&, float) override;
    void handleAsyncUpdate() override;
    double curSr = 0.0;
    int maxBlock = 2048;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DeEsserProcessor)
};
