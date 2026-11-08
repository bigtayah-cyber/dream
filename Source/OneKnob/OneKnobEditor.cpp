#include "OneKnobEditor.h"

using namespace kit;
using juce::Colour, juce::Rectangle, juce::String;

namespace {
const char* kKeyName[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
constexpr float kBoxY = 518.f, kBoxH = 22.f;
}

void InSwitch::paintButton(juce::Graphics& g, bool hover, bool down) {
    auto r = getLocalBounds().toFloat();
    const float cx = r.getCentreX(), cy = r.getCentreY() - 6, rad = juce::jmin(r.getWidth(), r.getHeight() - 14) * 0.42f;
    const bool on = !getToggleState();  // le paramètre est « bypass » : allumé quand l'effet travaille
    g.setColour(juce::Colours::black.withAlpha(0.55f));
    g.fillEllipse(cx - rad - 2, cy - rad, (rad + 2) * 2, (rad + 2) * 2);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xFF6A6A70), cx - rad * 0.6f, cy - rad, juce::Colour(0xFF17171A), cx + rad * 0.6f, cy + rad, false));
    g.fillEllipse(cx - rad, cy - rad + (down ? 1.f : 0.f), rad * 2, rad * 2);
    g.setColour(juce::Colours::white.withAlpha(hover ? 0.3f : 0.14f));
    g.drawEllipse(cx - rad, cy - rad, rad * 2, rad * 2, 1.f);
    led(g, cx, cy, rad * 0.28f, on, P.accent);
    tracked(g, "In", Rectangle<float>(0, r.getBottom() - 12, r.getWidth(), 12), juce::Justification::centred, 1.6f, fnt(9.f, true));
}

Palette OneKnobEditor::palette(int i) {
    Palette p;
    switch (i) {
        case 0:  // Grain Nord : acier bleu
            p.plateTop = Colour(0xFF334456); p.plateBot = Colour(0xFF18232E); p.ink = Colour(0xFFE6F0F7); p.accent = Colour(0xFF7FD1FF); p.accent2 = Colour(0xFFB5E6FF); break;
        case 1:  // Gorge : rouille et ambre
            p.plateTop = Colour(0xFF4F2A24); p.plateBot = Colour(0xFF2A1410); p.ink = Colour(0xFFF3E4D6); p.accent = Colour(0xFFFF9A4D); p.accent2 = Colour(0xFFFFC18A); break;
        case 2:  // Poupée : prune sombre, rose vif
            p.plateTop = Colour(0xFF3A2640); p.plateBot = Colour(0xFF170F1B); p.ink = Colour(0xFFF6E6F1); p.accent = Colour(0xFFFF4FA3); p.accent2 = Colour(0xFFFF9BCD); break;
        default:  // Patine : sauge et laiton
            p.plateTop = Colour(0xFF424838); p.plateBot = Colour(0xFF202419); p.ink = Colour(0xFFF0EBD9); p.accent = Colour(0xFFE0C677); p.accent2 = Colour(0xFFF2E3A8); break;
    }
    p.mute = p.ink.withAlpha(0.55f); p.line = p.ink.withAlpha(0.14f);
    p.screen = Colour(0xFF0C0C0E); p.screenInk = p.accent2; p.brush = 1.1f;
    return p;
}

OneKnobEditor::OneKnobEditor(OneKnobProcessor& p)
    : AudioProcessorEditor(&p), proc(p), pal(palette(OneKnobProcessor::index())), look(pal), inSw(pal) {
    setLookAndFeel(&look);
    knobS.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    knobS.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    knobS.setMouseDragSensitivity(260);
    knobS.setDoubleClickReturnValue(true, OneKnobProcessor::voicing().defaultK * 10.0);
    knobS.setTooltip(juce::String::fromUTF8("Caractère. Double-clic : valeur d'origine."));
    addAndMakeVisible(knobS);
    knobAtt = std::make_unique<SliderAtt>(proc.apvts, "char", knobS);
    addAndMakeVisible(inSw);
    inSw.setTooltip("Effet en service. Appuie pour le couper (la latence reste compensée).");
    inAtt = std::make_unique<ButtonAtt>(proc.apvts, "bypass", inSw);
    onek::Mapped m; m.eng.tension = 0; OneKnobProcessor::voicing().map(1.f, m);
    usesPitch = m.eng.tension > 0.01f;
    // sélecteurs de tonalité et de gamme (paramètres sauvegardés avec le projet)
    for (auto* b : {&keyBox, &scaleBox}) { addAndMakeVisible(*b); b->setJustificationType(juce::Justification::centredLeft); b->setScrollWheelEnabled(false); }
    keyBox.addItemList(juce::StringArray{"Auto", "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"}, 1);
    scaleBox.addItemList(juce::StringArray{"Auto", "Majeur", "Mineur"}, 1);
    keyAtt = std::make_unique<ComboAtt>(proc.apvts, "key", keyBox);
    scaleAtt = std::make_unique<ComboAtt>(proc.apvts, "scale", scaleBox);
    keyBox.setTooltip(u8("Tonalité. Auto : détectée sur ta voix, et AUCUNE correction vers une note tant qu'elle n'est pas fiable. Ou impose la tonique."));
    scaleBox.setTooltip(u8("Gamme. Auto : majeur ou mineur détecté. Ou impose majeur / mineur."));
    if (!usesPitch) {
        for (auto* b : {&keyBox, &scaleBox}) { b->setEnabled(false); b->setAlpha(0.35f); }
        keyBox.setTooltip(u8("Ce modèle ne corrige pas la hauteur : la tonalité ne sert à rien ici."));
        scaleBox.setTooltip(keyBox.getTooltip());
    }
    setResizable(true, true);
    setResizeLimits(240, 448, 600, 1120);
    getConstrainer()->setFixedAspectRatio(300.0 / 560.0);
    setSize(300, 560);
    startTimerHz(30);
}

OneKnobEditor::~OneKnobEditor() { stopTimer(); setLookAndFeel(nullptr); }

void OneKnobEditor::resized() {
    u_ = (float)getWidth() / 300.f;
    plate = brushedPlate(getWidth(), getHeight(), pal);
    knobS.setBounds(Si(150 - 105, 348 - 105, 210, 210));
    inSw.setBounds(Si(222, 486, 54, 56));
    look.u = u_;
    keyBox.setBounds(Si(24, kBoxY, 84, kBoxH));
    scaleBox.setBounds(Si(116, kBoxY, 84, kBoxH));
}

void OneKnobEditor::timerCallback() {
    const float lv = proc.level.load();
    const float db = 20.f * std::log10(juce::jmax(1e-5f, lv));
    const float target = juce::jlimit(-20.f, 3.f, db + 18.f);  // 0 VU = -18 dBFS
    needle += (target - needle) * (target > needle ? 0.22f : 0.10f);
    repaint(S(24, 96, 252, 110).toNearestInt());
    ++tick;
    repaint(S(20, 474, 186, 38).toNearestInt());
}

void OneKnobEditor::paintVu(juce::Graphics& g, Rectangle<float> r) {
    const float u = u_;
    screenWell(g, pal, r.expanded(0));
    auto f = r.reduced(6 * u);
    g.setGradientFill(juce::ColourGradient(Colour(0xFFEFE6C8), f.getX(), f.getY(), Colour(0xFFC9BC91), f.getX(), f.getBottom(), false));
    g.fillRoundedRectangle(f, 3 * u);
    g.setGradientFill(juce::ColourGradient(juce::Colours::transparentBlack, f.getCentreX(), f.getCentreY(), juce::Colours::black.withAlpha(0.28f), f.getX(), f.getY(), true));
    g.fillRoundedRectangle(f, 3 * u);
    // pivot sous la face, arc de graduation
    const float px = f.getCentreX(), py = f.getBottom() + 30 * u, R = 112 * u;
    auto angleOf = [&](float vu) { return juce::jmap(vu, -20.f, 3.f, -0.80f, 0.80f); };  // autour de la verticale
    auto pt = [&](float vu, float rad) { const float a = angleOf(vu); return juce::Point<float>(px + std::sin(a) * rad, py - std::cos(a) * rad); };
    juce::Path red; red.addCentredArc(px, py, R, R, 0, angleOf(0.f), angleOf(3.f), true);
    g.setColour(Colour(0xFFB3322B)); g.strokePath(red, juce::PathStrokeType(3.f * u));
    juce::Path blk; blk.addCentredArc(px, py, R, R, 0, angleOf(-20.f), angleOf(0.f), true);
    g.setColour(Colour(0xFF1B1A17)); g.strokePath(blk, juce::PathStrokeType(1.2f * u));
    const int marks[] = {-20, -10, -7, -5, -3, -2, -1, 0, 1, 2, 3};
    for (int v : marks) {
        const bool major = (v == -20 || v == -10 || v == -5 || v == 0 || v == 3);
        g.setColour(v >= 0 ? Colour(0xFFB3322B) : Colour(0xFF1B1A17));
        g.drawLine({pt((float)v, R - (major ? 9.f : 5.f) * u), pt((float)v, R + 1 * u)}, major ? 1.4f : 1.f);
        if (major) { g.setFont(fnt(9.5f * u, true)); auto q = pt((float)v, R - 20 * u); g.drawText(String(std::abs(v)), Rectangle<float>(q.x - 10 * u, q.y - 6 * u, 20 * u, 12 * u), juce::Justification::centred); }
    }
    g.setColour(Colour(0xFF1B1A17)); g.setFont(fnt(10.f * u, true));
    g.drawText("VU", Rectangle<float>(f.getCentreX() - 20 * u, f.getBottom() - 26 * u, 40 * u, 14 * u), juce::Justification::centred);
    // aiguille : ombre puis trait
    const auto tip = pt(needle, R + 2 * u), base = pt(needle, 18 * u);
    g.saveState(); g.reduceClipRegion(f.toNearestInt());
    g.setColour(juce::Colours::black.withAlpha(0.22f));
    g.drawLine({base.translated(2 * u, 2 * u), tip.translated(2 * u, 2 * u)}, 1.6f);
    g.setColour(Colour(0xFF121110));
    g.drawLine({base, tip}, 1.5f);
    g.restoreState();
    // vitre : reflet diagonal
    g.setGradientFill(juce::ColourGradient(juce::Colours::white.withAlpha(0.16f), f.getX(), f.getY(), juce::Colours::white.withAlpha(0.0f), f.getCentreX(), f.getCentreY(), false));
    g.fillRoundedRectangle(f, 3 * u);
}

void OneKnobEditor::paint(juce::Graphics& g) {
    const float u = u_;
    const auto& vc = OneKnobProcessor::voicing();
    g.drawImageAt(plate, 0, 0);
    // rails de rack : fentes de fixation en haut et en bas
    for (float y : {14.f, 546.f}) {
        for (float x : {14.f, 286.f}) screw(g, x * u, y * u, 5.f * u, (x + y) * 0.7f);
    }
    g.setColour(juce::Colours::black.withAlpha(0.45f)); g.fillRect(S(0, 0, 3, 560)); g.fillRect(S(297, 0, 3, 560));
    g.setColour(juce::Colours::white.withAlpha(0.07f)); g.fillRect(S(3, 0, 1, 560)); g.fillRect(S(296, 0, 1, 560));

    // en-tête : modèle, nom, description
    g.setColour(pal.mute); g.setFont(mono(9.f * u));
    g.drawText(vc.serial, S(220, 30, 60, 12), juce::Justification::centredRight);
    engraved(g, pal, "dreamaws", S(24, 30, 120, 12), juce::Justification::centredLeft, 9.f * u, 2.4f * u);
    engraved(g, pal, u8(vc.name), S(24, 46, 252, 34), juce::Justification::centredLeft, 26.f * u, 4.f * u, true);
    g.setColour(pal.mute); g.setFont(fnt(10.f * u));
    g.drawText(u8(vc.tag), S(24, 80, 252, 14), juce::Justification::centredLeft);

    paintVu(g, S(24, 102, 252, 96));

    // graduation numérotée autour du potard
    const float cx = 150 * u, cy = 348 * u, r = 70 * u;
    for (int i = 0; i <= 10; ++i) {
        const float a = juce::MathConstants<float>::pi * (0.75f + 1.5f * (float)i / 10.f);
        g.setColour(pal.ink.withAlpha(i % 5 == 0 ? 0.95f : 0.7f));
        g.setFont(fnt(11.f * u, true));
        g.drawText(String(i), Rectangle<float>(cx + std::cos(a) * r * 1.78f - 10 * u, cy + std::sin(a) * r * 1.78f - 7 * u, 20 * u, 14 * u), juce::Justification::centred);
    }
    engraved(g, pal, u8("Caractère"), S(50, 456, 200, 16), juce::Justification::centred, 13.f * u, 5.f * u, true);

    // afficheur de la tonalité réellement utilisée + état (manuel / auto / recherche)
    const int pc = proc.keyPc.load(), st = proc.keyState.load();
    auto kd = S(24, 478, 176, 26);
    screenWell(g, pal, kd);
    g.setFont(mono(9.f * u)); g.setColour(pal.mute);
    g.drawText(usesPitch ? "TON" : "---", kd.withTrimmedLeft(10 * u).withWidth(30 * u), juce::Justification::centredLeft);
    const bool searching = usesPitch && st == 2;
    String key = !usesPitch ? String("-") : (pc < 0 || searching) ? String("...") : String(kKeyName[pc]) + (proc.keyScale.load() == 2 ? " MIN" : " MAJ");
    g.setFont(mono(15.f * u)); g.setColour(searching || !usesPitch ? pal.mute : pal.screenInk);
    g.drawText(key, kd.withTrimmedLeft(40 * u).withWidth(76 * u), juce::Justification::centredLeft);
    const char* stTxt = !usesPitch ? "LIBRE" : st == 0 ? "MANUEL" : st == 1 ? "AUTO" : "RECHERCHE";
    g.setFont(mono(8.5f * u)); g.setColour(pal.mute);
    g.drawText(stTxt, kd.withTrimmedRight(22 * u), juce::Justification::centredRight);
    const bool lit = usesPitch && (st != 2 || (tick / 8) % 2 == 0);   // clignote tant que la tonalité n'est pas fiable
    led(g, kd.getRight() - 12 * u, kd.getCentreY(), 3.2f * u, lit, st == 2 ? pal.accent2 : pal.accent);
    // petites légendes au-dessus des sélecteurs
    g.setColour(pal.mute); g.setFont(fnt(7.5f * u, true));
    tracked(g, u8("TONALITÉ"), S(26, 508.5f, 80, 9), juce::Justification::centredLeft, 1.4f * u, fnt(7.5f * u, true));
    tracked(g, "GAMME", S(118, 508.5f, 80, 9), juce::Justification::centredLeft, 1.4f * u, fnt(7.5f * u, true));
}
