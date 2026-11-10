#include "MixEditor.h"

using namespace ou;
using kit::u8;
namespace v3 = kit::v3;

namespace {
void txt(juce::Graphics& g, const String& s, juce::Rectangle<float> r, float h, juce::Colour c, juce::Justification j = juce::Justification::centredLeft, bool bold = false, bool mono = false) {
    g.setColour(c); g.setFont(mono ? kit::mono(h) : kit::fnt(h, bold)); g.drawText(s, r, j, true);
}
String num(double v, bool valid, double floorV = -69.5) { return (!valid || v < floorV) ? String("--,-") : fr(v); }
}  // namespace

MixEditor::MixEditor(MixProcessor& p)
    : Base3(p, 1000.f, 640.f, 1180.f, 740.f, "Orbe Mix", kit::w::serial::mix, kit::Display::Amber, kWarn, "mix", "Orbe", "Mix", v3::accents::amber), proc(p), resetB(u8("Remettre à zéro")) {
    juce::StringArray groups;
    for (auto& pr : oa::kProfiles) if (!groups.contains(pr.group)) groups.add(pr.group);
    for (auto& gname : groups) {   // menu groupé ; les profils sont déjà rangés par groupe, l'index du paramètre suit l'ordre d'ajout
        styleBox.addSectionHeading(u8(gname.toRawUTF8()));
        for (int i = 0; i < 11; ++i) if (gname == oa::kProfiles[i].group) styleBox.addItem(u8(oa::kProfiles[i].label), i + 1);
    }
    addAndMakeVisible(styleBox); addAndMakeVisible(resetB);
    styleAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(proc.apvts, "profile", styleBox);
    styleBox.setTooltip(u8("Style visé : fixe la cible de volume, de dynamique et d'équilibre tonal"));
    resetB.setTooltip(u8("Efface toute la mesure et repart de zéro"));
    resetB.onClick = [this] { proc.requestReset(); blank = true; wantEpoch = proc.resetEpoch() + 1; scroll = 0; };
    setHelp({u8("Mesure le mix en lecture et le compare à un style visé. Le son passe tel quel : rien n'est modifié."),
             u8("- Lance la lecture dans l'hôte : les mesures se remplissent ; le score et les conseils arrivent après 3 secondes."),
             u8("- Style : fixe la cible de volume intégré, de dynamique (PLR) et d'équilibre tonal."),
             u8("- Équilibre tonal : barre = écart de chaque bande ; zone claire = tolérance du style."),
             u8("- Conseils : molette de la souris pour défiler."),
             u8("- Remettre à zéro efface toute la mesure.")});
    start(20, "mix");
}

void MixEditor::layout() { styleBox.setBounds(Ri(560, 11, 220, 28)); resetB.setBounds(Ri(792, 9, 130, 32)); }

void MixEditor::timerCallback() {
    if (blank && proc.resetEpoch() >= wantEpoch) blank = false;
    snap = proc.snapshot();
    ready = !blank && snap.valid && snap.seconds >= 3.0 && snap.lufsI > -70.f;
    if (ready) rep = oa::evaluate(snap, proc.profile());
    const int n = ready ? (int)rep.diags.size() : 0; scroll = juce::jlimit(0, juce::jmax(0, n - 3), scroll);
    repaint();
}

void MixEditor::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w) {
    if ((isV3() ? R(20, 566, 740, 170) : R(20, 452, 620, 168)).contains(e.position)) { if (w.deltaY == 0.f) return; const int nd = ready ? (int)rep.diags.size() : 0; scroll = juce::jlimit(0, juce::jmax(0, nd - 3), scroll + (w.deltaY < 0 ? 1 : -1)); repaint(); }
}

void MixEditor::body(juce::Graphics& g) {
    const float u = u_; const auto& P = oa::kProfiles[juce::jlimit(0, 10, proc.profile())];
    kit::engraved(g, pal, "Style", R(470, 11, 80, 28), juce::Justification::centredRight, 10.f * u, 2.f * u);

    // ---- bandeau de mesures
    const bool has = !blank && snap.lufsI > -70.f;
    struct Cell { String label; String val, unit, sub; juce::Colour c; };
    const float lufsI = snap.lufsI;
    const juce::Colour cI = !has ? scr : lufsI > P.lufsHi + 2.5 || lufsI < P.lufsLo - 3 ? kBad : lufsI > P.lufsHi + .5 || lufsI < P.lufsLo - .5 ? kWarn : kOk;
    const juce::Colour cT = !has ? scr : snap.tp > 0 ? kBad : snap.tp > -1 ? kWarn : kOk;
    const juce::Colour cP = !has ? scr : snap.plr < P.plr - 1.5 ? kBad : snap.plr < P.plr ? kWarn : kOk;
    const bool live = !blank && snap.seconds > 0;
    Cell cells[6] = {
        {u8("Momentané"), num(snap.lufsM, live), "LUFS", u8("fenêtre 400 ms"), scr},
        {u8("Court terme"), num(snap.lufsS, live), "LUFS", u8("fenêtre 3 s"), scr},
        {u8("Intégré"), num(lufsI, has), "LUFS", u8("visé ") + fr(P.lufsLo, 0) + u8(" à ") + fr(P.lufsHi, 0), cI},
        {u8("LRA"), has && snap.lra > 0 ? fr(snap.lra) : "--,-", "LU", u8("plage dynamique"), scr},
        {u8("Pic vrai"), num(snap.tp, live, -99), "dBTP", u8("plafond conseillé −1"), cT},
        {u8("PLR"), has ? fr(snap.plr) : "--,-", "dB", u8("mini ") + fr(P.plr), cP}};
    for (int i = 0; i < 6; ++i) {
        const float x = 20.f + (float)i * 162.f;
        kit::engraved(g, pal, cells[i].label, R(x + 4, 78, 146, 12), juce::Justification::centredLeft, 8.5f * u, 1.8f * u);
        well(g, R(x, 94, 150, 82));
        lcd(g, R(x + 6, 98, 120, 44), cells[i].val, 31.f * u, cells[i].c);
        txt(g, cells[i].unit, R(x + 104, 130, 40, 12), 8.5f * u, pal.mute, juce::Justification::centredRight, true);
        txt(g, cells[i].sub, R(x + 10, 152, 134, 14), 9.f * u, pal.mute, juce::Justification::centredLeft);
    }

    panel(g, pal, R(20, 190, 620, 250), u8("Équilibre tonal · 7 bandes contre la cible"), u);
    paintSpectrum(g, u);
    paintAdvice(g, u);

    // ---- score
    panel(g, pal, R(656, 190, 324, 196), "Score", u);
    {
        const int sc = ready ? rep.score : 0;
        dial(g, R(684, 214, 200, 110), (float)sc, 0.f, 100.f, {{0.f, 60.f, kBad.withAlpha(.75f)}, {60.f, 80.f, kWarn.withAlpha(.85f)}, {80.f, 100.f, Colour(0xFF3FAF6A)}},
             {0.f, 20.f, 40.f, 60.f, 80.f, 100.f}, "Score", ready);
        well(g, R(896, 246, 72, 46));
        lcd(g, R(898, 250, 68, 38), ready ? String(sc) : "--", 30.f * u, ready ? (sc >= 80 ? kOk : sc >= 60 ? kWarn : kBad) : scr.withAlpha(.5f), juce::Justification::centred);
        txt(g, "/ 100", R(896, 296, 72, 12), 9.f * u, pal.mute, juce::Justification::centred, true, true);
        int nb = 0, nw = 0; if (ready) for (auto& d : rep.diags) { nb += d.sev == 0; nw += d.sev == 1; }
        txt(g, ready ? String(nb) + u8(" à corriger") + kDot + String(nw) + u8(" à surveiller") : u8("Mesure en cours"), R(672, 352, 292, 16), 10.5f * u, pal.ink.withAlpha(.85f), juce::Justification::centred);
    }
    // ---- stéréo
    panel(g, pal, R(656, 398, 324, 106), u8("Stéréo"), u);
    {
        const bool st = live && snap.ch > 1;
        dial(g, R(672, 418, 150, 80), st ? snap.corrLive : 0.f, -1.f, 1.f, {{-1.f, 0.f, kBad.withAlpha(.75f)}, {0.f, .15f, kWarn.withAlpha(.85f)}, {.15f, 1.f, Colour(0xFF3FAF6A)}},
             {-1.f, 0.f, 1.f}, u8("Corr."), st);
        txt(g, st ? u8("Moyenne ") + fr(snap.corr, 2) : snap.ch == 1 && live ? u8("Source mono") : u8("En attente"), R(834, 432, 134, 14), 10.5f * u, pal.ink.withAlpha(.88f));
        txt(g, st ? u8("Direct ") + fr(snap.corrLive, 2) : String(), R(834, 450, 134, 14), 10.f * u, pal.mute);
        txt(g, live && snap.ch > 1 && snap.frames > 0 ? u8("Côtés < 150 Hz : ") + (snap.lowSide < -60.f ? String("< -60") : T(oa::f1(snap.lowSide))) + " dB" : String(), R(834, 470, 134, 14), 9.5f * u, pal.mute);
    }
    // ---- séance
    panel(g, pal, R(656, 516, 324, 104), u8("Séance"), u);
    {
        const int t = blank ? 0 : (int)snap.seconds;
        txt(g, u8("Durée mesurée"), R(672, 546, 130, 14), 10.f * u, pal.mute); txt(g, String(t / 60) + ":" + String(t % 60).paddedLeft('0', 2), R(802, 546, 162, 14), 11.f * u, pal.ink, juce::Justification::centredRight, false, true);
        txt(g, u8("Cible"), R(672, 564, 90, 14), 10.f * u, pal.mute); txt(g, u8(P.label), R(742, 564, 222, 14), 11.f * u, kLilac, juce::Justification::centredRight);
        txt(g, u8("Écrêtages · décalage DC"), R(672, 582, 160, 14), 10.f * u, pal.mute);
        txt(g, live ? String(snap.clips) + kDot + fr(snap.dc * 100, 2) + " %" : "--", R(812, 582, 152, 14), 11.f * u, snap.clips ? kWarn : pal.ink, juce::Justification::centredRight, false, true);
    }
}

void MixEditor::paintSpectrum(juce::Graphics& g, float u) {
    well(g, R(36, 218, 588, 186));
    const auto p = R(66, 226, 552, 170); const float yc = p.getCentreY(), ppd = p.getHeight() / 38.f;   // ±19 dB
    auto fx = [&](double f) { return p.getX() + p.getWidth() * (float)(std::log(f / 20.0) / std::log(1000.0)); };
    auto fy = [&](double d) { return yc - (float)juce::jlimit(-19.0, 19.0, d) * ppd; };
    for (int i = 0; i < 7; ++i) {   // fond des bandes
        if (i % 2 == 0) { g.setColour(juce::Colours::white.withAlpha(.025f)); g.fillRect(fx(oa::kBands[i].lo), p.getY(), fx(oa::kBands[i].hi) - fx(oa::kBands[i].lo), p.getHeight()); }
    }
    for (int d = -18; d <= 18; d += 6) {
        g.setColour(juce::Colours::white.withAlpha(d == 0 ? .16f : .06f)); g.fillRect(p.getX(), fy(d), p.getWidth(), 1.f);
        txt(g, (d > 0 ? "+" : "") + String(d), R(42, fy(d) / u - 6, 22, 12), 8.f * u, pal.mute, juce::Justification::centredRight, false, true);
    }
    for (double f : {100.0, 1000.0, 10000.0}) { g.setColour(juce::Colours::white.withAlpha(.06f)); g.fillRect(fx(f), p.getY(), 1.f, p.getHeight()); }
    if (!ready) { txt(g, blank ? u8("MESURE REMISE À ZÉRO") : u8("EN ATTENTE DE SIGNAL"), p, 12.f * u, scr.withAlpha(.7f), juce::Justification::centred, false, true); return; }
    double m = 0; for (int i = 0; i < 7; ++i) m += snap.bands[i]; m /= 7;
    juce::Path fill, line, liveP;   // courbe intégrée et courbe directe
    for (int i = 0; i < oa::kCurveN; ++i) {
        const float x = fx(oa::curveFreq(i)), y = fy(snap.curve[i] - m);
        if (i == 0) { line.startNewSubPath(x, y); fill.startNewSubPath(x, p.getBottom()); fill.lineTo(x, y); } else { line.lineTo(x, y); fill.lineTo(x, y); }
        const float yl = fy(snap.live[i] - m); if (i == 0) liveP.startNewSubPath(x, yl); else liveP.lineTo(x, yl);
    }
    fill.lineTo(fx(oa::curveFreq(oa::kCurveN - 1)), p.getBottom()); fill.closeSubPath();
    g.setColour(kLilac.withAlpha(.10f)); g.fillPath(fill);
    g.setColour(kLilac.withAlpha(.9f)); g.strokePath(line, juce::PathStrokeType(1.3f, juce::PathStrokeType::curved));
    g.setColour(scr.withAlpha(.55f)); g.strokePath(liveP, juce::PathStrokeType(1.f, juce::PathStrokeType::curved));
    for (int i = 0; i < 7; ++i) {
        const float x0 = fx(oa::kBands[i].lo) + 3 * u, x1 = fx(oa::kBands[i].hi) - 3 * u;
        const double d = rep.dev[i], t = rep.tol[i]; const int sev = std::abs(d) <= t ? 2 : std::abs(d) > t * 2 ? 0 : 1;
        g.setColour(scr.withAlpha(.13f)); g.fillRect(x0, fy(rep.target[i] + t), x1 - x0, fy(rep.target[i] - t) - fy(rep.target[i] + t));
        g.setColour(scr.withAlpha(.85f)); g.fillRect(x0, fy(rep.target[i]) - .75f, x1 - x0, 1.5f);
        g.setColour(sevColour(sev).withAlpha(.28f)); g.fillRoundedRectangle(x0, fy(rep.shape[i]) - 4.f * u, x1 - x0, 8.f * u, 2.f * u);
        g.setColour(sevColour(sev)); g.fillRoundedRectangle(x0, fy(rep.shape[i]) - 2.f * u, x1 - x0, 4.f * u, 2.f * u);
        // libellé et écart sous le graphique
        const float cx = (fx(oa::kBands[i].lo) + fx(oa::kBands[i].hi)) / 2;
        static const char* shortN[7] = {"Sub", "Basses", "Bas-méd.", "Médiums", "Haut-méd.", "Prés.", "Air"};
        txt(g, u8(shortN[i]), juce::Rectangle<float>(cx - 44 * u, 407 * u, 88 * u, 12 * u), 8.5f * u, pal.mute, juce::Justification::centred, true);
        txt(g, T(oa::f1(d)), juce::Rectangle<float>(cx - 44 * u, 420 * u, 88 * u, 14 * u), 10.5f * u, sevColour(sev), juce::Justification::centred, false, true);
    }
    // légende
    g.setColour(scr); g.fillRect(R(470, 233, 14, 1.5f)); txt(g, "cible", R(488, 227, 40, 12), 8.5f * u, pal.mute);
    g.setColour(kLilac); g.fillRect(R(530, 233, 14, 1.5f)); txt(g, u8("moyenne"), R(548, 227, 50, 12), 8.5f * u, pal.mute);
}

void MixEditor::paintAdvice(juce::Graphics& g, float u) {
    panel(g, pal, R(20, 452, 620, 168), "Conseils", u);
    well(g, R(32, 474, 596, 138));
    if (!ready) { txt(g, u8("EN ATTENTE DE SIGNAL"), R(44, 494, 572, 20), 12.f * u, scr.withAlpha(.7f), juce::Justification::centredLeft, false, true); return; }
    const int n = (int)rep.diags.size();
    txt(g, String(scroll + 1) + "-" + String(juce::jmin(n, scroll + 3)) + " / " + String(n) , R(380, 456, 246, 14), 8.5f * u, pal.mute, juce::Justification::centredRight, false, true);
    for (int k = 0; k < 3 && scroll + k >= 0 && scroll + k < n; ++k) {
        const auto& d = rep.diags[(size_t)(scroll + k)]; const float y = 480.f + (float)k * 44.f;
        kit::led(g, 38 * u, (y + 8) * u, 4.f * u, true, sevColour(d.sev));
        txt(g, T(d.title), R(52, y, 360, 16), 11.5f * u, scr, juce::Justification::centredLeft, false, true);
        txt(g, T(d.meas), R(340, y, 288, 16), 10.f * u, sevColour(d.sev), juce::Justification::centredRight, false, true);
        g.setColour(scr.withAlpha(.78f)); g.setFont(kit::lcdFont(10.f * u));
        g.drawFittedText(T(d.fix), R(52, y + 17, 576, 26).toNearestInt(), juce::Justification::topLeft, 2, 1.f);
        if (k < 2) { g.setColour(scr.withAlpha(.15f)); g.fillRect(R(40, y + 42, 580, 1.f)); }
    }
}

void MixEditor::themeChanged(const v3::Theme* tp) { resetB.T = tp; resetB.repaint(); styleBox.repaint(); }

// ============================================================ v3 « hybride v2 » : coordonnées de conception 1180 x 740
void MixEditor::layout3() { styleBox.setBounds(Ri(660, 34, 210, 32)); resetB.setBounds(Ri(882, 34, 120, 32)); }

void MixEditor::backdrop3(juce::Graphics& g, float k) {
    const auto& t = skin.T;
    auto Rk = [k](float x, float y, float w, float h) { return juce::Rectangle<float>(x * k, y * k, w * k, h * k); };
    for (int i = 0; i < 6; ++i) v3::hero(g, t, Rk(20.f + (float)i * 192.4f, 84, 178, 132), 18.f * k);
    v3::hero(g, t, Rk(20, 232, 740, 316), 18.f * k);
    v3::card(g, t, Rk(20, 564, 740, 172), 18.f * k);
    v3::card(g, t, Rk(780, 232, 380, 214), 18.f * k); v3::card(g, t, Rk(780, 458, 380, 140), 18.f * k); v3::card(g, t, Rk(780, 610, 380, 126), 18.f * k);
    v3::rk::label(g, t.inkMute, k, "Style", 616, 42, 40, 11.f, juce::Justification::centredRight);
    v3::rk::label(g, t.scrMute, k, u8("\xC3\x89" "quilibre tonal \xC2\xB7 7 bandes contre la cible"), 40, 246, 400);
    v3::rk::label(g, t.inkMute, k, "Conseils", 40, 578, 200); v3::rk::label(g, t.inkMute, k, "Score", 800, 246, 200);
    v3::rk::label(g, t.inkMute, k, u8("St\xC3\xA9r\xC3\xA9o"), 800, 472, 200); v3::rk::label(g, t.inkMute, k, u8("S\xC3\xA9" "ance"), 800, 624, 200);
}

void MixEditor::body3(juce::Graphics& g) {
    const float u = u_; const auto& P = oa::kProfiles[juce::jlimit(0, 10, proc.profile())]; const auto& T = skin.T;
    auto t3 = [&](const String& s, juce::Rectangle<float> rc, float px, juce::Colour c, juce::Justification j = juce::Justification::centredLeft, bool bold = false) {
        g.setColour(c); g.setFont(v3::font(px * u, bold)); g.drawText(s, rc, j, true);
    };
    auto tab = [&](const String& s, juce::Rectangle<float> rc, float px, juce::Colour c, juce::Justification j, bool bold = true) { g.setColour(c); v3::tabular(g, s, rc, v3::font(px * u, bold), j); };
    auto sev = [&](int s) { const auto c = sevColour(s); return T.dark ? c : c.darker(0.45f); };
    const bool has = !blank && snap.lufsI > -70.f, live = !blank && snap.seconds > 0;
    // ---- bandeau de mesures
    const float lufsI = snap.lufsI;
    const juce::Colour cI = !has ? T.scrInk : lufsI > P.lufsHi + 2.5 || lufsI < P.lufsLo - 3 ? kBad : lufsI > P.lufsHi + .5 || lufsI < P.lufsLo - .5 ? kWarn : T.out;
    const juce::Colour cT = !has ? T.scrInk : snap.tp > 0 ? kBad : snap.tp > -1 ? kWarn : T.out;
    const juce::Colour cP = !has ? T.scrInk : snap.plr < P.plr - 1.5 ? kBad : snap.plr < P.plr ? kWarn : T.out;
    struct Cell { String label, val, unit, sub; juce::Colour c; };
    const Cell cells[6] = {
        {u8("Momentan\xC3\xA9"), num(snap.lufsM, live), "LUFS", u8("fen\xC3\xAAtre 400 ms"), T.scrInk}, {u8("Court terme"), num(snap.lufsS, live), "LUFS", u8("fen\xC3\xAAtre 3 s"), T.scrInk},
        {u8("Int\xC3\xA9gr\xC3\xA9"), num(lufsI, has), "LUFS", u8("vis\xC3\xA9 ") + v3::fmt::num(P.lufsLo, 0) + u8(" \xC3\xA0 ") + v3::fmt::num(P.lufsHi, 0), cI},
        {"LRA", has && snap.lra > 0 ? v3::fmt::num(snap.lra) : "--,-", "LU", u8("plage dynamique"), T.scrInk},
        {u8("Pic vrai"), num(snap.tp, live, -99), "dBTP", u8("plafond conseill\xC3\xA9 \xE2\x88\x92" "1"), cT}, {"PLR", has ? v3::fmt::num(snap.plr) : "--,-", "dB", u8("mini ") + v3::fmt::num(P.plr), cP}};
    for (int i = 0; i < 6; ++i) {
        const float x = 20.f + (float)i * 192.4f;
        t3(cells[i].label, R(x + 16, 94, 150, 16), 11.f, T.scrMute, juce::Justification::centredLeft, true);
        tab(String(cells[i].val).replace(".", ","), R(x + 12, 112, 154, 52), 40.f, cells[i].c, juce::Justification::centredLeft);
        t3(cells[i].unit, R(x + 16, 164, 70, 16), 11.f, T.scrMute, juce::Justification::centredLeft, true);
        t3(cells[i].sub, R(x + 16, 186, 156, 18), 12.f, T.scrMute);
    }
    // ---- équilibre tonal
    {
        const auto p = R(76, 276, 660, 196); const float yc = p.getCentreY(), ppd = p.getHeight() / 38.f;
        auto fx = [&](double f) { return p.getX() + p.getWidth() * (float)(std::log(f / 20.0) / std::log(1000.0)); };
        auto fy = [&](double d) { return yc - (float)juce::jlimit(-19.0, 19.0, d) * ppd; };
        for (int i = 0; i < 7; i += 2) { g.setColour(juce::Colours::white.withAlpha(.03f)); g.fillRect(fx(oa::kBands[i].lo), p.getY(), fx(oa::kBands[i].hi) - fx(oa::kBands[i].lo), p.getHeight()); }
        for (int d = -18; d <= 18; d += 6) {
            g.setColour(juce::Colours::white.withAlpha(d == 0 ? .18f : .07f)); g.fillRect(p.getX(), fy(d), p.getWidth(), 1.f);
            tab((d > 0 ? String(u8("+")) : d < 0 ? String(u8("\xE2\x88\x92")) : String()) + String(std::abs(d)), R(40, fy(d) / u - 8, 30, 16), 11.f, T.scrMute, juce::Justification::centredRight, false);
        }
        for (double f : {100.0, 1000.0, 10000.0}) { g.setColour(juce::Colours::white.withAlpha(.07f)); g.fillRect(fx(f), p.getY(), 1.f, p.getHeight()); }
        if (!ready) { t3(blank ? u8("Mesure remise \xC3\xA0 z\xC3\xA9ro") : u8("En attente de signal"), p, 16.f, T.scrMute, juce::Justification::centred); }
        else {
            double m = 0; for (int i = 0; i < 7; ++i) m += snap.bands[i]; m /= 7;
            auto smooth = [&](bool integrated, juce::Path* fill) {
                juce::Path path; juce::Point<float> prev;
                for (int i = 0; i < oa::kCurveN; ++i) {
                    const juce::Point<float> q(fx(oa::curveFreq(i)), fy((integrated ? snap.curve[i] : snap.live[i]) - m));
                    if (i == 0) { path.startNewSubPath(q); if (fill) { fill->startNewSubPath(q.x, p.getBottom()); fill->lineTo(q); } }
                    else { const auto mid = (prev + q) * .5f; path.quadraticTo(prev, mid); if (fill) fill->quadraticTo(prev, mid); }
                    prev = q;
                }
                path.lineTo(prev); if (fill) { fill->lineTo(prev); fill->lineTo(prev.x, p.getBottom()); fill->closeSubPath(); }
                return path;
            };
            juce::Path fill; const auto line = smooth(true, &fill); const auto liveP = smooth(false, nullptr);
            g.setGradientFill(juce::ColourGradient(T.sung.withAlpha(.22f), 0, p.getY(), T.sung.withAlpha(0.f), 0, p.getBottom(), false)); g.fillPath(fill);
            g.setColour(T.scrMute.withAlpha(.6f)); g.strokePath(liveP, juce::PathStrokeType(1.2f * u, juce::PathStrokeType::curved));
            g.setColour(T.sung.withAlpha(.25f)); g.strokePath(line, juce::PathStrokeType(6.f * u, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour(T.sung); g.strokePath(line, juce::PathStrokeType(2.f * u, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            static const char* shortN[7] = {"Sub", "Basses", "Bas-m\xC3\xA9" "d.", "M\xC3\xA9" "diums", "Haut-m\xC3\xA9" "d.", "Pr\xC3\xA9s.", "Air"};
            for (int i = 0; i < 7; ++i) {
                const float x0 = fx(oa::kBands[i].lo) + 3 * u, x1 = fx(oa::kBands[i].hi) - 3 * u;
                const double d = rep.dev[i], t = rep.tol[i]; const int sv = std::abs(d) <= t ? 2 : std::abs(d) > t * 2 ? 0 : 1; const auto c = sevColour(sv);
                g.setColour(T.scrInk.withAlpha(.12f)); g.fillRoundedRectangle(x0, fy(rep.target[i] + t), x1 - x0, fy(rep.target[i] - t) - fy(rep.target[i] + t), 3 * u);
                g.setColour(T.scrInk.withAlpha(.8f)); g.fillRect(x0, fy(rep.target[i]) - .75f, x1 - x0, 1.5f);
                g.setColour(c.withAlpha(.30f)); g.fillRoundedRectangle(x0, fy(rep.shape[i]) - 5.f * u, x1 - x0, 10.f * u, 5.f * u);
                g.setColour(c); g.fillRoundedRectangle(x0, fy(rep.shape[i]) - 2.5f * u, x1 - x0, 5.f * u, 2.5f * u);
                const float cx = (fx(oa::kBands[i].lo) + fx(oa::kBands[i].hi)) / 2;
                t3(u8(shortN[i]), juce::Rectangle<float>(cx - 46 * u, 480 * u, 92 * u, 16 * u), 12.f, T.scrMute, juce::Justification::centred, true);
                tab(v3::fmt::signedNum(d, 1), juce::Rectangle<float>(cx - 46 * u, 498 * u, 92 * u, 20 * u), 15.f, c, juce::Justification::centred);
            }
            g.setColour(T.scrInk); g.fillRect(R(520, 253, 14, 1.5f)); t3("cible", R(540, 246, 48, 16), 11.f, T.scrMute);
            g.setColour(T.sung); g.fillRect(R(600, 253, 14, 1.5f)); t3("moyenne", R(620, 246, 70, 16), 11.f, T.scrMute);
        }
    }
    // ---- conseils
    {
        if (!ready) t3(u8("En attente de signal"), R(40, 612, 700, 24), 16.f, T.inkMute);
        else {
            const int n = (int)rep.diags.size();
            t3(String(scroll + 1) + "-" + String(juce::jmin(n, scroll + 3)) + " / " + String(n), R(560, 576, 180, 18), 12.f, T.inkMute, juce::Justification::centredRight);
            for (int k = 0; k < 3 && scroll + k >= 0 && scroll + k < n; ++k) {
                const auto& d = rep.diags[(size_t)(scroll + k)]; const float y = 604.f + (float)k * 44.f;
                g.setColour(sevColour(d.sev)); g.fillEllipse(juce::Rectangle<float>(10 * u, 10 * u).withCentre(R(48, y + 9, 1, 1).getPosition()));
                t3(ou::T(d.title), R(62, y, 400, 20), 15.f, T.ink, juce::Justification::centredLeft, true);
                t3(ou::T(d.meas), R(440, y, 300, 20), 13.f, sev(d.sev), juce::Justification::centredRight, true);
                g.setColour(T.inkMute); g.setFont(v3::font(13.f * u)); g.drawFittedText(ou::T(d.fix), R(62, y + 20, 678, 24).toNearestInt(), juce::Justification::topLeft, 2, 1.f);
            }
        }
    }
    // ---- score
    {
        const int sc = ready ? rep.score : 0;
        const auto ins = R(800, 272, 340, 126);
        g.setGradientFill(juce::ColourGradient(T.scrTop, ins.getX(), ins.getY(), T.scrBot, ins.getX(), ins.getBottom(), false)); g.fillRoundedRectangle(ins, 14 * u);
        v3::gauge(g, T, R(808, 274, 168, 124), (float)sc, 0.f, 100.f, "0", "50", "100", 10.f * u);
        tab(ready ? String(sc) : "--", R(980, 296, 150, 56), 46.f, ready ? (sc >= 80 ? T.out : sc >= 60 ? juce::Colour(kWarn) : juce::Colour(kBad)) : T.scrMute, juce::Justification::centred);
        t3("/ 100", R(980, 352, 150, 18), 12.f, T.scrMute, juce::Justification::centred);
        int nb = 0, nw = 0; if (ready) for (auto& d : rep.diags) { nb += d.sev == 0; nw += d.sev == 1; }
        t3(ready ? String(nb) + u8(" \xC3\xA0 corriger") + kDot + String(nw) + u8(" \xC3\xA0 surveiller") : u8("Mesure en cours"), R(800, 410, 340, 22), 14.f, T.ink, juce::Justification::centred);
    }
    // ---- stéréo
    {
        const bool st = live && snap.ch > 1;
        const auto ins = R(800, 494, 150, 90);
        g.setGradientFill(juce::ColourGradient(T.scrTop, ins.getX(), ins.getY(), T.scrBot, ins.getX(), ins.getBottom(), false)); g.fillRoundedRectangle(ins, 14 * u);
        v3::gauge(g, T, R(802, 498, 146, 84), st ? snap.corrLive : 0.f, -1.f, 1.f, u8("\xE2\x88\x92" "1"), "0", "+1", 9.f * u);
        t3(st ? u8("Moyenne ") + v3::fmt::num(snap.corr, 2) : snap.ch == 1 && live ? u8("Source mono") : u8("En attente"), R(966, 500, 180, 20), 14.f, T.ink, juce::Justification::centredLeft, true);
        t3(st ? u8("Direct ") + v3::fmt::num(snap.corrLive, 2) : String(), R(966, 524, 180, 18), 13.f, T.inkMute);
        t3(live && snap.ch > 1 && snap.frames > 0 ? u8("C\xC3\xB4t\xC3\xA9s < 150 Hz : ") + (snap.lowSide < -60.f ? String(u8("< \xE2\x88\x92" "60")) : ou::T(oa::f1(snap.lowSide))) + " dB" : String(), R(966, 546, 180, 18), 12.f, T.inkMute);
    }
    // ---- séance
    {
        const int t = blank ? 0 : (int)snap.seconds;
        t3(u8("Dur\xC3\xA9" "e mesur\xC3\xA9" "e"), R(800, 654, 170, 18), 13.f, T.inkMute); tab(String(t / 60) + ":" + String(t % 60).paddedLeft('0', 2), R(960, 654, 180, 18), 15.f, T.ink, juce::Justification::centredRight);
        t3("Cible", R(800, 676, 100, 18), 13.f, T.inkMute); t3(u8(P.label), R(880, 676, 260, 18), 14.f, T.accentText, juce::Justification::centredRight, true);
        t3(u8("\xC3\x89" "cr\xC3\xAAtages \xC2\xB7 d\xC3\xA9" "calage DC"), R(800, 698, 190, 18), 13.f, T.inkMute);
        tab(live ? String(snap.clips) + kDot + v3::fmt::num(snap.dc * 100, 2) + u8("\xC2\xA0%") : String("--"), R(960, 698, 180, 18), 15.f, snap.clips ? sev(1) : T.ink, juce::Justification::centredRight);
    }
}
