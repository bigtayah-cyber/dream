#include "CabineEditor.h"

using namespace kit;
using juce::Colour, juce::Rectangle, juce::String;

namespace v3 = kit::v3;
namespace {
constexpr float kW = 400.f, kH = 760.f;
using kit::w::kLilac, kit::w::kIce, kit::w::kPeach;
struct PotDef { const char* id; const char* label; const char* tip; int section, col, row; bool v3only = false; };
// Disposition de la face : trois sections, trois colonnes.
const PotDef kPots[] = {
    {"hp",    "Passe-haut",    "Passe-haut Butterworth 12 dB/oct : retire les graves inutiles de la voix.", 0, 0, 0},
    {"mud",   "Boue 300",      "Creux ou bosse centr\xC3\xA9 sur 300 Hz : enl\xC3\xA8ve le c\xC3\xA2" "ble et le son de bo\xC3\xAEte.", 0, 1, 0},
    {"pres",  "Pr\xC3\xA9sence 4k", "Cloche \xC3\xA0 4 kHz : fait avancer la voix devant le beat.", 0, 2, 0},
    {"air",   "Air 10k",       "Plateau aigu \xC3\xA0 10 kHz : brillance et a\xC3\xA9ration.", 0, 0, 1},
    {"lp",    "Passe-bas",     "Passe-bas optionnel. Tout \xC3\xA0 droite : coup\xC3\xA9. Utile pour les sons t\xC3\xA9l\xC3\xA9phone et lo-fi.", 0, 1, 1},
    {"sat",   "Saturation",    "Saturation tanh sur-\xC3\xA9" "chantillonn\xC3\xA9" "e 2x : chaleur et harmoniques.", 0, 2, 1},
    {"thr",   "Seuil",         "Seuil du compresseur. 0 dB avec ratio 1 : compresseur inactif.", 1, 0, 0},
    {"ratio", "Ratio",         "Taux de compression.", 1, 1, 0},
    {"atk",   "Attaque",       "Temps d'attaque du compresseur.", 1, 2, 0},
    {"rel",   "Rel\xC3\xA2" "che", "Temps de rel\xC3\xA2" "chement du compresseur.", 1, 0, 1},
    {"mk",    "Makeup",        "Gain de compensation apr\xC3\xA8s compression.", 1, 1, 1},
    {"out",   "Sortie",        "Niveau de sortie de la piste.", 1, 2, 1},
    {"dbl",   "Doubleur",      "Deux voix modul\xC3\xA9" "es (13 et 21 ms), ouvertes en st\xC3\xA9r\xC3\xA9o.", 2, 0, 0},
    {"rev",   "Reverb",        "Envoi vers la reverb \xC3\xA0 r\xC3\xA9seau de retards.", 2, 1, 0},
    {"dly",   "Delay",         "Envoi vers le delay en croche point\xC3\xA9" "e, synchronis\xC3\xA9 au tempo de l'h\xC3\xB4te.", 2, 2, 0},
    {"ingain", "Entr\xC3\xA9" "e",  "Gain d'entr\xC3\xA9" "e avant la cha\xC3\xAEne : cale la voix vers -12 dBFS pour que le seuil du compresseur morde.", 0, 0, 0, true},
    {"deess",  "Sifflantes", "De-esser relatif en t\xC3\xAAte de cha\xC3\xAEne (6 kHz) : r\xC3\xA9" "duit les S sans d\xC3\xA9pendre du niveau. 0 : coup\xC3\xA9.", 0, 1, 0, true},
};
const float kModY[3] = {246.f, 414.f, 582.f}, kModH = 162.f;
const char* kSecName[3] = {"Module EQ \xC2\xB7 ton et saturation", "Module FET \xC2\xB7 dynamique", "Module espace \xC2\xB7 doubleur et envois"};
const kit::KnobStyle kSecKnob[3] = {kit::KnobStyle::Cap, kit::KnobStyle::Fluted, kit::KnobStyle::Chicken};
const Colour kSecTint[3] = {Colour(0xFF3A4350), Colour(0xFF1C1C1F), Colour(0xFF2E2B2A)};
constexpr float kPotR = 16.5f, kPotW = 110.f;
float potX(int col) { return 22.f + 356.f / 3.f * ((float)col + 0.5f) - kPotW / 2; }
float potY(int sec, int row) { return kModY[sec] + (sec == 2 ? 50.f : 18.f) + 68.f * (float)row; }
}  // namespace

Palette CabineEditor::palette() { return kit::w::rack(Colour(0xFFFFB04A)); }

CabineEditor::CabineEditor(CabineProcessor& p)
    : AudioProcessorEditor(&p), proc(p), pal(palette()), look(pal), power(pal) {
    setLookAndFeel(&look);
    const Colour capCol[3] = {Colour(0xFF6FA8DC), Colour(0xFFE9E6F5), Colour(0xFFFFB04A)};
    for (auto& d : kPots) {
        const juce::String id(d.id);
        auto* pot = pots.add(new kit::w::Pot(proc.apvts, id, u8(d.label), u8(d.tip), [this, id] { return (double)CabineProcessor::presetValue(id, proc.currentPreset()); }, kPotR, capCol[d.section]));
        pot->setStyle(kSecKnob[d.section]);
        addAndMakeVisible(pot);
    }
    prev.onClick = [this] { step(-1); }; next.onClick = [this] { step(1); };
    prev.setTooltip(u8("Preset pr\xC3\xA9" "c\xC3\xA9" "dent")); next.setTooltip("Preset suivant");
    addAndMakeVisible(prev); addAndMakeVisible(next);
    cPrev.onClick = prev.onClick; cNext.onClick = next.onClick; cPrev.setTooltip(prev.getTooltip()); cNext.setTooltip(next.getTooltip());
    addChildComponent(cPrev); addChildComponent(cNext);
    addAndMakeVisible(power);
    power.setTooltip(u8("Effet en service. Bascule pour le couper (la latence reste compens\xC3\xA9" "e)."));
    powerAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, "bypass", power);
    help.install(*this, "Orbe Cabine", {u8("Cha\xC3\xAEne vocale par piste, en trois modules : EQ et saturation, compresseur, espace."),
        u8("- Afficheur : clic pour la liste des presets, fl\xC3\xA8" "ches pour d\xC3\xA9" "filer. \xE2\x97\x8F MODIFI\xC3\x89 : un r\xC3\xA9glage diff\xC3\xA8re du preset."),
        u8("- Double-clic sur un potard : retour \xC3\xA0 la valeur du preset."),
        u8("- VU : niveau de sortie (0 VU = -18 dBFS efficace). \xC3\x80 droite : r\xC3\xA9" "duction du compresseur (dB) et cr\xC3\xAAte de sortie (dBFS)."),
        u8("- Passe-bas tout \xC3\xA0 droite : coup\xC3\xA9. Seuil 0 dB et ratio 1 : compresseur inactif. Delay synchronis\xC3\xA9 au tempo de l'h\xC3\xB4te.")});
    // --- v3 : potards haut de gamme (mêmes paramètres), sélecteur de thème
    for (auto& d : kPots) {
        const juce::String id(d.id);
        auto* k = k3.add(new v3::rk::RefKnob(proc.apvts, id, u8(d.label), u8(d.tip), v3::KnobSize::S));
        k->resetTo = [this, id] { return (double)CabineProcessor::presetValue(id, proc.currentPreset()); };
        k->setBipolar(v3::rk::bipolarOf(proc.apvts, id)); k->setLabelPx(11.f);
        addChildComponent(k);
    }
    addAndMakeVisible(skin.btn);
    skin.btn.onTheme = [this](v3::Mode m) { setThemeMode(m, true); };
    skin.btn.onScale = [this](float s) { setUiScale(s); };
    setResizable(true, true);
    scale_ = v3::Prefs::scale("cabine", 1.f);
    setThemeMode(v3::Prefs::theme("cabine", v3::Mode::Light), false);
    startTimerHz(30);
}
CabineEditor::~CabineEditor() { stopTimer(); v3::Prefs::setScale("cabine", scale_); setLookAndFeel(nullptr); }

void CabineEditor::setThemeMode(v3::Mode m, bool persist) {
    skin.set(m);
    const bool n3 = isV3();
    setLookAndFeel(n3 ? static_cast<juce::LookAndFeel*>(&skin.look3) : static_cast<juce::LookAndFeel*>(&look));
    for (int i = 0; i < pots.size(); ++i) pots[i]->setVisible(!n3 && !kPots[i].v3only);
    for (auto* k : k3) { k->setVisible(n3); k->setup(&skin.T, &skin.art); }
    prev.setVisible(!n3); next.setVisible(!n3);
    const v3::Theme* tp = skin.tp();
    cPrev.T = cNext.T = tp; cPrev.setVisible(n3); cNext.setVisible(n3);
    power.T = tp; help.T = tp;
    if (persist) v3::Prefs::setTheme("cabine", m);
    if (v3::rk::fit(*this, n3, {kW3, kH3}, {kW, kH}, 0.75f, 2.f, scale_)) resized();
    sendLookAndFeelChange();
    repaint();
}

void CabineEditor::setUiScale(float s) {
    const bool n3 = isV3();
    scale_ = juce::jlimit(n3 ? 0.75f : 0.75f, n3 ? 1.5f : 2.f, s);
    setSize(juce::roundToInt((n3 ? kW3 : kW) * scale_), juce::roundToInt((n3 ? kH3 : kH) * scale_));
    v3::Prefs::setScale("cabine", scale_);
}

void CabineEditor::resized() {
    if (isV3()) resized3(); else resizedClassic();
    skin.btn.scale = scale_;
}

void CabineEditor::resizedClassic() {
    u_ = (float)getWidth() / kW; scale_ = u_;
    const auto content = getLocalBounds().toFloat().reduced(18.f * u_, 0);
    power.setBounds(kit::w::inSlot(content, u_).toNearestInt());
    help.layout(kit::w::helpSlot(content, u_, true).toNearestInt(), getLocalBounds());
    for (int i = 0; i < pots.size(); ++i) {
        const auto& d = kPots[i];
        pots[i]->setBounds(Si(potX(d.col), potY(d.section, d.row), kPotW, kit::w::Pot::heightFor(kPotR)));
        pots[i]->setScale(u_);
    }
    prev.setBounds(Si(24, 70, 18, 24)); next.setBounds(Si(358, 70, 18, 24));
}

void CabineEditor::timerCallback() {
    const float rms = proc.level.load(), pk = proc.peak.load(), gr = proc.reduction.load();
    vu.step(rms, 1.0 / 30.0);
    pkDisp = juce::jmax(20.f * std::log10(juce::jmax(1e-5f, pk)), pkDisp - 1.2f);
    grDisp = juce::jmax(gr, grDisp - 0.5f);
    modified = !proc.matchesPreset();
    if (isV3()) { repaint(S(20, 84, 680, 200).toNearestInt()); return; }
    repaint(S(18, 54, 364, 190).toNearestInt());
}

void CabineEditor::mouseUp(const juce::MouseEvent& e) {
    if (presetArea().contains(e.position) && !e.mouseWasDraggedSinceMouseDown()) showMenu();
}
juce::Rectangle<float> CabineEditor::presetArea() const { return isV3() ? S(66, 96, 320, 70) : S(46, 58, 308, 44); }
void CabineEditor::showMenu() {
    juce::PopupMenu m;
    for (int i = 0; i < cab::kPresets; ++i) m.addItem(i + 1, CabineProcessor::presetName(i), true, i == proc.currentPreset());
    m.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(localAreaToGlobal(presetArea().toNearestInt())).withMinimumWidth(juce::roundToInt(presetArea().getWidth())),
                    [this](int r) { if (r > 0) proc.applyPreset(r - 1); });
}

// Module « série 500 » vissé dans le châssis : plaque teintée, chanfrein, deux vis, nom sérigraphié.
void CabineEditor::module(juce::Graphics& g, int i, float y) {
    const float u = u_;
    auto r = S(22, y, 356, kModH);
    g.setColour(juce::Colours::black.withAlpha(0.55f)); g.fillRoundedRectangle(r.translated(0, 2.f * u).expanded(1.f), 3.f * u);
    g.setGradientFill(juce::ColourGradient(kSecTint[i].brighter(0.12f), r.getX(), r.getY(), kSecTint[i].darker(0.35f), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle(r, 3.f * u);
    g.setColour(juce::Colours::white.withAlpha(0.13f)); g.drawLine(r.getX() + 3 * u, r.getY() + 0.8f, r.getRight() - 3 * u, r.getY() + 0.8f, 1.f);
    g.setColour(juce::Colours::black.withAlpha(0.6f)); g.drawRoundedRectangle(r, 3.f * u, 1.f);
    for (float x : {r.getX() + 9.f * u, r.getRight() - 9.f * u}) screw(g, x, r.getY() + 9.f * u, 3.4f * u, x * 0.05f, ScrewHead::Slot);
    engraved(g, pal, u8(kSecName[i]), Rectangle<float>(r.getX() + 20 * u, r.getY() + 3 * u, r.getWidth() - 40 * u, 12 * u), juce::Justification::centred, 8.4f * u, 1.8f * u, true);
}

void CabineEditor::paintLadder(juce::Graphics& g, Rectangle<float> r, const char* title, const float* thr, const char* const* lab, float value, Colour hot) {
    const float u = u_;
    g.setColour(pal.mute); tracked(g, u8(title), Rectangle<float>(r.getX(), r.getY(), r.getWidth(), 10 * u), juce::Justification::centredLeft, 1.2f * u, fnt(8.f * u, true));
    for (int i = 0; i < 8; ++i) {
        const float cx = r.getX() + (6.5f + 13.8f * (float)i) * u, cy = r.getY() + 20 * u;
        led(g, cx, cy, 3.3f * u, value >= thr[i] ? 1.f : 0.f, i >= 6 ? hot : pal.ledAmber, LedBezel::None);
        g.setColour(pal.mute); g.setFont(mono(7.f * u));
        g.drawText(lab[i], Rectangle<float>(cx - 8 * u, cy + 6 * u, 16 * u, 9 * u), juce::Justification::centred);
    }
}

void CabineEditor::paint(juce::Graphics& g) {
    if (isV3()) { paint3(g); return; }
    const float u = u_;
    kit::w::drawFace(g, pal, getLocalBounds(), u, {kit::w::Family::Rack, "Orbe Cabine", kit::w::serial::cabine});

    // afficheur de preset
    auto well = S(46, 58, 308, 44);
    screenWell(g, pal, well, Display::VFD);
    const int pi = proc.currentPreset();
    g.setColour(pal.mute); g.setFont(mono(8.f * u));
    g.drawText(u8("PRESET ") + String(pi + 1).paddedLeft('0', 2) + "/" + String(cab::kPresets), S(56, 61, 120, 11), juce::Justification::centredLeft);
    g.setColour(modified ? kPeach : pal.mute); g.setFont(mono(8.f * u));
    g.drawText(modified ? u8("\xE2\x97\x8F MODIFI\xC3\x89") : u8("\xE2\x97\x8F D'ORIGINE"), S(224, 61, 124, 11), juce::Justification::centredRight);
    screenText(g, pal, Display::VFD, CabineProcessor::presetName(pi), S(52, 72, 296, 28), juce::Justification::centred, 13.f * u);

    // VU de sortie + échelles à voyants
    vuMeter(g, pal, S(22, 110, 228, 128), (float)vu.vu(), pkDisp > -0.5f);
    auto lw = S(258, 110, 120, 128);
    screenWell(g, pal, lw, Display::Classic);
    static const float grT[8] = {0.5f, 1.5f, 2.5f, 3.5f, 5.f, 7.f, 10.f, 14.f}, pkT[8] = {-40.f, -30.f, -20.f, -12.f, -8.f, -4.f, -1.5f, -0.2f};
    static const char* grL[8] = {"1", "2", "3", "4", "6", "8", "10", "14"};
    static const char* pkL[8] = {"-40", "-30", "-20", "-12", "-8", "-4", "-1", "0"};
    paintLadder(g, lw.reduced(4 * u, 0).withTrimmedTop(10 * u), "R\xC3\xA9" "duction dB", grT, grL, grDisp, pal.ledRed);
    paintLadder(g, lw.reduced(4 * u, 0).withTrimmedTop(68 * u), "Cr\xC3\xAAte dBFS", pkT, pkL, pkDisp, pal.ledRed);

    for (int i = 0; i < 3; ++i) module(g, i, kModY[i]);
}

// ============================================================ v3 « hybride v2 » : coordonnées de conception 720 x 836
namespace {
constexpr float kCardY[3] = {300.f, 472.f, 644.f}, kCardH = 160.f;
const char* kLadderTitle[2] = {"R\xC3\xA9" "duction dB", "Cr\xC3\xAAte dBFS"};
}

void CabineEditor::resized3() {
    u_ = (float)getWidth() / kW3; scale_ = u_;
    help.layout(Si(608, 34, 30, 30), getLocalBounds());
    skin.btn.setBounds(Si(574, 35, 28, 28));
    power.setBounds(Si(644, 36, 70, 26));
    cPrev.setBounds(Si(30, 108, 32, 44)); cNext.setBounds(Si(390, 108, 32, 44));
    int cnt[3] = {0, 0, 0}, tot[3] = {0, 0, 0};
    for (auto& d : kPots) ++tot[d.section];
    for (int i = 0; i < k3.size(); ++i) {
        const int sec = kPots[i].section, n = tot[sec], idx = cnt[sec]++;
        const float pitch = 640.f / (float)n, cx = 40.f + pitch * ((float)idx + 0.5f), w = std::min(100.f, pitch);
        k3[i]->setScale(u_);
        k3[i]->setBounds(Si(cx - w * 0.5f, kCardY[sec] + 34.f, w, v3::Knob3::boxHeight(v3::KnobSize::S, false)));
    }
}

void CabineEditor::paintBackdrop3(juce::Graphics& g, Rectangle<float> b) {
    const float u = b.getWidth() / kW3;
    const auto& T = skin.T;
    auto R = [u](float x, float y, float w, float h) { return Rectangle<float>(x * u, y * u, w * u, h * u); };
    v3::background(g, T, b);
    v3::rk::brand(g, T, u, "Orbe", "Cabine");
    v3::hero(g, T, R(20, 84, 680, 200), 20.f * u);
    g.setColour(juce::Colours::white.withAlpha(0.08f)); g.fillRect(R(424, 102, 1, 164));
    g.setColour(T.scrMute);
    v3::caps(g, u8(kLadderTitle[0]), R(40, 186, 120, 16), 11.f * u, juce::Justification::centredLeft);
    v3::caps(g, u8(kLadderTitle[1]), R(40, 236, 120, 16), 11.f * u, juce::Justification::centredLeft);
    for (int i = 0; i < 3; ++i) {
        v3::card(g, T, R(20, kCardY[i], 680, kCardH), 18.f * u);
        v3::rk::label(g, T.inkMute, u, u8(kSecName[i]).replace(u8("\xC2\xB7"), u8("\xC2\xB7")), 40, kCardY[i] + 14, 500);
    }
}

void CabineEditor::paintLadder3(juce::Graphics& g, Rectangle<float> r, const char* /*title*/, const float* thr, const char* const* lab, float value) {
    const float u = u_;
    const auto& T = skin.T;
    for (int i = 0; i < 8; ++i) {
        auto p = Rectangle<float>(r.getX() + (float)i * 32.f * u, r.getY(), 28.f * u, 12.f * u);
        const bool lit = value >= thr[i];
        const auto c = i >= 6 ? Colour(0xFFFF6A5A) : T.accent;
        if (lit) { g.setColour(c.withAlpha(0.25f)); g.fillRoundedRectangle(p.expanded(2.f * u), 8.f * u); }
        g.setColour(lit ? c : juce::Colours::white.withAlpha(0.09f)); g.fillRoundedRectangle(p, 6.f * u);
        g.setColour(T.scrMute); g.setFont(v3::font(10.f * u));
        g.drawText(String(lab[i]).replace("-", v3::fmt::minus()), p.withY(p.getBottom() + 3.f * u).withHeight(13.f * u).expanded(4.f * u, 0), juce::Justification::centred, false);
    }
}

void CabineEditor::paint3(juce::Graphics& g) {
    const float u = u_;
    const auto& T = skin.T;
    skin.art.draw(g, String("bd|") + v3::modeKey(skin.mode), getLocalBounds().toFloat(), [this](juce::Graphics& ig, Rectangle<float> b) { paintBackdrop3(ig, b); });
    const int pi = proc.currentPreset();
    g.setColour(T.scrMute);
    v3::caps(g, u8("Preset ") + String(pi + 1).paddedLeft('0', 2) + "/" + String(cab::kPresets), S(68, 96, 160, 16), 11.f * u, juce::Justification::centredLeft);
    g.setColour(modified ? T.out : T.scrMute);
    v3::caps(g, modified ? u8("\xE2\x97\x8F Modifi\xC3\xA9") : u8("\xE2\x97\x8F D'origine"), S(240, 96, 144, 16), 11.f * u, juce::Justification::centredRight);
    g.setColour(T.scrInk); g.setFont(v3::font(30.f * u, true));
    g.drawFittedText(CabineProcessor::presetName(pi), S(66, 118, 316, 44).toNearestInt(), juce::Justification::centred, 1, 0.7f);
    static const float grT[8] = {0.5f, 1.5f, 2.5f, 3.5f, 5.f, 7.f, 10.f, 14.f}, pkT[8] = {-40.f, -30.f, -20.f, -12.f, -8.f, -4.f, -1.5f, -0.2f};
    static const char* grL[8] = {"1", "2", "3", "4", "6", "8", "10", "14"};
    static const char* pkL[8] = {"-40", "-30", "-20", "-12", "-8", "-4", "-1", "0"};
    paintLadder3(g, S(160, 184, 260, 16), nullptr, grT, grL, grDisp);
    paintLadder3(g, S(160, 234, 260, 16), nullptr, pkT, pkL, pkDisp);
    v3::vu(g, T, S(444, 104, 240, 160), (float)vu.vu(), pkDisp > -0.5f, 10.f * u);
}
