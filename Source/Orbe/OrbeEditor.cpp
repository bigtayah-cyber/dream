#include "OrbeEditor.h"

using namespace kit;
using juce::Colour, juce::Rectangle, juce::String;
namespace v3 = kit::v3;

namespace {
const Colour kLilac(0xFFCFC6FF), kIce(0xFFA9ECFF), kPeach(0xFFFFC4AB);
const char* kSharpN[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
String noteName(int n) { return String(kSharpN[n % 12]) + String(n / 12 - 1); }
}  // namespace

// ---------------- Segmented ----------------
void Segmented::paint(juce::Graphics& g) {
    if (v3::bridge::v3(T)) { v3::segGrid(g, *T, getLocalBounds().toFloat().reduced(1.f), labels_, idx, -1, labels_.size(), false, 11.5f); return; }
    auto r = getLocalBounds().toFloat().reduced(0.5f);
    g.setColour(juce::Colours::black.withAlpha(0.6f));
    g.fillRoundedRectangle(r, 4.f);
    g.setColour(juce::Colours::white.withAlpha(0.10f));
    g.drawRoundedRectangle(r, 4.f, 1.f);
    const int n = labels_.size();
    const float w = r.getWidth() / (float)n;
    for (int i = 0; i < n; ++i) {
        auto c = Rectangle<float>(r.getX() + w * (float)i, r.getY(), w, r.getHeight()).reduced(2.f);
        const bool on = i == idx;
        if (on) {
            g.setGradientFill(juce::ColourGradient(P.accent.withAlpha(0.55f), c.getX(), c.getY(), P.accent.withAlpha(0.22f), c.getX(), c.getBottom(), false));
            g.fillRoundedRectangle(c, 3.f);
            g.setColour(P.accent.withAlpha(0.9f));
            g.drawRoundedRectangle(c, 3.f, 1.f);
        }
        g.setColour(on ? juce::Colours::white : P.ink.withAlpha(0.6f));
        tracked(g, labels_[i], c, juce::Justification::centred, 1.1f, fnt(juce::jmin(10.5f, r.getHeight() * 0.36f), true));
    }
}
void Segmented::mouseDown(const juce::MouseEvent& e) {
    const int n = labels_.size();
    const int i = juce::jlimit(0, n - 1, (int)(e.position.x / ((float)getWidth() / (float)n)));
    att.setValueAsCompleteGesture((float)i);
}

// ---------------- MidiHandle ----------------
void MidiHandle::paint(juce::Graphics& g) {
    if (v3::bridge::v3(T)) {
        auto r = getLocalBounds().toFloat().reduced(1.f);
        const float u = (float)getWidth() / 216.f, rad = 12.f * u;
        g.setColour(T->field); g.fillRoundedRectangle(r, rad);
        juce::Path pth; pth.addRoundedRectangle(r.reduced(1.f), rad);
        const float dash[2] = {5.f * u, 4.f * u}; juce::Path d; juce::PathStrokeType(1.2f).createDashedStroke(d, pth, dash, 2);
        g.setColour(dragging ? T->accent : T->tick); g.fillPath(d);
        auto ic = Rectangle<float>(r.getX() + 16 * u, r.getCentreY() - 18 * u, 28 * u, 36 * u);
        juce::Path doc; doc.startNewSubPath(ic.getX(), ic.getY()); doc.lineTo(ic.getRight() - 9 * u, ic.getY()); doc.lineTo(ic.getRight(), ic.getY() + 9 * u); doc.lineTo(ic.getRight(), ic.getBottom()); doc.lineTo(ic.getX(), ic.getBottom()); doc.closeSubPath();
        g.setColour(T->card); g.fillPath(doc); g.setColour(T->inkMute); g.strokePath(doc, juce::PathStrokeType(1.f));
        g.setColour(T->accentText); g.setFont(v3::font(10.f * u, true)); g.drawText("MID", ic.withTop(ic.getCentreY() - 6 * u).withHeight(12 * u), juce::Justification::centred, false);
        g.setColour(T->ink); v3::caps(g, "Glisser le MIDI", Rectangle<float>(r.getX() + 54 * u, r.getCentreY() - 19 * u, r.getWidth() - 58 * u, 16 * u), 11.f * u, juce::Justification::centredLeft);
        g.setColour(T->inkMute); g.setFont(v3::font(11.5f * u)); g.drawFittedText(u8("vers une piste de ton DAW"), Rectangle<float>(r.getX() + 54 * u, r.getCentreY() + 1 * u, r.getWidth() - 58 * u, 28 * u).toNearestInt(), juce::Justification::topLeft, 2);
        return;
    }
    auto r = getLocalBounds().toFloat().reduced(1.f);
    const float u = (float)getWidth() / 216.f;
    g.setColour(juce::Colours::black.withAlpha(0.35f));
    g.fillRoundedRectangle(r, 8.f);
    const float dash[2] = {5.f, 4.f};
    juce::Path pth; pth.addRoundedRectangle(r.reduced(1.f), 8.f);
    juce::PathStrokeType st(1.2f);
    juce::Path dashed; st.createDashedStroke(dashed, pth, dash, 2);
    g.setColour(dragging ? P.accent : P.ink.withAlpha(0.35f));
    g.fillPath(dashed);
    // icône de fichier
    auto ic = Rectangle<float>(r.getX() + 18 * u, r.getCentreY() - 22 * u, 34 * u, 44 * u);
    juce::Path doc;
    doc.startNewSubPath(ic.getX(), ic.getY()); doc.lineTo(ic.getRight() - 10 * u, ic.getY()); doc.lineTo(ic.getRight(), ic.getY() + 10 * u);
    doc.lineTo(ic.getRight(), ic.getBottom()); doc.lineTo(ic.getX(), ic.getBottom()); doc.closeSubPath();
    g.setGradientFill(juce::ColourGradient(Colour(0xFF4A4A52), ic.getX(), ic.getY(), Colour(0xFF202024), ic.getX(), ic.getBottom(), false));
    g.fillPath(doc);
    g.setColour(juce::Colours::white.withAlpha(0.22f)); g.strokePath(doc, juce::PathStrokeType(1.f));
    g.setColour(P.accent);
    g.setFont(mono(11.f * u)); g.drawText(".MID", ic.withTop(ic.getCentreY() - 6 * u).withHeight(12 * u), juce::Justification::centred);
    g.setColour(P.ink.withAlpha(0.9f));
    tracked(g, "Glisser le MIDI", Rectangle<float>(r.getX() + 62 * u, r.getCentreY() - 16 * u, r.getWidth() - 66 * u, 16 * u), juce::Justification::centredLeft, 1.1f, fnt(11.f * u, true));
    g.setColour(P.mute);
    g.setFont(fnt(10.5f * u));
    g.drawFittedText(u8("vers une piste de ton DAW"), Rectangle<float>(r.getX() + 62 * u, r.getCentreY(), r.getWidth() - 66 * u, 28 * u).toNearestInt(), juce::Justification::topLeft, 2);
}
void MidiHandle::mouseDrag(const juce::MouseEvent& e) {
    if (dragging || e.getDistanceFromDragStart() < 6) return;
    dragging = true; repaint();
    auto f = proc.writeMidiFile(chord, lead, bass, drums);
    if (auto* c = juce::DragAndDropContainer::findParentDragContainerFor(this))
        c->performExternalDragDropOfFiles({f.getFullPathName()}, false, this, [this] { dragging = false; repaint(); });
}

// ---------------- OrbeEditor ----------------
Palette OrbeEditor::palette() {
    Palette p;
    p.plateTop = Colour(0xFF2B2B31); p.plateBot = Colour(0xFF16161A);
    p.ink = Colour(0xFFE9E7F0); p.mute = Colour(0xFF8F8D9B); p.line = Colour(0xFF3B3B44);
    p.accent = Colour(0xFFB9A9FF); p.accent2 = kIce;
    p.screen = Colour(0xFF0B0C11); p.screenInk = Colour(0xFFEDEBF6);
    p.brush = 1.f;
    return p;
}

OrbeEditor::OrbeEditor(OrbeProcessor& p) : AudioProcessorEditor(&p), proc(p), pal(palette()), look(pal) {
    setLookAndFeel(&look);
    auto& ap = proc.apvts;

    String lastGroup;
    int id = 1;
    for (auto& s : orbe::styles()) {
        if (lastGroup != s.group) { styleBox.addSectionHeading(s.group); lastGroup = s.group; }
        styleBox.addItem(s.label, id++);
    }
    for (int i = 0; i < 12; ++i) keyBox.addItem(kSharpN[i], i + 1);
    for (auto& s : orbe::scales()) scaleBox.addItem(s.label, (int)(&s - &orbe::scales()[0]) + 1);
    for (auto* c : {&styleBox, &keyBox, &scaleBox}) { addAndMakeVisible(c); c->setJustificationType(juce::Justification::centredLeft); }
    styleAtt = std::make_unique<ComboAtt>(ap, "style", styleBox);
    keyAtt = std::make_unique<ComboAtt>(ap, "key", keyBox);
    scaleAtt = std::make_unique<ComboAtt>(ap, "scale", scaleBox);

    barsSeg = std::make_unique<Segmented>(*ap.getParameter("bars"), juce::StringArray{"4", "8"}, pal);
    arpSeg = std::make_unique<Segmented>(*ap.getParameter("arp"), juce::StringArray{u8("Plaq."), u8("Mont."), u8("A/R"), u8("Aléa")}, pal);
    addAndMakeVisible(*barsSeg); addAndMakeVisible(*arpSeg);

    b7.setButtonText("7e"); bAdd9.setButtonText("Add9"); bBorrow.setButtonText("Emprunts");
    for (auto* b : {&b7, &bAdd9, &bBorrow}) addAndMakeVisible(b);
    a7 = std::make_unique<ButtonAtt>(ap, "seventh", b7);
    a9 = std::make_unique<ButtonAtt>(ap, "add9", bAdd9);
    aB = std::make_unique<ButtonAtt>(ap, "borrow", bBorrow);

    auto knobOf = [&](juce::Slider& s) { s.setSliderStyle(juce::Slider::RotaryVerticalDrag); s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0); s.setMouseDragSensitivity(180); addAndMakeVisible(s); };
    knobOf(densityK); knobOf(humanK); knobOf(masterK);
    densityK.setTooltip(u8("Densité de la mélodie")); humanK.setTooltip(u8("Humanisation : décale légèrement le temps et la vélocité"));
    masterK.setTooltip(u8("Volume de l'aperçu sonore"));
    densAtt = std::make_unique<SliderAtt>(ap, "density", densityK);
    humAtt = std::make_unique<SliderAtt>(ap, "human", humanK);
    masterAtt = std::make_unique<SliderAtt>(ap, "master", masterK);

    for (int i = 0; i < 4; ++i) {
        fader[i].setSliderStyle(juce::Slider::LinearVertical); fader[i].setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible(fader[i]);
        volAtt[i] = std::make_unique<SliderAtt>(ap, "vol" + String(i), fader[i]);
        mute[i].setButtonText("Muet"); addAndMakeVisible(mute[i]);
        muteAtt[i] = std::make_unique<ButtonAtt>(ap, "mute" + String(i), mute[i]);
    }
    midiOut.setButtonText("Sortie MIDI"); addAndMakeVisible(midiOut);
    midiAtt = std::make_unique<ButtonAtt>(ap, "midiout", midiOut);
    midiOut.setTooltip(u8("Envoie aussi les notes en MIDI (canaux 1, 2, 3 et 10) vers les instruments de ton DAW"));

    for (auto* b : {&auditionB, &genAll, &genChords, &genLead}) addAndMakeVisible(b);
    {   // v3 : potards haut de gamme sur les mêmes paramètres
        k3.add(new v3::Knob3(ap, "density", u8("Densité"), u8("Densité de la mélodie"), v3::KnobSize::S));
        k3.add(new v3::Knob3(ap, "human", "Humain", u8("Humanisation : décale légèrement le temps et la vélocité"), v3::KnobSize::S));
        k3.add(new v3::Knob3(ap, "master", "Master", u8("Volume de l'aperçu sonore"), v3::KnobSize::S));
        k3[0]->setFormatter([](double v) { return v3::fmt::pct(v); });
        k3[1]->setFormatter([](double v) { return v3::fmt::pct(v); });
        k3[2]->setFormatter([](double v) { return v3::fmt::pct(v); });
        for (auto* k : k3) { k->setLabelPx(10.f); addChildComponent(k); }
    }
    addAndMakeVisible(skin.themeBtn);
    skin.onMode = [this] {
        const bool n3 = skin.on(); const auto* tp = skin.tp();
        for (auto* s : {&densityK, &humanK, &masterK}) s->setVisible(!n3);
        for (auto* k : k3) { k->setVisible(n3); k->setup(&skin.T, &skin.art); }
        barsSeg->T = tp; arpSeg->T = tp; handle->T = tp; repaint();
    };
    auditionB.setClickingTogglesState(true);
    auditionB.setTooltip(u8("Joue la boucle ici. Dans le DAW, la lecture suit directement le transport."));
    auditionB.onClick = [this] { proc.audition = auditionB.getToggleState(); auditionB.setButtonText(auditionB.getToggleState() ? u8("Stop") : u8("Écouter")); };
    genAll.onClick = [this] { proc.generate(OrbeProcessor::Gen::All); repaint(); };
    genChords.onClick = [this] { proc.generate(OrbeProcessor::Gen::Chords); repaint(); };
    genLead.onClick = [this] { proc.generate(OrbeProcessor::Gen::Lead); repaint(); };
    genAll.setTooltip(u8("Nouvelle progression et nouvelle mélodie (les accords verrouillés restent)"));
    genChords.setTooltip(u8("Nouvelle progression, la mélodie s'y adapte")); genLead.setTooltip(u8("Nouvelle mélodie sur les mêmes accords"));

    handle = std::make_unique<MidiHandle>(proc, pal);
    addAndMakeVisible(*handle);
    const char* cn[4] = {"Acc", "Lead", "Bas", "Bat"};
    for (int i = 0; i < 4; ++i) {
        chip[i].setButtonText(cn[i]); chip[i].setClickingTogglesState(true); chip[i].setToggleState(true, juce::dontSendNotification);
        chip[i].onClick = [this, i] { bool v = chip[i].getToggleState(); (i == 0 ? handle->chord : i == 1 ? handle->lead : i == 2 ? handle->bass : handle->drums) = v; };
        addAndMakeVisible(chip[i]);
    }

    setWantsKeyboardFocus(true);
    setResizable(true, true);
    setResizeLimits(800, 512, 1700, 1088);
    getConstrainer()->setFixedAspectRatio(1000.0 / 640.0);
    setSize(1000, 640);
    skin.init(*this, look, 1000.f, 640.f);
    startTimerHz(30);
}

OrbeEditor::~OrbeEditor() { stopTimer(); skin.shutdown(); }

void OrbeEditor::resized() {
    u_ = (float)getWidth() / 1000.f;
    skin.scale = u_;
    if (skin.on()) { resized3(); return; }
    const int w = getWidth(), h = getHeight();
    plate = brushedPlate(w, h, pal);
    skin.themeBtn.setBounds(Si(606, 24, 26, 26));

    styleBox.setBounds(Si(24, 102, 260, 30));
    keyBox.setBounds(Si(24, 158, 108, 30));
    scaleBox.setBounds(Si(144, 158, 140, 30));
    barsSeg->setBounds(Si(24, 214, 76, 30));
    arpSeg->setBounds(Si(112, 214, 172, 30));
    b7.setBounds(Si(24, 264, 70, 22)); bAdd9.setBounds(Si(100, 264, 84, 22)); bBorrow.setBounds(Si(180, 264, 104, 22));

    densityK.setBounds(Si(34, 330, 100, 100)); humanK.setBounds(Si(174, 330, 100, 100));

    auditionB.setBounds(Si(24, 520, 124, 40)); genAll.setBounds(Si(160, 520, 124, 40));
    genChords.setBounds(Si(24, 570, 124, 36)); genLead.setBounds(Si(160, 570, 124, 36));

    for (int i = 0; i < 4; ++i) {
        const float x = 324.f + (float)i * 84.f;
        fader[i].setBounds(Si(x + 16, 534, 40, 68));
        mute[i].setBounds(Si(x + 2, 604, 70, 22));
    }
    masterK.setBounds(Si(668, 524, 84, 84));
    handle->setBounds(Si(764, 508, 212, 70));
    for (int i = 0; i < 4; ++i) chip[i].setBounds(Si(764.f + (float)i * 53.f, 608.f, 50, 17));
    midiOut.setBounds(Si(764, 582, 160, 22));
    repaint();
}

void OrbeEditor::timerCallback() {
    static int tick = 0;
    if (skin.on()) for (auto* k : k3) k->tickMod(1.f / 30.f);
    if (proc.playStep.load() >= 0.f || (++tick % 6) == 0) repaint(S(300, 84, 690, 410).toNearestInt());
}

Rectangle<float> OrbeEditor::cardRect(int i, int n, float u) const {
    const float gap = 8.f * u, total = 672.f * u;
    const float w = (total - gap * (float)(n - 1)) / (float)n;
    return {304.f * u + (w + gap) * (float)i, 100.f * u, w, 106.f * u};
}

// ---------------- édition ----------------
namespace {
const Colour kTrackCol[4] = {kLilac, kIce, kPeach, Colour(0xFFE9E7F0)};
const int kDrumRow[7] = {49, 46, 42, 39, 38, 37, 36};
const char* kDrumName[7] = {"Crash", "Ch. ouvert", "Charley", "Clap", "Caisse cl.", "Rim", "Grosse c."};
int rowOfDrum(int n) { for (int i = 0; i < 7; ++i) if (kDrumRow[i] == n) return i; return n == 35 ? 6 : 2; }
}  // namespace

std::vector<OrbeEditor::Btn> OrbeEditor::buttons() const {
    std::vector<Btn> v;
    const float y = 213, h = 22;
    for (int i = 0; i < 4; ++i) { v.push_back({S(304.f + (float)i * 84.f, y, 50, h), 0, i}); v.push_back({S(304.f + (float)i * 84.f + 52, y, 24, h), 1, i}); }
    v.push_back({S(648, y, 54, h), 2, 0}); v.push_back({S(706, y, 54, h), 2, 1});
    v.push_back({S(840, y, 64, h), 3, 0}); v.push_back({S(908, y, 68, h), 4, 0});
    return v;
}

OrbeEditor::Geo OrbeEditor::geo() const {
    Geo g;
    g.a = S(312, 262, 656, 172);
    const int bars = juce::jmax(1, (int)proc.song().prog.size());
    g.stepW = g.a.getWidth() / (float)(bars * 16);
    if (editTrack == 3) { g.lo = 0; g.hi = 6; g.rowH = g.a.getHeight() / 7.f; return g; }
    static const int dlo[3] = {47, 58, 28}, dhi[3] = {83, 95, 56};
    g.lo = dlo[editTrack]; g.hi = dhi[editTrack];
    for (auto& n : proc.notes(editTrack)) { g.lo = std::min(g.lo, n.n - 1); g.hi = std::max(g.hi, n.n + 1); }
    g.rowH = g.a.getHeight() / (float)(g.hi - g.lo + 1);
    return g;
}

Rectangle<float> OrbeEditor::noteRect(const orbe::Note& n, const Geo& g) const {
    const float x = g.a.getX() + n.t * g.stepW;
    if (editTrack == 3) { const float rh = g.rowH; return {x - 1.f, g.a.getY() + (float)rowOfDrum(n.n) * rh + rh * 0.2f, juce::jmax(5.f * u_, g.stepW * 0.8f), rh * 0.6f}; }
    return {x, g.a.getY() + (float)(g.hi - n.n) * g.rowH + 0.5f, juce::jmax(3.f, n.d * g.stepW - 1.f), juce::jmax(3.f, g.rowH - 1.f)};
}

int OrbeEditor::findNote(int tr, juce::Point<float> p, const Geo& g) const {
    const auto& v = proc.notes(tr);
    for (int i = (int)v.size() - 1; i >= 0; --i) if (noteRect(v[(size_t)i], g).expanded(1.f, 1.f).contains(p)) return i;
    return -1;
}

void OrbeEditor::chordMenu(int bar, int slot) {
    const auto& sl = proc.song().slots;
    int cur = -1;
    { int k = 0; for (auto& c : sl) if (c.bar == bar) { if (k++ == slot) cur = c.deg; } }
    juce::PopupMenu m;
    auto label = [&](int d) { auto c = proc.chordFor(d); return String(c.name) + "   " + String(c.roman); };
    m.addSectionHeader(u8("Alternatives suggérées"));
    for (int d : proc.suggest(bar)) if (d != cur) m.addItem(100 + d, label(d));
    m.addSectionHeader("Gamme");
    for (int d = 0; d < 7; ++d) m.addItem(100 + d, label(d), true, d == cur);
    m.addSectionHeader("Emprunts");
    for (int d : proc.borrowList()) m.addItem(100 + d, label(d), true, d == cur);
    if (slot == 0) {
        juce::PopupMenu two;
        two.addItem(299, "Aucun");
        for (int d = 0; d < 7; ++d) two.addItem(300 + d, label(d));
        m.addSeparator(); m.addSubMenu(u8("2e accord (mi-mesure)"), two);
    } else m.addSeparator(), m.addItem(299, u8("Retirer le 2e accord"));
    juce::Component::SafePointer<OrbeEditor> safe(this);
    m.showMenuAsync(juce::PopupMenu::Options().withMinimumWidth(180), [safe, bar, slot](int r) {
        if (!safe || r <= 0) return;
        if (r == 299) safe->proc.setChord(bar, 1, -1);
        else if (r >= 300) safe->proc.setChord(bar, 1, r - 300);
        else safe->proc.setChord(bar, slot, r - 100);
        safe->repaint();
    });
}

void OrbeEditor::mouseDown(const juce::MouseEvent& e) {
    grabKeyboardFocus();
    for (auto& b : buttons()) if (b.r.contains(e.position)) {
        if (b.kind == 0) editTrack = b.idx;
        else if (b.kind == 1) proc.toggleTrackLock(b.idx);
        else if (b.kind == 2) tool = b.idx;
        else if (b.kind == 3) proc.undo();
        else proc.redo();
        repaint(); return;
    }
    const int n = (int)proc.song().chords.size();
    for (int i = 0; i < n; ++i) {
        auto r = cardRect(i, n, u_);
        const float s = 22.f * u_;
        if (Rectangle<float>(r.getX() + 4 * u_, r.getBottom() - s - 2 * u_, s + 6 * u_, s + 2 * u_).contains(e.position)) { proc.toggleLock(i); repaint(); return; }
        if (Rectangle<float>(r.getRight() - s - 8 * u_, r.getBottom() - s - 2 * u_, s + 6 * u_, s + 2 * u_).contains(e.position)) { proc.rerollChord(i); repaint(); return; }
        if (r.withTrimmedBottom(s + 4 * u_).contains(e.position)) {
            bool two = false; for (auto& c : proc.song().slots) two |= c.bar == i && c.t > (float)i * 16;
            chordMenu(i, two && e.position.x > r.getCentreX() ? 1 : 0); return;
        }
    }
    const auto G = geo();
    if (!G.a.contains(e.position)) return;
    drag = {};
    const int tr = editTrack;
    const float t = (e.position.x - G.a.getX()) / G.stepW;
    const int row = juce::jlimit(0, G.hi - G.lo, (int)((e.position.y - G.a.getY()) / G.rowH));
    const int hit = findNote(tr, e.position, G);
    if (e.mods.isPopupMenu() || tool == 1) { drag.mode = 3; if (hit >= 0) { pushOnce(); proc.removeNote(tr, hit); } repaint(); return; }
    if (tr == 3) {
        pushOnce();
        if (hit >= 0) proc.removeNote(tr, hit); else proc.addNote(tr, std::floor(t), 0.5f, kDrumRow[row]);
        repaint(); return;
    }
    if (hit >= 0) {
        const auto nr = noteRect(proc.notes(tr)[(size_t)hit], G);
        drag.mode = e.position.x > nr.getRight() - juce::jmax(4.f * u_, nr.getWidth() * 0.3f) && nr.getWidth() > 8.f * u_ ? 2 : 1;
        drag.idx = hit; drag.grab = t - proc.notes(tr)[(size_t)hit].t;
    } else {
        pushOnce();
        drag.idx = proc.addNote(tr, std::floor(t), tr == 0 ? 4.f : 2.f, G.hi - row);
        drag.mode = 2; drag.added = true;
    }
    repaint();
}

void OrbeEditor::mouseDrag(const juce::MouseEvent& e) {
    if (!drag.mode) return;
    const auto G = geo();
    const int tr = editTrack;
    const float t = (e.position.x - G.a.getX()) / G.stepW;
    if (drag.mode == 3) { const int h = findNote(tr, e.position, G); if (h >= 0) { pushOnce(); proc.removeNote(tr, h); repaint(); } return; }
    if (drag.idx < 0 || drag.idx >= (int)proc.notes(tr).size()) return;
    const orbe::Note n = proc.notes(tr)[(size_t)drag.idx];
    if (drag.mode == 1) {
        const int pitch = G.hi - juce::jlimit(0, G.hi - G.lo, (int)((e.position.y - G.a.getY()) / G.rowH));
        const float nt = std::round(t - drag.grab);
        if (nt != n.t || pitch != n.n) { drag.moved = true; pushOnce(); proc.setNote(tr, drag.idx, nt, n.d, pitch); }
    } else {
        const float nd = std::max(1.f, std::round(t - n.t));
        if (nd != n.d) { drag.moved = true; pushOnce(); proc.setNote(tr, drag.idx, n.t, nd, n.n); }
    }
    repaint();
}

void OrbeEditor::mouseUp(const juce::MouseEvent&) {
    // clic sans glisser sur une note existante = la supprimer
    if ((drag.mode == 1 || drag.mode == 2) && !drag.moved && !drag.added && drag.idx >= 0 && drag.idx < (int)proc.notes(editTrack).size()) {
        pushOnce(); proc.removeNote(editTrack, drag.idx);
    }
    drag = {};
    repaint();
}

bool OrbeEditor::keyPressed(const juce::KeyPress& k) {
    const bool mod = k.getModifiers().isCommandDown() || k.getModifiers().isCtrlDown();
    if (mod && (k.getKeyCode() == 'Z' || k.getKeyCode() == 'z')) { if (k.getModifiers().isShiftDown()) proc.redo(); else proc.undo(); repaint(); return true; }
    if (mod && (k.getKeyCode() == 'Y' || k.getKeyCode() == 'y')) { proc.redo(); repaint(); return true; }
    return false;
}

void OrbeEditor::paint(juce::Graphics& g) {
    if (skin.on()) { paint3(g); return; }
    const float u = u_;
    g.drawImageAt(plate, 0, 0);
    // filets de séparation gravés
    g.setColour(Colour(0xFF000000).withAlpha(0.5f)); g.fillRect(S(0, 70, 1000, 1.5f));
    g.setColour(juce::Colours::white.withAlpha(0.06f)); g.fillRect(S(0, 71.5f, 1000, 1.f));
    g.setColour(Colour(0xFF000000).withAlpha(0.5f)); g.fillRect(S(0, 498, 1000, 1.5f));
    g.setColour(juce::Colours::white.withAlpha(0.06f)); g.fillRect(S(0, 499.5f, 1000, 1.f));
    for (auto p : {juce::Point<float>(12, 12), {988, 12}, {12, 628}, {988, 628}}) screw(g, p.x * u, p.y * u, 5.f * u, (p.x + p.y) * 0.9f);

    paintHeader(g, u);

    // panneau gauche : libellés gravés
    engraved(g, pal, "Style", S(24, 84, 200, 14), juce::Justification::centredLeft, 10.f * u, 2.f * u);
    engraved(g, pal, "Tonique", S(24, 140, 108, 14), juce::Justification::centredLeft, 10.f * u, 2.f * u);
    engraved(g, pal, "Gamme", S(144, 140, 140, 14), juce::Justification::centredLeft, 10.f * u, 2.f * u);
    engraved(g, pal, "Mesures", S(24, 196, 76, 14), juce::Justification::centredLeft, 10.f * u, 2.f * u);
    engraved(g, pal, "Jeu des accords", S(112, 196, 172, 14), juce::Justification::centredLeft, 10.f * u, 2.f * u);
    engraved(g, pal, "Harmonie", S(24, 246, 140, 14), juce::Justification::centredLeft, 10.f * u, 2.f * u);
    g.setColour(pal.line); g.fillRect(S(24, 302, 260, 1.f));
    engraved(g, pal, u8("Mélodie"), S(24, 308, 260, 14), juce::Justification::centredLeft, 10.f * u, 2.f * u);
    engraved(g, pal, u8("Densité"), S(34, 436, 100, 14), juce::Justification::centred, 10.f * u, 2.f * u);
    engraved(g, pal, "Humain", S(174, 436, 100, 14), juce::Justification::centred, 10.f * u, 2.f * u);
    g.setColour(pal.mute);
    g.setFont(mono(10.f * u));
    g.drawText(String(juce::roundToInt(proc.apvts.getRawParameterValue("density")->load() * 100)) + " %", S(34, 452, 100, 14).toNearestInt(), juce::Justification::centred);
    g.drawText(String(juce::roundToInt(proc.apvts.getRawParameterValue("human")->load() * 100)) + " %", S(174, 452, 100, 14).toNearestInt(), juce::Justification::centred);

    // afficheur
    screenWell(g, pal, S(304, 96, 672, 114));  // fond des cartes
    paintCards(g, u);
    paintToolbar(g, u);
    screenWell(g, pal, S(304, 240, 672, 250));
    paintRoll(g, u);

    // pistes
    const char* tn[4] = {"Accords", "Lead", "Basse", "Batterie"};
    const Colour tc[4] = {kLilac, kIce, kPeach, Colour(0xFFE9E7F0)};
    for (int i = 0; i < 4; ++i) {
        const float x = 324.f + (float)i * 84.f;
        g.setColour(tc[i]);
        g.fillRoundedRectangle(S(x + 18, 513, 36, 3), 1.5f * u);
        engraved(g, pal, tn[i], S(x - 4, 518, 80, 12), juce::Justification::centred, 9.f * u, 1.6f * u);
    }
    engraved(g, pal, "Master", S(668, 612, 84, 12), juce::Justification::centred, 9.f * u, 1.6f * u);
}

void OrbeEditor::paintHeader(juce::Graphics& g, float u) {
    // orbe : sphère éclairée entourée d'un anneau, la signature de la marque
    const float cx = 48 * u, cy = 36 * u, r = 17 * u;
    g.setColour(juce::Colours::black.withAlpha(0.5f)); g.fillEllipse(cx - r - 3 * u, cy - r - 1 * u, (r + 3 * u) * 2, (r + 3 * u) * 2);
    g.setGradientFill(juce::ColourGradient(kIce, cx - r * 0.5f, cy - r * 0.6f, Colour(0xFF5A46C8), cx + r, cy + r, true));
    g.fillEllipse(cx - r, cy - r, r * 2, r * 2);
    g.setColour(juce::Colours::white.withAlpha(0.5f)); g.fillEllipse(cx - r * 0.62f, cy - r * 0.78f, r * 0.7f, r * 0.42f);
    g.setColour(kLilac.withAlpha(0.8f)); g.drawEllipse(cx - r * 1.28f, cy - r * 1.28f, r * 2.56f, r * 2.56f, 1.f * u);
    engraved(g, pal, "Orbe", S(84, 18, 200, 34), juce::Justification::centredLeft, 28.f * u, 9.f * u, true);
    g.setColour(pal.mute); g.setFont(fnt(10.f * u));
    g.drawText(u8("Accords · mélodie · basse · batterie"), S(290, 26, 280, 20).toNearestInt(), juce::Justification::centredLeft);

    const auto& st = proc.style();
    const auto& ap = proc.apvts;
    const int key = (int)ap.getRawParameterValue("key")->load(), sc = (int)ap.getRawParameterValue("scale")->load();
    const float hb = proc.hostBpm.load();
    const int bpm = hb > 0.f ? juce::roundToInt(hb) : st.bpm;
    screenWell(g, pal, S(640, 18, 336, 36));
    g.setColour(kIce); g.setFont(mono(13.f * u));
    String line = String(st.label).toUpperCase() + "   " + String(bpm) + " BPM   " + kSharpN[key] + " " + String(orbe::scales()[(size_t)sc].label).toUpperCase();
    g.drawText(line, S(652, 18, 320, 36).toNearestInt(), juce::Justification::centredLeft);
}

void OrbeEditor::paintCards(juce::Graphics& g, float u) {
    const auto& song = proc.song();
    const int n = (int)song.chords.size();
    const float step = proc.playStep.load();
    const int cur = step >= 0.f ? (int)(step / 16.f) : -1;
    for (int i = 0; i < n; ++i) {
        const auto& c = song.chords[(size_t)i];
        const orbe::Chord* c2 = nullptr;
        for (auto& s : song.slots) if (s.bar == i && s.t > (float)i * 16) c2 = &s;
        auto r = cardRect(i, n, u);
        const bool playing = i == cur, locked = proc.isLocked(i);
        g.setColour(playing ? kLilac.withAlpha(0.20f) : juce::Colours::white.withAlpha(0.045f));
        g.fillRoundedRectangle(r, 5.f * u);
        g.setColour(playing ? kLilac.withAlpha(0.9f) : juce::Colours::white.withAlpha(0.10f));
        g.drawRoundedRectangle(r, 5.f * u, playing ? 1.6f : 1.f);
        auto one = [&](const orbe::Chord& ch, Rectangle<float> rr, bool half) {
            g.setColour(kLilac.withAlpha(0.85f));
            g.setFont(mono(juce::jmin(11.f * u, rr.getWidth() * 0.16f)));
            g.drawText(ch.roman, Rectangle<float>(rr.getX() + 6 * u, rr.getY() + 6 * u, rr.getWidth() - 12 * u, 14 * u), juce::Justification::centredLeft);
            if (ch.borrowed && !half) { g.setColour(kPeach); g.drawText("emprunt", Rectangle<float>(rr.getX() + 8 * u, rr.getY() + 6 * u, rr.getWidth() - 16 * u, 14 * u), juce::Justification::centredRight); }
            g.setColour(pal.screenInk);
            g.setFont(fnt(juce::jmin(24.f * u, rr.getWidth() * 0.30f), true));
            g.drawFittedText(ch.name, Rectangle<float>(rr.getX() + 2 * u, rr.getY() + 22 * u, rr.getWidth() - 4 * u, 30 * u).toNearestInt(), juce::Justification::centred, 1, 0.5f);
            String ns;
            for (size_t k = 0; k < ch.voicing.size(); ++k) ns << (k ? " " : "") << noteName(ch.voicing[k]);
            g.setColour(pal.mute); g.setFont(mono(juce::jmin(10.f * u, rr.getWidth() * (half ? 0.17f : 0.085f))));
            g.drawFittedText(ns, Rectangle<float>(rr.getX() + 3 * u, rr.getY() + 54 * u, rr.getWidth() - 6 * u, 24 * u).toNearestInt(), juce::Justification::centred, 2);
        };
        if (c2) {
            auto left = r.withWidth(r.getWidth() / 2), right = r.withTrimmedLeft(r.getWidth() / 2);
            one(c, left, true); one(*c2, right, true);
            g.setColour(juce::Colours::white.withAlpha(0.12f)); g.fillRect(r.getCentreX() - 0.5f, r.getY() + 8 * u, 1.f, 68 * u);
        } else one(c, r, false);
        // cadenas et relance
        const float s = 22.f * u;
        auto lk = Rectangle<float>(r.getX() + 6 * u, r.getBottom() - s - 2 * u, s, s);
        g.setColour(locked ? kPeach : pal.mute.withAlpha(0.55f));
        juce::Path body; body.addRoundedRectangle(lk.getX() + s * 0.2f, lk.getY() + s * 0.46f, s * 0.6f, s * 0.42f, 2.f);
        g.fillPath(body);
        juce::Path sh;
        if (locked) sh.addCentredArc(lk.getCentreX(), lk.getY() + s * 0.46f, s * 0.2f, s * 0.26f, 0, -juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi, true);
        else sh.addCentredArc(lk.getCentreX() + s * 0.1f, lk.getY() + s * 0.42f, s * 0.2f, s * 0.26f, 0, -juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi * 0.6f, true);
        g.strokePath(sh, juce::PathStrokeType(1.6f * u));
        auto rl = Rectangle<float>(r.getRight() - s - 8 * u, r.getBottom() - s - 2 * u, s, s);
        g.setColour(pal.mute.withAlpha(0.8f));
        juce::Path ar; ar.addCentredArc(rl.getCentreX(), rl.getCentreY(), s * 0.3f, s * 0.3f, 0, 0.6f, 5.2f, true);
        g.strokePath(ar, juce::PathStrokeType(1.6f * u));
        juce::Path tip; tip.addTriangle(rl.getCentreX() + s * 0.3f, rl.getCentreY() - s * 0.02f, rl.getCentreX() + s * 0.46f, rl.getCentreY() - s * 0.2f, rl.getCentreX() + s * 0.14f, rl.getCentreY() - s * 0.22f);
        g.fillPath(tip);
    }
}

void OrbeEditor::paintToolbar(juce::Graphics& g, float u) {
    const char* tn[4] = {"Accords", "Lead", "Basse", "Batterie"};
    const char* tool_[2] = {"Crayon", "Gomme"};
    for (auto& b : buttons()) {
        auto r = b.r.reduced(0.5f);
        const bool sel = (b.kind == 0 && editTrack == b.idx) || (b.kind == 2 && tool == b.idx);
        const bool dis = (b.kind == 3 && !proc.canUndo()) || (b.kind == 4 && !proc.canRedo());
        const Colour col = b.kind == 0 || b.kind == 1 ? kTrackCol[b.idx] : pal.accent;
        g.setColour(juce::Colours::black.withAlpha(0.45f)); g.fillRoundedRectangle(r, 4.f * u);
        if (sel) { g.setColour(col.withAlpha(0.25f)); g.fillRoundedRectangle(r, 4.f * u); }
        g.setColour(sel ? col.withAlpha(0.9f) : juce::Colours::white.withAlpha(0.12f)); g.drawRoundedRectangle(r, 4.f * u, 1.f);
        if (b.kind == 1) {  // cadenas : verrou de piste
            const bool lk = proc.trackLocked(b.idx);
            const float s = r.getHeight() * 0.8f;
            auto m = Rectangle<float>(r.getCentreX() - s * 0.3f, r.getCentreY() - s * 0.02f, s * 0.6f, s * 0.42f);
            g.setColour(lk ? kPeach : pal.mute.withAlpha(0.6f));
            g.fillRoundedRectangle(m, 1.5f * u);
            juce::Path sh;
            sh.addCentredArc(m.getCentreX() + (lk ? 0.f : s * 0.14f), m.getY(), s * 0.17f, s * 0.24f, 0, -juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi, true);
            g.strokePath(sh, juce::PathStrokeType(1.5f * u));
            continue;
        }
        String txt = b.kind == 0 ? String(tn[b.idx]) + (proc.trackEdited(b.idx) ? " *" : "") : b.kind == 2 ? String(tool_[b.idx]) : b.kind == 3 ? String(u8("Annuler")) : String(u8("Rétablir"));
        g.setColour(dis ? pal.mute.withAlpha(0.4f) : sel ? juce::Colours::white : pal.ink.withAlpha(0.85f));
        tracked(g, txt, r, juce::Justification::centred, 0.8f, fnt(9.f * u, true));
    }
}

void OrbeEditor::paintRoll(juce::Graphics& g, float u) {
    const bool n3 = skin.on();
    const Colour muteC = n3 ? skin.T.scrMute : muteC;
    auto fm = [&](float px) { return n3 ? v3::font(px * 1.25f) : mono(px); };
    const auto& song = proc.song();
    const int bars = (int)song.prog.size();
    if (bars == 0) return;
    const auto G = geo();
    const auto area = G.a;
    const float stepW = G.stepW;
    const Colour tc = kTrackCol[editTrack];

    // fond de la zone d'édition : lignes de hauteur (touches noires plus sombres) ou lignes de batterie
    for (int r = 0; r <= G.hi - G.lo; ++r) {
        const int n = G.hi - r;
        const float y = area.getY() + (float)r * G.rowH;
        if (editTrack != 3) {
            static const bool blk[12] = {0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0};
            if (blk[n % 12]) { g.setColour(juce::Colours::black.withAlpha(0.22f)); g.fillRect(area.getX(), y, area.getWidth(), G.rowH); }
            if (n % 12 == 0) {
                g.setColour(juce::Colours::white.withAlpha(0.10f)); g.fillRect(area.getX(), y + G.rowH - 1.f, area.getWidth(), 1.f);
                g.setColour(muteC.withAlpha(0.8f)); g.setFont(fm(8.5f * u));
                g.drawText(noteName(n), Rectangle<float>(area.getX() + 2 * u, y - 3 * u, 30 * u, G.rowH + 6 * u), juce::Justification::centredLeft);
            }
        } else {
            g.setColour(juce::Colours::white.withAlpha(r % 2 ? 0.0f : 0.03f)); g.fillRect(area.getX(), y, area.getWidth(), G.rowH);
            g.setColour(muteC.withAlpha(0.8f)); g.setFont(fm(8.5f * u));
            g.drawText(kDrumName[r], Rectangle<float>(area.getX() + 2 * u, y, 60 * u, G.rowH), juce::Justification::centredLeft);
        }
    }
    // grille : mesures, temps et pas
    for (int s = 0; s <= bars * 16; ++s) {
        const bool bar = s % 16 == 0, beat = s % 4 == 0;
        if (!beat && stepW < 5.f * u) continue;
        g.setColour(juce::Colours::white.withAlpha(bar ? 0.20f : beat ? 0.09f : 0.035f));
        g.fillRect(area.getX() + (float)s * stepW, area.getY(), bar ? 1.4f : 1.f, area.getHeight());
        if (bar && s < bars * 16) { g.setColour(muteC.withAlpha(0.7f)); g.setFont(fm(9.f * u)); g.drawText(String(s / 16 + 1), Rectangle<float>(area.getX() + (float)s * stepW + 3 * u, area.getY() - 12 * u, 24 * u, 11 * u), juce::Justification::centredLeft); }
    }
    // accords en fantôme pendant l'édition de la mélodie
    if (editTrack == 1) for (auto& n : song.nChords) if (n.n >= G.lo && n.n <= G.hi) {
        g.setColour(kLilac.withAlpha(0.16f)); g.fillRoundedRectangle(Rectangle<float>(area.getX() + n.t * stepW, area.getY() + (float)(G.hi - n.n) * G.rowH + 0.5f, juce::jmax(2.f, n.d * stepW - 1.f), juce::jmax(2.f, G.rowH - 1.f)), 1.f);
    }
    for (auto& n : proc.notes(editTrack)) {
        if (editTrack != 3 && (n.n < G.lo || n.n > G.hi)) continue;
        const auto r = noteRect(n, G);
        g.setColour(tc.withAlpha(editTrack == 3 ? 0.35f + 0.6f * n.v : 0.55f + 0.4f * n.v)); g.fillRoundedRectangle(r, 1.5f * u);
        if (editTrack != 3) {
            g.setColour(juce::Colours::white.withAlpha(0.35f)); g.drawRoundedRectangle(r, 1.5f * u, 0.8f);
            if (n.gl) { g.setColour(juce::Colours::white.withAlpha(0.8f)); g.fillRect(r.getX(), r.getY(), 2.5f * u, r.getHeight()); }  // glissade 808
        }
    }
    g.setColour(muteC.withAlpha(0.7f)); g.setFont(fm(8.5f * u));
    g.drawText(u8("Clic : ajouter / supprimer  ·  glisser : déplacer  ·  bord droit : durée  ·  clic droit : effacer"), S(420, 481, 548, 9).toNearestInt(), juce::Justification::centredRight);

    // vue d'ensemble des 4 pistes
    auto ov = S(312, 440, 656, 39);
    g.setColour(juce::Colours::white.withAlpha(0.05f)); g.fillRect(ov.getX(), ov.getY() - 3 * u, ov.getWidth(), 1.f);
    auto laneOf = [&](std::vector<const std::vector<orbe::Note>*> srcs) {
        int lo = 200, hi = 0;
        for (auto* v : srcs) for (auto& n : *v) { lo = std::min(lo, n.n); hi = std::max(hi, n.n); }
        if (hi < lo) { lo = 40; hi = 52; }
        return std::make_pair(lo, hi);
    };
    auto drawLane = [&](Rectangle<float> lane, const std::vector<orbe::Note>& v, int lo, int hi, Colour c, float a) {
        const float ph = lane.getHeight() / (float)(hi - lo + 2);
        for (auto& n : v) g.setColour(c.withAlpha(a)), g.fillRect(Rectangle<float>(area.getX() + n.t * stepW, lane.getBottom() - (float)(n.n - lo + 1) * ph, juce::jmax(1.5f, n.d * stepW - 0.5f), juce::jmax(1.2f, ph * 0.9f)));
    };
    auto hiA = ov.withHeight(ov.getHeight() * 0.45f), bsA = ov.withTrimmedTop(ov.getHeight() * 0.47f).withHeight(ov.getHeight() * 0.25f), drA = ov.withTrimmedTop(ov.getHeight() * 0.76f);
    { auto [lo, hi] = laneOf({&song.nChords, &song.nLead}); drawLane(hiA, song.nChords, lo, hi, kLilac, 0.45f); drawLane(hiA, song.nLead, lo, hi, kIce, 0.95f); }
    { auto [lo, hi] = laneOf({&song.nBass}); drawLane(bsA, song.nBass, lo, hi, kPeach, 0.85f); }
    for (auto& n : song.nDrums) {
        const int row = rowOfDrum(n.n) == 6 ? 0 : (n.n == 38 || n.n == 39 || n.n == 37) ? 1 : 2;
        g.setColour(Colour(0xFFE9E7F0).withAlpha(0.25f + 0.6f * n.v));
        g.fillRect(Rectangle<float>(area.getX() + n.t * stepW, drA.getY() + drA.getHeight() / 3.f * (float)row, juce::jmax(1.5f, stepW * 0.7f), drA.getHeight() / 3.f - 0.5f));
    }

    // tête de lecture
    const float step = proc.playStep.load();
    if (step >= 0.f) {
        const float x = area.getX() + step * stepW;
        const float y0 = area.getY(), hh = ov.getBottom() - y0;
        g.setGradientFill(juce::ColourGradient(juce::Colours::white.withAlpha(0.0f), x - 10 * u, 0, juce::Colours::white.withAlpha(0.18f), x, 0, false));
        g.fillRect(x - 10 * u, y0, 10 * u, hh);
        g.setColour(juce::Colours::white.withAlpha(0.9f)); g.fillRect(x, y0, 1.4f, hh);
    }
}

// ============================================================ v3 « hybride v2 » (mêmes coordonnées de conception 1000 x 640)
void OrbeEditor::resized3() {
    skin.themeBtn.setBounds(Si(944, 24, 28, 28));
    styleBox.setBounds(Si(28, 102, 252, 32));
    keyBox.setBounds(Si(28, 158, 106, 32));
    scaleBox.setBounds(Si(144, 158, 136, 32));
    barsSeg->setBounds(Si(28, 214, 76, 32));
    arpSeg->setBounds(Si(112, 214, 168, 32));
    b7.setBounds(Si(28, 264, 70, 24)); bAdd9.setBounds(Si(100, 264, 84, 24)); bBorrow.setBounds(Si(180, 264, 104, 24));
    for (int i = 0; i < 2; ++i) { k3[i]->setScale(u_); k3[i]->setBounds(Si(34.f + 134.f * (float)i, 346, 108, 112)); }
    auditionB.setBounds(Si(24, 520, 124, 40)); genAll.setBounds(Si(160, 520, 124, 40));
    genChords.setBounds(Si(24, 570, 124, 36)); genLead.setBounds(Si(160, 570, 124, 36));
    for (int i = 0; i < 4; ++i) {
        const float x = 324.f + (float)i * 84.f;
        fader[i].setBounds(Si(x + 16, 534, 40, 68));
        mute[i].setBounds(Si(x + 2, 606, 70, 22));
    }
    k3[2]->setScale(u_); k3[2]->setBounds(Si(660, 516, 100, 112));
    handle->setBounds(Si(768, 508, 208, 70));
    for (int i = 0; i < 4; ++i) chip[i].setBounds(Si(768.f + (float)i * 52.f, 606.f, 48, 20));
    midiOut.setBounds(Si(768, 582, 170, 22));
    repaint();
}

void OrbeEditor::paintBackdrop3(juce::Graphics& g, Rectangle<float> b) {
    const float u = b.getWidth() / 1000.f;
    auto R = [u](float x, float y, float w, float h) { return Rectangle<float>(x * u, y * u, w * u, h * u); };
    const auto& T = skin.T;
    v3::background(g, T, b);
    v3::card(g, T, R(16, 80, 276, 414), 18.f * u);
    v3::hero(g, T, R(304, 240, 672, 250), 18.f * u);
    v3::card(g, T, R(16, 504, 960, 128), 18.f * u);
    v3::hero(g, T, R(590, 14, 340, 44), 14.f * u);
    v3::label(g, T, "Style", R(28, 84, 200, 14), 11.f * u);
    v3::label(g, T, "Tonique", R(28, 140, 108, 14), 11.f * u);
    v3::label(g, T, "Gamme", R(146, 140, 140, 14), 11.f * u);
    v3::label(g, T, "Mesures", R(28, 196, 76, 14), 11.f * u);
    v3::label(g, T, "Jeu des accords", R(114, 196, 172, 14), 11.f * u);
    v3::label(g, T, "Harmonie", R(28, 246, 140, 14), 11.f * u);
    v3::label(g, T, u8("Mélodie"), R(28, 304, 260, 14), 11.f * u);
    g.setColour(T.dark ? juce::Colours::white.withAlpha(0.07f) : Colour(0x14101828)); g.fillRect(R(28, 296, 252, 1));
    const char* tn[4] = {"Accords", "Lead", "Basse", "Batterie"};
    const Colour tc[4] = {T.accent, T.mod, T.out, T.inkMute};
    for (int i = 0; i < 4; ++i) {
        const float x = 324.f + (float)i * 84.f;
        g.setColour(kTrackCol[i].darker(T.dark ? 0.f : 0.25f)); g.fillRoundedRectangle(R(x + 6, 520, 8, 8), 4.f * u);
        v3::label(g, T, tn[i], R(x + 18, 516, 70, 16), 10.f * u);
        juce::ignoreUnused(tc);
    }
}

void OrbeEditor::paint3(juce::Graphics& g) {
    const float u = u_;
    skin.art.draw(g, String("bd|") + v3::modeKey(skin.mode), getLocalBounds().toFloat(), [this](juce::Graphics& ig, Rectangle<float> b) { paintBackdrop3(ig, b); });
    paintHeader3(g, u);
    paintCards3(g, u);
    paintToolbar3(g, u);
    paintRoll(g, u);
}

void OrbeEditor::paintHeader3(juce::Graphics& g, float u) {
    const auto& T = skin.T;
    const float cx = 40 * u, cy = 38 * u, r = 15 * u;   // l'orbe, signature de la marque
    g.setGradientFill(juce::ColourGradient(T.mod.brighter(0.4f), cx - r * 0.5f, cy - r * 0.6f, Colour(0xFF5A46C8), cx + r, cy + r, true)); g.fillEllipse(cx - r, cy - r, r * 2, r * 2);
    g.setColour(juce::Colours::white.withAlpha(0.5f)); g.fillEllipse(cx - r * 0.62f, cy - r * 0.78f, r * 0.7f, r * 0.42f);
    g.setColour(T.accent.withAlpha(0.8f)); g.drawEllipse(cx - r * 1.28f, cy - r * 1.28f, r * 2.56f, r * 2.56f, 1.f * u);
    v3::brand(g, T, u, "Orbe", "", 72.f);
    g.setColour(T.inkMute); g.setFont(v3::font(12.f * u)); g.drawText(u8("Accords · mélodie · basse · batterie"), S(250, 28, 320, 20), juce::Justification::centredLeft, false);
    const auto& st = proc.style(); const auto& ap = proc.apvts;
    const int key = (int)ap.getRawParameterValue("key")->load(), sc = (int)ap.getRawParameterValue("scale")->load();
    const float hb = proc.hostBpm.load(); const int bpm = hb > 0.f ? juce::roundToInt(hb) : st.bpm;
    g.setColour(T.scrInk); g.setFont(v3::font(14.f * u, true));
    g.drawText(String(st.label), S(606, 14, 150, 44), juce::Justification::centredLeft, true);
    v3::tabular(g, v3::fmt::withUnit(String(bpm), "BPM"), S(748, 14, 80, 44), v3::font(14.f * u, true), juce::Justification::centredRight);
    g.setColour(T.out); g.drawText(String(kSharpN[key]) + " " + String(orbe::scales()[(size_t)sc].label), S(836, 14, 90, 44), juce::Justification::centredRight, true);
}

void OrbeEditor::paintCards3(juce::Graphics& g, float u) {
    const auto& T = skin.T; const auto& song = proc.song();
    const int n = (int)song.chords.size();
    const float step = proc.playStep.load();
    const int cur = step >= 0.f ? (int)(step / 16.f) : -1;
    for (int i = 0; i < n; ++i) {
        const auto& c = song.chords[(size_t)i];
        const orbe::Chord* c2 = nullptr;
        for (auto& s : song.slots) if (s.bar == i && s.t > (float)i * 16) c2 = &s;
        auto r = cardRect(i, n, u);
        const bool playing = i == cur, locked = proc.isLocked(i);
        const float rad = 14.f * u;
        juce::DropShadow(T.shadow.withMultipliedAlpha(0.5f), juce::roundToInt(6 * u), {0, 2}).drawForRectangle(g, r.reduced(2.f).toNearestInt());
        g.setColour(playing ? (T.dark ? T.card.brighter(0.25f) : T.card) : T.card); g.fillRoundedRectangle(r, rad);
        g.setColour(playing ? T.accent : T.cardEdge); g.drawRoundedRectangle(r.reduced(0.5f), rad, playing ? 2.f : 1.f);
        auto one = [&](const orbe::Chord& ch, Rectangle<float> rr, bool half) {
            g.setColour(T.accentText); g.setFont(v3::font(juce::jmin(12.f * u, rr.getWidth() * 0.18f), true));
            g.drawText(ch.roman, Rectangle<float>(rr.getX() + 10 * u, rr.getY() + 7 * u, rr.getWidth() - 20 * u, 15 * u), juce::Justification::centredLeft, false);
            if (ch.borrowed && !half) { g.setColour(T.inkMute); v3::caps(g, "emprunt", Rectangle<float>(rr.getX() + 8 * u, rr.getY() + 7 * u, rr.getWidth() - 16 * u, 15 * u), 9.f * u, juce::Justification::centredRight); }
            g.setColour(T.ink); g.setFont(v3::font(juce::jmin(26.f * u, rr.getWidth() * 0.32f), true));
            g.drawFittedText(ch.name, Rectangle<float>(rr.getX() + 2 * u, rr.getY() + 22 * u, rr.getWidth() - 4 * u, 32 * u).toNearestInt(), juce::Justification::centred, 1, 0.5f);
            String ns; for (size_t k = 0; k < ch.voicing.size(); ++k) ns << (k ? " " : "") << noteName(ch.voicing[k]);
            g.setColour(T.inkMute); g.setFont(v3::font(juce::jmin(11.5f * u, rr.getWidth() * (half ? 0.17f : 0.09f))));
            g.drawFittedText(ns, Rectangle<float>(rr.getX() + 4 * u, rr.getY() + 55 * u, rr.getWidth() - 8 * u, 22 * u).toNearestInt(), juce::Justification::centred, 2);
        };
        if (c2) {
            auto left = r.withWidth(r.getWidth() / 2), right = r.withTrimmedLeft(r.getWidth() / 2);
            one(c, left, true); one(*c2, right, true);
            g.setColour(T.fieldEdge); g.fillRect(r.getCentreX() - 0.5f, r.getY() + 10 * u, 1.f, 62 * u);
        } else one(c, r, false);
        const float s = 22.f * u;
        auto lk = Rectangle<float>(r.getX() + 8 * u, r.getBottom() - s - 3 * u, s, s);
        g.setColour(locked ? T.accentText : T.inkMute);
        juce::Path body; body.addRoundedRectangle(lk.getX() + s * 0.2f, lk.getY() + s * 0.46f, s * 0.6f, s * 0.42f, 2.f);
        g.fillPath(body);
        juce::Path sh;
        if (locked) sh.addCentredArc(lk.getCentreX(), lk.getY() + s * 0.46f, s * 0.2f, s * 0.26f, 0, -juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi, true);
        else sh.addCentredArc(lk.getCentreX() + s * 0.1f, lk.getY() + s * 0.42f, s * 0.2f, s * 0.26f, 0, -juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi * 0.6f, true);
        g.strokePath(sh, juce::PathStrokeType(1.6f * u));
        auto rl = Rectangle<float>(r.getRight() - s - 8 * u, r.getBottom() - s - 3 * u, s, s);
        g.setColour(T.inkMute);
        juce::Path ar; ar.addCentredArc(rl.getCentreX(), rl.getCentreY(), s * 0.3f, s * 0.3f, 0, 0.6f, 5.2f, true);
        g.strokePath(ar, juce::PathStrokeType(1.6f * u));
        juce::Path tip; tip.addTriangle(rl.getCentreX() + s * 0.3f, rl.getCentreY() - s * 0.02f, rl.getCentreX() + s * 0.46f, rl.getCentreY() - s * 0.2f, rl.getCentreX() + s * 0.14f, rl.getCentreY() - s * 0.22f);
        g.fillPath(tip);
    }
}

void OrbeEditor::paintToolbar3(juce::Graphics& g, float u) {
    const auto& T = skin.T;
    const char* tn[4] = {"Accords", "Lead", "Basse", "Batterie"};
    const char* tool_[2] = {"Crayon", "Gomme"};
    for (auto& b : buttons()) {
        auto r = b.r.reduced(0.5f);
        const bool sel = (b.kind == 0 && editTrack == b.idx) || (b.kind == 2 && tool == b.idx);
        const bool dis = (b.kind == 3 && !proc.canUndo()) || (b.kind == 4 && !proc.canRedo());
        const float rad = r.getHeight() * 0.5f;
        g.setColour(sel ? (T.dark ? T.card.brighter(0.25f) : T.card) : T.field); g.fillRoundedRectangle(r, rad);
        g.setColour(sel ? T.accent : T.fieldEdge); g.drawRoundedRectangle(r.reduced(0.5f), rad, sel ? 1.5f : 1.f);
        if (b.kind == 1) {
            const bool lk = proc.trackLocked(b.idx);
            const float s = r.getHeight() * 0.8f;
            auto m = Rectangle<float>(r.getCentreX() - s * 0.3f, r.getCentreY() - s * 0.02f, s * 0.6f, s * 0.42f);
            g.setColour(lk ? T.accentText : T.inkMute); g.fillRoundedRectangle(m, 1.5f * u);
            juce::Path sh; sh.addCentredArc(m.getCentreX() + (lk ? 0.f : s * 0.14f), m.getY(), s * 0.17f, s * 0.24f, 0, -juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi, true);
            g.strokePath(sh, juce::PathStrokeType(1.5f * u));
            continue;
        }
        String txt = b.kind == 0 ? String(tn[b.idx]) + (proc.trackEdited(b.idx) ? " *" : "") : b.kind == 2 ? String(tool_[b.idx]) : b.kind == 3 ? String(u8("Annuler")) : String(u8("Rétablir"));
        g.setColour(dis ? T.inkMute.withAlpha(0.5f) : sel ? T.ink : T.inkMute);
        v3::caps(g, txt, r, 10.f * u, juce::Justification::centred, sel);
    }
}
