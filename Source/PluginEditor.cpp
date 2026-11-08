#include "PluginEditor.h"

using namespace kit;
using juce::Colour, juce::Rectangle, juce::String;

Palette VoileEditor::palette() {
    Palette p;
    p.plateTop = Colour(0xFF2E2D2B); p.plateBot = Colour(0xFF151413);
    p.ink = Colour(0xFFEDE8DE); p.mute = Colour(0xFF8D8982); p.line = Colour(0xFF3A3835);
    p.accent = Colour(0xFFFF5A36); p.accent2 = Colour(0xFFFFB199);
    p.screen = Colour(0xFF0D0C0B); p.screenInk = Colour(0xFFFFC2AE); p.brush = 1.f;
    return p;
}

VoileEditor::VoileEditor(VoileProcessor& p) : AudioProcessorEditor(&p), proc(p), pal(palette()), look(pal) {
    setLookAndFeel(&look);
    auto& ap = proc.apvts;
    const char* ids[4] = {"tension", "mordant", "brume", "halo"};
    const char* tips[4] = {"Force de la correction de hauteur : douce à gauche, dure à droite",
                           "Saturation et grain numérique",
                           "Écho et salle sombre derrière la voix",
                           "Doubles à l'octave au-dessus et à la quinte en dessous"};
    for (int i = 0; i < 4; ++i) {
        auto& k = knob[i];
        k.setSliderStyle(juce::Slider::RotaryVerticalDrag); k.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        k.setMouseDragSensitivity(220); k.setTooltip(u8(tips[i]));
        addAndMakeVisible(k);
        knobAtt[i] = std::make_unique<SliderAtt>(ap, ids[i], k);
    }
    for (auto* s : {&mixS, &outS}) { s->setSliderStyle(juce::Slider::RotaryVerticalDrag); s->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0); s->setMouseDragSensitivity(180); addAndMakeVisible(*s); }
    mixS.setTooltip(u8("Part du son traité")); outS.setTooltip(u8("Niveau de sortie"));
    mixAtt = std::make_unique<SliderAtt>(ap, "mix", mixS);
    outAtt = std::make_unique<SliderAtt>(ap, "out", outS);

    const char* keys[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    for (int i = 0; i < 12; ++i) keyBox.addItem(keys[i], i + 1);
    scaleBox.addItem("Chromatique", 1); scaleBox.addItem("Majeur", 2); scaleBox.addItem("Mineur", 3);
    keyBox.setTooltip(u8("Tonalité de la correction")); scaleBox.setTooltip(u8("Gamme de la correction"));
    addAndMakeVisible(keyBox); addAndMakeVisible(scaleBox);
    keyAtt = std::make_unique<ComboAtt>(ap, "key", keyBox);
    scaleAtt = std::make_unique<ComboAtt>(ap, "scale", scaleBox);

    for (int i = 0; i < proc.getNumPrograms(); ++i) presetBox.addItem(proc.getProgramName(i), i + 1);
    presetBox.setSelectedItemIndex(proc.getCurrentProgram(), juce::dontSendNotification);
    presetBox.onChange = [this] { proc.setCurrentProgram(presetBox.getSelectedItemIndex()); };
    addAndMakeVisible(presetBox); addAndMakeVisible(prevB); addAndMakeVisible(nextB);
    prevB.onClick = [this] { stepPreset(-1); }; nextB.onClick = [this] { stepPreset(1); };

    setResizable(true, true);
    setResizeLimits(608, 288, 1520, 720);
    getConstrainer()->setFixedAspectRatio(760.0 / 360.0);
    setSize(760, 360);
    startTimerHz(30);
}

VoileEditor::~VoileEditor() { stopTimer(); setLookAndFeel(nullptr); }

void VoileEditor::stepPreset(int d) {
    const int n = proc.getNumPrograms();
    presetBox.setSelectedItemIndex(((presetBox.getSelectedItemIndex() + d) % n + n) % n, juce::sendNotification);
}

void VoileEditor::resized() {
    u_ = (float)getWidth() / 760.f;
    plate = brushedPlate(getWidth(), getHeight(), pal);
    for (int i = 0; i < 4; ++i) knob[i].setBounds(Si(100.f + (float)i * 140.f - 66.f, 190.f - 66.f, 132, 132));
    presetBox.setBounds(Si(338, 20, 196, 30));
    prevB.setBounds(Si(304, 20, 28, 30)); nextB.setBounds(Si(540, 20, 28, 30));
    keyBox.setBounds(Si(612, 112, 56, 28)); scaleBox.setBounds(Si(674, 112, 70, 28));
    mixS.setBounds(Si(630 - 38, 232 - 38, 76, 76)); outS.setBounds(Si(716 - 38, 232 - 38, 76, 76));
}

void VoileEditor::timerCallback() {
    shiftSm += (proc.shift.load() - shiftSm) * 0.3f;
    lvlSm += (proc.level.load() - lvlSm) * 0.25f;
    repaint(S(20, 292, 560, 52).toNearestInt());
    if (presetBox.getSelectedItemIndex() != proc.getCurrentProgram() && proc.getCurrentProgram() >= 0 && !presetBox.isPopupActive())
        presetBox.setSelectedItemIndex(proc.getCurrentProgram(), juce::dontSendNotification);
}

void VoileEditor::paint(juce::Graphics& g) {
    const float u = u_;
    g.drawImageAt(plate, 0, 0);
    for (auto p : {juce::Point<float>(12, 12), {748, 12}, {12, 348}, {748, 348}}) screw(g, p.x * u, p.y * u, 5.f * u, (p.x + p.y) * 0.8f);
    g.setColour(juce::Colours::black.withAlpha(0.5f)); g.fillRect(S(0, 70, 760, 1.5f)); g.fillRect(S(0, 282, 760, 1.5f));
    g.setColour(juce::Colours::white.withAlpha(0.06f)); g.fillRect(S(0, 71.5f, 760, 1.f)); g.fillRect(S(0, 283.5f, 760, 1.f));

    engraved(g, pal, "Voile", S(28, 18, 200, 34), juce::Justification::centredLeft, 28.f * u, 9.f * u, true);
    g.setColour(pal.mute); g.setFont(fnt(10.f * u));
    g.drawText(u8("Chaîne vocale · dreamaws"), S(30, 50, 240, 14).toNearestInt(), juce::Justification::centredLeft);
    g.setColour(pal.mute); g.setFont(mono(9.f * u));
    g.drawText("DW-100", S(660, 22, 80, 12), juce::Justification::centredRight);

    const char* names[4] = {"Tension", "Mordant", "Brume", "Halo"};
    const juce::String subs[4] = {u8("hauteur"), u8("grain"), u8("écho · salle"), u8("doubles")};
    for (int i = 0; i < 4; ++i) {
        const float cx = 100.f + (float)i * 140.f;
        engraved(g, pal, names[i], S(cx - 60, 246, 120, 16), juce::Justification::centred, 12.f * u, 4.f * u, true);
        g.setColour(pal.mute); g.setFont(fnt(10.f * u));
        g.drawText(subs[i], S(cx - 60, 264, 120, 12).toNearestInt(), juce::Justification::centred);
    }
    // panneau droit
    g.setColour(pal.line); g.fillRect(S(590, 90, 1, 180));
    engraved(g, pal, u8("Tonalité"), S(612, 94, 130, 14), juce::Justification::centredLeft, 10.f * u, 2.f * u);
    g.setColour(pal.mute); g.setFont(fnt(9.5f * u));
    g.drawText(u8("la voix est recalée sur cette gamme"), S(612, 142, 132, 12).toNearestInt(), juce::Justification::centredLeft);
    engraved(g, pal, "Mix", S(588, 276 - 14, 84, 14), juce::Justification::centred, 10.f * u, 2.f * u);
    engraved(g, pal, "Sortie", S(674, 276 - 14, 84, 14), juce::Justification::centred, 10.f * u, 2.f * u);

    // afficheur de preset
    screenWell(g, pal, S(338, 20, 196, 30));

    // bas : correction appliquée et niveau
    auto d = S(28, 296, 548, 40);
    screenWell(g, pal, d);
    g.setColour(pal.mute); g.setFont(mono(9.f * u));
    g.drawText("CORRECTION", Rectangle<float>(d.getX() + 12 * u, d.getY() + 5 * u, 90 * u, 12 * u), juce::Justification::centredLeft);
    g.setColour(pal.screenInk); g.setFont(mono(15.f * u));
    g.drawText((shiftSm >= 0 ? "+" : "") + String(shiftSm, 1) + " st", Rectangle<float>(d.getX() + 12 * u, d.getY() + 17 * u, 110 * u, 18 * u), juce::Justification::centredLeft);
    // jauge de correction centrée (-6.5..+6.5)
    auto gb = Rectangle<float>(d.getX() + 140 * u, d.getCentreY() - 3 * u, 200 * u, 6 * u);
    g.setColour(juce::Colours::white.withAlpha(0.08f)); g.fillRoundedRectangle(gb, 3 * u);
    g.setColour(juce::Colours::white.withAlpha(0.3f)); g.fillRect(gb.getCentreX() - 0.5f, gb.getY() - 3 * u, 1.f, gb.getHeight() + 6 * u);
    const float sx = juce::jlimit(-1.f, 1.f, shiftSm / 6.5f);
    g.setColour(pal.accent);
    g.fillRoundedRectangle(sx >= 0 ? Rectangle<float>(gb.getCentreX(), gb.getY(), sx * gb.getWidth() / 2, gb.getHeight()) : Rectangle<float>(gb.getCentreX() + sx * gb.getWidth() / 2, gb.getY(), -sx * gb.getWidth() / 2, gb.getHeight()), 2.f * u);
    g.setColour(pal.mute); g.setFont(mono(9.f * u));
    g.drawText("NIVEAU", Rectangle<float>(d.getX() + 370 * u, d.getY() + 5 * u, 80 * u, 12 * u), juce::Justification::centredLeft);
    const int nseg = 24; const float db = 20.f * std::log10(juce::jmax(1e-5f, lvlSm));
    for (int i = 0; i < nseg; ++i) {
        const float thr = -48.f + 48.f * (float)i / (float)nseg;
        const bool on = db > thr;
        const Colour c = i > nseg - 4 ? Colour(0xFFFF3B30) : i > nseg - 8 ? Colour(0xFFFFB02E) : pal.accent2;
        g.setColour(on ? c : juce::Colours::white.withAlpha(0.07f));
        g.fillRoundedRectangle(d.getX() + 370 * u + (float)i * 7.2f * u, d.getY() + 20 * u, 5.4f * u, 14 * u, 1.f * u);
    }
}
