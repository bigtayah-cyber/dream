#pragma once
#include "../OrbeSampler/SamplerCommon.h"

// Orbe Pads : sampler 16 pads (C1 à D#2 = MIDI 36 à 51) sur un échantillon découpé.
class PadsProcessor : public juce::AudioProcessor, private juce::AudioProcessorValueTreeState::Listener, private juce::Timer {
public:
    PadsProcessor();
    ~PadsProcessor() override;

    void prepareToPlay(double sampleRate, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Orbe Pads"; }
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

    static constexpr int kFirstNote = 36;  // C1
    static const char* const* padParamNames();

    // --- thread message ---
    bool loadFile(const juce::File&);
    void loadDemo();
    std::vector<int> marksCopy() const { std::lock_guard<std::mutex> l(markLock); return marks; }
    void addMark(int frame);
    void moveMark(int idx, int frame);
    void removeMark(int idx);
    void clearMarks();
    void padDown(int pad, float vel) { uiOff[pad] = 0; uiOn[pad] = juce::jlimit(1, 127, juce::roundToInt(vel * 127.f)); }
    void padUp(int pad) { uiOff[pad] = 1; }
    float p(const juce::String& id) const { return apvts.getRawParameterValue(id)->load(); }

    juce::AudioProcessorValueTreeState apvts;
    osamp::SampleHolder holder;
    osamp::Swap<osamp::Bank> bank;
    std::atomic<int> sel{0};
    std::atomic<float> padPos[16];   // position de lecture 0..1, -1 si le pad est muet
    std::atomic<float> hostBpm{0.f};
    std::atomic<bool> truncated{false};

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();
    void parameterChanged(const juce::String&, float) override { chopDirty = true; }
    void timerCallback() override;
    void rechop();
    void publishBank();
    osamp::PadParams read(int pad) const;

    osamp::PadEngine engine;
    std::atomic<float>* pp[16][11] = {};
    std::atomic<float> *pPlay, *pDlt, *pMaster;
    std::atomic<int> uiOn[16], uiOff[16];
    std::atomic<bool> chopDirty{false}, bankDirty{false};
    mutable std::mutex markLock;
    std::vector<int> marks{0};
    std::vector<float> monoCache;
    const osamp::Sample* monoFor = nullptr;
    std::shared_ptr<const osamp::Sample> monoKeep;
    double sr = 48000.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PadsProcessor)
};
