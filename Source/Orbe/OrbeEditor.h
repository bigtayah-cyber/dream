#pragma once
#include "../Kit/Kit.h"
#include "OrbeProcessor.h"

// Sélecteur segmenté relié à un paramètre « choix ».
class Segmented : public juce::Component {
public:
    Segmented(juce::RangedAudioParameter& p, juce::StringArray labels, const kit::Palette& pal)
        : labels_(std::move(labels)), P(pal), att(p, [this](float v) { idx = juce::roundToInt(v); repaint(); }) { att.sendInitialUpdate(); }
    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
private:
    juce::StringArray labels_;
    kit::Palette P;
    int idx = 0;
    juce::ParameterAttachment att;
};

// Poignée pour glisser le fichier MIDI vers la piste du DAW.
class MidiHandle : public juce::Component {
public:
    MidiHandle(OrbeProcessor& p, const kit::Palette& pal) : proc(p), P(pal) { setMouseCursor(juce::MouseCursor::DraggingHandCursor); }
    bool chord = true, lead = true, bass = true, drums = true;
    void paint(juce::Graphics& g) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent&) override { dragging = false; repaint(); }
private:
    OrbeProcessor& proc;
    kit::Palette P;
    bool dragging = false;
};

class OrbeEditor : public juce::AudioProcessorEditor, public juce::DragAndDropContainer, private juce::Timer {
public:
    explicit OrbeEditor(OrbeProcessor&);
    ~OrbeEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    bool keyPressed(const juce::KeyPress&) override;

    static kit::Palette palette();

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAtt = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

    void timerCallback() override;
    void paintHeader(juce::Graphics&, float u);
    void paintCards(juce::Graphics&, float u);
    void paintRoll(juce::Graphics&, float u);
    void paintToolbar(juce::Graphics&, float u);
    // piano roll éditable : la piste choisie occupe la grande zone (hauteurs ou 7 lignes de batterie)
    struct Geo { juce::Rectangle<float> a; int lo = 0, hi = 0; float stepW = 1, rowH = 1; };
    Geo geo() const;
    juce::Rectangle<float> noteRect(const orbe::Note&, const Geo&) const;
    int findNote(int tr, juce::Point<float> p, const Geo&) const;
    struct Btn { juce::Rectangle<float> r; int kind, idx; };  // kind : 0 piste, 1 verrou, 2 outil, 3 annuler, 4 rétablir
    std::vector<Btn> buttons() const;
    void chordMenu(int bar, int slot);
    void pushOnce() { if (!drag.pushed) { proc.beginEdit(); drag.pushed = true; } }
    juce::Rectangle<float> cardRect(int i, int n, float u) const;
    juce::Rectangle<float> S(float x, float y, float w, float h) const { return {x * u_, y * u_, w * u_, h * u_}; }
    juce::Rectangle<int> Si(float x, float y, float w, float h) const { return S(x, y, w, h).toNearestInt(); }

    OrbeProcessor& proc;
    kit::Palette pal;
    kit::Look look;
    juce::TooltipWindow tooltip{this, 600};
    juce::Image plate;
    float u_ = 1.f;
    int editTrack = 1, tool = 0;  // piste éditée ; outil 0 crayon, 1 gomme
    struct Drag { int mode = 0, idx = -1; float grab = 0; bool moved = false, pushed = false, added = false; } drag;  // mode 1 déplacer, 2 durée, 3 gomme

    juce::ComboBox styleBox, keyBox, scaleBox;
    std::unique_ptr<Segmented> barsSeg, arpSeg;
    juce::ToggleButton b7, bAdd9, bBorrow;
    juce::Slider densityK, humanK, masterK, fader[4];
    juce::ToggleButton mute[4], midiOut;
    juce::TextButton auditionB{juce::String::fromUTF8("Écouter")}, genAll{juce::String::fromUTF8("Générer")}, genChords{"Accords"}, genLead{"Lead"};
    std::unique_ptr<MidiHandle> handle;
    juce::TextButton chip[4];
    std::unique_ptr<ComboAtt> styleAtt, keyAtt, scaleAtt;
    std::unique_ptr<SliderAtt> densAtt, humAtt, masterAtt, volAtt[4];
    std::unique_ptr<ButtonAtt> a7, a9, aB, muteAtt[4], midiAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OrbeEditor)
};
