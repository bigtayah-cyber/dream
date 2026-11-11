#include "PluginEditor.h"

using namespace kit;
using juce::Colour, juce::Rectangle, juce::String;
namespace v3 = kit::v3;

Palette VoileEditor::palette() {
    auto p = kit::w::rack(Colour(0xFFFF5A36));   // rack graphite, bande d'accent orange
    p.accent2 = Colour(0xFFFFB199);
    return p;
}

VoileEditor::VoileEditor(VoileProcessor& p) : AudioProcessorEditor(&p), proc(p), pal(palette()), look(pal) {
    auto& ap = proc.apvts;
    const char* ids[4] = {"tension", "mordant", "brume", "halo"};
    const char* tips[4] = {"Force de la correction de hauteur : douce à gauche, dure à droite",
                           "Saturation et grain numérique",
                           "Écho et salle sombre derrière la voix",
                           "Doubles à l'octave au-dessus et à la quinte en dessous"};
    for (int i = 0; i < 4; ++i) {
        auto& k = knob[i];
        k.setSliderStyle(juce::Slider::RotaryVerticalDrag); k.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        k.setMouseDragSensitivity(220); k.setTooltip(u8(tips[i]));
        k.getProperties().set("kitKnob", (int)KnobStyle::Skirt);   // gros boutons à jupe chiffrée
        addAndMakeVisible(k);
        knobAtt[i] = std::make_unique<SliderAtt>(ap, ids[i], k);
    }
    for (auto* s : {&mixS, &outS}) { s->setSliderStyle(juce::Slider::RotaryVerticalDrag); s->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0); s->setMouseDragSensitivity(180); addAndMakeVisible(*s); }
    mixS.setTooltip(u8("Part du son traité")); outS.setTooltip(u8("Niveau de sortie"));
    for (auto* s : {&mixS, &outS}) s->getProperties().set("kitKnob", (int)KnobStyle::Cap);
    mixAtt = std::make_unique<SliderAtt>(ap, "mix", mixS);
    outAtt = std::make_unique<SliderAtt>(ap, "out", outS);

    lastFlat = v3::notes::flatPref(ap);
    for (int i = 0; i < 12; ++i) keyBox.addItem(v3::notes::pcName(i, lastFlat), i + 1);
    scaleBox.addItem("Chromatique", 1); scaleBox.addItem("Majeur", 2); scaleBox.addItem("Mineur", 3);
    keyBox.setTooltip(u8("Tonalité de la correction")); scaleBox.setTooltip(u8("Gamme de la correction"));
    addAndMakeVisible(keyBox); addAndMakeVisible(scaleBox);
    keyPaster.apply = [this](const kit::keyparse::Key& k) {
        auto setP = [this](const char* id, int idx) { if (auto* q = proc.apvts.getParameter(id)) q->setValueNotifyingHost(q->convertTo0to1((float)idx)); };
        setP("key", k.pc); setP("scale", k.minor ? 2 : 1); };
    keyPaster.attach(keyBox);
    keyAtt = std::make_unique<ComboAtt>(ap, "key", keyBox);
    scaleAtt = std::make_unique<ComboAtt>(ap, "scale", scaleBox);
    accSeg = std::make_unique<v3::bridge::Seg>(ap, "accid", v3::notes::accidentalLabels(), v3::notes::accidentalTip(), pal);
    addAndMakeVisible(*accSeg);

    for (int i = 0; i < proc.getNumPrograms(); ++i) presetBox.addItem(proc.getProgramName(i), i + 1);
    presetBox.setSelectedItemIndex(proc.getCurrentProgram(), juce::dontSendNotification);
    presetBox.onChange = [this] { proc.setCurrentProgram(presetBox.getSelectedItemIndex()); };
    presetBox.setTooltip("Preset");
    addAndMakeVisible(presetBox); addAndMakeVisible(prevB); addAndMakeVisible(nextB);
    prevB.onClick = [this] { stepPreset(-1); }; nextB.onClick = [this] { stepPreset(1); };
    prevB.setTooltip(u8("Preset précédent")); nextB.setTooltip("Preset suivant");

    // --- v3 : potards haut de gamme (mêmes paramètres, attachements propres), interrupteur IN, chevrons, thème
    const char* names[4] = {"Tension", "Mordant", "Brume", "Halo"};
    const char* subs[4] = {"hauteur", "grain", "écho · salle", "doubles"};
    for (int i = 0; i < 4; ++i) {
        k3[i] = std::make_unique<v3::Knob3>(ap, ids[i], names[i], u8(tips[i]), v3::KnobSize::L);
        k3[i]->setSub(u8(subs[i])); k3[i]->setScaleLabels("0", "10");
        k3[i]->setFormatter([](double v) { return v3::fmt::num(v * 10.0, 1); });   // échelle 0–10 de la face
        addChildComponent(*k3[i]);
    }
    mix3 = std::make_unique<v3::Knob3>(ap, "mix", "Mix", u8("Part du son traité."), v3::KnobSize::M);
    out3 = std::make_unique<v3::Knob3>(ap, "out", "Sortie", u8("Niveau de sortie."), v3::KnobSize::M);
    mix3->setFormatter([](double v) { return v3::fmt::pct(v); });
    out3->setFormatter([](double v) { return v3::fmt::db(v, 1); });
    for (auto* k : {mix3.get(), out3.get()}) { k->setPeach(true); addChildComponent(*k); }
    out3->setBipolar(true);
    inSw = std::make_unique<v3::InSwitch>(*ap.getParameter("bypass"));
    addChildComponent(*inSw); addChildComponent(prev3); addChildComponent(next3);
    prev3.onClick = [this] { stepPreset(-1); }; next3.onClick = [this] { stepPreset(1); };
    prev3.setTooltip(u8("Preset précédent")); next3.setTooltip("Preset suivant");
    addAndMakeVisible(themeBtn);
    themeBtn.onTheme = [this](v3::Mode m) { setThemeMode(m, true); };
    themeBtn.onScale = [this](float s) { setUiScale(s); };

    help.install(*this, "Voile", {u8("Chaîne vocale en quatre gestes : la voix est recalée sur la gamme choisie, puis habillée."),
        u8("- Tension : force de la correction de hauteur (douce à gauche, dure à droite). Mordant : saturation et grain numérique."),
        u8("- Brume : écho et salle sombre derrière la voix. Halo : doubles à l'octave au-dessus et à la quinte en dessous."),
        u8("- Tonalité et gamme : la voix est recalée sur cette gamme (Chromatique : sur le demi-ton le plus proche)."),
        u8("- Écran HAUTEUR : voix chantée (bleu), voix corrigée (orange), note visée (blanc), sur les 3 dernières secondes."),
        u8("- Aiguille et grand chiffre : correction appliquée, en demi-tons (centre = aucune). VU : niveau de sortie."),
        u8("- Mix : part du son traité ; Sortie : niveau final."),
        u8("- Potards : double-clic = valeur d'origine, Maj = réglage fin, molette, clic droit = saisir une valeur."),
        u8("- Bouton demi-disque : thème Clair, Sombre ou Classique et taille de la fenêtre (mémorisés).")});
    for (auto& v : bSung) v = 0.f;
    for (auto& v : bOut) v = 0.f;
    for (auto& v : bTgt) v = 0.f;
    for (auto& v : bLvl) v = 0.f;

    setResizable(true, true);
    scale_ = v3::Prefs::scale("voile", 1.f);
    setThemeMode(v3::Prefs::theme("voile", v3::Mode::Light), false);
    startTimerHz(30);
}

VoileEditor::~VoileEditor() {
    stopTimer();
    v3::Prefs::setScale("voile", scale_);
    setLookAndFeel(nullptr);
}

void VoileEditor::setThemeMode(v3::Mode m, bool persist) {
    mode = m;
    T = v3::make(m == v3::Mode::Classic ? v3::Mode::Light : m, v3::accents::orange);
    T.mode = m;
    look3.setTheme(T);
    art.clear();
    const bool n3 = isV3();
    setLookAndFeel(n3 ? static_cast<juce::LookAndFeel*>(&look3) : static_cast<juce::LookAndFeel*>(&look));
    for (auto* c : std::initializer_list<juce::Component*>{&knob[0], &knob[1], &knob[2], &knob[3], &mixS, &outS, &prevB, &nextB}) c->setVisible(!n3);
    for (auto& k : k3) { k->setVisible(n3); k->setup(&T, &art); }
    for (auto* k : {mix3.get(), out3.get()}) { k->setVisible(n3); k->setup(&T, &art); }
    inSw->setVisible(n3); inSw->setup(&T);
    prev3.setVisible(n3); next3.setVisible(n3); prev3.T = next3.T = &T;
    help.T = &T; themeBtn.T = &T; themeBtn.current = m;
    accSeg->T = n3 ? &T : nullptr; accSeg->repaint();
    presetBox.getProperties().set("v3bare", n3);
    for (auto* cb : {&presetBox, &keyBox, &scaleBox}) cb->setJustificationType(n3 && cb == &presetBox ? juce::Justification::centred : juce::Justification::centredLeft);
    if (persist) v3::Prefs::setTheme("voile", m);
    // fenêtre : ratio fixe propre à l'apparence, échelle mémorisée
    const float bw = n3 ? kW3 : kClassicW, bh = n3 ? kH3 : kClassicH, hiS = n3 ? 1.5f : 2.0f;
    setResizeLimits(juce::roundToInt(bw * 0.75f), juce::roundToInt(bh * 0.75f), juce::roundToInt(bw * hiS), juce::roundToInt(bh * hiS));
    getConstrainer()->setFixedAspectRatio(bw / bh);
    scale_ = juce::jlimit(0.75f, hiS, scale_);
    const int w = juce::roundToInt(bw * scale_), h = juce::roundToInt(bh * scale_);
    if (getWidth() == w && getHeight() == h) resized(); else setSize(w, h);
    sendLookAndFeelChange();
    repaint();
}

void VoileEditor::setUiScale(float s) {
    const bool n3 = isV3();
    scale_ = juce::jlimit(0.75f, n3 ? 1.5f : 2.0f, s);
    setSize(juce::roundToInt((n3 ? kW3 : kClassicW) * scale_), juce::roundToInt((n3 ? kH3 : kClassicH) * scale_));
    v3::Prefs::setScale("voile", scale_);
}

void VoileEditor::stepPreset(int d) {
    const int n = proc.getNumPrograms();
    presetBox.setSelectedItemIndex(((presetBox.getSelectedItemIndex() + d) % n + n) % n, juce::sendNotification);
}

void VoileEditor::resized() {
    if (isV3()) resized3(); else resizedClassic();
    themeBtn.scale = scale_;
}

// ============================================================ Classique : face d'origine (inchangée)
void VoileEditor::resizedClassic() {
    u_ = (float)getWidth() / kClassicW;
    scale_ = u_;
    help.layout(kit::w::helpSlot(getLocalBounds().toFloat().reduced(18.f * u_, 0), u_, false).toNearestInt(), getLocalBounds());
    themeBtn.setBounds(help.getBounds().translated(-help.getWidth() - juce::roundToInt(6.f * u_), 0));
    for (int i = 0; i < 4; ++i) knob[i].setBounds(Si(96.f + (float)i * 128.f - 64.f, 140.f - 64.f, 128, 128));
    presetBox.setBounds(Si(318, 8, 220, 34));
    prevB.setBounds(Si(292, 14, 18, 22)); nextB.setBounds(Si(546, 14, 18, 22));
    mixS.setBounds(Si(608 - 36, 112 - 36, 72, 72)); outS.setBounds(Si(696 - 36, 112 - 36, 72, 72));
    keyBox.setBounds(Si(564, 290, 64, 28)); scaleBox.setBounds(Si(634, 290, 104, 28));
    accSeg->setBounds(Si(688, 262, 50, 22));
}

void VoileEditor::paintClassic(juce::Graphics& g) {
    const float u = u_;
    kit::w::drawFace(g, pal, getLocalBounds(), u, {kit::w::Family::Rack, "Voile", kit::w::serial::voile});
    screenWell(g, pal, S(318, 8, 220, 34), Display::VFD);   // la liste des presets s'affiche dans la police d'écran
    g.setColour(juce::Colours::black.withAlpha(0.5f)); g.fillRect(S(18, 246, 724, 1.5f));
    g.setColour(juce::Colours::white.withAlpha(0.06f)); g.fillRect(S(18, 247.5f, 724, 1.f));

    const char* names[4] = {"Tension", "Mordant", "Brume", "Halo"};
    const juce::String subs[4] = {u8("hauteur"), u8("grain"), u8("écho · salle"), u8("doubles")};
    for (int i = 0; i < 4; ++i) {
        const float cx = 96.f + (float)i * 128.f;
        engraved(g, pal, names[i], S(cx - 60, 206, 120, 16), juce::Justification::centred, 12.f * u, 4.f * u, true);
        g.setColour(pal.mute); g.setFont(fnt(10.f * u));
        g.drawText(subs[i], S(cx - 60, 224, 120, 12).toNearestInt(), juce::Justification::centred);
    }
    g.setColour(pal.line); g.fillRect(S(552, 62, 1, 176)); g.fillRect(S(552, 256, 1, 92));
    engraved(g, pal, "Mix", S(566, 152, 84, 14), juce::Justification::centred, 10.f * u, 2.f * u, true);
    engraved(g, pal, "Sortie", S(654, 152, 84, 14), juce::Justification::centred, 10.f * u, 2.f * u, true);
    auto val = [&](juce::Slider& s, float cx) {
        g.setColour(pal.screenInk.withAlpha(0.9f)); g.setFont(lcdFont(10.f * u));
        g.drawText(s.getTextFromValue(s.getValue()), S(cx - 40, 170, 80, 14).toNearestInt(), juce::Justification::centred);
    };
    val(mixS, 608); val(outS, 696);
    engraved(g, pal, u8("Tonalité · gamme"), S(566, 266, 172, 14), juce::Justification::centredLeft, 10.f * u, 2.f * u, true);

    // correction appliquée : galvanomètre centre zéro (±1 demi-ton) ; niveau de sortie : vrai VU
    kit::w::dial(g, S(32, 256, 220, 96), shiftSm, -1.f, 1.f, {{-1.f, -0.5f, Colour(0xFFE0603A).withAlpha(0.8f)}, {0.5f, 1.f, Colour(0xFFE0603A).withAlpha(0.8f)}},
                 {-1.f, -0.5f, 0.f, 0.5f, 1.f}, u8("Correction dt"), true);
    vuMeter(g, pal, S(268, 254, 168, 100), (float)vu.vu(), lvlSm > 0.89f);
    g.setColour(pal.mute); g.setFont(mono(9.f * u));
    g.drawText((shiftSm >= 0 ? "+" : "") + String(shiftSm, 2) + " dt", S(444, 296, 100, 14).toNearestInt(), juce::Justification::centredLeft);
}

// ============================================================ v3 « hybride v2 » (Clair / Sombre)
// Coordonnées de conception : 1000 × 596 (maquette hybride2-voile, pixels affichés / 2).
void VoileEditor::resized3() {
    u_ = (float)getWidth() / kW3;
    scale_ = u_;
    help.layout(Si(834, 20, 30, 30), getLocalBounds());
    themeBtn.setBounds(Si(794, 21, 28, 28));
    inSw->setBounds(Si(886, 22, 92, 26));
    presetBox.setBounds(Si(410, 17, 200, 36));
    prev3.setBounds(Si(326, 17, 36, 36)); next3.setBounds(Si(646, 17, 36, 36));
    const float kx[4] = {88, 227, 367, 506};
    const float kw = 136.f, kh = v3::Knob3::boxHeight(v3::KnobSize::L, true);
    for (int i = 0; i < 4; ++i) { k3[i]->setScale(u_); k3[i]->setBounds(Si(kx[i] - kw * 0.5f, 354, kw, kh)); }
    const float mh = v3::Knob3::boxHeight(v3::KnobSize::M, false);
    mix3->setScale(u_); out3->setScale(u_);
    mix3->setBounds(Si(631 - 40, 378, 80, mh)); out3->setBounds(Si(709 - 40, 378, 80, mh));
    keyBox.setBounds(Si(782, 362, 80, 36)); scaleBox.setBounds(Si(868, 362, 94, 36));
    accSeg->setBounds(Si(912, 332, 50, 28)); accSeg->px = 14.f * u_;
}

void VoileEditor::paintBackdrop3(juce::Graphics& g, Rectangle<float> b) {
    const float u = b.getWidth() / kW3;
    auto R = [u](float x, float y, float w, float h) { return Rectangle<float>(x * u, y * u, w * u, h * u); };
    v3::background(g, T, b);
    // marque
    g.setColour(T.inkMute); v3::caps(g, "dreamaws", R(27, 14, 160, 16), 12.f * u, juce::Justification::centredLeft);
    g.setColour(T.accentText); juce::ignoreUnused(g);
    tracked(g, "Voile", R(26, 28, 200, 32), juce::Justification::centredLeft, 3.f * u, v3::font(36.f * u, true));
    g.setColour(T.accent); g.fillRoundedRectangle(R(28, 61, 30, 3), 1.5f * u);
    // pilule des presets
    v3::card(g, T, R(318, 12, 364, 46), 23.f * u);
    // écran héros et cartes
    v3::hero(g, T, R(20, 74, 960, 235), 20.f * u);
    v3::card(g, T, R(20, 324, 556, 252), 16.f * u);
    v3::card(g, T, R(589, 324, 162, 252), 16.f * u);
    v3::card(g, T, R(764, 324, 216, 252), 16.f * u);
    g.setColour(T.ink); v3::caps(g, u8("Tonalité · gamme"), R(782, 338, 190, 18), 12.5f * u, juce::Justification::centredLeft);
    // textes fixes de l'écran
    g.setColour(T.scrMute);
    v3::caps(g, u8("Hauteur · cible"), R(38, 88, 300, 18), 12.f * u, juce::Justification::centredLeft);
    v3::caps(g, "Correction", R(676, 88, 160, 18), 12.f * u, juce::Justification::centredLeft);
    v3::caps(g, "Note", R(850, 88, 110, 18), 12.f * u, juce::Justification::centredRight);
    const char* leg[3] = {"chantée", "corrigée", "cible"};
    const Colour lc[3] = {T.sung, T.out, T.target};
    float lx = 318.f;
    for (int i = 0; i < 3; ++i) {
        g.setColour(lc[i]); g.fillRoundedRectangle(R(lx, 96, 13, 3), 1.5f * u);
        g.setColour(T.scrInk.withAlpha(0.85f)); g.setFont(v3::font(12.5f * u));
        g.drawText(u8(leg[i]), R(lx + 19, 88, 70, 18), juce::Justification::centredLeft, false);
        lx += i == 0 ? 85.f : 82.f;
    }
    g.setColour(juce::Colours::white.withAlpha(0.10f)); g.fillRect(R(653, 96, 1, 190));
}

void VoileEditor::paintHero(juce::Graphics& g) {
    const float u = u_;
    const auto plot = S(70, 118, 565, 172);   // zone des courbes (dans l'écran)
    const auto clip = S(22, 76, 630, 231);
    // lignes de notes de la gamme (bornes de vue lissées), étiquettes à gauche
    const int key = proc.scope.key.load(), sc = proc.scope.scale.load();
    const float range = 8.f;
    auto Y = [&](float m) { return plot.getCentreY() - (m - viewC) * plot.getHeight() / range; };
    const int lo = (int)std::floor(viewC - range * 0.5f), hi = (int)std::ceil(viewC + range * 0.5f);
    g.setFont(v3::font(11.5f * u));
    for (int n = lo; n <= hi; ++n) {
        if (!voile::allowedNote(n, key, sc)) continue;
        const float y = Y((float)n);
        if (y < plot.getY() - 4.f * u || y > plot.getBottom() + 4.f * u) continue;
        const bool cur = n == lastNote && valid > 0;
        g.setColour(juce::Colours::white.withAlpha(cur ? 0.16f : 0.07f));
        for (float x = plot.getX(); x < plot.getRight(); x += 6.f * u) g.fillRect(x, y - 0.5f, 2.f * u, 1.f);
        g.setColour(cur ? T.out : T.scrMute);
        g.drawText(v3::notes::name(n, lastFlat), Rectangle<float>(S(34, 0, 30, 0).getX(), y - 8.f * u, 30.f * u, 16.f * u), juce::Justification::centredRight, false);
    }
    juce::Graphics::ScopedSaveState st(g);
    g.reduceClipRegion(clip.toNearestInt());
    const float x0 = plot.getX(), x1 = plot.getRight();
    auto FX = [&](float i) { return x0 + (x1 - x0) * i / (float)juce::jmax(1, nView - 1); };
    // enveloppe de niveau : voile lumineux très discret au pied de l'écran
    const float base = S(0, 300, 0, 0).getY();
    trLvl.build(bLvl, nView, FX, [&](float v) { return base - juce::jmin(1.f, std::sqrt(v) * 1.6f) * 120.f * u; }, base, {200, 2.5f, 1e9f, -1.f});
    trLvl.draw(g, T.scrHalo.brighter(0.4f).withAlpha(0.30f), 1.f * u, 0.f, 0.14f, base);
    if (valid == 0) {
        g.setColour(T.scrMute);
        const bool lowT = proc.apvts.getRawParameterValue("tension")->load() < 0.02f;
        v3::caps(g, lowT ? u8("Tension à 0 : hauteur non lue") : u8("En attente de la voix"), plot, 13.f * u, juce::Justification::centred);
        return;
    }
    const v3::Trace::Opts po{300, 1.6f, 1e9f, 0.f};   // pas de coupure aux sauts : la courbe ondule d'une note à l'autre
    trTgt.build(bTgt, nView, FX, Y, plot.getBottom(), {300, 0.8f, 0.5f, 0.f});
    trSung.build(bSung, nView, FX, Y, plot.getBottom(), po);
    trOut.build(bOut, nView, FX, Y, plot.getBottom(), po);
    trTgt.draw(g, T.target.withAlpha(0.55f), 1.f * u, 0.f);
    trSung.draw(g, T.sung, 1.5f * u, 0.7f);
    trOut.draw(g, T.out, 2.4f * u, 1.f, 0.10f, plot.getBottom(), true);
}

void VoileEditor::paint3(juce::Graphics& g) {
    const float u = u_;
    art.draw(g, String("bd|") + v3::modeKey(mode), getLocalBounds().toFloat(), [this](juce::Graphics& ig, Rectangle<float> b) { paintBackdrop3(ig, b); });
    // compteur de presets
    const int n = proc.getNumPrograms(), cur = juce::jmax(0, presetBox.getSelectedItemIndex());
    g.setColour(T.inkMute);
    v3::tabular(g, String(cur + 1).paddedLeft('0', 2) + "/" + String(n).paddedLeft('0', 2), S(362, 17, 46, 36), v3::font(13.f * u), juce::Justification::centredLeft);
    paintHero(g);
    // lecture de la correction : grand chiffre tabulaire, unité, note visée, aiguille
    const auto big = v3::font(64.f * u, true);
    const String num = v3::fmt::signedNum(shiftSm, 2);
    g.setColour(T.scrInk); v3::tabular(g, num, S(672, 108, 172, 74), big, juce::Justification::centredLeft);
    const float nw = v3::tabularWidth(num, big);
    g.setColour(T.scrMute); g.setFont(v3::font(17.f * u)); g.drawText("dt", Rectangle<float>(S(672, 0, 0, 0).getX() + nw + 3.f * u, 152.f * u, 30.f * u, 22.f * u), juce::Justification::centredLeft, false);
    g.setColour(T.out); g.setFont(v3::font(48.f * u, true));
    g.drawText(lastNote >= 0 && valid > 0 ? v3::notes::name(lastNote, lastFlat) : String(v3::fmt::minus()), S(860, 112, 98, 64), juce::Justification::centredRight, false);
    v3::gauge(g, T, S(712, 196, 200, 100), shiftSm, -1.f, 1.f, v3::fmt::minus() + "1", "0", "+1", 11.5f * u);
    // voix détectée (lecture honnête : rien d'inventé quand la voix n'est pas lisible)
    g.setColour(T.inkMute); g.setFont(v3::font(13.5f * u));
    const String voix = valid > 0 && lastSung > 0.f ? u8("Voix : ") + v3::notes::name((int)std::lround(lastSung), lastFlat) + " " + v3::fmt::cents((lastSung - std::round(lastSung)) * 100.0)
                                                    : u8("Voix : en attente");
    g.drawText(voix, S(782, 402, 190, 22), juce::Justification::centredLeft, true);
    v3::vu(g, T, S(782, 436, 180, 120), (float)vu.vu(), lvlSm > 0.89f, 11.f * u);
    // séparateur de la carte MIX / SORTIE (repère visuel discret)
    g.setColour(T.cardEdge); g.fillRect(S(670, 360, 1, 160));
}

void VoileEditor::paint(juce::Graphics& g) { if (isV3()) paint3(g); else paintClassic(g); }

void VoileEditor::timerCallback() {
    if (const bool f = v3::notes::flatPref(proc.apvts); f != lastFlat) { lastFlat = f; v3::notes::relabelKeyItems(keyBox, 1, f); repaint(); }
    shiftSm += (proc.shift.load() - shiftSm) * 0.3f;
    lvlSm += (proc.level.load() - lvlSm) * 0.25f;
    vu.step(proc.level.load(), 1.0 / 30.0);
    if (presetBox.getSelectedItemIndex() != proc.getCurrentProgram() && proc.getCurrentProgram() >= 0 && !presetBox.isPopupActive())
        presetBox.setSelectedItemIndex(proc.getCurrentProgram(), juce::dontSendNotification);
    if (!isV3()) { repaint(S(18, 250, 540, 106).toNearestInt()); return; }
    // copie de l'historique (3 s) : lecture des atomiques seulement, aucune allocation
    const auto& sc = proc.scope;
    constexpr int N = VoileProcessor::Scope::N;
    nView = juce::jmin(N, (int)(3.f / VoileProcessor::Scope::period));
    const int wr = sc.wr.load(std::memory_order_acquire);
    valid = 0; float sum = 0.f; int recent = 0;
    for (int i = 0; i < nView; ++i) {
        const int k = ((wr - nView + i) % N + N) % N;
        bSung[i] = sc.sung[k].load(std::memory_order_relaxed); bOut[i] = sc.out[k].load(std::memory_order_relaxed);
        bTgt[i] = sc.tgt[k].load(std::memory_order_relaxed); bLvl[i] = sc.lvl[k].load(std::memory_order_relaxed);
        if (bSung[i] > 0.f) { ++valid; if (i > nView - 150) { sum += bSung[i]; ++recent; } }
    }
    for (int i = nView - 1; i >= juce::jmax(0, nView - 60); --i) if (bTgt[i] > 0.f) { lastNote = (int)bTgt[i]; lastSung = bSung[i]; break; }
    if (recent > 6) { const float mc = sum / (float)recent; viewC += (mc - viewC) * (std::fabs(mc - viewC) > 4.f ? 0.4f : 0.06f); }
    // modulation / automation visible sur les potards
    if (proc.getCurrentProgram() != lastProg) { lastProg = proc.getCurrentProgram(); for (auto& k : k3) k->resetMod(); mix3->resetMod(); out3->resetMod(); }
    for (auto& k : k3) k->tickMod(1.f / 30.f);
    mix3->tickMod(1.f / 30.f); out3->tickMod(1.f / 30.f);
    repaint(S(20, 74, 960, 236).toNearestInt());
    repaint(S(780, 400, 184, 160).toNearestInt());
    repaint(S(360, 15, 50, 40).toNearestInt());
}
