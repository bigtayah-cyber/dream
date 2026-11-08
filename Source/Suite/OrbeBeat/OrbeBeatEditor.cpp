#include "OrbeBeatEditor.h"

using namespace kit;
using juce::Colour, juce::Justification, juce::Rectangle, juce::String;
namespace Colours = juce::Colours;
using suiteui::kLilac, suiteui::kIce, suiteui::kPeach;
using obeat::kLanes;

namespace {
Colour laneColour(int i) { return Colour(obeat::lanes()[i].rgb); }
String letter(int midi) { return suiteui::noteName(midi).removeCharacters("0123456789-"); }
}  // namespace

// ---------------- grille ----------------
bool OrbeBeatGrid::locate(juce::Point<float> p, int& lane, int& step) const {
    const int st = proc.steps();
    const float rh = (float)getHeight() / (float)kLanes, cw = (float)getWidth() / (float)st;
    lane = (int)std::floor(p.y / rh); step = (int)std::floor(p.x / cw);
    return lane >= 0 && lane < kLanes && step >= 0 && step < st;
}

void OrbeBeatGrid::apply(int li, int s, bool first, bool alt) {
    const int pat = proc.currentPattern();
    const auto cur = proc.cell(pat, li, s);
    auto sound = [&](obeat::Cell c, float v) { proc.preview(li, v, li == obeat::B ? proc.noteOfDegree(obeat::degOf(c)) : obeat::lanes()[li].midi); };
    if (tool == Draw) {
        if (alt && first) { if (obeat::filled(cur)) proc.setCell(pat, li, s, obeat::withRoll(cur, obeat::rollOf(cur) % 4 + 1)); return; }
        if (first) paintMode = obeat::filled(cur) ? 2 : 1;
        if (paintMode == 1 && !obeat::filled(cur)) { const auto c = obeat::mk(1.f); proc.setCell(pat, li, s, c); sound(c, .8f); }
        else if (paintMode == 2 && obeat::filled(cur)) proc.setCell(pat, li, s, 0);
    } else if (!obeat::filled(cur)) {
        if (first) proc.setCell(pat, li, s, obeat::mk(1.f));
    } else if (first) {
        if (tool == Roll) proc.setCell(pat, li, s, obeat::withRoll(cur, obeat::rollOf(cur) % 4 + 1));
        else if (tool == Vel) { const float v = obeat::velOf(cur); proc.setCell(pat, li, s, obeat::withVel(cur, v > .9f ? .7f : v > .6f ? .45f : 1.f)); }
        else if (tool == Note && li == obeat::B) { const auto c = obeat::withDeg(cur, (obeat::degOf(cur) + 1) % 8); proc.setCell(pat, li, s, c); sound(c, .8f); }
    }
    repaint();
}

void OrbeBeatGrid::mouseDown(const juce::MouseEvent& e) { int l, s; if (locate(e.position, l, s)) apply(l, s, true, e.mods.isAltDown()); }
void OrbeBeatGrid::mouseDrag(const juce::MouseEvent& e) { int l, s; if (tool == Draw && paintMode && locate(e.position, l, s)) apply(l, s, false, false); }

void OrbeBeatGrid::paint(juce::Graphics& g) {
    const int st = proc.steps(), pat = proc.currentPattern();
    const float u = (float)getHeight() / (OrbeBeatEditor::kRow * kLanes);
    const float rh = (float)getHeight() / (float)kLanes, cw = (float)getWidth() / (float)st;
    const float ps = proc.playStep.load();
    const int cur = ps >= 0.f && proc.playPat.load() == pat ? (int)ps : -1;
    for (int li = 0; li < kLanes; ++li) {
        const Colour col = laneColour(li);
        const bool aud = proc.audible(li);
        const float y = rh * (float)li;
        for (int s = 0; s < st; ++s) {
            auto r = Rectangle<float>(cw * (float)s + 1.5f * u, y + 3.f * u, cw - 3.f * u, rh - 6.f * u);
            g.setColour(Colours::white.withAlpha(s == cur ? 0.10f : s % 4 == 0 ? 0.075f : 0.04f));
            g.fillRoundedRectangle(r, 2.5f * u);
            if (s == cur) { g.setColour(kLilac.withAlpha(0.7f)); g.drawRoundedRectangle(r, 2.5f * u, 1.2f); }
            const auto c = proc.cell(pat, li, s);
            if (!obeat::filled(c)) continue;
            const float a = (aud ? 0.45f + 0.5f * obeat::velOf(c) : 0.28f) * (s == cur ? 1.1f : 1.f);
            const int rep = obeat::rollOf(c);
            for (int k = 0; k < rep; ++k) {
                const float sw = r.getWidth() / (float)rep;
                auto b = Rectangle<float>(r.getX() + sw * (float)k + (rep > 1 ? 0.5f : 0.f), r.getY(), sw - (rep > 1 ? 1.5f * u : 0.f), r.getHeight());
                g.setGradientFill(juce::ColourGradient(col.withAlpha(juce::jmin(1.f, a)), b.getX(), b.getY(), col.withAlpha(juce::jmin(1.f, a * 0.72f)), b.getX(), b.getBottom(), false));
                g.fillRoundedRectangle(b, 2.5f * u);
                g.setColour(Colours::white.withAlpha(0.25f * a)); g.fillRect(b.getX() + 2.f * u, b.getY() + 1.f, b.getWidth() - 4.f * u, 1.f);
            }
            g.setColour(Colour(0xFF0A0A0D).withAlpha(0.85f)); g.setFont(mono(juce::jmin(10.5f * u, cw * 0.38f)));
            const String t = li == obeat::B ? letter(proc.noteOfDegree(obeat::degOf(c))) : rep > 1 ? (rep == 3 ? String("3") : String(juce::CharPointer_UTF8("\xC3\x97")) + String(rep)) : String();
            if (t.isNotEmpty()) g.drawText(t, r.reduced(3.f * u, 0).toNearestInt(), Justification::centredLeft);
        }
    }
    if (st == 32) { g.setColour(Colours::white.withAlpha(0.35f)); g.fillRect(cw * 16.f - 0.5f, 0.f, 1.f, (float)getHeight()); }
}

// ---------------- éditeur ----------------
Palette OrbeBeatEditor::palette() {
    Palette p;
    p.plateTop = Colour(0xFF2B2B31); p.plateBot = Colour(0xFF16161A);
    p.ink = Colour(0xFFE9E7F0); p.mute = Colour(0xFF8F8D9B); p.line = Colour(0xFF3B3B44);
    p.accent = Colour(0xFFB9A9FF); p.accent2 = kIce;
    p.screen = Colour(0xFF0B0C11); p.screenInk = Colour(0xFFEDEBF6);
    return p;
}

OrbeBeatEditor::OrbeBeatEditor(OrbeBeatProcessor& p) : AudioProcessorEditor(&p), proc(p), pal(palette()), look(pal), grid(p) {
    setLookAndFeel(&look);
    auto& ap = proc.apvts;
    for (int i = 0; i < obeat::kKits; ++i) kitBox.addItem(obeat::kits()[i].label, i + 1);
    for (int i = 0; i < 12; ++i) keyBox.addItem(suiteui::noteName(i + 12).removeCharacters("0123456789-"), i + 1);
    for (auto* c : {&kitBox, &keyBox}) { addAndMakeVisible(c); c->setJustificationType(Justification::centredLeft); }
    kitAtt = std::make_unique<ComboAtt>(ap, "kit", kitBox);
    keyAtt = std::make_unique<ComboAtt>(ap, "key", keyBox);
    kitBox.setTooltip(u8("Kit : sons et tempo conseillé de la boîte à rythmes"));
    keyBox.setTooltip(u8("Tonique de la piste 808"));

    patSeg = std::make_unique<suiteui::Segmented>(*ap.getParameter("pat"), juce::StringArray{"A", "B", "C", "D"}, pal);
    stepsSeg = std::make_unique<suiteui::Segmented>(*ap.getParameter("steps"), juce::StringArray{"16", "32"}, pal);
    chainSeg = std::make_unique<suiteui::Segmented>(*ap.getParameter("chain"), juce::StringArray{"Aucun", "A B", "A B C D"}, pal);
    midiSeg = std::make_unique<suiteui::Segmented>(*ap.getParameter("midimap"), juce::StringArray{"GM + 808", u8("Canal 10 = batterie")}, pal);
    modeSeg = std::make_unique<suiteui::Segmented>(*ap.getParameter("mode"), juce::StringArray{"Mineur", "Majeur"}, pal);
    patSeg->setTooltip(u8("Pattern à éditer et à jouer (A à D)"));
    chainSeg->setTooltip(u8("Joue les patterns à la suite, un par durée de pattern, en suivant le transport"));
    midiSeg->setTooltip(u8("GM + 808 : les notes de batterie GM déclenchent les pistes, les autres notes jouent l'808. Canal 10 = batterie : seul le canal 10 est de la batterie, le reste joue l'808."));
    for (auto* c : {(juce::Component*)patSeg.get(), (juce::Component*)stepsSeg.get(), (juce::Component*)chainSeg.get(), (juce::Component*)midiSeg.get(), (juce::Component*)modeSeg.get(), (juce::Component*)&grid}) addAndMakeVisible(c);

    for (auto* b : {&genB, &varyB, &clearB, &auditionB}) addAndMakeVisible(b);
    genB.setTooltip(u8("Remplit le pattern courant avec un motif adapté au kit"));
    varyB.setTooltip(u8("Copie le pattern courant dans le suivant avec des rolls, des trous et une relance de caisse claire"));
    clearB.setTooltip(u8("Efface le pattern courant"));
    auditionB.setClickingTogglesState(true);
    auditionB.setTooltip(u8("Joue le séquenceur ici quand le DAW est à l'arrêt. Dans le DAW, la lecture suit directement le transport."));
    genB.onClick = [this] { proc.generateCurrent(); grid.repaint(); };
    varyB.onClick = [this] { proc.varyCurrent(); grid.repaint(); };
    clearB.onClick = [this] { proc.clearCurrent(); grid.repaint(); };
    auditionB.onClick = [this] { proc.audition = auditionB.getToggleState(); auditionB.setButtonText(auditionB.getToggleState() ? "Stop" : u8("Écouter")); };

    const String tt[4] = {u8("Poser et effacer des pas (glisser pour peindre ; Alt : roll)"), u8("Cliquer un pas : 1, 2, 3 ou 4 frappes dans le pas"), u8("Cliquer un pas : fort, moyen, doux"), u8("Piste 808 : cliquer un pas change le degré de la note")};
    const String tl[4] = {"Dessin", "Roll", u8("Vélo"), "Note"};
    for (int i = 0; i < 4; ++i) {
        toolB[i].setButtonText(tl[i]); toolB[i].setClickingTogglesState(true); toolB[i].setRadioGroupId(41); toolB[i].setTooltip(tt[i]);
        toolB[i].onClick = [this, i] { grid.tool = (OrbeBeatGrid::Tool)i; };
        addAndMakeVisible(toolB[i]);
    }
    toolB[0].setToggleState(true, juce::dontSendNotification);

    seqB.setButtonText(u8("Séquenceur")); addAndMakeVisible(seqB);
    seqAtt = std::make_unique<ButtonAtt>(ap, "seq", seqB);
    seqB.setTooltip(u8("Joue les patterns en suivant le transport de l'hôte. Désactivé : seules les notes MIDI jouent."));

    for (auto* s : {&swingK, &masterK}) { s->setSliderStyle(juce::Slider::RotaryVerticalDrag); s->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0); s->setMouseDragSensitivity(180); addAndMakeVisible(s); }
    swingAtt = std::make_unique<SliderAtt>(ap, "swing", swingK);
    masterAtt = std::make_unique<SliderAtt>(ap, "master", masterK);
    swingK.setTooltip(u8("Swing : retarde un pas sur deux")); masterK.setTooltip(u8("Volume général"));
    swingK.setDoubleClickReturnValue(true, 0.0); masterK.setDoubleClickReturnValue(true, 0.85);

    for (int i = 0; i < kLanes; ++i) {
        vol[i].setSliderStyle(juce::Slider::LinearHorizontal); vol[i].setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0); addAndMakeVisible(vol[i]);
        volAtt[i] = std::make_unique<SliderAtt>(ap, "vol" + String(i), vol[i]);
        vol[i].setTooltip(u8("Volume de la piste"));
        muteB[i].setButtonText("M"); soloB[i].setButtonText("S");
        muteB[i].setTooltip("Muet"); soloB[i].setTooltip("Solo");
        for (auto* b : {&muteB[i], &soloB[i]}) { b->setClickingTogglesState(true); addAndMakeVisible(b); }
        muteAtt[i] = std::make_unique<ButtonAtt>(ap, "mute" + String(i), muteB[i]);
        soloAtt[i] = std::make_unique<ButtonAtt>(ap, "solo" + String(i), soloB[i]);
    }

    handle = std::make_unique<suiteui::DragHandle>([this] { return proc.writeMidiFile(); }, pal, "Glisser le MIDI", u8("batterie + 808 vers une piste du DAW"));
    addAndMakeVisible(*handle);
    handle->setTooltip(u8("Glisse les patterns (selon l'enchaînement) vers une piste du DAW : batterie sur le canal 10, 808 sur le canal 1"));

    setResizable(true, true);
    setResizeLimits(880, 512, 1760, 1024);
    getConstrainer()->setFixedAspectRatio((double)kW / (double)kH);
    setSize((int)kW, (int)kH);
    startTimerHz(30);
}

OrbeBeatEditor::~OrbeBeatEditor() { stopTimer(); setLookAndFeel(nullptr); }

void OrbeBeatEditor::resized() {
    u_ = (float)getWidth() / kW;
    plate = brushedPlate(getWidth(), getHeight(), pal);
    kitBox.setBounds(Si(24, 98, 180, 30));
    patSeg->setBounds(Si(220, 98, 186, 30));
    genB.setBounds(Si(424, 98, 96, 30)); varyB.setBounds(Si(528, 98, 96, 30)); clearB.setBounds(Si(632, 98, 70, 30)); auditionB.setBounds(Si(710, 98, 96, 30));
    for (int i = 0; i < 4; ++i) toolB[i].setBounds(Si(836.f + (float)i * 60.f, 98, 58, 30));
    grid.setBounds(Si(kGridX, kGridY, kGridW, kRow * kLanes));
    for (int i = 0; i < kLanes; ++i) {
        const float y = kGridY + kRow * (float)i + 4.f;
        muteB[i].setBounds(Si(166, y, 26, 26)); soloB[i].setBounds(Si(196, y, 26, 26));
        vol[i].setBounds(Si(228, y, 80, 26));
    }
    swingK.setBounds(Si(32, 468, 96, 96)); masterK.setBounds(Si(148, 468, 96, 96));
    stepsSeg->setBounds(Si(300, 470, 96, 28)); chainSeg->setBounds(Si(410, 470, 210, 28));
    midiSeg->setBounds(Si(300, 530, 320, 28));
    keyBox.setBounds(Si(660, 470, 96, 28));
    modeSeg->setBounds(Si(766, 470, 112, 28));
    seqB.setBounds(Si(660, 534, 200, 22));
    handle->setBounds(Si(900, 470, 176, 74));
    repaint();
}

void OrbeBeatEditor::timerCallback() {
    meterShown = juce::jmax(proc.meter.load(), meterShown * 0.9f);
    bool any = proc.playStep.load() >= 0.f;
    for (auto& f : proc.flash) { const float v = f.load(); if (v > 0.f) { any = true; f.store(v < 0.05f ? 0.f : v * 0.8f); } }
    if (any || meterShown > 0.001f) { repaint(S(0, 0, kW, 70).toNearestInt()); repaint(S(24, 134, kW - 48, kRow * kLanes + 22).toNearestInt()); }
    if (!proc.audition.load() && auditionB.getToggleState()) { auditionB.setToggleState(false, juce::dontSendNotification); auditionB.setButtonText(u8("Écouter")); }
}

void OrbeBeatEditor::paintHeader(juce::Graphics& g, float u) {
    suiteui::logo(g, 48 * u, 36 * u, 17 * u, u, kLilac, Colour(0xFF5A46C8));
    engraved(g, pal, "Orbe Beat", S(84, 18, 250, 34), Justification::centredLeft, 28.f * u, 6.f * u, true);
    g.setColour(pal.mute); g.setFont(fnt(10.f * u));
    g.drawText(u8("Boîte à rythmes · 8 pistes · 7 kits"), S(360, 26, 280, 20).toNearestInt(), Justification::centredLeft);
    const int kitI = (int)proc.apvts.getRawParameterValue("kit")->load(), pat = proc.currentPattern();
    const float ps = proc.playStep.load();
    screenWell(g, pal, S(700, 18, 376, 36));
    const bool run = ps >= 0.f;
    led(g, 718 * u, 36 * u, 3.6f * u, run, kLilac);
    g.setColour(kIce); g.setFont(mono(13.f * u));
    const String where = run ? String::charToString("ABCD"[proc.playPat.load() & 3]) + "  " + String((int)ps + 1).paddedLeft('0', 2) + "/" + String(proc.steps()) : String::charToString("ABCD"[pat & 3]) + "  " + String(proc.steps()) + " " + u8("pas");
    g.drawText(upper(String(obeat::kits()[kitI].label)) + "   " + String(juce::roundToInt(proc.tempo())) + " BPM", S(732, 18, 190, 36).toNearestInt(), Justification::centredLeft);
    g.setColour(pal.screenInk.withAlpha(0.85f));
    g.drawText("PATTERN " + where, S(900, 18, 168, 36).toNearestInt(), Justification::centredRight);
}

void OrbeBeatEditor::paint(juce::Graphics& g) {
    const float u = u_;
    g.drawImageAt(plate, 0, 0);
    suiteui::rule(g, u, 70, kW); suiteui::rule(g, u, 136, kW); suiteui::rule(g, u, 442, kW);
    for (auto p : {juce::Point<float>(12, 12), {kW - 12, 12}, {12, kH - 12}, {kW - 12, kH - 12}}) screw(g, p.x * u, p.y * u, 5.f * u, (p.x + p.y) * 0.9f);
    paintHeader(g, u);

    engraved(g, pal, "Kit", S(24, 82, 180, 12), Justification::centredLeft, 9.5f * u, 2.f * u);
    engraved(g, pal, "Pattern", S(220, 82, 186, 12), Justification::centredLeft, 9.5f * u, 2.f * u);
    engraved(g, pal, "Motif", S(424, 82, 280, 12), Justification::centredLeft, 9.5f * u, 2.f * u);
    engraved(g, pal, "Outil", S(836, 82, 240, 12), Justification::centredLeft, 9.5f * u, 2.f * u);

    // règle des pas
    const int st = proc.steps();
    const float cw = kGridW / (float)st;
    g.setFont(mono(9.f * u)); g.setColour(pal.mute.withAlpha(0.85f));
    for (int s = 0; s < st; s += 4) g.drawText(String(s / 4 + 1), S(kGridX + cw * (float)s + 2, 140, 30, 11).toNearestInt(), Justification::centredLeft);
    // fond de la grille
    screenWell(g, pal, S(kGridX - 2, kGridY - 2, kGridW + 4, kRow * kLanes + 4).reduced(1.f));
    // bandes de pistes
    for (int i = 0; i < kLanes; ++i) {
        const float y = kGridY + kRow * (float)i;
        const Colour c = Colour(obeat::lanes()[i].rgb);
        const bool aud = proc.audible(i);
        g.setColour(Colours::white.withAlpha(i % 2 ? 0.0f : 0.02f)); g.fillRect(S(24, y, 284, kRow));
        g.setColour(c.withAlpha(aud ? 0.95f : 0.35f)); g.fillRoundedRectangle(S(24, y + 6, 4, kRow - 12), 2.f * u);
        led(g, 42 * u, (y + kRow * 0.5f) * u, 3.8f * u, proc.flash[i].load() > 0.08f, c);
        g.setColour(c.withAlpha(aud ? 0.95f : 0.4f));
        tracked(g, u8(obeat::lanes()[i].label), S(54, y, 112, kRow), Justification::centredLeft, 1.1f, fnt(10.f * u, true));
    }
    // volumes / master
    engraved(g, pal, "Groove", S(32, 452, 212, 14), Justification::centredLeft, 10.f * u, 2.f * u);
    engraved(g, pal, "Swing", S(32, 570, 96, 12), Justification::centred, 9.f * u, 1.6f * u);
    engraved(g, pal, "Master", S(148, 570, 96, 12), Justification::centred, 9.f * u, 1.6f * u);
    g.setColour(pal.mute); g.setFont(mono(10.f * u));
    g.drawText(String(juce::roundToInt(proc.apvts.getRawParameterValue("swing")->load() * 200.f)) + " %", S(32, 584, 96, 14).toNearestInt(), Justification::centred);
    g.drawText(String(juce::roundToInt(proc.apvts.getRawParameterValue("master")->load() * 100.f)) + " %", S(148, 584, 96, 14).toNearestInt(), Justification::centred);
    suiteui::ledBar(g, S(32, 608, 212, 6), juce::jlimit(0.f, 1.f, (20.f * std::log10(juce::jmax(1e-4f, meterShown)) + 42.f) / 42.f), 18, kLilac);
    engraved(g, pal, "Pas", S(300, 452, 96, 14), Justification::centredLeft, 10.f * u, 2.f * u);
    engraved(g, pal, u8("Enchaînement"), S(410, 452, 210, 14), Justification::centredLeft, 10.f * u, 2.f * u);
    engraved(g, pal, u8("Entrée MIDI"), S(300, 512, 320, 14), Justification::centredLeft, 10.f * u, 2.f * u);
    engraved(g, pal, "Tonique 808", S(660, 452, 100, 14), Justification::centredLeft, 10.f * u, 2.f * u);
    engraved(g, pal, "Gamme 808", S(770, 452, 100, 14), Justification::centredLeft, 10.f * u, 2.f * u);
    engraved(g, pal, "Export MIDI", S(900, 452, 176, 14), Justification::centredLeft, 10.f * u, 2.f * u);
    g.setColour(pal.mute); g.setFont(fnt(10.f * u));
    g.drawFittedText(u8("Notes GM : 36 kick · 38 snare · 39 clap · 42 charley · 46 ouvert · 37 rim · 47 perc. Les autres notes jouent l'808."), S(300, 576, 570, 30).toNearestInt(), Justification::topLeft, 2);
    g.setColour(pal.line);
    g.fillRect(S(270, 456, 1.f, 150)); g.fillRect(S(640, 456, 1.f, 100)); g.fillRect(S(884, 456, 1.f, 150));
}
