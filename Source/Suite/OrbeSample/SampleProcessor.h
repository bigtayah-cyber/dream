#pragma once
#include "../OrbeSampler/SamplerCommon.h"

class SampleProcessor;

// Thread de travail : rend la boucle transformée hors du thread audio, avec un cache de l'étape lente (WSOLA).
class RenderThread : public juce::Thread {
public:
    explicit RenderThread(SampleProcessor& o) : juce::Thread("Orbe rendu"), owner(o) {}
    void poke();
    void run() override;
    std::atomic<bool> dirty{false}, cancel{false}, busy{false};
private:
    SampleProcessor& owner;
    osamp::Transformer tf;
    std::atomic<double> lastChange{0.0};
};

// Orbe Sample : transformateur de sample jouable.
class SampleProcessor : public juce::AudioProcessor, private juce::AudioProcessorValueTreeState::Listener, private juce::Timer {
public:
    SampleProcessor();
    ~SampleProcessor() override;

    void prepareToPlay(double sampleRate, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Orbe Sample"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    static constexpr int kFirstNote = 36;  // C1 : les découpes se jouent à partir de là
    static const int kPresets = 9;
    static juce::String presetName(int i);

    // --- thread message ---
    bool loadFile(const juce::File&);
    void loadDemo();
    void applyPreset(int i);
    void setZone(float a, float b);
    void sliceDown(int i) { uiSlice = i; }
    float p(const juce::String& id) const { return apvts.getRawParameterValue(id)->load(); }
    // utilisé par le thread de rendu
    std::shared_ptr<osamp::Rendered> renderNow(osamp::Transformer&, const std::atomic<bool>& cancel);
    void publish(std::shared_ptr<const osamp::Rendered> r) { out.publish(std::move(r)); }

    juce::AudioProcessorValueTreeState apvts;
    osamp::SampleHolder holder;
    osamp::Swap<osamp::Rendered> out;
    std::atomic<bool> audition{false};
    std::atomic<float> playPos{-1.f};   // position dans la boucle 0..1, -1 si muette
    std::atomic<int> slicePlaying{-1};
    std::atomic<double> hostBpm{0.0};
    std::atomic<bool> truncated{false};
    std::atomic<int> presetIdx{0};      // -1 = réglage personnalisé
    std::atomic<int> loadedVersion{3};  // version d'état lue (nouvelle instance : 3) ; < 3 : ancien étirement WSOLA
    RenderThread worker;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();
    void parameterChanged(const juce::String& id, float) override;
    void timerCallback() override;

    struct Play { const osamp::Rendered* r = nullptr; double pos = 0; };
    struct Slice { const osamp::Rendered* r = nullptr; double pos = 0, end = 0; float fade = 1.f, dec = 0.f; int idx = -1; bool on = false; };
    Play cur, old;
    int fadeLeft = 0, fadeTotal = 1;
    Slice sv[2];
    int svCur = 0;
    float loopLevel = 0.f;
    bool wasHostPlaying = false;
    double sr = 48000.0;
    std::atomic<double> lastHostUsed{0.0};
    std::atomic<bool> applying{false}, restoring{false};
    std::atomic<int> uiSlice{-1};
    std::atomic<float>* pMaster;
    std::atomic<float>* pFollow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SampleProcessor)
};
