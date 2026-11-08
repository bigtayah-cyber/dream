#include "LooksEditor.h"

using namespace kit;
using juce::Colour, juce::Rectangle, juce::String;
using oc::ui::kLilac, oc::ui::kIce, oc::ui::kPeach;

namespace {
constexpr float kW = 460.f, kH = 610.f;
struct MacroDef { const char* id; const char* label; const char* tip; };
const MacroDef kMacros[4] = {
    {"int", "Intensité", "Profondeur du look : force de la correction, de la compression et des doubles."},
    {"esp", "Espace", "Quantité et longueur des échos et de la réverbération."},
    {"mor", "Mordant", "Grain : saturation, mordant et dégradation du signal."},
    {"cou", "Couleur", "Teinte : de sombre et feutré à clair et brillant."},
};
const char* const kModNames[8] = {"EQ", "COMP", "SAT", "TUNE", "DESS", "DBL", "DLY", "REV"};
const char kModKeys[8] = {'E', 'C', 'S', 'T', 'D', '2', 'Y', 'R'};
}  // namespace

LooksEditor::LooksEditor(LooksProcessor& p) : AudioProcessorEditor(&p), proc(p), pal(oc::ui::palette()), look(pal) {
    setLookAndFeel(&look);
    const float R = 24.f;
    for (auto& d : kMacros) {
        const String id(d.id);
        auto* pot = pots.add(new oc::ui::Pot(proc.apvts, id, u8(d.label), u8(d.tip), [this, id] { return LooksProcessor::macroDefault(id, proc.currentLook()); }, R));
        addAndMakeVisible(pot);
    }
    for (auto* d : {"mix", "out"}) {
        const String id(d); const bool isMix = id == "mix";
        auto* pot = pots.add(new oc::ui::Pot(proc.apvts, id, isMix ? "Mix" : "Sortie", isMix ? u8("Sec / traité : 100 % = voix entièrement traitée.") : u8("Niveau de sortie, avant le limiteur."),
                                             [isMix] { return isMix ? 1.0 : 0.0; }, 19.f, kPeach));
        addAndMakeVisible(pot);
    }
    keyBox.addItem("Auto", 1); for (int i = 0; i < 12; ++i) keyBox.addItem(oc::ui::kNotes[i], i + 2);
    scaleBox.addItem(u8("Majeur"), 1); scaleBox.addItem(u8("Mineur"), 2);
    keyBox.setTooltip(u8("Tonalité de la correction de hauteur. Auto : détection prudente, aucune correction tant qu'elle n'est pas sûre."));
    scaleBox.setTooltip(u8("Mode utilisé quand la tonalité est choisie à la main."));
    addAndMakeVisible(keyBox); addAndMakeVisible(scaleBox);
    keyAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(proc.apvts, "key", keyBox);
    scaleAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(proc.apvts, "scale", scaleBox);
    prev.onClick = [this] { step(-1); }; next.onClick = [this] { step(1); };
    prev.setTooltip(u8("Look précédent")); next.setTooltip("Look suivant");
    addAndMakeVisible(prev); addAndMakeVisible(next); addAndMakeVisible(power);
    power.setTooltip(u8("Effet en service. Appuie pour le couper (la latence reste compensée)."));
    powerAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, "bypass", power);
    setResizable(true, true);
    setResizeLimits(322, 468, 920, 1336);
    getConstrainer()->setFixedAspectRatio((double)kW / kH);
    setSize((int)kW, (int)kH);
    startTimerHz(30);
}
LooksEditor::~LooksEditor() { stopTimer(); setLookAndFeel(nullptr); }

void LooksEditor::resized() {
    u_ = (float)getWidth() / kW; look.k = u_;
    plate = brushedPlate(getWidth(), getHeight(), pal);
    for (int i = 0; i < 4; ++i) { pots[i]->setScale(u_); pots[i]->setBounds(Si(28 + 101 * (float)i, 258, 101, 112)); }
    pots[4]->setScale(u_); pots[4]->setBounds(Si(28, 488, 96, 96));
    pots[5]->setScale(u_); pots[5]->setBounds(Si(336, 488, 96, 96));
    prev.setBounds(Si(36, 126, 20, 26)); next.setBounds(Si(404, 126, 20, 26));
    keyBox.setBounds(Si(28, 398, 100, 30)); scaleBox.setBounds(Si(136, 398, 100, 30));
    power.setBounds(Si(374, 20, 58, 52));
}

void LooksEditor::timerCallback() {
    const float pk = proc.host.peak.load(), gr = proc.host.reduction.load(), sh = proc.host.shift.load();
    const float db = 20.f * std::log10(juce::jmax(1e-5f, pk));
    outDb = juce::jmax(db, outDb - 1.0f);
    grDisp = juce::jmax(gr, grDisp - 0.5f);
    shiftDisp += (sh - shiftDisp) * 0.4f;
    moved = proc.macrosMoved();
    repaint(S(24, 90, 412, 150).toNearestInt()); repaint(S(24, 380, 412, 70).toNearestInt()); repaint(S(130, 470, 200, 130).toNearestInt());
}

void LooksEditor::mouseUp(const juce::MouseEvent& e) {
    if (S(66, 112, 328, 80).contains(e.position) && !e.mouseWasDraggedSinceMouseDown()) showMenu();
}
void LooksEditor::showMenu() {
    juce::PopupMenu m;
    for (int i = 0; i < oc::numLooks(); ++i) m.addItem(i + 1, LooksProcessor::lookName(i) + u8("  ·  ") + u8(oc::looks()[(size_t)i].tag), true, i == proc.currentLook());
    m.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(localAreaToGlobal(Si(66, 112, 328, 80))).withMinimumWidth(juce::roundToInt(328 * u_)),
                    [this](int r) { if (r > 0) proc.applyLook(r - 1); });
}

void LooksEditor::section(juce::Graphics& g, const char* title, float y) {
    engraved(g, pal, u8(title), S(28, y, 330, 12), juce::Justification::centredLeft, 8.8f * u_, 2.2f * u_, true);
    g.setColour(juce::Colours::black.withAlpha(0.5f)); g.fillRect(S(24, y + 15, 412, 1.2f));
    g.setColour(juce::Colours::white.withAlpha(0.07f)); g.fillRect(S(24, y + 16.2f, 412, 1.f));
}

void LooksEditor::paint(juce::Graphics& g) {
    const float u = u_;
    g.drawImageAt(plate, 0, 0);
    // en-tête
    engraved(g, pal, "Orbe", S(28, 22, 150, 30), juce::Justification::centredLeft, 25.f * u, 5.f * u, true);
    g.setColour(kLilac); tracked(g, "Looks", S(128, 22, 200, 30), juce::Justification::centredLeft, 5.f * u, fnt(25.f * u, true));
    engraved(g, pal, u8("Chaîne vocale à looks"), S(30, 56, 260, 12), juce::Justification::centredLeft, 8.6f * u, 2.f * u);
    for (float sx : {14.f, 446.f}) for (float sy : {14.f, 596.f}) screw(g, sx * u, sy * u, 4.6f * u, sx * 0.3f + sy);

    // afficheur du look
    const auto L = (size_t)proc.currentLook(); const auto& look_ = oc::looks()[L];
    auto scr = S(28, 94, 404, 124);
    screenWell(g, pal, scr);
    g.setColour(kIce.withAlpha(0.55f)); g.setFont(mono(10.f * u));
    g.drawText(String(proc.currentLook() + 1).paddedLeft('0', 2) + " / " + String(oc::numLooks()), S(70, 102, 100, 14).toNearestInt(), juce::Justification::centredLeft);
    if (moved) { led(g, 400 * u, 109 * u, 3.2f * u, true, kPeach); g.setColour(kPeach.withAlpha(0.8f)); g.setFont(mono(8.6f * u)); g.drawText(u8("modifié"), S(318, 102, 70, 14).toNearestInt(), juce::Justification::centredRight); }
    g.setColour(kIce); g.setFont(fnt(33.f * u, true)); g.drawText(u8(look_.name), S(60, 116, 340, 40).toNearestInt(), juce::Justification::centred);
    g.setColour(pal.ink.withAlpha(0.72f)); g.setFont(fnt(12.5f * u)); g.drawText(u8(look_.tag), S(60, 156, 340, 18).toNearestInt(), juce::Justification::centred);
    // modules actifs
    const String mods(look_.mods);
    for (int i = 0; i < 8; ++i) {
        const float x = 50.f + 43.f * (float)i; const bool on = mods.containsChar(kModKeys[i]);
        led(g, (x + 7) * u, 188 * u, 2.6f * u, on, kLilac);
        g.setColour(on ? kIce.withAlpha(0.9f) : pal.ink.withAlpha(0.28f)); tracked(g, kModNames[i], S(x - 8, 196, 30, 10), juce::Justification::centred, 0.6f * u, fnt(6.6f * u, true));
    }

    section(g, "Macros", 236);
    section(g, "Tonalité · correction de hauteur", 374);
    // lecture de la tonalité
    auto det = S(244, 398, 188, 30); screenWell(g, pal, det);
    String txt; Colour tc = kIce;
    const int kp = proc.host.keyPc.load(), sc = proc.host.keyScale.load();
    const bool corrects = mods.containsChar('T'); const bool manual = proc.apvts.getRawParameterValue("key")->load() >= 0.5f;
    if (!corrects) { txt = u8("Ce look ne corrige pas la hauteur"); tc = pal.ink.withAlpha(0.5f); }
    else if (manual) txt = u8("Tonalité choisie");
    else if (kp >= 0) txt = u8("Détectée : ") + String(oc::ui::kNotes[kp]) + (sc == 2 ? u8(" mineur") : u8(" majeur"));
    else { txt = u8("Hauteur intacte · à l'écoute…"); tc = kPeach; }
    g.setColour(tc); g.setFont(mono(9.6f * u)); g.drawText(txt, det.reduced(6 * u).toNearestInt(), juce::Justification::centred);
    g.setColour(pal.ink.withAlpha(0.5f)); g.setFont(fnt(8.8f * u));
    g.drawText(u8("Auto : aucune correction vers une note tant que la tonalité n'est pas sûre."), S(28, 436, 404, 12).toNearestInt(), juce::Justification::centredLeft);

    section(g, "Sortie", 458);
    // afficheur de niveaux
    auto mt = S(136, 496, 188, 86); screenWell(g, pal, mt);
    const float x0 = mt.getX() + 10 * u, w = mt.getWidth() - 20 * u;
    g.setFont(mono(7.8f * u));
    auto row = [&](float y, const String& lab, float v, float lo, float hi, Colour c, bool bi) {
        g.setColour(pal.ink.withAlpha(0.6f)); g.drawText(lab, Rectangle<float>(x0, mt.getY() + y * u, w, 9 * u), juce::Justification::centredLeft);
        oc::ui::bar(g, Rectangle<float>(x0, mt.getY() + (y + 10) * u, w, 7 * u), v, lo, hi, c, bi);
    };
    row(6, u8("SORTIE dB"), outDb, -48.f, 0.f, kIce, false);
    row(32, u8("RÉDUCTION dB"), grDisp, 0.f, 12.f, kPeach, false);
    row(58, u8("CORRECTION DEMI-TONS"), shiftDisp, -3.f, 3.f, kLilac, true);
}
