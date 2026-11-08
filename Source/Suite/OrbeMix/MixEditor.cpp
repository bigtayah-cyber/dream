#include "MixEditor.h"

using namespace ou;
using kit::u8;

namespace {
void txt(juce::Graphics& g, const String& s, juce::Rectangle<float> r, float h, juce::Colour c, juce::Justification j = juce::Justification::centredLeft, bool bold = false, bool mono = false) {
    g.setColour(c); g.setFont(mono ? kit::mono(h) : kit::fnt(h, bold)); g.drawText(s, r, j, true);
}
String num(double v, bool valid, double floorV = -69.5) { return (!valid || v < floorV) ? String("--,-") : fr(v); }
}  // namespace

MixEditor::MixEditor(MixProcessor& p)
    : Base(p, 1000.f, 640.f, "MIX", u8("Volume perçu · pic vrai · équilibre tonal · conseils")), proc(p), resetB(u8("Remettre à zéro")) {
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
    start(20);
}

void MixEditor::layout() { styleBox.setBounds(Ri(560, 19, 236, 32)); resetB.setBounds(Ri(808, 19, 168, 32)); }

void MixEditor::timerCallback() {
    if (blank && proc.resetEpoch() >= wantEpoch) blank = false;
    snap = proc.snapshot();
    ready = !blank && snap.valid && snap.seconds >= 3.0 && snap.lufsI > -70.f;
    if (ready) rep = oa::evaluate(snap, proc.profile());
    const int n = ready ? (int)rep.diags.size() : 0; scroll = juce::jlimit(0, juce::jmax(0, n - 3), scroll);
    repaint();
}

void MixEditor::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w) {
    if (R(20, 452, 620, 168).contains(e.position)) { scroll += w.deltaY < 0 ? 1 : -1; repaint(); }
}

void MixEditor::body(juce::Graphics& g) {
    const float u = u_; const auto& P = oa::kProfiles[juce::jlimit(0, 10, proc.profile())];
    kit::engraved(g, pal, "Style", R(470, 19, 80, 32), juce::Justification::centredRight, 10.f * u, 2.f * u);

    // ---- bandeau de mesures
    const bool has = !blank && snap.lufsI > -70.f;
    struct Cell { String label; String val, unit, sub; juce::Colour c; };
    const float lufsI = snap.lufsI;
    const juce::Colour cI = !has ? kIce : lufsI > P.lufsHi + 2.5 || lufsI < P.lufsLo - 3 ? kBad : lufsI > P.lufsHi + .5 || lufsI < P.lufsLo - .5 ? kWarn : kOk;
    const juce::Colour cT = !has ? kIce : snap.tp > 0 ? kBad : snap.tp > -1 ? kWarn : kOk;
    const juce::Colour cP = !has ? kIce : snap.plr < P.plr - 1.5 ? kBad : snap.plr < P.plr ? kWarn : kOk;
    const bool live = !blank && snap.seconds > 0;
    Cell cells[6] = {
        {u8("Momentané"), num(snap.lufsM, live), "LUFS", u8("fenêtre 400 ms"), kIce},
        {u8("Court terme"), num(snap.lufsS, live), "LUFS", u8("fenêtre 3 s"), kIce},
        {u8("Intégré"), num(lufsI, has), "LUFS", u8("visé ") + fr(P.lufsLo, 0) + u8(" à ") + fr(P.lufsHi, 0), cI},
        {u8("LRA"), has && snap.lra > 0 ? fr(snap.lra) : "--,-", "LU", u8("plage dynamique"), kIce},
        {u8("Pic vrai"), num(snap.tp, live, -99), "dBTP", u8("plafond conseillé −1"), cT},
        {u8("PLR"), has ? fr(snap.plr) : "--,-", "dB", u8("mini ") + fr(P.plr), cP}};
    for (int i = 0; i < 6; ++i) {
        const float x = 20.f + (float)i * 162.f;
        kit::engraved(g, pal, cells[i].label, R(x + 4, 78, 146, 12), juce::Justification::centredLeft, 8.5f * u, 1.8f * u);
        kit::screenWell(g, pal, R(x, 94, 150, 82));
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
        const float cx = 818 * u, cy = 296 * u, r = 62 * u; const int sc = ready ? rep.score : 0;
        juce::Path tr, fl; const float a0 = juce::MathConstants<float>::pi * 1.25f, a1 = juce::MathConstants<float>::pi * 2.75f;
        tr.addCentredArc(cx, cy, r, r, 0, a0, a1, true); fl.addCentredArc(cx, cy, r, r, 0, a0, a0 + (a1 - a0) * (float)sc / 100.f, true);
        g.setColour(juce::Colours::black.withAlpha(.55f)); g.strokePath(tr, juce::PathStrokeType(9.f * u, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        if (ready) { const auto c = sc >= 80 ? kOk : sc >= 60 ? kWarn : kBad; g.setColour(c.withAlpha(.25f)); g.strokePath(fl, juce::PathStrokeType(14.f * u, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                     g.setColour(c); g.strokePath(fl, juce::PathStrokeType(6.f * u, juce::PathStrokeType::curved, juce::PathStrokeType::rounded)); }
        lcd(g, R(768, 270, 100, 46), ready ? String(sc) : "--", 40.f * u, ready ? (sc >= 80 ? kOk : sc >= 60 ? kWarn : kBad) : kIce.withAlpha(.5f), juce::Justification::centred);
        txt(g, "/ 100", R(768, 314, 100, 14), 9.5f * u, pal.mute, juce::Justification::centred, true, true);
        int nb = 0, nw = 0; if (ready) for (auto& d : rep.diags) { nb += d.sev == 0; nw += d.sev == 1; }
        txt(g, ready ? String(nb) + u8(" à corriger") + kDot + String(nw) + u8(" à surveiller") : u8("Mesure en cours : 3 s minimum"), R(672, 352, 292, 16), 10.5f * u, pal.ink.withAlpha(.85f), juce::Justification::centred);
    }
    // ---- stéréo
    panel(g, pal, R(656, 398, 324, 106), u8("Stéréo"), u);
    {
        const auto tk = R(672, 430, 292, 18); kit::screenWell(g, pal, tk);
        auto zone = [&](float c0, float c1, juce::Colour c) { const float x0 = tk.getX() + (c0 + 1) / 2 * tk.getWidth(), x1 = tk.getX() + (c1 + 1) / 2 * tk.getWidth(); g.setColour(c.withAlpha(.16f)); g.fillRect(x0, tk.getY() + 2 * u, x1 - x0, tk.getHeight() - 4 * u); };
        zone(-1, 0, kBad); zone(0, .15f, kWarn); zone(.15f, .97f, kOk); zone(.97f, 1, kWarn);
        g.setColour(pal.ink.withAlpha(.3f)); g.fillRect(tk.getCentreX() - .5f, tk.getY() + 2 * u, 1.f, tk.getHeight() - 4 * u);
        if (live && snap.ch > 1) {
            const float xl = tk.getX() + (snap.corrLive + 1) / 2 * tk.getWidth(), xi = tk.getX() + (snap.corr + 1) / 2 * tk.getWidth();
            g.setColour(kIce); g.fillRoundedRectangle(xl - 1.5f * u, tk.getY() + 1, 3.f * u, tk.getHeight() - 2, 1.5f * u);
            juce::Path t; t.addTriangle(xi - 4 * u, tk.getBottom() + 6 * u, xi + 4 * u, tk.getBottom() + 6 * u, xi, tk.getBottom() + 1); g.setColour(kLilac); g.fillPath(t);
        }
        txt(g, "-1", R(672, 454, 30, 12), 8.5f * u, pal.mute, juce::Justification::centredLeft, false, true); txt(g, "0", R(803, 454, 30, 12), 8.5f * u, pal.mute, juce::Justification::centred, false, true); txt(g, "+1", R(934, 454, 30, 12), 8.5f * u, pal.mute, juce::Justification::centredRight, false, true);
        txt(g, live && snap.ch > 1 ? u8("Corrélation ") + fr(snap.corr, 2) + u8("  (direct ") + fr(snap.corrLive, 2) + ")" : snap.ch == 1 && live ? u8("Source mono") : u8("Corrélation G/D : en attente"), R(672, 468, 292, 14), 10.5f * u, pal.ink.withAlpha(.88f));
        txt(g, live && snap.ch > 1 && snap.frames > 0 ? u8("Côtés sous 150 Hz : ") + (snap.lowSide < -60.f ? String("< -60") : T(oa::f1(snap.lowSide))) + " dB" : String(), R(672, 484, 292, 14), 9.5f * u, pal.mute);
    }
    // ---- séance
    panel(g, pal, R(656, 516, 324, 104), u8("Séance"), u);
    {
        const int t = blank ? 0 : (int)snap.seconds;
        txt(g, u8("Durée mesurée"), R(672, 546, 130, 14), 10.f * u, pal.mute); txt(g, String(t / 60) + ":" + String(t % 60).paddedLeft('0', 2), R(802, 546, 162, 14), 11.f * u, pal.ink, juce::Justification::centredRight, false, true);
        txt(g, u8("Cible"), R(672, 564, 90, 14), 10.f * u, pal.mute); txt(g, u8(P.label), R(742, 564, 222, 14), 11.f * u, kLilac, juce::Justification::centredRight);
        txt(g, u8("Écrêtages · décalage DC"), R(672, 582, 160, 14), 10.f * u, pal.mute);
        txt(g, live ? String(snap.clips) + kDot + fr(snap.dc * 100, 2) + " %" : "--", R(812, 582, 152, 14), 11.f * u, snap.clips ? kWarn : pal.ink, juce::Justification::centredRight, false, true);
        txt(g, u8("Le mix passe tel quel : rien n'est modifié."), R(672, 600, 292, 12), 8.5f * u, pal.mute.withAlpha(.8f));
    }
}

void MixEditor::paintSpectrum(juce::Graphics& g, float u) {
    const auto well = R(36, 218, 588, 186); kit::screenWell(g, pal, well);
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
    if (!ready) { txt(g, blank ? u8("Mesure remise à zéro : lance la lecture dans l'hôte.") : u8("En attente de signal : lance la lecture dans l'hôte."), p, 11.f * u, pal.mute, juce::Justification::centred); return; }
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
    g.setColour(kIce.withAlpha(.55f)); g.strokePath(liveP, juce::PathStrokeType(1.f, juce::PathStrokeType::curved));
    for (int i = 0; i < 7; ++i) {
        const float x0 = fx(oa::kBands[i].lo) + 3 * u, x1 = fx(oa::kBands[i].hi) - 3 * u;
        const double d = rep.dev[i], t = rep.tol[i]; const int sev = std::abs(d) <= t ? 2 : std::abs(d) > t * 2 ? 0 : 1;
        g.setColour(kIce.withAlpha(.13f)); g.fillRect(x0, fy(rep.target[i] + t), x1 - x0, fy(rep.target[i] - t) - fy(rep.target[i] + t));
        g.setColour(kIce.withAlpha(.85f)); g.fillRect(x0, fy(rep.target[i]) - .75f, x1 - x0, 1.5f);
        g.setColour(sevColour(sev).withAlpha(.28f)); g.fillRoundedRectangle(x0, fy(rep.shape[i]) - 4.f * u, x1 - x0, 8.f * u, 2.f * u);
        g.setColour(sevColour(sev)); g.fillRoundedRectangle(x0, fy(rep.shape[i]) - 2.f * u, x1 - x0, 4.f * u, 2.f * u);
        // libellé et écart sous le graphique
        const float cx = (fx(oa::kBands[i].lo) + fx(oa::kBands[i].hi)) / 2;
        static const char* shortN[7] = {"Sub", "Basses", "Bas-méd.", "Médiums", "Haut-méd.", "Prés.", "Air"};
        txt(g, u8(shortN[i]), juce::Rectangle<float>(cx - 44 * u, 407 * u, 88 * u, 12 * u), 8.5f * u, pal.mute, juce::Justification::centred, true);
        txt(g, T(oa::f1(d)), juce::Rectangle<float>(cx - 44 * u, 420 * u, 88 * u, 14 * u), 10.5f * u, sevColour(sev), juce::Justification::centred, false, true);
    }
    // légende
    g.setColour(kIce); g.fillRect(R(470, 233, 14, 1.5f)); txt(g, "cible", R(488, 227, 40, 12), 8.5f * u, pal.mute);
    g.setColour(kLilac); g.fillRect(R(530, 233, 14, 1.5f)); txt(g, u8("moyenne"), R(548, 227, 50, 12), 8.5f * u, pal.mute);
}

void MixEditor::paintAdvice(juce::Graphics& g, float u) {
    panel(g, pal, R(20, 452, 620, 168), "Conseils", u);
    if (!ready) { txt(g, u8("Les conseils apparaissent après quelques secondes de signal."), R(34, 490, 590, 20), 11.f * u, pal.mute); return; }
    const int n = (int)rep.diags.size();
    txt(g, String(scroll + 1) + "-" + String(juce::jmin(n, scroll + 3)) + " / " + String(n) + u8("  ·  molette pour défiler"), R(380, 458, 246, 14), 8.5f * u, pal.mute, juce::Justification::centredRight, false, true);
    for (int k = 0; k < 3 && scroll + k < n; ++k) {
        const auto& d = rep.diags[(size_t)(scroll + k)]; const float y = 482.f + (float)k * 44.f;
        kit::led(g, 38 * u, (y + 8) * u, 4.f * u, true, sevColour(d.sev));
        txt(g, T(d.title), R(52, y, 360, 16), 11.5f * u, pal.ink, juce::Justification::centredLeft, true);
        txt(g, T(d.meas), R(340, y, 288, 16), 10.f * u, sevColour(d.sev), juce::Justification::centredRight, false, true);
        g.setColour(pal.mute); g.setFont(kit::fnt(10.f * u));
        g.drawFittedText(T(d.fix), R(52, y + 17, 576, 26).toNearestInt(), juce::Justification::topLeft, 2, 1.f);
        if (k < 2) { g.setColour(pal.line.withAlpha(.5f)); g.fillRect(R(34, y + 42, 592, 1.f)); }
    }
}
