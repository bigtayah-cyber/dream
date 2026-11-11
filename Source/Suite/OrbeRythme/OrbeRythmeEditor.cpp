#include "OrbeRythmeEditor.h"

using namespace kit;
using juce::Colour, juce::Rectangle, juce::String;
namespace v3 = kit::v3;

namespace {
const char* kRowName[7] = {"Crash", "Ch. ouvert", "Charley", "Clap", "Caisse cl.", "Rim", "Grosse c."};
const Colour kRowCol[7] = {Colour(0xFFB9A9FF), Colour(0xFF8FD9FF), Colour(0xFF6FD6FF), Colour(0xFFFFC857), Colour(0xFFFF8A5C), Colour(0xFFFF9FC8), Colour(0xFF7FE0B5)};
constexpr float kGridW = 654.f, kLabW = 86.f, kHeadH = 18.f;
}  // namespace

// ---------------- grille ----------------
Rectangle<float> RythmeGrid::cells() const {
    const float u = (float)getWidth() / kGridW;
    return getLocalBounds().toFloat().withTrimmedLeft(kLabW * u).withTrimmedTop(kHeadH * u);
}

bool RythmeGrid::locate(juce::Point<float> p, int& row, int& step) const {
    const auto c = cells();
    if (!c.contains(p)) return false;
    const int steps = juce::jmax(1, proc.bars()) * 16;
    row = juce::jlimit(0, 6, (int)((p.y - c.getY()) / (c.getHeight() / 7.f)));
    step = juce::jlimit(0, steps - 1, (int)((p.x - c.getX()) / (c.getWidth() / (float)steps)));
    return true;
}

void RythmeGrid::apply(int row, int step) {
    if (row == lastRow && step == lastStep) return;
    lastRow = row; lastStep = step;
    const bool has = proc.findHit(row, (float)step) >= 0;
    if ((mode == 1 && !has) || (mode == 2 && has)) { proc.toggleHit(row, step); repaint(); }
}

void RythmeGrid::mouseDown(const juce::MouseEvent& e) {
    grabKeyboardFocus();
    int r, s;
    if (!locate(e.position, r, s)) return;
    proc.beginEdit();   // un seul niveau d'annulation par geste
    if (e.mods.isShiftDown()) { proc.cycleVelocity(r, s); repaint(); return; }
    mode = (e.mods.isPopupMenu() || proc.findHit(r, (float)s) >= 0) ? 2 : 1;
    lastRow = lastStep = -1;
    apply(r, s);
}

void RythmeGrid::mouseDrag(const juce::MouseEvent& e) {
    int r, s;
    if (mode != 0 && locate(e.position, r, s)) apply(r, s);
}

void RythmeGrid::paint(juce::Graphics& g) {
    if (T == nullptr) return;
    const float u = (float)getWidth() / kGridW;
    const auto c = cells();
    const int bars = juce::jmax(1, proc.bars()), steps = bars * 16;
    const float sw = c.getWidth() / (float)steps, rh = c.getHeight() / 7.f;
    for (int r = 0; r < 7; ++r) {
        auto row = Rectangle<float>(c.getX(), c.getY() + rh * (float)r, c.getWidth(), rh).reduced(0.f, 1.f * u);
        g.setColour(juce::Colours::white.withAlpha(r % 2 ? 0.035f : 0.06f)); g.fillRoundedRectangle(row, 3.f * u);
        g.setColour(T->scrMute); v3::caps(g, kRowName[r], Rectangle<float>(14.f * u, c.getY() + rh * (float)r, (kLabW - 18.f) * u, rh), 10.f * u, juce::Justification::centredLeft, false);
    }
    for (int s = 0; s <= steps; s += 4) {   // temps ; mesures plus marquées, numérotées
        const bool bar = s % 16 == 0;
        g.setColour(juce::Colours::white.withAlpha(bar ? 0.20f : 0.07f));
        g.fillRect(Rectangle<float>(c.getX() + sw * (float)s - 0.5f, c.getY(), 1.f, c.getHeight()));
        if (bar && s < steps) { g.setColour(T->scrMute); g.setFont(v3::font(10.f * u)); g.drawText(String(s / 16 + 1), Rectangle<float>(c.getX() + sw * (float)s + 3.f * u, 0.f, 24.f * u, kHeadH * u), juce::Justification::centredLeft, false); }
    }
    for (auto& n : proc.notes()) {
        const int r = oryt::rowOfNote(n.n);
        auto b = Rectangle<float>(c.getX() + n.t * sw, c.getY() + rh * (float)r + rh * 0.2f, juce::jmax(2.5f * u, sw * 0.8f), rh * 0.6f);
        g.setColour(kRowCol[r].withAlpha(0.35f + 0.65f * juce::jlimit(0.f, 1.f, n.v))); g.fillRoundedRectangle(b, juce::jmin(2.5f * u, b.getWidth() * 0.4f));
    }
    const float ps = proc.playStep.load();
    if (ps >= 0.f) {   // tête de lecture
        const float x = c.getX() + ps * sw;
        g.setGradientFill(juce::ColourGradient(T->out.withAlpha(0.f), x - 12.f * u, 0.f, T->out.withAlpha(0.22f), x, 0.f, false));
        g.fillRect(Rectangle<float>(x - 12.f * u, c.getY(), 12.f * u, c.getHeight()));
        g.setColour(T->out); g.fillRect(Rectangle<float>(x - 0.75f, c.getY(), 1.5f, c.getHeight()));
    }
}

// ---------------- poignée MIDI ----------------
void RythmeHandle::paint(juce::Graphics& g) {
    if (T == nullptr) return;
    auto r = getLocalBounds().toFloat().reduced(1.f);
    const float u = (float)getWidth() / 236.f, rad = 12.f * u;
    g.setColour(T->field); g.fillRoundedRectangle(r, rad);
    juce::Path pth; pth.addRoundedRectangle(r.reduced(1.f), rad);
    const float dash[2] = {5.f * u, 4.f * u}; juce::Path d; juce::PathStrokeType(1.2f).createDashedStroke(d, pth, dash, 2);
    g.setColour(dragging ? T->accent : T->tick); g.fillPath(d);
    auto ic = Rectangle<float>(r.getX() + 16 * u, r.getCentreY() - 18 * u, 28 * u, 36 * u);
    juce::Path doc; doc.startNewSubPath(ic.getX(), ic.getY()); doc.lineTo(ic.getRight() - 9 * u, ic.getY()); doc.lineTo(ic.getRight(), ic.getY() + 9 * u); doc.lineTo(ic.getRight(), ic.getBottom()); doc.lineTo(ic.getX(), ic.getBottom()); doc.closeSubPath();
    g.setColour(T->card); g.fillPath(doc); g.setColour(T->inkMute); g.strokePath(doc, juce::PathStrokeType(1.f));
    g.setColour(T->accentText); g.setFont(v3::font(10.f * u, true)); g.drawText("MID", ic.withTop(ic.getCentreY() - 6 * u).withHeight(12 * u), juce::Justification::centred, false);
    g.setColour(T->ink); v3::caps(g, "Glisser le MIDI", Rectangle<float>(r.getX() + 54 * u, r.getCentreY() - 19 * u, r.getWidth() - 58 * u, 16 * u), 11.f * u, juce::Justification::centredLeft);
    g.setColour(T->inkMute); g.setFont(v3::font(11.5f * u)); g.drawFittedText(u8("la batterie vers une piste de ton DAW"), Rectangle<float>(r.getX() + 54 * u, r.getCentreY() + 1 * u, r.getWidth() - 62 * u, 28 * u).toNearestInt(), juce::Justification::topLeft, 2);
}

void RythmeHandle::mouseDrag(const juce::MouseEvent& e) {
    if (dragging || e.getDistanceFromDragStart() < 6) return;
    dragging = true; repaint();
    auto f = proc.writeMidiFile();
    juce::Component::SafePointer<RythmeHandle> self(this);
    if (auto* c = juce::DragAndDropContainer::findParentDragContainerFor(this))
        c->performExternalDragDropOfFiles({f.getFullPathName()}, false, this, [self] { if (self != nullptr) { self->dragging = false; self->repaint(); } });
}

// ---------------- éditeur ----------------
OrbeRythmeEditor::OrbeRythmeEditor(OrbeRythmeProcessor& p)
    : AudioProcessorEditor(&p), proc(p), pal(kit::w::rack(v3::accents::peach)), grid(p), handle(p) {
    auto& ap = proc.apvts;
    String lastGroup; int id = 1;
    for (auto& s : orbe::styles()) {
        if (lastGroup != s.group) { styleBox.addSectionHeading(s.group); lastGroup = s.group; }
        styleBox.addItem(s.label, id++);
    }
    styleBox.setJustificationType(juce::Justification::centredLeft);
    styleBox.setTooltip(u8("Style : choisit le motif de batterie, le tempo conseillé et le swing d'origine."));
    styleBox.setTitle("Style");
    addAndMakeVisible(styleBox);
    styleAtt = std::make_unique<ComboAtt>(ap, "style", styleBox);

    auto knob = [&](const char* pid, const String& lab, const String& tip, bool peach) {
        auto* k = k3.add(new v3::Knob3(ap, pid, lab, tip, v3::KnobSize::S));
        k->setLabelPx(10.f); k->setFormatter([](double v) { return v3::fmt::pct(v); }); k->setPeach(peach);
        addAndMakeVisible(k);
    };
    knob("human", "Humain", u8("Humanisation : décale légèrement les frappes dans le temps et en vélocité."), false);
    knob("swing", "Swing", u8("Ajoute du swing au style (0 % : celui du style) : retarde un pas sur deux."), false);
    knob("level", "Niveau", u8("Niveau de la batterie (aperçu et vélocités envoyées)."), false);
    knob("master", "Master", u8("Volume de l'aperçu sonore. N'agit pas sur le MIDI."), true);

    barsSeg = std::make_unique<v3::bridge::Seg>(ap, "bars", juce::StringArray{"4", "8"}, u8("Longueur de la boucle en mesures. Change le tirage : les frappes modifiées à la main sont remplacées."), pal);
    muteSeg = std::make_unique<v3::bridge::Seg>(ap, "mute", juce::StringArray{"Son", "Muet"}, u8("Coupe l'aperçu et le MIDI envoyé."), pal);
    midiSeg = std::make_unique<v3::bridge::Seg>(ap, "midiout", juce::StringArray{"Off", "On"}, u8("Envoie aussi les frappes en MIDI (canal 10, notes GM) vers un instrument de ton DAW. Voir l'aide (?)."), pal);
    for (auto* s : {barsSeg.get(), muteSeg.get(), midiSeg.get()}) addAndMakeVisible(*s);

    for (auto* b : {&auditionB, &genB, &undoB, &redoB}) addAndMakeVisible(b);
    auditionB.setClickingTogglesState(true);
    auditionB.setTooltip(u8("Joue la boucle ici. Dans le DAW, la lecture suit directement le transport."));
    auditionB.onClick = [this] { proc.audition = auditionB.getToggleState(); auditionB.setButtonText(auditionB.getToggleState() ? "Stop" : u8("Écouter")); };
    genB.setTooltip(u8("Nouveau tirage de batterie dans le style choisi (remplace les frappes modifiées à la main)."));
    genB.onClick = [this] { proc.generate(); grid.repaint(); };
    undoB.onClick = [this] { proc.undo(); repaint(); };
    redoB.onClick = [this] { proc.redo(); repaint(); };
    undoB.setTooltip(u8("Annule la dernière modification ou le dernier tirage (Ctrl/Cmd + Z)."));
    redoB.setTooltip(u8("Rétablit (Ctrl/Cmd + Maj + Z)."));

    help.install(*this, "Orbe Rythme", {
        u8("Générateur de batterie et de rythmes : il écrit des frappes, en MIDI et en aperçu sonore."),
        u8("- Style : motif, tempo conseillé et swing d'origine. Générer : nouveau tirage dans le même style."),
        u8("- Grille : clic pour poser ou retirer une frappe, glisser pour peindre, Maj + clic pour fort / moyen / doux. Annuler : Ctrl ou Cmd + Z."),
        u8("- Humain et Swing agissent à la lecture. Le fichier MIDI glissé est celui des frappes, sans humanisation."),
        u8("- Dans FL Studio : Sortie MIDI sur On, puis règle le port de sortie MIDI du plugin et envoie vers un instrument de batterie (canal 10, notes GM)."),
        u8("- Les accords, la lead et la basse sont dans Orbe. Un ancien état d'Orbe (batterie comprise) se relit ici : voir la documentation.")});
    addAndMakeVisible(face.themeBtn);
    addAndMakeVisible(grid); addAndMakeVisible(handle);
    face.onMode = [this] {
        for (auto* k : k3) k->setup(&face.T, &face.art);
        for (auto* s : {barsSeg.get(), muteSeg.get(), midiSeg.get()}) { s->T = &face.T; s->repaint(); }
        for (auto* b : {&auditionB, &genB, &undoB, &redoB}) b->T = &face.T;
        help.T = &face.T; grid.T = &face.T; handle.T = &face.T;
        repaint();
    };
    setWantsKeyboardFocus(true);
    face.init(*this);
    startTimerHz(30);
}

OrbeRythmeEditor::~OrbeRythmeEditor() { stopTimer(); face.shutdown(); }

void OrbeRythmeEditor::resized() {
    u_ = face.onResized();
    help.layout(Si(944, 34, 30, 30), getLocalBounds());
    face.themeBtn.setBounds(Si(908, 35, 28, 28));
    styleBox.setBounds(Si(28, 110, 254, 32));
    barsSeg->setBounds(Si(28, 170, 120, 32)); barsSeg->px = 12.f * u_;
    midiSeg->setBounds(Si(162, 170, 120, 32)); midiSeg->px = 12.f * u_;
    const float kh = v3::Knob3::boxHeight(v3::KnobSize::S, false);
    const float kx[4] = {31.f, 171.f, 31.f, 171.f}, ky[4] = {214.f, 214.f, 322.f, 322.f};
    for (int i = 0; i < k3.size(); ++i) { k3[i]->setScale(u_); k3[i]->setBounds(Si(kx[i], ky[i], 108.f, kh)); }
    muteSeg->setBounds(Si(28, 450, 254, 32)); muteSeg->px = 12.f * u_;
    auditionB.setBounds(Si(28, 496, 124, 44)); genB.setBounds(Si(158, 496, 124, 44));
    grid.setBounds(Si(316, 112, kGridW, 292));
    handle.setBounds(Si(322, 444, 236, 72));
    undoB.setBounds(Si(574, 444, 100, 38)); redoB.setBounds(Si(680, 444, 100, 38));
}

void OrbeRythmeEditor::paintBackdrop(juce::Graphics& g, Rectangle<float> b) {
    const float u = b.getWidth() / kW;
    auto R = [u](float x, float y, float w, float h) { return Rectangle<float>(x * u, y * u, w * u, h * u); };
    const auto& T = face.T;
    v3::background(g, T, b);
    v3::rk::brand(g, T, u, "Orbe", "Rythme");
    v3::card(g, T, R(20, 82, 270, 478), 18.f * u);
    v3::hero(g, T, R(306, 82, 674, 330), 20.f * u);
    v3::card(g, T, R(306, 426, 674, 134), 18.f * u);
    auto lab = [&](const char* s, float x, float y, float w) { v3::rk::label(g, T.inkMute, u, u8(s), x, y, w, 11.f); };
    lab("Style", 32, 92, 200); lab("Mesures", 32, 152, 100); lab("Sortie MIDI", 166, 152, 112); lab("Aper\xC3\xA7u", 32, 432, 200);
    g.setColour(T.scrMute); v3::caps(g, "Grille de batterie", R(326, 94, 220, 16), 11.f * u, juce::Justification::centredLeft);
}

void OrbeRythmeEditor::paint(juce::Graphics& g) {
    const auto& T = face.T;
    face.art.draw(g, String("bd|") + face.artKey(), getLocalBounds().toFloat(), [this](juce::Graphics& ig, Rectangle<float> b) { paintBackdrop(ig, b); });
    const auto& st = proc.style();
    g.setColour(T.scrInk); g.setFont(v3::font(14.f * u_, true));
    g.drawText(String(st.label), S(660, 90, 150, 22), juce::Justification::centredRight, false);
    g.setColour(T.out); v3::tabular(g, v3::fmt::withUnit(String(st.bpm), "BPM"), S(816, 90, 148, 22), v3::font(14.f * u_, true), juce::Justification::centredRight);
    g.setColour(T.inkMute); g.setFont(v3::font(12.5f * u_));
    const String info = String(proc.bars()) + u8(" mesures · ") + String((int)proc.notes().size()) + " frappes" + (proc.edited() ? u8(" · modifié à la main") : String());
    g.drawText(info, S(796, 444, 168, 38), juce::Justification::centredLeft, true);
    g.drawFittedText(u8("Clic : poser ou retirer. Glisser : peindre. Maj + clic : vélocité."), S(574, 490, 390, 40).toNearestInt(), juce::Justification::topLeft, 2);
}

bool OrbeRythmeEditor::keyPressed(const juce::KeyPress& k) {
    const bool mod = k.getModifiers().isCommandDown() || k.getModifiers().isCtrlDown();
    if (mod && (k.getKeyCode() == 'Z' || k.getKeyCode() == 'z')) { if (k.getModifiers().isShiftDown()) proc.redo(); else proc.undo(); repaint(); return true; }
    if (mod && (k.getKeyCode() == 'Y' || k.getKeyCode() == 'y')) { proc.redo(); repaint(); return true; }
    return false;
}

void OrbeRythmeEditor::timerCallback() {
    static int tick = 0;
    for (auto* k : k3) k->tickMod(1.f / 30.f);
    undoB.setEnabled(proc.canUndo()); redoB.setEnabled(proc.canRedo());
    ++tick;
    if (proc.playStep.load() >= 0.f || tick % 6 == 0) grid.repaint();
    if (tick % 6 == 0) { repaint(S(306, 82, 674, 30).toNearestInt()); repaint(S(574, 426, 406, 134).toNearestInt()); }
}
