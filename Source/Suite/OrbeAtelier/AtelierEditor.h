#pragma once
#include "AtelierProcessor.h"
#include "../../Kit/v3/Widgets3.h"
#include "../../Kit/KeyClipboard.h"

class AtelierEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit AtelierEditor(AtelierProcessor&);
    ~AtelierEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseUp(const juce::MouseEvent&) override;
    static constexpr float kW3 = 1200.f, kH3 = 700.f;
    // Thème (mémorisé hors apvts si persist) : Clair / Sombre (v3) ou Classique (face d'origine) ; échelle de fenêtre.
    void setThemeMode(kit::v3::Mode m, bool persist = true);
    void setUiScale(float s);
private:
    bool isV3() const { return mode != kit::v3::Mode::Classic; }
    void resized3();
    void paint3(juce::Graphics&);
    void paintBackdrop3(juce::Graphics&, juce::Rectangle<float>);
    juce::Rectangle<float> menuArea() const { return isV3() ? S(460, 12, 280, 46) : S(284, 58, 344, 66); }
    struct Mod { const char* title; const char* on; std::vector<juce::String> ids; int row; juce::Rectangle<float> panel; int firstPot = 0, nPots = 0; std::vector<juce::Rectangle<float>> labels; };
    void timerCallback() override;
    void showMenu();
    void step(int d) { proc.applyPreset((proc.currentPreset() + d + atl::numPresets()) % atl::numPresets()); }
    juce::Rectangle<float> S(float x, float y, float w, float h) const { return {x * u_, y * u_, w * u_, h * u_}; }
    juce::Rectangle<int> Si(float x, float y, float w, float h) const { return S(x, y, w, h).toNearestInt(); }

    AtelierProcessor& proc;
    kit::Palette pal;
    oc::ui::ScaledLook look;
    juce::TooltipWindow tooltip{this, 600};
    float u_ = 1.f, outDb = -60.f, grDisp = 0.f;
    bool modified = false;
    std::vector<Mod> mods;
    juce::OwnedArray<oc::ui::Pot> pots;
    juce::OwnedArray<juce::ToggleButton> toggles; juce::OwnedArray<juce::ComboBox> combos;
    juce::ArrowButton prev{"prev", 0.5f, kit::w::kSilk.withAlpha(0.7f)}, next{"next", 0.f, kit::w::kSilk.withAlpha(0.7f)};
    kit::v3::bridge::Power power;
    kit::v3::bridge::HelpKey help;
    // --- v3 : un Knob3 par potard (même ordre que pots), cartes de modules calculées dans resized3
    kit::v3::Mode mode = kit::v3::Mode::Light;
    kit::v3::Theme T;
    kit::v3::Look3 look3{T};
    kit::v3::ArtCache art;
    kit::v3::ThemeMenu themeBtn;
    juce::OwnedArray<kit::v3::Knob3> k3;
    std::vector<juce::String> potIds;
    std::vector<juce::Rectangle<float>> panels3;
    std::vector<std::pair<juce::Rectangle<float>, juce::String>> labels3;
    kit::v3::Chevron prev3{true}, next3{false};
    int lastPreset = -1;
    std::unique_ptr<kit::v3::bridge::Seg> accSeg;   // # / b (cellule Tune)
    kit::KeyPaster keyPaster;   // clic droit sur Tonalité : coller la tonalité copiée depuis Orbe Détect
    juce::ToggleButton dsFirstBtn{"De-ess avant comp"};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> dsFirstAtt;
    juce::ComboBox* keyCombo = nullptr;
    bool flat_ = false;
    float scale_ = 1.f;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAtt;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> togAtt;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> cmbAtt;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AtelierEditor)
};
