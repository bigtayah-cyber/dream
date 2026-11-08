#include "OneKnobEditor.h"

using namespace kit;
using juce::Colour, juce::Rectangle, juce::String;

namespace {
const char* kKeyName[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
// lettres lisibles en 7 segments (b et d minuscules) ; le dièse s'affiche en point décimal
const char* kKeySeg[12] = {"C", "C.", "d", "d.", "E", "F", "F.", "G", "G.", "A", "A.", "b"};

// Parti pris matériel par modèle (audit § 3.2).
struct FaceCfg { KnobStyle big, small; juce::uint32 bigCap, smallCap; };
FaceCfg faceCfg(int vi) {
    switch (vi) {
        case 0: return {KnobStyle::Skirt, KnobStyle::Cap, 0, 0xFFE0352B};            // Grain Nord : harmoniseur fin 70
        case 1: return {KnobStyle::Chicken, KnobStyle::Chicken, 0, 0};                // Gorge : compresseur à lampes
        case 2: return {KnobStyle::Cap, KnobStyle::Cap, 0xFFF1E4CC, 0xFFFF4FA3};       // Poupée : pédale boutique
        default: return {KnobStyle::Fluted, KnobStyle::Fluted, 0, 0};                 // Patine : préampli de radio
    }
}
Rectangle<float> centred(float cx, float cy, float w, float h) { return {cx - w / 2, cy - h / 2, w, h}; }
}  // namespace

Palette OneKnobEditor::palette(int i) {
    Palette p;
    switch (i) {
        case 0:  // Grain Nord : aluminium anodisé bleu nuit, rouge des afficheurs
            p.finish = Finish::Anodized; p.plateTop = Colour(0xFF27395A); p.plateBot = Colour(0xFF121C30);
            p.ink = Colour(0xFFE6EEF6); p.accent = Colour(0xFFFF3B2E); p.accent2 = Colour(0xFF7FD1FF); p.screenInk = Colour(0xFFFF7A6A); break;
        case 1:  // Gorge : peinture martelée rouille-bordeaux, ambre
            p.finish = Finish::Hammertone; p.plateTop = Colour(0xFF60291F); p.plateBot = Colour(0xFF33130E);
            p.ink = Colour(0xFFF2E3CC); p.accent = Colour(0xFFFFA040); p.accent2 = Colour(0xFFFFC98A); p.screenInk = Colour(0xFFFFB02E); break;
        case 2:  // Poupée : laque prune métallisée, sérigraphie rose et turquoise
            p.finish = Finish::Lacquer; p.plateTop = Colour(0xFF5E2C66); p.plateBot = Colour(0xFF2A1131);
            p.ink = Colour(0xFFFFEAF5); p.silk = Colour(0xFFFFD6EE); p.accent = Colour(0xFFFF4FA3); p.accent2 = Colour(0xFF5CE1E6); p.screenInk = Colour(0xFF7FEFF2); break;
        default:  // Patine : vert sauge peint sur aluminium brossé, laiton
            p.finish = Finish::BrushedAlu; p.plateTop = Colour(0xFF6A785D); p.plateBot = Colour(0xFF3A4533);
            p.ink = Colour(0xFFF5F0DE); p.accent = Colour(0xFFE0C677); p.accent2 = Colour(0xFFF2E3A8); p.screenInk = Colour(0xFFF2E3A8); break;
    }
    p.mute = p.ink.withAlpha(0.6f); p.line = p.ink.withAlpha(0.14f);
    p.screen = Colour(0xFF0C0C0E); p.brush = 1.f;
    p.seed = seedFor(onek::voicings()[(size_t)juce::jlimit(0, 3, i)].id);
    return p;
}

OneKnobEditor::OneKnobEditor(OneKnobProcessor& p)
    : AudioProcessorEditor(&p), proc(p), vi(OneKnobProcessor::index()), pal(palette(vi)), look(pal) {
    setLookAndFeel(&look);
    const auto& vc = OneKnobProcessor::voicing();
    const auto cfg = faceCfg(vi);
    onek::Mapped m; m.eng.tension = 0; vc.map(1.f, m);
    usesPitch = m.eng.tension > 0.01f;

    auto setupKnob = [&](juce::Slider& s, KnobStyle st, juce::uint32 cap, bool scale, const String& tip) {
        s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        s.getProperties().set("kitKnob", (int)st);
        s.getProperties().set("scale", scale);
        if (cap != 0) s.getProperties().set("cap", (juce::int64)cap);
        s.setTooltip(tip);
        addAndMakeVisible(s);
    };
    setupKnob(knobS, cfg.big, cfg.bigCap, true, u8("Caractère : toute la chaîne vocale du modèle en un geste. Double-clic : valeur d'origine."));
    knobS.setMouseDragSensitivity(260);
    knobAtt = std::make_unique<SliderAtt>(proc.apvts, "char", knobS);
    knobS.setDoubleClickReturnValue(true, vc.defaultK * 10.0);

    struct SmallDef { juce::Slider* s; const char* id; const char* tip; std::unique_ptr<SliderAtt>* att; };
    const SmallDef smalls[] = {
        {&trimS, "trim", "Entrée : ±12 dB avant la chaîne. Avec l'auto-entrée, le niveau est de toute façon ramené vers -18 dBFS RMS.", &trimAtt},
        {&toneS, "tone", "Ton : incline le timbre, vers le grave (à gauche) ou vers l'aigu (à droite), ±6 dB.", &toneAtt},
        {&mixS, "mix", "Mix : part du son traité. 0 % = voix sèche (alignée sur la latence).", &mixAtt},
        {&outS, "out", "Sortie : ±12 dB après tout le traitement.", &outAtt},
        {&specS, "spec", vc.specTip, &specAtt},
    };
    for (auto& d : smalls) {
        setupKnob(*d.s, cfg.small, cfg.smallCap, true, u8(d.tip));
        d.s->setMouseDragSensitivity(200);
        *d.att = std::make_unique<SliderAtt>(proc.apvts, d.id, *d.s);
        if (auto* prm = dynamic_cast<juce::RangedAudioParameter*>(proc.apvts.getParameter(d.id)))
            d.s->setDoubleClickReturnValue(true, prm->convertFrom0to1(prm->getDefaultValue()));
        d.s->onValueChange = [this] { if (panelOpen) repaint(Si(0, kH, kW, kPanel)); };
    }

    // IN : levier (Grain Nord, Gorge, Patine) ou footswitch (Poupée). Le paramètre est « bypass » : en service = non enfoncé.
    inSw.setTooltip(u8("Effet en service. Bascule pour le contourner : la voix passe intacte, latence compensée, fondu de 10 ms."));
    inSw.painter = [this](juce::Graphics& g, FaceButton& b, bool hover, bool down) {
        const auto r = b.getLocalBounds().toFloat();
        const bool on = !b.getToggleState();
        if (vi == 2) {  // footswitch chromé
            const float cx = r.getCentreX(), cy = r.getCentreY(), R = juce::jmin(r.getWidth(), r.getHeight()) * 0.5f - 2.f;
            juce::Path nut; nut.addPolygon({cx, cy}, 6, R, 0.f);
            g.setColour(juce::Colours::black.withAlpha(0.5f)); g.fillPath(nut, juce::AffineTransform::translation(1.f, 2.5f));
            g.setGradientFill(juce::ColourGradient(Colour(0xFFE8E8EC), cx - R, cy - R, Colour(0xFF4A4A50), cx + R, cy + R, false)); g.fillPath(nut);
            g.setColour(juce::Colours::black.withAlpha(0.5f)); g.strokePath(nut, juce::PathStrokeType(0.8f));
            const float cr = R * 0.66f, dy = down ? 1.5f : 0.f;
            g.setColour(juce::Colours::black.withAlpha(0.45f)); g.fillEllipse(cx - cr + 1, cy - cr + 3 + dy, cr * 2, cr * 2);
            g.setGradientFill(juce::ColourGradient(Colour(0xFFFAFAFC), cx - cr * 0.4f, cy - cr * 0.5f + dy, Colour(0xFF5A5A60), cx + cr, cy + cr + dy, true));
            g.fillEllipse(cx - cr, cy - cr + dy, cr * 2, cr * 2);
            g.setColour(juce::Colours::white.withAlpha(hover ? 0.75f : 0.5f)); g.drawEllipse(cx - cr * 0.7f, cy - cr * 0.7f + dy, cr * 1.4f, cr * 1.4f, 0.8f);
            g.setColour(juce::Colours::black.withAlpha(0.35f)); g.drawEllipse(cx - cr, cy - cr + dy, cr * 2, cr * 2, 1.f);
            juce::ignoreUnused(on);
            return;
        }
        const float lh = 11.f * u_;
        engraved(g, pal, "IN", r.withHeight(lh), juce::Justification::centred, 9.5f * u_, 1.5f * u_, true);
        engraved(g, pal, "OFF", r.withTrimmedTop(r.getHeight() - lh), juce::Justification::centred, 9.f * u_, 1.2f * u_, true);
        leverSwitch(g, pal, r.reduced(0, lh), on, {}, {});
        juce::ignoreUnused(hover, down);
    };
    addAndMakeVisible(inSw);
    inAtt = std::make_unique<ButtonAtt>(proc.apvts, "bypass", inSw);

    // poussoirs à LED du volet
    auto pushPainter = [this](const char* label, Colour ledC) {
        return [this, label, ledC](juce::Graphics& g, FaceButton& b, bool, bool down) {
            const auto r = b.getLocalBounds().toFloat();
            const float h = r.getHeight();
            pushButton(g, pal, Rectangle<float>(r.getX() + 1.5f, r.getY() + 1.5f, h * 1.3f, h - 3.f), b.getToggleState(), down, ledC);
            engraved(g, pal, u8(label), r.withTrimmedLeft(h * 1.3f + 8.f * u_), juce::Justification::centredLeft, 9.5f * u_, 1.2f * u_, true);
        };
    };
    autoInB.painter = pushPainter("Auto-entrée", pal.ledGreen);
    autoInB.setTooltip(u8("Auto-entrée : un étage lent (RMS sur ~1 s) ramène la voix vers -18 dBFS avant le compresseur et le de-esser. "
                          "Le même réglage sonne pareil que la prise arrive à -30 ou à -6 dBFS. Le gain est rendu en sortie."));
    autoInAtt = std::make_unique<ButtonAtt>(proc.apvts, "autoin", autoInB);
    autoGainB.painter = pushPainter("Auto-gain", pal.ledAmber);
    autoGainB.setTooltip(u8("Auto-gain : la sortie garde le niveau d'entrée quel que soit le potard (compensation lente, ~0,5 s). Coupe-le pour juger le gain réel."));
    autoGainAtt = std::make_unique<ButtonAtt>(proc.apvts, "autogain", autoGainB);
    for (auto* b : {&autoInB, &autoGainB}) addChildComponent(*b);

    advBtn.painter = [this](juce::Graphics& g, FaceButton& b, bool hover, bool down) {
        const auto r = b.getLocalBounds().toFloat();
        const float h = r.getHeight();
        pushButton(g, pal, Rectangle<float>(r.getX() + 1.5f, r.getY() + 1.5f, h * 1.3f, h - 3.f), b.getToggleState(), down, pal.accent);
        const auto tr = r.withTrimmedLeft(h * 1.3f + 8.f * u_);
        g.setColour(pal.silkInk().withAlpha(hover ? 1.f : 0.88f));
        engraved(g, pal, u8("Réglages"), tr, juce::Justification::centredLeft, 9.5f * u_, 1.6f * u_, true);
        const float tx = r.getRight() - 8.f * u_, ty = r.getCentreY(), t = 3.2f * u_;   // chevron gravé
        juce::Path tri;
        if (b.getToggleState()) tri.addTriangle(tx - t, ty + t * 0.5f, tx + t, ty + t * 0.5f, tx, ty - t * 0.75f);
        else tri.addTriangle(tx - t, ty - t * 0.5f, tx + t, ty - t * 0.5f, tx, ty + t * 0.75f);
        g.setColour(pal.silkInk().withAlpha(0.85f)); g.fillPath(tri);
    };
    advBtn.setTooltip(u8("Ouvre le volet : entrée, ton, mix, sortie, réglage propre au modèle, auto-entrée et auto-gain. Fermé, le potard suffit."));
    advBtn.onClick = [this] { setPanel(advBtn.getToggleState(), true); };
    addAndMakeVisible(advBtn);

    // sélecteurs de tonalité et de gamme (paramètres sauvegardés avec le projet) : seulement sur les modèles qui corrigent la hauteur
    for (auto* b : {&keyBox, &scaleBox}) { addChildComponent(*b); b->setJustificationType(juce::Justification::centredLeft); b->setScrollWheelEnabled(false); }
    keyBox.addItemList(juce::StringArray{"Auto", "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"}, 1);
    scaleBox.addItemList(juce::StringArray{"Auto", "Majeur", "Mineur"}, 1);
    keyAtt = std::make_unique<ComboAtt>(proc.apvts, "key", keyBox);
    scaleAtt = std::make_unique<ComboAtt>(proc.apvts, "scale", scaleBox);
    keyBox.setTooltip(u8("Tonalité. Auto : détectée sur ta voix, et AUCUNE correction vers une note tant qu'elle n'est pas fiable (l'afficheur clignote). Ou impose la tonique."));
    scaleBox.setTooltip(u8("Gamme. Auto : majeur ou mineur détecté. Ou impose majeur / mineur."));
    keyBox.setVisible(usesPitch); scaleBox.setVisible(usesPitch);

    // taille et volet mémorisés dans l'état du projet
    panelOpen = (bool)proc.apvts.state.getProperty("adv", false);
    advBtn.setToggleState(panelOpen, juce::dontSendNotification);
    setResizable(true, true);
    const int w = juce::jlimit(240, 600, (int)proc.apvts.state.getProperty("uiW", 300));
    setPanel(panelOpen, false);
    setSize(w, juce::roundToInt((float)w * (kH + (panelOpen ? kPanel : 0.f)) / kW));
    startTimerHz(30);
}

OneKnobEditor::~OneKnobEditor() { stopTimer(); setLookAndFeel(nullptr); }

void OneKnobEditor::setPanel(bool open, bool resize) {
    panelOpen = open;
    proc.apvts.state.setProperty("adv", open, nullptr);
    const float h = kH + (open ? kPanel : 0.f);
    setResizeLimits(240, juce::roundToInt(240.f * h / kW), 600, juce::roundToInt(600.f * h / kW));
    getConstrainer()->setFixedAspectRatio(kW / h);
    for (juce::Component* c : std::initializer_list<juce::Component*>{&trimS, &toneS, &mixS, &outS, &specS, &autoInB, &autoGainB}) c->setVisible(open);
    if (resize) setSize(getWidth(), juce::roundToInt((float)getWidth() * h / kW));
}

OneKnobEditor::Layout OneKnobEditor::layout() const {
    // zones en unités de base (largeur 300) : afficheur héros, potard (carré de 3 r), légende, zone basse
    switch (vi) {
        case 0: return {{22, 96, 256, 110}, centred(150, 338, 192, 192), {40, 438, 220, 16}, {22, 462, 256, 58}};
        case 1: return {{22, 94, 256, 132}, centred(150, 350, 180, 180), {40, 444, 220, 16}, {22, 466, 256, 56}};
        case 2: return {{22, 96, 256, 98}, centred(150, 318, 180, 180), {40, 410, 220, 16}, {22, 432, 256, 90}};
        default: return {{22, 94, 256, 128}, centred(150, 348, 186, 186), {40, 444, 220, 16}, {22, 466, 256, 56}};
    }
}

void OneKnobEditor::resized() {
    u_ = (float)getWidth() / kW;
    look.u = u_;
    const auto L = layout();
    knobS.setBounds(S(L.knob.getX(), L.knob.getY(), L.knob.getWidth(), L.knob.getHeight()).toNearestInt());
    if (vi == 2) inSw.setBounds(Si(124, 452, 52, 52));
    else inSw.setBounds(Si(232, 8, 48, 86));
    if (vi == 2) {
        keyBox.setBounds(Si(214, 132, 64, 22));
        scaleBox.setBounds(Si(214, 160, 64, 22));
    } else {
        keyBox.setBounds(Si(22, 480, 100, 24));
        scaleBox.setBounds(Si(132, 480, 100, 24));
    }
    advBtn.setBounds(Si(20, 529, 112, 20));
    const float xs[5] = {46, 98, 150, 202, 254};
    juce::Slider* sm[5] = {&trimS, &toneS, &mixS, &outS, &specS};
    for (int i = 0; i < 5; ++i) sm[i]->setBounds(S(xs[i] - 24, kH + 20, 48, 48).toNearestInt());
    autoInB.setBounds(Si(22, kH + 96, 130, 22));
    autoGainB.setBounds(Si(162, kH + 96, 110, 22));
    proc.apvts.state.setProperty("uiW", getWidth(), nullptr);
}

void OneKnobEditor::timerCallback() {
    vu = proc.vuDb.load();
    gr += (proc.reduction.load() - gr) * 0.35f;
    inDb = proc.inDb.load();
    ++tick;
    const auto L = layout();
    repaint(S(L.hero.getX() - 4, L.hero.getY() - 4, L.hero.getWidth() + 8, L.hero.getHeight() + 8).toNearestInt());
    repaint(S(L.bottom.getX(), L.bottom.getY(), L.bottom.getWidth(), L.bottom.getHeight()).toNearestInt());
    if (vi == 2) repaint(Si(236, 26, 40, 30));
    if (panelOpen) repaint(Si(20, kH + 4, 100, 18));
}

String OneKnobEditor::keyText(bool seg7) const {
    const int pc = proc.keyPc.load(), st = proc.keyState.load();
    if (!usesPitch || pc < 0 || pc > 11 || st == 2) return seg7 ? "--" : "...";
    if (seg7) return kKeySeg[pc];
    return String(kKeyName[pc]) + (proc.keyScale.load() == 2 ? " MIN" : " MAJ");
}

// Plaque de série rivetée (aluminium) ou plaque laiton gravée.
void OneKnobEditor::serialPlate(juce::Graphics& g, Rectangle<float> r, bool brass) {
    const auto top = brass ? Colour(0xFFE2C77A) : Colour(0xFFD4D6DA), bot = brass ? Colour(0xFF8C6B2C) : Colour(0xFF7E8288);
    g.setColour(juce::Colours::black.withAlpha(0.45f));
    g.fillRoundedRectangle(r.translated(0.6f * u_, 1.4f * u_), 2.f * u_);
    juce::ColourGradient cg(top, r.getX(), r.getY(), bot, r.getRight(), r.getBottom(), false);
    cg.addColour(0.45, top.darker(0.15f));
    g.setGradientFill(cg);
    g.fillRoundedRectangle(r, 2.f * u_);
    g.setColour(juce::Colours::white.withAlpha(0.35f));
    g.drawLine(r.getX() + 2 * u_, r.getY() + 0.7f, r.getRight() - 2 * u_, r.getY() + 0.7f, 0.8f);
    g.setColour(juce::Colours::black.withAlpha(0.4f));
    g.drawRoundedRectangle(r, 2.f * u_, 0.8f);
    const float rv = 1.7f * u_;
    for (float x : {r.getX() + 4.5f * u_, r.getRight() - 4.5f * u_}) {
        const float y = r.getCentreY();
        g.setGradientFill(juce::ColourGradient(top.brighter(0.4f), x - rv, y - rv, bot.darker(0.4f), x + rv, y + rv, true));
        g.fillEllipse(x - rv, y - rv, rv * 2, rv * 2);
        g.setColour(juce::Colours::black.withAlpha(0.4f)); g.drawEllipse(x - rv, y - rv, rv * 2, rv * 2, 0.6f);
    }
}

void OneKnobEditor::paint(juce::Graphics& g) {
    const float u = u_;
    const auto& vc = OneKnobProcessor::voicing();
    const auto b = getLocalBounds();
    drawPlate(g, b, pal, pal.seed);
    chassis(g, pal, b.toFloat());
    const float H = (float)getHeight() / u;
    for (float y : {14.f, H - 14.f})
        for (float x : {14.f, 286.f}) screw(g, x * u, y * u, 4.6f * u, (x * 0.7f + y * 0.3f) * 0.05f, vi == 0 ? ScrewHead::Hex : vi == 1 ? ScrewHead::Slot : ScrewHead::Phillips);

    // en-tête : marque, nom du modèle
    engraved(g, pal, "dreamaws", S(22, 18, 140, 12), juce::Justification::centredLeft, 9.5f * u, 2.6f * u);
    const auto name = upper(u8(vc.name));
    if (vi == 2) {  // sérigraphie années 90 : rose vif décalé sur turquoise
        const auto f = fnt(25.f * u, true);
        g.setColour(pal.accent2.withAlpha(0.9f)); tracked(g, name, S(24.5f, 33.5f, 200, 32), juce::Justification::centredLeft, 3.5f * u, f);
        g.setColour(pal.accent); tracked(g, name, S(22, 31, 200, 32), juce::Justification::centredLeft, 3.5f * u, f);
    } else {
        engraved(g, pal, name, S(22, 31, 206, 32), juce::Justification::centredLeft, vi == 0 ? 23.f * u : 25.f * u, 3.5f * u, true);
    }
    if (vi == 2) {  // voyant d'état 5 mm de la pédale
        const bool on = !proc.apvts.getRawParameterValue("bypass")->load();
        led(g, 256 * u, 41 * u, 4.4f * u, on ? 1.f : 0.f, pal.ledRed, LedBezel::Chrome);
    }

    const auto L = layout();
    paintHero(g, S(L.hero.getX(), L.hero.getY(), L.hero.getWidth(), L.hero.getHeight()));
    engraved(g, pal, u8("Caractère"), S(L.label.getX(), L.label.getY(), L.label.getWidth(), L.label.getHeight()), juce::Justification::centred, 12.f * u, 5.f * u, true);
    paintBottom(g, S(L.bottom.getX(), L.bottom.getY(), L.bottom.getWidth(), L.bottom.getHeight()));

    // bande basse : série (sauf Patine : plaque laiton dans la zone basse)
    if (vi != 3) {
        auto pr = S(196, 530, 68, 18);
        serialPlate(g, pr, false);
        g.setColour(Colour(0xFF1C1D20)); g.setFont(mono(9.5f * u));
        g.drawText(vc.serial, pr.reduced(8 * u, 0), juce::Justification::centred, false);
    }
    if (panelOpen) paintPanel(g, S(0, kH, kW, kPanel));
}

void OneKnobEditor::paintKeyLeds(juce::Graphics& g, Rectangle<float> r) {
    // trois voyants légendés : MAJ, MIN, AUTO (clignote en recherche)
    const float u = u_;
    const int st = proc.keyState.load(), sc = proc.keyScale.load();
    const bool known = proc.keyPc.load() >= 0 && st != 2;
    const bool blink = (tick / 8) % 2 == 0;
    struct L { const char* t; float lv; } leds[3] = {{"MAJ", known && sc == 1 ? 1.f : 0.f}, {"MIN", known && sc == 2 ? 1.f : 0.f},
                                                       {"AUTO", st == 1 ? 1.f : st == 2 ? (blink ? 0.8f : 0.f) : 0.f}};
    const float w = r.getWidth() / 3.f;
    for (int i = 0; i < 3; ++i) {
        const float x = r.getX() + w * (float)i;
        led(g, x + 6 * u, r.getCentreY(), 3.f * u, leds[i].lv, pal.ledRed, LedBezel::Black);
        engraved(g, pal, leds[i].t, Rectangle<float>(x + 14 * u, r.getY(), w - 14 * u, r.getHeight()), juce::Justification::centredLeft, 9.f * u, 1.2f * u, true);
    }
}

void OneKnobEditor::paintHero(juce::Graphics& g, Rectangle<float> r) {
    const float u = u_;
    const bool peak = proc.level.load() > 0.89f;
    switch (vi) {
        case 0: {  // afficheur rouge à 7 segments + barre-graphe LED
            auto win = Rectangle<float>(r.getX(), r.getY() + 2 * u, 180 * u, 72 * u);
            screenWell(g, pal, win, Display::Seg7);
            const auto kt = keyText(true);
            const bool dim = kt == "--" && (tick / 8) % 2 == 1;
            g.saveState(); if (dim) g.setOpacity(0.35f);
            screenText(g, pal, Display::Seg7, kt, Rectangle<float>(win.getX() + 10 * u, win.getY() + 8 * u, 84 * u, win.getHeight() - 16 * u), juce::Justification::centredLeft, 46 * u);
            g.restoreState();
            const float kv = (float)knobS.getValue();
            screenText(g, pal, Display::Seg7, String(kv, 1), Rectangle<float>(win.getX() + 96 * u, win.getY() + 22 * u, 74 * u, 40 * u), juce::Justification::centredRight, 24 * u);
            paintKeyLeds(g, Rectangle<float>(r.getX() + 2 * u, win.getBottom() + 10 * u, 176 * u, 14 * u));
            // barre-graphe vertical (balistique VU)
            const float vals[12] = {-20, -15, -10, -7, -5, -3, -2, -1, 0, 1, 2, 3};
            const float x0 = r.getRight() - 22 * u, segH = 6.6f * u, gap = 2.4f * u, yb = r.getBottom() - 2 * u;
            g.setColour(juce::Colours::black.withAlpha(0.55f));
            g.fillRoundedRectangle(x0 - 3 * u, yb - 12 * (segH + gap) - 1 * u, 20 * u, 12 * (segH + gap) + 4 * u, 2.f * u);
            for (int i = 0; i < 12; ++i) {
                const float y = yb - (float)(i + 1) * (segH + gap) + gap;
                const auto c = vals[i] > 0 ? pal.ledRed : vals[i] >= -3 ? pal.ledAmber : pal.ledGreen;
                const bool lit = vu >= vals[i];
                const auto lens = lit ? c : c.withMultipliedBrightness(0.26f).withMultipliedSaturation(0.8f);
                auto seg = Rectangle<float>(x0, y, 14 * u, segH);
                if (lit) { g.setColour(c.withAlpha(0.28f)); g.fillRoundedRectangle(seg.expanded(2.f * u), 2.f * u); }
                g.setGradientFill(juce::ColourGradient(lens.brighter(lit ? 0.5f : 0.15f), seg.getX(), seg.getY(), lens.darker(0.3f), seg.getX(), seg.getBottom(), false));
                g.fillRoundedRectangle(seg, 1.2f * u);
                if (vals[i] == 3 || vals[i] == 0 || vals[i] == -5 || vals[i] == -10 || vals[i] == -20) {
                    const String t = vals[i] > 0 ? "+" + String((int)vals[i]) : String((int)vals[i]);
                    engraved(g, pal, t, Rectangle<float>(x0 - 32 * u, y - 3 * u, 26 * u, segH + 6 * u), juce::Justification::centredRight, 9.f * u, 0.4f * u, true);
                }
            }
            break;
        }
        case 1:  // grand VU rétroéclairé ambre
            vuMeter(g, pal, r, vu, peak, Colour(0xFFFFA23A));
            break;
        case 2: {  // petite fenêtre VU + LCD de tonalité
            vuMeter(g, pal, Rectangle<float>(r.getX(), r.getY(), 140 * u, 92 * u), vu, peak, Colour(0xFFFF8FC8));
            auto lcd = Rectangle<float>(r.getX() + 154 * u, r.getY() + 2 * u, 102 * u, 26 * u);
            screenWell(g, pal, lcd, Display::LCD);
            const int st = proc.keyState.load();
            screenText(g, pal, Display::LCD, keyText(false), lcd.reduced(7 * u, 0), juce::Justification::centredLeft, 13.f * u);
            screenText(g, pal, Display::LCD, st == 0 ? "MAN" : st == 1 ? "AUTO" : ((tick / 8) % 2 ? "" : "AUTO"), lcd.reduced(7 * u, 0), juce::Justification::centredRight, 10.f * u);
            engraved(g, pal, "TON", Rectangle<float>(r.getX() + 154 * u, r.getY() + 36 * u, 38 * u, 22 * u), juce::Justification::centredLeft, 9.f * u, 1.2f * u, true);
            engraved(g, pal, "GAMME", Rectangle<float>(r.getX() + 154 * u, r.getY() + 64 * u, 38 * u, 22 * u), juce::Justification::centredLeft, 9.f * u, 0.6f * u, true);
            break;
        }
        default:  // VU crème large
            vuMeter(g, pal, r, vu, peak, Colour(0xFFFFC46A));
            break;
    }
}

void OneKnobEditor::paintBottom(juce::Graphics& g, Rectangle<float> r) {
    const float u = u_;
    switch (vi) {
        case 0:
            engraved(g, pal, u8("Tonalité"), Rectangle<float>(r.getX() + 2 * u, r.getY(), 100 * u, 14 * u), juce::Justification::centredLeft, 9.f * u, 1.4f * u, true);
            engraved(g, pal, "Gamme", Rectangle<float>(r.getX() + 112 * u, r.getY(), 100 * u, 14 * u), juce::Justification::centredLeft, 9.f * u, 1.4f * u, true);
            break;
        case 1: {  // voyant « lampe » qui rougeoie avec la compression + rampe de réduction
            const float lv = juce::jlimit(0.f, 1.f, 0.35f + gr / 14.f);
            const float cx = r.getX() + 22 * u, cy = r.getCentreY() + 2 * u;
            led(g, cx, cy, 9.f * u, lv, Colour(0xFFFF7A1A), LedBezel::Chrome);
            engraved(g, pal, u8("Réduction"), Rectangle<float>(r.getX() + 58 * u, r.getY() + 2 * u, 190 * u, 13 * u), juce::Justification::centredLeft, 9.f * u, 1.8f * u, true);
            const int th[6] = {1, 3, 6, 9, 12, 15};
            for (int i = 0; i < 6; ++i) {
                const float x = r.getX() + 66 * u + (float)i * 34 * u;
                led(g, x, r.getY() + 28 * u, 3.4f * u, gr >= (float)th[i] ? 1.f : 0.f, i < 4 ? pal.ledAmber : pal.ledRed, LedBezel::Black);
                engraved(g, pal, String(th[i]), Rectangle<float>(x - 12 * u, r.getY() + 37 * u, 24 * u, 12 * u), juce::Justification::centred, 9.f * u, 0.4f * u, true);
            }
            break;
        }
        case 2: break;  // footswitch (composant)
        default: {  // plaque laiton gravée
            auto pl = Rectangle<float>(r.getX() + 30 * u, r.getY() + 4 * u, 196 * u, 44 * u);
            serialPlate(g, pl, true);
            const auto ink = Colour(0xFF3A2A0E);
            g.setColour(juce::Colours::white.withAlpha(0.3f));
            tracked(g, u8("Patine"), pl.withHeight(28 * u).translated(0, 0.8f * u), juce::Justification::centred, 5.f * u, fnt(14.f * u, true));
            g.setColour(ink);
            tracked(g, u8("Patine"), pl.withHeight(28 * u), juce::Justification::centred, 5.f * u, fnt(14.f * u, true));
            g.setColour(ink.withAlpha(0.85f));
            tracked(g, String(u8("N° ")) + OneKnobProcessor::voicing().serial + "  dreamaws", pl.withTrimmedTop(24 * u).withHeight(16 * u), juce::Justification::centred, 1.4f * u, fnt(9.f * u, true));
            break;
        }
    }
}

void OneKnobEditor::paintPanel(juce::Graphics& g, Rectangle<float> r) {
    const float u = u_;
    // volet : sous-plaque encastrée, charnière en haut
    auto in = r.reduced(10 * u, 0).withTrimmedTop(2 * u).withTrimmedBottom(26 * u);
    g.setColour(juce::Colours::black.withAlpha(0.22f));
    g.fillRoundedRectangle(in, 4 * u);
    g.setGradientFill(juce::ColourGradient(juce::Colours::black.withAlpha(0.45f), in.getX(), in.getY(), juce::Colours::transparentBlack, in.getX(), in.getY() + 10 * u, false));
    g.fillRoundedRectangle(in, 4 * u);
    g.setColour(juce::Colours::white.withAlpha(0.10f));
    g.drawRoundedRectangle(in.translated(0, 0.8f), 4 * u, 0.8f);
    g.setColour(juce::Colours::black.withAlpha(0.6f));
    g.fillRect(r.getX() + 10 * u, r.getY(), r.getWidth() - 20 * u, 1.2f * u);

    const auto& vc = OneKnobProcessor::voicing();
    const char* ids[5] = {"trim", "tone", "mix", "out", "spec"};
    const String labels[5] = {u8("Entrée"), "Ton", "Mix", "Sortie", u8(vc.specName)};
    const float xs[5] = {46, 98, 150, 202, 254};
    for (int i = 0; i < 5; ++i) {
        const float cx = xs[i] * u;
        engraved(g, pal, labels[i], Rectangle<float>(cx - 26 * u, r.getY() + 7 * u, 52 * u, 12 * u), juce::Justification::centred, 9.f * u, 1.f * u, true);
        String v;
        if (auto* p = proc.apvts.getParameter(ids[i])) v = p->getCurrentValueAsText();
        if (i == 1 && v.startsWith("+")) v = u8("aigu ") + v;
        else if (i == 1 && v.startsWith("-")) v = u8("grave ") + v.substring(1);
        g.setColour(pal.silkInk().withAlpha(0.92f));
        g.setFont(mono(9.f * u));
        g.drawText(v, Rectangle<float>(cx - 27 * u, r.getY() + 70 * u, 54 * u, 12 * u), juce::Justification::centred, false);
    }
    // voyant de niveau d'entrée : vert dans la zone idéale (-24 à -12 dBFS RMS), ambre au-dessus, éteint en dessous de -40
    const auto c = inDb > -12.f ? pal.ledAmber : pal.ledGreen;
    const float lv = inDb < -40.f ? 0.f : inDb < -24.f ? 0.35f : 1.f;
    led(g, (46 + 26) * u, r.getY() + 13 * u, 2.6f * u, lv, c, LedBezel::Black);
}
