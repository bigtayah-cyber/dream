#pragma once
#include "LooksProcessor.h"
#include "../../Kit/v3/Widgets3.h"

class LooksEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit LooksEditor(LooksProcessor&);
    ~LooksEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseUp(const juce::MouseEvent&) override;
    static constexpr float kW3 = 1000.f, kH3 = 596.f;
    // Thème (mémorisé hors apvts si persist) : Clair / Sombre (v3) ou Classique (face d'origine) ; échelle de fenêtre.
    void setThemeMode(kit::v3::Mode m, bool persist = true);
    void setUiScale(float s);
private:
    bool isV3() const { return mode != kit::v3::Mode::Classic; }
    void resized3();
    void paint3(juce::Graphics&);
    void paintBackdrop3(juce::Graphics&, juce::Rectangle<float>);
    juce::Rectangle<float> menuArea() const { return isV3() ? S(380, 12, 240, 46) : S(58, 62, 344, 110); }
    void timerCallback() override;
    void showMenu();
    void step(int d) { proc.applyLook((proc.currentLook() + d + oc::numLooks()) % oc::numLooks()); }
    void section(juce::Graphics&, const char* title, float y);
    juce::Rectangle<float> S(float x, float y, float w, float h) const { return {x * u_, y * u_, w * u_, h * u_}; }
    juce::Rectangle<int> Si(float x, float y, float w, float h) const { return S(x, y, w, h).toNearestInt(); }

    LooksProcessor& proc;
    kit::Palette pal;
    oc::ui::ScaledLook look;
    juce::TooltipWindow tooltip{this, 600};
    float u_ = 1.f, outDb = -60.f, grDisp = 0.f, shiftDisp = 0.f;
    int shownKey = -2;
    bool moved = false;
    float modLevel[8] = {};
    juce::OwnedArray<oc::ui::Pot> pots;
    juce::ComboBox keyBox, scaleBox;
    kit::KeyPaster keyPaster;   // clic droit sur Tonalité : coller la tonalité copiée depuis Orbe Détect
    juce::ToggleButton keepBtn{"Garder mes macros"};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> keepAtt;
    float autoGainDisp = 0.f;
    std::unique_ptr<kit::v3::bridge::Seg> accSeg;   // # / b
    bool flat_ = false;
    juce::ArrowButton prev{"prev", 0.5f, kit::w::kSilk.withAlpha(0.7f)}, next{"next", 0.f, kit::w::kSilk.withAlpha(0.7f)};
    kit::v3::bridge::Power power;
    kit::v3::bridge::HelpKey help;
    // --- v3
    kit::v3::Mode mode = kit::v3::Mode::Light;
    kit::v3::Theme T;
    kit::v3::Look3 look3{T};
    kit::v3::ArtCache art;
    kit::v3::ThemeMenu themeBtn;
    std::unique_ptr<kit::v3::Knob3> k3[6];
    kit::v3::Chevron prev3{true}, next3{false};
    kit::v3::Trace trLvl, trGr, trSh;
    static constexpr int kHist = 120;
    float rLvl[kHist] = {}, rGr[kHist] = {}, rSh[kHist] = {}, bLvl[kHist] = {}, bGr[kHist] = {}, bSh[kHist] = {};
    int rPos = 0, lastLook = -1;
    float scale_ = 1.f;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> keyAtt, scaleAtt;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LooksEditor)
};
