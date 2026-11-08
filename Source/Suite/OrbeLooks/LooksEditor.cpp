#include "LooksEditor.h"

using namespace kit;
using juce::Colour, juce::Rectangle, juce::String;
using oc::ui::kLilac, oc::ui::kIce, oc::ui::kPeach;

namespace {
constexpr float kW = 460.f, kH = 560.f;
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
        pot->setStyle(KnobStyle::Skirt); addAndMakeVisible(pot);
    }
    for (auto* d : {"mix", "out"}) {
        const String id(d); const bool isMix = id == "mix";
        auto* pot = pots.add(new oc::ui::Pot(proc.apvts, id, isMix ? "Mix" : "Sortie", isMix ? u8("Sec / traité : 100 % = voix entièrement traitée.") : u8("Niveau de sortie, avant le limiteur."),
                                             [isMix] { return isMix ? 1.0 : 0.0; }, 19.f, kIce));
        pot->setStyle(KnobStyle::Cap); addAndMakeVisible(pot);
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
    power.setTooltip(u8("Effet en service. Bascule pour le couper (la latence reste compensée)."));
    help.install(*this, "Orbe Looks", {u8("Chaîne vocale complète réglée par un « look » et quatre macros."),
        u8("- Afficheur : clic pour la liste des looks, flèches pour défiler. Les 8 voyants montrent les modules réellement en service (éteints si le module est coupé ou à zéro)."),
        u8("- Macros : Intensité, Espace, Mordant, Couleur. Double-clic : valeur d'origine du look."),
        u8("- Tonalité Auto : détection prudente ; aucune correction vers une note tant que la tonalité n'est pas sûre."),
        u8("- Sortie : Mix sec / traité et niveau avant le limiteur ; barres de sortie, de réduction et de correction (demi-tons).")});
    powerAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, "bypass", power);
    setResizable(true, true);
    setResizeLimits(322, (int)(322 * kH / kW), 920, (int)(920 * kH / kW));
    getConstrainer()->setFixedAspectRatio((double)kW / kH);
    setSize((int)kW, (int)kH);
    startTimerHz(30);
}
LooksEditor::~LooksEditor() { stopTimer(); setLookAndFeel(nullptr); }

void LooksEditor::resized() {
    u_ = (float)getWidth() / kW; look.k = u_;
    const auto content = getLocalBounds().toFloat().reduced(18.f * u_, 0);
    power.setBounds(kit::w::inSlot(content, u_).toNearestInt());
    help.layout(kit::w::helpSlot(content, u_, true).toNearestInt(), getLocalBounds());
    for (int i = 0; i < 4; ++i) { pots[i]->setScale(u_); pots[i]->setBounds(Si(28 + 101 * (float)i, 230, 101, 100)); }
    pots[4]->setScale(u_); pots[4]->setBounds(Si(28, 446, 96, 90));
    pots[5]->setScale(u_); pots[5]->setBounds(Si(336, 446, 96, 90));
    prev.setBounds(Si(30, 117, 20, 26)); next.setBounds(Si(410, 117, 20, 26));
    keyBox.setBounds(Si(28, 366, 100, 30)); scaleBox.setBounds(Si(136, 366, 100, 30));
}

void LooksEditor::timerCallback() {
    const float pk = proc.host.peak.load(), gr = proc.host.reduction.load(), sh = proc.host.shift.load();
    const float db = 20.f * std::log10(juce::jmax(1e-5f, pk));
    outDb = juce::jmax(db, outDb - 1.0f);
    grDisp = juce::jmax(gr, grDisp - 0.5f);
    shiftDisp += (sh - shiftDisp) * 0.4f;
    moved = proc.macrosMoved();
    {   // état réel des modules : interrupteur du module × dosage effectif ; tout éteint si l'effet est court-circuité
        const auto st = proc.settings();
        const bool byp = proc.apvts.getRawParameterValue("bypass")->load() > 0.5f;
        auto lv = [&](float on, float amt) { return byp || on < 0.5f || amt <= 0.001f ? 0.f : juce::jlimit(0.35f, 1.f, amt); };
        const float t[8] = {lv(st.eqOn, 1.f), lv(st.cpOn, st.cpMix), lv(st.saOn, st.saMix * juce::jmin(1.f, 0.4f + st.drive)), lv(st.tnOn, st.tnAmt),
                            lv(st.dsOn, st.dsAmt * 1.5f), lv(st.dbOn, st.dbMix * 2.5f), lv(st.dyOn, st.dyMix * 3.f), lv(st.rvOn, st.rvMix * 3.f)};
        for (int i = 0; i < 8; ++i) modLevel[i] = t[i];
    }
    repaint(S(18, 54, 424, 150).toNearestInt()); repaint(S(24, 350, 412, 60).toNearestInt()); repaint(S(130, 430, 200, 120).toNearestInt());
}

void LooksEditor::mouseUp(const juce::MouseEvent& e) {
    if (S(58, 62, 344, 110).contains(e.position) && !e.mouseWasDraggedSinceMouseDown()) showMenu();
}
void LooksEditor::showMenu() {
    juce::PopupMenu m;
    for (int i = 0; i < oc::numLooks(); ++i) m.addItem(i + 1, LooksProcessor::lookName(i) + u8("  ·  ") + u8(oc::looks()[(size_t)i].tag), true, i == proc.currentLook());
    m.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(localAreaToGlobal(Si(58, 62, 344, 110))).withMinimumWidth(juce::roundToInt(328 * u_)),
                    [this](int r) { if (r > 0) proc.applyLook(r - 1); });
}

void LooksEditor::section(juce::Graphics& g, const char* title, float y) {
    engraved(g, pal, u8(title), S(28, y, 330, 12), juce::Justification::centredLeft, 8.8f * u_, 2.2f * u_, true);
    g.setColour(juce::Colours::black.withAlpha(0.5f)); g.fillRect(S(24, y + 15, 412, 1.2f));
    g.setColour(juce::Colours::white.withAlpha(0.07f)); g.fillRect(S(24, y + 16.2f, 412, 1.f));
}

void LooksEditor::paint(juce::Graphics& g) {
    const float u = u_;
    kit::w::drawFace(g, pal, getLocalBounds(), u, {kit::w::Family::Rack, "Orbe Looks", kit::w::serial::looks});

    // afficheur du look (VFD)
    const auto L = (size_t)proc.currentLook(); const auto& look_ = oc::looks()[L];
    auto scr = S(58, 62, 344, 136);
    screenWell(g, pal, scr, Display::VFD);
    const auto vfd = displayColours(Display::VFD, pal).ink;
    g.setColour(vfd.withAlpha(0.6f)); g.setFont(mono(10.f * u));
    g.drawText(String(proc.currentLook() + 1).paddedLeft('0', 2) + " / " + String(oc::numLooks()), S(70, 68, 100, 14).toNearestInt(), juce::Justification::centredLeft);
    if (moved) { g.setColour(kPeach.withAlpha(0.9f)); g.setFont(mono(9.f * u)); g.drawText(u8("● MODIFIÉ"), S(296, 68, 96, 14).toNearestInt(), juce::Justification::centredRight); }
    screenText(g, pal, Display::VFD, u8(look_.name), S(62, 86, 336, 34), juce::Justification::centred, 24.f * u);
    g.setColour(vfd.withAlpha(0.7f)); g.setFont(lcdFont(12.f * u)); g.drawText(u8(look_.tag), S(62, 124, 336, 18).toNearestInt(), juce::Justification::centred);
    // voyants des modules : état réel de la chaîne
    for (int i = 0; i < 8; ++i) {
        const float x = 76.f + 40.f * (float)i, lvl = modLevel[i];
        led(g, (x + 8) * u, 160 * u, 3.f * u, lvl, pal.ledAmber, LedBezel::None);
        g.setColour(lvl > 0.f ? vfd.withAlpha(0.9f) : vfd.withAlpha(0.25f)); tracked(g, kModNames[i], S(x - 7, 170, 30, 11), juce::Justification::centred, 0.6f * u, fnt(8.f * u, true));
    }

    section(g, "Macros", 208);
    section(g, "Tonalité · correction de hauteur", 342);
    // lecture de la tonalité
    auto det = S(244, 366, 188, 30); screenWell(g, pal, det, Display::VFD);
    String txt; Colour tc = vfd;
    const int kp = proc.host.keyPc.load(), sc = proc.host.keyScale.load();
    const bool corrects = modLevel[3] > 0.f; const bool manual = proc.apvts.getRawParameterValue("key")->load() >= 0.5f;
    if (!corrects) { txt = u8("Correction coupée"); tc = vfd.withAlpha(0.45f); }
    else if (manual) txt = u8("Tonalité choisie");
    else if (kp >= 0) txt = u8("Détectée : ") + String(oc::ui::kNotes[kp]) + (sc == 2 ? u8(" mineur") : u8(" majeur"));
    else { txt = u8("À l'écoute…"); tc = pal.ledAmber; }
    g.setColour(tc); g.setFont(lcdFont(11.f * u)); g.drawText(txt, det.reduced(6 * u).toNearestInt(), juce::Justification::centred);

    section(g, "Sortie", 420);
    // afficheur de niveaux
    auto mt = S(136, 446, 188, 90); screenWell(g, pal, mt);
    const float x0 = mt.getX() + 10 * u, w = mt.getWidth() - 20 * u;
    g.setFont(mono(8.f * u));
    auto row = [&](float y, const String& lab, float v, float lo, float hi, Colour c, bool bi) {
        g.setColour(pal.ink.withAlpha(0.65f)); g.drawText(lab, Rectangle<float>(x0, mt.getY() + y * u, w, 10 * u), juce::Justification::centredLeft);
        oc::ui::bar(g, Rectangle<float>(x0, mt.getY() + (y + 11) * u, w, 7 * u), v, lo, hi, c, bi);
    };
    row(6, u8("SORTIE dB"), outDb, -48.f, 0.f, kIce, false);
    row(34, u8("RÉDUCTION dB"), grDisp, 0.f, 12.f, kPeach, false);
    row(62, u8("CORRECTION DEMI-TONS"), shiftDisp, -3.f, 3.f, kLilac, true);
}
