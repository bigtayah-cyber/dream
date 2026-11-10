#include "RelanceEditor.h"

using namespace kit;
using juce::Colour, juce::Rectangle, juce::String;
namespace v3 = kit::v3;

namespace {
const Colour kAccent(0xFF8F7CFF);   // famille « voix générative » : lilas
struct KDef { const char* id; const char* label; const char* tip; float cx; };
const KDef kKnobs[] = {
    {"dens", "Densit\xC3\xA9", "Probabilit\xC3\xA9 qu'une fin de phrase re\xC3\xA7oive une relance. Bas par d\xC3\xA9" "faut : quelques r\xC3\xA9ponses bien plac\xC3\xA9" "es valent mieux qu'un \xC3\xA9" "cho permanent.", 85},
    {"audace", "Audace", "Relances plus marqu\xC3\xA9" "es : plus de r\xC3\xA9p\xC3\xA9titions d'\xC3\xA9" "cho et de b\xC3\xA9gaiement, octave haute possible en harmonie ; en M\xC3\xA9lange, les caract\xC3\xA8res fous (invers\xC3\xA9, b\xC3\xA9gaiement, t\xC3\xA9l\xC3\xA9phone) reviennent plus souvent.", 185},
    {"thresh", "Seuil", "Niveau au-dessus duquel la voix est consid\xC3\xA9r\xC3\xA9" "e pr\xC3\xA9sente. Une fin de phrase = au moins 250 ms de voix puis 150 ms sous le seuil. Monte-le si le bruit de fond emp\xC3\xAA" "che les fins de phrase.", 390},
    {"len", "Longueur", "Dur\xC3\xA9" "e captur\xC3\xA9" "e \xC3\xA0 la fin de chaque phrase (200 \xC3\xA0 600 ms) : le dernier mot ou les derni\xC3\xA8res syllabes.", 490},
    {"width", "Largeur", "\xC3\x89" "cart st\xC3\xA9r\xC3\xA9o des relances (elles alternent gauche et droite).", 595},
    {"tone", "Couleur", "Passe-bas des relances : sombre (en retrait, derri\xC3\xA8re la voix) \xC3\xA0 clair (pr\xC3\xA9sent).", 695},
    {"seed", "Graine", "Tirage des relances. M\xC3\xAAme graine = m\xC3\xAAmes choix. Change-la pour entendre d'autres r\xC3\xA9ponses sur la m\xC3\xAAme prise.", 815},
    {"level", "Niveau", "Niveau des relances.", 985},
};
// sections de la carte : titre, x de début (la dernière valeur ferme)
const char* kSecTitle[5] = {"R\xC3\xA9ponse", "\xC3\x89" "coute", "Espace", "Graine", "Sortie"};
const float kSecX[6] = {30, 335, 545, 745, 900, 1070};
constexpr float kSlot = 100.f, kKnobY = 466.f;
}  // namespace

Colour RelanceEditor::typeColour(int t) {
    static const Colour c[rel::kNumTypes] = {Colour(0xFF6FD6FF), Colour(0xFF9C8BFF), Colour(0xFFF2B33D), Colour(0xFF39D39A), Colour(0xFFFF8A5C), Colour(0xFFFF7FC8)};
    return t >= 0 && t < rel::kNumTypes ? c[t] : Colour(0xFFA9B0D0);
}

RelanceEditor::RelanceEditor(RelanceProcessor& p) : AudioProcessorEditor(&p), proc(p), pal(kit::w::rack(Colour(0xFFFF6A50)))   /* palette du rendu Classique */ {
    for (auto& d : kKnobs) {
        auto* k = k3.add(new v3::Knob3(proc.apvts, d.id, u8(d.label), u8(d.tip), v3::KnobSize::M));
        k->setLabelPx(11.5f);
        if (String(d.id) == "level") k->setPeach(true);
        addAndMakeVisible(k);
    }
    for (int i = 0; i < RelanceProcessor::numPresets(); ++i) presetBox.addItem(proc.getProgramName(i), i + 1);
    presetBox.addItem("Perso", 100);
    presetBox.setTooltip(u8("Point de d\xC3\xA9part : densit\xC3\xA9, audace, caract\xC3\xA8re, retard, capture, couleur. La sortie et le verrou de graine restent \xC3\xA0 toi."));
    presetBox.onChange = [this] { const int id = presetBox.getSelectedId(); if (id >= 1 && id <= RelanceProcessor::numPresets()) proc.setCurrentProgram(id - 1); };
    if (auto* cp = dynamic_cast<juce::AudioParameterChoice*>(proc.apvts.getParameter("carac"))) for (int i = 0; i < cp->choices.size(); ++i) caracBox.addItem(cp->choices[i], i + 1);
    caracAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(proc.apvts, "carac", caracBox);
    caracBox.setTooltip(u8("Caract\xC3\xA8re des relances. M\xC3\xA9lange : tir\xC3\xA9 au hasard \xC3\xA0 chaque phrase (\xC3\xA9" "cho de mot, octave basse, t\xC3\xA9l\xC3\xA9phone, invers\xC3\xA9, b\xC3\xA9gaiement, harmonie)."));
    addAndMakeVisible(presetBox); addAndMakeVisible(caracBox);
    delaySeg = std::make_unique<v3::bridge::Seg>(proc.apvts, "delay", juce::StringArray{"1/8", "1/4", "Hasard"},
        u8("Retard de la r\xC3\xA9ponse apr\xC3\xA8s la fin de phrase : une croche, deux croches, ou l'un ou l'autre au hasard. Cal\xC3\xA9 sur la grille du morceau pendant la lecture."), pal);
    lockSeg = std::make_unique<v3::bridge::Seg>(proc.apvts, "lock", juce::StringArray{"Libre", "Verrou"},
        u8("Verrou : \xC3\xA0 chaque lancement de la lecture, les tirages repartent du d\xC3\xA9" "but (m\xC3\xAAme prise = m\xC3\xAAmes relances, rendu reproductible). Libre : ils continuent d'un passage \xC3\xA0 l'autre."), pal);
    outSeg = std::make_unique<v3::bridge::Seg>(proc.apvts, "outmode", juce::StringArray{"Tout", "Relances"},
        u8("Tout : voix + relances (insert). Relances : les relances seules, pour un bus parall\xC3\xA8le (envoi \xC3\xA0 100 %)."), pal);
    for (auto* s : {delaySeg.get(), lockSeg.get(), outSeg.get()}) addAndMakeVisible(*s);
    inSw = std::make_unique<v3::InSwitch>(*proc.apvts.getParameter("bypass"));
    addAndMakeVisible(*inSw);
    help.install(*this, "Orbe Relance", {u8("Le backeur g\xC3\xA9n\xC3\xA9ratif : il \xC3\xA9" "coute la voix et lui r\xC3\xA9pond avec elle-m\xC3\xAAme."),
        u8("- \xC3\x80 chaque fin de phrase (voix puis 150 ms sous le seuil), les derni\xC3\xA8res syllabes sont captur\xC3\xA9" "es et rejou\xC3\xA9" "es une ou deux croches plus tard."),
        u8("- Caract\xC3\xA8res : \xC3\xA9" "cho de mot, octave basse, t\xC3\xA9l\xC3\xA9phone, invers\xC3\xA9, b\xC3\xA9gaiement, harmonie (quinte, octave haute avec de l'audace)."),
        u8("- \xC3\x89" "cran : enveloppe de la voix (bleu), seuil (pointill\xC3\xA9), fins de phrase (traits), relances (pastilles color\xC3\xA9" "es) reli\xC3\xA9" "es \xC3\xA0 leur phrase."),
        u8("- Densit\xC3\xA9 basse par d\xC3\xA9" "faut : quelques r\xC3\xA9ponses bien plac\xC3\xA9" "es. Graine + Verrou : rendu reproductible."),
        u8("- Sortie Relances : \xC3\xA0 mettre sur un bus parall\xC3\xA8le. Aucune latence ajout\xC3\xA9" "e sur la voix.")});
    addAndMakeVisible(themeBtn);
    themeBtn.onTheme = [this](v3::Mode m) { setThemeMode(m, true); };
    themeBtn.onScale = [this](float s) { setUiScale(s); };
    setResizable(true, true);
    scale_ = v3::Prefs::scale("relance", 1.f);
    setThemeMode(v3::Prefs::theme("relance", v3::Mode::Light), false);
    startTimerHz(30);
}

RelanceEditor::~RelanceEditor() { stopTimer(); v3::Prefs::setScale("relance", scale_); setLookAndFeel(nullptr); }

void RelanceEditor::setThemeMode(v3::Mode m, bool persist) {
    mode = m;
    if (m == v3::Mode::Classic) {   // Classique : graphite du rack Orbe, mêmes composants
        T = v3::make(v3::Mode::Dark, Colour(0xFFFF6A50));
        T.bgTop = Colour(0xFF34343A); T.bgBot = Colour(0xFF202024); T.card = Colour(0xFF2A2A2F); T.cardEdge = Colour(0x22FFFFFF);
        T.field = Colour(0xFF1E1E22); T.fieldEdge = Colour(0xFF46464E); T.track = Colour(0xFF3C3C44);
        T.capTop = Colour(0xFF55555C); T.capBot = Colour(0xFF1A1A1D); T.capRim = Colour(0xFF5A5A62);
        T.accentText = v3::ensureContrast(Colour(0xFFFF8A70), T.card);
        T.mode = v3::Mode::Classic;
    } else {
        T = v3::make(m, kAccent);
    }
    look3.setTheme(T);
    art.clear();
    setLookAndFeel(&look3);
    for (auto* k : k3) k->setup(&T, &art);
    for (auto* s : {delaySeg.get(), lockSeg.get(), outSeg.get()}) { s->T = &T; s->repaint(); }
    help.T = &T; inSw->setup(&T);
    themeBtn.T = &T; themeBtn.current = m;
    if (persist) v3::Prefs::setTheme("relance", m);
    setResizeLimits(juce::roundToInt(kW3 * 0.6f), juce::roundToInt(kH3 * 0.6f), juce::roundToInt(kW3 * 1.5f), juce::roundToInt(kH3 * 1.5f));
    getConstrainer()->setFixedAspectRatio(kW3 / kH3);
    scale_ = juce::jlimit(0.75f, 1.5f, scale_);
    const int w = juce::roundToInt(kW3 * scale_), h = juce::roundToInt(kH3 * scale_);
    if (getWidth() == w && getHeight() == h) resized(); else setSize(w, h);
    sendLookAndFeelChange();
    repaint();
}

void RelanceEditor::setUiScale(float s) {
    scale_ = juce::jlimit(0.75f, 1.5f, s);
    setSize(juce::roundToInt(kW3 * scale_), juce::roundToInt(kH3 * scale_));
    v3::Prefs::setScale("relance", scale_);
}

void RelanceEditor::resized() {
    u_ = (float)getWidth() / kW3;
    scale_ = u_; themeBtn.scale = scale_;
    help.layout(Si(976, 34, 30, 30), getLocalBounds());
    themeBtn.setBounds(Si(942, 35, 28, 28));
    inSw->setBounds(Si(1012, 36, 70, 26));
    presetBox.setBounds(Si(560, 34, 300, 32));
    const float kh = v3::Knob3::boxHeight(v3::KnobSize::M, false);
    for (int i = 0; i < k3.size(); ++i) { k3[i]->setScale(u_); k3[i]->setBounds(Si(kKnobs[i].cx - kSlot * 0.5f, kKnobY, kSlot, kh)); }
    caracBox.setBounds(Si(kSecX[0] + 10, 614, kSecX[1] - kSecX[0] - 24, 32));
    delaySeg->setBounds(Si(kSecX[1] + 12, 614, kSecX[2] - kSecX[1] - 24, 32)); delaySeg->px = 11.5f * u_;
    lockSeg->setBounds(Si(kSecX[3] + 10, 614, kSecX[4] - kSecX[3] - 20, 32)); lockSeg->px = 11.5f * u_;
    outSeg->setBounds(Si(kSecX[4] + 10, 614, kSecX[5] - kSecX[4] - 20, 32)); outSeg->px = 11.5f * u_;
}

void RelanceEditor::paintBackdrop(juce::Graphics& g, Rectangle<float> b) {
    const float u = b.getWidth() / kW3;
    auto R = [u](float x, float y, float w, float h) { return Rectangle<float>(x * u, y * u, w * u, h * u); };
    v3::background(g, T, b);
    g.setColour(T.inkMute); v3::caps(g, "dreamaws", R(29, 14, 160, 16), 12.f * u, juce::Justification::centredLeft);
    const auto big = v3::font(30.f * u, true);
    g.setColour(T.ink); tracked(g, "Orbe", R(28, 30, 120, 34), juce::Justification::centredLeft, 2.6f * u, big);
    const float ow = v3::tabularWidth("ORBE", big) + 3.f * 2.6f * u + 12.f * u;
    g.setColour(T.accentText); tracked(g, "Relance", Rectangle<float>(28.f * u + ow, 30.f * u, 220.f * u, 34.f * u), juce::Justification::centredLeft, 2.6f * u, big);
    g.setColour(T.accent); g.fillRoundedRectangle(R(30, 66, 30, 3), 1.5f * u);
    g.setColour(T.inkFaint); v3::caps(g, kit::w::serial::relance, R(300, 40, 120, 20), 11.f * u, juce::Justification::centredLeft, false);
    g.setColour(T.inkMute); v3::caps(g, "Preset", R(562, 14, 120, 16), 11.f * u, juce::Justification::centredLeft);
    v3::hero(g, T, R(20, 82, 763, 330), 20.f * u);
    v3::hero(g, T, R(800, 82, 280, 330), 18.f * u);
    v3::card(g, T, R(20, 428, 1060, 258), 18.f * u);
    for (int i = 0; i < 5; ++i) {
        g.setColour(T.inkMute); v3::caps(g, u8(kSecTitle[i]), R(kSecX[i] + 12, 442, kSecX[i + 1] - kSecX[i] - 16, 16), 11.f * u, juce::Justification::centredLeft);
        if (i > 0) { g.setColour(T.dark ? juce::Colours::white.withAlpha(0.07f) : juce::Colour(0x14101828)); g.fillRect(R(kSecX[i], 444, 1, 216)); }
    }
    g.setColour(T.inkMute);
    v3::caps(g, u8("Caract\xC3\xA8re"), R(kSecX[0] + 12, 596, 200, 16), 10.5f * u, juce::Justification::centredLeft, false);
    v3::caps(g, "Retard", R(kSecX[1] + 14, 596, 180, 16), 10.5f * u, juce::Justification::centredLeft, false);
    v3::caps(g, "Tirage", R(kSecX[3] + 12, 596, 140, 16), 10.5f * u, juce::Justification::centredLeft, false);
    v3::caps(g, "Sortie", R(kSecX[4] + 12, 596, 140, 16), 10.5f * u, juce::Justification::centredLeft, false);
    g.setColour(T.scrMute);
    v3::caps(g, "Relances", R(818, 96, 200, 16), 11.f * u, juce::Justification::centredLeft);
    v3::caps(g, u8("Derni\xC3\xA8re r\xC3\xA9ponse"), R(818, 206, 240, 16), 11.f * u, juce::Justification::centredLeft);
    v3::caps(g, "Voix actives", R(818, 322, 240, 16), 11.f * u, juce::Justification::centredLeft);
}

void RelanceEditor::paint(juce::Graphics& g) {
    art.draw(g, String("bd|") + v3::modeKey(mode), getLocalBounds().toFloat(), [this](juce::Graphics& ig, Rectangle<float> b) { paintBackdrop(ig, b); });
    paintScreen(g, S(20, 82, 763, 330));
    paintReadout(g, S(800, 82, 280, 330));
}

// Écran héros : 5 s d'activité défilantes + zone « à venir » (relances programmées).
void RelanceEditor::paintScreen(juce::Graphics& g, Rectangle<float> r) {
    const float u = u_;
    const auto& t = proc.tel();
    const int N = rel::Telemetry::N, wr = t.wr.load(std::memory_order_acquire), now = t.frameAbs.load();
    for (int i = 0; i < N; ++i) {
        const int k = (wr + i) % N;
        const float a = t.inDb[k].load(), b = t.wetDb[k].load();
        sIn[i] = a > -60.f ? a + 60.f : 0.f; sWet[i] = b > -60.f ? b + 60.f : 0.f; sVo[i] = t.voiced[k].load();
    }
    const int F = 40;   // trames futures visibles
    auto p = Rectangle<float>(r.getX() + 52.f * u, r.getY() + 64.f * u, r.getWidth() - 72.f * u, r.getHeight() - 104.f * u);
    auto X = [&](float i) { return p.getX() + p.getWidth() * i / (float)(N - 1 + F); };   // i : 0..N-1 = passé, N-1 = maintenant
    auto XA = [&](int absFrame) { return X((float)(N - 1 - (now - absFrame))); };
    auto Y = [&](float v) { return p.getBottom() - p.getHeight() * juce::jlimit(0.f, 1.f, v / 60.f); };
    // grille de niveaux
    g.setFont(v3::font(11.f * u));
    for (int db = 0; db >= -60; db -= 20) {
        const float y = Y((float)db + 60.f);
        g.setColour(T.scrGrid); g.fillRect(p.getX(), y - 0.5f, p.getWidth(), 1.f);
        g.setColour(T.scrMute); g.drawText(db == 0 ? String("0") : v3::fmt::minus() + String(-db), Rectangle<float>(r.getX() + 8.f * u, y - 8.f * u, 36.f * u, 16.f * u), juce::Justification::centredRight, false);
    }
    const float xNow = X((float)(N - 1));
    g.setColour(T.scrMute.withAlpha(0.06f)); g.fillRect(Rectangle<float>(xNow, p.getY(), p.getRight() - xNow, p.getHeight()));
    g.setColour(T.scrMute.withAlpha(0.45f)); g.fillRect(xNow - 0.5f, p.getY(), 1.f, p.getHeight());
    // seuil
    const float thr = proc.raw("thresh") + 60.f, yT = Y(thr);
    {
        juce::Path d; d.startNewSubPath(p.getX(), yT); d.lineTo(xNow, yT);
        juce::Path dash; const float dl[2] = {5.f * u, 4.f * u};
        juce::PathStrokeType(1.f).createDashedStroke(dash, d, dl, 2);
        g.setColour(T.scrInk.withAlpha(0.5f)); g.fillPath(dash);
    }
    // voisement : fine bande sous le tracé
    for (int i = 0; i < N; ++i) if (sVo[i]) { g.setColour(T.sung.withAlpha(0.55f)); g.fillRect(Rectangle<float>(X((float)i) - 0.5f * u, p.getBottom() + 6.f * u, juce::jmax(1.f, p.getWidth() / (float)(N + F)), 4.f * u)); }
    {
        juce::Graphics::ScopedSaveState st(g);
        g.reduceClipRegion(p.expanded(4.f * u, 6.f * u).toNearestInt());
        const v3::Trace::Opts o{N, 1.2f, 1e9f, 0.5f, 3};
        trIn.build(sIn, N, X, Y, p.getBottom(), o);
        trWet.build(sWet, N, X, Y, p.getBottom(), o);
        trIn.draw(g, T.sung, 1.6f * u, 0.7f, 0.16f, p.getBottom(), false);
        trWet.draw(g, T.out, 2.2f * u, 1.f, 0.10f, p.getBottom(), true);
    }
    // événements : fins de phrase et relances
    const int ew = t.evWr.load(std::memory_order_acquire);
    for (int j = std::max(0, ew - rel::Telemetry::kEv); j < ew; ++j) {
        const int k = j % rel::Telemetry::kEv;
        const int fr = t.evFrame[k].load(), ty = t.evType[k].load(), s0 = t.evStart[k].load(), du = t.evDur[k].load();
        const bool endVisible = now - fr < N - 1;
        const int endF = ty >= 0 ? s0 + du : fr;
        if (now - endF >= N - 1) continue;
        const float xe = XA(fr);
        if (endVisible) {
            g.setColour(T.scrInk.withAlpha(ty >= 0 ? 0.55f : 0.25f)); g.fillRect(xe - 0.75f * u, p.getY() + 4.f * u, 1.5f * u, p.getHeight() - 4.f * u);
            g.fillEllipse(Rectangle<float>(5.f * u, 5.f * u).withCentre({xe, p.getY() + 4.f * u}));
        }
        if (ty < 0) continue;
        const auto c = typeColour(ty);
        const float x0 = juce::jmax(p.getX(), XA(s0)), x1 = juce::jmin(p.getRight(), juce::jmax(x0 + 18.f * u, XA(s0 + du)));
        auto pill = Rectangle<float>(x0, p.getY() - 22.f * u, x1 - x0, 18.f * u);
        if (endVisible) {   // arc de la phrase à sa réponse
            juce::Path arc; arc.startNewSubPath(xe, p.getY() + 4.f * u);
            arc.cubicTo(xe, pill.getBottom() - 2.f * u, x0, pill.getBottom() + 6.f * u, x0 + 4.f * u, pill.getBottom());
            g.setColour(c.withAlpha(0.6f)); g.strokePath(arc, juce::PathStrokeType(1.3f * u));
        }
        g.setColour(c.withAlpha(0.24f)); g.fillRoundedRectangle(pill, 9.f * u);
        g.setColour(c); g.drawRoundedRectangle(pill.reduced(0.5f), 9.f * u, 1.2f * u);
        g.setColour(T.scrInk); g.setFont(v3::font(11.5f * u, true));
        const String nm = u8(rel::typeName(ty));
        if (pill.getWidth() > v3::tabularWidth(nm, v3::font(11.5f * u, true)) + 16.f * u) g.drawText(nm, pill.reduced(7.f * u, 0), juce::Justification::centredLeft, false);
        else { static const char* ab[rel::kNumTypes] = {"E", "8", "T", "R", "B", "H"}; g.drawText(ab[ty], pill, juce::Justification::centred, false); }
    }
    // titre d'état et légende
    const int act = t.active.load();
    String state = "En attente de la voix";
    bool anyVoice = false; for (int i = N - 8; i < N; ++i) anyVoice = anyVoice || sIn[i] > thr;
    if (act > 0 && t.lastType.load() >= 0) state = u8("Relance : ") + u8(rel::typeName(t.lastType.load()));
    else if (anyVoice) state = u8("Phrase en cours");
    else if (t.phrases.load() > 0) state = u8("\xC3\x80 l'\xC3\xA9" "coute");
    g.setColour(act > 0 ? T.out : T.scrInk); g.setFont(v3::font(15.f * u, true));
    g.drawText(state, Rectangle<float>(r.getX() + 20.f * u, r.getY() + 10.f * u, 330.f * u, 22.f * u), juce::Justification::centredLeft, false);
    const char* leg[3] = {"Voix", "Relances", "Seuil"};
    const Colour lc[3] = {T.sung, T.out, T.scrInk.withAlpha(0.6f)};
    for (int i = 0; i < 3; ++i) {
        const float x = r.getRight() - (float)(3 - i) * 92.f * u - 12.f * u;
        g.setColour(lc[i]); g.fillRoundedRectangle(Rectangle<float>(x, r.getY() + 19.f * u, 14.f * u, 3.f * u), 1.5f * u);
        g.setColour(T.scrMute); v3::caps(g, u8(leg[i]), Rectangle<float>(x + 20.f * u, r.getY() + 11.f * u, 70.f * u, 18.f * u), 11.f * u, juce::Justification::centredLeft);
    }
    const float total = (float)N * t.hopSec.load();
    g.setColour(T.scrMute); g.setFont(v3::font(11.5f * u));
    g.drawText(v3::fmt::minus() + String(juce::roundToInt(total)) + juce::String::charToString((juce::juce_wchar)0x00A0) + "s", Rectangle<float>(p.getX(), r.getBottom() - 24.f * u, 60.f * u, 18.f * u), juce::Justification::centredLeft, false);
    g.drawText("maintenant", Rectangle<float>(xNow - 45.f * u, r.getBottom() - 24.f * u, 90.f * u, 18.f * u), juce::Justification::centred, false);
    g.drawText(u8("\xC3\xA0 venir"), Rectangle<float>(xNow + 4.f * u, r.getBottom() - 24.f * u, p.getRight() - xNow, 18.f * u), juce::Justification::centredRight, false);
}

void RelanceEditor::paintReadout(juce::Graphics& g, Rectangle<float> r) {
    const float u = u_;
    const auto& t = proc.tel();
    const int nr = t.relances.load(), np = t.phrases.load(), lt = t.lastType.load(), act = t.active.load();
    const auto big = v3::font(50.f * u, true);
    g.setColour(T.out); v3::tabular(g, String(nr), Rectangle<float>(r.getX() + 16.f * u, r.getY() + 32.f * u, 120.f * u, 64.f * u), big, juce::Justification::centredLeft);
    g.setColour(T.scrMute); g.setFont(v3::font(14.f * u));
    g.drawText(u8("sur ") + String(np) + (np > 1 ? " phrases" : " phrase"), Rectangle<float>(r.getX() + 140.f * u, r.getY() + 58.f * u, 130.f * u, 22.f * u), juce::Justification::centredLeft, false);
    // densité réelle
    const float ratio = np > 0 ? (float)nr / (float)np : 0.f;
    auto bar = Rectangle<float>(r.getX() + 18.f * u, r.getY() + 104.f * u, r.getWidth() - 36.f * u, 5.f * u);
    g.setColour(juce::Colours::white.withAlpha(0.08f)); g.fillRoundedRectangle(bar, 2.5f * u);
    g.setColour(T.out); g.fillRoundedRectangle(bar.withWidth(juce::jmax(bar.getHeight(), bar.getWidth() * ratio)), 2.5f * u);
    // dernière réponse
    g.setColour(lt >= 0 ? typeColour(lt) : T.scrMute); g.setFont(v3::font(30.f * u, true));
    g.drawText(lt >= 0 ? u8(rel::typeName(lt)) : String(v3::fmt::minus() + v3::fmt::minus()), Rectangle<float>(r.getX() + 16.f * u, r.getY() + 144.f * u, r.getWidth() - 32.f * u, 40.f * u), juce::Justification::centredLeft, true);
    // pastilles des six caractères (le dernier allumé)
    const float d = 30.f * u, gap = (r.getWidth() - 32.f * u - 6.f * d) / 5.f;
    for (int i = 0; i < rel::kNumTypes; ++i) {
        auto c = Rectangle<float>(r.getX() + 16.f * u + (float)i * (d + gap), r.getY() + 196.f * u, d, d);
        const auto col = typeColour(i);
        g.setColour(col.withAlpha(i == lt ? 0.9f : 0.16f)); g.fillEllipse(c);
        if (i == lt && act > 0) { g.setColour(col.withAlpha(0.3f)); g.fillEllipse(c.expanded(5.f * u)); }
        g.setColour(i == lt ? juce::Colour(0xFF0A0D1C) : T.scrMute); g.setFont(v3::font(12.f * u, true));
        const char* ab[rel::kNumTypes] = {"E", "8", "T", "R", "B", "H"};
        g.drawText(ab[i], c, juce::Justification::centred, false);
    }
    // voix de relance actives
    for (int i = 0; i < rel::Engine::kVoices; ++i) {
        auto c = Rectangle<float>(14.f * u, 14.f * u).withCentre({r.getX() + 26.f * u + (float)i * 26.f * u, r.getY() + 286.f * u});
        g.setColour(i < act ? T.out : juce::Colours::white.withAlpha(0.10f)); g.fillEllipse(c);
        if (i < act) { g.setColour(T.out.withAlpha(0.25f)); g.fillEllipse(c.expanded(4.f * u)); }
    }
    g.setColour(T.scrMute); g.setFont(v3::font(12.5f * u));
    const String outTxt = juce::roundToInt(proc.raw("outmode")) == 1 ? u8("relances seules") : u8("voix + relances");
    g.drawText(outTxt, Rectangle<float>(r.getX() + 130.f * u, r.getY() + 276.f * u, r.getWidth() - 146.f * u, 20.f * u), juce::Justification::centredRight, true);
}

void RelanceEditor::timerCallback() {
    int sel = 100;
    for (int i = 0; i < RelanceProcessor::numPresets(); ++i) if (proc.presetMatches(i)) { sel = i + 1; if (i == proc.getCurrentProgram()) break; }
    if (presetBox.getSelectedId() != sel) presetBox.setSelectedId(sel, juce::dontSendNotification);
    for (auto* k : k3) k->tickMod(1.f / 30.f);
    if (proc.getCurrentProgram() != lastProg) { lastProg = proc.getCurrentProgram(); for (auto* k : k3) k->resetMod(); }
    repaint(S(20, 82, 1060, 330).toNearestInt());
}
