#pragma once
#include "Orbe808Processor.h"
#include "SuiteUi.h"

// Clavier de deux octaves (C1–B2) pour jouer la basse depuis l'interface ; glisser d'une touche à l'autre fait glisser la hauteur (legato).
class Orbe808Keys : public juce::Component {
public:
    Orbe808Keys(Orbe808Processor& p, const kit::Palette& pal) : proc(p), P(pal) {}
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent& e) override { press(keyAt(e.position)); }
    void mouseDrag(const juce::MouseEvent& e) override { press(keyAt(e.position)); }
    void mouseUp(const juce::MouseEvent&) override { press(-1); }
    static constexpr int kBase = 24, kWhite = 14;
private:
    int keyAt(juce::Point<float>) const;
    void press(int note);
    Orbe808Processor& proc;
    kit::Palette P;
    int cur = -1;
};

class Orbe808Editor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit Orbe808Editor(Orbe808Processor&);
    ~Orbe808Editor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    static kit::Palette palette();

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    static constexpr int kKnobs = 9;
    void timerCallback() override;
    void refreshShot();
    void paintHeader(juce::Graphics&, float u);
    void paintScope(juce::Graphics&, float u);
    juce::Rectangle<float> S(float x, float y, float w, float h) const { return {x * u_, y * u_, w * u_, h * u_}; }
    juce::Rectangle<int> Si(float x, float y, float w, float h) const { return S(x, y, w, h).toNearestInt(); }
    juce::String valueText(int i) const;

    Orbe808Processor& proc;
    kit::Palette pal;
    kit::Look look;
    juce::TooltipWindow tooltip{this, 600};
    kit::w::HelpKey help;
    float u_ = 1.f;

    juce::Slider knobs[kKnobs];
    std::unique_ptr<SliderAtt> att[kKnobs];
    std::unique_ptr<suiteui::Segmented> presetSeg, satSeg, bendSeg;
    Orbe808Keys keysUi;

    static constexpr int kCols = 640;
    std::vector<float> pkMin, pkMax;
    float shotSeconds = 1.f;
    uint64_t shotKey = 0;
    int shotNote = 33;
    float meterShown = 0.f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Orbe808Editor)
};
