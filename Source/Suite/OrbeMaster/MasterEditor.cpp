#include "MasterEditor.h"

using namespace kit;
using juce::Colour, juce::Rectangle, juce::String;

namespace {
constexpr float kW = 1000.f, kH = 490.f, kPotH = 78.f, kKnobR = 19.f;
const Colour kLilac(0xFFCFC6FF), kIce(0xFFA9ECFF), kPeach(0xFFFFC4AB);
struct PotDef { const char* id; const char* label; const char* tip; float x, y, w; };
const char* kEqLabel[mas::kBands] = {"Sub 45", "Basses 120", "Bas-m\xC3\xA9" "d 350", "M\xC3\xA9" "diums 1k", "Haut-m\xC3\xA9" "d 3k", "Pr\xC3\xA9sence 5k", "Air 10k"};
const char* kEqTip[mas::kBands] = {"Plateau grave \xC3\xA0 45 Hz.", "Cloche \xC3\xA0 120 Hz : corps des basses.", "Cloche \xC3\xA0 350 Hz : bas-m\xC3\xA9" "diums, zone de boue.", "Cloche \xC3\xA0 1 kHz.", "Cloche \xC3\xA0 3 kHz : mordant.", "Cloche \xC3\xA0 5 kHz : pr\xC3\xA9sence.", "Plateau aigu \xC3\xA0 10 kHz : air."};
const PotDef kOther[] = {
    {"cAmt",   "Quantit\xC3\xA9", "Quantit\xC3\xA9 de compression de bus (seuil de -4 \xC3\xA0 -30 dB). 1 \xC3\xA0 2 dB de r\xC3\xA9" "duction suffisent.", 24, 372, 62},
    {"cRatio", "Ratio",       "Taux de compression.", 86, 372, 62},
    {"cAtt",   "Attaque",     "Attaque lente : laisse passer le punch.", 148, 372, 62},
    {"cGain",  "Gain",        "Gain de compensation apr\xC3\xA8s compression.", 210, 372, 62},
    {"drive",  "Drive",       "Quantit\xC3\xA9 de saturation tanh (sur-\xC3\xA9" "chantillonn\xC3\xA9" "e 2x).", 282, 372, 66},
    {"satMix", "M\xC3\xA9lange",   "Part du signal satur\xC3\xA9 m\xC3\xA9lang\xC3\xA9" "e au signal sec.", 348, 372, 66},
    {"width",  "Largeur",     "Largeur st\xC3\xA9r\xC3\xA9o (traitement m\xC3\xA9" "dian / lat\xC3\xA9ral). 100 % : inchang\xC3\xA9" "e.", 424, 372, 62},
    {"monoHz", "Mono sous",   "Fr\xC3\xA9quence sous laquelle les basses sont remises en mono.", 486, 372, 62},
    {"lGain",  "Gain",        "Gain d'entr\xC3\xA9" "e du limiteur : il d\xC3\xA9" "cide du niveau sonore final.", 782, 258, 64.7f},
    {"ceil",   "Plafond",     "Plafond true-peak (interpolation 4x). -1 dBTP conseill\xC3\xA9 pour la diffusion.", 846.7f, 258, 64.7f},
    {"lRel",   "Rel\xC3\xA2" "che", "Temps de rel\xC3\xA2" "chement du limiteur.", 911.4f, 258, 64.7f},
};
struct TogDef { const char* id; const char* text; float x, y, w; };
const TogDef kTog[] = {
    {"eqOn", "\xC3\x89galiseur", 30, 76, 110}, {"compOn", "", 250, 346, 22}, {"satOn", "", 396, 346, 22},
    {"stOn", "", 586, 346, 22}, {"mono", "Mono", 556, 396, 54}, {"limOn", "", 954, 74, 22},
};
}  // namespace

MasPot::MasPot(juce::AudioProcessorValueTreeState& a, const String& id, const String& lab, const String& tip) : label(lab) {
    par = a.getParameter(id);
    s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    s.setMouseDragSensitivity(240);
    s.setTooltip(tip + u8(" Double-clic : valeur d'origine."));
    s.onValueChange = [this] { repaint(); };
    addAndMakeVisible(s);
    att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(a, id, s);
    s.setDoubleClickReturnValue(true, par->convertFrom0to1(par->getDefaultValue()));
}
void MasPot::resized() {
    const float u = (float)getHeight() / kPotH;
    s.setBounds(juce::Rectangle<float>((float)getWidth() / 2 - 1.5f * kKnobR * u, (30.f - 1.5f * kKnobR) * u, 3 * kKnobR * u, 3 * kKnobR * u).toNearestInt());
}
void MasPot::paint(juce::Graphics& g) {
    const float u = (float)getHeight() / kPotH, w = (float)getWidth();
    Palette pal; pal.ink = Colour(0xFFE9E6F5);
    engraved(g, pal, label, Rectangle<float>(0, 57 * u, w, 11 * u), juce::Justification::centred, 8.4f * u, 1.1f * u, true);
    auto r = Rectangle<float>(w / 2 - 28 * u, 68 * u, 56 * u, 11.5f * u);
    g.setColour(juce::Colours::black.withAlpha(0.42f)); g.fillRoundedRectangle(r, 2.5f * u);
    g.setColour(juce::Colours::white.withAlpha(0.05f)); g.drawRoundedRectangle(r, 2.5f * u, 1.f);
    g.setColour(kIce.withAlpha(0.92f)); g.setFont(mono(8.8f * u));
    g.drawText(par ? par->getText(par->getValue(), 0) : String(), r, juce::Justification::centred);
}

void MasPower::paintButton(juce::Graphics& g, bool hover, bool down) {
    auto r = getLocalBounds().toFloat();
    const float cx = r.getCentreX(), cy = r.getCentreY() - 5, rad = juce::jmin(r.getWidth(), r.getHeight() - 12) * 0.42f;
    const bool on = !getToggleState();
    g.setColour(juce::Colours::black.withAlpha(0.55f)); g.fillEllipse(cx - rad - 2, cy - rad, (rad + 2) * 2, (rad + 2) * 2);
    g.setGradientFill(juce::ColourGradient(Colour(0xFF6A6A72), cx - rad * 0.6f, cy - rad, Colour(0xFF17171B), cx + rad * 0.6f, cy + rad, false));
    g.fillEllipse(cx - rad, cy - rad + (down ? 1.f : 0.f), rad * 2, rad * 2);
    g.setColour(juce::Colours::white.withAlpha(hover ? 0.3f : 0.14f)); g.drawEllipse(cx - rad, cy - rad, rad * 2, rad * 2, 1.f);
    led(g, cx, cy, rad * 0.28f, on, P.accent);
    g.setColour(P.ink.withAlpha(0.8f));
    tracked(g, "In", Rectangle<float>(0, r.getBottom() - 11, r.getWidth(), 11), juce::Justification::centred, 1.6f, fnt(juce::jmax(7.f, rad * 0.55f), true));
}

Palette MasterEditor::palette() {
    Palette p;
    p.plateTop = Colour(0xFF2E2E36); p.plateBot = Colour(0xFF16161B);
    p.ink = Colour(0xFFE9E6F5); p.accent = kLilac; p.accent2 = kIce;
    p.mute = p.ink.withAlpha(0.55f); p.line = p.ink.withAlpha(0.14f);
    p.screen = Colour(0xFF0B0B10); p.screenInk = kIce; p.brush = 0.9f;
    return p;
}

MasterEditor::MasterEditor(MasterProcessor& p) : AudioProcessorEditor(&p), proc(p), pal(palette()), look(pal), power(pal) {
    setLookAndFeel(&look);
    for (int i = 0; i < mas::kBands; ++i) pots.add(new MasPot(proc.apvts, "eq" + String(i), u8(kEqLabel[i]), u8(kEqTip[i])));
    for (auto& d : kOther) pots.add(new MasPot(proc.apvts, d.id, u8(d.label), u8(d.tip)));
    for (auto* pt : pots) addAndMakeVisible(pt);
    for (auto& d : kTog) {
        auto* t = toggles.add(new juce::ToggleButton(u8(d.text)));
        addAndMakeVisible(t);
        toggleAtts.add(new juce::AudioProcessorValueTreeState::ButtonAttachment(proc.apvts, d.id, *t));
    }
    toggles[0]->setTooltip(u8("Active ou contourne les sept bandes de l'\xC3\xA9galiseur."));
    toggles[4]->setTooltip(u8("Remet en mono les basses du signal lat\xC3\xA9ral sous la fr\xC3\xA9quence choisie."));
    resetBtn.setButtonText(u8("Remise \xC3\xA0 z\xC3\xA9ro")); resetBtn.setTooltip(u8("Remet toute la cha\xC3\xAEne \xC3\xA0 z\xC3\xA9ro (valeurs d'origine)."));
    resetBtn.onClick = [this] { proc.resetAll(); };
    addAndMakeVisible(resetBtn); addAndMakeVisible(power);
    power.setTooltip(u8("Effet en service. Appuie pour le couper (la latence reste compens\xC3\xA9" "e)."));
    powerAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, "bypass", power);
    setResizable(true, true);
    setResizeLimits(600, 294, 1800, 882);
    getConstrainer()->setFixedAspectRatio((double)kW / kH);
    setSize((int)kW, (int)kH);
    startTimerHz(30);
}
MasterEditor::~MasterEditor() { stopTimer(); setLookAndFeel(nullptr); }

void MasterEditor::resized() {
    u_ = (float)getWidth() / kW;
    plate = brushedPlate(getWidth(), getHeight(), pal);
    for (int i = 0; i < mas::kBands; ++i) pots[i]->setBounds(Si(30.f + 81.4f * (float)i, 252, 81.4f, kPotH));
    for (int i = 0; i < (int)(sizeof(kOther) / sizeof(kOther[0])); ++i) pots[mas::kBands + i]->setBounds(Si(kOther[i].x, kOther[i].y, kOther[i].w, kPotH));
    for (int i = 0; i < toggles.size(); ++i) toggles[i]->setBounds(Si(kTog[i].x, kTog[i].y, kTog[i].w, 20));
    resetBtn.setBounds(Si(770, 26, 124, 28)); power.setBounds(Si(920, 16, 58, 52));
}

void MasterEditor::timerCallback() {
    auto db = [](float v) { return 20.f * std::log10(juce::jmax(1e-5f, v)); };
    auto fall = [](float& d, float v, float rate) { d = v > d ? v : d - rate; };
    fall(dIn, db(proc.inPk.load()), 0.8f); fall(dOut, db(proc.outPk.load()), 0.8f);
    dGc = juce::jmax(proc.grComp.load(), dGc - 0.25f); dGl = juce::jmax(proc.grLim.load(), dGl - 0.5f);
    holdOut = juce::jmax(db(proc.outPk.load()), holdOut - 0.15f);
    lufsO = proc.lufsOut.load(); lufsI = proc.lufsIn.load();
    bool eqChanged = false;
    for (int i = 0; i < mas::kBands; ++i) { const float v = proc.raw(("eq" + String(i)).toRawUTF8()) * (proc.raw("eqOn") > 0.5f ? 1.f : 0.f); if (v != eqCache[i]) { eqCache[i] = v; eqChanged = true; } }
    if (eqChanged) repaint(S(24, 90, 584, 160).toNearestInt());
    repaint(S(616, 90, 364, 372).toNearestInt());
}

void MasterEditor::module(juce::Graphics& g, const char* title, float x, float w, float y) {
    engraved(g, pal, u8(title), S(x + 6, y, w - 100, 12), juce::Justification::centredLeft, 8.8f * u_, 2.2f * u_, true);
    g.setColour(juce::Colours::black.withAlpha(0.5f)); g.fillRect(S(x, y + 18, w, 1.2f));
    g.setColour(juce::Colours::white.withAlpha(0.07f)); g.fillRect(S(x, y + 19.2f, w, 1.f));
}

void MasterEditor::paintCurve(juce::Graphics& g, Rectangle<float> r) {
    const float u = u_;
    screenWell(g, pal, r);
    auto f = r.reduced(6 * u, 8 * u);
    auto X = [&](float hz) { return f.getX() + f.getWidth() * std::log10(hz / 20.f) / 3.f; };
    auto Y = [&](float db) { return f.getCentreY() - f.getHeight() * 0.5f * db / 7.f; };
    g.setFont(mono(7.4f * u));
    for (float hz : {50.f, 100.f, 200.f, 500.f, 1000.f, 2000.f, 5000.f, 10000.f}) {
        g.setColour(juce::Colours::white.withAlpha(hz == 100.f || hz == 1000.f || hz == 10000.f ? 0.10f : 0.045f)); g.drawVerticalLine((int)X(hz), f.getY(), f.getBottom());
        if (hz == 100.f || hz == 1000.f || hz == 10000.f) { g.setColour(pal.mute); g.drawText(hz >= 1000.f ? String((int)(hz / 1000)) + " k" : String((int)hz), Rectangle<float>(X(hz) + 2 * u, f.getBottom() - 9 * u, 24 * u, 9 * u), juce::Justification::centredLeft); }
    }
    for (int d : {-6, -3, 0, 3, 6}) {
        g.setColour(juce::Colours::white.withAlpha(d == 0 ? 0.20f : 0.06f)); g.drawHorizontalLine((int)Y((float)d), f.getX(), f.getRight());
        g.setColour(pal.mute); g.drawText((d > 0 ? "+" : "") + String(d), Rectangle<float>(f.getX() + 1, Y((float)d) - 9 * u, 20 * u, 9 * u), juce::Justification::centredLeft);
    }
    juce::Path line, fill;
    const int N = 200;
    for (int i = 0; i <= N; ++i) {
        const float hz = 20.f * std::pow(1000.f, (float)i / N), db = juce::jlimit(-7.f, 7.f, mas::eqResponseDb(eqCache, hz));
        const float x = X(hz), y = Y(db);
        if (i == 0) { line.startNewSubPath(x, y); fill.startNewSubPath(x, Y(0)); fill.lineTo(x, y); } else { line.lineTo(x, y); fill.lineTo(x, y); }
    }
    fill.lineTo(f.getRight(), Y(0)); fill.closeSubPath();
    g.setGradientFill(juce::ColourGradient(kLilac.withAlpha(0.28f), 0, f.getY(), kLilac.withAlpha(0.02f), 0, f.getCentreY(), false));
    g.saveState(); g.reduceClipRegion(f.toNearestInt()); g.fillPath(fill);
    g.setColour(kIce.withAlpha(0.2f)); g.strokePath(line, juce::PathStrokeType(4.f * u));
    g.setColour(kIce); g.strokePath(line, juce::PathStrokeType(1.7f * u, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.restoreState();
    for (int i = 0; i < mas::kBands; ++i) {
        const float x = X(mas::bands()[i].f), y = Y(juce::jlimit(-7.f, 7.f, eqCache[i]));
        g.setColour(juce::Colours::black.withAlpha(0.6f)); g.fillEllipse(x - 4.6f * u, y - 3.6f * u, 9.2f * u, 9.2f * u);
        g.setColour(std::fabs(eqCache[i]) > 0.05f ? kPeach : kLilac.withAlpha(0.7f)); g.fillEllipse(x - 3.6f * u, y - 3.6f * u, 7.2f * u, 7.2f * u);
    }
}

void MasterEditor::paintMeters(juce::Graphics& g, Rectangle<float> r) {
    const float u = u_;
    screenWell(g, pal, r);
    const float top = r.getY() + 26 * u, bot = r.getBottom() - 14 * u, bw = 20 * u, gap = 6 * u, x0 = r.getX() + 34 * u;
    auto Y = [&](float db, float lo) { return bot - (bot - top) * juce::jlimit(0.f, 1.f, (db - lo) / (0.f - lo)); };
    g.setFont(mono(7.2f * u));
    for (int db : {0, -6, -12, -24, -36, -48}) {
        const float y = Y((float)db, -54.f);
        g.setColour(pal.mute); g.drawText(String(db), Rectangle<float>(r.getX() + 2 * u, y - 5 * u, 24 * u, 10 * u), juce::Justification::centredRight);
        g.setColour(juce::Colours::white.withAlpha(0.06f)); g.drawHorizontalLine((int)y, x0, x0 + 4 * bw + 3 * gap);
    }
    struct Bar { const char* name; float v; bool gr; float span; };
    const Bar bars[4] = {{"Ent.", dIn, false, 54.f}, {"Sort.", dOut, false, 54.f}, {"Comp", dGc, true, 12.f}, {"Lim.", dGl, true, 18.f}};
    for (int i = 0; i < 4; ++i) {
        const float x = x0 + (float)i * (bw + gap);
        auto well = Rectangle<float>(x, top, bw, bot - top);
        g.setColour(juce::Colours::black.withAlpha(0.55f)); g.fillRoundedRectangle(well, 2 * u);
        const auto& b = bars[i];
        const int segs = 44; const float sh = (bot - top) / (float)segs;
        for (int k = 0; k < segs; ++k) {
            const float frac = (float)k / (float)segs;   // 0 = bas
            const bool lit = b.gr ? (1.f - frac - 1.f / segs) * b.span < b.v : b.v > -54.f + frac * 54.f;
            if (!lit) { g.setColour(juce::Colours::white.withAlpha(0.035f)); g.fillRect(x + 2 * u, bot - (float)(k + 1) * sh + 0.8f, bw - 4 * u, sh - 1.4f); continue; }
            Colour c = b.gr ? kPeach : (frac > 0.95f ? kPeach : frac > 0.8f ? kPeach.interpolatedWith(kIce, 0.5f) : kIce.interpolatedWith(kLilac, 1.f - frac));
            g.setColour(c.withAlpha(0.95f)); g.fillRect(x + 2 * u, bot - (float)(k + 1) * sh + 0.8f, bw - 4 * u, sh - 1.4f);
        }
        g.setColour(pal.ink.withAlpha(0.85f)); tracked(g, b.name, Rectangle<float>(x - 6 * u, r.getY() + 8 * u, bw + 12 * u, 11 * u), juce::Justification::centred, 0.8f * u, fnt(7.4f * u, true));
        g.setColour(pal.mute); g.setFont(mono(6.8f * u));
        g.drawText(b.gr ? u8("dB R\xC3\x89" "D") : String("dBFS"), Rectangle<float>(x - 6 * u, bot + 1 * u, bw + 12 * u, 10 * u), juce::Justification::centred);
    }
}

void MasterEditor::paintLufs(juce::Graphics& g, Rectangle<float> r) {
    const float u = u_;
    screenWell(g, pal, r);
    g.setColour(pal.mute); tracked(g, u8("LUFS momentan\xC3\xA9"), Rectangle<float>(r.getX() + 10 * u, r.getY() + 8 * u, r.getWidth() - 20 * u, 10 * u), juce::Justification::centredLeft, 1.3f * u, fnt(7.6f * u, true));
    auto txt = [](float v) { return v <= -69.5f ? String("---") : String(v, 1); };
    g.setColour(kIce); g.setFont(mono(34.f * u));
    g.drawText(txt(lufsO), Rectangle<float>(r.getX() + 6 * u, r.getY() + 24 * u, r.getWidth() - 56 * u, 46 * u), juce::Justification::centredRight);
    g.setColour(pal.mute); g.setFont(mono(8.f * u));
    g.drawText("SORTIE", Rectangle<float>(r.getRight() - 50 * u, r.getY() + 32 * u, 44 * u, 10 * u), juce::Justification::centredLeft);
    g.drawText("LUFS", Rectangle<float>(r.getRight() - 50 * u, r.getY() + 44 * u, 44 * u, 10 * u), juce::Justification::centredLeft);
    g.setColour(kLilac.withAlpha(0.9f)); g.setFont(mono(15.f * u));
    g.drawText(txt(lufsI), Rectangle<float>(r.getX() + 6 * u, r.getY() + 84 * u, r.getWidth() - 56 * u, 22 * u), juce::Justification::centredRight);
    g.setColour(pal.mute); g.setFont(mono(8.f * u));
    g.drawText(u8("ENTR\xC3\x89" "E"), Rectangle<float>(r.getRight() - 50 * u, r.getY() + 84 * u, 48 * u, 10 * u), juce::Justification::centredLeft);
    g.drawText("LUFS", Rectangle<float>(r.getRight() - 50 * u, r.getY() + 95 * u, 44 * u, 10 * u), juce::Justification::centredLeft);
    const float d = (lufsO <= -69.5f || lufsI <= -69.5f) ? 0.f : lufsO - lufsI;
    g.setColour(pal.line); g.fillRect(Rectangle<float>(r.getX() + 10 * u, r.getY() + 74 * u, r.getWidth() - 20 * u, 1.f));
    g.setColour(pal.mute); g.setFont(mono(8.f * u));
    g.drawText(u8("\xCE\x94 ") + String::formatted("%+.1f LU", d), Rectangle<float>(r.getX() + 10 * u, r.getBottom() - 16 * u, 90 * u, 10 * u), juce::Justification::centredLeft);
}

void MasterEditor::paint(juce::Graphics& g) {
    const float u = u_;
    g.drawImageAt(plate, 0, 0);
    for (float y : {14.f, 476.f}) for (float x : {14.f, 986.f}) screw(g, x * u, y * u, 5.f * u, (x + y) * 0.7f);
    g.setColour(juce::Colours::black.withAlpha(0.45f)); g.fillRect(S(0, 0, 3, kH)); g.fillRect(S(997, 0, 3, kH));
    g.setColour(juce::Colours::white.withAlpha(0.07f)); g.fillRect(S(3, 0, 1, kH)); g.fillRect(S(996, 0, 1, kH));

    engraved(g, pal, "dreamaws", S(30, 20, 120, 12), juce::Justification::centredLeft, 8.8f * u, 2.6f * u);
    engraved(g, pal, "Orbe Master", S(30, 34, 300, 30), juce::Justification::centredLeft, 24.f * u, 3.6f * u, true);
    g.setColour(pal.mute); g.setFont(fnt(9.6f * u));
    g.drawText(u8("Cha\xC3\xAEne de mastering st\xC3\xA9r\xC3\xA9o \xC2\xB7 limiteur look-ahead true-peak"), S(334, 44, 400, 13), juce::Justification::centredLeft);
    g.setFont(mono(8.4f * u)); g.drawText("DW-602", S(334, 28, 70, 12), juce::Justification::centredLeft);
    g.setColour(kPeach.withAlpha(0.9f)); g.fillRect(S(30, 66, 28, 1.6f)); g.setColour(kLilac.withAlpha(0.9f)); g.fillRect(S(58, 66, 28, 1.6f)); g.setColour(kIce.withAlpha(0.9f)); g.fillRect(S(86, 66, 28, 1.6f));
    const int lat = proc.getLatencySamples(); const double sr = proc.getSampleRate() > 0 ? proc.getSampleRate() : 48000.0;

    // égaliseur
    g.setColour(juce::Colours::black.withAlpha(0.5f)); g.fillRect(S(24, 94, 584, 1.2f));
    paintCurve(g, S(30, 98, 572, 148));
    // modules
    module(g, "Compresseur de bus", 24, 250, 348);
    module(g, "Saturation", 282, 138, 348);
    module(g, "St\xC3\xA9r\xC3\xA9o M/S", 428, 182, 348);
    // limiteur
    engraved(g, pal, "Limiteur true-peak", S(622, 78, 250, 12), juce::Justification::centredLeft, 8.8f * u, 2.2f * u, true);
    g.setColour(juce::Colours::black.withAlpha(0.5f)); g.fillRect(S(616, 94, 364, 1.2f));
    paintMeters(g, S(620, 100, 150, 360));
    paintLufs(g, S(782, 100, 194, 150));
    auto lw = S(782, 344, 194, 22);
    screenWell(g, pal, lw);
    g.setColour(pal.mute); g.setFont(mono(8.f * u));
    g.drawText(u8("LATENCE ") + String(lat) + u8(" \xC3\xA9" "ch. \xC2\xB7 ") + String(1000.0 * lat / sr, 1) + " ms", lw.reduced(8 * u, 0), juce::Justification::centredLeft);
    auto tw = S(782, 376, 194, 84);
    screenWell(g, pal, tw);
    const char* names[4] = {"CR\xC3\x8ATE ENTR\xC3\x89" "E", "CR\xC3\x8ATE SORTIE", "R\xC3\x89" "D. COMPRESSEUR", "R\xC3\x89" "D. LIMITEUR"};
    const String vals[4] = {String(dIn, 1) + " dBFS", String(holdOut, 1) + " dBFS", String(dGc, 1) + " dB", String(dGl, 1) + " dB"};
    for (int i = 0; i < 4; ++i) {
        const auto row = Rectangle<float>(tw.getX() + 10 * u, tw.getY() + (6.f + 19.f * (float)i) * u, tw.getWidth() - 20 * u, 16 * u);
        g.setColour(pal.mute); g.setFont(mono(7.4f * u)); g.drawText(u8(names[i]), row, juce::Justification::centredLeft);
        g.setColour(i == 1 && holdOut > -0.5f ? kPeach : kIce); g.setFont(mono(9.6f * u)); g.drawText(vals[i], row, juce::Justification::centredRight);
    }
}
