#include "FredonneEditor.h"

using namespace ou;
using kit::u8;

namespace {
String mmss(double s) { const int t = (int)s; return String(t / 60) + ":" + String(t % 60).paddedLeft('0', 2); }
void txt(juce::Graphics& g, const String& s, juce::Rectangle<float> r, float h, juce::Colour c, juce::Justification j = juce::Justification::centredLeft, bool bold = false, bool mono = false) {
    g.setColour(c); g.setFont(mono ? kit::mono(h) : kit::fnt(h, bold)); g.drawText(s, r, j, true);
}
}  // namespace

// ---------------- poignée MIDI ----------------
void MidiHandle::paint(juce::Graphics& g) {
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
    : Base(p, 1000.f, 640.f, "Orbe Fredonne", kit::w::serial::fredonne, kit::Display::OLED, kLilac), proc(p), take(p.take()) {
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
    gridSeg = std::make_unique<Seg>(*ap.getParameter("grid"), juce::StringArray{"Libre", "1/4", "1/8", "1/16"}, pal);
    octSeg = std::make_unique<Seg>(*ap.getParameter("oct"), juce::StringArray{u8("−2"), u8("−1"), "0", "+1"}, pal);
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
    setHelp({u8("Transforme une mélodie fredonnée en notes MIDI."),
             u8("- REC arme l'enregistreur : il démarre dès que tu chantes ; STOP l'arrête (2 minutes au plus). L'analyse découpe la voix en notes."),
             u8("- Glisse ensuite le fichier .mid vers une piste de ton DAW. Effacer supprime la prise."),
             u8("- Note en direct : note chantée, fréquence et écart en cents (vert : moins de 10 ct)."),
             u8("- Sensibilité garde plus ou moins de notes faibles ; Plus courte ignore les notes brèves ; Grille, Octave, Tonalité et Mode règlent le calage."),
             u8("- Écoute directe laisse passer ta voix (casque conseillé) ; Jouer dans l'hôte envoie aussi les notes en MIDI pendant la lecture.")});
    start(24);
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
