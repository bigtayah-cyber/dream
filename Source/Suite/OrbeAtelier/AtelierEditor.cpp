#include "AtelierEditor.h"

using namespace kit;
using juce::Colour, juce::Rectangle, juce::String;
using oc::ui::kLilac, oc::ui::kIce, oc::ui::kPeach;
namespace v3 = kit::v3;

namespace {
// Unités de conception : 1120 de large ; taille par défaut 1400 × 695 (texte ≥ 9 px).
constexpr float kW = 1120.f, kH = 556.f, kDefaultW = 1400.f, kR = 15.f, kPanelH = 118.f, kRowY[3] = {134.f, 260.f, 386.f}, kPad = 8.f, kGap = 8.f, kMargin = 26.f, kFlowY = 516.f;
const kit::KnobStyle kModKnob[9] = {kit::KnobStyle::Cap, kit::KnobStyle::Cap, kit::KnobStyle::Fluted, kit::KnobStyle::Chicken, kit::KnobStyle::Cap,
                                    kit::KnobStyle::Chicken, kit::KnobStyle::Chicken, kit::KnobStyle::Chicken, kit::KnobStyle::Stepped};
const juce::Colour kModCap[9] = {juce::Colour(0xFFCFC6FF), juce::Colour(0xFFA9ECFF), juce::Colour(0xFFE9E6F5), juce::Colour(0xFFFFB04A), juce::Colour(0xFFFFC4AB),
                                 juce::Colour(0xFFE9E6F5), juce::Colour(0xFFE9E6F5), juce::Colour(0xFFE9E6F5), juce::Colour(0xFFE9E6F5)};
const juce::Colour kModTint[9] = {juce::Colour(0xFF2C2A36), juce::Colour(0xFF27313A), juce::Colour(0xFF1D1D20), juce::Colour(0xFF34291F), juce::Colour(0xFF332A29),
                                  juce::Colour(0xFF26282C), juce::Colour(0xFF26282C), juce::Colour(0xFF26282C), juce::Colour(0xFF1D1D20)};
const char* const kFlow[] = {"Entrée", "Tune", "EQ", "Comp", "Sat", "De-ess", "Doubleur", "Delay", "Reverb", "Mix", "Limiteur", "Sortie"};
const char* const kFlowOn[] = {"", "tnOn", "eqOn", "cpOn", "saOn", "dsOn", "dbOn", "dyOn", "rvOn", "", "lmOn", ""};
}  // namespace

AtelierEditor::AtelierEditor(AtelierProcessor& p) : AudioProcessorEditor(&p), proc(p), pal(oc::ui::palette(kIce)), look(pal) {
    setLookAndFeel(&look);
    mods = {
        {"Tune", "tnOn", {"tnAmt", "tnSpeed", "shift", "@tonal"}, 0, {}},
        {"EQ", "eqOn", {"hp", "lsF", "lsG", "b1F", "b1G", "b1Q", "b2F", "b2G", "b2Q", "hsF", "hsG"}, 0, {}},
        {"Compresseur", "cpOn", {"thr", "ratio", "atk", "rel", "mk", "cpMix"}, 1, {}},
        {"Saturation", "saOn", {"drive", "bias", "crush", "saMix"}, 1, {}},
        {"De-ess", "dsOn", {"dsF", "dsAmt", "dsThr"}, 1, {}},
        {"Doubleur", "dbOn", {"dbMix", "dbA", "dbB", "dbWide"}, 2, {}},
        {"Delay", "dyOn", {"dyMs", "@dyDiv", "dyFb", "dyMix", "dyDuck"}, 2, {}},
        {"Reverb", "rvOn", {"rvSize", "rvPre", "rvDamp", "rvMix"}, 2, {}},
        {"Limiteur", "lmOn", {"lmCeil"}, 2, {}},
    };
    flat_ = v3::notes::flatPref(proc.apvts);
    accSeg = std::make_unique<v3::bridge::Seg>(proc.apvts, "accid", v3::notes::accidentalLabels(), v3::notes::accidentalTip(), pal);
    addAndMakeVisible(*accSeg);
    auto addCombo = [this](const String& id) {
        auto* cb = combos.add(new juce::ComboBox()); const auto items = atl::choices(id);
        for (int i = 0; i < items.size(); ++i) cb->addItem(id == "key" && i > 0 ? juce::String(v3::notes::pcName(i - 1, flat_)) : items[i], i + 1);
        if (id == "key") keyCombo = cb;
        if (auto* d = atl::find(id)) cb->setTooltip(u8(d->tip));
        addAndMakeVisible(cb); cmbAtt.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(proc.apvts, id, *cb));
    };
    for (auto& m : mods) {
        auto* t = toggles.add(new juce::ToggleButton(u8(m.title)));
        t->setTooltip(u8(atl::find(m.on)->tip) + u8(" Voyant allumé : module actif."));
        addAndMakeVisible(t); togAtt.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, m.on, *t));
        m.firstPot = pots.size();
        for (auto& id : m.ids) {
            if (id == "@tonal") { addCombo("key"); addCombo("scale"); continue; }
            if (id == "@dyDiv") { addCombo("dyDiv"); continue; }
            const auto* d = atl::find(id);
            const size_t mi = (size_t)(&m - mods.data());
            auto* pot = pots.add(new oc::ui::Pot(proc.apvts, id, u8(d->name), u8(d->tip), [id, this] { return AtelierProcessor::presetValue(id, proc.currentPreset()); }, kR, kModCap[mi]));
            pot->setStyle(kModKnob[mi]); addAndMakeVisible(pot);
            k3.add(new v3::Knob3(proc.apvts, id, u8(d->name), u8(d->tip), v3::KnobSize::S)); potIds.push_back(id);
        }
        m.nPots = pots.size() - m.firstPot;
    }
    for (auto* id : {"mix", "out"}) {
        const String s(id); const auto* d = atl::find(s);
        auto* pot = pots.add(new oc::ui::Pot(proc.apvts, s, u8(d->name), u8(d->tip), [s, this] { return s == "mix" ? 1.0 : AtelierProcessor::presetValue(s, proc.currentPreset()); }, 16.f, kIce));
        pot->setStyle(kit::KnobStyle::Cap); addAndMakeVisible(pot);
        auto* k = k3.add(new v3::Knob3(proc.apvts, s, u8(d->name), u8(d->tip), v3::KnobSize::S)); potIds.push_back(s);
        k->setPeach(true);
    }
    for (auto* k : k3) { k->setLabelPx(10.f); addChildComponent(k); }
    addChildComponent(prev3); addChildComponent(next3);
    prev3.onClick = [this] { step(-1); }; next3.onClick = [this] { step(1); };
    prev3.setTooltip(u8("Preset précédent")); next3.setTooltip("Preset suivant");
    addAndMakeVisible(themeBtn);
    themeBtn.onTheme = [this](v3::Mode m) { setThemeMode(m, true); };
    themeBtn.onScale = [this](float s) { setUiScale(s); };
    prev.onClick = [this] { step(-1); }; next.onClick = [this] { step(1); };
    prev.setTooltip(u8("Preset précédent")); next.setTooltip("Preset suivant");
    addAndMakeVisible(prev); addAndMakeVisible(next); addAndMakeVisible(power);
    power.setTooltip(u8("Effet en service. Bascule pour le couper (la latence reste compensée)."));
    help.install(*this, "Orbe Atelier", {u8("Console vocale modulaire : neuf modules dans l'ordre du trajet du signal (schéma en bas)."),
        u8("- Afficheur : clic pour la liste des presets, flèches pour défiler. ● MODIFIÉ : un réglage diffère du preset."),
        u8("- Chaque module a son interrupteur à voyant (allumé : module actif) ; le schéma du bas allume les étages en service."),
        u8("- Double-clic sur un potard : valeur du preset en cours."),
        u8("- Tune : Tonalité Auto = détection prudente, aucune correction tant qu'elle n'est pas sûre. Delay : division synchronisée au tempo de l'hôte ou temps libre."),
        u8("- Bouton demi-disque : thème Clair, Sombre ou Classique et taille de la fenêtre (mémorisés).")});
    powerAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, "bypass", power);
    setResizable(true, true);
    scale_ = v3::Prefs::scale("atelier", 1.f);
    setThemeMode(v3::Prefs::theme("atelier", v3::Mode::Light), false);
    startTimerHz(30);
}
AtelierEditor::~AtelierEditor() { stopTimer(); if (isV3()) v3::Prefs::setScale("atelier", scale_); setLookAndFeel(nullptr); }

void AtelierEditor::setThemeMode(v3::Mode m, bool persist) {
    mode = m;
    T = v3::make(m == v3::Mode::Classic ? v3::Mode::Light : m, v3::accents::ice);
    T.mode = m;
    look3.setTheme(T);
    art.clear();
    const bool n3 = isV3();
    setLookAndFeel(n3 ? static_cast<juce::LookAndFeel*>(&look3) : static_cast<juce::LookAndFeel*>(&look));
    for (auto* p : pots) p->setVisible(!n3);
    prev.setVisible(!n3); next.setVisible(!n3);
    for (auto* k : k3) { k->setVisible(n3); k->setup(&T, &art); }
    prev3.setVisible(n3); next3.setVisible(n3); prev3.T = next3.T = &T;
    power.T = &T; help.T = &T; themeBtn.T = &T; themeBtn.current = m;
    accSeg->T = n3 ? &T : nullptr; accSeg->repaint();
    lastPreset = -1;
    if (persist) v3::Prefs::setTheme("atelier", m);
    // Classique : taille d'origine (1400 de large par défaut, 1120 à 2240) ; v3 : 1200 × 700, 0,75 à 1,5
    if (n3) {
        setResizeLimits(juce::roundToInt(kW3 * 0.75f), juce::roundToInt(kH3 * 0.75f), juce::roundToInt(kW3 * 1.5f), juce::roundToInt(kH3 * 1.5f));
        getConstrainer()->setFixedAspectRatio((double)kW3 / kH3);
        scale_ = juce::jlimit(0.75f, 1.5f, scale_);
    } else {
        setResizeLimits((int)kW, (int)kH, 2240, (int)(2240 * kH / kW));
        getConstrainer()->setFixedAspectRatio((double)kW / kH);
        scale_ = juce::jlimit(1.f, 2.f, scale_ <= 1.5f && scale_ >= 0.75f ? kDefaultW / kW : scale_);
    }
    const int w = juce::roundToInt((n3 ? kW3 : kW) * scale_), h = juce::roundToInt((n3 ? kH3 : kH) * scale_);
    if (getWidth() == w && getHeight() == h) resized(); else setSize(w, h);
    sendLookAndFeelChange();
    repaint();
}

void AtelierEditor::setUiScale(float s) {
    const bool n3 = isV3();
    scale_ = n3 ? juce::jlimit(0.75f, 1.5f, s) : juce::jlimit(1.f, 2.f, s * kDefaultW / kW);
    setSize(juce::roundToInt((n3 ? kW3 : kW) * scale_), juce::roundToInt((n3 ? kH3 : kH) * scale_));
    if (n3) v3::Prefs::setScale("atelier", scale_);
}

// v3 : rangées de cartes (même découpage que la face d'origine), plus une carte « Sortie » (Mix, Sortie) en bas à droite.
void AtelierEditor::resized3() {
    u_ = (float)getWidth() / kW3; scale_ = u_; themeBtn.scale = scale_;
    help.layout(Si(1034, 20, 30, 30), getLocalBounds());
    themeBtn.setBounds(Si(994, 21, 28, 28));
    power.setBounds(Si(1084, 22, 96, 26));
    prev3.setBounds(Si(406, 17, 36, 36)); next3.setBounds(Si(758, 17, 36, 36));
    panels3.clear(); labels3.clear();
    const float rowY[3] = {236, 390, 544}, ph = 142, gap = 10, mx = 20, pad = 6;
    const float kh = v3::Knob3::boxHeight(v3::KnobSize::S, false);
    int potI = 0, cmbI = 0;
    for (int row = 0; row < 3; ++row) {
        auto wOf = [](const String& id) { return id.startsWithChar('@') ? 1.6f : 1.f; };   // listes : cellule plus large
        float cells = row == 2 ? 2.f : 0.f; int n = row == 2 ? 1 : 0;
        for (auto& m : mods) if (m.row == row) { for (auto& id : m.ids) cells += wOf(id); ++n; }
        const float cw = (kW3 - 2 * mx - (float)(n - 1) * gap - 2 * pad * (float)n) / cells;
        float x = mx;
        for (auto& m : mods) if (m.row == row) {
            float mw = 0.f; for (auto& id : m.ids) mw += wOf(id);
            const float pw = cw * mw + 2 * pad;
            panels3.push_back({x, rowY[row], pw, ph});
            const size_t mi = (size_t)(&m - mods.data());
            toggles[(int)mi]->setBounds(Si(x + 12, rowY[row] + 8, juce::jmin(pw - 20, 150.f), 22));
            float cx = x + pad;
            for (auto& id : m.ids) {
                const float c = cw * wOf(id);
                if (id == "@tonal") {
                    labels3.push_back({{cx, rowY[row] + 38, c, 12}, u8("Tonalité")}); labels3.push_back({{cx, rowY[row] + 86, c, 12}, "Mode"});
                    accSeg->setBounds(Si(cx + c - 56, rowY[row] + 56, 50, 20)); accSeg->px = 11.f * u_;
                    combos[cmbI]->setBounds(Si(cx + 4, rowY[row] + 52, c - 64, 28)); combos[cmbI + 1]->setBounds(Si(cx + 4, rowY[row] + 100, c - 8, 28)); cmbI += 2;
                } else if (id == "@dyDiv") {
                    labels3.push_back({{cx, rowY[row] + 60, c, 12}, "Division"});
                    combos[cmbI]->setBounds(Si(cx + 4, rowY[row] + 74, c - 8, 28)); ++cmbI;
                } else { k3[potI]->setScale(u_); k3[potI]->setBounds(Si(cx, rowY[row] + 36, c, kh)); ++potI; }
                cx += c;
            }
            x += pw + gap;
        }
        if (row == 2) {   // carte Sortie
            const float pw = cw * 2 + 2 * pad;
            panels3.push_back({x, rowY[row], pw, ph});
            labels3.push_back({{x + 12, rowY[row] + 12, pw - 20, 14}, "#Sortie"});
            for (int j = 0; j < 2; ++j) { k3[potI]->setScale(u_); k3[potI]->setBounds(Si(x + pad + cw * (float)j, rowY[row] + 36, cw, kh)); ++potI; }
        }
    }
}

void AtelierEditor::resized() {
    if (isV3()) { resized3(); return; }
    u_ = (float)getWidth() / kW; look.k = u_;
    scale_ = u_; themeBtn.scale = u_ * kW / kDefaultW;
    const auto content = getLocalBounds().toFloat().reduced(18.f * u_, 0);
    power.setBounds(kit::w::inSlot(content, u_).toNearestInt());
    help.layout(kit::w::helpSlot(content, u_, true).toNearestInt(), getLocalBounds());
    themeBtn.setBounds(help.getBounds().translated(-help.getWidth() - juce::roundToInt(6.f * u_), 0));
    // placement des modules : chaque rangée se partage la largeur selon le nombre de cellules
    int potI = 0, cmbI = 0;
        for (int row = 0; row < 3; ++row) {
        int cells = 0, n = 0; for (auto& m : mods) if (m.row == row) { cells += (int)m.ids.size(); ++n; }
        const float cw = (kW - 2 * kMargin - (float)(n - 1) * kGap - 2 * kPad * (float)n) / (float)cells;
        float x = kMargin;
        for (auto& m : mods) if (m.row == row) {
            const float pw = cw * (float)m.ids.size() + 2 * kPad;
            m.panel = {x, kRowY[row], pw, kPanelH}; m.labels.clear();
            const size_t mi = (size_t)(&m - mods.data());
            toggles[(int)mi]->setBounds(Si(x + 8, kRowY[row] + 5, juce::jmin(pw - 16, 130.f), 18));
            float cx = x + kPad;
            for (auto& id : m.ids) {
                const Rectangle<float> cell(cx, kRowY[row] + 28, cw, 82.f);
                if (id == "@tonal") {
                    accSeg->setBounds(Si(cx + cw - 46, kRowY[row] + 44, 42, 18));
                    combos[cmbI]->setBounds(Si(cx + 3, kRowY[row] + 42, cw - 52, 22)); combos[cmbI + 1]->setBounds(Si(cx + 3, kRowY[row] + 82, cw - 6, 22));
                    m.labels.push_back({cx, kRowY[row] + 30, cw, 10}); m.labels.push_back({cx, kRowY[row] + 70, cw, 10}); cmbI += 2;
                } else if (id == "@dyDiv") {
                    combos[cmbI]->setBounds(Si(cx + 3, kRowY[row] + 56, cw - 6, 22)); m.labels.push_back({cx, kRowY[row] + 44, cw, 10}); ++cmbI;
                } else { pots[potI]->setScale(u_); pots[potI]->setBounds(S(cell.getX(), cell.getY(), cell.getWidth(), cell.getHeight()).toNearestInt()); ++potI; }
                cx += cw;
            }
            x += pw + kGap;
        }
    }
    pots[potI]->setScale(u_); pots[potI]->setBounds(Si(866, 56, 84, 74)); pots[potI + 1]->setScale(u_); pots[potI + 1]->setBounds(Si(966, 56, 84, 74));
    prev.setBounds(Si(258, 79, 18, 24)); next.setBounds(Si(634, 79, 18, 24));
}

void AtelierEditor::timerCallback() {
    if (const bool f = v3::notes::flatPref(proc.apvts); f != flat_ && keyCombo != nullptr) { flat_ = f; v3::notes::relabelKeyItems(*keyCombo, 2, f); repaint(); }
    outDb = juce::jmax(20.f * std::log10(juce::jmax(1e-5f, proc.host.peak.load())), outDb - 1.0f);
    grDisp = juce::jmax(proc.host.reduction.load(), grDisp - 0.5f);
    modified = !proc.matchesPreset();
    if (isV3()) {
        if (proc.currentPreset() != lastPreset) {   // double-clic : valeur du preset en cours
            lastPreset = proc.currentPreset();
            for (int i = 0; i < k3.size(); ++i) {
                const auto& id = potIds[(size_t)i];
                k3[i]->setDoubleClickReturnValue(true, id == "mix" ? 1.0 : AtelierProcessor::presetValue(id, lastPreset));
                k3[i]->resetMod();
            }
        }
        for (auto* k : k3) k->tickMod(1.f / 30.f);
        repaint(S(0, 0, kW3, 228).toNearestInt());
        return;
    }
    repaint(S(250, 54, 600, 76).toNearestInt()); repaint(S(0, kFlowY - 4, kW, 40).toNearestInt());
}

void AtelierEditor::mouseUp(const juce::MouseEvent& e) {
    if (menuArea().contains(e.position) && !e.mouseWasDraggedSinceMouseDown()) showMenu();
}
void AtelierEditor::showMenu() {
    juce::PopupMenu m;
    for (int i = 0; i < atl::numPresets(); ++i) m.addItem(i + 1, atl::presetName(i) + u8("  ·  ") + atl::presetTag(i), true, i == proc.currentPreset());
    m.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(localAreaToGlobal(menuArea().toNearestInt())).withMinimumWidth(juce::roundToInt(344 * u_)),
                    [this](int r) { if (r > 0) proc.applyPreset(r - 1); });
}

// ============================================================ v3 « hybride v2 » (Clair / Sombre) : 1200 × 700
void AtelierEditor::paintBackdrop3(juce::Graphics& g, Rectangle<float> b) {
    const float u = b.getWidth() / kW3;
    auto R = [u](float x, float y, float w, float h) { return Rectangle<float>(x * u, y * u, w * u, h * u); };
    v3::background(g, T, b);
    g.setColour(T.inkMute); v3::caps(g, u8("dreamaws · orbe"), R(27, 14, 200, 16), 12.f * u, juce::Justification::centredLeft);
    g.setColour(T.accentText); tracked(g, "Atelier", R(26, 28, 240, 32), juce::Justification::centredLeft, 3.f * u, v3::font(36.f * u, true));
    g.setColour(T.accent); g.fillRoundedRectangle(R(28, 61, 30, 3), 1.5f * u);
    v3::card(g, T, R(398, 12, 404, 46), 23.f * u);
    v3::hero(g, T, R(20, 74, 1160, 148), 20.f * u);
    for (auto& p : panels3) v3::card(g, T, R(p.getX(), p.getY(), p.getWidth(), p.getHeight()), 14.f * u);
    for (auto& l : labels3) {
        const bool title = l.second.startsWithChar('#');
        g.setColour(title ? T.ink : T.inkMute);
        v3::caps(g, title ? l.second.substring(1) : l.second, R(l.first.getX(), l.first.getY(), l.first.getWidth(), l.first.getHeight()), (title ? 12.f : 10.5f) * u,
                 title ? juce::Justification::centredLeft : juce::Justification::centred, title);
    }
    g.setColour(T.scrMute);
    v3::caps(g, u8("Trajet du signal"), R(38, 88, 300, 18), 12.f * u, juce::Justification::centredLeft);
    v3::caps(g, "Sortie", R(900, 88, 120, 18), 12.f * u, juce::Justification::centredLeft);
    v3::caps(g, u8("Réduction"), R(1040, 88, 130, 18), 12.f * u, juce::Justification::centredLeft);
    g.setColour(juce::Colours::white.withAlpha(0.10f)); g.fillRect(R(884, 90, 1, 118));
}

void AtelierEditor::paint3(juce::Graphics& g) {
    const float u = u_;
    art.draw(g, String("bd|") + v3::modeKey(mode), getLocalBounds().toFloat(), [this](juce::Graphics& ig, Rectangle<float> b) { paintBackdrop3(ig, b); });
    const int pi = proc.currentPreset();
    g.setColour(T.inkMute);
    v3::tabular(g, String(pi).paddedLeft('0', 2) + "/" + String(atl::numPresets() - 1).paddedLeft('0', 2), S(442, 17, 50, 36), v3::font(13.f * u), juce::Justification::centredLeft);
    g.setColour(T.ink); g.setFont(v3::font(19.f * u, true));
    g.drawText(atl::presetName(pi), S(492, 17, 216, 36), juce::Justification::centred, true);
    if (modified) { g.setColour(T.accentText); g.setFont(v3::font(11.f * u)); g.drawText(u8("● modifié"), S(676, 40, 76, 14), juce::Justification::centredRight, false); }
    // écran : description du preset, trajet du signal (pilules lumineuses), niveaux
    g.setColour(T.scrInk.withAlpha(0.9f)); g.setFont(v3::font(13.f * u));
    g.drawText(atl::presetTag(pi), S(220, 88, 640, 18), juce::Justification::centredLeft, true);
    const int nf = (int)(sizeof(kFlow) / sizeof(kFlow[0]));
    const float fw = 840.f / (float)nf;
    for (int i = 0; i < nf; ++i) {
        const bool on = kFlowOn[i][0] == 0 || proc.flag(kFlowOn[i]) > 0.5f;
        auto c = S(38.f + fw * (float)i, 128, fw - 12.f, 30);
        if (on) { g.setColour(T.accent.withAlpha(0.14f)); g.fillRoundedRectangle(c.expanded(3.f * u), (c.getHeight() + 6.f * u) * 0.5f); }
        g.setColour(on ? T.accent.withAlpha(0.22f) : juce::Colours::white.withAlpha(0.04f)); g.fillRoundedRectangle(c, c.getHeight() * 0.5f);
        g.setColour(on ? T.accent.brighter(0.3f) : juce::Colours::white.withAlpha(0.10f)); g.drawRoundedRectangle(c.reduced(0.5f), c.getHeight() * 0.5f, 1.f);
        g.setColour(on ? T.scrInk : T.scrMute.withAlpha(0.55f)); g.setFont(v3::font(12.f * u, on));
        g.drawText(u8(kFlow[i]), c, juce::Justification::centred, true);
        if (i < nf - 1) { g.setColour(T.scrMute.withAlpha(0.6f)); g.fillEllipse(Rectangle<float>(3.f * u, 3.f * u).withCentre({c.getRight() + 6.f * u, c.getCentreY()})); }
    }
    g.setColour(T.scrMute); g.setFont(v3::font(12.f * u));
    g.drawText(u8("Voyant allumé : étage en service. Double-clic sur un potard : valeur du preset."), S(38, 174, 840, 18), juce::Justification::centredLeft, true);
    const auto big = v3::font(40.f * u, true);
    g.setColour(T.scrInk);
    v3::tabular(g, outDb <= -59.f ? String(v3::fmt::minus()) + u8("∞") : v3::fmt::num(outDb, 1), S(900, 112, 130, 50), big, juce::Justification::centredLeft);
    v3::tabular(g, v3::fmt::num(grDisp > 0.05f ? -(double)grDisp : 0.0, 1), S(1040, 112, 130, 50), big, juce::Justification::centredLeft);
    g.setColour(T.scrMute); g.setFont(v3::font(14.f * u));
    g.drawText("dB", S(900, 162, 60, 18), juce::Justification::centredLeft, false); g.drawText("dB", S(1040, 162, 60, 18), juce::Justification::centredLeft, false);
    // barres fines (crête de sortie, réduction)
    auto bar = [&](Rectangle<float> r, float t, juce::Colour c) {
        g.setColour(juce::Colours::white.withAlpha(0.10f)); g.fillRoundedRectangle(r, r.getHeight() * 0.5f);
        g.setColour(c); g.fillRoundedRectangle(r.withWidth(r.getWidth() * juce::jlimit(0.f, 1.f, t)), r.getHeight() * 0.5f);
    };
    bar(S(900, 188, 124, 4), (outDb + 48.f) / 48.f, T.sung);
    bar(S(1040, 188, 124, 4), grDisp / 12.f, T.out);
}

void AtelierEditor::paint(juce::Graphics& g) {
    if (isV3()) { paint3(g); return; }
    const float u = u_;
    kit::w::drawFace(g, pal, getLocalBounds(), u, {kit::w::Family::Rack, "Orbe Atelier", kit::w::serial::atelier});
    // preset (VFD)
    auto scr = S(284, 58, 344, 66); screenWell(g, pal, scr, Display::VFD);
    const auto vfd = displayColours(Display::VFD, pal).ink;
    const int pi = proc.currentPreset();
    g.setColour(vfd.withAlpha(0.6f)); g.setFont(mono(9.4f * u));
    g.drawText(String(pi).paddedLeft('0', 2) + " / " + String(atl::numPresets() - 1), S(296, 62, 80, 12).toNearestInt(), juce::Justification::centredLeft);
    if (modified) { g.setColour(kPeach.withAlpha(0.9f)); g.setFont(mono(9.f * u)); g.drawText(u8("● MODIFIÉ"), S(520, 62, 100, 12).toNearestInt(), juce::Justification::centredRight); }
    screenText(g, pal, Display::VFD, atl::presetName(pi), S(294, 76, 324, 26), juce::Justification::centred, 18.f * u);
    g.setColour(vfd.withAlpha(0.7f)); g.setFont(lcdFont(10.f * u)); g.drawText(atl::presetTag(pi), S(294, 104, 324, 16).toNearestInt(), juce::Justification::centred);
    // niveaux
    auto mt = S(668, 60, 176, 62); screenWell(g, pal, mt);
    const float x0 = mt.getX() + 9 * u, w = mt.getWidth() - 18 * u;
    g.setFont(mono(8.f * u));
    auto row = [&](float y, const String& lab, float v, float lo, float hi, Colour c) {
        g.setColour(pal.ink.withAlpha(0.65f)); g.drawText(lab, Rectangle<float>(x0, mt.getY() + y * u, w, 9 * u), juce::Justification::centredLeft);
        oc::ui::bar(g, Rectangle<float>(x0, mt.getY() + (y + 10) * u, w, 6 * u), v, lo, hi, c, false);
    };
    row(5, u8("SORTIE dB"), outDb, -48.f, 0.f, kIce); row(32, u8("RÉDUCTION dB"), grDisp, 0.f, 12.f, kPeach);
    // modules : plaques teintées vissées (une teinte par famille de traitement)
    for (size_t mi = 0; mi < mods.size(); ++mi) {
        auto& m = mods[mi];
        auto r = S(m.panel.getX(), m.panel.getY(), m.panel.getWidth(), m.panel.getHeight());
        g.setColour(juce::Colours::black.withAlpha(0.55f)); g.fillRoundedRectangle(r.translated(0, 2.f * u).expanded(1.f), 3.f * u);
        g.setGradientFill(juce::ColourGradient(kModTint[mi].brighter(0.12f), r.getX(), r.getY(), kModTint[mi].darker(0.35f), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle(r, 3.f * u);
        g.setColour(juce::Colours::white.withAlpha(0.12f)); g.drawLine(r.getX() + 3 * u, r.getY() + 0.8f, r.getRight() - 3 * u, r.getY() + 0.8f, 1.f);
        g.setColour(juce::Colours::black.withAlpha(0.6f)); g.drawRoundedRectangle(r, 3.f * u, 1.f);
        screw(g, r.getRight() - 8.f * u, r.getY() + 8.f * u, 3.f * u, r.getX() * 0.05f, ScrewHead::Slot);
        g.setColour(juce::Colours::black.withAlpha(0.35f)); g.fillRect(S(m.panel.getX() + 6, m.panel.getY() + 26, m.panel.getWidth() - 12, 1.2f));
        g.setColour(juce::Colours::white.withAlpha(0.06f)); g.fillRect(S(m.panel.getX() + 6, m.panel.getY() + 27.2f, m.panel.getWidth() - 12, 1.f));
        size_t li = 0;
        for (auto& id : m.ids) {
            if (id == "@tonal") {
                engraved(g, pal, u8("Tonalité"), S(m.labels[li].getX(), m.labels[li].getY(), m.labels[li].getWidth(), m.labels[li].getHeight()), juce::Justification::centred, 8.4f * u, 1.1f * u, true); ++li;
                engraved(g, pal, u8("Mode"), S(m.labels[li].getX(), m.labels[li].getY(), m.labels[li].getWidth(), m.labels[li].getHeight()), juce::Justification::centred, 8.4f * u, 1.1f * u, true); ++li;
            } else if (id == "@dyDiv") {
                engraved(g, pal, u8("Division"), S(m.labels[li].getX(), m.labels[li].getY(), m.labels[li].getWidth(), m.labels[li].getHeight()), juce::Justification::centred, 8.4f * u, 1.1f * u, true); ++li;
            }
        }
    }
    // schéma du trajet du signal : voyants selon l'état des modules
    const int nf = (int)(sizeof(kFlow) / sizeof(kFlow[0])); const float step_ = (kW - 2 * kMargin) / (float)nf;
    auto fw = S(kMargin, kFlowY, kW - 2 * kMargin, 28); screenWell(g, pal, fw);
    for (int i = 0; i < nf; ++i) {
        const bool on = kFlowOn[i][0] == 0 || proc.flag(kFlowOn[i]) > 0.5f; const float cx = kMargin + step_ * ((float)i + 0.5f);
        kit::led(g, (cx - 28) * u, (kFlowY + 14) * u, 2.8f * u, on ? 1.f : 0.f, pal.ledAmber, LedBezel::None);
        g.setColour(on ? kIce.withAlpha(0.92f) : pal.ink.withAlpha(0.32f)); tracked(g, u8(kFlow[i]), S(cx - 22, kFlowY + 7, 60, 14), juce::Justification::centredLeft, 0.8f * u, fnt(8.6f * u, true));
        if (i < nf - 1) { g.setColour(pal.ink.withAlpha(0.35f)); g.setFont(fnt(10.f * u)); g.drawText(u8("›"), S(cx + 36, kFlowY + 7, 12, 14).toNearestInt(), juce::Justification::centred); }
    }
}
