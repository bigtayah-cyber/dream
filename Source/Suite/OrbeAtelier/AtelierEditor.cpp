#include "AtelierEditor.h"

using namespace kit;
using juce::Colour, juce::Rectangle, juce::String;
using oc::ui::kLilac, oc::ui::kIce, oc::ui::kPeach;

namespace {
constexpr float kW = 1120.f, kH = 524.f, kR = 15.f, kPanelH = 114.f, kRowY[3] = {112.f, 234.f, 356.f}, kPad = 8.f, kGap = 8.f, kMargin = 12.f;
const char* const kFlow[] = {"Entrée", "Tune", "EQ", "Comp", "Sat", "De-ess", "Doubleur", "Delay", "Reverb", "Mix", "Limiteur", "Sortie"};
const char* const kFlowOn[] = {"", "tnOn", "eqOn", "cpOn", "saOn", "dsOn", "dbOn", "dyOn", "rvOn", "", "lmOn", ""};
}  // namespace

AtelierEditor::AtelierEditor(AtelierProcessor& p) : AudioProcessorEditor(&p), proc(p), pal(oc::ui::palette()), look(pal) {
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
            auto* pot = pots.add(new oc::ui::Pot(proc.apvts, id, u8(d->name), u8(d->tip), [id, this] { return AtelierProcessor::presetValue(id, proc.currentPreset()); }, kR));
            addAndMakeVisible(pot);
        }
        m.nPots = pots.size() - m.firstPot;
    }
    for (auto* id : {"mix", "out"}) {
        const String s(id); const auto* d = atl::find(s);
        auto* pot = pots.add(new oc::ui::Pot(proc.apvts, s, u8(d->name), u8(d->tip), [s, this] { return s == "mix" ? 1.0 : AtelierProcessor::presetValue(s, proc.currentPreset()); }, 16.f, kPeach));
        addAndMakeVisible(pot);
    }
    prev.onClick = [this] { step(-1); }; next.onClick = [this] { step(1); };
    prev.setTooltip(u8("Preset précédent")); next.setTooltip("Preset suivant");
    addAndMakeVisible(prev); addAndMakeVisible(next); addAndMakeVisible(power);
    power.setTooltip(u8("Effet en service. Appuie pour le couper (la latence reste compensée)."));
    powerAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, "bypass", power);
    setResizable(true, true);
    setResizeLimits(840, 390, 2240, 1048);
    getConstrainer()->setFixedAspectRatio((double)kW / kH);
    setSize((int)kW, (int)kH);
    startTimerHz(30);
}
AtelierEditor::~AtelierEditor() { stopTimer(); setLookAndFeel(nullptr); }

void AtelierEditor::resized() {
    u_ = (float)getWidth() / kW; look.k = u_;
    plate = brushedPlate(getWidth(), getHeight(), pal);
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
    pots[potI]->setScale(u_); pots[potI]->setBounds(Si(838, 8, 84, 92)); pots[potI + 1]->setScale(u_); pots[potI + 1]->setBounds(Si(928, 8, 84, 92));
    prev.setBounds(Si(262, 34, 18, 24)); next.setBounds(Si(630, 34, 18, 24));
    power.setBounds(Si(1040, 20, 58, 52));
}

void AtelierEditor::timerCallback() {
    outDb = juce::jmax(20.f * std::log10(juce::jmax(1e-5f, proc.host.peak.load())), outDb - 1.0f);
    grDisp = juce::jmax(proc.host.reduction.load(), grDisp - 0.5f);
    modified = !proc.matchesPreset();
    repaint(S(250, 8, 560, 80).toNearestInt()); repaint(S(0, 484, kW, 40).toNearestInt());
}

void AtelierEditor::mouseUp(const juce::MouseEvent& e) {
    if (S(286, 10, 340, 66).contains(e.position) && !e.mouseWasDraggedSinceMouseDown()) showMenu();
}
void AtelierEditor::showMenu() {
    juce::PopupMenu m;
    for (int i = 0; i < atl::numPresets(); ++i) m.addItem(i + 1, atl::presetName(i) + u8("  ·  ") + atl::presetTag(i), true, i == proc.currentPreset());
    m.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(localAreaToGlobal(Si(286, 10, 340, 66))).withMinimumWidth(juce::roundToInt(340 * u_)),
                    [this](int r) { if (r > 0) proc.applyPreset(r - 1); });
}

void AtelierEditor::paint(juce::Graphics& g) {
    const float u = u_;
    g.drawImageAt(plate, 0, 0);
    for (float sx : {12.f, 1108.f}) for (float sy : {12.f, 512.f}) screw(g, sx * u, sy * u, 4.6f * u, sx * 0.3f + sy);
    engraved(g, pal, "Orbe", S(34, 24, 120, 30), juce::Justification::centredLeft, 25.f * u, 5.f * u, true);
    g.setColour(kLilac); tracked(g, "Atelier", S(34, 56, 200, 24), juce::Justification::centredLeft, 6.f * u, fnt(19.f * u, true));
    engraved(g, pal, u8("Console vocale modulaire"), S(36, 82, 220, 12), juce::Justification::centredLeft, 8.4f * u, 1.8f * u);
    // preset
    auto scr = S(286, 10, 340, 66); screenWell(g, pal, scr);
    const int pi = proc.currentPreset();
    g.setColour(kIce.withAlpha(0.55f)); g.setFont(mono(9.4f * u));
    g.drawText(String(pi).paddedLeft('0', 2) + " / " + String(atl::numPresets() - 1), S(300, 16, 80, 12).toNearestInt(), juce::Justification::centredLeft);
    if (modified) { kit::led(g, 604 * u, 22 * u, 3.f * u, true, kPeach); g.setColour(kPeach.withAlpha(0.8f)); g.setFont(mono(8.4f * u)); g.drawText(u8("modifié"), S(520, 16, 76, 12).toNearestInt(), juce::Justification::centredRight); }
    g.setColour(kIce); g.setFont(fnt(24.f * u, true)); g.drawText(atl::presetName(pi), S(296, 28, 320, 30).toNearestInt(), juce::Justification::centred);
    g.setColour(pal.ink.withAlpha(0.7f)); g.setFont(fnt(11.f * u)); g.drawText(atl::presetTag(pi), S(296, 55, 320, 16).toNearestInt(), juce::Justification::centred);
    // niveaux
    auto mt = S(668, 14, 150, 62); screenWell(g, pal, mt);
    const float x0 = mt.getX() + 9 * u, w = mt.getWidth() - 18 * u;
    g.setFont(mono(7.4f * u));
    auto row = [&](float y, const String& lab, float v, float lo, float hi, Colour c) {
        g.setColour(pal.ink.withAlpha(0.6f)); g.drawText(lab, Rectangle<float>(x0, mt.getY() + y * u, w, 9 * u), juce::Justification::centredLeft);
        oc::ui::bar(g, Rectangle<float>(x0, mt.getY() + (y + 10) * u, w, 6 * u), v, lo, hi, c, false);
    };
    row(5, u8("SORTIE dB"), outDb, -48.f, 0.f, kIce); row(32, u8("RÉDUCTION dB"), grDisp, 0.f, 12.f, kPeach);
    // modules
    for (auto& m : mods) {
        auto r = S(m.panel.getX(), m.panel.getY(), m.panel.getWidth(), m.panel.getHeight());
        g.setColour(juce::Colours::black.withAlpha(0.26f)); g.fillRoundedRectangle(r, 6.f * u);
        g.setColour(juce::Colours::white.withAlpha(0.07f)); g.drawRoundedRectangle(r.reduced(0.5f), 6.f * u, 1.f);
        g.setColour(juce::Colours::black.withAlpha(0.35f)); g.fillRect(S(m.panel.getX() + 6, m.panel.getY() + 26, m.panel.getWidth() - 12, 1.2f));
        g.setColour(juce::Colours::white.withAlpha(0.06f)); g.fillRect(S(m.panel.getX() + 6, m.panel.getY() + 27.2f, m.panel.getWidth() - 12, 1.f));
        size_t li = 0;
        for (auto& id : m.ids) {
            if (id == "@tonal") {
                engraved(g, pal, u8("Tonalité"), S(m.labels[li].getX(), m.labels[li].getY(), m.labels[li].getWidth(), m.labels[li].getHeight()), juce::Justification::centred, 8.2f * u, 1.1f * u, true); ++li;
                engraved(g, pal, u8("Mode"), S(m.labels[li].getX(), m.labels[li].getY(), m.labels[li].getWidth(), m.labels[li].getHeight()), juce::Justification::centred, 8.2f * u, 1.1f * u, true); ++li;
            } else if (id == "@dyDiv") {
                engraved(g, pal, u8("Division"), S(m.labels[li].getX(), m.labels[li].getY(), m.labels[li].getWidth(), m.labels[li].getHeight()), juce::Justification::centred, 8.2f * u, 1.1f * u, true); ++li;
            }
        }
    }
    // schéma du trajet du signal : voyants selon l'état des modules
    const int nf = (int)(sizeof(kFlow) / sizeof(kFlow[0])); const float step_ = (kW - 2 * kMargin) / (float)nf;
    g.setColour(juce::Colours::black.withAlpha(0.3f)); g.fillRoundedRectangle(S(kMargin, 482, kW - 2 * kMargin, 28), 5.f * u);
    for (int i = 0; i < nf; ++i) {
        const bool on = kFlowOn[i][0] == 0 || proc.flag(kFlowOn[i]) > 0.5f; const float cx = kMargin + step_ * ((float)i + 0.5f);
        kit::led(g, (cx - 26) * u, 496 * u, 2.6f * u, on, kLilac);
        g.setColour(on ? kIce.withAlpha(0.9f) : pal.ink.withAlpha(0.3f)); tracked(g, u8(kFlow[i]), S(cx - 20, 489, 56, 14), juce::Justification::centredLeft, 0.8f * u, fnt(8.2f * u, true));
        if (i < nf - 1) { g.setColour(pal.ink.withAlpha(0.3f)); g.setFont(fnt(9.f * u)); g.drawText(u8("›"), S(cx + 36, 489, 12, 14).toNearestInt(), juce::Justification::centred); }
    }
}
