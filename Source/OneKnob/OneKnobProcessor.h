#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>
#include "OneKnob.h"
#include "../Kit/BypassFade.h"
#include "../Kit/v3/NoteNames.h"
#include "../Kit/VuBallistics.h"

#ifndef ONEK_INDEX
#define ONEK_INDEX 0
#endif

class OneKnobProcessor : public juce::AudioProcessor {
public:
    OneKnobProcessor();

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return juce::String::fromUTF8(voicing().name); }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }
    juce::AudioProcessorParameter* getBypassParameter() const override { return bypass; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    // Index fixé à la compilation par chaque plugin ; seul l'outil de capture le modifie à l'exécution.
    static int& index() { static int i = ONEK_INDEX; return i; }
    static const onek::Voicing& voicing() { return onek::voicings()[(size_t)index()]; }

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float> level{0.f}, reduction{0.f};
    std::atomic<float> vuDb{-60.f};           // lecture VU (balistique 300 ms, RMS, 0 VU = -18 dBFS)
    std::atomic<float> inDb{-100.f};          // niveau RMS d'entrée après trim (dBFS)
    std::atomic<float> autoInDb{0.f};         // gain appliqué par l'auto-entrée (dB)
    std::atomic<int> keyPc{-1}, keyScale{0};  // tonalité réellement utilisée (-1 = inconnue / en recherche)
    std::atomic<int> keyState{2};             // 0 manuel, 1 auto verrouillé, 2 auto en recherche (hauteur laissée telle quelle)

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();
    onek::Chain chain;
    kit::VuBallistics vu;
    double sr = 48000.0;
    std::atomic<float>* pChar = nullptr, *pKey = nullptr, *pScale = nullptr, *pTrim = nullptr, *pAutoIn = nullptr, *pTone = nullptr,
                      *pMix = nullptr, *pOut = nullptr, *pSpec = nullptr, *pAutoGain = nullptr, *pOpt = nullptr, *pEngine = nullptr;
    voile::Delay bypassL, bypassR;
    bypassfx::Fader fader;
    juce::AudioParameterBool* bypass = nullptr;
    int maxBlock = 2048, lastLat = 0, maxLat = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OneKnobProcessor)
};
