#include "AtelierEditor.h"

using namespace kit;
using juce::Colour, juce::Rectangle, juce::String;
using oc::ui::kLilac, oc::ui::kIce, oc::ui::kPeach;

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
    auto addCombo = [this](const String& id) {
        auto* cb = combos.add(new juce::ComboBox()); const auto items = atl::choices(id);
        for (int i = 0; i < items.size(); ++i) cb->addItem(items[i], i + 1);
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
        }
        m.nPots = pots.size() - m.firstPot;
    }
    for (auto* id : {"mix", "out"}) {
        const String s(id); const auto* d = atl::find(s);
        auto* pot = pots.add(new oc::ui::Pot(proc.apvts, s, u8(d->name), u8(d->tip), [s, this] { return s == "mix" ? 1.0 : AtelierProcessor::presetValue(s, proc.currentPreset()); }, 16.f, kIce));
        pot->setStyle(kit::KnobStyle::Cap); addAndMakeVisible(pot);
    }
    prev.onClick = [this] { step(-1); }; next.onClick = [this] { step(1); };
    prev.setTooltip(u8("Preset précédent")); next.setTooltip("Preset suivant");
    addAndMakeVisible(prev); addAndMakeVisible(next); addAndMakeVisible(power);
    power.setTooltip(u8("Effet en service. Bascule pour le couper (la latence reste compensée)."));
    help.install(*this, "Orbe Atelier", {u8("Console vocale modulaire : neuf modules dans l'ordre du trajet du signal (schéma en bas)."),
        u8("- Afficheur : clic pour la liste des presets, flèches pour défiler. ● MODIFIÉ : un réglage diffère du preset."),
        u8("- Chaque module a son interrupteur à voyant (allumé : module actif) ; le schéma du bas allume les étages en service."),
        u8("- Double-clic sur un potard : valeur du preset en cours."),
        u8("- Tune : Tonalité Auto = détection prudente, aucune correction tant qu'elle n'est pas sûre. Delay : division synchronisée au tempo de l'hôte ou temps libre.")});
    powerAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, "bypass", power);
    setResizable(true, true);
    setResizeLimits((int)kW, (int)kH, 2240, (int)(2240 * kH / kW));
    getConstrainer()->setFixedAspectRatio((double)kW / kH);
    setSize((int)kDefaultW, juce::roundToInt(kDefaultW * kH / kW));
    startTimerHz(30);
}
AtelierEditor::~AtelierEditor() { stopTimer(); setLookAndFeel(nullptr); }

void AtelierEditor::resized() {
    u_ = (float)getWidth() / kW; look.k = u_;
    const auto content = getLocalBounds().toFloat().reduced(18.f * u_, 0);
    power.setBounds(kit::w::inSlot(content, u_).toNearestInt());
    help.layout(kit::w::helpSlot(content, u_, true).toNearestInt(), getLocalBounds());
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
                    combos[cmbI]->setBounds(Si(cx + 3, kRowY[row] + 42, cw - 6, 22)); combos[cmbI + 1]->setBounds(Si(cx + 3, kRowY[row] + 82, cw - 6, 22));
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
    outDb = juce::jmax(20.f * std::log10(juce::jmax(1e-5f, proc.host.peak.load())), outDb - 1.0f);
    grDisp = juce::jmax(proc.host.reduction.load(), grDisp - 0.5f);
    modified = !proc.matchesPreset();
    repaint(S(250, 54, 600, 76).toNearestInt()); repaint(S(0, kFlowY - 4, kW, 40).toNearestInt());
}

void AtelierEditor::mouseUp(const juce::MouseEvent& e) {
    if (S(284, 58, 344, 66).contains(e.position) && !e.mouseWasDraggedSinceMouseDown()) showMenu();
}
void AtelierEditor::showMenu() {
    juce::PopupMenu m;
    for (int i = 0; i < atl::numPresets(); ++i) m.addItem(i + 1, atl::presetName(i) + u8("  ·  ") + atl::presetTag(i), true, i == proc.currentPreset());
    m.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(localAreaToGlobal(Si(284, 58, 344, 66))).withMinimumWidth(juce::roundToInt(344 * u_)),
                    [this](int r) { if (r > 0) proc.applyPreset(r - 1); });
}

void AtelierEditor::paint(juce::Graphics& g) {
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
