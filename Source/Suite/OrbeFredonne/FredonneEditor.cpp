#include "FredonneEditor.h"

using namespace ou;
using kit::u8;
namespace v3 = kit::v3;

namespace {
String mmss(double s) { const int t = (int)s; return String(t / 60) + ":" + String(t % 60).paddedLeft('0', 2); }
void txt(juce::Graphics& g, const String& s, juce::Rectangle<float> r, float h, juce::Colour c, juce::Justification j = juce::Justification::centredLeft, bool bold = false, bool mono = false) {
    g.setColour(c); g.setFont(mono ? kit::mono(h) : kit::fnt(h, bold)); g.drawText(s, r, j, true);
}
}  // namespace

// ---------------- poignée MIDI ----------------
void MidiHandle::paint(juce::Graphics& g) {
    if (v3::bridge::v3(T)) {
        const float u = (float)getWidth() / 292.f; auto r = getLocalBounds().toFloat().reduced(1.f);
        const float a = enabledLook ? 1.f : .45f, rad = 14.f * u;
        g.setColour(T->field); g.fillRoundedRectangle(r, rad);
        juce::Path pth; pth.addRoundedRectangle(r.reduced(1.f), rad);
        const float dash[2] = {5.f * u, 4.f * u}; juce::Path d; juce::PathStrokeType(1.2f).createDashedStroke(d, pth, dash, 2);
        g.setColour((dragging ? T->accent : T->tick).withAlpha(a)); g.fillPath(d);
        auto ic = juce::Rectangle<float>(r.getX() + 16 * u, r.getCentreY() - 19 * u, 30 * u, 38 * u);
        juce::Path doc; doc.startNewSubPath(ic.getX(), ic.getY()); doc.lineTo(ic.getRight() - 9 * u, ic.getY()); doc.lineTo(ic.getRight(), ic.getY() + 9 * u); doc.lineTo(ic.getRight(), ic.getBottom()); doc.lineTo(ic.getX(), ic.getBottom()); doc.closeSubPath();
        g.setColour(T->card.withAlpha(a)); g.fillPath(doc); g.setColour(T->inkMute.withAlpha(a)); g.strokePath(doc, juce::PathStrokeType(1.f));
        g.setColour(T->accentText.withAlpha(a)); g.setFont(v3::font(10.5f * u, true)); g.drawText("MID", ic.withTop(ic.getCentreY() - 6 * u).withHeight(12 * u), juce::Justification::centred, false);
        g.setColour(T->ink.withAlpha(a)); v3::caps(g, "Glisser le MIDI", juce::Rectangle<float>(r.getX() + 58 * u, r.getCentreY() - 19 * u, r.getWidth() - 62 * u, 17 * u), 11.5f * u, juce::Justification::centredLeft);
        g.setColour(T->inkMute.withAlpha(a)); g.setFont(v3::font(12.f * u));
        g.drawFittedText(enabledLook ? u8("vers une piste de ton DAW") : u8("disponible après l'analyse"), juce::Rectangle<float>(r.getX() + 58 * u, r.getCentreY() + 1 * u, r.getWidth() - 62 * u, 20 * u).toNearestInt(), juce::Justification::topLeft, 1);
        return;
    }
    const float u = (float)getWidth() / 292.f; auto r = getLocalBounds().toFloat().reduced(1.f);
    g.setOpacity(1.f);
    g.setColour(juce::Colours::black.withAlpha(.5f)); g.fillRoundedRectangle(r, 6.f * u);
    g.setColour(dragging ? P.accent : juce::Colours::white.withAlpha(enabledLook ? .22f : .08f)); g.drawRoundedRectangle(r, 6.f * u, 1.2f);
    const float a = enabledLook ? 1.f : .4f;
    auto ic = juce::Rectangle<float>(r.getX() + 14 * u, r.getCentreY() - 20 * u, 32 * u, 40 * u);
    juce::Path doc; doc.startNewSubPath(ic.getX(), ic.getY()); doc.lineTo(ic.getRight() - 9 * u, ic.getY()); doc.lineTo(ic.getRight(), ic.getY() + 9 * u); doc.lineTo(ic.getRight(), ic.getBottom()); doc.lineTo(ic.getX(), ic.getBottom()); doc.closeSubPath();
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xFF4A4A52).withAlpha(a), ic.getX(), ic.getY(), juce::Colour(0xFF202024).withAlpha(a), ic.getX(), ic.getBottom(), false)); g.fillPath(doc);
    g.setColour(juce::Colours::white.withAlpha(.22f * a)); g.strokePath(doc, juce::PathStrokeType(1.f));
    g.setColour(P.accent.withAlpha(a)); g.setFont(kit::mono(10.f * u)); g.drawText(".MID", ic.withTop(ic.getCentreY() - 6 * u).withHeight(12 * u), juce::Justification::centred);
    g.setColour(P.ink.withAlpha(.9f * a)); kit::tracked(g, "Glisser le MIDI", juce::Rectangle<float>(r.getX() + 58 * u, r.getCentreY() - 17 * u, r.getWidth() - 62 * u, 16 * u), juce::Justification::centredLeft, 1.1f * u, kit::fnt(11.5f * u, true));
    g.setColour(P.mute.withAlpha(a)); g.setFont(kit::fnt(10.f * u));
    g.drawFittedText(enabledLook ? u8("vers une piste de ton DAW") : u8("disponible après l'analyse"), juce::Rectangle<float>(r.getX() + 58 * u, r.getCentreY() + 1 * u, r.getWidth() - 62 * u, 18 * u).toNearestInt(), juce::Justification::topLeft, 1);
}
void MidiHandle::mouseDrag(const juce::MouseEvent& e) {
    if (dragging || e.getDistanceFromDragStart() < 6 || !enabledLook) return;
    auto f = proc.writeMidiFile(); if (!f.existsAsFile()) return;
    dragging = true; repaint();
    if (auto* c = juce::DragAndDropContainer::findParentDragContainerFor(this))
        c->performExternalDragDropOfFiles({f.getFullPathName()}, false, this, [this] { dragging = false; repaint(); });
}

// ---------------- éditeur ----------------
FredonneEditor::FredonneEditor(FredonneProcessor& p)
    : AudioProcessorEditor(&p), pal(ou::palette(kLilac)), look(pal), disp(kit::Display::OLED), scr(kit::displayColours(kit::Display::OLED, pal).ink), proc(p), take(p.take()) {
    setLookAndFeel(&look);
    auto& ap = proc.apvts;
    auto knobOf = [&](juce::Slider& s) { s.setSliderStyle(juce::Slider::RotaryVerticalDrag); s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0); s.setMouseDragSensitivity(180); addAndMakeVisible(s); };
    knobOf(sensK); knobOf(minK); knobOf(bpmK);
    sensA = std::make_unique<SliderAtt>(ap, "sens", sensK); minA = std::make_unique<SliderAtt>(ap, "minlen", minK); bpmA = std::make_unique<SliderAtt>(ap, "bpm", bpmK);
    sensK.setTooltip(u8("Sensibilité : plus haut, plus de notes faibles sont gardées")); minK.setTooltip(u8("Les notes plus courtes sont ignorées"));
    bpmK.setTooltip(u8("Tempo pour la grille (ignoré si l'hôte est suivi)"));
    keyBox.addItem("Auto", 1); for (int i = 0; i < 12; ++i) keyBox.addItem(note(i), i + 2);
    modeBox.addItem("Auto", 1); modeBox.addItem("Mineur", 2); modeBox.addItem("Majeur", 3); modeBox.addItem("Chromatique", 4);
    for (auto* c : {&keyBox, &modeBox}) { addAndMakeVisible(c); c->setJustificationType(juce::Justification::centredLeft); }
    keyA = std::make_unique<ComboAtt>(ap, "key", keyBox); modeA = std::make_unique<ComboAtt>(ap, "mode", modeBox);
    gridSeg = std::make_unique<v3::GridSeg>(*ap.getParameter("grid"), juce::StringArray{"Libre", "1/4", "1/8", "1/16"}, pal);
    octSeg = std::make_unique<v3::GridSeg>(*ap.getParameter("oct"), juce::StringArray{u8("−2"), u8("−1"), "0", "+1"}, pal);
    addAndMakeVisible(*gridSeg); addAndMakeVisible(*octSeg);
    struct Tg { juce::ToggleButton& b; const char* text; const char* id; std::unique_ptr<ButtonAtt>& a; const char* tip; };
    for (auto& t : {Tg{snapT, "Calage sur gamme", "snap", snapA, "Ramène chaque note dans la gamme"}, Tg{tuneT, "Justesse auto", "tune", tuneA, "Corrige l'écart moyen de ta voix en cents"},
                    Tg{followT, "Tempo de l'hôte", "follow", followA, "Utilise le tempo du projet pour la grille"}, Tg{playT, "Jouer dans l'hôte", "play", playA, "Envoie aussi les notes en MIDI pendant la lecture"},
                    Tg{monT, "Écoute directe", "monitor", monA, "Laisse passer ta voix en sortie (casque conseillé)"}}) {
        t.b.setButtonText(u8(t.text)); t.b.setTooltip(u8(t.tip)); addAndMakeVisible(t.b); t.a = std::make_unique<ButtonAtt>(ap, t.id, t.b);
    }
    recB.setClickingTogglesState(false); recB.setLed(juce::Colour(0xFFFF3B30)); recB.setTooltip(u8("Arme l'enregistrement ; il démarre quand tu chantes. Reclique pour arrêter.")); recB.onClick = [this] { proc.pressRecord(); };
    clearB.setButtonText("Effacer"); clearB.onClick = [this] { proc.clearTake(); };
    clearB.setTooltip(u8("Efface la prise et les notes"));
    addAndMakeVisible(recB); addAndMakeVisible(clearB);
    handle = std::make_unique<MidiHandle>(proc, pal); addAndMakeVisible(*handle);
    help.install(*this, "Orbe Fredonne", {u8("Transforme une mélodie fredonnée en notes MIDI."),
             u8("- REC arme l'enregistreur : il démarre dès que tu chantes ; STOP l'arrête (2 minutes au plus). L'analyse découpe la voix en notes."),
             u8("- Glisse ensuite le fichier .mid vers une piste de ton DAW. Effacer supprime la prise."),
             u8("- Note en direct : note chantée, fréquence et écart en cents (vert : moins de 10 ct)."),
             u8("- Sensibilité garde plus ou moins de notes faibles ; Plus courte ignore les notes brèves ; Grille, Octave, Tonalité et Mode règlent le calage."),
             u8("- Écoute directe laisse passer ta voix (casque conseillé) ; Jouer dans l'hôte envoie aussi les notes en MIDI pendant la lecture.")});
    {   // v3 : potards haut de gamme sur les mêmes paramètres
        k3.add(new v3::Knob3(ap, "sens", u8("Sensibilité"), u8("Sensibilité : plus haut, plus de notes faibles sont gardées"), v3::KnobSize::S));
        k3.add(new v3::Knob3(ap, "minlen", "Plus courte", u8("Les notes plus courtes sont ignorées"), v3::KnobSize::S));
        k3.add(new v3::Knob3(ap, "bpm", "Tempo", u8("Tempo pour la grille (ignoré si l'hôte est suivi)"), v3::KnobSize::S));
        k3[0]->setFormatter([](double v) { return v3::fmt::pct(v); });
        k3[1]->setFormatter([](double v) { return v3::fmt::withUnit(v3::fmt::num(v, 0), "ms"); });
        k3[2]->setFormatter([this](double) { return v3::fmt::withUnit(v3::fmt::num(proc.effectiveBpm(), 0), "BPM"); });
        for (auto* k : k3) { k->setLabelPx(10.f); addChildComponent(k); }
    }
    addAndMakeVisible(skin.themeBtn);
    skin.onMode = [this] {
        const bool n3 = skin.on(); const auto* tp = skin.tp();
        for (auto* x : {&sensK, &minK, &bpmK}) x->setVisible(!n3);
        for (auto* x : k3) { x->setVisible(n3); x->setup(&skin.T, &skin.art); }
        gridSeg->T = tp; octSeg->T = tp; gridSeg->px = octSeg->px = 11.5f; recB.T = tp; clearB.T = tp; handle->T = tp; help.T = tp; repaint();
    };
    setResizable(true, true); setResizeLimits((int)(W * .8f), (int)(H * .8f), (int)(W * 1.7f), (int)(H * 1.7f));
    getConstrainer()->setFixedAspectRatio((double)W / (double)H); setSize((int)W, (int)H);
    skin.init(*this, look, W, H);
    startTimerHz(24);
}

FredonneEditor::~FredonneEditor() { stopTimer(); skin.shutdown(); }

void FredonneEditor::paint(juce::Graphics& g) {
    if (skin.on()) { paint3(g); return; }
    content = kit::w::drawFace(g, pal, getLocalBounds(), u_, {kit::w::Family::Meter, "Orbe Fredonne", kit::w::serial::fredonne, kHeader, kSide});
    body(g);
}

void FredonneEditor::resized() {
    u_ = (float)getWidth() / W; skin.scale = u_;
    if (skin.on()) { layout3(); return; }
    look.k = u_;
    content = getLocalBounds().toFloat().reduced(kSide * u_, 0);
    help.layout(kit::w::helpSlot(content, u_, false, kHeader).toNearestInt(), getLocalBounds());
    skin.themeBtn.setBounds(help.getBounds().translated(-help.getWidth() - juce::roundToInt(6.f * u_), 0));
    layout(); repaint();
}

void FredonneEditor::layout() {
    recB.setBounds(Ri(36, 112, 248, 48)); clearB.setBounds(Ri(824, 222, 140, 22));
    handle->setBounds(Ri(672, 112, 292, 64));
    monT.setBounds(Ri(176, 216, 112, 22));
    sensK.setBounds(Ri(40, 500, 84, 84)); minK.setBounds(Ri(150, 500, 84, 84)); bpmK.setBounds(Ri(260, 500, 84, 84));
    gridSeg->setBounds(Ri(380, 520, 190, 30)); octSeg->setBounds(Ri(588, 520, 170, 30));
    keyBox.setBounds(Ri(776, 520, 80, 30)); modeBox.setBounds(Ri(866, 520, 98, 30));
    snapT.setBounds(Ri(380, 572, 150, 24)); tuneT.setBounds(Ri(540, 572, 130, 24)); followT.setBounds(Ri(680, 572, 140, 24)); playT.setBounds(Ri(828, 572, 140, 24));
}

void FredonneEditor::timerCallback() {
    take = proc.take(); state = proc.state(); recSec = proc.recSeconds(); level = proc.level.load();
    const double now = juce::Time::getMillisecondCounterHiRes(), hz = proc.liveHz.load();
    if (hz > 40.0) {
        const double m = oa::ftom(hz); liveNote = (int)std::round(m); liveHz = hz; lastLive = now;
        const float c = (float)((m - std::round(m)) * 100.0); cents += (c - cents) * .45f;
    }
    if (now - lastLive > 350) liveNote = -1;
    const char* lbl[] = {"REC", "ARMÉ", "STOP", "ANALYSE", "ANALYSE"};
    recB.setButtonText(u8(lbl[juce::jlimit(0, 4, state)])); recB.lit = state == FredonneProcessor::Recording || (state == FredonneProcessor::Armed && (juce::Time::getMillisecondCounter() / 450) % 2 == 0); recB.repaint();
    recB.setEnabled(state <= FredonneProcessor::Recording);
    clearB.setEnabled(!take->seg.notes.empty() && state == FredonneProcessor::Idle);
    handle->enabledLook = !take->seg.notes.empty(); handle->repaint();
    const bool fol = proc.apvts.getRawParameterValue("follow")->load() > .5f; bpmK.setEnabled(!fol); bpmK.setAlpha(fol ? .4f : 1.f);
    k3[2]->setEnabled(!fol); k3[2]->setAlpha(fol ? .4f : 1.f);
    if (skin.on()) for (auto* x : k3) x->tickMod(1.f / 24.f);
    repaint();
}

void FredonneEditor::body(juce::Graphics& g) {
    const float u = u_; const auto& tk = *take; const bool has = !tk.seg.notes.empty();
    // ---- état dans l'en-tête
    well(g, R(560, 10, 360, 30));
    String hs = has ? String((int)tk.seg.notes.size()) + u8(" NOTES") + kDot + note(tk.seg.key) + (tk.seg.mode == 0 ? "m" : tk.seg.mode == 1 ? " maj" : " chr") + kDot + fr(tk.bpm, 0) + " BPM"
                      : state == FredonneProcessor::Recording ? u8("ENREGISTREMENT  ") + mmss(recSec) : state == FredonneProcessor::Armed ? u8("ARMÉ  ·  EN ATTENTE DU SON") : u8("PRÊT");
    txt(g, hs, R(572, 10, 340, 30), 13.f * u, scr, juce::Justification::centredLeft, false, true);

    // ---- enregistrement
    panel(g, pal, R(20, 82, 280, 168), "Enregistrement", u);
    {
        const bool rec = state == FredonneProcessor::Recording, arm = state == FredonneProcessor::Armed;
        well(g, R(36, 172, 120, 40));
        lcd(g, R(40, 174, 112, 36), mmss(recSec), 28.f * u, rec ? kBad : scr);
        const auto lv = R(170, 176, 114, 10); well(g, lv);
        const float fillw = juce::jlimit(0.f, 1.f, std::sqrt(level)) * (lv.getWidth() - 4 * u);
        g.setGradientFill(juce::ColourGradient(kOk, lv.getX(), 0, level > .9f ? kBad : kWarn, lv.getRight(), 0, false)); g.fillRect(lv.getX() + 2 * u, lv.getY() + 2 * u, fillw, lv.getHeight() - 4 * u);
        txt(g, "NIVEAU", R(170, 164, 60, 10), 7.5f * u, pal.mute, juce::Justification::centredLeft, true);
        const auto pb = R(170, 200, 114, 8); g.setColour(juce::Colours::black.withAlpha(.6f)); g.fillRoundedRectangle(pb, 4.f * u);
        g.setColour(kLilac.withAlpha(.9f)); g.fillRoundedRectangle(pb.withWidth(pb.getWidth() * (float)juce::jlimit(0.0, 1.0, recSec / 120.0)), 4.f * u);
        txt(g, "/ 2:00", R(228, 163, 56, 10), 7.5f * u, pal.mute, juce::Justification::centredRight, true);
    }

    // ---- note en direct
    panel(g, pal, R(316, 82, 324, 168), "Note en direct", u);
    {
        well(g, R(332, 112, 292, 74));
        const bool on = liveNote >= 0;
        lcd(g, R(340, 114, 150, 70), on ? noteOct(liveNote) : "--", 46.f * u, on ? (std::abs(cents) <= 10 ? kOk : scr) : scr.withAlpha(.4f), juce::Justification::centredLeft);
        txt(g, on ? fr(liveHz) + " Hz" : "-- Hz", R(488, 124, 128, 18), 13.f * u, pal.ink.withAlpha(.9f), juce::Justification::centredRight, false, true);
        txt(g, on ? (cents >= 0 ? "+" : "") + String(juce::roundToInt(cents)) + " ct" : "-- ct", R(488, 148, 128, 24), 19.f * u, on ? (std::abs(cents) <= 10 ? kOk : std::abs(cents) <= 25 ? kWarn : kBad) : pal.mute, juce::Justification::centredRight, false, true);
        const auto gr = R(332, 198, 292, 40); const float cx = gr.getCentreX(), sc = gr.getWidth() / 100.f;
        well(g, gr);
        g.setColour(kOk.withAlpha(.13f)); g.fillRect(cx - 10 * sc, gr.getY() + 3 * u, 20 * sc, gr.getHeight() - 6 * u);
        for (int c = -50; c <= 50; c += 10) { const bool maj = c % 50 == 0 || c == 0; g.setColour(pal.ink.withAlpha(maj ? .55f : .25f)); g.fillRect(cx + (float)c * sc - .5f, gr.getBottom() - (maj ? 16.f : 11.f) * u, 1.f, (maj ? 12.f : 7.f) * u); }
        txt(g, "-50", R(332, 198, 30, 10), 7.5f * u, pal.mute, juce::Justification::centredLeft, false, true); txt(g, "+50", R(594, 198, 30, 10), 7.5f * u, pal.mute, juce::Justification::centredRight, false, true);
        if (on) { const float x = cx + juce::jlimit(-50.f, 50.f, cents) * sc; g.setColour(juce::Colours::black.withAlpha(.5f)); g.fillRect(x - 1.5f * u, gr.getY() + 5 * u, 4.f * u, gr.getHeight() - 14 * u);
                  g.setColour(std::abs(cents) <= 10 ? kOk : scr); g.fillRect(x - 1.f * u, gr.getY() + 4 * u, 2.5f * u, gr.getHeight() - 14 * u); }
    }

    // ---- sortie MIDI
    panel(g, pal, R(656, 82, 324, 168), "Sortie MIDI", u);
    {
        const String k = has ? note(tk.seg.key) + " " + (tk.seg.mode == 0 ? u8("mineur") : tk.seg.mode == 1 ? u8("majeur") : u8("chromatique")) : String("--");
        const bool autoK = proc.apvts.getRawParameterValue("key")->load() < .5f;
        txt(g, has ? String((int)tk.seg.notes.size()) + u8(" notes  ·  ") + k + (autoK ? " (auto)" : "") : u8("Aucune note"), R(672, 184, 292, 16), 11.f * u, pal.ink.withAlpha(.9f));
        txt(g, has ? u8("Justesse : ") + (tk.seg.cents >= 0 ? "+" : "") + String(juce::roundToInt(tk.seg.cents * 100)) + u8(" ct  ·  grille sur ") + fr(tk.bpm, 0) + " BPM" : String(), R(672, 202, 292, 14), 9.5f * u, pal.mute);
    }

    // ---- mélodie
    panel(g, pal, R(20, 262, 960, 202), u8("Mélodie"), u);
    paintRoll(g, R(36, 290, 928, 162), u);

    // ---- réglages
    panel(g, pal, R(20, 476, 960, 144), u8("Réglages"), u);
    kit::engraved(g, pal, u8("Sensibilité"), R(40, 588, 84, 12), juce::Justification::centred, 8.5f * u, 1.5f * u);
    kit::engraved(g, pal, "Plus courte", R(150, 588, 84, 12), juce::Justification::centred, 8.5f * u, 1.5f * u);
    kit::engraved(g, pal, "Tempo", R(260, 588, 84, 12), juce::Justification::centred, 8.5f * u, 1.5f * u);
    auto& ap = proc.apvts;
    txt(g, String(juce::roundToInt(ap.getRawParameterValue("sens")->load() * 100)) + " %", R(40, 602, 84, 12), 9.f * u, pal.mute, juce::Justification::centred, false, true);
    txt(g, String(juce::roundToInt(ap.getRawParameterValue("minlen")->load())) + " ms", R(150, 602, 84, 12), 9.f * u, pal.mute, juce::Justification::centred, false, true);
    txt(g, fr(proc.effectiveBpm()) + " BPM", R(260, 602, 84, 12), 9.f * u, pal.mute, juce::Justification::centred, false, true);
    kit::engraved(g, pal, "Grille", R(380, 504, 190, 12), juce::Justification::centredLeft, 8.5f * u, 1.8f * u);
    kit::engraved(g, pal, "Octave", R(588, 504, 170, 12), juce::Justification::centredLeft, 8.5f * u, 1.8f * u);
    kit::engraved(g, pal, u8("Tonalité"), R(776, 504, 80, 12), juce::Justification::centredLeft, 8.5f * u, 1.8f * u);
    kit::engraved(g, pal, "Mode", R(866, 504, 98, 12), juce::Justification::centredLeft, 8.5f * u, 1.8f * u);
    g.setColour(pal.line); g.fillRect(R(364, 498, 1.f, 108));
}

void FredonneEditor::paintRoll(juce::Graphics& g, juce::Rectangle<float> r, float u) {
    well(g, r);
    const auto& tk = *take; const auto& ns = tk.seg.notes;
    if (ns.empty()) {
        const char* m = state == FredonneProcessor::Recording ? "ENREGISTREMENT" : state >= FredonneProcessor::Captured ? "ANALYSE" : "AUCUNE NOTE";
        txt(g, u8(m), r, 12.f * u, scr.withAlpha(.6f), juce::Justification::centred, false, true); return;
    }
    int lo = 127, hi = 0; double D = tk.track ? tk.track->dur : 1.0;
    for (auto& n : ns) { lo = juce::jmin(lo, n.n); hi = juce::jmax(hi, n.n); D = juce::jmax(D, n.e); }
    lo -= 2; hi += 2; while (hi - lo < 13) { --lo; ++hi; lo = juce::jmax(0, lo); }
    const float gut = 40 * u, x0 = r.getX() + gut, w = r.getWidth() - gut - 6 * u, rh = (r.getHeight() - 8 * u) / (float)(hi - lo + 1), y0 = r.getY() + 4 * u;
    auto yOf = [&](double midi) { return y0 + (float)(hi + 1 - midi) * rh; };
    for (int m = lo; m <= hi; ++m) {   // clavier et rangées
        const int pc = m % 12; const bool blk = pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
        g.setColour(blk ? juce::Colours::white.withAlpha(.04f) : juce::Colours::white.withAlpha(.012f)); g.fillRect(x0, yOf(m + 1) + 0.f, w, rh);
        g.setColour(blk ? juce::Colour(0xFF1A1A1E) : juce::Colour(0xFFD8D6E0).withAlpha(.85f)); g.fillRect(r.getX() + 4 * u, yOf(m + 1) + .5f, blk ? gut * .6f : gut - 6 * u, rh - 1.f);
        if (pc == 0 || rh > 11 * u) txt(g, noteOct(m), juce::Rectangle<float>(r.getX() + 4 * u, yOf(m + 1), gut - 8 * u, rh), juce::jmin(9.f * u, rh * .8f), blk ? pal.mute : juce::Colour(0xFF2A2A30), juce::Justification::centredRight, false, true);
    }
    const double beat = 60.0 / tk.bpm;   // repères de temps
    for (double t = 0; t < D; t += beat) { const bool bar = std::fmod(std::round(t / beat), 4.0) == 0; g.setColour(juce::Colours::white.withAlpha(bar ? .12f : .05f)); g.fillRect(x0 + (float)(t / D) * w, y0, 1.f, rh * (float)(hi - lo + 1)); }
    juce::Graphics::ScopedSaveState clip(g); g.reduceClipRegion(r.toNearestInt());
    if (tk.track) {   // contour de la voix
        g.setColour(scr.withAlpha(.32f));
        for (auto& f : tk.track->frames) if (f.f > 0 && f.rms > tk.track->maxR * .05f) g.fillRect(x0 + (float)(f.t / D) * w, yOf(oa::ftom(f.f) + .5), 1.6f * u, 1.6f * u);
    }
    for (auto& n : ns) {
        auto b = juce::Rectangle<float>(x0 + (float)(n.s / D) * w, yOf(n.n + 1) + 1.f, juce::jmax(3.f * u, (float)((n.e - n.s) / D) * w), rh - 2.f);
        g.setColour(juce::Colours::black.withAlpha(.45f)); g.fillRoundedRectangle(b.translated(0, 1.5f), 2.5f * u);
        g.setGradientFill(juce::ColourGradient(kLilac, b.getX(), b.getY(), kLilac.withMultipliedBrightness(.68f), b.getX(), b.getBottom(), false)); g.fillRoundedRectangle(b, 2.5f * u);
        g.setColour(juce::Colours::white.withAlpha(.35f)); g.drawRoundedRectangle(b, 2.5f * u, 1.f);
        if (b.getWidth() > 24 * u) txt(g, noteOct(n.n), b.reduced(3 * u, 0), juce::jmin(9.5f * u, rh * .8f), Colour(0xFF14131C), juce::Justification::centredLeft, true, true);
    }
}

// ============================================================ v3 « hybride v2 » (mêmes coordonnées de conception 1000 x 640)
void FredonneEditor::layout3() {
    help.layout(Ri(954, 24, 28, 28), getLocalBounds());
    skin.themeBtn.setBounds(Ri(918, 24, 28, 28));
    recB.setBounds(Ri(36, 116, 248, 46)); clearB.setBounds(Ri(824, 222, 140, 24));
    handle->setBounds(Ri(672, 116, 292, 66));
    monT.setBounds(Ri(176, 218, 120, 22));
    for (int i = 0; i < 3; ++i) { k3[i]->setScale(u_); k3[i]->setBounds(Ri(36.f + 110.f * (float)i, 498, 104, 108)); }
    gridSeg->setBounds(Ri(380, 524, 190, 32)); octSeg->setBounds(Ri(588, 524, 170, 32));
    keyBox.setBounds(Ri(776, 524, 80, 32)); modeBox.setBounds(Ri(866, 524, 98, 32));
    snapT.setBounds(Ri(380, 576, 150, 24)); tuneT.setBounds(Ri(540, 576, 130, 24)); followT.setBounds(Ri(680, 576, 140, 24)); playT.setBounds(Ri(828, 576, 140, 24));
    repaint();
}

void FredonneEditor::paintBackdrop3(juce::Graphics& g, juce::Rectangle<float> b) {
    const float u = b.getWidth() / W;
    auto Rr = [u](float x, float y, float w, float h) { return juce::Rectangle<float>(x * u, y * u, w * u, h * u); };
    const auto& T = skin.T;
    v3::background(g, T, b);
    v3::brand(g, T, u, "Orbe", "Fredonne");
    v3::hero(g, T, Rr(420, 16, 480, 40), 14.f * u);
    v3::card(g, T, Rr(20, 82, 280, 168), 18.f * u);
    v3::hero(g, T, Rr(316, 82, 324, 168), 18.f * u);
    v3::card(g, T, Rr(656, 82, 324, 168), 18.f * u);
    v3::card(g, T, Rr(20, 262, 960, 202), 18.f * u);
    v3::card(g, T, Rr(20, 476, 960, 144), 18.f * u);
    v3::hero(g, T, Rr(36, 290, 928, 162), 14.f * u);
    v3::label(g, T, "Enregistrement", Rr(36, 94, 240, 14), 11.f * u);
    v3::label(g, T, u8("Sortie MIDI"), Rr(672, 94, 240, 14), 11.f * u);
    v3::label(g, T, u8("Mélodie"), Rr(36, 272, 240, 14), 11.f * u);
    v3::label(g, T, u8("Réglages"), Rr(36, 486, 240, 14), 11.f * u);
    v3::label(g, T, "Grille", Rr(382, 506, 190, 14), 10.5f * u);
    v3::label(g, T, "Octave", Rr(590, 506, 170, 14), 10.5f * u);
    v3::label(g, T, u8("Tonalité"), Rr(778, 506, 80, 14), 10.5f * u);
    v3::label(g, T, "Mode", Rr(868, 506, 98, 14), 10.5f * u);
    g.setColour(T.dark ? juce::Colours::white.withAlpha(0.07f) : juce::Colour(0x14101828)); g.fillRect(Rr(364, 500, 1, 106));
    g.setColour(T.scrMute); v3::caps(g, "Note en direct", Rr(334, 94, 200, 14), 11.f * u, juce::Justification::centredLeft);
}

void FredonneEditor::paint3(juce::Graphics& g) {
    const float u = u_; const auto& T = skin.T; const auto& tk = *take; const bool has = !tk.seg.notes.empty();
    skin.art.draw(g, String("bd|") + v3::modeKey(skin.mode), getLocalBounds().toFloat(), [this](juce::Graphics& ig, juce::Rectangle<float> b) { paintBackdrop3(ig, b); });
    // état
    String hs = has ? String((int)tk.seg.notes.size()) + u8(" notes") + ou::kDot + ou::note(tk.seg.key) + (tk.seg.mode == 0 ? "m" : tk.seg.mode == 1 ? " maj" : " chr") + ou::kDot + v3::fmt::withUnit(v3::fmt::num(tk.bpm, 0), "BPM")
                      : state == FredonneProcessor::Recording ? u8("Enregistrement  ") + mmss(recSec) : state == FredonneProcessor::Armed ? u8("Armé · en attente du son") : u8("Prêt");
    g.setColour(T.scrInk); g.setFont(v3::font(15.f * u, true)); g.drawText(hs, R(440, 16, 450, 40), juce::Justification::centredLeft, true);
    // enregistrement
    {
        const bool rec = state == FredonneProcessor::Recording;
        g.setColour(rec ? ou::kBad : T.ink); v3::tabular(g, mmss(recSec), R(36, 170, 130, 44), v3::font(36.f * u, true), juce::Justification::centredLeft);
        g.setColour(T.inkMute); v3::caps(g, "Niveau", R(176, 170, 60, 12), 9.5f * u, juce::Justification::centredLeft);
        v3::meterBar(g, T, R(176, 186, 108, 7), std::sqrt(level), level > .9f ? ou::kBad : T.accent);
        v3::meterBar(g, T, R(176, 206, 108, 5), (float)juce::jlimit(0.0, 1.0, recSec / 120.0), T.mod);
        g.setColour(T.inkMute); g.setFont(v3::font(11.f * u)); g.drawText("/ 2:00", R(236, 214, 48, 14), juce::Justification::centredRight, false);
    }
    // note en direct : écran héros
    {
        const bool on = liveNote >= 0;
        const auto big = v3::font(54.f * u, true);
        const Colour inTune = on && std::abs(cents) <= 10 ? T.sung : T.out;
        g.setColour(on ? inTune : T.scrMute); g.setFont(big);
        g.drawText(on ? ou::noteOct(liveNote) : String(v3::fmt::minus() + v3::fmt::minus()), R(332, 120, 130, 62), juce::Justification::centredLeft, false);
        g.setColour(T.scrInk); v3::tabular(g, on ? v3::fmt::cents(cents) : String(v3::fmt::minus() + v3::fmt::minus() + " ct"), R(460, 120, 170, 62), v3::font(30.f * u, true), juce::Justification::centredRight);
        g.setColour(T.scrMute); v3::tabular(g, on ? v3::fmt::hz(liveHz) : String("-- Hz"), R(332, 182, 120, 18), v3::font(13.f * u), juce::Justification::centredLeft);
        v3::gauge(g, T, R(420, 164, 210, 80), on ? cents : 0.f, -50.f, 50.f, v3::fmt::minus() + "50", "0", "+50", 10.f * u);
    }
    // sortie MIDI
    {
        const String k = has ? ou::note(tk.seg.key) + " " + (tk.seg.mode == 0 ? u8("mineur") : tk.seg.mode == 1 ? u8("majeur") : u8("chromatique")) : String("--");
        const bool autoK = proc.apvts.getRawParameterValue("key")->load() < .5f;
        g.setColour(T.ink); g.setFont(v3::font(14.f * u, true));
        g.drawText(has ? String((int)tk.seg.notes.size()) + u8(" notes  ·  ") + k + (autoK ? " (auto)" : "") : u8("Aucune note"), R(672, 194, 292, 18), juce::Justification::centredLeft, true);
        g.setColour(T.inkMute); g.setFont(v3::font(12.f * u));
        g.drawText(has ? u8("Justesse ") + v3::fmt::cents(tk.seg.cents * 100) + u8("  ·  grille sur ") + v3::fmt::withUnit(v3::fmt::num(tk.bpm, 0), "BPM") : String(), R(672, 216, 292, 16), juce::Justification::centredLeft, true);
    }
    paintRoll3(g, R(36, 290, 928, 162), u);
}

void FredonneEditor::paintRoll3(juce::Graphics& g, juce::Rectangle<float> r, float u) {
    const auto& T = skin.T; const auto& tk = *take; const auto& ns = tk.seg.notes;
    if (ns.empty()) {
        g.setColour(T.scrMute);
        v3::caps(g, state == FredonneProcessor::Recording ? "Enregistrement" : state >= FredonneProcessor::Captured ? "Analyse" : "Aucune note", r, 13.f * u, juce::Justification::centred);
        return;
    }
    int lo = 127, hi = 0; double D = tk.track ? tk.track->dur : 1.0;
    for (auto& n : ns) { lo = juce::jmin(lo, n.n); hi = juce::jmax(hi, n.n); D = juce::jmax(D, n.e); }
    lo -= 2; hi += 2; while (hi - lo < 13) { --lo; ++hi; lo = juce::jmax(0, lo); }
    const float gut = 44 * u, x0 = r.getX() + gut, w = r.getWidth() - gut - 10 * u, rh = (r.getHeight() - 12 * u) / (float)(hi - lo + 1), y0 = r.getY() + 6 * u;
    auto yOf = [&](double midi) { return y0 + (float)(hi + 1 - midi) * rh; };
    for (int m = lo; m <= hi; ++m) {
        const int pc = m % 12; const bool blk = pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
        g.setColour(juce::Colours::white.withAlpha(blk ? .05f : .015f)); g.fillRect(x0, yOf(m + 1), w, rh);
        if (pc == 0 || rh > 12 * u) { g.setColour(pc == 0 ? T.scrInk : T.scrMute); g.setFont(v3::font(juce::jmin(10.5f * u, rh * .85f))); g.drawText(ou::noteOct(m), juce::Rectangle<float>(r.getX() + 6 * u, yOf(m + 1), gut - 10 * u, rh), juce::Justification::centredRight, false); }
    }
    const double beat = 60.0 / tk.bpm;
    for (double t = 0; t < D; t += beat) { const bool bar = std::fmod(std::round(t / beat), 4.0) == 0; g.setColour(juce::Colours::white.withAlpha(bar ? .12f : .05f)); g.fillRect(x0 + (float)(t / D) * w, y0, 1.f, rh * (float)(hi - lo + 1)); }
    juce::Graphics::ScopedSaveState clip(g); g.reduceClipRegion(r.reduced(2.f).toNearestInt());
    if (tk.track) {   // contour de la voix : courbe lissée (hauteur moyenne par tranche de temps)
        constexpr int N = 400; static thread_local std::vector<float> v; v.assign(N, 0.f);
        std::vector<int> c(N, 0);
        for (auto& f : tk.track->frames) if (f.f > 0 && f.rms > tk.track->maxR * .05f) { const int b = juce::jlimit(0, N - 1, (int)(f.t / D * N)); v[(size_t)b] += (float)oa::ftom(f.f); ++c[(size_t)b]; }
        for (int i = 0; i < N; ++i) v[(size_t)i] = c[(size_t)i] ? v[(size_t)i] / (float)c[(size_t)i] : 0.f;
        v3::Trace tr;
        tr.build(v.data(), N, [&](float i) { return x0 + (i + 0.5f) / (float)N * w; }, [&](float m) { return yOf(m + 0.5f); }, r.getBottom(), {N, 1.f, 3.f, 0.f, 8});
        tr.draw(g, T.sung.withAlpha(0.8f), 1.4f * u, 0.8f);
    }
    for (auto& n : ns) {
        auto b = juce::Rectangle<float>(x0 + (float)(n.s / D) * w, yOf(n.n + 1) + 1.f, juce::jmax(4.f * u, (float)((n.e - n.s) / D) * w), rh - 2.f);
        g.setColour(T.accent.withAlpha(0.28f)); g.fillRoundedRectangle(b.expanded(2.f * u), 6.f * u);
        g.setColour(T.accent.brighter(0.2f)); g.fillRoundedRectangle(b, juce::jmin(5.f * u, b.getHeight() * 0.5f));
        if (b.getWidth() > 26 * u) { g.setColour(juce::Colour(0xFF14131C)); g.setFont(v3::font(juce::jmin(10.5f * u, rh * .8f), true)); g.drawText(ou::noteOct(n.n), b.reduced(4 * u, 0), juce::Justification::centredLeft, false); }
    }
}
