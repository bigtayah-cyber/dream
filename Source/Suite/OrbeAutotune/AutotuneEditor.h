#pragma once
#include "../../Kit/Widgets.h"
#include "AutotuneProcessor.h"
#include "../OrbeDsp/OrbeFace.h"
#include "../../Kit/v3/Widgets3.h"

// Clavier d'une octave : le masque de notes de la gamme, éditable à la main (clic = inverser la note).
class AtKeys : public juce::Component {
public:
    explicit AtKeys(AutotuneProcessor& p) : proc(p) {}
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override { setMouseCursor(juce::MouseCursor::PointingHandCursor); }
    int sung = -1, root = -1;
    int mask = 0, mode = 2;
    bool flats = false;
    const kit::v3::Theme* T = nullptr;   // nul ou Classique : rendu d'origine
private:
    void paint3(juce::Graphics&);
    int keyAt(juce::Point<float>) const;
    AutotuneProcessor& proc;
};

// Édition note par note (mode Orbe) : courbe capturée, notes en « blobs » sur un piano-roll, déplacement, découpe, fusion,
// réglages par note, annuler / rétablir. Les modifications sont publiées vers le moteur à chaque geste.
class NoteRoll : public juce::Component {
public:
    NoteRoll(AutotuneProcessor& p, const kit::Palette& pal);
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed(const juce::KeyPress&) override;
    void tick();                      // appelé par le minuteur de l'éditeur
    void fitView();
    bool flats = false;
    float u = 1.f;
    void setTheme(const kit::v3::Theme* t);
private:
    void paint3(juce::Graphics&);
    juce::Rectangle<float> plot() const;
    float yOf(float m) const; float mOf(float y) const;
    float xOf(double pos) const; double posOf(float x) const;
    int noteAt(juce::Point<float>, int& edge) const;
    void edit(const std::function<void(std::vector<ned::Note>&)>& f, bool undoable = true);
    void syncSliders();
    AutotuneProcessor& proc;
    const kit::Palette& pal;
    kit::v3::bridge::PushKey arm, undoB, redoB, zeroB, clearB;
    const kit::v3::Theme* T = nullptr;
    kit::v3::Trace trace;
    std::vector<float> curveBuf;
    juce::Slider sForce, sExpr, sSpeed;
    juce::Label lForce, lExpr, lSpeed;
    double t0 = 0.0, t1 = 1.0;
    float mLo = 48.f, mHi = 72.f;
    int sel = -1, dragEdge = 0, lastArm = -1;
    bool dragging = false, moved = false;
    float dragStartY = 0.f, dragStartShift = 0.f;
    double dragStartPos = 0.0;
    std::vector<ned::Note> work;
};

class AutotuneEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit AutotuneEditor(AutotuneProcessor&);
    ~AutotuneEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    static kit::Palette palette();
    void showEdition(bool on) { editKey.setToggleState(on, juce::sendNotificationSync); }
    // Thème (mémorisé hors apvts si persist) et échelle de fenêtre (0,75 à 1,5 ; jusqu'à 1,8 en Classique).
    void setThemeMode(kit::v3::Mode m, bool persist = true);
    void setUiScale(float s);

private:
    static constexpr float kW3 = 1100.f, kH3 = 866.f;   // 866 : rangée « Comportement » (v3 seulement) sous la rangée de potards
    bool isV3() const { return mode != kit::v3::Mode::Classic; }
    void timerCallback() override;
    void resizedClassic();
    void resized3();
    void paint3(juce::Graphics&);
    void paintBackdrop3(juce::Graphics&, juce::Rectangle<float>);
    void paintScreen3(juce::Graphics&, juce::Rectangle<float>);
    void paintReadout3(juce::Graphics&, juce::Rectangle<float>);
    void paintScreen(juce::Graphics&, juce::Rectangle<float>);
    void paintGauge(juce::Graphics&, juce::Rectangle<float>);
    void paintReadout(juce::Graphics&, juce::Rectangle<float>);
    void module(juce::Graphics&, const char* title, float x, float w, float y);
    juce::String tonText() const;
    bool flats() const;
    juce::String nName(int midi) const;
    juce::Rectangle<float> S(float x, float y, float w, float h) const { return {x * u_, y * u_, w * u_, h * u_}; }
    juce::Rectangle<int> Si(float x, float y, float w, float h) const { return S(x, y, w, h).toNearestInt(); }

    AutotuneProcessor& proc;
    kit::Palette pal;
    kit::Look look;
    kit::v3::Mode mode = kit::v3::Mode::Light;
    kit::v3::Theme T;
    kit::v3::Look3 look3{T}, look3Screen{T};   // look3Screen : thème sombre, pour l'édition posée dans l'écran
    kit::v3::ArtCache art;
    juce::TooltipWindow tooltip{this, 600};
    float scale_ = 1.f;
    int lastProg = -1;
    juce::OwnedArray<kit::v3::Knob3> k3;
    juce::OwnedArray<kit::v3::Knob3> kb;   // comportement : warble, humanize, transition, vitesse < 0, suivi (v3 seulement)
    std::unique_ptr<kit::v3::bridge::Seg> stabSeg, atkSeg;   // stabilisateur de notes ; attaque accordée
    kit::v3::Trace trDet, trTgt, trOut;
    float sDet[atn::Telemetry::N] = {}, sTgt[atn::Telemetry::N] = {}, sOut[atn::Telemetry::N] = {};
    kit::v3::ThemeMenu themeBtn;
    float u_ = 1.f, viewC = 57.f, dispCorr = 0.f;
    int tMode = 2, tRoot = -1, tScale = 1, tFlips = 0, tMask = 0;
    float lastDet = 0.f, lastTgt = 0.f;
    juce::OwnedArray<kit::w::Pot> pots;
    juce::ComboBox keyBox, scaleBox, harmBox, presetBox;
    juce::ToggleButton formToggle;
    std::unique_ptr<kit::v3::bridge::Seg> latSeg;   // Latence : Direct / Qualité
    std::unique_ptr<kit::v3::bridge::Seg> algoSeg, accSeg, renduSeg;   // renduSeg : Satin / Précis / Robot   // Moteur : Classique / Orbe ; écriture # / b
    kit::v3::bridge::PushKey editKey;
    std::unique_ptr<NoteRoll> roll;
    int lastAcc = -1, lastAlgo = -1;
    int lastLat = -1, lastLatDecl = -1;
    kit::v3::bridge::PushKey resetNotes;
    AtKeys keys;
    kit::v3::bridge::Power power;
    kit::v3::bridge::HelpKey help;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> keyAtt, scaleAtt, harmAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> formAtt, powerAtt;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AutotuneEditor)
};
