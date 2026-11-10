#include "GaletEditor.h"

using namespace of;
using kit::u8;

namespace {
const Colour kJade(0xFFA8E6CF);   // galet de rivière : vert d'eau pâle
const Colour kInk(0xFFDDF7EC);
double gridQuarters(int g) { static const double q[8] = {4, 2, 1, 0.5, 0.25, 0.125, 1.0 / 3.0, 1.0 / 6.0}; return q[juce::jlimit(0, 7, g)]; }
int ch(const GaletProcessor& p, const char* id) { return juce::roundToInt(p.raw(id)); }
}  // namespace

// ------------------------------------------------------------------ éditeur de forme
// Clic : ajoute un point. Glisser : déplace. Double-clic ou clic droit : supprime. Losange au milieu d'un segment : courbure.
class ShapeView : public juce::Component, public juce::SettableTooltipClient {
public:
    explicit ShapeView(GaletProcessor& p) : proc(p) {
        setOpaque(true); lut.assign(gal::kLut + 1, 0.f);
        setTooltip(u8("Clic : ajouter un point. Glisser : d\xC3\xA9placer. Double-clic ou clic droit : supprimer. "
                      "Losange : courbure du segment (double-clic : droit). Haut = effet plein, bas = neutre."));
    }
    void paint(juce::Graphics& g) override {
        const float u = (float)getWidth() / 600.f; const auto pal = of::palette(kJade);
        auto well = getLocalBounds().toFloat(); kit::screenWell(g, pal, well, kit::Display::OLED);
        const auto r = area();
        refreshLut();
        const bool sync = ch(proc, "sync") == 0;
        const double cq = sync ? gal::cycleQuarters(ch(proc, "div"), ch(proc, "feel")) : 4.0;
        const int nd = divisions();
        // grille : divisions de la grille, temps, mesures
        for (int i = 0; i <= nd; ++i) {
            const float x = r.getX() + r.getWidth() * (float)i / (float)nd;
            const double q = cq * (double)i / (double)nd;
            const bool beat = sync && std::fabs(q - std::round(q)) < 1e-6, bar = beat && ((int)std::llround(q) % 4 == 0);
            g.setColour(juce::Colours::white.withAlpha(bar ? 0.16f : beat ? 0.10f : 0.045f));
            g.drawVerticalLine(juce::roundToInt(x), r.getY(), r.getBottom());
        }
        for (int i = 0; i <= 4; ++i) { g.setColour(juce::Colours::white.withAlpha(i % 2 ? 0.04f : 0.08f)); g.drawHorizontalLine(juce::roundToInt(r.getY() + r.getHeight() * (float)i / 4.f), r.getX(), r.getRight()); }
        // courbe
        juce::Path line, fill;
        const int steps = juce::jmax(64, (int)r.getWidth());
        for (int i = 0; i <= steps; ++i) {
            const float t = (float)i / (float)steps, x = r.getX() + r.getWidth() * t, y = yOf(at(t), r);
            if (i == 0) { line.startNewSubPath(x, y); fill.startNewSubPath(x, r.getBottom()); fill.lineTo(x, y); } else { line.lineTo(x, y); fill.lineTo(x, y); }
        }
        fill.lineTo(r.getRight(), r.getBottom()); fill.closeSubPath();
        g.setGradientFill(juce::ColourGradient(kJade.withAlpha(0.34f), 0, r.getY(), kJade.withAlpha(0.03f), 0, r.getBottom(), false)); g.fillPath(fill);
        g.setColour(kJade.withAlpha(0.25f)); g.strokePath(line, juce::PathStrokeType(5.f * u, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(kInk); g.strokePath(line, juce::PathStrokeType(1.8f * u, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        // poignées de courbure
        const auto& s = proc.shape(); const int ip = ch(proc, "interp");
        if (ip != gal::Spline)
            for (int k = 0; k < s.n; ++k) {
                const auto hp = handlePos(k, r); if (hp.x < 0) continue;
                juce::Path d; d.addPolygon(hp, 4, 3.4f * u, juce::MathConstants<float>::pi * 0.25f);
                g.setColour((dragHandle == k || hoverHandle == k) ? kJade : kJade.withAlpha(0.5f)); g.fillPath(d);
            }
        // points
        for (int k = 0; k < s.n; ++k) {
            const auto c = ptPos(k, r); const bool hot = dragPt == k || hoverPt == k;
            const float rad = (hot ? 5.2f : 4.2f) * u;
            g.setColour(juce::Colour(0xFF020203)); g.fillEllipse(c.x - rad, c.y - rad, 2 * rad, 2 * rad);
            g.setColour(hot ? kInk : kJade); g.drawEllipse(c.x - rad, c.y - rad, 2 * rad, 2 * rad, 1.5f * u);
            if (hot) g.fillEllipse(c.x - rad * 0.45f, c.y - rad * 0.45f, rad * 0.9f, rad * 0.9f);
        }
        // tête de lecture et valeur lissée
        const float ph = proc.phaseNow(), m = proc.modNow();
        const float px = r.getX() + r.getWidth() * ph;
        g.setColour(kIce.withAlpha(0.55f)); g.drawVerticalLine(juce::roundToInt(px), r.getY(), r.getBottom());
        g.setColour(kIce); g.fillEllipse(px - 3.5f * u, yOf(m, r) - 3.5f * u, 7.f * u, 7.f * u);
        // textes
        g.setFont(kit::mono(8.8f * u));
        const juce::StringArray divs{"4 mes.", "2 mes.", "1/1", "1/2", "1/4", "1/8", "1/16", "1/32"}, feels{"", " triolet", u8(" point\xC3\xA9")};
        const String head = sync ? u8("FORME \xC2\xB7 CYCLE ") + divs[ch(proc, "div")] + feels[ch(proc, "feel")] : u8("FORME \xC2\xB7 CYCLE ") + String(proc.raw("hz"), 2) + " Hz";
        g.setColour(pal.mute.brighter(0.2f)); g.drawText(kit::upper(head), Rectangle<float>(r.getX(), 5 * u, 340 * u, 13 * u), juce::Justification::centredLeft);
        g.setColour(kJade); g.drawText("MOD " + String(juce::roundToInt(m * 100.f)) + " %", Rectangle<float>(r.getRight() - 120 * u, 5 * u, 120 * u, 13 * u), juce::Justification::centredRight);
        g.setColour(pal.mute); g.drawText(String(s.n) + " / 32 PTS", Rectangle<float>(r.getRight() - 230 * u, 5 * u, 104 * u, 13 * u), juce::Justification::centredRight);
        // repères du bas : temps (mesure.temps) ou fraction du cycle
        const float by = r.getBottom() + 3 * u;
        if (sync && cq >= 1.0) {
            const int nq = (int)std::floor(cq + 1e-6), stepQ = nq > 8 ? 2 : 1;
            for (int q = 0; q < nq; q += stepQ) {
                const float x = r.getX() + r.getWidth() * (float)(q / cq);
                g.setColour(q % 4 == 0 ? kInk.withAlpha(0.7f) : pal.mute);
                g.drawText(String(q / 4 + 1) + "." + String(q % 4 + 1), Rectangle<float>(x + 2 * u, by, 40 * u, 12 * u), juce::Justification::centredLeft);
            }
        } else {
            g.setColour(pal.mute);
            g.drawText("0", Rectangle<float>(r.getX() + 2 * u, by, 30 * u, 12 * u), juce::Justification::centredLeft);
            g.drawText(u8("\xC2\xBD"), Rectangle<float>(r.getCentreX() - 15 * u, by, 30 * u, 12 * u), juce::Justification::centred);
            g.drawText("1 CYCLE", Rectangle<float>(r.getRight() - 80 * u, by, 80 * u, 12 * u), juce::Justification::centredRight);
        }
    }
    void mouseMove(const juce::MouseEvent& e) override {
        const auto r = area(); const int hp = hitPoint(e.position, r), hh = hp < 0 ? hitHandle(e.position, r) : -1;
        if (hp != hoverPt || hh != hoverHandle) { hoverPt = hp; hoverHandle = hh; repaint(); }
        setMouseCursor(hp >= 0 || hh >= 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::CrosshairCursor);
    }
    void mouseExit(const juce::MouseEvent&) override { hoverPt = hoverHandle = -1; repaint(); }
    void mouseDown(const juce::MouseEvent& e) override {
        const auto r = area(); auto s = proc.shape();
        const int hp = hitPoint(e.position, r);
        if (e.mods.isPopupMenu()) { if (hp > 0) { remove(s, hp); proc.setShape(s); } return; }
        if (hp >= 0) { dragPt = hp; return; }
        const int hh = hitHandle(e.position, r);
        if (hh >= 0) { dragHandle = hh; startC = s.p[hh].c; startY = e.position.y; return; }
        if (s.n >= gal::kMaxPts) return;
        const float x = snapX(juce::jlimit(0.f, 0.999f, (e.position.x - r.getX()) / r.getWidth())), y = valOf(e.position.y, r);
        int k = 1; while (k < s.n && s.p[k].x <= x) ++k;
        for (int i = s.n; i > k; --i) s.p[i] = s.p[i - 1];
        s.p[k] = {juce::jmax(x, 0.001f), y, 0.f}; ++s.n; dragPt = k;
        proc.setShape(s);
    }
    void mouseDrag(const juce::MouseEvent& e) override {
        const auto r = area(); auto s = proc.shape();
        if (dragPt >= 0 && dragPt < s.n) {
            const float lo = dragPt == 0 ? 0.f : s.p[dragPt - 1].x, hi = dragPt + 1 < s.n ? s.p[dragPt + 1].x : 0.999f;
            float x = snapX(juce::jlimit(0.f, 0.999f, (e.position.x - r.getX()) / r.getWidth()));
            s.p[dragPt].x = dragPt == 0 ? 0.f : juce::jlimit(lo, hi, x);
            s.p[dragPt].y = valOf(e.position.y, r);
            proc.setShape(s);
        } else if (dragHandle >= 0 && dragHandle < s.n) {
            const float y0 = s.p[dragHandle].y, y1 = dragHandle + 1 < s.n ? s.p[dragHandle + 1].y : s.p[0].y;
            const float dir = y1 >= y0 ? -1.f : 1.f;
            s.p[dragHandle].c = juce::jlimit(-1.f, 1.f, startC + dir * (startY - e.position.y) / (r.getHeight() * 0.5f));
            proc.setShape(s);
        }
    }
    void mouseUp(const juce::MouseEvent&) override { dragPt = dragHandle = -1; repaint(); }
    void mouseDoubleClick(const juce::MouseEvent& e) override {
        const auto r = area(); auto s = proc.shape();
        const int hp = hitPoint(e.position, r);
        if (hp > 0) { remove(s, hp); proc.setShape(s); return; }
        const int hh = hitHandle(e.position, r);
        if (hh >= 0) { s.p[hh].c = 0.f; proc.setShape(s); }
    }
private:
    juce::Rectangle<float> area() const { const float u = (float)getWidth() / 600.f; return getLocalBounds().toFloat().withTrimmedLeft(12 * u).withTrimmedRight(12 * u).withTrimmedTop(22 * u).withTrimmedBottom(19 * u); }
    static float yOf(float v, juce::Rectangle<float> r) { return r.getBottom() - r.getHeight() * v; }
    static float valOf(float y, juce::Rectangle<float> r) { float v = juce::jlimit(0.f, 1.f, (r.getBottom() - y) / r.getHeight()); if (v < 0.015f) v = 0.f; if (v > 0.985f) v = 1.f; return v; }
    int divisions() const {
        const bool sync = ch(proc, "sync") == 0;
        const double cq = sync ? gal::cycleQuarters(ch(proc, "div"), ch(proc, "feel")) : 4.0;
        return juce::jlimit(1, 64, (int)std::llround(cq / gridQuarters(ch(proc, "grid"))));
    }
    float snapX(float x) const { if (!proc.snap) return x; const int nd = divisions(); return juce::jlimit(0.f, 0.999f, (float)std::round(x * (float)nd) / (float)nd); }
    float at(float t) const { const float x = t * (float)gal::kLut; const int i = juce::jlimit(0, gal::kLut - 1, (int)x); return lut[(size_t)i] + (x - (float)i) * (lut[(size_t)i + 1] - lut[(size_t)i]); }
    void refreshLut() { const int ip = ch(proc, "interp"); if (proc.shapeVersion() != lutVer || ip != lutInterp) { gal::render(proc.shape(), ip, lut.data()); lutVer = proc.shapeVersion(); lutInterp = ip; } }
    juce::Point<float> ptPos(int k, juce::Rectangle<float> r) const { const auto& q = proc.shape().p[k]; return {r.getX() + r.getWidth() * q.x, yOf(q.y, r)}; }
    juce::Point<float> handlePos(int k, juce::Rectangle<float> r) const {
        const auto& s = proc.shape(); const float x0 = s.p[k].x, x1 = k + 1 < s.n ? s.p[k + 1].x : 1.f;
        const float y0 = s.p[k].y, y1 = k + 1 < s.n ? s.p[k + 1].y : s.p[0].y;
        if ((x1 - x0) * r.getWidth() < 16.f || std::fabs(y1 - y0) < 0.02f) return {-1, -1};
        const float xm = 0.5f * (x0 + x1), ym = y0 + (y1 - y0) * gal::bend(0.5f, s.p[k].c, ch(proc, "interp") == gal::Sym);
        return {r.getX() + r.getWidth() * xm, yOf(ym, r)};
    }
    int hitPoint(juce::Point<float> p, juce::Rectangle<float> r) const {
        const float u = (float)getWidth() / 600.f; int best = -1; float bd = 8.f * u;
        for (int k = 0; k < proc.shape().n; ++k) { const float d = ptPos(k, r).getDistanceFrom(p); if (d < bd) { bd = d; best = k; } }
        return best;
    }
    int hitHandle(juce::Point<float> p, juce::Rectangle<float> r) const {
        if (ch(proc, "interp") == gal::Spline) return -1;
        const float u = (float)getWidth() / 600.f;
        for (int k = 0; k < proc.shape().n; ++k) { const auto h = handlePos(k, r); if (h.x >= 0 && h.getDistanceFrom(p) < 7.f * u) return k; }
        return -1;
    }
    static void remove(gal::Shape& s, int k) { if (k <= 0 || k >= s.n) return; for (int i = k; i < s.n - 1; ++i) s.p[i] = s.p[i + 1]; --s.n; }

    GaletProcessor& proc;
    std::vector<float> lut; int lutVer = -1, lutInterp = -1;
    int dragPt = -1, dragHandle = -1, hoverPt = -1, hoverHandle = -1; float startC = 0, startY = 0;
};

// ------------------------------------------------------------------ courbe de transfert et activité des modules
class TransferView : public juce::Component {
public:
    explicit TransferView(GaletProcessor& p) : proc(p) { setOpaque(true); }
    void paint(juce::Graphics& g) override {
        const float u = (float)getWidth() / 280.f; const auto pal = of::palette(kJade);
        auto well = getLocalBounds().toFloat(); kit::screenWell(g, pal, well, kit::Display::OLED);
        const int type = ch(proc, "drv_type"); const bool on = ch(proc, "drv_on") == 1;
        const float maxDb = proc.raw("drv_drive") * proc.raw("drv_depth") * (on ? 1.f : 0.f), live = proc.driveNow(), color = proc.raw("drv_color");
        const juce::StringArray types{"TANH", "ARCTAN", "PLI DOUX"};
        g.setFont(kit::mono(8.8f * u)); g.setColour(pal.mute.brighter(0.2f));
        g.drawText(u8("TRANSFERT \xC2\xB7 ") + types[type], Rectangle<float>(10 * u, 5 * u, 150 * u, 13 * u), juce::Justification::centredLeft);
        if (!on) { g.setColour(kit::w::kWarn.withAlpha(0.8f)); g.drawText(u8("MODULE COUP\xC3\x89"), Rectangle<float>(10 * u, 5 * u, 260 * u, 13 * u), juce::Justification::centredRight); }
        auto r = Rectangle<float>(12 * u, 24 * u, 128 * u, 128 * u);
        g.setColour(juce::Colours::white.withAlpha(0.08f));
        g.drawVerticalLine(juce::roundToInt(r.getCentreX()), r.getY(), r.getBottom()); g.drawHorizontalLine(juce::roundToInt(r.getCentreY()), r.getX(), r.getRight());
        g.setColour(juce::Colours::white.withAlpha(0.05f)); g.drawRect(r);
        juce::Path diag; diag.startNewSubPath(r.getX(), r.getBottom()); diag.lineTo(r.getRight(), r.getY());
        const float dash[] = {3.f * u, 3.f * u}; juce::Path dd; juce::PathStrokeType(1.f).createDashedStroke(dd, diag, dash, 2); g.setColour(juce::Colours::white.withAlpha(0.18f)); g.fillPath(dd);
        auto curve = [&](float db) {
            juce::Path p;
            for (int i = 0; i <= 96; ++i) {
                const float x = -1.f + 2.f * (float)i / 96.f, y = juce::jlimit(-1.2f, 1.2f, gal::transfer(type, db, color, x));
                const float px = r.getX() + r.getWidth() * (x + 1.f) * 0.5f, py = r.getCentreY() - r.getHeight() * 0.5f * y / 1.2f;
                if (i == 0) p.startNewSubPath(px, py); else p.lineTo(px, py);
            }
            return p;
        };
        g.setColour(kJade.withAlpha(0.35f)); g.strokePath(curve(maxDb), juce::PathStrokeType(1.2f * u));
        g.setColour(kInk); g.strokePath(curve(live), juce::PathStrokeType(1.8f * u));
        // colonne d'informations
        const float ix = 152 * u, iw = 118 * u;
        auto row = [&](float y, const String& k, const String& v, Colour c) {
            g.setColour(pal.mute); g.setFont(kit::mono(8.4f * u)); g.drawText(k, Rectangle<float>(ix, y, iw, 11 * u), juce::Justification::centredLeft);
            g.setColour(c); g.setFont(kit::mono(10.f * u)); g.drawText(v, Rectangle<float>(ix, y + 11 * u, iw, 13 * u), juce::Justification::centredLeft);
        };
        row(24 * u, "DRIVE MAX", "+" + String(maxDb, 1) + " dB", kJade);
        row(52 * u, "EN DIRECT", "+" + String(live, 1) + " dB", kInk);
        row(80 * u, "COULEUR", String(juce::roundToInt(color * 100.f)) + " %", kJade);
        row(108 * u, u8("SUR\xC3\x89" "CHANTILLONNAGE"), u8("\xC3\x97" "4 \xC2\xB7 ADAA"), pal.mute.brighter(0.3f));
        row(136 * u, "LATENCE", String(gal::Engine::kLatency) + u8(" \xC3\xA9" "ch."), pal.mute.brighter(0.3f));
        // activité des quatre modules
        const char* names[4] = {"VOL", "FILTRE", "DIST", "PAN"};
        const char* ids[4] = {"vol_on", "flt_on", "drv_on", "pan_on"};
        const float y0 = 172 * u, bw = 62 * u;
        for (int k = 0; k < 4; ++k) {
            const float x = 12 * u + (float)k * (bw + 4 * u);
            const bool en = ch(proc, ids[k]) == 1; const float v = juce::jlimit(0.f, 1.f, std::fabs(proc.envNow(k)));
            kit::led(g, x + 4 * u, y0 + 6 * u, 2.6f * u, en ? 0.35f + 0.65f * v : 0.f, kJade, kit::LedBezel::None);
            g.setColour(en ? kInk.withAlpha(0.85f) : pal.mute); g.setFont(kit::mono(8.6f * u));
            g.drawText(names[k], Rectangle<float>(x + 10 * u, y0, bw - 10 * u, 12 * u), juce::Justification::centredLeft);
            auto bar = Rectangle<float>(x, y0 + 17 * u, bw, 5 * u);
            g.setColour(juce::Colours::white.withAlpha(0.07f)); g.fillRoundedRectangle(bar, 2.f * u);
            g.setColour(en ? kJade : pal.mute.withAlpha(0.4f)); g.fillRoundedRectangle(bar.withWidth(bw * v), 2.f * u);
        }
        g.setColour(pal.mute); g.setFont(kit::mono(8.4f * u));
        g.drawText(u8("ACTIVIT\xC3\x89 DES MODULES (FORME \xC3\x97 PROFONDEUR)"), Rectangle<float>(12 * u, 200 * u, 260 * u, 12 * u), juce::Justification::centredLeft);
    }
private:
    GaletProcessor& proc;
};

// ------------------------------------------------------------------ éditeur
GaletEditor::GaletEditor(GaletProcessor& p)
    : AudioProcessorEditor(&p), proc(p), pal(of::palette(kJade)), look(pal), snapKey("Aimant", pal, kJade), power(pal) {
    setLookAndFeel(&look);
    shapeView = std::make_unique<ShapeView>(proc); addAndMakeVisible(*shapeView);
    transfer = std::make_unique<TransferView>(proc); addAndMakeVisible(*transfer);
    struct D { const char* id; const char* lab; const char* tip; };
    const D defs[] = {
        {"div", "Division", "Dur\xC3\xA9" "e d'un cycle de la forme, en mesures ou valeurs de note (mode Tempo)."},
        {"hz", "Fr\xC3\xA9quence", "Cycles par seconde en mode Hz (tempo ignor\xC3\xA9)."},
        {"phase", "Phase", "D\xC3\xA9" "cale la forme dans le cycle (0 \xC3\xA0 360\xC2\xB0)."},
        {"smooth", "Lissage", "Adoucit la forme : fronts arrondis, aucun clic. Court = net, long = souple."},
        {"grid", "Grille", "Pas de la grille d'\xC3\xA9" "dition (avec Aimant, les points s'y accrochent)."},
        {"vol_depth", "Profondeur", "Baisse de volume quand la forme est en haut (100 % : silence)."},
        {"flt_mode", "Type", "Passe-bas, passe-bande ou passe-haut (fondu sans clic entre les types)."},
        {"flt_freq", "Fr\xC3\xA9quence", "Coupure atteinte quand la forme est en haut ; en bas, le filtre est ouvert."},
        {"flt_res", "R\xC3\xA9sonance", "Accent autour de la coupure (\xC3\xA9tats-variables stable)."},
        {"flt_depth", "Profondeur", "Part du trajet vers la coupure."},
        {"mix", "Mix", "Dosage sec / trait\xC3\xA9 (le sec est retard\xC3\xA9 de la latence : aucun filtrage en peigne)."},
        {"out", "Sortie", "Gain de sortie."},
        {"drv_type", "Type", "Tanh : rondeur classique. Arctan : plus douce. Pli doux : repli sinuso\xC3\xAF" "dal, plus riche."},
        {"drv_drive", "Drive", "Saturation atteinte quand la forme est en haut (niveau compens\xC3\xA9)."},
        {"drv_color", "Couleur", "Asym\xC3\xA9trie : ajoute des harmoniques paires, plus chaudes."},
        {"drv_depth", "Profondeur", "Part du drive appliqu\xC3\xA9" "e par la forme."},
        {"pan_pan", "Pan", "Position atteinte quand la forme est en haut (G / D)."},
        {"pan_width", "Largeur", "Largeur st\xC3\xA9r\xC3\xA9o atteinte quand la forme est en haut (-100 % : mono, +100 % : \xC3\xA9largi)."}};
    for (auto& d : defs) { auto* pt = pots.add(new Pot(proc.apvts, d.id, u8(d.lab), u8(d.tip), pal)); pt->setStyle(kit::KnobStyle::Cap); addAndMakeVisible(pt); }
    syncSeg = std::make_unique<Seg>(proc.apvts, "sync", juce::StringArray{"Tempo", "Hz"}, u8("Tempo : cycle cal\xC3\xA9 sur le tempo de l'h\xC3\xB4te. Hz : vitesse libre."), pal);
    retrigSeg = std::make_unique<Seg>(proc.apvts, "retrig", juce::StringArray{u8("H\xC3\xB4te"), "Note", "Libre"},
                                      u8("H\xC3\xB4te : suit la position de lecture. Note : chaque note MIDI relance la forme. Libre : tourne sans fin."), pal);
    feelSeg = std::make_unique<Seg>(proc.apvts, "feel", juce::StringArray{"Droit", "Triolet", u8("Point\xC3\xA9")}, u8("Valeur de la division : droite, en triolet (\xC3\x97 2/3) ou point\xC3\xA9" "e (\xC3\x97 1,5)."), pal);
    interpSeg = std::make_unique<Seg>(proc.apvts, "interp", juce::StringArray{"Libre", "En S", "Spline"},
                                      u8("Libre : courbure exponentielle par segment. En S : courbure sym\xC3\xA9trique. Spline : passe en douceur par tous les points."), pal);
    for (auto* s : {syncSeg.get(), retrigSeg.get(), feelSeg.get(), interpSeg.get()}) addAndMakeVisible(s);
    const char* mods[4] = {"vol_on", "flt_on", "drv_on", "pan_on"};
    for (auto* id : mods) {
        auto* k = modKeys.add(new Key("On", pal, kJade)); addAndMakeVisible(k);
        k->setTooltip(u8("Active le module : la forme le pilote (fondu sans clic)."));
        modAtts.add(new juce::AudioProcessorValueTreeState::ButtonAttachment(proc.apvts, id, *k));
    }
    snapKey.setToggleState(proc.snap, juce::dontSendNotification);
    snapKey.onClick = [this] { proc.snap = snapKey.getToggleState(); };
    snapKey.setTooltip(u8("Les points s'accrochent \xC3\xA0 la grille."));
    addAndMakeVisible(snapKey);
    for (int i = 0; i < gal::kNumStarts; ++i) {
        auto* b = starts.add(new kit::w::PushKey(u8(gal::kStartNames[i]), pal, kJade)); addAndMakeVisible(b);
        b->setTooltip(u8("Remplace la forme par une forme de d\xC3\xA9part (modifiable ensuite)."));
        b->onClick = [this, i] {
            int ip = gal::Free; const auto s = gal::startShape(i, &ip);
            proc.setShape(s);
            if (auto* q = proc.apvts.getParameter("interp")) q->setValueNotifyingHost(q->convertTo0to1((float)ip));
        };
    }
    for (int i = 0; i < proc.getNumPrograms(); ++i) presets.addItem(proc.getProgramName(i), i + 1);
    presets.setTooltip(u8("Pr\xC3\xA9r\xC3\xA9glages d'usine (forme + r\xC3\xA9glages)."));
    presets.onChange = [this] { const int i = presets.getSelectedId() - 1; if (i >= 0 && i != proc.getCurrentProgram()) { proc.setCurrentProgram(i); proc.updateHostDisplay(); } shownProgram = i; };
    addAndMakeVisible(presets);
    addAndMakeVisible(power);
    power.setTooltip(u8("Effet en service. Bascule pour le couper (latence de 71 \xC3\xA9" "chantillons compens\xC3\xA9" "e)."));
    powerAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, "bypass", power);
    help.install(*this, "Orbe Galet", {u8("Dessine une forme : elle module le volume, un filtre, une saturation lisse et le placement st\xC3\xA9r\xC3\xA9o, en rythme."),
        u8("- Haut de l'\xC3\xA9" "cran = effet plein, bas = neutre (une forme plate en bas ne change rien)."),
        u8("- Clic : ajouter un point ; glisser : d\xC3\xA9placer ; double-clic ou clic droit : supprimer ; losange : courbure."),
        u8("- Cycle : Tempo (division, triolet, point\xC3\xA9) ou Hz. D\xC3\xA9part : position de l'h\xC3\xB4te, note MIDI ou libre."),
        u8("- Lissage : arrondit les fronts ; tout est liss\xC3\xA9 (aucun clic, m\xC3\xAA" "me sur un saut de lecture)."),
        u8("- Chaque module s'allume avec sa touche On et a sa propre profondeur."),
        u8("- Distorsion : sur-\xC3\xA9" "chantillonn\xC3\xA9" "e \xC3\x97" "4 avec anti-repliement ADAA ; la courbe de transfert est \xC3\xA0 droite."),
        u8("- Latence de 71 \xC3\xA9" "chantillons d\xC3\xA9" "clar\xC3\xA9" "e \xC3\xA0 l'h\xC3\xB4te ; le Mix reste en phase.")});
    setResizable(true, true);
    setResizeLimits(560, (int)(560 * kH / kW), 1680, (int)(1680 * kH / kW));
    getConstrainer()->setFixedAspectRatio((double)kW / kH);
    setSize((int)kW, (int)kH);
    timerCallback();
    startTimerHz(30);
}
GaletEditor::~GaletEditor() { stopTimer(); setLookAndFeel(nullptr); }

void GaletEditor::resized() {
    u_ = (float)getWidth() / kW;
    const auto content = getLocalBounds().toFloat().reduced(18.f * u_, 0);
    power.setBounds(kit::w::inSlot(content, u_).toNearestInt());
    help.layout(kit::w::helpSlot(content, u_, true).toNearestInt(), getLocalBounds());
    presets.setBounds(Si(606, 13, 222, 24));
    shapeView->setBounds(Si(32, 66, 600, 232)); transfer->setBounds(Si(648, 66, 280, 232));
    syncSeg->setBounds(Si(32, 328, 116, 30)); retrigSeg->setBounds(Si(156, 328, 196, 30)); feelSeg->setBounds(Si(360, 328, 272, 30));
    interpSeg->setBounds(Si(648, 328, 196, 30)); snapKey.setBounds(Si(852, 328, 76, 30));
    for (int i = 0; i < 5; ++i) pots[i]->setBounds(Si(32 + 120 * (float)i, 390, 117, 78));
    for (int i = 0; i < gal::kNumStarts; ++i) starts[i]->setBounds(Si(648 + 70 * (float)(i % 4), 392 + 36 * (float)(i / 4), 70, 34));
    pots[5]->setBounds(Si(32, 498, 117, 78));
    for (int i = 0; i < 4; ++i) pots[6 + i]->setBounds(Si(160 + 118 * (float)i, 498, 117, 78));
    pots[10]->setBounds(Si(671, 498, 117, 78)); pots[11]->setBounds(Si(788, 498, 117, 78));
    for (int i = 0; i < 4; ++i) pots[12 + i]->setBounds(Si(47 + 150 * (float)i, 606, 117, 78));
    pots[16]->setBounds(Si(671, 606, 117, 78)); pots[17]->setBounds(Si(788, 606, 117, 78));
    // touches On dans la ligne de titre des modules
    modKeys[0]->setBounds(Si(32 + 117 - 46, 472, 46, 22)); modKeys[1]->setBounds(Si(160 + 472 - 46, 472, 46, 22));
    modKeys[2]->setBounds(Si(32 + 600 - 46, 580, 46, 22)); modKeys[3]->setBounds(Si(648 + 280 - 46, 580, 46, 22));
}

void GaletEditor::timerCallback() {
    shapeView->repaint(); transfer->repaint();
    const bool sync = ch(proc, "sync") == 0;   // réglages sans effet dans le mode courant : estompés
    pots[0]->setAlpha(sync ? 1.f : 0.4f); pots[1]->setAlpha(sync ? 0.4f : 1.f); feelSeg->setAlpha(sync ? 1.f : 0.4f);
    if (proc.getCurrentProgram() != shownProgram) { shownProgram = proc.getCurrentProgram(); presets.setSelectedId(shownProgram + 1, juce::dontSendNotification); }
}

void GaletEditor::paint(juce::Graphics& g) {
    const float u = u_;
    kit::w::drawFace(g, pal, getLocalBounds(), u, {kit::w::Family::Rack, "Orbe Galet", kit::w::serial::galet});
    section(g, pal, u, u8("Cycle \xC2\xB7 d\xC3\xA9part"), 32, 306, 600);
    section(g, pal, u, u8("Trac\xC3\xA9"), 648, 306, 280);
    section(g, pal, u, u8("Rythme"), 32, 368, 600);
    section(g, pal, u, u8("Formes de d\xC3\xA9part"), 648, 368, 280);
    section(g, pal, u, u8("Volume"), 32, 476, 117 - 52);
    section(g, pal, u, u8("Filtre"), 160, 476, 472 - 52);
    section(g, pal, u, u8("Sortie"), 648, 476, 280);
    section(g, pal, u, u8("Distorsion lisse"), 32, 584, 600 - 52);
    section(g, pal, u, u8("Pan \xC2\xB7 largeur"), 648, 584, 280 - 52);
}
