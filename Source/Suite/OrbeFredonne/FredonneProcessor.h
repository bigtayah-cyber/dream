// Orbe Fredonne : instrument à entrée audio, sortie MIDI. Enregistre une voix (jusqu'à 120 s), la découpe en notes (YIN) et les envoie à l'hôte.
#pragma once
#include "../../Kit/v3/NoteNames.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>
#include <memory>
#include "../OrbeAnalysis/OrbeAnalysis.h"
#include "../../Engine.h"

class FredonneProcessor : public juce::AudioProcessor, private juce::AudioProcessorValueTreeState::Listener, private juce::Timer {
public:
    enum State { Idle, Armed, Recording, Captured, Analysing };
    static constexpr int kMaxSec = 120;
    struct Take { oa::SegResult seg; std::shared_ptr<const oa::PitchTrack> track; double bpm = 100; };

    FredonneProcessor();
    ~FredonneProcessor() override;
    void prepareToPlay(double sampleRate, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Orbe Fredonne"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    // --- interface (thread message) ---
    void pressRecord();                       // Idle → Armed → (auto) Recording → stop + analyse
    void clearTake();
    void loadAudio(const float* mono, int n, double rate);   // outils de test : analyse un signal existant
    juce::File writeMidiFile();
    int state() const { return st.load(); }
    double recSeconds() const { return double(recLen.load(std::memory_order_acquire)) / recSr; }
    std::shared_ptr<const Take> take() const { return std::atomic_load(&takeP); }
    double effectiveBpm() const;
    oa::SegParams segParams() const;

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float> level{0.f}, liveHz{0.f}, hostBpm{0.f};

private:
    struct Worker : juce::Thread { Worker(FredonneProcessor& o) : juce::Thread("Orbe Fredonne analyse"), owner(o) {} void run() override; FredonneProcessor& owner; };
    static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();
    void parameterChanged(const juce::String&, float) override { dirty = true; }
    void timerCallback() override;
    void resegment();
    float p(const char* id) const { return apvts.getRawParameterValue(id)->load(); }
    std::atomic<float> *pPlay = nullptr, *pMonitor = nullptr, *pFollow = nullptr, *pBpm = nullptr;   // accès direct pour le fil audio

    double sr = 48000.0;
    double recSr = 48000.0;   // fréquence à laquelle `rec` a été capté (peut différer de sr après un changement de l'hôte)
    std::atomic<int> st{Idle}, recLen{0};
    std::vector<float> rec; int recMax = 0;
    float pre[4096] = {}; int preIdx = 0;
    voile::Yin yin; float yAcc = 0.f; int yDec = 0, yCount = 0;
    std::atomic<bool> dirty{false}, analysisDone{false};
    std::atomic<double> takeStart{0.0};
    std::shared_ptr<const oa::PitchTrack> trackP;
    std::shared_ptr<const Take> takeP;
    std::shared_ptr<const std::vector<oa::MidiNote>> playNotes;
    Worker worker{*this};
    // lecture MIDI vers l'hôte
    double lastP1 = -1.0; bool wasPlaying = false; bool active[128] = {};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FredonneProcessor)
};
