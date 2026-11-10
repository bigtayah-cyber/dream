#pragma once
#include "../OrbeDsp/OrbeFace.h"
#include "GaletDsp.h"

class GaletProcessor : public of::FxProcessor {
public:
    GaletProcessor();
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool acceptsMidi() const override { return true; }
    int getNumPrograms() override;
    int getCurrentProgram() override { return program; }
    void setCurrentProgram(int) override;
    const juce::String getProgramName(int) override;

    gal::Params readParams() const;
    void writeParams(const gal::Params&);
    // Forme : modifiée par l'interface (fil de message), transmise au moteur sans attente côté audio.
    const gal::Shape& shape() const { return shape_; }
    void setShape(const gal::Shape& s);
    int shapeVersion() const { return shapeVer; }
    bool snap = true;                    // aimant de la grille (interface), sauvegardé dans l'état

    // lecture par l'interface
    float phaseNow() const { return uiPhase.load(); }
    float modNow() const { return uiMod.load(); }
    float envNow(int k) const { return uiEnv[(size_t)k].load(); }
    float driveNow() const { return uiDrive.load(); }
    double bpmNow() const { return uiBpm.load(); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();
    void beforeSave() override;
    void afterLoad() override;
    gal::Engine engine;
    gal::Shape shape_;
    int shapeVer = 0, program = 0, maxBlock = 0;   // 0 : pas encore préparé
    std::array<int, 128> notes{};
    std::atomic<float> uiPhase{0.f}, uiMod{0.f}, uiDrive{0.f};
    std::array<std::atomic<float>, 4> uiEnv{};
    std::atomic<double> uiBpm{120.0};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GaletProcessor)
};
