#pragma once
#include <array>
#include <atomic>
#include <juce_audio_utils/juce_audio_utils.h>
#include "OrbeBeatEngine.h"
#include "../../Kit/v3/NoteNames.h"
#include "../../Kit/AuxOuts.h"

class OrbeBeatProcessor : public juce::AudioProcessor {
public:
    OrbeBeatProcessor();
    ~OrbeBeatProcessor() override = default;

    void prepareToPlay(double sampleRate, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Orbe Beat"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    // --- motifs (lisibles/écrivables depuis n'importe quel thread : cases atomiques) ---
    obeat::Cell cell(int pat, int lane, int step) const { return cells[idx(pat, lane, step)].load(std::memory_order_relaxed); }
    void setCell(int pat, int lane, int step, obeat::Cell c) { cells[idx(pat, lane, step)].store(c, std::memory_order_relaxed); }
    int currentPattern() const { return (int)p("pat"); }
    int steps() const { return p("steps") > 0.5f ? 32 : 16; }
    void generateCurrent();
    int varyCurrent();                // crée la variation dans le pattern suivant et le sélectionne
    void clearCurrent();
    void preview(int lane, float v, int note);  // jouer un coup depuis l'interface
    juce::File writeMidiFile();
    float tempo() const { const float b = hostBpm.load(); return b > 0.f ? b : (float)obeat::kits()[(int)p("kit")].bpm; }
    int noteOfDegree(int d) const { return obeat::degNote((int)p("key"), (int)p("mode"), d); }
    bool audible(int lane) const;

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<bool> audition{false};       // horloge interne quand l'hôte est à l'arrêt
    std::atomic<float> playStep{-1.f};       // pas courant (avec fraction), -1 si arrêté
    std::atomic<int> playPat{0};
    std::atomic<float> hostBpm{0.f};
    std::atomic<float> flash[obeat::kLanes];
    std::atomic<float> meter{0.f};

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();
    static int idx(int pat, int lane, int step) { return (pat * obeat::kLanes + lane) * obeat::kMaxSteps + step; }
    float p(const char* id) const { return apvts.getRawParameterValue(id)->load(); }
    int chainPattern(long long bar) const;
    obeat::Pattern readPattern(int pat) const;
    void writePattern(int pat, const obeat::Pattern&);

    struct Ev { int off, lane, note; float v; bool release; };
    std::array<Ev, 1024> evs;
    int nev = 0;
    std::vector<float> mono;
    std::vector<float> stemBuf[obeat::kLanes];   // une piste seule, rendue seulement si une sortie auxiliaire est active
    obeat::Engine engine;
    double sr = 48000.0, internalPpq = 0.0, lastEnd = 0.0;
    bool lastValid = false, wasAudition = false;
    int lastKit = -1;
    std::array<std::atomic<obeat::Cell>, obeat::kPats * obeat::kLanes * obeat::kMaxSteps> cells;
    std::array<std::atomic<uint32_t>, 32> ring;  // aperçus : lane | note<<4 | vel<<12
    std::atomic<uint32_t> ringW{0}, ringR{0};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OrbeBeatProcessor)
};
