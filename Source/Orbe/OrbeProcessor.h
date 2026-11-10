#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>
#include <memory>
#include <set>
#include "OrbeCore.h"
#include "OrbeSynth.h"
#include "../Kit/v3/NoteNames.h"

// Instantané lisible par le thread audio (créé sur le thread message, jamais modifié ensuite).
struct OrbeEv { float t, d; uint8_t n, track; float v; uint8_t gl; };
struct OrbeRendered { std::vector<OrbeEv> ev; int bars = 4; float bpm = 120.f; };

class OrbeProcessor : public juce::AudioProcessor,
                      private juce::AudioProcessorValueTreeState::Listener,
                      private juce::Timer {
public:
    OrbeProcessor();
    ~OrbeProcessor() override;

    void prepareToPlay(double sampleRate, int) override;
    void releaseResources() override {}
    static BusesProperties makeBuses();
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    #ifdef ORBE_FX
    const juce::String getName() const override { return "Orbe FX"; }
#else
    const juce::String getName() const override { return "Orbe"; }
#endif
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

    // --- thread message uniquement ---
    enum class Gen { All, Chords, Lead };
    void generate(Gen);
    void toggleLock(int chord);
    void rerollChord(int chord);
    bool isLocked(int chord) const { return locks.count(chord) > 0; }
    // édition (thread message) : beginEdit() une fois par geste, puis les opérations ; tout est republié (aperçu, MIDI sorti, MIDI glissé)
    void beginEdit() { gen.pushUndo(); }
    // un glisser envoie 60 à 120 événements par seconde : la republication est confiée au timer (30 Hz au plus)
    int addNote(int tr, float t, float d, int n) { const int i = gen.addNote(tr, t, d, n, 0.8f); publishReq = true; return i; }
    void setNote(int tr, int i, float t, float d, int n) { gen.setNote(tr, i, t, d, n); publishReq = true; }
    void removeNote(int tr, int i) { gen.removeNote(tr, i); publishReq = true; }
    const std::vector<orbe::Note>& notes(int tr) const { return gen.song().tr(tr); }
    void setChord(int bar, int slot, int deg);
    orbe::Chord chordFor(int deg) const { return gen.buildChord(deg); }
    std::vector<int> suggest(int bar) const { return gen.suggest(bar); }
    std::vector<int> borrowList() const { return gen.borrowList(); }
    void toggleTrackLock(int tr) { gen.lockMask ^= 1u << tr; }
    bool trackLocked(int tr) const { return gen.lockMask >> tr & 1; }
    bool trackEdited(int tr) const { return gen.editMask >> tr & 1; }
    bool undo();
    bool redo();
    bool canUndo() const { return gen.canUndo(); }
    bool canRedo() const { return gen.canRedo(); }
    const orbe::Song& song() const { return gen.song(); }
    const orbe::Style& style() const { return gen.style(); }
    juce::File writeMidiFile(bool chords, bool lead, bool bass, bool drums);

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<bool> audition{false};
    std::atomic<float> playStep{-1.f};  // position de lecture (pas), -1 si arrêté
    std::atomic<float> hostBpm{0.f};    // dernier tempo vu dans l'hôte (0 = inconnu)

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();
    void parameterChanged(const juce::String& id, float) override;
    void timerCallback() override;
    float p(const char* id) const { return apvts.getRawParameterValue(id)->load(); }
    orbe::Settings settingsFromParams() const;
    void applyDirty(int flags);
    void publish();

    orbe::Generator gen;
    std::set<int> locks;
    uint32_t seed = 20260101u, leadSeed = 777u;
    int lastStyle = -1;
    std::atomic<int> dirty{0};  // 1 = refaire accords+lead sur la même progression, 2 = tout régénérer, 4 = republier
    std::atomic<const OrbeRendered*> live{nullptr};  // lu par le thread audio (sans verrou ni allocation)
    // propriétaires des instantanés publiés : chacun est gardé au moins 2 s après avoir été remplacé (un bloc audio, même de 4096
    // échantillons sur un fil préempté, a fini de le lire bien avant) ; jamais libérés dans le thread audio
    struct Owned { std::shared_ptr<const OrbeRendered> r; juce::uint32 replacedMs; };
    std::vector<Owned> owners;
    bool publishReq = false;  // thread message : édition de notes en attente de republication
    // pointeurs de paramètres résolus une fois (le thread audio ne fait aucune recherche par nom)
    std::atomic<float>* prVol[4] = {}; std::atomic<float>* prMute[4] = {}; std::atomic<float>* prMaster = nullptr; std::atomic<float>* prMidi = nullptr;
    void syncSeeds() { seed = gen.settings().seed; leadSeed = gen.settings().leadSeed; }

    // audio
    orbe::Synth synth;
    double sr = 48000.0;
    double internalPos = 0.0;
    struct Off { int note, ch, left; };
    Off pend[128];
    int nPend = 0;
    bool wasRunning = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OrbeProcessor)
};
