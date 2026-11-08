#include "CabineEditor.h"

using namespace kit;
using juce::Colour, juce::Rectangle, juce::String;

namespace {
constexpr float kW = 400.f, kH = 760.f, kPotW = 117.f, kPotH = 78.f, kKnobR = 19.f;
const Colour kLilac(0xFFCFC6FF), kIce(0xFFA9ECFF), kPeach(0xFFFFC4AB);
struct PotDef { const char* id; const char* label; const char* tip; int section, col, row; };
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
};
const float kSecY[3] = {268.f, 452.f, 636.f};
const char* kSecName[3] = {"Ton \xC2\xB7 filtres et saturation", "Dynamique \xC2\xB7 niveau", "Espace \xC2\xB7 doubleur et envois"};
float potX(int col) { return 24.f + 352.f / 3.f * ((float)col + 0.5f) - kPotW / 2; }
float potY(int sec, int row) { return kSecY[sec] + 20.f + kPotH * (float)row; }
}  // namespace

CabPot::CabPot(juce::AudioProcessorValueTreeState& a, const String& id, const String& lab, const String& tip, std::function<double()> reset) : label(lab) {
    par = a.getParameter(id);
    s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    s.setMouseDragSensitivity(240);
    s.setTooltip(tip + u8(" Double-clic : valeur du preset."));
    s.resetTo = std::move(reset);
    s.onValueChange = [this] { repaint(); };
    addAndMakeVisible(s);
    att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(a, id, s);
}
void CabPot::resized() {
    const float u = (float)getWidth() / kPotW;
    s.setBounds(juce::Rectangle<float>(kPotW * u / 2 - 1.5f * kKnobR * u, (30.f - 1.5f * kKnobR) * u, 3 * kKnobR * u, 3 * kKnobR * u).toNearestInt());
}
void CabPot::paint(juce::Graphics& g) {
    const float u = (float)getWidth() / kPotW;
    auto pal = Palette{};
    pal.ink = Colour(0xFFE9E6F5);
    engraved(g, pal, label, Rectangle<float>(0, 57 * u, kPotW * u, 11 * u), juce::Justification::centred, 8.6f * u, 1.3f * u, true);
    auto r = Rectangle<float>(kPotW * u / 2 - 31 * u, 68 * u, 62 * u, 11.5f * u);
    g.setColour(juce::Colours::black.withAlpha(0.42f)); g.fillRoundedRectangle(r, 2.5f * u);
    g.setColour(juce::Colours::white.withAlpha(0.05f)); g.drawRoundedRectangle(r, 2.5f * u, 1.f);
    g.setColour(kIce.withAlpha(0.92f)); g.setFont(mono(9.2f * u));
    g.drawText(par ? par->getText(par->getValue(), 0) : String(), r, juce::Justification::centred);
}

void CabPower::paintButton(juce::Graphics& g, bool hover, bool down) {
    auto r = getLocalBounds().toFloat();
    const float cx = r.getCentreX(), cy = r.getCentreY() - 5, rad = juce::jmin(r.getWidth(), r.getHeight() - 12) * 0.42f;
    const bool on = !getToggleState();  // le paramètre est « bypass »
    g.setColour(juce::Colours::black.withAlpha(0.55f)); g.fillEllipse(cx - rad - 2, cy - rad, (rad + 2) * 2, (rad + 2) * 2);
    g.setGradientFill(juce::ColourGradient(Colour(0xFF6A6A72), cx - rad * 0.6f, cy - rad, Colour(0xFF17171B), cx + rad * 0.6f, cy + rad, false));
    g.fillEllipse(cx - rad, cy - rad + (down ? 1.f : 0.f), rad * 2, rad * 2);
    g.setColour(juce::Colours::white.withAlpha(hover ? 0.3f : 0.14f)); g.drawEllipse(cx - rad, cy - rad, rad * 2, rad * 2, 1.f);
    led(g, cx, cy, rad * 0.28f, on, P.accent);
    g.setColour(P.ink.withAlpha(0.8f));
    tracked(g, "In", Rectangle<float>(0, r.getBottom() - 11, r.getWidth(), 11), juce::Justification::centred, 1.6f, fnt(juce::jmax(7.f, rad * 0.55f), true));
}

Palette CabineEditor::palette() {
    Palette p;
    p.plateTop = Colour(0xFF2E2E36); p.plateBot = Colour(0xFF16161B);
    p.ink = Colour(0xFFE9E6F5); p.accent = kLilac; p.accent2 = kIce;
    p.mute = p.ink.withAlpha(0.55f); p.line = p.ink.withAlpha(0.14f);
    p.screen = Colour(0xFF0B0B10); p.screenInk = kIce; p.brush = 0.9f;
    return p;
}

CabineEditor::CabineEditor(CabineProcessor& p)
    : AudioProcessorEditor(&p), proc(p), pal(palette()), look(pal), power(pal) {
    setLookAndFeel(&look);
    for (auto& d : kPots) {
        const juce::String id(d.id);
        auto* pot = pots.add(new CabPot(proc.apvts, id, u8(d.label), u8(d.tip), [this, id] { return (double)CabineProcessor::presetValue(id, proc.currentPreset()); }));
        addAndMakeVisible(pot);
    }
    prev.onClick = [this] { step(-1); }; next.onClick = [this] { step(1); };
    prev.setTooltip(u8("Preset pr\xC3\xA9" "c\xC3\xA9" "dent")); next.setTooltip("Preset suivant");
    addAndMakeVisible(prev); addAndMakeVisible(next);
    addAndMakeVisible(power);
    power.setTooltip(u8("Effet en service. Appuie pour le couper (la latence reste compens\xC3\xA9" "e)."));
    powerAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, "bypass", power);
    setResizable(true, true);
    setResizeLimits(300, 570, 800, 1520);
    getConstrainer()->setFixedAspectRatio((double)kW / kH);
    setSize((int)kW, (int)kH);
    startTimerHz(30);
}
CabineEditor::~CabineEditor() { stopTimer(); setLookAndFeel(nullptr); }

void CabineEditor::resized() {
    u_ = (float)getWidth() / kW;
    plate = brushedPlate(getWidth(), getHeight(), pal);
    for (int i = 0; i < pots.size(); ++i) {
        const auto& d = kPots[i];
        pots[i]->setBounds(Si(potX(d.col), potY(d.section, d.row), kPotW, kPotH));
    }
    prev.setBounds(Si(32, 112, 20, 24)); next.setBounds(Si(348, 112, 20, 24));
    power.setBounds(Si(318, 20, 58, 52));
}

void CabineEditor::timerCallback() {
    const float rms = proc.level.load(), pk = proc.peak.load(), gr = proc.reduction.load();
    const float vu = 20.f * std::log10(juce::jmax(1e-5f, rms)) + 18.f;  // 0 VU = -18 dBFS efficace
    needle += (juce::jlimit(-20.f, 3.f, vu) - needle) * (vu > needle ? 0.30f : 0.12f);
    pkDisp = juce::jmax(20.f * std::log10(juce::jmax(1e-5f, pk)), pkDisp - 1.2f);
    grDisp = juce::jmax(gr, grDisp - 0.5f);
    modified = !proc.matchesPreset();
    repaint(S(24, 90, 352, 180).toNearestInt());
}

void CabineEditor::mouseUp(const juce::MouseEvent& e) {
    if (S(66, 100, 268, 48).contains(e.position) && !e.mouseWasDraggedSinceMouseDown()) showMenu();
}
void CabineEditor::showMenu() {
    juce::PopupMenu m;
    for (int i = 0; i < cab::kPresets; ++i) m.addItem(i + 1, CabineProcessor::presetName(i), true, i == proc.currentPreset());
    m.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(localAreaToGlobal(Si(66, 100, 268, 48))).withMinimumWidth(juce::roundToInt(268 * u_)),
                    [this](int r) { if (r > 0) proc.applyPreset(r - 1); });
}

void CabineEditor::section(juce::Graphics& g, const char* title, float y) {
    const float u = u_;
    engraved(g, pal, u8(title), S(28, y, 330, 12), juce::Justification::centredLeft, 8.8f * u, 2.2f * u, true);
    g.setColour(juce::Colours::black.withAlpha(0.5f)); g.fillRect(S(24, y + 15, 352, 1.2f));
    g.setColour(juce::Colours::white.withAlpha(0.07f)); g.fillRect(S(24, y + 16.2f, 352, 1.f));
}

void CabineEditor::paintVu(juce::Graphics& g, Rectangle<float> r) {
    const float u = u_;
    screenWell(g, pal, r);
    auto f = r.reduced(5 * u);
    g.setGradientFill(juce::ColourGradient(Colour(0xFFFFE3D6), f.getX(), f.getY(), Colour(0xFFE9AE96), f.getX(), f.getBottom(), false));
    g.fillRoundedRectangle(f, 3 * u);
    g.setGradientFill(juce::ColourGradient(juce::Colours::transparentBlack, f.getCentreX(), f.getCentreY(), juce::Colours::black.withAlpha(0.30f), f.getX(), f.getY(), true));
    g.fillRoundedRectangle(f, 3 * u);
    const float px = f.getCentreX(), py = f.getBottom() + 26 * u, R = 98 * u;
    auto ang = [&](float vu) { return juce::jmap(vu, -20.f, 3.f, -0.78f, 0.78f); };
    auto pt = [&](float vu, float rad) { const float a = ang(vu); return juce::Point<float>(px + std::sin(a) * rad, py - std::cos(a) * rad); };
    const Colour ink(0xFF211B1E), red(0xFFB0302E);
    juce::Path rp; rp.addCentredArc(px, py, R, R, 0, ang(0.f), ang(3.f), true);
    g.setColour(red); g.strokePath(rp, juce::PathStrokeType(3.f * u));
    juce::Path bp; bp.addCentredArc(px, py, R, R, 0, ang(-20.f), ang(0.f), true);
    g.setColour(ink); g.strokePath(bp, juce::PathStrokeType(1.1f * u));
    for (int v : {-20, -10, -7, -5, -3, -2, -1, 0, 1, 2, 3}) {
        const bool major = v == -20 || v == -10 || v == -5 || v == 0 || v == 3;
        g.setColour(v >= 0 ? red : ink);
        g.drawLine({pt((float)v, R - (major ? 8.f : 4.f) * u), pt((float)v, R + 1 * u)}, major ? 1.3f : 0.9f);
        if (major) { g.setFont(fnt(8.6f * u, true)); auto q = pt((float)v, R - 17 * u); g.drawText(String(std::abs(v)), Rectangle<float>(q.x - 9 * u, q.y - 6 * u, 18 * u, 12 * u), juce::Justification::centred); }
    }
    g.setColour(ink); g.setFont(fnt(9.f * u, true));
    g.drawText("VU", Rectangle<float>(f.getCentreX() - 20 * u, f.getBottom() - 24 * u, 40 * u, 12 * u), juce::Justification::centred);
    tracked(g, "Sortie", Rectangle<float>(f.getX() + 6 * u, f.getBottom() - 15 * u, 60 * u, 10 * u), juce::Justification::centredLeft, 1.2f, fnt(6.8f * u, true));
    const auto tip = pt(needle, R + 2 * u), base = pt(needle, 14 * u);
    g.saveState(); g.reduceClipRegion(f.toNearestInt());
    g.setColour(juce::Colours::black.withAlpha(0.22f)); g.drawLine({base.translated(2 * u, 2 * u), tip.translated(2 * u, 2 * u)}, 1.5f);
    g.setColour(Colour(0xFF121110)); g.drawLine({base, tip}, 1.4f);
    g.restoreState();
    g.setGradientFill(juce::ColourGradient(juce::Colours::white.withAlpha(0.16f), f.getX(), f.getY(), juce::Colours::white.withAlpha(0.f), f.getCentreX(), f.getCentreY(), false));
    g.fillRoundedRectangle(f, 3 * u);
}

void CabineEditor::paintLadder(juce::Graphics& g, Rectangle<float> r, const char* title, const float* thr, const char* const* lab, float value, Colour hot) {
    const float u = u_;
    g.setColour(pal.mute); tracked(g, u8(title), Rectangle<float>(r.getX(), r.getY(), r.getWidth(), 10 * u), juce::Justification::centredLeft, 1.4f * u, fnt(7.2f * u, true));
    for (int i = 0; i < 8; ++i) {
        const float cx = r.getX() + (6.5f + 13.f * (float)i) * u, cy = r.getY() + 20 * u;
        led(g, cx, cy, 3.3f * u, value >= thr[i], i >= 6 ? hot : pal.accent);
        g.setColour(pal.mute.withAlpha(0.8f)); g.setFont(mono(6.3f * u));
        g.drawText(lab[i], Rectangle<float>(cx - 8 * u, cy + 6 * u, 16 * u, 8 * u), juce::Justification::centred);
    }
}

void CabineEditor::paint(juce::Graphics& g) {
    const float u = u_;
    g.drawImageAt(plate, 0, 0);
    for (float y : {14.f, 746.f}) for (float x : {14.f, 386.f}) screw(g, x * u, y * u, 5.f * u, (x + y) * 0.7f);
    g.setColour(juce::Colours::black.withAlpha(0.45f)); g.fillRect(S(0, 0, 3, kH)); g.fillRect(S(397, 0, 3, kH));
    g.setColour(juce::Colours::white.withAlpha(0.07f)); g.fillRect(S(3, 0, 1, kH)); g.fillRect(S(396, 0, 1, kH));

    // en-tête
    engraved(g, pal, "dreamaws", S(28, 24, 120, 12), juce::Justification::centredLeft, 8.8f * u, 2.6f * u);
    engraved(g, pal, "Orbe Cabine", S(28, 38, 280, 30), juce::Justification::centredLeft, 24.f * u, 3.6f * u, true);
    g.setColour(pal.mute); g.setFont(fnt(9.6f * u));
    g.drawText(u8("Cha\xC3\xAEne vocale par piste \xC2\xB7 mod\xC3\xA8le 500"), S(28, 70, 260, 13), juce::Justification::centredLeft);
    g.setFont(mono(8.4f * u)); g.drawText("DW-601", S(236, 24, 70, 12), juce::Justification::centredRight);
    g.setColour(kPeach.withAlpha(0.9f)); g.fillRect(S(28, 88, 28, 1.6f)); g.setColour(kLilac.withAlpha(0.9f)); g.fillRect(S(56, 88, 28, 1.6f)); g.setColour(kIce.withAlpha(0.9f)); g.fillRect(S(84, 88, 28, 1.6f));

    // afficheur de preset
    auto well = S(30, 100, 340, 48);
    screenWell(g, pal, well);
    const int pi = proc.currentPreset();
    g.setColour(pal.mute); g.setFont(mono(8.f * u));
    g.drawText(u8("PRESET ") + String(pi + 1).paddedLeft('0', 2) + "/" + String(cab::kPresets), S(62, 104, 120, 11), juce::Justification::centredLeft);
    g.setColour(modified ? kPeach : kLilac.withAlpha(0.8f)); g.setFont(mono(8.f * u));
    g.drawText(modified ? u8("\xE2\x97\x8F MODIFI\xC3\x89") : u8("\xE2\x97\x8F D'ORIGINE"), S(212, 104, 124, 11), juce::Justification::centredRight);
    g.setColour(pal.screenInk); g.setFont(mono(19.f * u));
    g.drawText(upper(CabineProcessor::presetName(pi)), S(62, 114, 276, 30), juce::Justification::centred);

    // VU + échelles à voyants
    paintVu(g, S(30, 166, 214, 90));
    auto lw = S(256, 166, 114, 90);
    screenWell(g, pal, lw);
    static const float grT[8] = {0.5f, 1.5f, 2.5f, 3.5f, 5.f, 7.f, 10.f, 14.f}, pkT[8] = {-40.f, -30.f, -20.f, -12.f, -8.f, -4.f, -1.5f, -0.2f};
    static const char* grL[8] = {"1", "2", "3", "4", "6", "8", "10", "14"};
    static const char* pkL[8] = {"-40", "-30", "-20", "-12", "-8", "-4", "-1", "0"};
    paintLadder(g, lw.reduced(5 * u, 0).withTrimmedTop(6 * u), "R\xC3\xA9" "duction dB", grT, grL, grDisp, kPeach);
    paintLadder(g, lw.reduced(5 * u, 0).withTrimmedTop(50 * u), "Cr\xC3\xAAte dBFS", pkT, pkL, pkDisp, kPeach);

    for (int s = 0; s < 3; ++s) section(g, kSecName[s], kSecY[s]);
}
