#include <complex>
#include "OrbeFxEditor.h"

using namespace kit;
using juce::Colour, juce::Rectangle, juce::String;
namespace v3 = kit::v3;

namespace {
constexpr float kCardY = 326.f, kCardW = 203.f, kCardH = 352.f, kCardStep = 214.f;
const char* kModName[5] = {"Filtre", "Saturation", "Chorus", "Delay", "R\xC3\xA9verbe"};
const char* kModOn[5] = {"fon", "son", "con", "don", "ron"};
const float kGridY[5] = {82.f, 82.f, 46.f, 118.f, 38.f};   // début de la grille de potards de chaque carte

float dbOf(float peak) { return peak > 1e-5f ? juce::Decibels::gainToDecibels(peak) : -90.f; }

// Module du SVF 2 pôles (même Q que le moteur) : gain en dB à la fréquence f.
float filterDb(int mode, float fc, float res, float f) {
    const float q = 0.6f + res * res * 14.f, x = f / fc;
    const std::complex<float> s(0.f, x), den = s * s + s / q + 1.f;
    const std::complex<float> h = mode == 0 ? 1.f / den : mode == 1 ? (s / q) / den : (s * s) / den;
    return 20.f * std::log10(juce::jmax(1e-4f, std::abs(h)));
}
}  // namespace

// ---------------- écran ----------------
void FxScreen::tick() {
    auto fall = [](float& cur, float v, float rate) { cur = v > cur ? v : cur - rate; };
    fall(inDb, dbOf(proc.inPeak.exchange(0.f)), 2.f);
    fall(outDb, dbOf(proc.outPeak.exchange(0.f)), 2.f);
    fall(inHold, inDb, 0.25f); fall(outHold, outDb, 0.25f);
    repaint();
}

void FxScreen::paint(juce::Graphics& g) {
    if (T == nullptr) return;
    const float u = (float)getWidth() / 360.f;
    auto b = getLocalBounds().toFloat();
    auto meters = b.removeFromRight(70.f * u);
    auto plot = b.reduced(2.f * u, 6.f * u);
    plot.removeFromBottom(14.f * u);
    const float lo = 20.f, hi = 20000.f, dbLo = -30.f, dbHi = 18.f;
    auto X = [&](float f) { return plot.getX() + plot.getWidth() * std::log(f / lo) / std::log(hi / lo); };
    auto Y = [&](float d) { return plot.getY() + plot.getHeight() * (1.f - (juce::jlimit(dbLo, dbHi, d) - dbLo) / (dbHi - dbLo)); };
    g.setFont(v3::font(9.5f * u));
    for (float f : {100.f, 1000.f, 10000.f}) {
        g.setColour(T->scrGrid); g.drawVerticalLine((int)X(f), plot.getY(), plot.getBottom());
        g.setColour(T->scrMute); g.drawText(f >= 1000.f ? String((int)(f / 1000)) + " k" : String((int)f), Rectangle<float>(X(f) - 20.f * u, plot.getBottom() + 1.f * u, 40.f * u, 12.f * u), juce::Justification::centred, false);
    }
    for (float d : {-24.f, -12.f, 0.f, 12.f}) { g.setColour(d == 0.f ? T->scrMute.withAlpha(0.45f) : T->scrGrid); g.drawHorizontalLine((int)Y(d), plot.getX(), plot.getRight()); }
    const bool on = proc.raw("fon") > 0.5f;
    const int mode = juce::roundToInt(proc.raw("fmode"));
    const float fc = proc.raw("fcut"), res = proc.raw("fres");
    juce::Path p, fill;
    for (int i = 0; i <= 160; ++i) {
        const float f = lo * std::pow(hi / lo, (float)i / 160.f), d = on ? filterDb(mode, fc, res, f) : 0.f;
        const float x = X(f), y = Y(d);
        if (i == 0) { p.startNewSubPath(x, y); } else p.lineTo(x, y);
    }
    fill = p; fill.lineTo(plot.getRight(), plot.getBottom()); fill.lineTo(plot.getX(), plot.getBottom()); fill.closeSubPath();
    g.setColour(T->accent.withAlpha(on ? 0.16f : 0.06f)); g.fillPath(fill);
    g.setColour(on ? T->accent : T->scrMute); g.strokePath(p, juce::PathStrokeType(2.f * u, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    if (on) { g.setColour(T->scrInk); g.fillEllipse(Rectangle<float>(7.f * u, 7.f * u).withCentre({X(fc), Y(filterDb(mode, fc, res, fc))})); }
    // crêtes entrée / sortie
    meters = meters.reduced(6.f * u, 6.f * u);
    const float bw = 14.f * u, gap = 12.f * u, x0 = meters.getX() + (meters.getWidth() - (bw * 2.f + gap)) * 0.5f;
    auto bar = [&](float x, float db, float hold, const char* name, Colour c) {
        auto r = Rectangle<float>(x, meters.getY(), bw, meters.getHeight() - 16.f * u);
        g.setColour(T->scrGrid); g.fillRoundedRectangle(r, 3.f * u);
        const float h = r.getHeight() * juce::jlimit(0.f, 1.f, (db + 60.f) / 66.f);
        g.setColour(db > -0.5f ? juce::Colour(0xFFFF5A5A) : c); g.fillRoundedRectangle(r.withTop(r.getBottom() - h).withHeight(h), 3.f * u);
        const float hy = r.getBottom() - r.getHeight() * juce::jlimit(0.f, 1.f, (hold + 60.f) / 66.f);
        g.setColour(T->scrInk); g.fillRect(x, hy, bw, 1.5f * u);
        g.setColour(T->scrMute); v3::caps(g, name, Rectangle<float>(x - 8.f * u, r.getBottom() + 2.f * u, bw + 16.f * u, 12.f * u), 9.f * u, juce::Justification::centred);
    };
    bar(x0, inDb, inHold, "In", T->scrMute); bar(x0 + bw + gap, outDb, outHold, "Out", T->accent);
}

// ---------------- éditeur ----------------
void OrbeFxEditor::addKnob(const char* id, const String& lab, const String& tip, v3::KnobSize sz, bool peach, bool bipolar) {
    auto* k = k3.add(new v3::Knob3(proc.apvts, id, lab, tip, sz));
    k->setLabelPx(sz == v3::KnobSize::S ? 10.f : 11.f); k->setPeach(peach); k->setBipolar(bipolar);
    knobIds.add(id); addAndMakeVisible(k);
}

void OrbeFxEditor::addCombo(juce::ComboBox& box, std::unique_ptr<ComboAtt>& att, const char* id, const String& tip) {
    if (auto* c = dynamic_cast<juce::AudioParameterChoice*>(proc.apvts.getParameter(id))) {
        int n = 1; for (auto& s : c->choices) box.addItem(s, n++);
        box.setTitle(c->getName(64));
    }
    box.setJustificationType(juce::Justification::centredLeft); box.setTooltip(tip);
    addAndMakeVisible(box);
    att = std::make_unique<ComboAtt>(proc.apvts, id, box);
}

OrbeFxEditor::OrbeFxEditor(OrbeFxProcessor& p)
    : AudioProcessorEditor(&p), proc(p), pal(kit::w::rack(v3::accents::ice)), screen(p) {
    auto& ap = proc.apvts;
    for (int i = 0; i < OrbeFxProcessor::numPresets(); ++i) presetBox.addItem(proc.getProgramName(i), i + 1);
    presetBox.addItem("Perso", 100);
    presetBox.setTitle("Preset");
    presetBox.setTooltip(u8("Point de départ : règle les cinq modules et le mélange. Les macros reviennent à zéro ; Mix et Sortie restent à toi."));
    presetBox.onChange = [this] { const int id = presetBox.getSelectedId(); if (id >= 1 && id <= OrbeFxProcessor::numPresets()) gotoPreset(id - 1); };
    addAndMakeVisible(presetBox);
    prevB.onClick = [this] { const int n = OrbeFxProcessor::numPresets(); gotoPreset((proc.getCurrentProgram() + n - 1) % n); };
    nextB.onClick = [this] { gotoPreset((proc.getCurrentProgram() + 1) % OrbeFxProcessor::numPresets()); };
    addAndMakeVisible(prevB); addAndMakeVisible(nextB);
    inSw = std::make_unique<v3::InSwitch>(*ap.getParameter("bypass")); addAndMakeVisible(*inSw);

    using KS = v3::KnobSize;
    // macros (M) puis sortie (S, Sortie en pêche)
    addKnob("mcol", "Couleur", u8("Macro : tire le son vers le sombre (gauche) ou le brillant (droite) : tilt de tonalité, filtre et mix de saturation."), KS::M, false, true);
    addKnob("mgrain", "Grain", u8("Macro : ajoute de la saturation, du suivi et du chorus pour du grain et de la densité."), KS::M);
    addKnob("mmove", "Mouvement", u8("Macro : ajoute du chorus, de la largeur et de la réinjection du delay pour que le son bouge."), KS::M);
    addKnob("mspace", "Espace", u8("Macro : ajoute du delay et de la réverbe (mix, taille, durée)."), KS::M);
    addKnob("mix", "Mix", u8("Dosage global sec / traité. 100 % : tout l'effet."), KS::S);
    addKnob("gain", "Sortie", u8("Niveau de sortie, appliqué après le mix."), KS::S, true);
    // filtre
    addCombo(modeBox, modeAtt, "fmode", u8("Mode du filtre : passe-bas, passe-bande ou passe-haut."));
    addKnob("fcut", u8("Fréquence"), u8("Fréquence de coupure du filtre."), KS::S);
    addKnob("fres", u8("Résonance"), u8("Accentue la fréquence de coupure."), KS::S);
    addKnob("fenv", "Suivi", u8("Le niveau de l'entrée ouvre le filtre : plus il est fort, plus la coupure monte."), KS::S);
    // saturation
    addCombo(typeBox, typeAtt, "stype", u8("Caractère : doux (tanh), dur (écrêtage) ou asymétrique (harmoniques paires)."));
    addKnob("sdrive", "Drive", u8("Gain envoyé dans la saturation (la sortie est compensée)."), KS::S);
    addKnob("stone", "Ton", u8("Tonalité après la saturation : sombre à gauche, brillant à droite."), KS::S);
    addKnob("smix", "Mix", u8("Dosage sec / saturé de ce module."), KS::S);
    // chorus
    addKnob("crate", "Vitesse", u8("Vitesse de la modulation du chorus."), KS::S);
    addKnob("cdepth", u8("Profondeur"), u8("Profondeur de la modulation."), KS::S);
    addKnob("cspread", "Largeur", u8("Largeur stéréo des trois voix de l'ensemble."), KS::S);
    addKnob("cmix", "Mix", u8("Dosage sec / chorus."), KS::S);
    // delay
    addCombo(timeBox, timeAtt, "dtime", u8("Durée du delay, calée sur le tempo du morceau."));
    addKnob("dfb", u8("Réinjection"), u8("Nombre de répétitions. Au maximum le delay reste stable."), KS::S);
    addKnob("dtone", "Ton", u8("Assombrit les répétitions successives (de la bande, de l'écho)."), KS::S);
    addKnob("dmix", "Mix", u8("Dosage sec / delay."), KS::S);
    // réverbe
    addKnob("rsize", "Taille", u8("Taille de la pièce."), KS::S);
    addKnob("rdecay", u8("Durée"), u8("Durée de la queue de réverbe."), KS::S);
    addKnob("rpre", u8("Pré-retard"), u8("Délai avant l'arrivée de la réverbe."), KS::S);
    addKnob("rdamp", "Amorti", u8("Absorbe les aigus de la queue."), KS::S);
    addKnob("rmix", "Mix", u8("Dosage sec / réverbe."), KS::S);

    for (int i = 0; i < 5; ++i)
        segs.add(new v3::bridge::Seg(ap, kModOn[i], juce::StringArray{"Off", "On"}, String(u8("Active ou coupe le module ")) + u8(kModName[i]) + ".", pal));
    segs.add(new v3::bridge::Seg(ap, "dping", juce::StringArray{u8("Stéréo"), "Ping-pong"}, u8("Ping-pong : les répétitions rebondissent entre gauche et droite."), pal));
    for (auto* s : segs) addAndMakeVisible(*s);
    addAndMakeVisible(screen);

    help.install(*this, "Orbe FX", {
        u8("Effet audio : filtre, saturation, chorus, delay puis réverbe, dans cet ordre. Chaque module a son interrupteur Off / On."),
        u8("- Preset : point de départ. « Perso » apparaît dès que tu changes un réglage."),
        u8("- Macros : Couleur (sombre à brillant), Grain (saturation et suivi), Mouvement (chorus et réinjection), Espace (delay et réverbe). Elles s'ajoutent aux modules sans les modifier."),
        u8("- Mix dose l'effet entier ; Sortie règle le niveau final. IN contourne tout l'effet sans clic ni retard."),
        u8("- Delay : calé sur le tempo de l'hôte (120 BPM sans transport). La queue de réverbe et de delay continue après l'arrêt."),
        u8("- Ce plugin traite le son qu'il reçoit : place-le en insert sur une piste audio.")});
    addAndMakeVisible(face.themeBtn);
    face.onMode = [this] {
        for (auto* k : k3) k->setup(&face.T, &face.art);
        for (auto* s : segs) { s->T = &face.T; s->repaint(); }
        prevB.T = &face.T; nextB.T = &face.T; inSw->setup(&face.T);
        help.T = &face.T; screen.T = &face.T;
        repaint();
    };
    setWantsKeyboardFocus(true);
    face.init(*this);
    startTimerHz(30);
}

OrbeFxEditor::~OrbeFxEditor() { stopTimer(); face.shutdown(); }

void OrbeFxEditor::gotoPreset(int i) {
    proc.setCurrentProgram(i);
    for (auto* k : k3) k->resetMod();
    repaint();
}

void OrbeFxEditor::resized() {
    u_ = face.onResized();
    help.layout(Si(1044, 34, 30, 30), getLocalBounds());
    face.themeBtn.setBounds(Si(1008, 35, 28, 28));
    inSw->setBounds(Si(860, 34, 110, 30));
    prevB.setBounds(Si(352, 34, 32, 32)); presetBox.setBounds(Si(390, 34, 280, 32)); nextB.setBounds(Si(676, 34, 32, 32));
    screen.setBounds(Si(30, 112, 360, 192));
    const float kS = v3::Knob3::boxHeight(v3::KnobSize::S, false), kM = v3::Knob3::boxHeight(v3::KnobSize::M, false);
    auto place = [&](const char* id, float x, float y, float w, float h) { const int i = knobIds.indexOf(id); if (i >= 0) { k3[i]->setScale(u_); k3[i]->setBounds(Si(x, y, w, h)); } };
    const char* macros[4] = {"mcol", "mgrain", "mmove", "mspace"};
    for (int i = 0; i < 4; ++i) place(macros[i], 424.f + (float)i * 122.f, 126.f, 118.f, kM);
    place("mix", 954.f, 108.f, 106.f, kS); place("gain", 954.f, 208.f, 106.f, kS);
    const char* grid[5][5] = {{"fcut", "fres", "fenv", nullptr, nullptr}, {"sdrive", "stone", "smix", nullptr, nullptr}, {"crate", "cdepth", "cspread", "cmix", nullptr},
                              {"dfb", "dtone", "dmix", nullptr, nullptr}, {"rsize", "rdecay", "rpre", "rdamp", "rmix"}};
    for (int m = 0; m < 5; ++m) {
        const float x0 = 20.f + (float)m * kCardStep;
        for (int j = 0; j < 5 && grid[m][j] != nullptr; ++j) place(grid[m][j], x0 + 1.f + (float)(j % 2) * 101.f, kCardY + kGridY[m] + (float)(j / 2) * 103.f, 101.f, kS);
        segs[m]->setBounds(Si(x0 + 112.f, kCardY + 10.f, 80.f, 26.f)); segs[m]->px = 11.f * u_;
    }
    modeBox.setBounds(Si(20.f + 12.f, kCardY + 44.f, kCardW - 24.f, 30.f));
    typeBox.setBounds(Si(20.f + kCardStep + 12.f, kCardY + 44.f, kCardW - 24.f, 30.f));
    timeBox.setBounds(Si(20.f + 3.f * kCardStep + 12.f, kCardY + 44.f, kCardW - 24.f, 30.f));
    segs[5]->setBounds(Si(20.f + 3.f * kCardStep + 12.f, kCardY + 80.f, kCardW - 24.f, 30.f)); segs[5]->px = 11.f * u_;
}

void OrbeFxEditor::paintBackdrop(juce::Graphics& g, Rectangle<float> b) {
    const float u = b.getWidth() / kW;
    auto R = [u](float x, float y, float w, float h) { return Rectangle<float>(x * u, y * u, w * u, h * u); };
    const auto& T = face.T;
    v3::background(g, T, b);
    v3::rk::brand(g, T, u, "Orbe", "FX");
    auto lab = [&](const String& s, float x, float y, float w) { v3::rk::label(g, T.inkMute, u, s, x, y, w, 11.f); };
    lab("Preset", 392, 14, 120);
    v3::hero(g, T, R(20, 82, 380, 232), 20.f * u);
    g.setColour(T.scrMute); v3::caps(g, u8("Réponse du filtre · niveaux"), R(40, 92, 300, 16), 11.f * u, juce::Justification::centredLeft);
    v3::card(g, T, R(414, 82, 506, 232), 18.f * u);
    lab("Macros", 434, 94, 200);
    v3::card(g, T, R(934, 82, 146, 232), 18.f * u);
    lab("Sortie", 954, 94, 100);
    for (int m = 0; m < 5; ++m) {
        const float x0 = 20.f + (float)m * kCardStep;
        v3::card(g, T, R(x0, kCardY, kCardW, kCardH), 18.f * u);
        g.setColour(T.ink); v3::caps(g, u8(kModName[m]), R(x0 + 14, kCardY + 14, 100, 18), 12.f * u, juce::Justification::centredLeft);
    }
}

void OrbeFxEditor::paint(juce::Graphics& g) {
    face.art.draw(g, String("bd|") + face.artKey(), getLocalBounds().toFloat(), [this](juce::Graphics& ig, Rectangle<float> b) { paintBackdrop(ig, b); });
}

void OrbeFxEditor::timerCallback() {
    for (auto* k : k3) k->tickMod(1.f / 30.f);
    screen.tick();
    int sel = 100;
    for (int i = 0; i < OrbeFxProcessor::numPresets(); ++i) if (proc.presetMatches(i)) { sel = i + 1; if (i == proc.getCurrentProgram()) break; }
    if (presetBox.getSelectedId() != sel) presetBox.setSelectedId(sel, juce::dontSendNotification);
}
