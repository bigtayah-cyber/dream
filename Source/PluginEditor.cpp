#include "PluginEditor.h"

using namespace kit;
using juce::Colour, juce::Rectangle, juce::String;

Palette VoileEditor::palette() {
    auto p = kit::w::rack(Colour(0xFFFF5A36));   // rack graphite, bande d'accent orange
    p.accent2 = Colour(0xFFFFB199);
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
        k.getProperties().set("kitKnob", (int)KnobStyle::Skirt);   // gros boutons à jupe chiffrée
        addAndMakeVisible(k);
        knobAtt[i] = std::make_unique<SliderAtt>(ap, ids[i], k);
    }
    for (auto* s : {&mixS, &outS}) { s->setSliderStyle(juce::Slider::RotaryVerticalDrag); s->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0); s->setMouseDragSensitivity(180); addAndMakeVisible(*s); }
    mixS.setTooltip(u8("Part du son traité")); outS.setTooltip(u8("Niveau de sortie"));
    for (auto* s : {&mixS, &outS}) s->getProperties().set("kitKnob", (int)KnobStyle::Cap);
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
    prevB.setTooltip(u8("Preset précédent")); nextB.setTooltip("Preset suivant");
    help.install(*this, "Voile", {u8("Chaîne vocale en quatre gestes : la voix est recalée sur la gamme choisie, puis habillée."),
        u8("- Tension : force de la correction de hauteur (douce à gauche, dure à droite). Mordant : saturation et grain numérique."),
        u8("- Brume : écho et salle sombre derrière la voix. Halo : doubles à l'octave au-dessus et à la quinte en dessous."),
        u8("- Tonalité et gamme : la voix est recalée sur cette gamme (Chromatique : sur le demi-ton le plus proche)."),
        u8("- Aiguille : correction appliquée, en demi-tons (centre = aucune). VU : niveau de sortie."),
        u8("- Mix : part du son traité ; Sortie : niveau final.")});

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
    help.layout(kit::w::helpSlot(getLocalBounds().toFloat().reduced(18.f * u_, 0), u_, false).toNearestInt(), getLocalBounds());
    for (int i = 0; i < 4; ++i) knob[i].setBounds(Si(96.f + (float)i * 128.f - 64.f, 140.f - 64.f, 128, 128));
    presetBox.setBounds(Si(318, 8, 220, 34));
    prevB.setBounds(Si(292, 14, 18, 22)); nextB.setBounds(Si(546, 14, 18, 22));
    mixS.setBounds(Si(608 - 36, 112 - 36, 72, 72)); outS.setBounds(Si(696 - 36, 112 - 36, 72, 72));
    keyBox.setBounds(Si(564, 290, 64, 28)); scaleBox.setBounds(Si(634, 290, 104, 28));
}

void VoileEditor::timerCallback() {
    shiftSm += (proc.shift.load() - shiftSm) * 0.3f;
    lvlSm += (proc.level.load() - lvlSm) * 0.25f;
    vu.step(proc.level.load(), 1.0 / 30.0);
    repaint(S(18, 250, 540, 106).toNearestInt());
    if (presetBox.getSelectedItemIndex() != proc.getCurrentProgram() && proc.getCurrentProgram() >= 0 && !presetBox.isPopupActive())
        presetBox.setSelectedItemIndex(proc.getCurrentProgram(), juce::dontSendNotification);
}

void VoileEditor::paint(juce::Graphics& g) {
    const float u = u_;
    kit::w::drawFace(g, pal, getLocalBounds(), u, {kit::w::Family::Rack, "Voile", kit::w::serial::voile});
    screenWell(g, pal, S(318, 8, 220, 34), Display::VFD);   // la liste des presets s'affiche dans la police d'écran
    g.setColour(juce::Colours::black.withAlpha(0.5f)); g.fillRect(S(18, 246, 724, 1.5f));
    g.setColour(juce::Colours::white.withAlpha(0.06f)); g.fillRect(S(18, 247.5f, 724, 1.f));

    const char* names[4] = {"Tension", "Mordant", "Brume", "Halo"};
    const juce::String subs[4] = {u8("hauteur"), u8("grain"), u8("écho · salle"), u8("doubles")};
    for (int i = 0; i < 4; ++i) {
        const float cx = 96.f + (float)i * 128.f;
        engraved(g, pal, names[i], S(cx - 60, 206, 120, 16), juce::Justification::centred, 12.f * u, 4.f * u, true);
        g.setColour(pal.mute); g.setFont(fnt(10.f * u));
        g.drawText(subs[i], S(cx - 60, 224, 120, 12).toNearestInt(), juce::Justification::centred);
    }
    g.setColour(pal.line); g.fillRect(S(552, 62, 1, 176)); g.fillRect(S(552, 256, 1, 92));
    engraved(g, pal, "Mix", S(566, 152, 84, 14), juce::Justification::centred, 10.f * u, 2.f * u, true);
    engraved(g, pal, "Sortie", S(654, 152, 84, 14), juce::Justification::centred, 10.f * u, 2.f * u, true);
    auto val = [&](juce::Slider& s, float cx) {
        g.setColour(pal.screenInk.withAlpha(0.9f)); g.setFont(lcdFont(10.f * u));
        g.drawText(s.getTextFromValue(s.getValue()), S(cx - 40, 170, 80, 14).toNearestInt(), juce::Justification::centred);
    };
    val(mixS, 608); val(outS, 696);
    engraved(g, pal, u8("Tonalité · gamme"), S(566, 266, 172, 14), juce::Justification::centredLeft, 10.f * u, 2.f * u, true);

    // correction appliquée : galvanomètre centre zéro (±1 demi-ton) ; niveau de sortie : vrai VU
    kit::w::dial(g, S(32, 256, 220, 96), shiftSm, -1.f, 1.f, {{-1.f, -0.5f, Colour(0xFFE0603A).withAlpha(0.8f)}, {0.5f, 1.f, Colour(0xFFE0603A).withAlpha(0.8f)}},
                 {-1.f, -0.5f, 0.f, 0.5f, 1.f}, u8("Correction dt"), true);
    vuMeter(g, pal, S(268, 254, 168, 100), (float)vu.vu(), lvlSm > 0.89f);
    g.setColour(pal.mute); g.setFont(mono(9.f * u));
    g.drawText((shiftSm >= 0 ? "+" : "") + String(shiftSm, 2) + " dt", S(444, 296, 100, 14).toNearestInt(), juce::Justification::centredLeft);
}
