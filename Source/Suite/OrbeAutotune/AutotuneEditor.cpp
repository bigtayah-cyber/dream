#include "AutotuneEditor.h"

using namespace kit;
using juce::Colour, juce::Rectangle, juce::String;

namespace {
constexpr float kW = 1000.f, kH = 548.f, kPotH = 82.f, kKnobR = 19.f;
using kit::w::kLilac, kit::w::kIce, kit::w::kPeach;
struct PotDef { const char* id; const char* label; const char* tip; float x; };
const PotDef kPots[] = {
    {"speed", "Vitesse", "Rapidit\xC3\xA9 de la correction vers la note. 0 = effet robot (saut imm\xC3\xA9" "diat), 10 \xC3\xA0 30 ms = tendance moderne, 100 ms et plus = naturel.", 24},
    {"tol", "Tol\xC3\xA9rance", "Zone morte autour de la note : tant que la voix en reste proche, elle n'est pas touch\xC3\xA9" "e.", 102},
    {"amt", "Quantit\xC3\xA9", "Part de la correction appliqu\xC3\xA9" "e. 100 % = pile sur la note, 0 % = aucune correction.", 180},
    {"trans", "Transposer", "D\xC3\xA9" "cale toute la voix en demi-tons (dt), apr\xC3\xA8s la correction.", 282},
    {"form", "Formants", "D\xC3\xA9" "place le timbre (les formants) en demi-tons : n\xC3\xA9gatif = voix plus grave, positif = plus fine.", 360},
    {"vd", "Vibrato", "Profondeur du vibrato ajout\xC3\xA9 (en cents). Il appara\xC3\xAEt en fondu apr\xC3\xA8s le d\xC3\xA9" "but de chaque note.", 462},
    {"vr", "Vit. vibrato", "Vitesse du vibrato, de 3 \xC3\xA0 8 Hz.", 540},
    {"hv", "Volume", "Niveau de la voix d'harmonie.", 677},
    {"dbl", "Doublage", "Deux voix l\xC3\xA9g\xC3\xA8rement d\xC3\xA9saccord\xC3\xA9" "es et d\xC3\xA9" "cal\xC3\xA9" "es, ouvertes en st\xC3\xA9r\xC3\xA9o.", 806},
    {"mix", "Mix", "Sec / trait\xC3\xA9 : 100 % = voix corrig\xC3\xA9" "e seule.", 898},
};
constexpr float kPotY = 450.f;
String nName(int midi) { return String(atn::noteNames()[atn::pcOf(midi)]) + String(midi / 12 - 1); }
}  // namespace

// ------------------------------------------------------------------ clavier de masque
int AtKeys::keyAt(juce::Point<float> p) const {
    const float kw = (float)getWidth() / 7.f, bh = (float)getHeight() * 0.6f;
    static const int blackPc[5] = {1, 3, 6, 8, 10};
    static const float blackX[5] = {1.f, 2.f, 4.f, 5.f, 6.f};
    if (p.y < bh) for (int i = 0; i < 5; ++i) if (std::fabs(p.x - blackX[i] * kw) < kw * 0.3f) return blackPc[i];
    static const int whitePc[7] = {0, 2, 4, 5, 7, 9, 11};
    return whitePc[juce::jlimit(0, 6, (int)(p.x / kw))];
}
void AtKeys::mouseDown(const juce::MouseEvent& e) {
    if (mode == 2) return;   // grille inconnue : rien à éditer
    proc.toggleNote(keyAt(e.position));
}
// Touches éclairées : note de la gamme = touche claire + LED ambre ; hors gamme = touche éteinte.
void AtKeys::paint(juce::Graphics& g) {
    const float w = (float)getWidth(), h = (float)getHeight(), kw = w / 7.f, bh = h * 0.6f, u = h / 48.f;
    auto bounds = getLocalBounds().toFloat();
    const auto pal0 = AutotuneEditor::palette();
    g.setColour(juce::Colours::black.withAlpha(0.75f)); g.fillRoundedRectangle(bounds.reduced(1.f), 3.f);
    static const int whitePc[7] = {0, 2, 4, 5, 7, 9, 11};
    static const int blackPc[5] = {1, 3, 6, 8, 10};
    static const float blackX[5] = {1.f, 2.f, 4.f, 5.f, 6.f};
    auto on = [&](int pc) { return ((mask >> pc) & 1) != 0; };
    const Colour amber(0xFFFFB02E);
    for (int i = 0; i < 7; ++i) {
        const int pc = whitePc[i];
        auto r = Rectangle<float>((float)i * kw + 1.5f, 3.f, kw - 3.f, h - 6.f);
        const bool lit = on(pc);
        g.setGradientFill(lit ? juce::ColourGradient(Colour(0xFFF3EEDF), r.getX(), r.getY(), Colour(0xFFCFC8B6), r.getX(), r.getBottom(), false)
                              : juce::ColourGradient(Colour(0xFF56565C), r.getX(), r.getY(), Colour(0xFF3A3A40), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle(r, 2.5f);
        g.setColour(juce::Colours::black.withAlpha(0.5f)); g.drawRoundedRectangle(r, 2.5f, 1.f);
        led(g, r.getCentreX(), r.getBottom() - 21.f * u, 2.6f * u, lit ? 1.f : 0.f, amber, LedBezel::None);
        g.setColour(lit ? Colour(0xFF1A1A22) : Colour(0xFFB5B3BD)); g.setFont(fnt(9.f * u, true));
        g.drawText(atn::noteNames()[pc], r.withTrimmedBottom(3.f * u), juce::Justification::centredBottom);
        if (pc == sung) { g.setColour(kIce); g.fillRoundedRectangle(r.getX() + 3.f * u, r.getBottom() - 3.5f * u, r.getWidth() - 6.f * u, 2.5f * u, 1.2f * u); }
        if (pc == root && mode != 2) { g.setColour(Colour(0xFFE0603A)); g.fillRoundedRectangle(r.getX() + 3.f * u, r.getY() + bh + 2.f * u, r.getWidth() - 6.f * u, 2.f * u, 1.f * u); }
    }
    for (int i = 0; i < 5; ++i) {
        const int pc = blackPc[i];
        auto r = Rectangle<float>(blackX[i] * kw - kw * 0.29f, 3.f, kw * 0.58f, bh);
        const bool lit = on(pc);
        g.setColour(juce::Colours::black.withAlpha(0.6f)); g.fillRoundedRectangle(r.translated(0, 2.f), 2.5f);
        g.setGradientFill(juce::ColourGradient(Colour(lit ? 0xFF2A2A30 : 0xFF141417), r.getX(), r.getY(), Colour(0xFF07070A), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle(r, 2.5f);
        g.setColour(juce::Colours::white.withAlpha(lit ? 0.25f : 0.08f)); g.drawRoundedRectangle(r, 2.5f, 1.f);
        led(g, r.getCentreX(), r.getY() + 7.f * u, 2.3f * u, lit ? 1.f : 0.f, amber, LedBezel::None);
        g.setColour(lit ? Colour(0xFFF4F1FF) : pal0.mute.withAlpha(0.7f));
        g.setFont(fnt(8.f * u, true)); g.drawText(atn::noteNames()[pc], r.withTrimmedBottom(2.f * u), juce::Justification::centredBottom);
        if (pc == sung) { g.setColour(kIce); g.fillRoundedRectangle(r.getX() + 2.f * u, r.getBottom() - 3.f * u, r.getWidth() - 4.f * u, 2.f * u, 1.f * u); }
        if (pc == root && mode != 2) { g.setColour(Colour(0xFFE0603A)); g.fillEllipse(r.getCentreX() - 2.5f * u, r.getY() + 13.f * u, 5.f * u, 5.f * u); }
    }
    if (mode == 2) {
        g.setColour(Colour(0xD00B0B10)); g.fillRoundedRectangle(bounds.reduced(1.f), 3.f);
        g.setColour(Colour(0xFFFFB02E));
        tracked(g, u8("Tonalit\xC3\xA9 en recherche"), bounds, juce::Justification::centred, 1.6f * u, fnt(9.4f * u, true));
    }
}

// ------------------------------------------------------------------ éditeur
Palette AutotuneEditor::palette() { return kit::w::rack(Colour(0xFFFF6A50)); }

AutotuneEditor::AutotuneEditor(AutotuneProcessor& p) : AudioProcessorEditor(&p), proc(p), pal(palette()), look(pal), keys(p), power(pal) {
    setLookAndFeel(&look);
    for (auto& d : kPots) { auto* pt = pots.add(new kit::w::Pot(proc.apvts, d.id, u8(d.label), u8(d.tip), nullptr, kKnobR, Colour(0xFFFF6A50))); pt->setStyle(KnobStyle::Fluted); addAndMakeVisible(pt); }
    // listes
    keyBox.addItem(u8("Auto (d'apr\xC3\xA8s la voix)"), 1);
    for (int i = 0; i < 12; ++i) keyBox.addItem(atn::noteNames()[i], i + 2);
    for (int i = 0; i < atn::kScales; ++i) scaleBox.addItem(u8(atn::scales()[i].name), i + 1);
    if (auto* hp = dynamic_cast<juce::AudioParameterChoice*>(proc.apvts.getParameter("harm"))) for (int i = 0; i < hp->choices.size(); ++i) harmBox.addItem(hp->choices[(int)i], i + 1);
    keyAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(proc.apvts, "key", keyBox);
    scaleAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(proc.apvts, "scale", scaleBox);
    harmAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(proc.apvts, "harm", harmBox);
    keyBox.onChange = [this] { proc.clearNotes(); };      // une nouvelle tonalité ou gamme repart d'une grille propre
    scaleBox.onChange = [this] { proc.clearNotes(); };
    keyBox.setTooltip(u8("Tonalit\xC3\xA9 du morceau. Auto : elle est estim\xC3\xA9" "e d'apr\xC3\xA8s ta voix, et tant qu'elle n'est pas s\xC3\xBBre la hauteur passe telle quelle."));
    scaleBox.setTooltip(u8("Gamme de correction. Les notes de la gamme sont allum\xC3\xA9" "es sur le clavier ; clique une touche pour l'ajouter ou la retirer."));
    harmBox.setTooltip(u8("Voix d'harmonie ajout\xC3\xA9" "e, calcul\xC3\xA9" "e dans la gamme (tierce, quinte) ou fixe (octaves)."));
    presetBox.setTooltip(u8("Point de d\xC3\xA9part : r\xC3\xA8gle vitesse, tol\xC3\xA9rance, formants, harmonie et doublage."));
    for (int i = 0; i < atn::kPresets; ++i) presetBox.addItem(u8(atn::presets()[i].name), i + 1);
    presetBox.addItem("Perso", 100);
    presetBox.onChange = [this] { const int id = presetBox.getSelectedId(); if (id >= 1 && id <= atn::kPresets) proc.setCurrentProgram(id - 1); };
    for (auto* c : {&keyBox, &scaleBox, &harmBox, &presetBox}) addAndMakeVisible(c);
    formToggle.setButtonText(u8("Formants pr\xC3\xA9serv\xC3\xA9s"));
    formToggle.setTooltip(u8("Garde le timbre de la voix quand la hauteur est d\xC3\xA9plac\xC3\xA9" "e (sinon le timbre suit la hauteur, effet chipmunk)."));
    addAndMakeVisible(formToggle);
    formAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, "formPres", formToggle);
    latSeg = std::make_unique<of::Seg>(proc.apvts, "latmode", juce::StringArray{"Direct", u8("Qualit\xC3\xA9")},
        u8("Direct : latence minimale (moins de 6 ms), correction rapide sans formants. Qualit\xC3\xA9 : formants pr\xC3\xA9serv\xC3\xA9s, latence d'environ 37 ms. Le choix change la latence d\xC3\xA9" "clar\xC3\xA9" "e \xC3\xA0 l'h\xC3\xB4te."), pal);
    addAndMakeVisible(*latSeg);
    resetNotes.setButtonText(u8("Notes \xC3\xA0 z\xC3\xA9ro"));
    resetNotes.setTooltip(u8("Annule les notes ajout\xC3\xA9" "es ou retir\xC3\xA9" "es \xC3\xA0 la main : retour \xC3\xA0 la gamme pure."));
    resetNotes.onClick = [this] { proc.clearNotes(); };
    addAndMakeVisible(resetNotes); addAndMakeVisible(keys); addAndMakeVisible(power);
    power.setTooltip(u8("Effet en service. Bascule pour le couper (la latence reste compens\xC3\xA9" "e)."));
    help.install(*this, "Orbe Autotune", {u8("Correction de hauteur en temps r\xC3\xA9" "el (d\xC3\xA9tection YIN), harmonie et doublage."),
        u8("- \xC3\x89" "cran : hauteur chant\xC3\xA9" "e (bleu), sortie corrig\xC3\xA9" "e (p\xC3\xAA" "che), note cible (blanc) ; les lignes claires sont les notes de la gamme."),
        u8("- Afficheur rouge : note chant\xC3\xA9" "e et \xC3\xA9" "cart en cents ; aiguille : correction appliqu\xC3\xA9" "e (\xC2\xB1" "50 ct)."),
        u8("- Clavier : touches allum\xC3\xA9" "es = notes de la gamme ; clic sur une touche pour l'ajouter ou la retirer. Notes \xC3\xA0 z\xC3\xA9ro : retour \xC3\xA0 la gamme pure."),
        u8("- Tonalit\xC3\xA9 Auto : estim\xC3\xA9" "e d'apr\xC3\xA8s la voix ; tant qu'elle n'est pas s\xC3\xBBre, la hauteur passe telle quelle."),
        u8("- Vitesse 0 = effet robot ; 10 \xC3\xA0 30 ms = moderne ; 100 ms et plus = naturel."),
        u8("- Latence : Direct (moins de 6 ms, sans formants) ou Qualit\xC3\xA9 (formants pr\xC3\xA9serv\xC3\xA9s, environ 37 ms) ; d\xC3\xA9" "clar\xC3\xA9" "e \xC3\xA0 l'h\xC3\xB4te.")});
    powerAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, "bypass", power);
    setResizable(true, true);
    setResizeLimits(600, (int)(600 * kH / kW), 1800, (int)(1800 * kH / kW));
    getConstrainer()->setFixedAspectRatio((double)kW / kH);
    setSize((int)kW, (int)kH);
    startTimerHz(30);
}
AutotuneEditor::~AutotuneEditor() { stopTimer(); setLookAndFeel(nullptr); }

void AutotuneEditor::resized() {
    u_ = (float)getWidth() / kW;
    const auto content = getLocalBounds().toFloat().reduced(18.f * u_, 0);
    power.setBounds(kit::w::inSlot(content, u_).toNearestInt());
    help.layout(kit::w::helpSlot(content, u_, true).toNearestInt(), getLocalBounds());
    for (int i = 0; i < pots.size(); ++i) { pots[i]->setBounds(Si(kPots[i].x, kPotY, 78, kPotH)); pots[i]->setScale(u_); }
    keyBox.setBounds(Si(770, 76, 206, 26)); scaleBox.setBounds(Si(770, 124, 206, 26));
    harmBox.setBounds(Si(642, 420, 148, 24)); presetBox.setBounds(Si(440, 11, 200, 28));
    formToggle.setBounds(Si(290, 420, 150, 20));
    latSeg->setBounds(Si(718, 9, 130, 32));
    keys.setBounds(Si(24, 338, 726, 52)); resetNotes.setBounds(Si(770, 356, 206, 32));
}

String AutotuneEditor::tonText() const {
    const auto& sc = atn::scales()[juce::jlimit(0, atn::kScales - 1, tScale)];
    if (tMode == 2) return u8("TON : \xE2\x80\x94 en recherche");
    const String name = (sc.n == 12 ? String() : String(atn::noteNames()[atn::pcOf(tRoot)]) + " ") + sc.tag + (tFlips ? " + notes" : "");
    return "TON : " + name + u8(" \xE2\x80\x94 ") + (tMode == 0 ? "manuel" : "auto");
}

void AutotuneEditor::timerCallback() {
    const auto& t = proc.tel();
    tMode = t.mode.load(); tRoot = t.root.load(); tScale = t.scale.load(); tFlips = t.flips.load(); tMask = t.mask.load();
    const int wr = t.wr.load(std::memory_order_acquire), N = atn::Telemetry::N;
    lastDet = lastTgt = 0.f;
    for (int i = 1; i <= 24; ++i) { const int k = ((wr - i) % N + N) % N; const float d = t.det[k].load(); if (d > 0.f) { lastDet = d; lastTgt = t.tgt[k].load(); break; } }
    dispCorr += (t.corr.load() - dispCorr) * 0.4f;
    keys.mask = tMask; keys.mode = tMode; keys.root = tMode == 2 ? -1 : atn::pcOf(tRoot);
    keys.sung = lastDet > 0.f ? atn::pcOf((int)std::lround(lastDet)) : -1;
    keys.repaint();
    // le menu des presets suit les réglages
    int sel = 100;
    for (int i = 0; i < atn::kPresets; ++i) if (proc.presetMatches(i)) { sel = i + 1; if (i == proc.getCurrentProgram()) break; }
    if (presetBox.getSelectedId() != sel) presetBox.setSelectedId(sel, juce::dontSendNotification);
    resetNotes.setEnabled(tFlips != 0);
    const int lm = juce::roundToInt(proc.raw("latmode")), latNow = proc.getLatencySamples();
    if (lm != lastLat || latNow != lastLatDecl) {   // formants : seulement en mode Qualité ; l'afficheur de latence suit
        lastLat = lm; lastLatDecl = latNow;
        formToggle.setEnabled(lm == 1); for (int i = 0; i < pots.size(); ++i) if (juce::String(kPots[i].id) == "form") pots[i]->setEnabled(lm == 1);
        repaint();
    }
    repaint(S(24, 58, 952, 280).toNearestInt());
}

void AutotuneEditor::module(juce::Graphics& g, const char* title, float x, float w, float y) {
    engraved(g, pal, u8(title), S(x + 4, y, w - 8, 12), juce::Justification::centredLeft, 8.8f * u_, 2.2f * u_, true);
    g.setColour(juce::Colours::black.withAlpha(0.5f)); g.fillRect(S(x, y + 16, w, 1.2f));
    g.setColour(juce::Colours::white.withAlpha(0.07f)); g.fillRect(S(x, y + 17.2f, w, 1.f));
}

void AutotuneEditor::paintScreen(juce::Graphics& g, Rectangle<float> r) {
    const float u = u_;
    screenWell(g, pal, r, Display::OLED);
    auto f = r.reduced(6 * u, 8 * u);
    auto p = f.withTrimmedLeft(30 * u);
    const auto& t = proc.tel();
    const int N = atn::Telemetry::N, wr = t.wr.load(std::memory_order_acquire);
    static float det[atn::Telemetry::N], tgt[atn::Telemetry::N], out[atn::Telemetry::N];
    int voiced = 0; float sum = 0.f;
    for (int i = 0; i < N; ++i) {
        const int k = (wr + i) % N;
        det[i] = t.det[k].load(); tgt[i] = t.tgt[k].load(); out[i] = t.out[k].load();
        if (det[i] > 0.f && i > N - 240) { sum += det[i]; ++voiced; }
    }
    if (voiced > 8) { const float mc = sum / (float)voiced; viewC += (mc - viewC) * (std::fabs(mc - viewC) > 5.f ? 0.5f : 0.08f); }
    const float range = 19.f;
    auto Y = [&](float m) { return p.getCentreY() - (m - viewC) * p.getHeight() / range; };
    auto X = [&](int i) { return p.getX() + p.getWidth() * (float)i / (float)(N - 1); };
    g.saveState();
    g.reduceClipRegion(f.toNearestInt());
    // lignes de notes : celles de la gamme sont allumées
    const int lo = (int)std::floor(viewC - range / 2) - 1, hi = (int)std::ceil(viewC + range / 2) + 1;
    g.setFont(mono(7.6f * u));
    for (int n = lo; n <= hi; ++n) {
        const int pc = atn::pcOf(n);
        const bool al = (tMask >> pc) & 1, isRoot = tMode != 2 && pc == atn::pcOf(tRoot);
        const float y = Y((float)n);
        g.setColour(isRoot && al ? kPeach.withAlpha(0.5f) : al ? kLilac.withAlpha(0.55f) : juce::Colours::white.withAlpha(0.035f));
        g.fillRect(p.getX(), y - 0.5f * u, p.getWidth(), isRoot && al ? 1.4f * u : 1.f * u);
        if (al || pc == 0) { g.setColour(al ? kLilac.withAlpha(0.85f) : pal.mute); g.drawText(nName(n), Rectangle<float>(f.getX(), y - 5 * u, 28 * u, 10 * u), juce::Justification::centredRight); }
    }
    // repères de temps
    const float total = (float)N * t.hopSec.load();
    g.setColour(juce::Colours::white.withAlpha(0.05f));
    for (float s = 0.5f; s < total; s += 0.5f) g.fillRect(p.getRight() - p.getWidth() * s / total, p.getY(), 1.f, p.getHeight());
    // bande de correction : trait vertical entre la hauteur chantée et la sortie
    g.setColour(kPeach.withAlpha(0.30f));
    for (int i = 0; i < N; ++i) if (det[i] > 0.f && out[i] > 0.f && std::fabs(out[i] - det[i]) > 0.01f) g.drawLine(X(i), Y(det[i]), X(i), Y(out[i]), juce::jmax(1.f, p.getWidth() / (float)N));
    auto line = [&](const float* v, Colour c, float th, bool glow) {
        juce::Path path; bool open = false; float prev = 0.f;
        for (int i = 0; i < N; ++i) {
            if (v[i] <= 0.f || (open && std::fabs(v[i] - prev) > 3.f)) { open = false; if (v[i] <= 0.f) continue; }
            if (!open) { path.startNewSubPath(X(i), Y(v[i])); open = true; } else path.lineTo(X(i), Y(v[i]));
            prev = v[i];
        }
        if (glow) { g.setColour(c.withAlpha(0.18f)); g.strokePath(path, juce::PathStrokeType(th * 3.f * u, juce::PathStrokeType::curved, juce::PathStrokeType::rounded)); }
        g.setColour(c); g.strokePath(path, juce::PathStrokeType(th * u, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    };
    line(tgt, Colour(0xFFF4F1FF).withAlpha(0.75f), 1.1f, false);
    line(det, kIce, 1.7f, true);
    line(out, kPeach, 1.5f, true);
    g.restoreState();
    if (voiced < 3) { g.setColour(pal.mute); tracked(g, "En attente de la voix", p, juce::Justification::centred, 2.f * u, fnt(10.f * u, true)); }
    // lecture TON et légende
    g.setColour(juce::Colours::black.withAlpha(0.55f)); g.fillRoundedRectangle(Rectangle<float>(f.getX() + 32 * u, f.getY() + 3 * u, 210 * u, 17 * u), 3 * u);
    g.setColour(tMode == 2 ? kPeach : kIce); g.setFont(mono(10.5f * u));
    g.drawText(tonText(), Rectangle<float>(f.getX() + 38 * u, f.getY() + 3 * u, 204 * u, 17 * u), juce::Justification::centredLeft);
    const char* leg[3] = {"CHANT\xC3\x89", "SORTIE", "CIBLE"};
    const Colour lc[3] = {kIce, kPeach, Colour(0xFFF4F1FF)};
    for (int i = 0; i < 3; ++i) {
        const float x = f.getRight() - (3 - i) * 62 * u;
        g.setColour(juce::Colours::black.withAlpha(0.55f)); g.fillRoundedRectangle(Rectangle<float>(x - 3 * u, f.getY() + 3 * u, 60 * u, 17 * u), 3 * u);
        g.setColour(lc[i]); g.fillRect(x + 2 * u, f.getY() + 11 * u, 10 * u, 2 * u);
        g.setColour(pal.mute); g.setFont(mono(7.4f * u)); g.drawText(u8(leg[i]), Rectangle<float>(x + 15 * u, f.getY() + 3 * u, 44 * u, 17 * u), juce::Justification::centredLeft);
    }
    g.setColour(pal.mute); g.setFont(mono(7.2f * u));
    g.drawText(String(juce::roundToInt(total)) + " s", Rectangle<float>(p.getX() + 2 * u, f.getBottom() - 11 * u, 40 * u, 10 * u), juce::Justification::centredLeft);
    g.drawText("maintenant", Rectangle<float>(p.getRight() - 60 * u, f.getBottom() - 11 * u, 58 * u, 10 * u), juce::Justification::centredRight);
}

void AutotuneEditor::paintGauge(juce::Graphics& g, Rectangle<float> r) {
    kit::w::dial(g, r, dispCorr, -50.f, 50.f, {{-50.f, -25.f, Colour(0xFFE0603A).withAlpha(0.8f)}, {25.f, 50.f, Colour(0xFFE0603A).withAlpha(0.8f)}},
                 {-50.f, -25.f, 0.f, 25.f, 50.f}, u8("Correction ct"), true);
}

// Afficheur rouge à 7 segments : note chantée (grande), écart en cents, note cible.
void AutotuneEditor::paintReadout(juce::Graphics& g, Rectangle<float> r) {
    const float u = u_;
    screenWell(g, pal, r, Display::Seg7);
    const auto ink = displayColours(Display::Seg7, pal).ink;
    const bool v = lastDet > 0.f;
    const int sungN = (int)std::lround(lastDet);
    const float dev = v ? (lastDet - (lastTgt > 0.f ? lastTgt : (float)sungN)) * 100.f : 0.f;
    g.setColour(ink.withAlpha(0.07f)); g.setFont(lcdFont(40.f * u)); g.drawText("888", Rectangle<float>(r.getX() + 8 * u, r.getY() + 4 * u, 100 * u, r.getHeight() - 8 * u), juce::Justification::centredLeft, false);
    g.setColour(ink); g.drawText(v ? nName(sungN) : String("--"), Rectangle<float>(r.getX() + 8 * u, r.getY() + 4 * u, 100 * u, r.getHeight() - 8 * u), juce::Justification::centredLeft, false);
    screenText(g, pal, Display::Seg7, v ? String::formatted("%+03d", juce::roundToInt(dev)) : String("---"), Rectangle<float>(r.getRight() - 92 * u, r.getY() + 8 * u, 84 * u, 30 * u), juce::Justification::centredRight, 24.f * u);
    g.setColour(ink.withAlpha(0.7f)); g.setFont(mono(8.f * u));
    g.drawText("CT", Rectangle<float>(r.getRight() - 30 * u, r.getY() + 40 * u, 22 * u, 10 * u), juce::Justification::centredRight);
    g.drawText(u8("CIBLE ") + (v && lastTgt > 0.f ? nName((int)lastTgt) : String("--")), Rectangle<float>(r.getRight() - 120 * u, r.getBottom() - 16 * u, 84 * u, 10 * u), juce::Justification::centredRight);
}

void AutotuneEditor::paint(juce::Graphics& g) {
    const float u = u_;
    kit::w::drawFace(g, pal, getLocalBounds(), u, {kit::w::Family::Rack, "Orbe Autotune", kit::w::serial::autotune});
    engraved(g, pal, "Preset", S(370, 11, 64, 28), juce::Justification::centredRight, 8.f * u, 2.f * u, true);
    engraved(g, pal, "Latence", S(650, 11, 62, 28), juce::Justification::centredRight, 8.f * u, 2.f * u, true);
    engraved(g, pal, u8("Tonalit\xC3\xA9"), S(770, 62, 200, 12), juce::Justification::centredLeft, 8.f * u, 2.f * u, true);
    engraved(g, pal, "Gamme", S(770, 110, 200, 12), juce::Justification::centredLeft, 8.f * u, 2.f * u, true);

    paintScreen(g, S(24, 62, 726, 268));
    paintReadout(g, S(770, 160, 206, 62));
    paintGauge(g, S(770, 230, 206, 100));
    engraved(g, pal, u8("Masque de notes"), S(770, 340, 206, 12), juce::Justification::centredLeft, 8.f * u, 1.6f * u, true);

    module(g, "Correction", 24, 234, 398);
    module(g, "Hauteur", 282, 156, 398);
    module(g, "Vibrato", 462, 156, 398);
    module(g, "Harmonie", 642, 148, 398);
    module(g, "Doublage", 806, 78, 398);
    module(g, "Sortie", 898, 78, 398);

}
