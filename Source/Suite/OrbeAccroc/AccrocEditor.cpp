#include "AccrocEditor.h"

using namespace of;
using kit::u8;
namespace v3 = kit::v3;

namespace {
const Colour kAcid(0xFFCFF56A);   // accroc : vert acide de signal
const Colour kInk(0xFFF1FFD2);
const Colour kTypeCol[acr::kNumTypes] = {Colour(0xFFCFF56A), Colour(0xFF8FD8FF), Colour(0xFFFFB86B), Colour(0xFFC9C2FF),
                                         Colour(0xFFFF8FC8), Colour(0xFFFF6B6B), Colour(0xFFFFE36B), Colour(0xFF7DF2C0)};
const char* const kTypeTip[acr::kNumTypes] = {
    "Stutter : r\xC3\xA9p\xC3\xA8te une tranche tr\xC3\xA8s courte (1/4 \xC3\xA0 1/64), avec d\xC3\xA9" "clin et rampe de hauteur.",
    "Retour : relit chaque tranche \xC3\xA0 l'envers.",
    "Arr\xC3\xAAt : la bande ralentit jusqu'\xC3\xA0 l'arr\xC3\xAAt (ou plonge en hauteur si la profondeur est partielle).",
    "Gel : fige un instant et le prolonge en grains.",
    "B\xC3\xA9gaie : r\xC3\xA9p\xC3\xA8te une syllabe avec un court silence entre les reprises.",
    "Crush : r\xC3\xA9" "duction de bits et de fr\xC3\xA9quence, lissable.",
    "Saut : la lecture saute en arri\xC3\xA8re de quelques doubles-croches, au hasard.",
    "Trous : micro-coupures al\xC3\xA9" "atoires sur la grille choisie."};
int ch(const AccrocProcessor& p, const char* id) { return juce::roundToInt(p.raw(id)); }

// Titre de section gravé (≥ 9 px) + filet ; pastille de couleur optionnelle.
void sec(juce::Graphics& g, const kit::Palette& pal, float u, const String& title, float x, float y, float w, Colour dot = {}) {
    float tx = x;
    if (!dot.isTransparent()) { kit::led(g, (x + 4) * u, (y + 6) * u, 3.f * u, 1.f, dot, kit::LedBezel::None); tx += 12; }
    kit::engraved(g, pal, title, Rectangle<float>(tx * u, y * u, (w - (tx - x) - 2) * u, 12 * u), juce::Justification::centredLeft, 9.6f * u, 1.6f * u, true);
    g.setColour(juce::Colours::black.withAlpha(0.5f)); g.fillRect(Rectangle<float>(x * u, (y + 15) * u, w * u, 1.2f * u));
    g.setColour(juce::Colours::white.withAlpha(0.07f)); g.fillRect(Rectangle<float>(x * u, (y + 16.2f) * u, w * u, u));
}
}  // namespace

// ------------------------------------------------------------------ séquenceur 16 pas
// Rangée TYPE : clic = peindre avec le pinceau, clic droit = effacer. INTENS. / PROBA : cliquer-glisser (plusieurs pas d'un geste).
// DURÉE : clic = +1 pas, clic droit = -1 (molette aussi).
class SeqView : public juce::Component, public juce::SettableTooltipClient {
public:
    SeqView(AccrocProcessor& p, std::function<int()> brushFn) : proc(p), brush(std::move(brushFn)) {
        setOpaque(true);
        setTooltip(u8("TYPE : clic = peindre avec le pinceau choisi dessous, clic droit = effacer. INTENS. et PROBA : cliquer-glisser. "
                      "DUR\xC3\x89" "E : clic +1 pas, clic droit -1 (ou molette)."));
    }
    const v3::Theme* T = nullptr;   // nul : rendu Classique
    void setTheme(const v3::Theme* t) { T = t; setOpaque(t == nullptr); repaint(); }
    void paint3(juce::Graphics& g) {
        const auto& t = *T; const float u = (float)getWidth() / 688.f;
        const auto& q = proc.seq(); const int now = proc.stepNow();
        const char* rows[4] = {"Type", "Intens.", "Proba", nullptr};
        const float ry[4] = {kTypeY, kIntY, kProbY, kLenY}, rh[4] = {kTypeH, kBarH, kBarH, kLenH};
        for (int k = 0; k < 4; ++k) { g.setColour(t.scrMute); v3::caps(g, k < 3 ? String(rows[k]) : u8("Dur\xC3\xA9" "e"), Rectangle<float>(14 * u, ry[k] * u, 56 * u, rh[k] * u), 11.f * u, juce::Justification::centredLeft); }
        for (int i = 0; i < 16; ++i) {
            const auto c = col(i, u); const auto& s = q.s[(size_t)i];
            const bool play = i == now, beat = i % 4 == 0;
            if (play) { g.setColour(t.accent.withAlpha(0.14f)); g.fillRoundedRectangle(c.getX() - 1 * u, 4 * u, c.getWidth() + 2 * u, (kLenY + kLenH) * u, 6 * u); }
            g.setColour(play ? t.accent.brighter(0.5f) : beat ? t.scrInk : t.scrMute); g.setFont(v3::font(12.f * u, play || beat));
            g.drawText(String(i + 1), Rectangle<float>(c.getX(), 5 * u, c.getWidth(), 14 * u), juce::Justification::centred, false);
            auto tile = Rectangle<float>(c.getX() + 2 * u, kTypeY * u, c.getWidth() - 4 * u, kTypeH * u);
            if (s.type >= 0) {
                const Colour tc = kTypeCol[s.type];
                g.setColour(tc.withAlpha(0.25f + 0.65f * s.prob)); g.fillRoundedRectangle(tile, 8 * u);
                g.setColour(Colour(0xFF0B0C14)); g.setFont(v3::font(11.5f * u, true));
                g.drawText(u8(acr::kTypeShort[s.type]), tile, juce::Justification::centred, false);
                const int end = juce::jmin(16, i + s.len); const float x1 = col(end - 1, u).getRight() - 2 * u;
                g.setColour(tc.withAlpha(0.85f)); g.fillRoundedRectangle(tile.getX(), (kTypeY + kTypeH + 3) * u, x1 - tile.getX(), 3 * u, 1.5f * u);
            } else { g.setColour(juce::Colours::white.withAlpha(beat ? 0.12f : 0.07f)); g.drawRoundedRectangle(tile, 8 * u, 1.f); }
            auto bar = [&](float y, float v, Colour cc) {
                auto r = Rectangle<float>(c.getX() + 6 * u, y * u, c.getWidth() - 12 * u, kBarH * u);
                g.setColour(juce::Colours::white.withAlpha(0.06f)); g.fillRoundedRectangle(r, 4 * u);
                if (s.type < 0 || v <= 0.f) return;
                g.setColour(cc); g.fillRoundedRectangle(r.withTop(juce::jmin(r.getBottom() - 4 * u, r.getBottom() - r.getHeight() * v)), 4 * u);
            };
            const Colour tc = s.type >= 0 ? kTypeCol[s.type] : t.scrMute;
            bar(kIntY, s.inten, tc.withAlpha(0.9f)); bar(kProbY, s.prob, t.scrInk.withAlpha(0.55f));
            g.setColour(s.type >= 0 ? t.scrInk : t.scrMute.withAlpha(0.6f)); g.setFont(v3::font(12.f * u, true));
            g.drawText(String(s.len), Rectangle<float>(c.getX(), kLenY * u, c.getWidth(), kLenH * u), juce::Justification::centred, false);
        }
    }
    void paint(juce::Graphics& g) override {
        if (T != nullptr) { paint3(g); return; }
        const float u = (float)getWidth() / 688.f; const auto pal = of::palette(kAcid);
        kit::screenWell(g, pal, getLocalBounds().toFloat(), kit::Display::OLED);
        const auto& q = proc.seq(); const int now = proc.stepNow();
        g.setFont(kit::mono(9.4f * u));
        const char* rows[4] = {"TYPE", "INTENS.", "PROBA", nullptr};
        const float ry[4] = {kTypeY, kIntY, kProbY, kLenY}, rh[4] = {kTypeH, kBarH, kBarH, kLenH};
        for (int k = 0; k < 4; ++k) {
            g.setColour(pal.mute.brighter(0.25f));
            g.drawText(k < 3 ? String(rows[k]) : u8("DUR\xC3\x89" "E"), Rectangle<float>(8 * u, ry[k] * u, 58 * u, rh[k] * u), juce::Justification::centredLeft);
        }
        for (int i = 0; i < 16; ++i) {
            const auto c = col(i, u); const auto& s = q.s[(size_t)i];
            const bool play = i == now, beat = i % 4 == 0;
            if (play) { g.setColour(kAcid.withAlpha(0.10f)); g.fillRoundedRectangle(c.getX() - 1 * u, 3 * u, c.getWidth() + 2 * u, (kLenY + kLenH + 2) * u, 3 * u); }
            g.setColour(play ? kAcid : beat ? kInk.withAlpha(0.75f) : pal.mute);
            g.drawText(String(i + 1), Rectangle<float>(c.getX(), 4 * u, c.getWidth(), 13 * u), juce::Justification::centred);
            // tuile de type
            auto tile = Rectangle<float>(c.getX() + 2 * u, kTypeY * u, c.getWidth() - 4 * u, kTypeH * u);
            if (s.type >= 0) {
                const Colour tc = kTypeCol[s.type];
                g.setColour(tc.withAlpha(0.25f + 0.65f * s.prob)); g.fillRoundedRectangle(tile, 3 * u);
                g.setColour(Colour(0xFF0B0C0A)); g.setFont(kit::mono(9.6f * u));
                g.drawText(u8(acr::kTypeShort[s.type]), tile, juce::Justification::centred);
                // durée de l'accident : trait sous les pas couverts
                const int end = juce::jmin(16, i + s.len);
                const float x1 = col(end - 1, u).getRight() - 2 * u;
                g.setColour(tc.withAlpha(0.8f)); g.fillRoundedRectangle(tile.getX(), (kTypeY + kTypeH + 3) * u, x1 - tile.getX(), 3 * u, 1.5f * u);
            } else {
                g.setColour(juce::Colours::white.withAlpha(beat ? 0.10f : 0.06f)); g.drawRoundedRectangle(tile, 3 * u, 1.f);
            }
            // barres
            auto bar = [&](float y, float v, Colour cc) {
                auto r = Rectangle<float>(c.getX() + 5 * u, y * u, c.getWidth() - 10 * u, kBarH * u);
                g.setColour(juce::Colours::white.withAlpha(0.05f)); g.fillRoundedRectangle(r, 2 * u);
                if (s.type < 0) return;
                g.setColour(cc); g.fillRoundedRectangle(r.withTop(r.getBottom() - r.getHeight() * v), 2 * u);
            };
            const Colour tc = s.type >= 0 ? kTypeCol[s.type] : pal.mute;
            bar(kIntY, s.inten, tc.withAlpha(0.85f));
            bar(kProbY, s.prob, kInk.withAlpha(0.55f));
            g.setFont(kit::mono(9.4f * u));
            g.setColour(s.type >= 0 ? kInk.withAlpha(0.85f) : pal.mute.withAlpha(0.6f));
            g.drawText(String(s.len), Rectangle<float>(c.getX(), kLenY * u, c.getWidth(), kLenH * u), juce::Justification::centred);
        }
    }
    void mouseDown(const juce::MouseEvent& e) override { dragRow = rowAt(e.position.y); edit(e, true); }
    void mouseDrag(const juce::MouseEvent& e) override { if (dragRow == 1 || dragRow == 2 || dragRow == 0) edit(e, false); }
    void mouseUp(const juce::MouseEvent&) override { dragRow = -1; lastCol = -1; }
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wd) override {
        const int i = colAt(e.position.x); if (i < 0 || wd.deltaY == 0.f) return;
        auto q = proc.seq(); auto& s = q.s[(size_t)i]; s.len = juce::jlimit(1, acr::kMaxLen, s.len + (wd.deltaY > 0 ? 1 : -1)); proc.setSeq(q);
    }
private:
    static constexpr float kGut = 66.f, kTypeY = 20.f, kTypeH = 40.f, kIntY = 70.f, kProbY = 120.f, kBarH = 42.f, kLenY = 170.f, kLenH = 20.f;
    Rectangle<float> col(int i, float u) const { const float cw = (688.f - kGut - 6.f) / 16.f; return {(kGut + cw * (float)i) * u, 0, cw * u, (float)getHeight()}; }
    int colAt(float x) const { const float u = (float)getWidth() / 688.f; const float cw = (688.f - kGut - 6.f) / 16.f; const int i = (int)std::floor((x / u - kGut) / cw); return i >= 0 && i < 16 ? i : -1; }
    int rowAt(float y) const { const float v = y / ((float)getWidth() / 688.f); return v < kIntY - 4 ? 0 : v < kProbY - 4 ? 1 : v < kLenY - 4 ? 2 : 3; }
    void edit(const juce::MouseEvent& e, bool down) {
        const int i = colAt(e.position.x); if (i < 0) return;
        const float u = (float)getWidth() / 688.f;
        auto q = proc.seq(); auto& s = q.s[(size_t)i];
        if (dragRow == 0) {
            if (!down && i == lastCol) return;
            lastCol = i;
            const int b = brush();
            if (e.mods.isPopupMenu() || b < 0) s.type = acr::None;
            else { if (s.type != b) { s.type = b; if (s.inten <= 0.f) s.inten = 0.9f; } else if (down) s.type = acr::None; }   // re-clic sur le même type : efface
        } else if (dragRow == 1 || dragRow == 2) {
            const float y0 = (dragRow == 1 ? kIntY : kProbY) * u, v = juce::jlimit(0.f, 1.f, (y0 + kBarH * u - e.position.y) / (kBarH * u));
            (dragRow == 1 ? s.inten : s.prob) = std::round(v * 20.f) / 20.f;
        } else if (dragRow == 3 && down) {
            s.len = e.mods.isPopupMenu() ? (s.len <= 1 ? acr::kMaxLen : s.len - 1) : (s.len >= acr::kMaxLen ? 1 : s.len + 1);
        }
        proc.setSeq(q); repaint();
    }
    AccrocProcessor& proc; std::function<int()> brush;
    int dragRow = -1, lastCol = -1;
};

// ------------------------------------------------------------------ état en direct : accidents actifs, pas, tempo
class StatusView : public juce::Component {
public:
    explicit StatusView(AccrocProcessor& p) : proc(p) { setOpaque(true); }
    const v3::Theme* T = nullptr;
    void setTheme(const v3::Theme* t) { T = t; setOpaque(t == nullptr); repaint(); }
    void paint3(juce::Graphics& g) {
        const auto& t = *T; const float u = (float)getWidth() / 336.f;
        g.setColour(t.scrMute); v3::caps(g, "En cours", Rectangle<float>(14 * u, 6 * u, 100 * u, 14 * u), 10.5f * u, juce::Justification::centredLeft);
        const int st = proc.stepNow();
        g.setColour(t.accent.brighter(0.4f));
        v3::tabular(g, (st >= 0 ? "Pas " + String(st + 1).paddedLeft('0', 2) + "/16" : String("Pas --/16")) + u8("  \xC2\xB7  ") + v3::fmt::withUnit(v3::fmt::num(proc.bpmNow(), 1), "BPM"), Rectangle<float>(110 * u, 4 * u, 214 * u, 18 * u), v3::font(12.5f * u, true), juce::Justification::centredRight);
        for (int k = 0; k < acr::kNumTypes; ++k) {
            const float x = 14 * u + (float)(k % 4) * 78 * u, y = 30 * u + (float)(k / 4) * 32 * u;
            const float a = juce::jlimit(0.f, 1.f, proc.activity(k));
            if (a > 0.01f) { g.setColour(kTypeCol[k].withAlpha(0.28f * a)); g.fillEllipse(Rectangle<float>(13 * u, 13 * u).withCentre({x + 5 * u, y + 7 * u})); }
            g.setColour(a > 0.01f ? kTypeCol[k] : t.scrMute.withAlpha(0.5f)); g.fillEllipse(Rectangle<float>(6 * u, 6 * u).withCentre({x + 5 * u, y + 7 * u}));
            g.setColour(a > 0.01f ? t.scrInk : t.scrMute); g.setFont(v3::font(11.f * u, a > 0.01f));
            g.drawText(u8(acr::kTypeShort[k]), Rectangle<float>(x + 14 * u, y, 56 * u, 14 * u), juce::Justification::centredLeft, false);
            g.setColour(juce::Colours::white.withAlpha(0.08f)); g.fillRoundedRectangle(x + 14 * u, y + 17 * u, 56 * u, 4 * u, 2 * u);
            g.setColour(kTypeCol[k]); g.fillRoundedRectangle(x + 14 * u, y + 17 * u, juce::jmax(0.f, 56 * u * a), 4 * u, 2 * u);
        }
    }
    void paint(juce::Graphics& g) override {
        if (T != nullptr) { paint3(g); return; }
        const float u = (float)getWidth() / 336.f; const auto pal = of::palette(kAcid);
        kit::screenWell(g, pal, getLocalBounds().toFloat(), kit::Display::OLED);
        g.setFont(kit::mono(9.6f * u));
        g.setColour(pal.mute.brighter(0.25f)); g.drawText("EN COURS", Rectangle<float>(10 * u, 5 * u, 120 * u, 13 * u), juce::Justification::centredLeft);
        const int st = proc.stepNow();
        g.setColour(kAcid);
        g.drawText((st >= 0 ? "PAS " + String(st + 1).paddedLeft('0', 2) + "/16" : String("PAS --/16")) + u8(" \xC2\xB7 ") + kit::w::fr(proc.bpmNow(), 1) + " BPM",
                   Rectangle<float>(120 * u, 5 * u, 206 * u, 13 * u), juce::Justification::centredRight);
        for (int k = 0; k < acr::kNumTypes; ++k) {
            const float x = 10 * u + (float)(k % 4) * 80 * u, y = 28 * u + (float)(k / 4) * 32 * u;
            const float a = juce::jlimit(0.f, 1.f, proc.activity(k));
            kit::led(g, x + 6 * u, y + 8 * u, 4.f * u, a > 0.01f ? 0.3f + 0.7f * a : 0.f, kTypeCol[k], kit::LedBezel::Black);
            g.setColour(a > 0.01f ? kInk : pal.mute.brighter(0.1f)); g.setFont(kit::mono(9.6f * u));
            g.drawText(u8(acr::kTypeShort[k]), Rectangle<float>(x + 15 * u, y + 1 * u, 56 * u, 14 * u), juce::Justification::centredLeft);
            g.setColour(juce::Colours::white.withAlpha(0.06f)); g.fillRoundedRectangle(x + 15 * u, y + 17 * u, 52 * u, 3 * u, 1.5f * u);
            g.setColour(kTypeCol[k]); g.fillRoundedRectangle(x + 15 * u, y + 17 * u, 52 * u * a, 3 * u, 1.5f * u);
        }
    }
private:
    AccrocProcessor& proc;
};

// ------------------------------------------------------------------ pad jouable à la souris : accident tant qu'on appuie
// Touche poussoir à pastille de la couleur du type d'accident (pinceaux et pads) ; Classique : rendu d'origine.
class TypeKey : public v3::bridge::PushKey {
public:
    TypeKey(const String& text, const kit::Palette& pal, Colour c) : v3::bridge::PushKey(text, pal, c), tc(c) {}
    void paintButton(juce::Graphics& g, bool hover, bool down) override {
        if (!v3::bridge::v3(T)) { v3::bridge::PushKey::paintButton(g, hover, down); return; }
        auto r = getLocalBounds().toFloat().reduced(1.f); const bool on = getToggleState() || lit; const float rad = juce::jmin(r.getHeight() * 0.5f, 16.f);
        juce::Graphics::ScopedSaveState st(g); if (!isEnabled()) g.setOpacity(0.5f);
        juce::DropShadow(T->shadow, juce::roundToInt(rad * 0.8f), {0, 2}).drawForRectangle(g, r.reduced(2.f).toNearestInt());
        g.setColour(down ? T->card.darker(0.05f) : hover ? T->card.brighter(0.04f) : T->card); g.fillRoundedRectangle(r, rad);
        if (on) { g.setColour(tc.withAlpha(T->dark ? 0.30f : 0.26f)); g.fillRoundedRectangle(r, rad); }
        g.setColour(on ? tc : T->fieldEdge); g.drawRoundedRectangle(r.reduced(0.5f), rad, on ? 1.6f : 1.f);
        const float d = juce::jmax(7.f, r.getHeight() * 0.2f);
        if (on) { g.setColour(tc.withAlpha(0.3f)); g.fillEllipse(Rectangle<float>(d * 1.8f, d * 1.8f).withCentre({r.getX() + rad * 0.95f, r.getCentreY()})); }
        g.setColour(tc); g.fillEllipse(Rectangle<float>(d, d).withCentre({r.getX() + rad * 0.95f, r.getCentreY()}));
        g.setColour(T->ink); v3::caps(g, getButtonText(), r.withTrimmedLeft(rad * 1.5f).translated(0, down ? 0.5f : 0.f), juce::jlimit(9.f, 13.f, r.getHeight() * 0.30f), juce::Justification::centredLeft);
    }
private:
    Colour tc;
};
class PadKey : public TypeKey {
public:
    PadKey(AccrocProcessor& p, int t, const kit::Palette& pal) : TypeKey(u8(acr::kTypeNames[t]), pal, kTypeCol[t]), proc(p), type(t) {}
    ~PadKey() override { proc.pad(type, 0.f); }
    void buttonStateChanged() override { const bool d = isDown(); if (d != down) { down = d; proc.pad(type, d ? 1.f : 0.f); } }
private:
    AccrocProcessor& proc; int type; bool down = false;
};

// ------------------------------------------------------------------ éditeur
AccrocEditor::AccrocEditor(AccrocProcessor& p)
    : AudioProcessorEditor(&p), proc(p), pal(of::palette(kAcid)), look(pal),
      reroll(u8("Nouvelle graine"), pal, kAcid), clear(u8("Vider la s\xC3\xA9quence"), pal, kAcid), power(pal) {
    setLookAndFeel(&look);
    seqView = std::make_unique<SeqView>(proc, [this] { return proc.brush; }); addAndMakeVisible(*seqView);
    status = std::make_unique<StatusView>(proc); addAndMakeVisible(*status);
    struct D { const char* id; const char* lab; const char* tip; };
    const D defs[] = {
        {"chaos", "Chaos", "Accidents surprises sur les pas libres et variation des divisions. Reproductible : m\xC3\xAAme graine, m\xC3\xAAme r\xC3\xA9sultat."},
        {"prob", "Probabilit\xC3\xA9", "Multiplie la probabilit\xC3\xA9 de chaque pas de la s\xC3\xA9quence."},
        {"seed", "Graine", "Graine du hasard : chaque position du morceau donne toujours le m\xC3\xAAme tirage."},
        {"mix", "Mix", "Dosage sec / trait\xC3\xA9 (le sec est retard\xC3\xA9 de la latence : aucun filtrage en peigne)."},
        {"out", "Sortie", "Gain de sortie (limiteur doux au-del\xC3\xA0 de -0,9 dBFS)."},
        {"stu_div", "Division", "Longueur de la tranche r\xC3\xA9p\xC3\xA9t\xC3\xA9" "e (1/4 \xC3\xA0 1/64)."},
        {"stu_decay", "D\xC3\xA9" "clin", "Chaque r\xC3\xA9p\xC3\xA9tition est un peu plus douce."},
        {"stu_pitch", "Rampe", "Hauteur ajout\xC3\xA9" "e \xC3\xA0 chaque r\xC3\xA9p\xC3\xA9tition, en demi-tons (mont\xC3\xA9" "e ou descente)."},
        {"rev_div", "Tranche", "Longueur des tranches relues \xC3\xA0 l'envers."},
        {"tape_depth", "Profondeur", "100 % : arr\xC3\xAAt complet. Moins : plong\xC3\xA9" "e de hauteur partielle."},
        {"tape_curve", "Courbe", "Bas : freinage tardif. Haut : la bande l\xC3\xA2" "che tout de suite."},
        {"frz_size", "Grain", "Taille de l'instant fig\xC3\xA9 (grains de Hann crois\xC3\xA9s, sans clic)."},
        {"frz_jitter", "Flou", "Variation de position des grains : plus vivant, moins m\xC3\xA9tallique."},
        {"rep_div", "Syllabe", "Longueur de la syllabe r\xC3\xA9p\xC3\xA9t\xC3\xA9" "e."},
        {"rep_gap", "Silence", "Silence entre deux reprises de la syllabe."},
        {"cr_bits", "Bits", "R\xC3\xA9solution (2 \xC3\xA0 16 bits)."},
        {"cr_rate", "Fr\xC3\xA9quence", "Fr\xC3\xA9quence d'\xC3\xA9" "chantillonnage r\xC3\xA9" "duite."},
        {"cr_smooth", "Lissage", "Adoucit les marches : 0 = num\xC3\xA9rique dur, 100 % = lo-fi feutr\xC3\xA9."},
        {"skip_range", "Port\xC3\xA9" "e", "Saut maximal en arri\xC3\xA8re, en doubles-croches."},
        {"gate_div", "Grille", "Taille des cases de micro-coupure."},
        {"gate_dens", "Densit\xC3\xA9", "Part des cases coup\xC3\xA9" "es (rampes de 2 ms, sans clic)."}};
    for (auto& d : defs) {
        auto* pt = pots.add(new Pot(proc.apvts, d.id, u8(d.lab), u8(d.tip), pal)); pt->setStyle(kit::KnobStyle::Cap); addAndMakeVisible(pt);
        auto* k = k3.add(new v3::Knob3(proc.apvts, d.id, u8(d.lab), u8(d.tip), v3::KnobSize::S));
        k->setBipolar(v3::rk::bipolarOf(proc.apvts, d.id)); k->setLabelPx(11.f); addChildComponent(k);
    }
    stepSeg = std::make_unique<v3::bridge::Seg>(proc.apvts, "step", juce::StringArray{"Pas 1/8", "Pas 1/16", "Pas 1/32"}, u8("Dur\xC3\xA9" "e d'un pas de la s\xC3\xA9quence (16 pas)."), pal);
    clockSeg = std::make_unique<v3::bridge::Seg>(proc.apvts, "clock", juce::StringArray{"Lecture", "Toujours"},
                                     u8("Lecture : la s\xC3\xA9quence ne joue que quand l'h\xC3\xB4te lit (cal\xC3\xA9" "e sur sa position). "
                                        "Toujours : horloge interne au tempo quand l'h\xC3\xB4te est \xC3\xA0 l'arr\xC3\xAAt."), pal);
    addAndMakeVisible(*stepSeg); addAndMakeVisible(*clockSeg);
    for (int i = 0; i <= acr::kNumTypes; ++i) {
        const bool erase = i == acr::kNumTypes;
        auto* b = brushes.add(new TypeKey(erase ? u8("Gomme") : u8(acr::kTypeNames[i]), pal, erase ? kit::w::kSilk : kTypeCol[i])); addAndMakeVisible(b);
        b->setTooltip(erase ? u8("Pinceau gomme : un clic sur un pas l'efface.") : u8("Pinceau : ") + u8(kTypeTip[i]));
        b->onClick = [this, i, erase] { setBrush(erase ? -1 : i); };
    }
    for (int i = 0; i < acr::kNumTypes; ++i) {
        auto* k = pads.add(new PadKey(proc, i, pal)); addAndMakeVisible(k);
        k->setTooltip(u8("Pad (ou note MIDI ") + String(kit::w::kNotes[i]) + u8(" de toute octave) : l'accident dure tant qu'on appuie. ") + u8(kTypeTip[i]));
    }
    reroll.setTooltip(u8("Tire une nouvelle graine (autre r\xC3\xA9partition des accidents al\xC3\xA9" "atoires)."));
    reroll.onClick = [this] { if (auto* q = proc.apvts.getParameter("seed")) q->setValueNotifyingHost(q->convertTo0to1((float)juce::Random::getSystemRandom().nextInt({1, 1000}))); };
    clear.setTooltip(u8("Efface les 16 pas."));
    clear.onClick = [this] { proc.setSeq(acr::Seq{}); };
    addAndMakeVisible(reroll); addAndMakeVisible(clear);
    for (int i = 0; i < proc.getNumPrograms(); ++i) presets.addItem(proc.getProgramName(i), i + 1);
    presets.setTooltip(u8("Pr\xC3\xA9r\xC3\xA9glages d'usine (s\xC3\xA9quence + r\xC3\xA9glages ; mix, sortie et horloge conserv\xC3\xA9s)."));
    presets.onChange = [this] { const int i = presets.getSelectedId() - 1; if (i >= 0 && i != proc.getCurrentProgram()) { proc.setCurrentProgram(i); proc.updateHostDisplay(); } shownProgram = i; };
    addAndMakeVisible(presets);
    addAndMakeVisible(power);
    power.setTooltip(u8("Effet en service. Bascule pour le couper (latence de 4 ms compens\xC3\xA9" "e, fondu sans clic)."));
    powerAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, "bypass", power);
    help.install(*this, "Orbe Accroc", {u8("Accidents de lecture volontaires sur la voix, cal\xC3\xA9s sur le tempo : propres, sans clic."),
        u8("- S\xC3\xA9quence : choisis un pinceau, clique sur les pas. Intensit\xC3\xA9, probabilit\xC3\xA9 et dur\xC3\xA9" "e par pas."),
        u8("- Pads : clic maintenu, ou notes MIDI C \xC3\xA0 G (toute octave) si l'h\xC3\xB4te envoie le MIDI ; la v\xC3\xA9locit\xC3\xA9 dose l'effet."),
        u8("- Chaos : accidents surprises ; la graine les rend reproductibles (m\xC3\xAAme position, m\xC3\xAAme tirage)."),
        u8("- Horloge : Lecture = suit la position de l'h\xC3\xB4te ; Toujours = tourne aussi \xC3\xA0 l'arr\xC3\xAAt."),
        u8("- Intensit\xC3\xA9 0 = signal intact. Raccords en fondus de 4 ms, limiteur doux en sortie."),
        u8("- Latence de 4 ms d\xC3\xA9" "clar\xC3\xA9" "e \xC3\xA0 l'h\xC3\xB4te ; le Mix reste en phase.")});
    setBrush(proc.brush);
    addAndMakeVisible(skin.btn);
    skin.btn.onTheme = [this](v3::Mode m) { setThemeMode(m, true); };
    skin.btn.onScale = [this](float s) { setUiScale(s); };
    setResizable(true, true);
    scale_ = v3::Prefs::scale("accroc", 1.f);
    setThemeMode(v3::Prefs::theme("accroc", v3::Mode::Light), false);
    timerCallback();
    startTimerHz(30);
}
AccrocEditor::~AccrocEditor() { stopTimer(); v3::Prefs::setScale("accroc", scale_); setLookAndFeel(nullptr); }

void AccrocEditor::setThemeMode(v3::Mode m, bool persist) {
    skin.set(m);
    const bool n3 = isV3();
    setLookAndFeel(n3 ? static_cast<juce::LookAndFeel*>(&skin.look3) : static_cast<juce::LookAndFeel*>(&look));
    const v3::Theme* tp = skin.tp();
    seqView->setTheme(tp); status->setTheme(tp);
    for (auto* p : pots) p->setVisible(!n3);
    for (auto* k : k3) { k->setVisible(n3); k->setup(&skin.T, &skin.art); }
    for (auto* sg : {stepSeg.get(), clockSeg.get()}) { sg->T = tp; sg->repaint(); }
    for (auto* b : brushes) b->T = tp;
    for (auto* b : pads) b->T = tp;
    reroll.T = clear.T = tp; power.T = tp; help.T = tp;
    if (persist) v3::Prefs::setTheme("accroc", m);
    if (v3::rk::fit(*this, n3, {kW3, kH3}, {kW, kH}, 0.58f, 1.63f, scale_)) resized();
    sendLookAndFeelChange();
    repaint();
}

void AccrocEditor::setUiScale(float s) {
    const bool n3 = isV3();
    scale_ = juce::jlimit(n3 ? 0.75f : 0.58f, n3 ? 1.5f : 1.63f, s);
    setSize(juce::roundToInt((n3 ? kW3 : kW) * scale_), juce::roundToInt((n3 ? kH3 : kH) * scale_));
    v3::Prefs::setScale("accroc", scale_);
}

void AccrocEditor::setBrush(int b) {
    proc.brush = b;
    for (int i = 0; i < brushes.size(); ++i) { brushes[i]->lit = (i == acr::kNumTypes ? b < 0 : i == b); brushes[i]->repaint(); }
}

void AccrocEditor::resized() {
    if (isV3()) resized3(); else resizedClassic();
    skin.btn.scale = scale_;
}

void AccrocEditor::resizedClassic() {
    u_ = (float)getWidth() / kW; scale_ = u_;
    const auto content = getLocalBounds().toFloat().reduced(18.f * u_, 0);
    power.setBounds(kit::w::inSlot(content, u_).toNearestInt());
    help.layout(kit::w::helpSlot(content, u_, true).toNearestInt(), getLocalBounds());
    presets.setBounds(Si(730, 13, 250, 24));
    seqView->setBounds(Si(32, 80, 688, 196));
    for (int i = 0; i < pads.size(); ++i) pads[i]->setBounds(Si(736 + 84 * (float)(i % 4), 80 + 48 * (float)(i / 4), 84, 46));
    status->setBounds(Si(736, 182, 336, 94));
    const float bw = 688.f / 9.f;
    for (int i = 0; i < brushes.size(); ++i) brushes[i]->setBounds(Si(32 + bw * (float)i, 284, bw, 34));
    reroll.setBounds(Si(736, 284, 166, 34)); clear.setBounds(Si(906, 284, 166, 34));
    const float ph = 78.f * 124.f / 117.f;
    for (int i = 0; i < 3; ++i) pots[i]->setBounds(Si(32 + 124 * (float)i, 350, 124, ph));
    stepSeg->setBounds(Si(420, 354, 300, 32)); clockSeg->setBounds(Si(420, 394, 300, 32));
    pots[3]->setBounds(Si(780, 350, 124, ph)); pots[4]->setBounds(Si(920, 350, 124, ph));
    // effets, rangée 1 : Stutter (3) · Retour (1) · Arrêt (2) · Gel (2)
    const float x1[8] = {32, 156, 280, 420, 560, 684, 824, 948};
    for (int i = 0; i < 8; ++i) pots[5 + i]->setBounds(Si(x1[i], 462, 124, ph));
    // rangée 2 : Bégaiement (2) · Crush (3) · Saut (1) · Trous (2)
    const float x2[8] = {32, 156, 296, 420, 544, 684, 824, 948};
    for (int i = 0; i < 8; ++i) pots[13 + i]->setBounds(Si(x2[i], 574, 124, ph));
}

void AccrocEditor::timerCallback() {
    seqView->repaint(); status->repaint();
    if (brushes[acr::kNumTypes]->lit != (proc.brush < 0) || (proc.brush >= 0 && !brushes[proc.brush]->lit)) setBrush(proc.brush);
    for (auto* k : k3) k->tickMod(1.f / 30.f);
    for (int i = 0; i < pads.size(); ++i) { const bool on = proc.activity(i) > 0.02f; if (pads[i]->lit != on) { pads[i]->lit = on; pads[i]->repaint(); } }
    if (proc.getCurrentProgram() != shownProgram) { shownProgram = proc.getCurrentProgram(); presets.setSelectedId(shownProgram + 1, juce::dontSendNotification); }
}

void AccrocEditor::paint(juce::Graphics& g) {
    if (isV3()) { paint3(g); return; }
    const float u = u_;
    kit::w::drawFace(g, pal, getLocalBounds(), u, {kit::w::Family::Rack, "Orbe Accroc", kit::w::serial::accroc});
    sec(g, pal, u, u8("S\xC3\xA9quence \xC2\xB7 16 pas"), 32, 60, 688);
    sec(g, pal, u, u8("Pads \xC2\xB7 notes C \xC3\xA0 G"), 736, 60, 336);
    sec(g, pal, u, "Hasard", 32, 328, 372);
    sec(g, pal, u, u8("Pas \xC2\xB7 horloge"), 420, 328, 300);
    sec(g, pal, u, "Sortie", 736, 328, 336);
    sec(g, pal, u, "Stutter", 32, 440, 372, kTypeCol[acr::Stutter]);
    sec(g, pal, u, "Retour", 420, 440, 124, kTypeCol[acr::Reverse]);
    sec(g, pal, u, u8("Arr\xC3\xAAt bande"), 560, 440, 248, kTypeCol[acr::TapeStop]);
    sec(g, pal, u, "Gel granulaire", 824, 440, 248, kTypeCol[acr::Freeze]);
    sec(g, pal, u, u8("B\xC3\xA9gaiement"), 32, 552, 248, kTypeCol[acr::Stammer]);
    sec(g, pal, u, "Crush", 296, 552, 372, kTypeCol[acr::Crush]);
    sec(g, pal, u, "Saut", 684, 552, 124, kTypeCol[acr::Skip]);
    sec(g, pal, u, "Micro-coupures", 824, 552, 248, kTypeCol[acr::Gate]);
}

// ============================================================ v3 « hybride v2 » : coordonnées de conception 1240 x 870
namespace {
constexpr float kRowH = 148.f, kRowA = 382.f, kRow1 = 542.f, kRow2 = 702.f;
// début des sections d'effets (pas de 120 par potard, 28 d'écart) : rangée 1 et rangée 2
const float kX1[4] = {40.f, 428.f, 576.f, 844.f}, kX2[4] = {40.f, 308.f, 696.f, 844.f};
}

void AccrocEditor::resized3() {
    u_ = (float)getWidth() / kW3; scale_ = u_;
    help.layout(Si(1114, 34, 30, 30), getLocalBounds());
    skin.btn.setBounds(Si(1080, 35, 28, 28));
    power.setBounds(Si(1150, 36, 70, 26));
    presets.setBounds(Si(400, 34, 290, 32));
    seqView->setBounds(Si(20, 84, 780, 230));
    for (int i = 0; i < pads.size(); ++i) pads[i]->setBounds(Si(820.f + 101.f * (float)(i % 4), 84.f + 58.f * (float)(i / 4), 97, 54));
    status->setBounds(Si(820, 202, 400, 112));
    const float bw = 780.f / 9.f;
    for (int i = 0; i < brushes.size(); ++i) brushes[i]->setBounds(Si(20.f + bw * (float)i + 2.f, 326, bw - 4.f, 44));
    reroll.setBounds(Si(820, 326, 194, 44)); clear.setBounds(Si(1026, 326, 194, 44));
    const float kh = v3::Knob3::boxHeight(v3::KnobSize::S, false), kw = 112.f;
    auto place = [&](int i, float cx, float y) { k3[i]->setScale(u_); k3[i]->setBounds(Si(cx - kw * 0.5f, y, kw, kh)); };
    for (int i = 0; i < 3; ++i) place(i, 40.f + 120.f * ((float)i + 0.5f), kRowA + 34.f);
    place(3, 776.f + 60.f, kRowA + 34.f); place(4, 776.f + 180.f, kRowA + 34.f);
    stepSeg->setBounds(Si(440, kRowA + 44, 300, 34)); clockSeg->setBounds(Si(440, kRowA + 90, 300, 34));
    stepSeg->px = clockSeg->px = 12.f * u_;
    const int n1[4] = {3, 1, 2, 2}, n2[4] = {2, 3, 1, 2};
    int idx = 5;
    for (int s2 = 0; s2 < 4; ++s2) for (int i = 0; i < n1[s2]; ++i) place(idx++, kX1[s2] + 120.f * ((float)i + 0.5f), kRow1 + 34.f);
    for (int s2 = 0; s2 < 4; ++s2) for (int i = 0; i < n2[s2]; ++i) place(idx++, kX2[s2] + 120.f * ((float)i + 0.5f), kRow2 + 34.f);
}

void AccrocEditor::paint3(juce::Graphics& g) {
    skin.art.draw(g, String("bd|") + v3::modeKey(skin.mode), getLocalBounds().toFloat(), [this](juce::Graphics& ig, Rectangle<float> b) {
        const float k = b.getWidth() / kW3; const auto& t = skin.T;
        auto R = [k](float x, float y, float w, float h) { return Rectangle<float>(x * k, y * k, w * k, h * k); };
        v3::background(ig, t, b);
        v3::rk::brand(ig, t, k, "Orbe", "Accroc");
        v3::rk::label(ig, t.inkMute, k, "Preset", 402, 14, 120);
        v3::hero(ig, t, R(20, 84, 780, 230), 22.f * k); v3::hero(ig, t, R(820, 202, 400, 112), 18.f * k);
        for (float y : {kRowA, kRow1, kRow2}) v3::card(ig, t, R(20, y, 1200, kRowH), 18.f * k);
        const auto c = t.inkMute;
        auto sec = [&](const String& title, float x, float y, Colour dot = {}) {
            float tx = x;
            if (!dot.isTransparent()) { ig.setColour(t.dark ? dot : dot.darker(0.25f)); ig.fillEllipse(R(x, y + 5, 9, 9)); tx += 16; }
            v3::rk::label(ig, c, k, title, tx, y, 260);
        };
        sec(u8("Hasard"), 40, kRowA + 12); sec(u8("Pas \xC2\xB7 horloge"), 440, kRowA + 12); sec(u8("Sortie"), 776, kRowA + 12);
        sec("Stutter", kX1[0], kRow1 + 12, kTypeCol[acr::Stutter]); sec("Retour", kX1[1], kRow1 + 12, kTypeCol[acr::Reverse]);
        sec(u8("Arr\xC3\xAAt bande"), kX1[2], kRow1 + 12, kTypeCol[acr::TapeStop]); sec("Gel granulaire", kX1[3], kRow1 + 12, kTypeCol[acr::Freeze]);
        sec(u8("B\xC3\xA9gaiement"), kX2[0], kRow2 + 12, kTypeCol[acr::Stammer]); sec("Crush", kX2[1], kRow2 + 12, kTypeCol[acr::Crush]);
        sec("Saut", kX2[2], kRow2 + 12, kTypeCol[acr::Skip]); sec("Micro-coupures", kX2[3], kRow2 + 12, kTypeCol[acr::Gate]);
        for (float x : {420.f, 756.f}) v3::rk::sep(ig, t, k, x, kRowA + 14, 120);
        for (float x : {414.f, 562.f, 830.f}) v3::rk::sep(ig, t, k, x, kRow1 + 14, 120);
        for (float x : {294.f, 682.f, 830.f}) v3::rk::sep(ig, t, k, x, kRow2 + 14, 120);
    });
}
