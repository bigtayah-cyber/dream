#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "Engine.h"
#include "Kit/BypassFade.h"
#include "Kit/Sanitize.h"

class VoileProcessor : public juce::AudioProcessor {
public:
    VoileProcessor();

    void prepareToPlay(double sampleRate, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Voile"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 3.5; }
    juce::AudioProcessorParameter* getBypassParameter() const override { return bypass; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return current; }
    void setCurrentProgram(int i) override;
    const juce::String getProgramName(int i) override;
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float> level{0.f}, shift{0.f};  // pour l'interface

    // Historique d'affichage (écran HAUTEUR) : une entrée toutes les 4 ms, écrite par le fil audio sans verrou ni allocation.
    // sung / out / tgt : notes MIDI (0 = rien de lisible) ; lvl : niveau (enveloppe). Aucune influence sur le son.
    struct Scope {
        static constexpr int N = 1024;
        static constexpr float period = 0.004f;
        std::atomic<float> sung[N], out[N], tgt[N], lvl[N];
        std::atomic<int> wr{0}, key{0}, scale{1};
        Scope() { for (int i = 0; i < N; ++i) sung[i] = out[i] = tgt[i] = lvl[i] = 0.f; }
    } scope;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();
    void setParam(const char* id, float v);

    // bypass : sortie = entrée retardée de la latence, fondu de 10 ms ; le moteur tourne toujours (queues à jour, aucun fantôme)
    void run(juce::AudioBuffer<float>&, bool bypassed);

    voile::Engine engine;
    bypassfx::Fader fader;
    juce::RangedAudioParameter* bypass = nullptr;  // choix à 2 valeurs (0 actif, 1 contourné), déclaré en dernier
    int current = 0;
    double scopeAcc = 0.0, scopeSr = 48000.0;   // secondes non encore converties en entrées d'historique
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VoileProcessor)
};
