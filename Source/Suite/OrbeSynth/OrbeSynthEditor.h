#pragma once
#include "../../Kit/Widgets.h"
#include "OrbeSynthProcessor.h"
#include "../../Kit/v3/InstrumentSkin.h"

namespace osui {
using APVTS = juce::AudioProcessorValueTreeState;

// Potard moleté, libellé gravé et valeur lue sur l'afficheur (taille de référence 86 x 88).
class Knob : public juce::Component {
public:
    Knob(APVTS&, const char* id, juce::String label, juce::String tip, const kit::Palette&);
    void resized() override;
    void paint(juce::Graphics&) override;
private:
    juce::RangedAudioParameter* prm;
    juce::Slider sl;
    std::unique_ptr<APVTS::SliderAttachment> att;
    juce::String label;
    kit::Palette P;
};

// Sélecteur segmenté relié à un paramètre « choix » ; icônes d'onde ou texte.
class Seg : public juce::Component, public juce::SettableTooltipClient {
public:
    Seg(juce::RangedAudioParameter&, juce::StringArray labels, bool waveIcons, const kit::Palette&);
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    const kit::v3::Theme* T = nullptr;   // nul ou Classique : rendu d'origine
    void mouseMove(const juce::MouseEvent& e) override { const int h = juce::jlimit(0, labels_.size() - 1, (int)(e.position.x / ((float)getWidth() / (float)labels_.size()))); if (h != hover) { hover = h; repaint(); } }
    void mouseExit(const juce::MouseEvent&) override { hover = -1; repaint(); }
private:
    int hover = -1;
    juce::StringArray labels_;
    bool icons;
    kit::Palette P;
    int idx = 0;
    juce::ParameterAttachment att;
};

class PresetBar : public juce::Component, public juce::SettableTooltipClient {
public:
    PresetBar(OrbeSynthProcessor&, const kit::Palette&);
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    const kit::v3::Theme* T = nullptr;
private:
    void pick(int);
    OrbeSynthProcessor& proc;
    kit::Palette P;
};

// Affichages : courbes recalculées à chaque réglage.
class CurveView : public juce::Component {
public:
    const kit::v3::Theme* T = nullptr;
protected:
    CurveView(OrbeSynthProcessor&, const kit::Palette&, std::initializer_list<const char*> ids);
    OrbeSynthProcessor& proc;
    kit::Palette P;
    std::vector<std::unique_ptr<juce::ParameterAttachment>> watch;
};
class EnvView : public CurveView {
public:
    EnvView(OrbeSynthProcessor& p, const kit::Palette& pal) : CurveView(p, pal, {"a", "d", "s", "r"}) {}
    void paint(juce::Graphics&) override;
};
class FilterView : public CurveView {
public:
    FilterView(OrbeSynthProcessor& p, const kit::Palette& pal) : CurveView(p, pal, {"cut", "res", "famt", "fdec"}) {}
    void paint(juce::Graphics&) override;
};

class Keys : public juce::Component, public juce::SettableTooltipClient {
public:
    static constexpr int lo = 36, hi = 84;  // C2 .. C6
    Keys(juce::MidiKeyboardState&, const kit::Palette&);
    ~Keys() override;
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent& e) override { press(e); }
    void mouseDrag(const juce::MouseEvent& e) override { press(e); }
    void mouseUp(const juce::MouseEvent&) override { release(); }
    const kit::v3::Theme* T = nullptr;
private:
    void paint3(juce::Graphics&);
    int noteAt(juce::Point<float>, float& vel) const;
    void press(const juce::MouseEvent&);
    void release();
    juce::Rectangle<float> keyRect(int n) const;
    juce::MidiKeyboardState& st;
    kit::Palette P;
    int cur = -1;
};

class Wheel : public juce::Component, public juce::SettableTooltipClient {
public:
    Wheel(std::atomic<float>& v, float lo, float hi, bool spring, const kit::Palette&);
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent& e) override { drag(e); }
    void mouseDrag(const juce::MouseEvent& e) override { drag(e); }
    void mouseUp(const juce::MouseEvent&) override { if (spring) val = 0.f; repaint(); }
    void mouseDoubleClick(const juce::MouseEvent&) override { val = 0.f; repaint(); }
    const kit::v3::Theme* T = nullptr;
private:
    void drag(const juce::MouseEvent&);
    std::atomic<float>& val;
    float lo_, hi_;
    bool spring;
    kit::Palette P;
};
}  // namespace osui

class OrbeSynthEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit OrbeSynthEditor(OrbeSynthProcessor&);
    ~OrbeSynthEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    static kit::Palette palette();

private:
    void timerCallback() override;
    void paintHeader(juce::Graphics&, float u);
    void section(juce::Graphics&, float u, juce::Rectangle<float> r, const char* num, const juce::String& title, const juce::String& sub);
    juce::Rectangle<float> S(float x, float y, float w, float h) const { return {x * u_, y * u_, w * u_, h * u_}; }
    juce::Rectangle<int> Si(float x, float y, float w, float h) const { return S(x, y, w, h).toNearestInt(); }

    OrbeSynthProcessor& proc;
    kit::Palette pal;
    kit::Look look;
    juce::TooltipWindow tooltip{this, 600};
    kit::v3::bridge::HelpKey help;
    kit::v3::Skin skin{"orbesynth", kit::v3::accents::lilac};
    juce::OwnedArray<kit::v3::Knob3> k3;
    void layout5(bool n3);
    void resized3(); void paint3(juce::Graphics&); void paintBackdrop3(juce::Graphics&, juce::Rectangle<float>);
    float u_ = 1.f;

    osui::PresetBar presetBar;
    std::unique_ptr<osui::Seg> wave1, wave2, divSeg, modeSeg, bendSeg, l1shape, l1dest, l2shape, l2dest, f2Seg;
    std::vector<std::unique_ptr<osui::Knob>> knobs;  // ordre de création = ordre de mise en page
    osui::EnvView envView;
    osui::FilterView fltView;
    osui::Keys keyboard;
    osui::Wheel bendWheel, modWheel;

    int lastVoices = -1;
    float lastBend = 0.f, lastMod = 0.f;
    uint64_t lastMask[2] = {0, 0};
    bool lastModified = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OrbeSynthEditor)
};
