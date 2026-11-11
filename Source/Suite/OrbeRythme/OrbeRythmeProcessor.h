#pragma once
// Orbe Rythme : générateur de batterie et de rythmes (MIDI + aperçu sonore), issu d'Orbe.
// Moteur : le même orbe::Generator que celui d'Orbe (Source/Orbe/OrbeCore.h), la même batterie par style, les mêmes graines.
// Un état d'ancien Orbe (balise « ORBE ») se relit ici : voir setStateInformation et docs/orbe-rythme-migration.md.
#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>
#include <memory>
#include "../../Orbe/OrbeCore.h"
#include "../../Orbe/OrbeSynth.h"

namespace oryt {
// Instantané lisible par le fil audio (créé sur le fil message, jamais modifié ensuite).
struct Ev { float t, d; uint8_t n; float v; };
struct Rendered { std::vector<Ev> ev; int bars = 4; float bpm = 120.f; };
// Lignes de la grille, de haut en bas : crash, charley ouvert, charley, clap, caisse claire, rim, grosse caisse (notes GM).
inline const int kRowNote[7] = {49, 46, 42, 39, 38, 37, 36};
inline int rowOfNote(int n) { for (int i = 0; i < 7; ++i) if (kRowNote[i] == n) return i; return n == 35 ? 6 : 2; }
}  // namespace oryt

class OrbeRythmeProcessor : public juce::AudioProcessor, private juce::AudioProcessorValueTreeState::Listener, private juce::Timer {
public:
    OrbeRythmeProcessor();
    ~OrbeRythmeProcessor() override;

    void prepareToPlay(double sampleRate, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& l) const override { return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() && l.getMainInputChannelSet().isDisabled(); }
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Orbe Rythme"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 1.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    // --- fil message uniquement ---
    void generate();                                  // nouveau tirage (nouvelle graine)
    void toggleHit(int row, int step);                // pose ou retire une frappe sur la ligne et le pas
    void cycleVelocity(int row, int step);            // fort -> moyen -> doux
    void beginEdit() { gen.pushUndo(); }
    bool undo();
    bool redo();
    bool canUndo() const { return gen.canUndo(); }
    bool canRedo() const { return gen.canRedo(); }
    bool edited() const { return gen.editMask >> 3 & 1; }
    const std::vector<orbe::Note>& notes() const { return gen.song().nDrums; }
    int bars() const { return (int)gen.song().prog.size(); }
    const orbe::Style& style() const { return gen.style(); }
    juce::File writeMidiFile();
    // Index de la frappe de la ligne `row` la plus proche du pas `step` (à moins d'un demi-pas), -1 si aucune.
    int findHit(int row, float step) const;

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<bool> audition{false};
    std::atomic<float> playStep{-1.f};   // position de lecture (pas), -1 si arrêté
    std::atomic<float> hostBpm{0.f};

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();
    void parameterChanged(const juce::String& id, float) override;
    void timerCallback() override;
    float p(const char* id) const { return apvts.getRawParameterValue(id)->load(); }
    orbe::Settings settings() const;
    void applyDirty(int flags);
    void publish();
    void rebuildFromSeed();
    void loadLegacyOrbe(const juce::ValueTree& tree);   // état d'un ancien Orbe

    orbe::Generator gen;
    // Contexte harmonique : la batterie d'Orbe dépend du tirage des accords (même flux aléatoire). Conservé tel quel pour qu'un état d'ancien
    // Orbe redonne exactement ses frappes ; pour un Rythme neuf : tonique A, gamme et 7e du style, sans emprunts ni arpège.
    struct Ctx { int key = 9, scale = -1, arp = 0; bool seventh = false, seventhSet = false, add9 = false, borrow = false; float density = 0.55f; } ctx;
    uint32_t seed = 20260101u;
    int lastStyle = -1;
    std::atomic<int> dirty{0};   // 2 = tout régénérer (style, mesures), 4 = republier (humain, swing)
    std::atomic<const oryt::Rendered*> live{nullptr};
    struct Owned { std::shared_ptr<const oryt::Rendered> r; juce::uint32 replacedMs; };
    std::vector<Owned> owners;
    bool publishReq = false;
    std::atomic<float>* prLevel = nullptr; std::atomic<float>* prMute = nullptr; std::atomic<float>* prMaster = nullptr; std::atomic<float>* prMidi = nullptr;

    orbe::Synth synth;
    double sr = 48000.0, internalPos = 0.0;
    struct Off { int note, ch, left; };
    Off pend[128];
    int nPend = 0;
    bool wasRunning = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OrbeRythmeProcessor)
};
