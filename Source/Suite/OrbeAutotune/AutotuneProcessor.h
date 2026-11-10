#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "AutotuneDsp.h"
#include "NoteEdit.h"
#include "../../Kit/BypassFade.h"

class AutotuneProcessor : public juce::AudioProcessor {
public:
    AutotuneProcessor();
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Orbe Autotune"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    juce::AudioProcessorParameter* getBypassParameter() const override { return bypass; }

    int getNumPrograms() override { return atn::kPresets; }
    int getCurrentProgram() override { return prog; }
    void setCurrentProgram(int i) override;
    const juce::String getProgramName(int i) override { return juce::String::fromUTF8(atn::presets()[juce::jlimit(0, atn::kPresets - 1, i)].name); }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    atn::Params readParams() const;
    float raw(const char* id) const { return apvts.getRawParameterValue(id)->load(); }
    bool presetMatches(int i) const;   // les réglages actuels sont-ils ceux du preset i ?
    void toggleNote(int pc);           // inverse la note par rapport à la gamme (masque éditable)
    void clearNotes();
    const atn::Telemetry& tel() const { return engine.tel; }
    bool flatsPref() const { return raw("accid") > 0.5f; }
    bool orbeOn() const { return raw("algo") > 0.5f || juce::roundToInt(raw("rendu")) == 0; }   // Satin passe par le moteur Orbe

    // ---- édition note par note (fil de l'interface uniquement)
    ned::Capture capture;
    const std::vector<ned::Note>& notes() const { return notesUi; }
    const std::vector<ned::Frame>& curve() const { return curveUi; }
    void setNotes(std::vector<ned::Note> v, bool undoable = true);   // publie aussi vers le fil audio
    bool undo(); bool redo();
    bool canUndo() const { return histPos > 0; }
    bool canRedo() const { return histPos + 1 < (int)hist.size(); }
    void armCapture();
    void stopCapture();
    bool pollCapture();                          // vrai quand une capture vient d'être découpée en notes
    double playPos() const { return lastPos.load(); }
    bool isPlaying() const { return lastPlaying.load(); }
    double sampleRateNow() const { return getSampleRate() > 0 ? getSampleRate() : 44100.0; }

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();
    void setPlain(const char* id, float v);
    void publishEdits();
    void initRaw();
    enum RawId { rSpeed, rTol, rAmt, rTrans, rForm, rFormPres, rVd, rVr, rHarm, rHv, rDbl, rMix, rLat, rKey, rScale, rAlgo, rExpr, rRender, rGlide, rNum };
    std::atomic<float>* rp[rNum] = {};
    std::atomic<float>* rx[12] = {};
    atn::Engine engine;
    juce::SpinLock editLock;
    std::unique_ptr<ned::EditList> editLive = std::make_unique<ned::EditList>();
    std::vector<ned::Note> notesUi;
    std::vector<ned::Frame> curveUi;
    std::vector<std::vector<ned::Note>> hist;
    int histPos = -1;
    std::atomic<double> lastPos{0.0};
    std::atomic<bool> lastPlaying{false};
    atn::BypassDelay byp;
    bypassfx::Fader fader, faderIdle;   // l'un pour le mode actif, l'autre (latence de l'autre mode) tenu chaud pour basculer sans trou
    bool faderIsLow = false;
    int latLow = 0, latQual = 0;
    juce::AudioParameterBool* bypass = nullptr;
    juce::AudioBuffer<float> scratch;
    int prog = 2;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AutotuneProcessor)
};
