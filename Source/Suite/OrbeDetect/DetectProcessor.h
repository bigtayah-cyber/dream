// Orbe Détect : analyseur passthrough. Capture jusqu'à 120 s d'audio entrant, analyse (tempo, tonalité) sur demande dans un fil de fond.
#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>
#include <memory>
#include "../OrbeAnalysis/OrbeAnalysis.h"
#include "FalseNotes.h"

struct DetectResult {
    bool valid = false;
    double bpm = 0, bpm0 = 0, bconf = 0, alt[2] = {0, 0}, kconf = 0, chroma[12] = {}, dur = 0;
    int nAlt = 0, pc = 0, rankPc[4] = {}; bool minor = false, rankMinor[4] = {};
};

class DetectProcessor : public juce::AudioProcessor {
public:
    static constexpr int kMaxSec = 120, kMinSec = 5, kPeaks = kMaxSec * 20;
    DetectProcessor();
    ~DetectProcessor() override;

    void prepareToPlay(double sampleRate, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&) override { getHostBpm(); }

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Orbe Detect"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    juce::AudioProcessorParameter* getBypassParameter() const override { return bypass; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    // --- interface (thread message) ---
    void analyze();                       // lance l'analyse de la capture en fil de fond
    void clearCapture();
    bool busy() const { return working.load(); }
    double captureSeconds() const { return clearReq.load() ? 0.0 : double(capLen.load(std::memory_order_acquire)) / sr; }
    bool captureFull() const { return !clearReq.load() && capLen.load() >= capMax; }
    float peakAt(int i) const { return peaks[(size_t)i].load(std::memory_order_relaxed); }
    int peakCount() const { return clearReq.load() ? 0 : juce::jmin(kPeaks, peakIdx.load(std::memory_order_acquire)); }
    std::shared_ptr<const DetectResult> result() const { return std::atomic_load(&res); }
    void setResult(const DetectResult& r) { std::atomic_store(&res, std::make_shared<const DetectResult>(r)); }
    void analyzeMap();                    // carte des fausses notes : juge la hauteur chantée de la capture par rapport à la tonalité (collée, sinon détectée)
    std::shared_ptr<const fn::Map> falseMap() const { return std::atomic_load(&fmap); }
    void setRefKey(int pc, bool minor) { refKey.store(pc * 2 + (minor ? 1 : 0)); }   // tonalité de référence (collée) ; -1 : celle que Détect mesure
    void clearRefKey() { refKey.store(-1); }
    int refKeyCode() const { return refKey.load(); }
    void scaleBpm(double f);              // ×½, ×2
    void setBpm(double v);

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float> hostBpm{0.f}, level{0.f};

private:
    struct Worker : juce::Thread { Worker(DetectProcessor& o) : juce::Thread("Orbe Detect analyse"), owner(o) {} void run() override; DetectProcessor& owner; };
    static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();
    void getHostBpm();

    juce::AudioParameterBool* bypass = nullptr;
    std::atomic<float>* capParam = nullptr;
    double sr = 48000.0;
    int capMax = 0;
    std::vector<float> cap;
    std::atomic<int> capLen{0}, peakIdx{0};
    std::atomic<bool> clearReq{false}, working{false};
    std::unique_ptr<std::atomic<float>[]> peaks;
    float peakAcc = 0.f; int peakCnt = 0, spp = 2400;
    std::shared_ptr<const DetectResult> res;
    std::shared_ptr<const fn::Map> fmap;
    std::atomic<int> refKey{-1};
    std::atomic<bool> mapOnly{false};
    std::shared_ptr<std::vector<float>> work; double workSr = 48000.0;
    Worker worker{*this};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DetectProcessor)
};
