#include "LooksEditor.h"

using namespace kit;
using juce::Colour, juce::Rectangle, juce::String;
using oc::ui::kLilac, oc::ui::kIce, oc::ui::kPeach;
namespace v3 = kit::v3;

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
    flat_ = v3::notes::flatPref(proc.apvts);
    keyBox.addItem("Auto", 1); for (int i = 0; i < 12; ++i) keyBox.addItem(v3::notes::pcName(i, flat_), i + 2);
    scaleBox.addItem(u8("Majeur"), 1); scaleBox.addItem(u8("Mineur"), 2);
    keyBox.setTooltip(u8("Tonalité de la correction de hauteur. Auto : détection prudente, aucune correction tant qu'elle n'est pas sûre."));
    scaleBox.setTooltip(u8("Mode utilisé quand la tonalité est choisie à la main."));
    addAndMakeVisible(keyBox); addAndMakeVisible(scaleBox);
    keyPaster.apply = [this](const kit::keyparse::Key& k) {
        auto setP = [this](const char* id, int idx) { if (auto* q = proc.apvts.getParameter(id)) q->setValueNotifyingHost(q->convertTo0to1((float)idx)); };
        setP("key", k.pc + 1); setP("scale", k.minor ? 1 : 0); };
    keyPaster.attach(keyBox);
    keepBtn.setButtonText(u8("Garder mes macros"));
    keepBtn.setTooltip(u8("Allumé : changer de look garde vos 4 macros (comparez les looks avec vos réglages). Éteint : chaque look reprend ses macros d'origine."));
    addAndMakeVisible(keepBtn); keepAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, "keep", keepBtn);
    keyAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(proc.apvts, "key", keyBox);
    scaleAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(proc.apvts, "scale", scaleBox);
    accSeg = std::make_unique<v3::bridge::Seg>(proc.apvts, "accid", v3::notes::accidentalLabels(), v3::notes::accidentalTip(), pal);
    addAndMakeVisible(*accSeg);
    prev.onClick = [this] { step(-1); }; next.onClick = [this] { step(1); };
    prev.setTooltip(u8("Look précédent")); next.setTooltip("Look suivant");
    addAndMakeVisible(prev); addAndMakeVisible(next); addAndMakeVisible(power);
    power.setTooltip(u8("Effet en service. Bascule pour le couper (la latence reste compensée)."));
    help.install(*this, "Orbe Looks", {u8("Chaîne vocale complète réglée par un « look » et quatre macros."),
        u8("- Afficheur : clic pour la liste des looks, flèches pour défiler. Les 8 voyants montrent les modules réellement en service (éteints si le module est coupé ou à zéro)."),
        u8("- Macros : Intensité, Espace, Mordant, Couleur. Double-clic : valeur d'origine du look."),
        u8("- Tonalité Auto : détection prudente ; aucune correction vers une note tant que la tonalité n'est pas sûre."),
        u8("- Sortie : Mix sec / traité et niveau avant le limiteur ; barres de sortie, de réduction et de correction (demi-tons)."),
        u8("- Bouton demi-disque : thème Clair, Sombre ou Classique et taille de la fenêtre (mémorisés).")});
    powerAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, "bypass", power);
    // --- v3 : potards haut de gamme (mêmes paramètres, attachements propres), chevrons, thème
    for (int i = 0; i < 4; ++i) {
        k3[i] = std::make_unique<v3::Knob3>(proc.apvts, kMacros[i].id, u8(kMacros[i].label), u8(kMacros[i].tip), v3::KnobSize::L);
        k3[i]->setScaleLabels("0", "10");
        k3[i]->setFormatter([](double v) { return v3::fmt::num(v * 10.0, 1); });
    }
    k3[4] = std::make_unique<v3::Knob3>(proc.apvts, "mix", "Mix", u8("Sec / traité : 100 % = voix entièrement traitée."), v3::KnobSize::M);
    k3[5] = std::make_unique<v3::Knob3>(proc.apvts, "out", "Sortie", u8("Niveau de sortie, avant le limiteur."), v3::KnobSize::M);
    k3[4]->setFormatter([](double v) { return v3::fmt::pct(v); });
    k3[5]->setFormatter([](double v) { return v3::fmt::db(v, 1); });
    k3[4]->setDoubleClickReturnValue(true, 1.0); k3[5]->setDoubleClickReturnValue(true, 0.0);
    for (int i = 4; i < 6; ++i) k3[i]->setPeach(true);
    for (auto& k : k3) addChildComponent(*k);
    addChildComponent(prev3); addChildComponent(next3);
    prev3.onClick = [this] { step(-1); }; next3.onClick = [this] { step(1); };
    prev3.setTooltip(u8("Look précédent")); next3.setTooltip("Look suivant");
    addAndMakeVisible(themeBtn);
    themeBtn.onTheme = [this](v3::Mode m) { setThemeMode(m, true); };
    themeBtn.onScale = [this](float s) { setUiScale(s); };
    setResizable(true, true);
    scale_ = v3::Prefs::scale("looks", 1.f);
    setThemeMode(v3::Prefs::theme("looks", v3::Mode::Light), false);
    startTimerHz(30);
}
LooksEditor::~LooksEditor() { stopTimer(); v3::Prefs::setScale("looks", scale_); setLookAndFeel(nullptr); }

void LooksEditor::setThemeMode(v3::Mode m, bool persist) {
    mode = m;
    T = v3::make(m == v3::Mode::Classic ? v3::Mode::Light : m, v3::accents::lilac);
    T.mode = m;
    look3.setTheme(T);
    art.clear();
    const bool n3 = isV3();
    setLookAndFeel(n3 ? static_cast<juce::LookAndFeel*>(&look3) : static_cast<juce::LookAndFeel*>(&look));
    for (auto* p : pots) p->setVisible(!n3);
    prev.setVisible(!n3); next.setVisible(!n3);
    for (auto& k : k3) { k->setVisible(n3); k->setup(&T, &art); }
    prev3.setVisible(n3); next3.setVisible(n3); prev3.T = next3.T = &T;
    power.T = &T; help.T = &T; themeBtn.T = &T; themeBtn.current = m;
    accSeg->T = m != v3::Mode::Classic ? &T : nullptr; accSeg->repaint();
    lastLook = -1;
    if (persist) v3::Prefs::setTheme("looks", m);
    const float bw = n3 ? kW3 : kW, bh = n3 ? kH3 : kH, lo = n3 ? 0.75f : 0.7f, hi = n3 ? 1.5f : 2.f;
    setResizeLimits(juce::roundToInt(bw * lo), juce::roundToInt(bh * lo), juce::roundToInt(bw * hi), juce::roundToInt(bh * hi));
    getConstrainer()->setFixedAspectRatio((double)bw / bh);
    scale_ = juce::jlimit(lo, hi, scale_);
    const int w = juce::roundToInt(bw * scale_), h = juce::roundToInt(bh * scale_);
    if (getWidth() == w && getHeight() == h) resized(); else setSize(w, h);
    sendLookAndFeelChange();
    repaint();
}

void LooksEditor::setUiScale(float s) {
    const bool n3 = isV3();
    scale_ = juce::jlimit(n3 ? 0.75f : 0.7f, n3 ? 1.5f : 2.f, s);
    setSize(juce::roundToInt((n3 ? kW3 : kW) * scale_), juce::roundToInt((n3 ? kH3 : kH) * scale_));
    v3::Prefs::setScale("looks", scale_);
}

void LooksEditor::resized3() {
    u_ = (float)getWidth() / kW3; scale_ = u_; themeBtn.scale = scale_;
    help.layout(Si(834, 20, 30, 30), getLocalBounds());
    themeBtn.setBounds(Si(794, 21, 28, 28));
    power.setBounds(Si(884, 22, 96, 26));
    prev3.setBounds(Si(326, 17, 36, 36)); next3.setBounds(Si(638, 17, 36, 36));
    const float kx[4] = {88, 227, 367, 506};
    const float kh = v3::Knob3::boxHeight(v3::KnobSize::L, false);
    for (int i = 0; i < 4; ++i) { k3[i]->setScale(u_); k3[i]->setBounds(Si(kx[i] - 68, 372, 136, kh)); }
    const float mh = v3::Knob3::boxHeight(v3::KnobSize::M, false);
    k3[4]->setScale(u_); k3[5]->setScale(u_);
    k3[4]->setBounds(Si(631 - 40, 384, 80, mh)); k3[5]->setBounds(Si(709 - 40, 384, 80, mh));
    keyBox.setBounds(Si(782, 368, 84, 36)); scaleBox.setBounds(Si(872, 368, 90, 36));
    accSeg->setBounds(Si(912, 332, 50, 28)); accSeg->px = 14.f * u_;
    keepBtn.setBounds(Si(770, 334, 138, 24));
}

void LooksEditor::resized() {
    if (isV3()) { resized3(); return; }
    u_ = (float)getWidth() / kW; look.k = u_;
    scale_ = u_; themeBtn.scale = u_;
    const auto content = getLocalBounds().toFloat().reduced(18.f * u_, 0);
    power.setBounds(kit::w::inSlot(content, u_).toNearestInt());
    help.layout(kit::w::helpSlot(content, u_, true).toNearestInt(), getLocalBounds());
    themeBtn.setBounds(help.getBounds().translated(-help.getWidth() - juce::roundToInt(6.f * u_), 0));
    for (int i = 0; i < 4; ++i) { pots[i]->setScale(u_); pots[i]->setBounds(Si(28 + 101 * (float)i, 230, 101, 100)); }
    pots[4]->setScale(u_); pots[4]->setBounds(Si(28, 446, 96, 90));
    pots[5]->setScale(u_); pots[5]->setBounds(Si(336, 446, 96, 90));
    prev.setBounds(Si(30, 117, 20, 26)); next.setBounds(Si(410, 117, 20, 26));
    keyBox.setBounds(Si(28, 366, 100, 30)); scaleBox.setBounds(Si(136, 366, 100, 30));
    accSeg->setBounds(Si(244, 366, 60, 30));
    keepBtn.setBounds(Si(28, 404, 200, 24));
}

void LooksEditor::timerCallback() {
    if (const bool f = v3::notes::flatPref(proc.apvts); f != flat_) { flat_ = f; v3::notes::relabelKeyItems(keyBox, 2, f); repaint(); }
    const float pk = proc.host.peak.load(), gr = proc.host.reduction.load(), sh = proc.host.shift.load();
    const float db = 20.f * std::log10(juce::jmax(1e-5f, pk));
    outDb = juce::jmax(db, outDb - 1.0f);
    grDisp = juce::jmax(gr, grDisp - 0.5f);
    shiftDisp += (sh - shiftDisp) * 0.4f;
    moved = proc.macrosMoved();
    autoGainDisp += (proc.host.autoGain.load() - autoGainDisp) * 0.3f;
    {   // état réel des modules : interrupteur du module × dosage effectif ; tout éteint si l'effet est court-circuité
        const auto st = proc.settings();
        const bool byp = proc.apvts.getRawParameterValue("bypass")->load() > 0.5f;
        auto lv = [&](float on, float amt) { return byp || on < 0.5f || amt <= 0.001f ? 0.f : juce::jlimit(0.35f, 1.f, amt); };
        const float t[8] = {lv(st.eqOn, 1.f), lv(st.cpOn, st.cpMix), lv(st.saOn, st.saMix * juce::jmin(1.f, 0.4f + st.drive)), lv(st.tnOn, st.tnAmt),
                            lv(st.dsOn, st.dsAmt * 1.5f), lv(st.dbOn, st.dbMix * 2.5f), lv(st.dyOn, st.dyMix * 3.f), lv(st.rvOn, st.rvMix * 3.f)};
        for (int i = 0; i < 8; ++i) modLevel[i] = t[i];
    }
    if (isV3()) {
        rLvl[rPos] = juce::jlimit(0.f, 1.f, (outDb + 48.f) / 48.f); rGr[rPos] = juce::jlimit(0.f, 12.f, gr); rSh[rPos] = juce::jlimit(-3.f, 3.f, sh);
        rPos = (rPos + 1) % kHist;
        if (proc.currentLook() != lastLook) {   // double-clic : valeurs d'origine du look ; arcs de modulation remis à zéro
            lastLook = proc.currentLook();
            for (int i = 0; i < 4; ++i) k3[i]->setDoubleClickReturnValue(true, LooksProcessor::macroDefault(kMacros[i].id, lastLook));
            for (auto& k : k3) k->resetMod();
        }
        for (auto& k : k3) k->tickMod(1.f / 30.f);
        repaint(S(20, 0, 960, 312).toNearestInt()); repaint(S(764, 410, 216, 166).toNearestInt());
        return;
    }
    repaint(S(18, 54, 424, 150).toNearestInt()); repaint(S(24, 350, 412, 60).toNearestInt()); repaint(S(130, 430, 200, 120).toNearestInt());
}

void LooksEditor::mouseUp(const juce::MouseEvent& e) {
    if (menuArea().contains(e.position) && !e.mouseWasDraggedSinceMouseDown()) showMenu();
}
void LooksEditor::showMenu() {
    juce::PopupMenu m;
    const char* cat = "";   // liste groupée par catégorie (en-têtes) ; le menu défile seul quand il dépasse l'écran
    for (int i = 0; i < oc::numLooks(); ++i) {
        const auto& L = oc::looks()[(size_t)i];
        if (std::strcmp(cat, L.cat) != 0) { cat = L.cat; m.addSectionHeader(u8(cat)); }
        m.addItem(i + 1, LooksProcessor::lookName(i) + u8("  ·  ") + u8(L.tag), true, i == proc.currentLook());
    }
    m.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(localAreaToGlobal(menuArea().toNearestInt())).withMinimumWidth(juce::roundToInt(328 * u_)),
                    [this](int r) { if (r > 0) proc.applyLook(r - 1); });
}

void LooksEditor::section(juce::Graphics& g, const char* title, float y) {
    engraved(g, pal, u8(title), S(28, y, 330, 12), juce::Justification::centredLeft, 8.8f * u_, 2.2f * u_, true);
    g.setColour(juce::Colours::black.withAlpha(0.5f)); g.fillRect(S(24, y + 15, 412, 1.2f));
    g.setColour(juce::Colours::white.withAlpha(0.07f)); g.fillRect(S(24, y + 16.2f, 412, 1.f));
}

// ============================================================ v3 « hybride v2 » (Clair / Sombre) : 1000 × 596
void LooksEditor::paintBackdrop3(juce::Graphics& g, Rectangle<float> b) {
    const float u = b.getWidth() / kW3;
    auto R = [u](float x, float y, float w, float h) { return Rectangle<float>(x * u, y * u, w * u, h * u); };
    v3::background(g, T, b);
    g.setColour(T.inkMute); v3::caps(g, u8("dreamaws · orbe"), R(27, 14, 200, 16), 12.f * u, juce::Justification::centredLeft);
    g.setColour(T.accentText); tracked(g, "Looks", R(26, 28, 200, 32), juce::Justification::centredLeft, 3.f * u, v3::font(36.f * u, true));
    g.setColour(T.accent); g.fillRoundedRectangle(R(28, 61, 30, 3), 1.5f * u);
    v3::card(g, T, R(318, 12, 364, 46), 23.f * u);
    v3::hero(g, T, R(20, 74, 960, 235), 20.f * u);
    v3::card(g, T, R(20, 324, 556, 252), 16.f * u);
    v3::card(g, T, R(589, 324, 162, 252), 16.f * u);
    v3::card(g, T, R(764, 324, 216, 252), 16.f * u);
    g.setColour(T.ink);
    v3::caps(g, "Macros", R(38, 338, 300, 18), 12.5f * u, juce::Justification::centredLeft);
    v3::caps(g, u8("Mix · sortie"), R(606, 338, 140, 18), 12.5f * u, juce::Justification::centredLeft);
    v3::caps(g, u8("Tonalité · mode"), R(782, 338, 190, 18), 12.5f * u, juce::Justification::centredLeft);
    g.setColour(T.cardEdge); g.fillRect(R(670, 372, 1, 150));
    g.setColour(T.scrMute);
    v3::caps(g, u8("Chaîne"), R(38, 88, 120, 18), 12.f * u, juce::Justification::centredLeft);
    v3::caps(g, "Correction", R(676, 88, 160, 18), 12.f * u, juce::Justification::centredLeft);
    const char* leg[3] = {"sortie", "réduction", "correction"};
    const juce::Colour lc[3] = {T.sung, T.accent.brighter(0.3f), T.out};
    float lx = 300.f;
    for (int i = 0; i < 3; ++i) {
        g.setColour(lc[i]); g.fillRoundedRectangle(R(lx, 142, 13, 3), 1.5f * u);
        g.setColour(T.scrInk.withAlpha(0.85f)); g.setFont(v3::font(12.5f * u));
        g.drawText(u8(leg[i]), R(lx + 19, 134, 90, 18), juce::Justification::centredLeft, false);
        lx += 105.f;
    }
    g.setColour(juce::Colours::white.withAlpha(0.10f)); g.fillRect(R(653, 92, 1, 200));
}

void LooksEditor::paint3(juce::Graphics& g) {
    const float u = u_;
    art.draw(g, String("bd|") + v3::modeKey(mode), getLocalBounds().toFloat(), [this](juce::Graphics& ig, Rectangle<float> b) { paintBackdrop3(ig, b); });
    const auto L = (size_t)proc.currentLook(); const auto& lk = oc::looks()[L];
    // pilule du look : compteur, nom, modifié
    g.setColour(T.inkMute);
    v3::tabular(g, String(proc.currentLook() + 1).paddedLeft('0', 2) + "/" + String(oc::numLooks()), S(362, 17, 50, 36), v3::font(13.f * u), juce::Justification::centredLeft);
    g.setColour(T.ink); g.setFont(v3::font(19.f * u, true));
    g.drawText(u8(lk.name), S(412, 17, 196, 36), juce::Justification::centred, true);
    if (moved) { g.setColour(T.accentText); g.setFont(v3::font(11.f * u)); g.drawText(u8("● modifié"), S(560, 40, 76, 14), juce::Justification::centredRight, false); }
    // écran : catégorie et description, modules en service
    g.setColour(T.scrInk.withAlpha(0.9f)); g.setFont(v3::font(13.f * u));
    g.drawText(u8(lk.cat) + u8(" · ") + u8(lk.tag), S(140, 88, 500, 18), juce::Justification::centredLeft, true);
    for (int i = 0; i < 8; ++i) {
        auto c = S(38.f + 75.f * (float)i, 110, 68, 22);
        const float lv = modLevel[i];
        g.setColour(lv > 0.f ? T.accent.withAlpha(0.18f + 0.2f * lv) : juce::Colours::white.withAlpha(0.05f)); g.fillRoundedRectangle(c, c.getHeight() * 0.5f);
        g.setColour(lv > 0.f ? T.accent.brighter(0.4f).withAlpha(0.5f + 0.5f * lv) : juce::Colours::white.withAlpha(0.10f)); g.drawRoundedRectangle(c.reduced(0.5f), c.getHeight() * 0.5f, 1.f);
        g.setColour(lv > 0.f ? T.scrInk : T.scrMute.withAlpha(0.6f)); v3::caps(g, kModNames[i], c, 10.5f * u, juce::Justification::centred);
    }
    // courbes : sortie, réduction (depuis le haut), correction (centre)
    const auto plot = S(38, 160, 600, 132);
    for (int i = 0; i < kHist; ++i) { const int k = (rPos + i) % kHist; bLvl[i] = rLvl[k]; bGr[i] = rGr[k]; bSh[i] = rSh[k]; }
    auto FX = [&](float i) { return plot.getX() + plot.getWidth() * i / (float)(kHist - 1); };
    g.setColour(juce::Colours::white.withAlpha(0.07f));
    for (float x = plot.getX(); x < plot.getRight(); x += 6.f * u) g.fillRect(x, plot.getCentreY() - 0.5f, 2.f * u, 1.f);
    {
        juce::Graphics::ScopedSaveState st(g);
        g.reduceClipRegion(plot.expanded(8.f * u, 6.f * u).toNearestInt());
        const v3::Trace::Opts o{kHist, 1.4f, 1e9f, -1e9f, 6};
        trLvl.build(bLvl, kHist, FX, [&](float v) { return plot.getBottom() - v * plot.getHeight(); }, plot.getBottom(), o);
        trGr.build(bGr, kHist, FX, [&](float v) { return plot.getY() + v / 12.f * plot.getHeight() * 0.7f; }, plot.getY(), o);
        trSh.build(bSh, kHist, FX, [&](float v) { return plot.getCentreY() - v / 3.f * plot.getHeight() * 0.5f; }, plot.getCentreY(), o);
        trLvl.draw(g, T.sung, 1.6f * u, 0.7f, 0.12f, plot.getBottom(), false);
        trGr.draw(g, T.accent.brighter(0.3f), 1.6f * u, 0.8f);
        trSh.draw(g, T.out, 2.2f * u, 1.f, 0.f, 0.f, true);
    }
    // correction : grand chiffre, réduction, tonalité
    const auto big = v3::font(60.f * u, true);
    const String num = v3::fmt::signedNum(shiftDisp, 2);
    g.setColour(T.scrInk); v3::tabular(g, num, S(672, 108, 200, 70), big, juce::Justification::centredLeft);
    g.setColour(T.scrMute); g.setFont(v3::font(17.f * u)); g.drawText("dt", Rectangle<float>(S(672, 0, 0, 0).getX() + v3::tabularWidth(num, big) + 4.f * u, 150.f * u, 30.f * u, 22.f * u), juce::Justification::centredLeft, false);
    g.setColour(T.scrMute); v3::caps(g, u8("Réduction"), S(676, 196, 130, 16), 11.f * u, juce::Justification::centredLeft);
    v3::caps(g, "Sortie", S(826, 196, 130, 16), 11.f * u, juce::Justification::centredLeft);
    g.setColour(T.scrInk);
    v3::tabular(g, v3::fmt::db(grDisp > 0.05f ? -(double)grDisp : 0.0, 1), S(676, 214, 140, 28), v3::font(22.f * u, true), juce::Justification::centredLeft);
    v3::tabular(g, outDb <= -59.f ? String(v3::fmt::minus()) + u8("∞") : v3::fmt::db(outDb, 1), S(826, 214, 140, 28), v3::font(22.f * u, true), juce::Justification::centredLeft);
    g.setColour(T.scrMute); v3::caps(g, u8("Niveau auto"), S(676, 250, 130, 16), 11.f * u, juce::Justification::centredLeft);
    g.setColour(T.scrInk); v3::tabular(g, v3::fmt::db(autoGainDisp, 1), S(826, 246, 140, 22), v3::font(16.f * u, true), juce::Justification::centredLeft);
    // tonalité détectée (carte)
    String txt; juce::Colour tc = T.ink;
    const int kp = proc.host.keyPc.load(), sc = proc.host.keyScale.load();
    const bool corrects = modLevel[3] > 0.f; const bool manual = proc.apvts.getRawParameterValue("key")->load() >= 0.5f;
    if (!corrects) { txt = u8("Correction coupée"); tc = T.inkMute; }
    else if (manual) txt = u8("Tonalité choisie");
    else if (kp >= 0) txt = u8("Détectée : ") + String(v3::notes::pcName(kp, flat_)) + (sc == 2 ? u8(" mineur") : u8(" majeur"));
    else { txt = u8("À l'écoute…"); tc = T.accentText; }
    g.setColour(tc); g.setFont(v3::font(14.f * u)); g.drawText(txt, S(782, 414, 190, 22), juce::Justification::centredLeft, true);
    g.setColour(T.inkMute); g.setFont(v3::font(12.5f * u));
    g.drawFittedText(u8("Auto : détection prudente, aucune correction tant que la tonalité n'est pas sûre. Le mode sert quand la tonalité est choisie."),
                     S(782, 446, 182, 110).toNearestInt(), juce::Justification::topLeft, 6, 1.f);
}

void LooksEditor::paint(juce::Graphics& g) {
    if (isV3()) { paint3(g); return; }
    const float u = u_;
    kit::w::drawFace(g, pal, getLocalBounds(), u, {kit::w::Family::Rack, "Orbe Looks", kit::w::serial::looks});

    // afficheur du look (VFD)
    const auto L = (size_t)proc.currentLook(); const auto& look_ = oc::looks()[L];
    auto scr = S(58, 62, 344, 136);
    screenWell(g, pal, scr, Display::VFD);
    const auto vfd = displayColours(Display::VFD, pal).ink;
    g.setColour(vfd.withAlpha(0.6f)); g.setFont(mono(10.f * u));
    g.drawText(String(proc.currentLook() + 1).paddedLeft('0', 2) + " / " + String(oc::numLooks()) + u8("  ·  ") + u8(look_.cat), S(70, 68, 220, 14).toNearestInt(), juce::Justification::centredLeft);
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
    else if (kp >= 0) txt = u8("Détectée : ") + String(v3::notes::pcName(kp, flat_)) + (sc == 2 ? u8(" mineur") : u8(" majeur"));
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
