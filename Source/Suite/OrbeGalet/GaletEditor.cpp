#include "GaletEditor.h"

using namespace of;
using kit::u8;
namespace v3 = kit::v3;

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
    const v3::Theme* T = nullptr;   // nul : rendu Classique
    void setTheme(const v3::Theme* t) { T = t; setOpaque(t == nullptr); repaint(); }
    void paint3(juce::Graphics& g) {
        const auto& t = *T; const float u = k();
        const auto r = area(); refreshLut();
        const bool sync = ch(proc, "sync") == 0;
        const double cq = sync ? gal::cycleQuarters(ch(proc, "div"), ch(proc, "feel")) : 4.0;
        const int nd = divisions();
        for (int i = 0; i <= nd; ++i) {
            const float x = r.getX() + r.getWidth() * (float)i / (float)nd; const double q = cq * (double)i / (double)nd;
            const bool beat = sync && std::fabs(q - std::round(q)) < 1e-6, bar = beat && ((int)std::llround(q) % 4 == 0);
            g.setColour(juce::Colours::white.withAlpha(bar ? 0.16f : beat ? 0.10f : 0.045f)); g.fillRect(std::round(x), r.getY(), 1.f, r.getHeight());
        }
        for (int i = 0; i <= 4; ++i) { g.setColour(juce::Colours::white.withAlpha(i % 2 ? 0.04f : 0.08f)); g.fillRect(r.getX(), std::round(r.getY() + r.getHeight() * (float)i / 4.f), r.getWidth(), 1.f); }
        juce::Path line, fill; const int steps = juce::jmax(64, (int)r.getWidth());
        for (int i = 0; i <= steps; ++i) {
            const float tt = (float)i / (float)steps, x = r.getX() + r.getWidth() * tt, y = yOf(at(tt), r);
            if (i == 0) { line.startNewSubPath(x, y); fill.startNewSubPath(x, r.getBottom()); fill.lineTo(x, y); } else { line.lineTo(x, y); fill.lineTo(x, y); }
        }
        fill.lineTo(r.getRight(), r.getBottom()); fill.closeSubPath();
        g.setGradientFill(juce::ColourGradient(t.accent.withAlpha(0.32f), 0, r.getY(), t.accent.withAlpha(0.02f), 0, r.getBottom(), false)); g.fillPath(fill);
        const juce::PathStrokeType rs(1.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
        g.setColour(t.accent.withAlpha(0.10f)); g.strokePath(line, juce::PathStrokeType(10.f * u, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(t.accent.withAlpha(0.22f)); g.strokePath(line, juce::PathStrokeType(5.f * u, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(t.accent.brighter(0.45f)); g.strokePath(line, juce::PathStrokeType(2.2f * u, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        const auto& s = proc.shape();
        if (ch(proc, "interp") != gal::Spline)
            for (int kk = 0; kk < s.n; ++kk) {
                const auto hp = handlePos(kk, r); if (hp.x < 0) continue;
                juce::Path d; d.addPolygon(hp, 4, 4.6f * u, juce::MathConstants<float>::pi * 0.25f);
                g.setColour((dragHandle == kk || hoverHandle == kk) ? t.accent.brighter(0.5f) : t.accent.withAlpha(0.55f)); g.fillPath(d);
            }
        for (int kk = 0; kk < s.n; ++kk) {
            const auto c = ptPos(kk, r); const bool hot = dragPt == kk || hoverPt == kk; const float rad = (hot ? 7.f : 5.6f) * u;
            g.setColour(juce::Colour(0xFF0A0D1C)); g.fillEllipse(Rectangle<float>(rad * 2, rad * 2).withCentre(c));
            g.setColour(hot ? t.scrInk : t.accent.brighter(0.3f)); g.drawEllipse(Rectangle<float>(rad * 2, rad * 2).withCentre(c), 2.f * u);
            if (hot) g.fillEllipse(Rectangle<float>(rad * 0.9f, rad * 0.9f).withCentre(c));
        }
        const float ph = proc.phaseNow(), m = proc.modNow(), px = r.getX() + r.getWidth() * ph;
        g.setColour(t.out.withAlpha(0.55f)); g.fillRect(std::round(px), r.getY(), 1.4f, r.getHeight());
        g.setColour(t.out.withAlpha(0.25f)); g.fillEllipse(Rectangle<float>(16.f * u, 16.f * u).withCentre({px, yOf(m, r)}));
        g.setColour(t.out); g.fillEllipse(Rectangle<float>(9.f * u, 9.f * u).withCentre({px, yOf(m, r)}));
        const juce::StringArray divs{"4 mes.", "2 mes.", "1/1", "1/2", "1/4", "1/8", "1/16", "1/32"}, feels{"", " triolet", u8(" point\xC3\xA9")};
        const String head = sync ? u8("Forme \xC2\xB7 cycle ") + divs[ch(proc, "div")] + feels[ch(proc, "feel")] : u8("Forme \xC2\xB7 cycle ") + v3::fmt::hz(proc.raw("hz"));
        g.setColour(t.scrMute); v3::caps(g, head, Rectangle<float>(r.getX(), 10 * u, 360 * u, 18 * u), 11.5f * u, juce::Justification::centredLeft);
        g.setColour(t.scrMute); v3::caps(g, String(s.n) + " / 32 pts", Rectangle<float>(r.getRight() - 300 * u, 10 * u, 130 * u, 18 * u), 11.5f * u, juce::Justification::centredRight);
        g.setColour(t.accent.brighter(0.3f)); v3::tabular(g, "MOD " + v3::fmt::pct(m), Rectangle<float>(r.getRight() - 160 * u, 8 * u, 160 * u, 22 * u), v3::font(15.f * u, true), juce::Justification::centredRight);
        const float by = r.getBottom() + 6 * u;
        g.setFont(v3::font(11.5f * u));
        if (sync && cq >= 1.0) {
            const int nq = (int)std::floor(cq + 1e-6), stepQ = nq > 8 ? 2 : 1;
            for (int q = 0; q < nq; q += stepQ) { const float x = r.getX() + r.getWidth() * (float)(q / cq); g.setColour(q % 4 == 0 ? t.scrInk : t.scrMute); g.drawText(String(q / 4 + 1) + "," + String(q % 4 + 1), Rectangle<float>(x + 2 * u, by, 40 * u, 16 * u), juce::Justification::centredLeft, false); }
        } else {
            g.setColour(t.scrMute);
            g.drawText("0", Rectangle<float>(r.getX() + 2 * u, by, 30 * u, 16 * u), juce::Justification::centredLeft, false);
            g.drawText(u8("\xC2\xBD"), Rectangle<float>(r.getCentreX() - 15 * u, by, 30 * u, 16 * u), juce::Justification::centred, false);
            g.drawText("1 cycle", Rectangle<float>(r.getRight() - 80 * u, by, 80 * u, 16 * u), juce::Justification::centredRight, false);
        }
    }
    void paint(juce::Graphics& g) override {
        if (T != nullptr) { paint3(g); return; }
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
    float k() const { return (float)getWidth() / (T != nullptr ? 740.f : 600.f); }
    juce::Rectangle<float> area() const { const float u = k(); return getLocalBounds().toFloat().withTrimmedLeft((T ? 20 : 12) * u).withTrimmedRight((T ? 20 : 12) * u).withTrimmedTop((T ? 40 : 22) * u).withTrimmedBottom((T ? 32 : 19) * u); }
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
        const float u = k(); int best = -1; float bd = (T ? 11.f : 8.f) * u;
        for (int k = 0; k < proc.shape().n; ++k) { const float d = ptPos(k, r).getDistanceFrom(p); if (d < bd) { bd = d; best = k; } }
        return best;
    }
    int hitHandle(juce::Point<float> p, juce::Rectangle<float> r) const {
        if (ch(proc, "interp") == gal::Spline) return -1;
        const float u = this->k();
        for (int i = 0; i < proc.shape().n; ++i) { const auto h = handlePos(i, r); if (h.x >= 0 && h.getDistanceFrom(p) < (T ? 10.f : 7.f) * u) return i; }
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
    const v3::Theme* T = nullptr;
    void setTheme(const v3::Theme* t) { T = t; setOpaque(t == nullptr); repaint(); }
    void paint3(juce::Graphics& g) {
        const auto& t = *T; const float u = (float)getWidth() / 380.f;
        const int type = ch(proc, "drv_type"); const bool on = ch(proc, "drv_on") == 1;
        const float maxDb = proc.raw("drv_drive") * proc.raw("drv_depth") * (on ? 1.f : 0.f), live = proc.driveNow(), color = proc.raw("drv_color");
        const juce::StringArray types{"Tanh", "Arctan", "Pli doux"};
        g.setColour(t.scrMute); v3::caps(g, u8("Transfert \xC2\xB7 ") + types[type], Rectangle<float>(18 * u, 12 * u, 220 * u, 18 * u), 11.5f * u, juce::Justification::centredLeft);
        if (!on) { g.setColour(t.out); v3::caps(g, u8("Module coup\xC3\xA9"), Rectangle<float>(200 * u, 12 * u, 162 * u, 18 * u), 11.5f * u, juce::Justification::centredRight); }
        auto r = Rectangle<float>(18 * u, 42 * u, 138 * u, 138 * u);
        g.setColour(juce::Colours::white.withAlpha(0.08f)); g.fillRect(std::round(r.getCentreX()), r.getY(), 1.f, r.getHeight()); g.fillRect(r.getX(), std::round(r.getCentreY()), r.getWidth(), 1.f);
        g.setColour(juce::Colours::white.withAlpha(0.06f)); g.drawRoundedRectangle(r, 6 * u, 1.f);
        juce::Path diag; diag.startNewSubPath(r.getX(), r.getBottom()); diag.lineTo(r.getRight(), r.getY());
        const float dash[] = {3.f * u, 3.f * u}; juce::Path dd; juce::PathStrokeType(1.f).createDashedStroke(dd, diag, dash, 2); g.setColour(juce::Colours::white.withAlpha(0.2f)); g.fillPath(dd);
        auto curve = [&](float db) {
            juce::Path p;
            for (int i = 0; i <= 96; ++i) {
                const float x = -1.f + 2.f * (float)i / 96.f, y = juce::jlimit(-1.2f, 1.2f, gal::transfer(type, db, color, x));
                const float px = r.getX() + r.getWidth() * (x + 1.f) * 0.5f, py = r.getCentreY() - r.getHeight() * 0.5f * y / 1.2f;
                if (i == 0) p.startNewSubPath(px, py); else p.lineTo(px, py);
            }
            return p;
        };
        { juce::Graphics::ScopedSaveState st(g); g.reduceClipRegion(r.toNearestInt());
          g.setColour(t.accent.withAlpha(0.45f)); g.strokePath(curve(maxDb), juce::PathStrokeType(1.6f * u, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
          const auto lp = curve(live);
          g.setColour(t.accent.withAlpha(0.2f)); g.strokePath(lp, juce::PathStrokeType(6.f * u, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
          g.setColour(t.scrInk); g.strokePath(lp, juce::PathStrokeType(2.2f * u, juce::PathStrokeType::curved, juce::PathStrokeType::rounded)); }
        const float ix = 184.f * u, iw = 180.f * u;
        auto row = [&](float y, const String& kk, const String& v, Colour c) {
            g.setColour(t.scrMute); v3::caps(g, kk, Rectangle<float>(ix, y, iw, 16 * u), 11.f * u, juce::Justification::centredLeft);
            g.setColour(c); v3::tabular(g, v, Rectangle<float>(ix, y + 14 * u, iw, 24 * u), v3::font(18.f * u, true), juce::Justification::centredLeft);
        };
        row(40 * u, "Drive max", v3::fmt::withUnit("+" + v3::fmt::num(maxDb, 1), "dB"), t.accent.brighter(0.4f));
        row(80 * u, "En direct", v3::fmt::withUnit("+" + v3::fmt::num(live, 1), "dB"), t.scrInk);
        row(120 * u, "Couleur", v3::fmt::pct(color), t.accent.brighter(0.4f));
        g.setColour(t.scrMute); g.setFont(v3::font(11.f * u));
        g.drawText(u8("\xC3\x97" "4 \xC2\xB7 ADAA \xC2\xB7 ") + String(gal::Engine::kLatency) + u8(" \xC3\xA9" "ch."), Rectangle<float>(ix, 164 * u, iw, 16 * u), juce::Justification::centredLeft, false);
        const char* names[4] = {"Vol", "Filtre", "Dist", "Pan"}; const char* ids[4] = {"vol_on", "flt_on", "drv_on", "pan_on"};
        const float bw = 80.f * u;
        for (int kk = 0; kk < 4; ++kk) {
            const float x = 18.f * u + (float)kk * (bw + 8.f * u), y0 = 194.f * u;
            const bool en = ch(proc, ids[kk]) == 1; const float v = juce::jlimit(0.f, 1.f, std::fabs(proc.envNow(kk)));
            if (en && v > 0.02f) { g.setColour(t.accent.withAlpha(0.25f * v)); g.fillEllipse(Rectangle<float>(14.f * u, 14.f * u).withCentre({x + 5 * u, y0 + 8 * u})); }
            g.setColour(en ? t.accent.withAlpha(0.35f + 0.65f * v) : t.scrMute.withAlpha(0.4f)); g.fillEllipse(Rectangle<float>(7.f * u, 7.f * u).withCentre({x + 5 * u, y0 + 8 * u}));
            g.setColour(en ? t.scrInk : t.scrMute); v3::caps(g, names[kk], Rectangle<float>(x + 14 * u, y0, bw - 14 * u, 16 * u), 11.f * u, juce::Justification::centredLeft);
            auto bar = Rectangle<float>(x, y0 + 20 * u, bw, 6 * u);
            g.setColour(juce::Colours::white.withAlpha(0.08f)); g.fillRoundedRectangle(bar, 3 * u);
            g.setColour(en ? t.accent : t.scrMute.withAlpha(0.4f)); g.fillRoundedRectangle(bar.withWidth(juce::jmax(6.f * u, bw * v)), 3 * u);
        }
    }
    void paint(juce::Graphics& g) override {
        if (T != nullptr) { paint3(g); return; }
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
    for (auto& d : defs) {
        auto* pt = pots.add(new Pot(proc.apvts, d.id, u8(d.lab), u8(d.tip), pal)); pt->setStyle(kit::KnobStyle::Cap); addAndMakeVisible(pt);
        auto* k = k3.add(new v3::Knob3(proc.apvts, d.id, u8(d.lab), u8(d.tip), v3::KnobSize::S));
        k->setBipolar(v3::rk::bipolarOf(proc.apvts, d.id)); k->setLabelPx(11.f); addChildComponent(k);
    }
    syncSeg = std::make_unique<v3::bridge::Seg>(proc.apvts, "sync", juce::StringArray{"Tempo", "Hz"}, u8("Tempo : cycle cal\xC3\xA9 sur le tempo de l'h\xC3\xB4te. Hz : vitesse libre."), pal);
    retrigSeg = std::make_unique<v3::bridge::Seg>(proc.apvts, "retrig", juce::StringArray{u8("H\xC3\xB4te"), "Note", "Libre"},
                                      u8("H\xC3\xB4te : suit la position de lecture. Note : chaque note MIDI relance la forme. Libre : tourne sans fin."), pal);
    feelSeg = std::make_unique<v3::bridge::Seg>(proc.apvts, "feel", juce::StringArray{"Droit", "Triolet", u8("Point\xC3\xA9")}, u8("Valeur de la division : droite, en triolet (\xC3\x97 2/3) ou point\xC3\xA9" "e (\xC3\x97 1,5)."), pal);
    interpSeg = std::make_unique<v3::bridge::Seg>(proc.apvts, "interp", juce::StringArray{"Libre", "En S", "Spline"},
                                      u8("Libre : courbure exponentielle par segment. En S : courbure sym\xC3\xA9trique. Spline : passe en douceur par tous les points."), pal);
    for (auto* s : {syncSeg.get(), retrigSeg.get(), feelSeg.get(), interpSeg.get()}) addAndMakeVisible(s);
    const char* mods[4] = {"vol_on", "flt_on", "drv_on", "pan_on"};
    for (auto* id : mods) {
        auto* k = modKeys.add(new v3::rk::Key("On", pal, kJade)); addAndMakeVisible(k);
        k->setTooltip(u8("Active le module : la forme le pilote (fondu sans clic)."));
        modAtts.add(new juce::AudioProcessorValueTreeState::ButtonAttachment(proc.apvts, id, *k));
    }
    snapKey.setToggleState(proc.snap, juce::dontSendNotification);
    snapKey.onClick = [this] { proc.snap = snapKey.getToggleState(); };
    snapKey.setTooltip(u8("Les points s'accrochent \xC3\xA0 la grille."));
    addAndMakeVisible(snapKey);
    linkKey = std::make_unique<v3::rk::Key>(u8("S\xC3\xA9par\xC3\xA9"), pal, kJade); addAndMakeVisible(*linkKey);
    linkKey->setTooltip(u8("Une forme par module : Volume, Filtre, Distorsion et Pan ont chacun la leur (choisir le module \xC3\xA0 droite). D\xC3\xA9sactiv\xC3\xA9 : une forme commune, comme avant."));
    linkAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, "shape_link", *linkKey);
    {
        const char* nm[4] = {"Vol", "Flt", "Dst", "Pan"}, *tp[4] = {"Volume", "Filtre", "Distorsion", "Pan"};
        for (int i = 0; i < 4; ++i) {
            auto* k = modSel.add(new v3::rk::Key(nm[i], pal, kJade)); addChildComponent(k);
            k->setClickingTogglesState(true); k->setRadioGroupId(4417);
            k->setTooltip(u8("\xC3\x89" "diter la forme du module ") + juce::String(tp[i]) + u8(" (formes s\xC3\xA9par\xC3\xA9" "es)."));
            k->onClick = [this, i] { proc.setEditSlot(i); };
        }
    }
    for (int i = 0; i < gal::kNumStarts; ++i) {
        auto* b = starts.add(new v3::bridge::PushKey(u8(gal::kStartNames[i]), pal, kJade)); addAndMakeVisible(b);
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
    addAndMakeVisible(skin.btn);
    skin.btn.onTheme = [this](v3::Mode m) { setThemeMode(m, true); };
    skin.btn.onScale = [this](float s) { setUiScale(s); };
    setResizable(true, true);
    scale_ = v3::Prefs::scale("galet", 1.f);
    setThemeMode(v3::Prefs::theme("galet", v3::Mode::Light), false);
    timerCallback();
    startTimerHz(30);
}
GaletEditor::~GaletEditor() { stopTimer(); v3::Prefs::setScale("galet", scale_); setLookAndFeel(nullptr); }

void GaletEditor::setThemeMode(v3::Mode m, bool persist) {
    skin.set(m);
    const bool n3 = isV3();
    setLookAndFeel(n3 ? static_cast<juce::LookAndFeel*>(&skin.look3) : static_cast<juce::LookAndFeel*>(&look));
    const v3::Theme* tp = skin.tp();
    shapeView->setTheme(tp); transfer->setTheme(tp);
    for (auto* p : pots) p->setVisible(!n3);
    for (auto* k : k3) { k->setVisible(n3); k->setup(&skin.T, &skin.art); }
    for (auto* sg : {syncSeg.get(), retrigSeg.get(), feelSeg.get(), interpSeg.get()}) { sg->T = tp; sg->repaint(); }
    for (auto* k : modKeys) k->T = tp;
    snapKey.T = tp; linkKey->T = tp; for (auto* b : modSel) b->T = tp; for (auto* b : starts) b->T = tp;
    power.T = tp; help.T = tp;
    if (persist) v3::Prefs::setTheme("galet", m);
    if (v3::rk::fit(*this, n3, {kW3, kH3}, {kW, kH}, 0.583f, 1.75f, scale_)) resized();
    sendLookAndFeelChange();
    repaint();
}

void GaletEditor::setUiScale(float s) {
    const bool n3 = isV3();
    scale_ = juce::jlimit(n3 ? 0.75f : 0.583f, n3 ? 1.5f : 1.75f, s);
    setSize(juce::roundToInt((n3 ? kW3 : kW) * scale_), juce::roundToInt((n3 ? kH3 : kH) * scale_));
    v3::Prefs::setScale("galet", scale_);
}

void GaletEditor::resized() {
    if (isV3()) resized3(); else resizedClassic();
    skin.btn.scale = scale_;
}

void GaletEditor::resizedClassic() {
    u_ = (float)getWidth() / kW; scale_ = u_;
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
    k3[0]->setAlpha(sync ? 1.f : 0.4f); k3[1]->setAlpha(sync ? 0.4f : 1.f);
    for (auto* k : k3) k->tickMod(1.f / 30.f);
    {   // formes séparées : sélecteur de module visible, module édité en surbrillance
        const bool sep = !proc.linked();
        for (int i = 0; i < modSel.size(); ++i) { modSel[i]->setVisible(sep); modSel[i]->setToggleState(sep && proc.editSlot() == i, juce::dontSendNotification); }
    }
    if (proc.getCurrentProgram() != shownProgram) { shownProgram = proc.getCurrentProgram(); presets.setSelectedId(shownProgram + 1, juce::dontSendNotification); }
}

void GaletEditor::paint(juce::Graphics& g) {
    if (isV3()) { paint3(g); return; }
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

// ============================================================ v3 « hybride v2 » : coordonnées de conception 1180 x 884
namespace {
constexpr float kCardH = 148.f, kRowC = 390.f, kRowD = 550.f, kRowE = 710.f;
}

void GaletEditor::resized3() {
    u_ = (float)getWidth() / kW3; scale_ = u_;
    help.layout(Si(1054, 34, 30, 30), getLocalBounds());
    skin.btn.setBounds(Si(1020, 35, 28, 28));
    power.setBounds(Si(1090, 36, 70, 26));
    presets.setBounds(Si(360, 34, 270, 32));
    shapeView->setBounds(Si(20, 84, 740, 230)); transfer->setBounds(Si(780, 84, 380, 230));
    syncSeg->setBounds(Si(40, 336, 100, 32)); retrigSeg->setBounds(Si(148, 336, 170, 32)); feelSeg->setBounds(Si(326, 336, 240, 32));
    interpSeg->setBounds(Si(574, 336, 190, 32)); snapKey.setBounds(Si(772, 336, 70, 32));
    linkKey->setBounds(Si(850, 336, 92, 32));
    for (int i = 0; i < 4; ++i) modSel[i]->setBounds(Si(950.f + 52.f * (float)i, 336, 48, 32));
    for (auto* s : {syncSeg.get(), retrigSeg.get(), feelSeg.get(), interpSeg.get()}) s->px = 12.f * u_;
    const float kh = v3::Knob3::boxHeight(v3::KnobSize::S, false), kw = 104.f;
    auto place = [&](int i, float cx, float y) { k3[i]->setScale(u_); k3[i]->setBounds(Si(cx - kw * 0.5f, y, kw, kh)); };
    for (int i = 0; i < 5; ++i) place(i, 40.f + 110.f * ((float)i + 0.5f), kRowC + 34.f);
    for (int i = 0; i < gal::kNumStarts; ++i) starts[i]->setBounds(Si(660.f + 122.f * (float)(i % 4), kRowC + 40.f + 52.f * (float)(i / 4), 116, 44));
    place(5, 95.f, kRowD + 34.f);
    for (int i = 0; i < 4; ++i) place(6 + i, 190.f + 110.f * ((float)i + 0.5f), kRowD + 34.f);
    place(10, 700.f + 55.f, kRowD + 34.f); place(11, 700.f + 165.f, kRowD + 34.f);
    for (int i = 0; i < 4; ++i) place(12 + i, 40.f + 110.f * ((float)i + 0.5f), kRowE + 34.f);
    place(16, 700.f + 55.f, kRowE + 34.f); place(17, 700.f + 165.f, kRowE + 34.f);
    modKeys[0]->setBounds(Si(120, kRowD + 8, 56, 26)); modKeys[1]->setBounds(Si(574, kRowD + 8, 56, 26));
    modKeys[2]->setBounds(Si(424, kRowE + 8, 56, 26)); modKeys[3]->setBounds(Si(864, kRowE + 8, 56, 26));
}

void GaletEditor::paint3(juce::Graphics& g) {
    const auto& T0 = skin.T; juce::ignoreUnused(T0);
    skin.art.draw(g, String("bd|") + v3::modeKey(skin.mode), getLocalBounds().toFloat(), [this](juce::Graphics& ig, Rectangle<float> b) {
        const float k = b.getWidth() / kW3; const auto& t = skin.T;
        auto R = [k](float x, float y, float w, float h) { return Rectangle<float>(x * k, y * k, w * k, h * k); };
        v3::background(ig, t, b);
        v3::rk::brand(ig, t, k, "Orbe", "Galet");
        v3::rk::label(ig, t.inkMute, k, "Preset", 362, 14, 120);
        v3::hero(ig, t, R(20, 84, 740, 230), 22.f * k); v3::hero(ig, t, R(780, 84, 380, 230), 22.f * k);
        v3::card(ig, t, R(20, 326, 1140, 56), 18.f * k);
        for (float y : {kRowC, kRowD, kRowE}) v3::card(ig, t, R(20, y, 1140, kCardH), 18.f * k);
        const auto c = t.inkMute;
        v3::rk::label(ig, c, k, u8("Rythme"), 40, kRowC + 12, 200); v3::rk::label(ig, c, k, u8("Formes de d\xC3\xA9part"), 660, kRowC + 12, 300);
        v3::rk::label(ig, c, k, u8("Volume"), 40, kRowD + 12, 100); v3::rk::label(ig, c, k, u8("Filtre"), 190, kRowD + 12, 200); v3::rk::label(ig, c, k, u8("Sortie"), 700, kRowD + 12, 200);
        v3::rk::label(ig, c, k, u8("Distorsion lisse"), 40, kRowE + 12, 300); v3::rk::label(ig, c, k, u8("Pan \xC2\xB7 largeur"), 700, kRowE + 12, 200);
        v3::rk::sep(ig, t, k, 180, kRowD + 14, 120); v3::rk::sep(ig, t, k, 690, kRowD + 14, 120); v3::rk::sep(ig, t, k, 690, kRowE + 14, 120); v3::rk::sep(ig, t, k, 640, kRowC + 14, 120);
    });
}
