#include "AutotuneEditor.h"

using namespace kit;
using juce::Colour, juce::Rectangle, juce::String;
namespace v3 = kit::v3;

namespace {
constexpr float kW = 1000.f, kH = 548.f, kPotH = 82.f, kKnobR = 19.f;
using kit::w::kLilac, kit::w::kIce, kit::w::kPeach;
struct PotDef { const char* id; const char* label; const char* tip; float x; };
const PotDef kPots[] = {
    {"speed", "Vitesse", "Rapidit\xC3\xA9 de la correction vers la note. 0 = effet robot (saut imm\xC3\xA9" "diat), 10 \xC3\xA0 30 ms = tendance moderne, 100 ms et plus = naturel.", 24},
    {"tol", "Tol\xC3\xA9rance", "Zone morte autour de la note : tant que la voix en reste proche, elle n'est pas touch\xC3\xA9" "e.", 94},
    {"amt", "Quantit\xC3\xA9", "Part de la correction appliqu\xC3\xA9" "e. 100 % = pile sur la note, 0 % = aucune correction.", 164},
    {"expr", "Expression", "Moteur Orbe : part du vibrato et des inflexions gard\xC3\xA9" "e quand la vitesse est naturelle (50 ms et plus). \xC3\x80 vitesse 0, tout est aplati (robot).", 234},
    {"trans", "Transposer", "D\xC3\xA9" "cale toute la voix en demi-tons (dt), apr\xC3\xA8s la correction.", 320},
    {"form", "Formants", "D\xC3\xA9" "place le timbre (les formants) en demi-tons : n\xC3\xA9gatif = voix plus grave, positif = plus fine.", 390},
    {"vd", "Vibrato", "Profondeur du vibrato ajout\xC3\xA9 (en cents). Il appara\xC3\xAEt en fondu apr\xC3\xA8s le d\xC3\xA9" "but de chaque note.", 476},
    {"vr", "Vit. vibrato", "Vitesse du vibrato, de 3 \xC3\xA0 8 Hz.", 546},
    {"hv", "Volume", "Niveau de la voix d'harmonie.", 668},
    {"dbl", "Doublage", "Deux voix l\xC3\xA9g\xC3\xA8rement d\xC3\xA9saccord\xC3\xA9" "es et d\xC3\xA9" "cal\xC3\xA9" "es, ouvertes en st\xC3\xA9r\xC3\xA9o.", 806},
    {"mix", "Mix", "Sec / trait\xC3\xA9 : 100 % = voix corrig\xC3\xA9" "e seule.", 898},
};
constexpr float kPotY = 450.f;
String noteLabel(int midi, bool fl) { return String(ned::noteName(atn::pcOf(midi), fl)) + String((int)std::floor(midi / 12.0) - 1); }
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
    if (v3::bridge::v3(T)) { paint3(g); return; }
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
        g.drawText(ned::noteName(pc, flats), r.withTrimmedBottom(3.f * u), juce::Justification::centredBottom);
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
        g.setFont(fnt(8.f * u, true)); g.drawText(ned::noteName(pc, flats), r.withTrimmedBottom(2.f * u), juce::Justification::centredBottom);
        if (pc == sung) { g.setColour(kIce); g.fillRoundedRectangle(r.getX() + 2.f * u, r.getBottom() - 3.f * u, r.getWidth() - 4.f * u, 2.f * u, 1.f * u); }
        if (pc == root && mode != 2) { g.setColour(Colour(0xFFE0603A)); g.fillEllipse(r.getCentreX() - 2.5f * u, r.getY() + 13.f * u, 5.f * u, 5.f * u); }
    }
    if (mode == 2) {
        g.setColour(Colour(0xD00B0B10)); g.fillRoundedRectangle(bounds.reduced(1.f), 3.f);
        g.setColour(Colour(0xFFFFB02E));
        tracked(g, u8("Tonalit\xC3\xA9 en recherche"), bounds, juce::Justification::centred, 1.6f * u, fnt(9.4f * u, true));
    }
}

// Clavier v3 : touches blanches en cartes, point ambre = note de la gamme, soulignés bleu (chantée) et accent (tonique).
void AtKeys::paint3(juce::Graphics& g) {
    const auto& Tm = *T;
    const float w = (float)getWidth(), h = (float)getHeight(), kw = w / 7.f, u = h / 59.f;
    static const int whitePc[7] = {0, 2, 4, 5, 7, 9, 11};
    static const int blackPc[5] = {1, 3, 6, 8, 10};
    static const float blackX[5] = {1.f, 2.f, 4.f, 5.f, 6.f};
    auto on = [&](int pc) { return ((mask >> pc) & 1) != 0; };
    const Colour amber(0xFFF2A516);
    for (int i = 0; i < 7; ++i) {
        const int pc = whitePc[i];
        auto r = Rectangle<float>((float)i * kw + 2.f * u, 1.f, kw - 4.f * u, h - 3.f);
        g.setColour(Tm.shadow.withMultipliedAlpha(0.5f)); g.fillRoundedRectangle(r.translated(0, 1.5f), 10.f * u);
        g.setColour(Tm.card); g.fillRoundedRectangle(r, 10.f * u);
        g.setColour(Tm.cardEdge); g.drawRoundedRectangle(r.reduced(0.5f), 10.f * u, 1.f);
        if (on(pc)) { g.setColour(amber); g.fillEllipse(Rectangle<float>(8.f * u, 8.f * u).withCentre({r.getCentreX(), r.getY() + 23.f * u})); }
        g.setColour(on(pc) ? Tm.ink : Tm.inkMute); g.setFont(v3::font(14.f * u, true));
        g.drawText(ned::noteName(pc, flats), r.withTrimmedBottom(8.f * u).withTrimmedTop(30.f * u), juce::Justification::centred, false);
        auto bar = Rectangle<float>(r.getWidth() * 0.46f, 3.f * u).withCentre({r.getCentreX(), r.getBottom() - 4.f * u});
        if (pc == sung) { g.setColour(Tm.mod); g.fillRoundedRectangle(bar, 1.5f * u); }
        else if (pc == root && mode != 2) { g.setColour(Tm.accent); g.fillRoundedRectangle(bar, 1.5f * u); }
    }
    for (int i = 0; i < 5; ++i) {
        const int pc = blackPc[i];
        auto r = Rectangle<float>(kw * 0.38f, h * 0.56f).withCentre({blackX[i] * kw, h * 0.28f + 1.f});
        g.setColour(juce::Colours::black.withAlpha(0.25f)); g.fillRoundedRectangle(r.translated(0, 1.5f), 7.f * u);
        g.setColour(on(pc) ? Colour(0xFF2A2F3C) : Colour(0xFF161920)); g.fillRoundedRectangle(r, 7.f * u);
        if (Tm.dark) { g.setColour(juce::Colours::white.withAlpha(0.16f)); g.drawRoundedRectangle(r.reduced(0.5f), 7.f * u, 1.f); }
        g.setColour(on(pc) ? Colour(0xFFF2F3F6) : Colour(0xFFA9AFBD)); g.setFont(v3::font(12.f * u, true));
        g.drawText(ned::noteName(pc, flats), r, juce::Justification::centred, false);
        if (on(pc)) { g.setColour(amber); g.fillEllipse(Rectangle<float>(5.f * u, 5.f * u).withCentre({r.getCentreX(), r.getY() + 6.f * u})); }
        auto bar = Rectangle<float>(r.getWidth() * 0.6f, 2.5f * u).withCentre({r.getCentreX(), r.getBottom() - 4.f * u});
        if (pc == sung) { g.setColour(Tm.sung); g.fillRoundedRectangle(bar, 1.2f * u); }
        else if (pc == root && mode != 2) { g.setColour(Tm.accent); g.fillRoundedRectangle(bar, 1.2f * u); }
    }
    if (mode == 2) {
        g.setColour(Tm.card.withAlpha(0.82f)); g.fillRoundedRectangle(getLocalBounds().toFloat(), 10.f * u);
        g.setColour(Tm.accentText); v3::caps(g, u8("Tonalit\xC3\xA9 en recherche"), getLocalBounds().toFloat(), 13.f * u, juce::Justification::centred);
    }
}

// ------------------------------------------------------------------ éditeur
Palette AutotuneEditor::palette() { return kit::w::rack(Colour(0xFFFF6A50)); }

AutotuneEditor::AutotuneEditor(AutotuneProcessor& p) : AudioProcessorEditor(&p), proc(p), pal(palette()), look(pal), keys(p), power(pal) {
    setLookAndFeel(&look);
    for (auto& d : kPots) { auto* pt = pots.add(new kit::w::Pot(proc.apvts, d.id, u8(d.label), u8(d.tip), nullptr, kKnobR, Colour(0xFFFF6A50))); pt->setStyle(KnobStyle::Fluted); addAndMakeVisible(pt); }
    // listes
    keyBox.addItem(u8("Auto (d'apr\xC3\xA8s la voix)"), 1);
    for (int i = 0; i < 12; ++i) keyBox.addItem(ned::noteName(i, proc.flatsPref()), i + 2);
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
    latSeg = std::make_unique<v3::bridge::Seg>(proc.apvts, "latmode", juce::StringArray{"Direct", u8("Qualit\xC3\xA9")},
        u8("Direct : latence minimale (moins de 6 ms) ; formants pr\xC3\xA9serv\xC3\xA9s avec le moteur Orbe, pas avec Classique. Qualit\xC3\xA9 : analyse anticip\xC3\xA9" "e et d\xC3\xA9placement des formants, latence d'environ 37 ms. Le choix change la latence d\xC3\xA9" "clar\xC3\xA9" "e \xC3\xA0 l'h\xC3\xB4te."), pal);
    addAndMakeVisible(*latSeg);
    algoSeg = std::make_unique<v3::bridge::Seg>(proc.apvts, "algo", juce::StringArray{"Classique", "Orbe"},
        u8("Moteur de correction. Orbe : d\xC3\xA9tection plus s\xC3\xBBre, vibrato gard\xC3\xA9 en r\xC3\xA9glage naturel, formants pr\xC3\xA9serv\xC3\xA9s m\xC3\xAAme en Direct, consonnes et souffle intacts, \xC3\xA9" "dition note par note. Classique : l'ancien moteur (les anciens projets le gardent)."), pal);
    addAndMakeVisible(*algoSeg);
    accSeg = std::make_unique<v3::bridge::Seg>(proc.apvts, "accid", juce::StringArray{"#", u8("\xE2\x99\xAD")},
        u8("\xC3\x89" "criture des notes : di\xC3\xA8ses ou b\xC3\xA9mols. Quand la tonalit\xC3\xA9 est connue, l'armure d\xC3\xA9" "cide (F majeur : Bb, G majeur : F#)."), pal);
    addAndMakeVisible(*accSeg);
    renduSeg = std::make_unique<v3::bridge::Seg>(proc.apvts, "rendu", juce::StringArray{"Satin", u8("Pr\xC3\xA9" "cis"), "Robot"},
        u8("Rendu. Satin : corrige le centre des notes et garde ton vibrato, glissement doux entre les notes, aucun effet de modulation (Mix dose alors la correction). Pr\xC3\xA9" "cis : comportement d'origine, suit chaque micro-variation. Robot : correction instantan\xC3\xA9" "e."), pal);
    addAndMakeVisible(*renduSeg);
    editKey.setButtonText(u8("\xC3\x89" "DITION"));
    editKey.setClickingTogglesState(true);
    editKey.setTooltip(u8("Moteur Orbe : \xC3\xA9" "dition note par note d'un passage captur\xC3\xA9 pendant la lecture (ARMER, puis lance la lecture)."));
    editKey.onClick = [this] { roll->setVisible(editKey.getToggleState()); if (roll->isVisible()) { roll->fitView(); roll->grabKeyboardFocus(); } repaint(); };
    addAndMakeVisible(editKey);
    roll = std::make_unique<NoteRoll>(proc, pal);
    addChildComponent(*roll);
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
        u8("- Latence : Direct (moins de 6 ms) ou Qualit\xC3\xA9 (analyse anticip\xC3\xA9" "e, environ 37 ms) ; d\xC3\xA9" "clar\xC3\xA9" "e \xC3\xA0 l'h\xC3\xB4te."),
        u8("- Moteur Orbe : vibrato gard\xC3\xA9 en r\xC3\xA9glage naturel (potard Expression), consonnes et souffle intacts, formants pr\xC3\xA9serv\xC3\xA9s."),
        u8("- \xC3\x89" "DITION (Orbe) : ARMER puis lancer la lecture capture le passage ; glisser une note = la d\xC3\xA9placer (Maj : au cent pr\xC3\xA8s), fl\xC3\xA8" "ches haut/bas = 1 demi-ton (Maj : 10 cents), double-clic = caler sur la note, Alt-clic = couper, M = fusionner avec la suivante, Suppr = note \xC3\xA0 z\xC3\xA9ro, Ctrl+Z / Ctrl+Maj+Z.")});
    powerAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, "bypass", power);
    // --- v3 : potards haut de gamme (mêmes paramètres, attachements propres), sélecteur de thème
    for (auto& d : kPots) {
        auto* k = k3.add(new v3::Knob3(proc.apvts, d.id, u8(d.label), u8(d.tip), v3::KnobSize::M));
        k->setLabelPx(11.5f);
        const String id(d.id);
        k->setBipolar(id == "trans" || id == "form");
        addChildComponent(k);
    }
    {   // comportement (P1/P2) : mêmes attachements apvts, v3 seulement
        static const PotDef kBeh[] = {
            {"warb", "Warble", "Bascule de note sur un vibrato \xC3\xA0 cheval entre deux notes. 100 % = libre (effet warble des rappeurs), 0 % = la note ne bascule plus sur le vibrato.", 0},
            {"human", "Humanize", "Plus la note dure, plus la correction se rel\xC3\xA2" "che (jusqu'\xC3\xA0 +120 ms) : les notes br\xC3\xA8ves restent dures, les tenues respirent.", 0},
            {"tspd", "Transition", "Vitesse de la correction juste apr\xC3\xA8s un changement de note (ms). 0 = celle de Vitesse.", 0},
            {"ovs", "Vitesse < 0", "D\xC3\xA9passe la nouvelle note de quelques cents au changement, puis revient : escalier plus claqu\xC3\xA9, au-del\xC3\xA0 de la vitesse 0.", 0},
            {"track", "Suivi", "D\xC3\xA9tection de la hauteur : serr\xC3\xA9 = voix nette ; souple = voix souffl\xC3\xA9" "e ou chuchot\xC3\xA9" "e mieux suivie (un peu plus de faux positifs).", 0},
        };
        for (auto& d : kBeh) { auto* k = kb.add(new v3::Knob3(proc.apvts, d.id, u8(d.label), u8(d.tip), v3::KnobSize::M)); k->setLabelPx(11.5f); addChildComponent(k); }
        stabSeg = std::make_unique<v3::bridge::Seg>(proc.apvts, "stab", juce::StringArray{"Off", "40", "80", "200"},
            u8("Stabilisateur de notes : une note nouvelle doit tenir 40, 80 ou 200 ms avant d'\xC3\xAAtre adopt\xC3\xA9" "e (Direct : 40 ms au plus). Ignore les notes parasites d'un vibrato ou d'un souffle."), pal);
        atkSeg = std::make_unique<v3::bridge::Seg>(proc.apvts, "attack", juce::StringArray{"Non", "Oui"},
            u8("Attaque accord\xC3\xA9" "e (Direct, vitesse dure) : la correction est pos\xC3\xA9" "e d\xC3\xA8s les premi\xC3\xA8res ms d'une note, sans rampe."), pal);
        addChildComponent(*stabSeg); addChildComponent(*atkSeg);
    }
    addAndMakeVisible(themeBtn);
    themeBtn.onTheme = [this](v3::Mode m) { setThemeMode(m, true); };
    themeBtn.onScale = [this](float s) { setUiScale(s); };
    setResizable(true, true);
    scale_ = v3::Prefs::scale("autotune", 1.f);
    setThemeMode(v3::Prefs::theme("autotune", v3::Mode::Light), false);
    startTimerHz(30);
}
AutotuneEditor::~AutotuneEditor() { stopTimer(); v3::Prefs::setScale("autotune", scale_); roll->setLookAndFeel(nullptr); setLookAndFeel(nullptr); }

void AutotuneEditor::setThemeMode(v3::Mode m, bool persist) {
    mode = m;
    T = v3::make(m == v3::Mode::Classic ? v3::Mode::Light : m, v3::accents::orange);
    T.mode = m;
    look3.setTheme(T);
    { auto ts = v3::make(v3::Mode::Dark, v3::accents::orange); look3Screen.setTheme(ts); }
    art.clear();
    const bool n3 = isV3();
    setLookAndFeel(n3 ? static_cast<juce::LookAndFeel*>(&look3) : static_cast<juce::LookAndFeel*>(&look));
    roll->setLookAndFeel(n3 ? &look3Screen : nullptr);
    for (auto* p : pots) p->setVisible(!n3);
    for (auto* k : k3) { k->setVisible(n3); k->setup(&T, &art); }
    for (auto* k : kb) { k->setVisible(n3); k->setup(&T, &art); }
    for (auto* sg : {stabSeg.get(), atkSeg.get()}) { sg->setVisible(n3); sg->T = n3 ? &T : nullptr; sg->repaint(); }
    const v3::Theme* tp = n3 ? &T : nullptr;
    for (auto* sg : {latSeg.get(), algoSeg.get(), accSeg.get(), renduSeg.get()}) { sg->T = tp; sg->repaint(); }
    editKey.T = resetNotes.T = tp; power.T = tp; help.T = tp; keys.T = tp; roll->setTheme(tp);
    themeBtn.T = &T; themeBtn.current = m;
    if (persist) v3::Prefs::setTheme("autotune", m);
    const float bw = n3 ? kW3 : kW, bh = n3 ? kH3 : kH, hiS = n3 ? 1.5f : 1.8f;
    setResizeLimits(juce::roundToInt(bw * 0.6f), juce::roundToInt(bh * 0.6f), juce::roundToInt(bw * hiS), juce::roundToInt(bh * hiS));
    getConstrainer()->setFixedAspectRatio(bw / bh);
    scale_ = juce::jlimit(n3 ? 0.75f : 0.6f, hiS, scale_);
    const int w = juce::roundToInt(bw * scale_), h = juce::roundToInt(bh * scale_);
    if (getWidth() == w && getHeight() == h) resized(); else setSize(w, h);
    sendLookAndFeelChange();
    repaint();
}

void AutotuneEditor::setUiScale(float s) {
    const bool n3 = isV3();
    scale_ = juce::jlimit(n3 ? 0.75f : 0.6f, n3 ? 1.5f : 1.8f, s);
    setSize(juce::roundToInt((n3 ? kW3 : kW) * scale_), juce::roundToInt((n3 ? kH3 : kH) * scale_));
    v3::Prefs::setScale("autotune", scale_);
}

void AutotuneEditor::resized() {
    if (isV3()) resized3(); else resizedClassic();
    themeBtn.scale = scale_;
}

void AutotuneEditor::resizedClassic() {
    u_ = (float)getWidth() / kW;
    scale_ = u_;
    const auto content = getLocalBounds().toFloat().reduced(18.f * u_, 0);
    power.setBounds(kit::w::inSlot(content, u_).toNearestInt());
    help.layout(kit::w::helpSlot(content, u_, true).toNearestInt(), getLocalBounds());
    themeBtn.setBounds(help.getBounds().translated(-help.getWidth() - juce::roundToInt(6.f * u_), 0));
    for (int i = 0; i < pots.size(); ++i) pots[i]->setScale(u_);
    for (int i = 0; i < pots.size(); ++i) pots[i]->setBounds(Si(kPots[i].x - 3, kPotY, 72, kPotH));
    keyBox.setBounds(Si(770, 76, 140, 26)); accSeg->setBounds(Si(916, 76, 60, 26)); scaleBox.setBounds(Si(770, 124, 206, 26));
    harmBox.setBounds(Si(632, 420, 146, 24)); presetBox.setBounds(Si(440, 11, 200, 28));
    formToggle.setBounds(Si(320, 420, 142, 20));
    latSeg->setBounds(Si(718, 9, 130, 32));
    algoSeg->setBounds(Si(238, 9, 124, 32));
    renduSeg->setBounds(Si(770, 228, 206, 26));
    editKey.setBounds(Si(24, 338, 90, 52));
    roll->u = u_; roll->setBounds(Si(24, 62, 726, 268)); roll->resized();
    keys.setBounds(Si(122, 338, 628, 52)); resetNotes.setBounds(Si(770, 356, 206, 32));
}

// ============================================================ v3 « hybride v2 » : coordonnées de conception 1100 × 706
namespace {
constexpr float kSlot = 88.f, kRow0 = 30.f;
// x de début de chaque section (Correction, Hauteur, Vibrato, Harmonie, Doublage, Sortie) et fin
const float kSecX[7] = {kRow0, kRow0 + 3 * kSlot, kRow0 + 5 * kSlot, kRow0 + 8 * kSlot, kRow0 + 8 * kSlot + 150.f, kRow0 + 8 * kSlot + 243.f, 1070.f};
// potard i -> centre x (ordre de kPots : speed tol amt expr | trans form | vd vr | hv | dbl | mix)
float potX(int i) {
    static const float xs[11] = {kRow0 + 0.5f * kSlot, kRow0 + 1.5f * kSlot, kRow0 + 2.5f * kSlot, kRow0 + 5.5f * kSlot,
                                 kRow0 + 3.5f * kSlot, kRow0 + 4.5f * kSlot, kRow0 + 6.5f * kSlot, kRow0 + 7.5f * kSlot,
                                 (kSecX[3] + kSecX[4]) * 0.5f, (kSecX[4] + kSecX[5]) * 0.5f, (kSecX[5] + kSecX[6]) * 0.5f};
    return xs[juce::jlimit(0, 10, i)];
}
}

void AutotuneEditor::resized3() {
    u_ = (float)getWidth() / kW3;
    scale_ = u_;
    help.layout(Si(976, 34, 30, 30), getLocalBounds());
    themeBtn.setBounds(Si(942, 35, 28, 28));
    power.setBounds(Si(1012, 36, 70, 26));
    algoSeg->setBounds(Si(300, 34, 170, 32)); algoSeg->px = 12.f * u_;
    presetBox.setBounds(Si(488, 34, 250, 32));
    latSeg->setBounds(Si(758, 34, 172, 32)); latSeg->px = 12.f * u_;
    keyBox.setBounds(Si(800, 96, 214, 34)); accSeg->setBounds(Si(1020, 96, 60, 34)); accSeg->px = 12.f * u_;
    scaleBox.setBounds(Si(800, 156, 280, 34));
    roll->u = u_ * 1.05f; roll->setBounds(Si(20, 77, 763, 316)); roll->resized();
    editKey.setBounds(Si(20, 407, 92, 59));
    keys.setBounds(Si(126, 407, 657, 59));
    resetNotes.setBounds(Si(800, 427, 280, 40));
    renduSeg->setBounds(Si(800, 397, 280, 26)); renduSeg->px = 10.f * u_;
    const float kh = v3::Knob3::boxHeight(v3::KnobSize::M, false);
    for (int i = 0; i < k3.size(); ++i) { k3[i]->setScale(u_); k3[i]->setBounds(Si(potX(i) - kSlot * 0.5f, 556, kSlot, kh)); }
    {   // rangée « Comportement » : stabilisateur et attaque à gauche, cinq potards ensuite
        for (int i = 0; i < kb.size(); ++i) { kb[i]->setScale(u_); kb[i]->setBounds(Si(30.f + 260.f + (float)i * kSlot, 726, kSlot, kh)); }
        stabSeg->setBounds(Si(42, 754, 200, 28)); stabSeg->px = 11.f * u_;
        atkSeg->setBounds(Si(42, 812, 120, 28)); atkSeg->px = 11.f * u_;
    }
    formToggle.setBounds(Si(kSecX[1] + 12, 525, 170, 22));
    harmBox.setBounds(Si(kSecX[3] + 8, 522, 134, 28));
}

void AutotuneEditor::paintBackdrop3(juce::Graphics& g, Rectangle<float> b) {
    const float u = b.getWidth() / kW3;
    auto R = [u](float x, float y, float w, float h) { return Rectangle<float>(x * u, y * u, w * u, h * u); };
    v3::background(g, T, b);
    g.setColour(T.inkMute); v3::caps(g, "dreamaws", R(29, 14, 160, 16), 12.f * u, juce::Justification::centredLeft);
    const auto big = v3::font(30.f * u, true);
    g.setColour(T.ink); tracked(g, "Orbe", R(28, 30, 120, 34), juce::Justification::centredLeft, 2.6f * u, big);
    const float ow = v3::tabularWidth("ORBE", big) + 3.f * 2.6f * u + 12.f * u;
    g.setColour(T.accentText); tracked(g, "Autotune", Rectangle<float>(28.f * u + ow, 30.f * u, 220.f * u, 34.f * u), juce::Justification::centredLeft, 2.6f * u, big);
    g.setColour(T.accent); g.fillRoundedRectangle(R(30, 66, 30, 3), 1.5f * u);
    g.setColour(T.inkMute);
    v3::caps(g, "Moteur", R(302, 14, 120, 16), 11.f * u, juce::Justification::centredLeft);
    v3::caps(g, "Preset", R(490, 14, 120, 16), 11.f * u, juce::Justification::centredLeft);
    v3::caps(g, "Latence", R(760, 14, 120, 16), 11.f * u, juce::Justification::centredLeft);
    v3::caps(g, u8("Tonalité"), R(802, 78, 160, 16), 11.f * u, juce::Justification::centredLeft);
    v3::caps(g, "Gamme", R(802, 138, 160, 16), 11.f * u, juce::Justification::centredLeft);
    v3::caps(g, u8("Masque de notes"), R(802, 405, 220, 16), 11.f * u, juce::Justification::centredLeft);
    v3::hero(g, T, R(20, 77, 763, 316), 20.f * u);
    v3::hero(g, T, R(800, 205, 280, 188), 18.f * u);
    v3::card(g, T, R(20, 482, 1060, 205), 18.f * u);
    const char* titles[6] = {"Correction", "Hauteur", "Vibrato", "Harmonie", "Doublage", "Sortie"};
    for (int i = 0; i < 6; ++i) {
        g.setColour(T.inkMute); v3::caps(g, u8(titles[i]), R(kSecX[i] + 12, 498, kSecX[i + 1] - kSecX[i] - 16, 16), 11.f * u, juce::Justification::centredLeft);
        if (i > 0) { g.setColour(T.dark ? juce::Colours::white.withAlpha(0.07f) : juce::Colour(0x14101828)); g.fillRect(R(kSecX[i], 500, 1, 168)); }
    }
    v3::card(g, T, R(20, 700, 1060, 150), 18.f * u);
    g.setColour(T.inkMute);
    v3::caps(g, "Comportement", R(32, 712, 200, 16), 11.f * u, juce::Justification::centredLeft);
    v3::caps(g, "Stabilisateur (ms)", R(44, 736, 200, 16), 11.f * u, juce::Justification::centredLeft);
    v3::caps(g, u8("Attaque accordée"), R(44, 794, 200, 16), 11.f * u, juce::Justification::centredLeft);
    g.setColour(T.scrMute);
    v3::caps(g, "Note", R(818, 218, 100, 16), 11.f * u, juce::Justification::centredLeft);
    v3::caps(g, u8("Écart"), R(960, 218, 102, 16), 11.f * u, juce::Justification::centredRight);
}

void AutotuneEditor::paint3(juce::Graphics& g) {
    art.draw(g, String("bd|") + v3::modeKey(mode), getLocalBounds().toFloat(), [this](juce::Graphics& ig, Rectangle<float> b) { paintBackdrop3(ig, b); });
    if (!roll->isVisible()) paintScreen3(g, S(20, 77, 763, 316));
    paintReadout3(g, S(800, 205, 280, 188));
}

// Écran de hauteur v3 : courbes lissées et lumineuses, défilement continu, bulle de note qui suit la tête du tracé.
void AutotuneEditor::paintScreen3(juce::Graphics& g, Rectangle<float> r) {
    const float u = u_;
    const auto& t = proc.tel();
    const int N = atn::Telemetry::N, wr = t.wr.load(std::memory_order_acquire);
    int voiced = 0; float sum = 0.f;
    for (int i = 0; i < N; ++i) {
        const int k = (wr + i) % N;
        sDet[i] = t.det[k].load(); sTgt[i] = t.tgt[k].load(); sOut[i] = t.out[k].load();
        if (sDet[i] > 0.f && i > N - 240) { sum += sDet[i]; ++voiced; }
    }
    if (voiced > 8) { const float mc = sum / (float)voiced; viewC += (mc - viewC) * (std::fabs(mc - viewC) > 5.f ? 0.5f : 0.08f); }
    auto p = Rectangle<float>(r.getX() + 57.f * u, r.getY() + 46.f * u, r.getWidth() - 77.f * u, r.getHeight() - 76.f * u);
    const float range = 19.f;
    auto Y = [&](float m) { return p.getCentreY() - (m - viewC) * p.getHeight() / range; };
    auto X = [&](float i) { return p.getX() + p.getWidth() * i / (float)(N - 1); };
    const int lo = (int)std::floor(viewC - range / 2) - 1, hi = (int)std::ceil(viewC + range / 2) + 1;
    g.setFont(v3::font(11.5f * u));
    for (int n = lo; n <= hi; ++n) {
        const int pc = atn::pcOf(n);
        const bool al = (tMask >> pc) & 1, isRoot = tMode != 2 && pc == atn::pcOf(tRoot);
        const float y = Y((float)n);
        if (y < p.getY() - 6.f * u || y > p.getBottom() + 6.f * u || !al) continue;
        g.setColour(isRoot ? T.out.withAlpha(0.35f) : juce::Colours::white.withAlpha(0.08f));
        g.fillRect(p.getX(), y - 0.5f, p.getWidth(), 1.f);
        g.setColour(isRoot ? T.out : T.scrMute);
        g.drawText(nName(n), Rectangle<float>(r.getX() + 14.f * u, y - 8.f * u, 36.f * u, 16.f * u), juce::Justification::centredRight, false);
    }
    {
        juce::Graphics::ScopedSaveState st(g);
        g.reduceClipRegion(p.expanded(4.f * u, 10.f * u).toNearestInt());
        const v3::Trace::Opts o{340, 1.4f, 1e9f, 0.f};   // aucune coupure aux sauts de note : la courbe ondule d'une note à l'autre
        trTgt.build(sTgt, N, X, Y, p.getBottom(), {340, 0.6f, 0.5f, 0.f});
        trDet.build(sDet, N, X, Y, p.getBottom(), o);
        trOut.build(sOut, N, X, Y, p.getBottom(), o);
        trTgt.draw(g, T.target.withAlpha(0.6f), 1.1f * u, 0.f);
        trDet.draw(g, T.sung, 1.6f * u, 0.7f);
        trOut.draw(g, T.out, 2.4f * u, 1.f, 0.08f, p.getY() + p.getHeight() * 0.7f, true);
    }
    // bulle de note : valeurs brutes (non lissées), suit la tête du tracé de sortie
    if (lastDet > 0.f && trOut.hasHead) {
        const int sungN = (int)std::lround(lastDet);
        const float dev = (lastDet - (lastTgt > 0.f ? lastTgt : (float)sungN)) * 100.f;
        const auto f = v3::font(20.f * u, true);
        auto bub = Rectangle<float>(150.f * u, 38.f * u);
        bub.setPosition(juce::jlimit(p.getX(), r.getRight() - bub.getWidth() - 10.f * u, trOut.headPt.x - bub.getWidth() + 14.f * u),
                        trOut.headPt.y + 22.f * u + bub.getHeight() > r.getBottom() - 8.f * u ? trOut.headPt.y - 22.f * u - bub.getHeight() : trOut.headPt.y + 22.f * u);
        g.setColour(juce::Colour(0xE60A0D1C)); g.fillRoundedRectangle(bub, 10.f * u);
        g.setColour(juce::Colours::white.withAlpha(0.14f)); g.drawRoundedRectangle(bub, 10.f * u, 1.f);
        g.setColour(T.scrInk); g.setFont(f); g.drawText(nName(sungN), bub.reduced(14.f * u, 0), juce::Justification::centredLeft, false);
        g.setColour(T.out); v3::tabular(g, v3::fmt::cents(dev), bub.reduced(14.f * u, 0), f, juce::Justification::centredRight);
    }
    if (voiced < 3) { g.setColour(T.scrMute); v3::caps(g, "En attente de la voix", p, 13.f * u, juce::Justification::centred); }
    // ton et légende
    g.setColour(tMode == 2 ? T.out : T.scrInk); g.setFont(v3::font(14.f * u, true));
    String ton = tonText().fromFirstOccurrenceOf("TON : ", false, false).replace(u8(" \xE2\x80\x94 "), u8(" \xC2\xB7 "));
    g.drawText("Ton : " + ton, Rectangle<float>(r.getX() + 20.f * u, r.getY() + 12.f * u, 330.f * u, 22.f * u), juce::Justification::centredLeft, false);
    const char* leg[3] = {"Chant\xC3\xA9", "Sortie", "Cible"};
    const Colour lc[3] = {T.sung, T.out, T.target};
    for (int i = 0; i < 3; ++i) {
        const float x = r.getRight() - (float)(3 - i) * 88.f * u - 16.f * u;
        g.setColour(lc[i]); g.fillRoundedRectangle(Rectangle<float>(x, r.getY() + 21.f * u, 14.f * u, 3.f * u), 1.5f * u);
        g.setColour(T.scrMute); v3::caps(g, u8(leg[i]), Rectangle<float>(x + 20.f * u, r.getY() + 13.f * u, 66.f * u, 18.f * u), 11.f * u, juce::Justification::centredLeft);
    }
    const float total = (float)N * t.hopSec.load();
    g.setColour(T.scrMute); g.setFont(v3::font(11.5f * u));
    g.drawText(v3::fmt::minus() + String(juce::roundToInt(total)) + juce::String::charToString((juce::juce_wchar)0x00A0) + "s", Rectangle<float>(p.getX(), r.getBottom() - 24.f * u, 60.f * u, 18.f * u), juce::Justification::centredLeft, false);
    g.drawText("maintenant", Rectangle<float>(p.getRight() - 90.f * u, r.getBottom() - 24.f * u, 90.f * u, 18.f * u), juce::Justification::centredRight, false);
}

void AutotuneEditor::paintReadout3(juce::Graphics& g, Rectangle<float> r) {
    const float u = u_;
    const bool v = lastDet > 0.f;
    const int sungN = (int)std::lround(lastDet);
    const float dev = v ? (lastDet - (lastTgt > 0.f ? lastTgt : (float)sungN)) * 100.f : 0.f;
    const auto big = v3::font(54.f * u, true);
    g.setColour(T.out); g.setFont(big);
    g.drawText(v ? nName(sungN) : String(v3::fmt::minus() + v3::fmt::minus()), Rectangle<float>(r.getX() + 16.f * u, r.getY() + 32.f * u, 110.f * u, 62.f * u), juce::Justification::centredLeft, false);
    const String devTxt = v ? (dev < -0.5f ? v3::fmt::minus() : dev > 0.5f ? String("+") : String()) + String(std::abs(juce::roundToInt(dev))).paddedLeft('0', 2) : v3::fmt::minus() + v3::fmt::minus();
    g.setColour(T.scrInk); v3::tabular(g, devTxt, Rectangle<float>(r.getX() + 112.f * u, r.getY() + 32.f * u, 120.f * u, 62.f * u), big, juce::Justification::centredRight);
    g.setColour(T.scrMute); g.setFont(v3::font(16.f * u)); g.drawText("ct", Rectangle<float>(r.getRight() - 42.f * u, r.getY() + 66.f * u, 26.f * u, 22.f * u), juce::Justification::centredRight, false);
    g.setFont(v3::font(13.f * u));
    g.drawText("cible " + (v && lastTgt > 0.f ? nName((int)lastTgt) : String(v3::fmt::minus())), Rectangle<float>(r.getX() + 18.f * u, r.getY() + 96.f * u, 120.f * u, 18.f * u), juce::Justification::centredLeft, false);
    v3::gauge(g, T, Rectangle<float>(r.getX() + 120.f * u, r.getY() + 104.f * u, 150.f * u, 78.f * u), dispCorr, -50.f, 50.f, v3::fmt::minus() + "50", "0", "+50", 11.f * u);
}

bool AutotuneEditor::flats() const { return ned::useFlats(tMode == 2 ? -1 : atn::pcOf(tRoot), tScale, proc.flatsPref()); }
String AutotuneEditor::nName(int midi) const { return noteLabel(midi, flats()); }

String AutotuneEditor::tonText() const {
    const auto& sc = atn::scales()[juce::jlimit(0, atn::kScales - 1, tScale)];
    if (tMode == 2) return u8("TON : \xE2\x80\x94 en recherche");
    const String name = (sc.n == 12 ? String() : String(ned::noteName(atn::pcOf(tRoot), flats())) + " ") + sc.tag + (tFlips ? " + notes" : "");
    return "TON : " + name + u8(" \xE2\x80\x94 ") + (tMode == 0 ? "manuel" : "auto");
}

void AutotuneEditor::timerCallback() {
    const auto& t = proc.tel();
    tMode = t.mode.load(); tRoot = t.root.load(); tScale = t.scale.load(); tFlips = t.flips.load(); tMask = t.mask.load();
    const int wr = t.wr.load(std::memory_order_acquire), N = atn::Telemetry::N;
    lastDet = lastTgt = 0.f;
    for (int i = 1; i <= 24; ++i) { const int k = ((wr - i) % N + N) % N; const float d = t.det[k].load(); if (d > 0.f) { lastDet = d; lastTgt = t.tgt[k].load(); break; } }
    dispCorr += (t.corr.load() - dispCorr) * 0.4f;
    keys.mask = tMask; keys.mode = tMode; keys.root = tMode == 2 ? -1 : atn::pcOf(tRoot); keys.flats = flats();
    const int acc = proc.flatsPref() ? 1 : 0, algo = proc.orbeOn() ? 1 : 0;
    if (acc != lastAcc) { lastAcc = acc; for (int i = 0; i < 12; ++i) keyBox.changeItemText(i + 2, ned::noteName(i, acc == 1)); }
    if (algo != lastAlgo) { lastAlgo = algo; editKey.setEnabled(algo == 1); if (algo == 0 && editKey.getToggleState()) { editKey.setToggleState(false, juce::dontSendNotification); roll->setVisible(false); } lastLat = -1; }
    roll->flats = flats();
    if (roll->isVisible()) roll->tick();
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
        formToggle.setEnabled(lm == 1 || proc.orbeOn()); for (int i = 0; i < pots.size(); ++i) if (juce::String(kPots[i].id) == "form") pots[i]->setEnabled(lm == 1); else if (juce::String(kPots[i].id) == "expr") pots[i]->setEnabled(proc.orbeOn());
        repaint();
    }
    if (!isV3()) { if (!roll->isVisible()) repaint(S(24, 58, 952, 280).toNearestInt()); else repaint(S(770, 58, 206, 280).toNearestInt()); return; }
    for (int i = 0; i < k3.size(); ++i) {
        const String id(kPots[i].id);
        if (id == "form") k3[i]->setEnabled(juce::roundToInt(proc.raw("latmode")) == 1); else if (id == "expr") k3[i]->setEnabled(proc.orbeOn());
        k3[i]->tickMod(1.f / 30.f);
    }
    if (proc.getCurrentProgram() != lastProg) { lastProg = proc.getCurrentProgram(); for (auto* k : k3) k->resetMod(); }
    if (!roll->isVisible()) repaint(S(20, 77, 763, 316).toNearestInt());
    repaint(S(800, 205, 280, 188).toNearestInt());
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
    if (isV3()) { paint3(g); return; }
    const float u = u_;
    kit::w::drawFace(g, pal, getLocalBounds(), u, {kit::w::Family::Rack, "Orbe Autotune", kit::w::serial::autotune});
    engraved(g, pal, "Preset", S(370, 11, 64, 28), juce::Justification::centredRight, 8.f * u, 2.f * u, true);
    engraved(g, pal, "Latence", S(650, 11, 62, 28), juce::Justification::centredRight, 8.f * u, 2.f * u, true);
    engraved(g, pal, "Moteur", S(176, 11, 58, 28), juce::Justification::centredRight, 8.f * u, 2.f * u, true);
    engraved(g, pal, u8("Tonalit\xC3\xA9"), S(770, 62, 200, 12), juce::Justification::centredLeft, 8.f * u, 2.f * u, true);
    engraved(g, pal, "Gamme", S(770, 110, 200, 12), juce::Justification::centredLeft, 8.f * u, 2.f * u, true);

    if (!roll->isVisible()) paintScreen(g, S(24, 62, 726, 268));
    paintReadout(g, S(770, 160, 206, 62));
    paintGauge(g, S(770, 262, 206, 68));
    engraved(g, pal, u8("Masque de notes"), S(770, 340, 206, 12), juce::Justification::centredLeft, 8.f * u, 1.6f * u, true);

    module(g, "Correction", 24, 282, 398);
    module(g, "Hauteur", 320, 142, 398);
    module(g, "Vibrato", 476, 142, 398);
    module(g, "Harmonie", 632, 146, 398);
    module(g, "Doublage", 806, 78, 398);
    module(g, "Sortie", 898, 78, 398);

}

// ------------------------------------------------------------------ édition note par note
namespace {
void styleSlider(juce::Slider& sl, juce::Label& lb, const String& name, double lo, double hi, double step) {
    sl.setSliderStyle(juce::Slider::LinearHorizontal); sl.setTextBoxStyle(juce::Slider::TextBoxRight, false, 44, 18);
    sl.setRange(lo, hi, step); lb.setText(name, juce::dontSendNotification); lb.setJustificationType(juce::Justification::centredRight);
}
}

NoteRoll::NoteRoll(AutotuneProcessor& p, const kit::Palette& pl) : proc(p), pal(pl) {
    setWantsKeyboardFocus(true);
    arm.setButtonText("ARMER"); undoB.setButtonText("ANNULER"); redoB.setButtonText(u8("R\xC3\x89TABLIR"));
    zeroB.setButtonText(u8("NOTE \xC3\x80 Z\xC3\x89RO")); clearB.setButtonText("TOUT EFFACER");
    arm.setTooltip(u8("Capture : ARMER, puis lance la lecture du passage (60 s au plus). La capture s'arr\xC3\xAA" "te au retour de boucle ou par un second clic."));
    for (auto* b : {&arm, &undoB, &redoB, &zeroB, &clearB}) addAndMakeVisible(b);
    arm.onClick = [this] { if (proc.capture.armed.load() == 0) proc.armCapture(); else proc.stopCapture(); };
    undoB.onClick = [this] { proc.undo(); sel = -1; syncSliders(); repaint(); };
    redoB.onClick = [this] { proc.redo(); sel = -1; syncSliders(); repaint(); };
    zeroB.onClick = [this] { if (sel >= 0) edit([this](std::vector<ned::Note>& v) { v[(size_t)sel].shift = 0.f; v[(size_t)sel].edited = 0; }); };
    clearB.onClick = [this] { proc.setNotes({}, true); sel = -1; syncSliders(); repaint(); };
    styleSlider(sForce, lForce, "Force", 0.0, 1.0, 0.01); styleSlider(sExpr, lExpr, "Vibrato", 0.0, 1.0, 0.01); styleSlider(sSpeed, lSpeed, "Vitesse", 0.0, 200.0, 1.0);
    sForce.setTooltip(u8("Force de correction de la note s\xC3\xA9lectionn\xC3\xA9" "e."));
    sExpr.setTooltip(u8("Vibrato et inflexions gard\xC3\xA9s dans la note (0 = note plate)."));
    sSpeed.setTooltip(u8("Vitesse d'accrochage de la note (ms) ; 0 = imm\xC3\xA9" "diat."));
    sForce.onValueChange = [this] { if (sel >= 0 && !dragging) edit([this](std::vector<ned::Note>& v) { v[(size_t)sel].amount = (float)sForce.getValue(); v[(size_t)sel].edited = 1; }, false); };
    sExpr.onValueChange = [this] { if (sel >= 0 && !dragging) edit([this](std::vector<ned::Note>& v) { v[(size_t)sel].expr = (float)sExpr.getValue(); v[(size_t)sel].edited = 1; }, false); };
    sSpeed.onValueChange = [this] { if (sel >= 0 && !dragging) edit([this](std::vector<ned::Note>& v) { v[(size_t)sel].speedMs = (float)sSpeed.getValue(); v[(size_t)sel].edited = 1; }, false); };
    for (auto* sl : {&sForce, &sExpr, &sSpeed}) { sl->onDragEnd = [this] { proc.setNotes(proc.notes(), true); }; addAndMakeVisible(sl); }
    for (auto* lb : {&lForce, &lExpr, &lSpeed}) { lb->setColour(juce::Label::textColourId, kIce); addAndMakeVisible(lb); }
    syncSliders();
}

void NoteRoll::resized() {
    const float k = u;
    auto R = [k](float x, float y, float w, float h) { return juce::Rectangle<float>(x * k, y * k, w * k, h * k).toNearestInt(); };
    arm.setBounds(R(6, 4, 96, 24)); undoB.setBounds(R(106, 4, 86, 24)); redoB.setBounds(R(196, 4, 86, 24)); zeroB.setBounds(R(286, 4, 110, 24)); clearB.setBounds(R(400, 4, 104, 24));
    const float y = (float)getHeight() / k - 30.f;   // 238 à la taille classique
    lForce.setBounds(R(6, y, 50, 22)); sForce.setBounds(R(58, y, 168, 22));
    lExpr.setBounds(R(232, y, 58, 22)); sExpr.setBounds(R(292, y, 168, 22));
    lSpeed.setBounds(R(466, y, 58, 22)); sSpeed.setBounds(R(526, y, 190, 22));
    const bool n3 = v3::bridge::v3(T);
    for (auto* lb : {&lForce, &lExpr, &lSpeed}) lb->setFont(n3 ? v3::font(12.f * k) : mono(9.f * k));
    for (auto* sl : {&sForce, &sExpr, &sSpeed}) sl->setTextBoxStyle(juce::Slider::TextBoxRight, false, juce::roundToInt(44 * k), juce::roundToInt(18 * k));
}
juce::Rectangle<float> NoteRoll::plot() const { return getLocalBounds().toFloat().withTrimmedTop(32.f * u).withTrimmedBottom(34.f * u).withTrimmedLeft(34.f * u).reduced(4.f * u, 2.f * u); }
float NoteRoll::yOf(float m) const { const auto p = plot(); return p.getBottom() - (m - mLo) / (mHi - mLo) * p.getHeight(); }
float NoteRoll::mOf(float y) const { const auto p = plot(); return mLo + (p.getBottom() - y) / p.getHeight() * (mHi - mLo); }
float NoteRoll::xOf(double pos) const { const auto p = plot(); return p.getX() + (float)((pos - t0) / std::max(1.0, t1 - t0)) * p.getWidth(); }
double NoteRoll::posOf(float x) const { const auto p = plot(); return t0 + (double)((x - p.getX()) / p.getWidth()) * (t1 - t0); }

void NoteRoll::fitView() {
    const auto& c = proc.curve(); const auto& n = proc.notes();
    double a = 1e300, b = -1e300; float lo = 1e9f, hi = -1e9f;
    for (const auto& f : c) { a = std::min(a, f.pos); b = std::max(b, f.pos); if (f.midi > 0.f) { lo = std::min(lo, f.midi); hi = std::max(hi, f.midi); } }
    for (const auto& q : n) { a = std::min(a, q.start); b = std::max(b, q.end); lo = std::min({lo, q.center, q.target()}); hi = std::max({hi, q.center, q.target()}); }
    if (b <= a) { t0 = 0.0; t1 = 4.0 * proc.sampleRateNow(); mLo = 48.f; mHi = 72.f; repaint(); return; }
    const double pad = 0.03 * (b - a); t0 = a - pad; t1 = b + pad;
    mLo = std::floor(lo) - 3.f; mHi = std::ceil(hi) + 3.f; if (mHi - mLo < 12.f) { const float c0 = 0.5f * (mLo + mHi); mLo = c0 - 6.f; mHi = c0 + 6.f; }
    repaint();
}

int NoteRoll::noteAt(juce::Point<float> pt, int& edge) const {
    const auto& n = proc.notes(); edge = 0;
    for (int i = (int)n.size() - 1; i >= 0; --i) {
        const auto& q = n[(size_t)i]; const float x0 = xOf(q.start), x1 = xOf(q.end), y = yOf(q.target()), h = std::max(6.f * u, plot().getHeight() / (mHi - mLo) * 0.8f);
        if (pt.x >= x0 - 3.f && pt.x <= x1 + 3.f && std::fabs(pt.y - y) <= h * 0.5f + 2.f) { edge = pt.x - x0 < 5.f * u ? -1 : x1 - pt.x < 5.f * u ? 1 : 0; return i; }
    }
    return -1;
}

void NoteRoll::edit(const std::function<void(std::vector<ned::Note>&)>& f, bool undoable) {
    auto v = proc.notes(); if (sel >= (int)v.size()) sel = -1;
    f(v); proc.setNotes(std::move(v), undoable); syncSliders(); repaint();
}
void NoteRoll::syncSliders() {
    const auto& n = proc.notes(); const bool ok = sel >= 0 && sel < (int)n.size();
    for (auto* sl : {&sForce, &sExpr, &sSpeed}) sl->setEnabled(ok);
    zeroB.setEnabled(ok);
    if (!ok) return;
    const auto& q = n[(size_t)sel];
    sForce.setValue(q.amount, juce::dontSendNotification); sExpr.setValue(q.expr, juce::dontSendNotification); sSpeed.setValue(q.speedMs, juce::dontSendNotification);
}

void NoteRoll::tick() {
    const int a = proc.capture.armed.load();
    if (proc.pollCapture()) { sel = -1; fitView(); syncSliders(); }
    if (a != lastArm) { lastArm = a; arm.setButtonText(a == 1 ? "EN ATTENTE" : a == 2 ? "CAPTURE" : "ARMER"); arm.lit = a == 1 || a == 2; arm.repaint(); }
    undoB.setEnabled(proc.canUndo()); redoB.setEnabled(proc.canRedo()); clearB.setEnabled(!proc.notes().empty());
    repaint();
}

void NoteRoll::setTheme(const v3::Theme* t) {
    T = t;
    for (auto* b : {&arm, &undoB, &redoB, &zeroB, &clearB}) { b->T = t; b->onScreen = true; b->repaint(); }
    const bool n3 = v3::bridge::v3(t);   // v3 : valeurs à la française (virgule) ; Classique : texte d'origine
    sForce.textFromValueFunction = n3 ? std::function<String(double)>([](double v) { return v3::fmt::num(v, 2); }) : nullptr;
    sExpr.textFromValueFunction = sForce.textFromValueFunction;
    sSpeed.textFromValueFunction = n3 ? std::function<String(double)>([](double v) { return v3::fmt::ms(v); }) : nullptr;
    for (auto* sl : {&sForce, &sExpr, &sSpeed}) { sl->valueFromTextFunction = [](const String& t) { return t.replace(",", ".").retainCharacters("0123456789.-").getDoubleValue(); }; sl->updateText(); }
    resized();
    repaint();
}

// Édition v3 : même géométrie et mêmes gestes ; écran nuit, notes en pastilles arrondies à bords doux, courbe lissée.
void NoteRoll::paint3(juce::Graphics& g) {
    const auto& Tm = *T;
    auto b = getLocalBounds().toFloat();
    g.setGradientFill(juce::ColourGradient(Tm.scrTop, 0, 0, Tm.scrBot, 0, b.getBottom(), false)); g.fillRoundedRectangle(b, 20.f * u);
    const auto p = plot();
    const auto& c = proc.curve(); const auto& n = proc.notes();
    g.setFont(v3::font(11.f * u));
    for (int m = (int)std::floor(mLo); m <= (int)std::ceil(mHi); ++m) {
        const float y = yOf((float)m); if (y < p.getY() - 1.f || y > p.getBottom() + 1.f) continue;
        const int pc = atn::pcOf(m); const bool black = pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
        g.setColour(juce::Colours::white.withAlpha(pc == 0 ? 0.12f : black ? 0.025f : 0.05f)); g.fillRect(p.getX(), y - 0.5f, p.getWidth(), 1.f);
        if (!black || (mHi - mLo) < 16.f) { g.setColour(pc == 0 ? Tm.scrInk : Tm.scrMute); g.drawText(noteLabel(m, flats), juce::Rectangle<float>(2.f * u, y - 7.f * u, 34.f * u, 14.f * u), juce::Justification::centredRight); }
    }
    juce::Graphics::ScopedSaveState st(g); g.reduceClipRegion(p.toNearestInt());
    const float bh = std::max(8.f * u, p.getHeight() / (mHi - mLo) * 0.9f);
    for (int i = 0; i < (int)n.size(); ++i) {
        const auto& q = n[(size_t)i]; const float x0 = xOf(q.start), x1 = std::max(x0 + 4.f, xOf(q.end));
        auto blob = juce::Rectangle<float>(x0, yOf(q.target()) - bh * 0.5f, x1 - x0, bh);
        if (q.edited && std::fabs(q.shift) > 0.5f) { g.setColour(juce::Colours::white.withAlpha(0.22f)); g.drawRoundedRectangle(blob.withY(yOf(q.center) - bh * 0.5f), bh * 0.5f, 1.f); }
        const auto col = q.edited ? Tm.out : Tm.sung;
        g.setColour(col.withAlpha(0.12f)); g.fillRoundedRectangle(blob.expanded(2.f * u), bh * 0.5f + 2.f * u);   // bord doux
        g.setGradientFill(juce::ColourGradient(col.withAlpha(i == sel ? 0.85f : 0.5f), 0, blob.getY(), col.withAlpha(i == sel ? 0.6f : 0.3f), 0, blob.getBottom(), false));
        g.fillRoundedRectangle(blob, bh * 0.5f);
        g.setColour(i == sel ? juce::Colours::white : col.withAlpha(0.9f)); g.drawRoundedRectangle(blob, bh * 0.5f, i == sel ? 1.6f : 1.f);
    }
    // courbe capturée : lissée, sans marche (valeurs éditées intactes)
    curveBuf.resize(c.size());
    for (size_t i = 0; i < c.size(); ++i) curveBuf[i] = c[i].midi;
    if (!c.empty()) {
        const double hop = proc.capture.hop;
        trace.build(curveBuf.data(), (int)c.size(), [&](float i) { return xOf(c[(size_t)juce::jlimit(0, (int)c.size() - 1, (int)std::lround(i))].pos + hop * (double)(i - std::round(i))); },
                    [&](float m) { return yOf(m); }, p.getBottom(), {juce::jmax(64, (int)p.getWidth() / 2), 1.f, 3.f, 0.f});
        trace.draw(g, Tm.sung.withAlpha(0.95f), 1.3f * u, 0.6f);
    }
    if (proc.isPlaying()) { const float x = xOf(proc.playPos()); g.setColour(juce::Colours::white.withAlpha(0.5f)); g.fillRect(x, p.getY(), 1.f, p.getHeight()); }
}

void NoteRoll::paint(juce::Graphics& g) {
    if (v3::bridge::v3(T)) {
        paint3(g);
        g.setFont(v3::font(12.f * u));
        const auto p = plot(); const auto& c = proc.curve(); const auto& n = proc.notes();
        if (!proc.orbeOn()) { g.setColour(T->out); g.drawText(u8("Moteur Orbe requis pour l'\xC3\xA9" "dition"), p, juce::Justification::centred); }
        else if (n.empty() && c.empty()) { g.setColour(T->scrMute); g.drawText(u8("ARMER, puis lance la lecture du passage \xC3\xA0 \xC3\xA9" "diter"), p, juce::Justification::centred); }
        if (sel >= 0 && sel < (int)n.size()) {
            const auto& q = n[(size_t)sel]; const float t = q.target(); const int nm = (int)std::lround(t);
            const String txt = noteLabel(nm, flats) + " " + v3::fmt::cents((t - (float)nm) * 100.f) + u8(" \xC2\xB7 d\xC3\xA9tect\xC3\xA9" "e ") + noteLabel((int)std::lround(q.center), flats)
                               + " " + v3::fmt::cents((q.center - std::round(q.center)) * 100.f) + (q.edited ? u8(" \xC2\xB7 \xC3\xA9" "dit\xC3\xA9" "e") : String());
            g.setColour(T->sung); g.drawText(txt, juce::Rectangle<float>(510.f * u, 4.f * u, 210.f * u, 24.f * u), juce::Justification::centredRight, true);
        }
        return;
    }
    screenWell(g, pal, getLocalBounds().toFloat(), Display::OLED);
    const auto p = plot();
    const auto& c = proc.curve(); const auto& n = proc.notes();
    // grille des demi-tons
    g.setFont(mono(7.6f * u));
    for (int m = (int)std::floor(mLo); m <= (int)std::ceil(mHi); ++m) {
        const float y = yOf((float)m); if (y < p.getY() - 1.f || y > p.getBottom() + 1.f) continue;
        const int pc = atn::pcOf(m); const bool black = pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
        g.setColour(juce::Colours::white.withAlpha(pc == 0 ? 0.12f : black ? 0.025f : 0.05f)); g.fillRect(p.getX(), y - 0.5f, p.getWidth(), 1.f);
        g.setColour(pc == 0 ? kLilac : pal.mute); g.drawText(noteLabel(m, flats), juce::Rectangle<float>(2.f * u, y - 5.f * u, 34.f * u, 10.f * u), juce::Justification::centredRight);
    }
    g.saveState(); g.reduceClipRegion(p.toNearestInt());
    // notes (blobs) : contour à la hauteur détectée si la note est déplacée
    const float bh = std::max(6.f * u, p.getHeight() / (mHi - mLo) * 0.9f);
    for (int i = 0; i < (int)n.size(); ++i) {
        const auto& q = n[(size_t)i]; const float x0 = xOf(q.start), x1 = std::max(x0 + 2.f, xOf(q.end));
        if (q.edited && std::fabs(q.shift) > 0.5f) { g.setColour(juce::Colours::white.withAlpha(0.25f)); g.drawRoundedRectangle(x0, yOf(q.center) - bh * 0.5f, x1 - x0, bh, bh * 0.4f, 1.f); }
        const auto col = q.edited ? kPeach : kLilac;
        g.setColour(col.withAlpha(i == sel ? 0.75f : 0.38f)); g.fillRoundedRectangle(x0, yOf(q.target()) - bh * 0.5f, x1 - x0, bh, bh * 0.4f);
        g.setColour(col.withAlpha(0.9f)); g.drawRoundedRectangle(x0, yOf(q.target()) - bh * 0.5f, x1 - x0, bh, bh * 0.4f, 1.f);
        if (i == sel) { g.setColour(juce::Colours::white); g.drawRoundedRectangle(x0, yOf(q.target()) - bh * 0.5f, x1 - x0, bh, bh * 0.4f, 1.5f); }
    }
    // courbe de hauteur capturée
    juce::Path path; bool open = false; double prevPos = 0;
    const double hop = proc.capture.hop;
    for (const auto& f : c) {
        if (f.midi <= 0.f || (open && f.pos - prevPos > 2.5 * hop)) { open = false; if (f.midi <= 0.f) continue; }
        if (!open) { path.startNewSubPath(xOf(f.pos), yOf(f.midi)); open = true; } else path.lineTo(xOf(f.pos), yOf(f.midi));
        prevPos = f.pos;
    }
    g.setColour(kIce.withAlpha(0.9f)); g.strokePath(path, juce::PathStrokeType(1.1f * u));
    // tête de lecture
    if (proc.isPlaying()) { const float x = xOf(proc.playPos()); g.setColour(juce::Colours::white.withAlpha(0.5f)); g.fillRect(x, p.getY(), 1.f, p.getHeight()); }
    g.restoreState();
    // messages et note sélectionnée
    g.setFont(mono(9.f * u));
    if (!proc.orbeOn()) { g.setColour(kPeach); g.drawText(u8("Moteur Orbe requis pour l'\xC3\xA9" "dition"), p, juce::Justification::centred); }
    else if (n.empty() && c.empty()) { g.setColour(pal.mute); g.drawText(u8("ARMER, puis lance la lecture du passage \xC3\xA0 \xC3\xA9" "diter"), p, juce::Justification::centred); }
    if (sel >= 0 && sel < (int)n.size()) {
        const auto& q = n[(size_t)sel]; const float t = q.target(); const int nm = (int)std::lround(t);
        const String txt = noteLabel(nm, flats) + String::formatted(" %+d ct", juce::roundToInt((t - (float)nm) * 100.f)) + u8(" \xC2\xB7 d\xC3\xA9tect\xC3\xA9" "e ") + noteLabel((int)std::lround(q.center), flats)
                           + String::formatted(" %+d ct", juce::roundToInt((q.center - std::round(q.center)) * 100.f)) + (q.edited ? u8(" \xC2\xB7 \xC3\xA9" "dit\xC3\xA9" "e") : String());
        g.setColour(kIce); g.drawText(txt, juce::Rectangle<float>(510.f * u, 4.f * u, 210.f * u, 24.f * u), juce::Justification::centredRight);
    }
}

void NoteRoll::mouseMove(const juce::MouseEvent& e) {
    int edge; const int i = noteAt(e.position, edge);
    setMouseCursor(i < 0 ? juce::MouseCursor::NormalCursor : edge != 0 ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::UpDownResizeCursor);
}
void NoteRoll::mouseDown(const juce::MouseEvent& e) {
    grabKeyboardFocus();
    int edge; sel = noteAt(e.position, edge); dragEdge = edge; moved = false; dragging = sel >= 0;
    if (sel >= 0 && e.mods.isAltDown()) {   // Alt-clic : couper la note à cet instant
        const double at = posOf(e.position.x);
        edit([this, at](std::vector<ned::Note>& v) { auto q = v[(size_t)sel]; if (at <= q.start + 64 || at >= q.end - 64) return; v[(size_t)sel].end = at; q.start = at; v.insert(v.begin() + sel + 1, q); });
        dragging = false;
    }
    if (sel >= 0) { const auto& q = proc.notes()[(size_t)sel]; dragStartY = e.position.y; dragStartShift = q.shift; dragStartPos = edge < 0 ? q.start : q.end; }
    syncSliders(); repaint();
}
void NoteRoll::mouseDrag(const juce::MouseEvent& e) {
    if (!dragging || sel < 0) return;
    moved = true;
    if (dragEdge != 0) {   // redimensionner (sans chevaucher les voisines)
        const double at = posOf(e.position.x);
        edit([this, at](std::vector<ned::Note>& v) {
            auto& q = v[(size_t)sel];
            if (dragEdge < 0) q.start = std::clamp(at, sel > 0 ? v[(size_t)sel - 1].end : -1e300, q.end - 256.0);
            else q.end = std::clamp(at, q.start + 256.0, sel + 1 < (int)v.size() ? v[(size_t)sel + 1].start : 1e300);
        }, false);
        return;
    }
    const float dm = mOf(e.position.y) - mOf(dragStartY);
    const bool fine = e.mods.isShiftDown();
    edit([this, dm, fine](std::vector<ned::Note>& v) {
        auto& q = v[(size_t)sel]; float tgt = q.center + (dragStartShift + dm * 100.f) * 0.01f;
        if (!fine) tgt = std::round(tgt);   // sans Maj : la note tombe sur un demi-ton
        q.shift = std::round((tgt - q.center) * 100.f); q.edited = 1;
    }, false);
}
void NoteRoll::mouseUp(const juce::MouseEvent&) { if (dragging && moved) proc.setNotes(proc.notes(), true); dragging = false; }
void NoteRoll::mouseDoubleClick(const juce::MouseEvent& e) {
    int edge; const int i = noteAt(e.position, edge); if (i < 0) return; sel = i;
    edit([this](std::vector<ned::Note>& v) { auto& q = v[(size_t)sel]; q.shift = std::round((std::round(q.target()) - q.center) * 100.f); q.edited = 1; });   // caler sur la note
}
void NoteRoll::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w) {
    if (e.mods.isCommandDown()) { const double at = posOf(e.position.x), k = w.deltaY > 0 ? 0.8 : 1.25; t0 = at - (at - t0) * k; t1 = at + (t1 - at) * k; }
    else { const float d = (w.deltaY > 0 ? 1.f : -1.f); mLo += d; mHi += d; }
    repaint();
}
bool NoteRoll::keyPressed(const juce::KeyPress& k) {
    const auto& n = proc.notes();
    if (k.getModifiers().isCommandDown() && (k.getKeyCode() == 'Z' || k.getKeyCode() == 'z')) { if (k.getModifiers().isShiftDown()) proc.redo(); else proc.undo(); sel = -1; syncSliders(); repaint(); return true; }
    if (k.getModifiers().isCommandDown() && (k.getKeyCode() == 'Y' || k.getKeyCode() == 'y')) { proc.redo(); sel = -1; syncSliders(); repaint(); return true; }
    if (k.getKeyCode() == juce::KeyPress::leftKey || k.getKeyCode() == juce::KeyPress::rightKey) {
        if (!n.empty()) sel = juce::jlimit(0, (int)n.size() - 1, sel + (k.getKeyCode() == juce::KeyPress::leftKey ? -1 : 1));
        syncSliders(); repaint(); return true;
    }
    if (sel < 0 || sel >= (int)n.size()) return false;
    if (k.getKeyCode() == juce::KeyPress::upKey || k.getKeyCode() == juce::KeyPress::downKey) {
        const float step = (k.getModifiers().isShiftDown() ? 10.f : 100.f) * (k.getKeyCode() == juce::KeyPress::upKey ? 1.f : -1.f);
        edit([this, step](std::vector<ned::Note>& v) { v[(size_t)sel].shift += step; v[(size_t)sel].edited = 1; });
        return true;
    }
    if (k.getKeyCode() == juce::KeyPress::deleteKey || k.getKeyCode() == juce::KeyPress::backspaceKey) { edit([this](std::vector<ned::Note>& v) { v[(size_t)sel].shift = 0.f; v[(size_t)sel].edited = 0; }); return true; }
    if ((k.getKeyCode() == 'M' || k.getKeyCode() == 'm') && sel + 1 < (int)n.size()) {   // fusionner avec la suivante
        edit([this](std::vector<ned::Note>& v) { auto& a = v[(size_t)sel]; const auto& b = v[(size_t)sel + 1];
            const double la = a.end - a.start, lb = b.end - b.start; const float tgt = a.target();
            a.center = (float)((a.center * la + b.center * lb) / std::max(1.0, la + lb)); a.end = b.end; a.shift = a.edited ? std::round((tgt - a.center) * 100.f) : 0.f;
            v.erase(v.begin() + sel + 1); });
        return true;
    }
    return false;
}
