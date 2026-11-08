#include "MasterEditor.h"

using namespace kit;
using juce::Colour, juce::Rectangle, juce::String;

namespace {
constexpr float kW = 1000.f, kH = 490.f, kPotH = 82.f, kKnobR = 19.f;
using kit::w::kLilac, kit::w::kIce, kit::w::kPeach;
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

Palette MasterEditor::palette() { return kit::w::rack(kIce); }

MasterEditor::MasterEditor(MasterProcessor& p) : AudioProcessorEditor(&p), proc(p), pal(palette()), look(pal), power(pal) {
    setLookAndFeel(&look);
    for (int i = 0; i < mas::kBands; ++i) pots.add(new kit::w::Pot(proc.apvts, "eq" + String(i), u8(kEqLabel[i]), u8(kEqTip[i]), nullptr, kKnobR, kIce));
    for (auto& d : kOther) pots.add(new kit::w::Pot(proc.apvts, d.id, u8(d.label), u8(d.tip), nullptr, kKnobR, kIce));
    for (auto* pt : pots) { pt->setStyle(kit::KnobStyle::Stepped); addAndMakeVisible(pt); }
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
    power.setTooltip(u8("Effet en service. Bascule pour le couper (la latence reste compens\xC3\xA9" "e)."));
    help.install(*this, "Orbe Master", {u8("Cha\xC3\xAEne de mastering st\xC3\xA9r\xC3\xA9o : \xC3\xA9galiseur 7 bandes, compresseur de bus, saturation, st\xC3\xA9r\xC3\xA9o M/S, limiteur look-ahead true-peak."),
        u8("- Chaque module a son interrupteur \xC3\xA0 voyant. Double-clic sur un potard : valeur d'origine."),
        u8("- Compresseur : 1 \xC3\xA0 2 dB de r\xC3\xA9" "duction suffisent. Mono sous : remet les basses en mono sous la fr\xC3\xA9quence choisie."),
        u8("- Limiteur : Gain fixe le niveau final ; Plafond -1 dBTP conseill\xC3\xA9 pour la diffusion."),
        u8("- Afficheurs : barres de cr\xC3\xAAte entr\xC3\xA9" "e/sortie et r\xC3\xA9" "duction (compresseur, limiteur) ; LUFS momentan\xC3\xA9 de sortie et d'entr\xC3\xA9" "e ; VU de sortie (voyant : cr\xC3\xAAte au-dessus de -0,5 dBFS)."),
        u8("- Latence du limiteur : ") + String(proc.getLatencySamples()) + u8(" \xC3\xA9" "chantillons, d\xC3\xA9" "clar\xC3\xA9" "e \xC3\xA0 l'h\xC3\xB4te.")});
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
    const auto content = getLocalBounds().toFloat().reduced(18.f * u_, 0);
    power.setBounds(kit::w::inSlot(content, u_).toNearestInt());
    help.layout(kit::w::helpSlot(content, u_, true).toNearestInt(), getLocalBounds());
    for (int i = 0; i < mas::kBands; ++i) pots[i]->setBounds(Si(30.f + 81.4f * (float)i, 252, 81.4f, kPotH));
    for (int i = 0; i < (int)(sizeof(kOther) / sizeof(kOther[0])); ++i) pots[mas::kBands + i]->setBounds(Si(kOther[i].x, kOther[i].y, kOther[i].w, kPotH));
    for (auto* pt : pots) pt->setScale(u_);
    for (int i = 0; i < toggles.size(); ++i) toggles[i]->setBounds(Si(kTog[i].x, kTog[i].y, kTog[i].w, 20));
    resetBtn.setBounds(Si(700, 9, 150, 32));
}

void MasterEditor::timerCallback() {
    auto db = [](float v) { return 20.f * std::log10(juce::jmax(1e-5f, v)); };
    auto fall = [](float& d, float v, float rate) { d = v > d ? v : d - rate; };
    fall(dIn, db(proc.inPk.load()), 0.8f); fall(dOut, db(proc.outPk.load()), 0.8f);
    dGc = juce::jmax(proc.grComp.load(), dGc - 0.25f); dGl = juce::jmax(proc.grLim.load(), dGl - 0.5f);
    holdOut = juce::jmax(db(proc.outPk.load()), holdOut - 0.15f);
    lufsO = proc.lufsOut.load(); lufsI = proc.lufsIn.load();
    vu.step(lufsO > -69.5f ? std::sqrt(std::pow(10.0, (lufsO + 0.691) / 10.0)) : 0.0, 1.0 / 30.0);   // LUFS momentané (K) → tension efficace
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
    screenWell(g, pal, r, Display::OLED);
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
    screenWell(g, pal, r, Display::Seg7);
    auto txt = [](float v) { return v <= -69.5f ? String("-.-") : String(v, 1); };
    const auto ink = displayColours(Display::Seg7, pal).ink;
    g.setColour(ink.withAlpha(0.6f)); g.setFont(mono(8.f * u));
    g.drawText(u8("SORTIE \xC2\xB7 LUFS M"), Rectangle<float>(r.getX() + 10 * u, r.getY() + 6 * u, r.getWidth() - 20 * u, 10 * u), juce::Justification::centredLeft);
    screenText(g, pal, Display::Seg7, txt(lufsO), Rectangle<float>(r.getX() + 8 * u, r.getY() + 20 * u, r.getWidth() - 16 * u, 44 * u), juce::Justification::centredRight, 36.f * u);
    g.setColour(ink.withAlpha(0.18f)); g.fillRect(Rectangle<float>(r.getX() + 10 * u, r.getY() + 74 * u, r.getWidth() - 20 * u, 1.f));
    g.setColour(ink.withAlpha(0.6f)); g.setFont(mono(8.f * u));
    g.drawText(u8("ENTR\xC3\x89" "E \xC2\xB7 LUFS M"), Rectangle<float>(r.getX() + 10 * u, r.getY() + 80 * u, r.getWidth() - 20 * u, 10 * u), juce::Justification::centredLeft);
    screenText(g, pal, Display::Seg7, txt(lufsI), Rectangle<float>(r.getX() + 8 * u, r.getY() + 92 * u, r.getWidth() - 16 * u, 24 * u), juce::Justification::centredRight, 20.f * u);
    const float d = (lufsO <= -69.5f || lufsI <= -69.5f) ? 0.f : lufsO - lufsI;
    g.setColour(ink.withAlpha(0.6f)); g.setFont(mono(8.f * u));
    g.drawText(u8("\xCE\x94 ") + String::formatted("%+.1f LU", d), Rectangle<float>(r.getX() + 10 * u, r.getBottom() - 16 * u, 120 * u, 10 * u), juce::Justification::centredLeft);
}

void MasterEditor::paint(juce::Graphics& g) {
    const float u = u_;
    kit::w::drawFace(g, pal, getLocalBounds(), u, {kit::w::Family::Rack, "Orbe Master", kit::w::serial::master});

    // égaliseur
    g.setColour(juce::Colours::black.withAlpha(0.5f)); g.fillRect(S(24, 94, 584, 1.2f));
    paintCurve(g, S(30, 98, 572, 148));
    // modules
    module(g, "Compresseur de bus", 24, 250, 348);
    module(g, "Saturation", 282, 138, 348);
    module(g, "St\xC3\xA9r\xC3\xA9o M/S", 428, 182, 348);
    // limiteur
    engraved(g, pal, "Limiteur true-peak", S(622, 76, 250, 12), juce::Justification::centredLeft, 8.8f * u, 2.2f * u, true);
    g.setColour(juce::Colours::black.withAlpha(0.5f)); g.fillRect(S(616, 94, 364, 1.2f));
    paintMeters(g, S(620, 100, 150, 360));
    paintLufs(g, S(782, 100, 194, 150));
    vuMeter(g, pal, S(782, 344, 194, 116), (float)vu.vu(), holdOut > -0.5f);
}
