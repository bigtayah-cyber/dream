#include "EqEditor.h"

using namespace of;
using kit::u8;
using juce::Point;
namespace v3 = kit::v3;

namespace {
const char* kTypeName[] = {"Cloche", "Plateau grave", "Plateau aigu", "Passe-haut", "Passe-bas", "Notch"};
String typeName(int t) { return u8(kTypeName[juce::jlimit(0, eq::kTypes - 1, t)]); }
}  // namespace

Colour EqEditor::bandColour(int i) {
    static const Colour c[eq::kBands] = {Colour(0xFFE9E6F5), kPeach, kLilac, kIce, kLilac, kIce, kPeach, Colour(0xFFE9E6F5)};
    return c[juce::jlimit(0, eq::kBands - 1, i)];
}

// ------------------------------------------------------------------ graphe interactif
class EqGraph : public juce::Component, public juce::SettableTooltipClient, private juce::Timer {
public:
    EqGraph(EqProcessor& p, EqEditor& e) : proc(p), ed(e) {
        setOpaque(true);
        setTooltip(u8("Glisser un point : fr\xC3\xA9quence et gain. Molette : Q. Double-clic : r\xC3\xA9initialiser (ou ajouter une bande). Clic droit : type de bande. Maj : r\xC3\xA9glage fin."));
        startTimerHz(30);
    }
    ~EqGraph() override { stopTimer(); }

    Rectangle<float> plot() const {
        const float u = scale();
        return getLocalBounds().toFloat().withTrimmedLeft(36 * u).withTrimmedRight(12 * u).withTrimmedTop(12 * u).withTrimmedBottom(22 * u);
    }
    float scale() const { return (float)getWidth() / 836.f; }
    float sr() const { return (float)(proc.getSampleRate() > 0 ? proc.getSampleRate() : 48000.0); }

    Point<float> pointOf(const eq::Params& p, int i, Rectangle<float> r) const {
        const auto& b = p.b[i]; const float R = proc.rangeDb();
        float d = 0.f;
        if (b.type <= eq::HighShelf) d = b.gain; else if (b.type != eq::Notch) d = (float)eq::bandDb(b, b.freq, sr());
        return {gfx::xOf(b.freq, r), gfx::yOfDb(juce::jlimit(-R, R, d), r, R, -R)};
    }
    int bandAt(Point<float> pos) const {
        const auto p = proc.readParams(); const auto r = plot(); const float rad = 11.f * scale() + 3.f;
        int best = -1; float bd = rad * rad;
        for (int i = eq::kBands - 1; i >= 0; --i) { const auto q = pointOf(p, i, r); const float d = q.getDistanceSquaredFrom(pos); if (d <= bd) { bd = d; best = i; } }
        return best;
    }

    const v3::Theme* T = nullptr;   // nul : rendu Classique
    void setTheme(const v3::Theme* t) { T = t; setOpaque(t == nullptr); repaint(); }
    void paint3(juce::Graphics& g) {
        const auto& t = *T; const float u = scale(), ku = (float)getWidth() / 1060.f;
        const auto r = plot(); const auto P = proc.readParams(); const float R = proc.rangeDb();
        const float stepDb = R > 13.f ? 6.f : 3.f, y0 = gfx::yOfDb(0.f, r, R, -R);
        { juce::Graphics::ScopedSaveState st(g); g.reduceClipRegion(r.expanded(2).toNearestInt());
            for (float d = -R; d <= R + 0.01f; d += stepDb) { g.setColour(juce::Colours::white.withAlpha(std::fabs(d) < 0.01f ? 0.22f : 0.06f)); g.fillRect(r.getX(), gfx::yOfDb(d, r, R, -R) - 0.5f, r.getWidth(), 1.f); }
            gfx::freqGrid(g, r, u, 20.f, 20000.f, {20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000}, false);
            g.setGradientFill(juce::ColourGradient(t.sung.withAlpha(0.22f), 0, r.getY(), t.sung.withAlpha(0.02f), 0, r.getBottom(), false));
            g.fillPath(gfx::specPath(proc.analyzer.db.data(), odsp::Analyzer::kBins, sr(), r, -10.f, -100.f, true));
            g.setColour(t.sung.withAlpha(0.45f)); g.strokePath(gfx::specPath(proc.analyzer.db.data(), odsp::Analyzer::kBins, sr(), r, -10.f, -100.f, false), juce::PathStrokeType(1.2f * ku, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            odsp::Coef cf[eq::kBands]; for (int i = 0; i < eq::kBands; ++i) cf[i] = eq::coefOf(P.b[i], sr());
            auto db = [&](float f, int only) { double m = 1; for (int i = 0; i < eq::kBands; ++i) if (P.b[i].on && (only < 0 || only == i)) m *= cf[i].mag(f, sr()); return (float)(20 * std::log10(m + 1e-9)); };
            const int N = juce::jmax(64, (int)(r.getWidth() / 3.f)), sel = ed.selected();
            vals.resize((size_t)N); selv.resize((size_t)N);
            for (int i = 0; i < N; ++i) { const float f = gfx::fOf(r.getX() + r.getWidth() * (float)i / (float)(N - 1), r); vals[(size_t)i] = db(f, -1); selv[(size_t)i] = P.b[sel].on ? db(f, sel) : 0.f; }
            auto X = [&](float i) { return r.getX() + r.getWidth() * i / (float)(N - 1); };
            auto Y = [&](float d) { return juce::jlimit(r.getY() - 2, r.getBottom() + 2, gfx::yOfDb(d, r, R, -R)); };
            trSel.build(selv.data(), N, X, Y, y0, {N, 0.f, 1e9f, -1e9f, 0}); trTot.build(vals.data(), N, X, Y, y0, {N, 0.f, 1e9f, -1e9f, 0});
            trSel.draw(g, EqEditor::bandColour(sel), 1.f * ku, 0.f, 0.28f, y0);
            trTot.draw(g, t.out, 2.4f * ku, 1.f, 0.10f, y0);
        }
        g.setFont(v3::font(11.5f * ku));
        for (float d = -R; d <= R + 0.01f; d += stepDb) { g.setColour(t.scrMute); g.drawText(d > 0.01f ? "+" + String((int)d) : d < -0.01f ? v3::fmt::minus() + String((int)-d) : String(0), Rectangle<float>(2 * u, gfx::yOfDb(d, r, R, -R) - 8 * ku, 38 * u, 16 * ku), juce::Justification::centredRight, false); }
        for (float f : {20.f, 50.f, 100.f, 200.f, 500.f, 1000.f, 2000.f, 5000.f, 10000.f, 20000.f}) { g.setColour(t.scrMute); g.drawText(gfx::hzLabel(f), Rectangle<float>(gfx::xOf(f, r) - 18 * ku, r.getBottom() + 5 * ku, 36 * ku, 16 * ku), juce::Justification::centred, false); }
        g.setColour(t.scrMute); v3::caps(g, u8("Spectre d'entr\xC3\xA9" "e"), Rectangle<float>(r.getX() + 10 * ku, r.getY() + 6 * ku, 220 * ku, 16 * ku), 11.f * ku, juce::Justification::centredLeft);
        for (int i = 0; i < eq::kBands; ++i) {
            const auto c = pointOf(P, i, r); const auto col = EqEditor::bandColour(i); const bool on = P.b[i].on, isSel = i == ed.selected(), hot = i == hover || i == dragBand;
            const float rad = (isSel ? 9.f : 7.5f) * ku + (hot ? 1.5f * ku : 0.f);
            if (isSel || hot) { g.setColour(col.withAlpha(on ? 0.25f : 0.10f)); g.fillEllipse(Rectangle<float>((rad + 6 * ku) * 2, (rad + 6 * ku) * 2).withCentre(c)); }
            g.setColour(on ? col : juce::Colour(0xFF121626)); g.fillEllipse(Rectangle<float>(rad * 2, rad * 2).withCentre(c));
            if (!on) { g.setColour(col.withAlpha(0.6f)); g.drawEllipse(Rectangle<float>(rad * 2, rad * 2).withCentre(c), 1.5f * ku); }
            if (isSel) { g.setColour(juce::Colours::white.withAlpha(0.9f)); g.drawEllipse(Rectangle<float>((rad + 3 * ku) * 2, (rad + 3 * ku) * 2).withCentre(c), 1.3f * ku); }
            g.setColour(on ? juce::Colour(0xFF0E1124) : col); g.setFont(v3::font(10.5f * ku, true));
            g.drawText(String(i + 1), Rectangle<float>(rad * 2, rad * 2).withCentre(c), juce::Justification::centred, false);
        }
        const int hb = dragBand >= 0 ? dragBand : hover;
        if (hb >= 0) {
            const auto& b = P.b[hb]; const auto c = pointOf(P, hb, r);
            String tx = String(hb + 1) + "   " + v3::fmt::hz(b.freq);
            if (eq::hasGain(b.type)) tx += "   " + v3::fmt::db(b.gain);
            tx += "   Q " + v3::fmt::num(b.q, 2);
            const auto f = v3::font(14.f * ku, true); const float w = v3::tabularWidth(tx, f) + 24 * ku;
            auto box = Rectangle<float>(c.x - w / 2, c.y - 44 * ku, w, 28 * ku).constrainedWithin(r);
            g.setColour(juce::Colour(0xE60A0D1C)); g.fillRoundedRectangle(box, 9 * ku); g.setColour(EqEditor::bandColour(hb).withAlpha(0.7f)); g.drawRoundedRectangle(box, 9 * ku, 1.f);
            g.setColour(t.scrInk); v3::tabular(g, tx, box, f, juce::Justification::centred);
        }
    }
    v3::Trace trSel, trTot; std::vector<float> vals, selv;
    void paint(juce::Graphics& g) override {
        if (T != nullptr) { paint3(g); return; }
        const float u = scale(); const auto pal = of::palette();
        kit::screenWell(g, pal, getLocalBounds().toFloat(), kit::Display::OLED);
        const auto r = plot(); const auto P = proc.readParams(); const float R = proc.rangeDb();
        g.saveState(); g.reduceClipRegion(r.expanded(2).toNearestInt());
        // grille en dB
        const float stepDb = R > 13.f ? 6.f : 3.f;
        for (float d = -R; d <= R + 0.01f; d += stepDb) {
            g.setColour(juce::Colours::white.withAlpha(std::fabs(d) < 0.01f ? 0.28f : 0.07f));
            g.drawHorizontalLine(juce::roundToInt(gfx::yOfDb(d, r, R, -R)), r.getX(), r.getRight());
        }
        gfx::freqGrid(g, r, u, 20.f, 20000.f, {20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000}, false);
        // analyseur (entrée) en fond
        {
            const auto sp = gfx::specPath(proc.analyzer.db.data(), odsp::Analyzer::kBins, sr(), r, -10.f, -100.f, true);
            g.setGradientFill(juce::ColourGradient(kLilac.withAlpha(0.20f), 0, r.getY(), kLilac.withAlpha(0.02f), 0, r.getBottom(), false)); g.fillPath(sp);
            g.setColour(kLilac.withAlpha(0.35f)); g.strokePath(gfx::specPath(proc.analyzer.db.data(), odsp::Analyzer::kBins, sr(), r, -10.f, -100.f, false), juce::PathStrokeType(1.f * u));
        }
        // courbe de réponse cumulée
        odsp::Coef cf[eq::kBands]; for (int i = 0; i < eq::kBands; ++i) cf[i] = eq::coefOf(P.b[i], sr());
        juce::Path curve, fill; const float y0 = gfx::yOfDb(0.f, r, R, -R);
        auto db = [&](float f, int only) { double m = 1; for (int i = 0; i < eq::kBands; ++i) if (P.b[i].on && (only < 0 || only == i)) m *= cf[i].mag(f, sr()); return (float)(20 * std::log10(m + 1e-9)); };
        const int sel = ed.selected();
        juce::Path selFill;
        for (float x = r.getX(); x <= r.getRight() + 0.1f; x += 2.f) {
            const float f = gfx::fOf(x, r), y = juce::jlimit(r.getY() - 2, r.getBottom() + 2, gfx::yOfDb(db(f, -1), r, R, -R));
            if (x == r.getX()) { curve.startNewSubPath(x, y); fill.startNewSubPath(x, y0); fill.lineTo(x, y); selFill.startNewSubPath(x, y0); } else { curve.lineTo(x, y); fill.lineTo(x, y); }
            selFill.lineTo(x, juce::jlimit(r.getY() - 2, r.getBottom() + 2, gfx::yOfDb(P.b[sel].on ? db(f, sel) : 0.f, r, R, -R)));
        }
        fill.lineTo(r.getRight(), y0); fill.closeSubPath(); selFill.lineTo(r.getRight(), y0); selFill.closeSubPath();
        g.setColour(bandColourFor(sel).withAlpha(0.16f)); g.fillPath(selFill);
        g.setColour(kIce.withAlpha(0.10f)); g.fillPath(fill);
        g.setColour(juce::Colours::black.withAlpha(0.5f)); g.strokePath(curve, juce::PathStrokeType(3.6f * u));
        g.setColour(Colour(0xFFF3F0FF)); g.strokePath(curve, juce::PathStrokeType(1.8f * u));
        g.restoreState();
        // repères dB
        g.setFont(kit::mono(7.6f * u));
        for (float d = -R; d <= R + 0.01f; d += stepDb) { g.setColour(pal.mute); g.drawText((d > 0.01f ? "+" : "") + String((int)d), Rectangle<float>(2 * u, gfx::yOfDb(d, r, R, -R) - 5 * u, 31 * u, 10 * u), juce::Justification::centredRight); }
        for (float f : {20.f, 50.f, 100.f, 200.f, 500.f, 1000.f, 2000.f, 5000.f, 10000.f, 20000.f}) { g.setColour(pal.mute); g.drawText(gfx::hzLabel(f), Rectangle<float>(gfx::xOf(f, r) - 14 * u, r.getBottom() + 3 * u, 28 * u, 10 * u), juce::Justification::centred); }
        g.setColour(pal.mute); g.setFont(kit::mono(7.6f * u));
        g.drawText("HZ", Rectangle<float>(r.getRight() - 20 * u, r.getBottom() + 12 * u, 30 * u, 9 * u), juce::Justification::centredRight);
        g.drawText(u8("SPECTRE D'ENTR\xC3\x89" "E"), Rectangle<float>(r.getX() + 6 * u, r.getY() + 2 * u, 160 * u, 10 * u), juce::Justification::centredLeft);
        // points
        for (int i = 0; i < eq::kBands; ++i) {
            const auto c = pointOf(P, i, r); const auto col = bandColourFor(i); const bool on = P.b[i].on, isSel = i == sel, hot = i == hover || i == dragBand;
            const float rad = (isSel ? 8.f : 6.5f) * u + (hot ? 1.5f * u : 0.f);
            if (isSel || hot) { g.setColour(col.withAlpha(on ? 0.25f : 0.10f)); g.fillEllipse(c.x - rad - 5 * u, c.y - rad - 5 * u, (rad + 5 * u) * 2, (rad + 5 * u) * 2); }
            g.setColour(juce::Colours::black.withAlpha(0.6f)); g.fillEllipse(c.x - rad - 1, c.y - rad + 1.5f, rad * 2 + 2, rad * 2 + 2);
            if (on) { g.setGradientFill(juce::ColourGradient(col.brighter(0.25f), c.x - rad * 0.4f, c.y - rad, col.darker(0.35f), c.x + rad * 0.5f, c.y + rad, false)); g.fillEllipse(c.x - rad, c.y - rad, rad * 2, rad * 2); }
            else { g.setColour(Colour(0xFF121216)); g.fillEllipse(c.x - rad, c.y - rad, rad * 2, rad * 2); g.setColour(col.withAlpha(0.55f)); g.drawEllipse(c.x - rad, c.y - rad, rad * 2, rad * 2, 1.4f * u); }
            if (isSel) { g.setColour(juce::Colours::white.withAlpha(0.9f)); g.drawEllipse(c.x - rad - 2.5f * u, c.y - rad - 2.5f * u, (rad + 2.5f * u) * 2, (rad + 2.5f * u) * 2, 1.2f * u); }
            g.setColour(on ? Colour(0xFF17171B) : col.withAlpha(0.8f)); g.setFont(kit::fnt(8.4f * u, true));
            g.drawText(String(i + 1), Rectangle<float>(c.x - rad, c.y - rad, rad * 2, rad * 2), juce::Justification::centred);
        }
        // info-bulle de la bande survolée ou déplacée
        const int hb = dragBand >= 0 ? dragBand : hover;
        if (hb >= 0) {
            const auto& b = P.b[hb]; const auto c = pointOf(P, hb, r);
            String t = String(hb + 1) + "  " + fmtHz(b.freq);
            if (eq::hasGain(b.type)) t += "   " + fmtDb(b.gain);
            t += "   Q " + String(b.q, 2);
            g.setFont(kit::mono(9.f * u)); const float w = juce::GlyphArrangement::getStringWidth(g.getCurrentFont(), t) + 14 * u;
            auto box = Rectangle<float>(c.x - w / 2, c.y - 34 * u, w, 17 * u).constrainedWithin(r);
            g.setColour(Colour(0xE60B0B10)); g.fillRoundedRectangle(box, 3 * u); g.setColour(bandColourFor(hb).withAlpha(0.7f)); g.drawRoundedRectangle(box, 3 * u, 1.f);
            g.setColour(pal.screenInk); g.drawText(t, box, juce::Justification::centred);
        }
    }
    static Colour bandColourFor(int i) { return EqEditor::bandColour(i); }

    void mouseMove(const juce::MouseEvent& e) override {
        const int h = bandAt(e.position);
        setMouseCursor(h >= 0 ? juce::MouseCursor::UpDownLeftRightResizeCursor : juce::MouseCursor::NormalCursor);
        hover = h;
    }
    void mouseExit(const juce::MouseEvent&) override { hover = -1; }
    void mouseDown(const juce::MouseEvent& e) override {
        const int h = bandAt(e.position); dragBand = -1;
        if (h < 0) return;
        ed.select(h);
        if (e.mods.isPopupMenu()) { showMenu(h); return; }
        auto* on = proc.par(h, "on");
        if (on->getValue() < 0.5f) proc.setReal(on, 1.f);
        dragBand = h; startMouse = e.position; startPt = pointOf(proc.readParams(), h, plot());
        proc.par(h, "f")->beginChangeGesture(); proc.par(h, "g")->beginChangeGesture();
    }
    void mouseDrag(const juce::MouseEvent& e) override {
        if (dragBand < 0) return;
        const auto r = plot(); const float R = proc.rangeDb();
        const float k = e.mods.isShiftDown() ? 0.12f : 1.f;
        const auto t = startPt + (e.position - startMouse) * k;
        const float x = juce::jlimit(r.getX(), r.getRight(), t.x), y = juce::jlimit(r.getY(), r.getBottom(), t.y);
        proc.setReal(proc.par(dragBand, "f"), gfx::fOf(x, r));
        if (eq::hasGain(proc.readParams().b[dragBand].type)) proc.setReal(proc.par(dragBand, "g"), R - (y - r.getY()) / r.getHeight() * 2.f * R);
    }
    void mouseUp(const juce::MouseEvent&) override {
        if (dragBand >= 0) { proc.par(dragBand, "f")->endChangeGesture(); proc.par(dragBand, "g")->endChangeGesture(); }
        dragBand = -1;
    }
    void mouseDoubleClick(const juce::MouseEvent& e) override {
        const int h = bandAt(e.position);
        if (h >= 0) { ed.select(h); proc.resetBand(h); return; }
        const auto P = proc.readParams(); const auto r = plot(); const float R = proc.rangeDb();
        if (!r.contains(e.position)) return;
        for (int i = 2; i <= 5; ++i) if (!P.b[i].on) {   // première cloche libre : on la place sous le curseur
            proc.setReal(proc.par(i, "t"), (float)eq::Bell); proc.setReal(proc.par(i, "f"), gfx::fOf(e.position.x, r));
            proc.setReal(proc.par(i, "g"), juce::jlimit(-R, R, R - (e.position.y - r.getY()) / r.getHeight() * 2.f * R)); proc.setReal(proc.par(i, "on"), 1.f);
            ed.select(i); return;
        }
    }
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override {
        int b = bandAt(e.position); if (b < 0) b = ed.selected();
        auto* q = proc.par(b, "q"); ed.select(b);
        const float d = w.deltaY * (w.isReversed ? -1.f : 1.f);
        const float nq = juce::jlimit(0.1f, 18.f, q->convertFrom0to1(q->getValue()) * std::exp(d * 1.6f));
        q->beginChangeGesture(); proc.setReal(q, nq); q->endChangeGesture();
    }
private:
    void timerCallback() override { proc.analyzer.compute(); repaint(); }
    void showMenu(int b) {
        juce::PopupMenu m;
        const int cur = juce::roundToInt(proc.par(b, "t")->convertFrom0to1(proc.par(b, "t")->getValue()));
        m.addSectionHeader(u8("Type de la bande ") + String(b + 1));
        for (int t = 0; t < eq::kTypes; ++t) m.addItem(t + 1, typeName(t), true, t == cur);
        m.addSeparator();
        m.addItem(20, "Bande active", true, proc.par(b, "on")->getValue() > 0.5f);
        m.addItem(21, u8("R\xC3\xA9initialiser la bande"));
        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withTargetScreenArea(localAreaToGlobal(Rectangle<int>((int)startMouse.x, (int)startMouse.y, 1, 1))),
                        [this, b](int res) {
                            if (res >= 1 && res <= eq::kTypes) { auto* t = proc.par(b, "t"); t->beginChangeGesture(); proc.setReal(t, (float)(res - 1)); t->endChangeGesture(); }
                            else if (res == 20) { auto* o = proc.par(b, "on"); proc.setReal(o, o->getValue() > 0.5f ? 0.f : 1.f); }
                            else if (res == 21) proc.resetBand(b);
                        });
    }
    EqProcessor& proc; EqEditor& ed;
    int hover = -1, dragBand = -1;
    Point<float> startMouse, startPt;
};

// ------------------------------------------------------------------ rangée des 8 bandes
class BandChips : public juce::Component, public juce::SettableTooltipClient {
public:
    BandChips(EqProcessor& p, EqEditor& e) : proc(p), ed(e) { setTooltip(u8("Choisir une bande. Le voyant active ou coupe la bande.")); }
    const v3::Theme* T = nullptr;
    void paint(juce::Graphics& g) override {
        if (T != nullptr) {
            const auto& t = *T; const float ku = (float)getWidth() / 1060.f, w = (float)getWidth() / 8.f; const auto P = proc.readParams();
            for (int i = 0; i < eq::kBands; ++i) {
                auto c = Rectangle<float>(w * (float)i, 0, w, (float)getHeight()).reduced(4 * ku, 2);
                const bool s = i == ed.selected(), on = P.b[i].on; const float rad = c.getHeight() * 0.5f;
                if (s) { juce::DropShadow(t.shadow, juce::roundToInt(rad * 0.6f), {0, 1}).drawForRectangle(g, c.toNearestInt()); g.setColour(t.card); } else g.setColour(t.field);
                g.fillRoundedRectangle(c, rad);
                g.setColour(s ? t.accent : t.fieldEdge); g.drawRoundedRectangle(c.reduced(0.5f), rad, s ? 1.5f : 1.f);
                const auto bc = EqEditor::bandColour(i), dot = on ? (t.dark ? bc : bc.darker(0.35f)) : t.track;
                if (on) { g.setColour(dot.withAlpha(0.25f)); g.fillEllipse(Rectangle<float>(18 * ku, 18 * ku).withCentre({c.getX() + 18 * ku, c.getCentreY()})); }
                g.setColour(dot); g.fillEllipse(Rectangle<float>(10 * ku, 10 * ku).withCentre({c.getX() + 18 * ku, c.getCentreY()}));
                g.setColour(on ? t.ink : t.inkMute); g.setFont(v3::font(13.f * ku, true));
                g.drawText(String(i + 1), Rectangle<float>(c.getX() + 32 * ku, c.getY(), 14 * ku, c.getHeight()), juce::Justification::centred, false);
                v3::tabular(g, on ? v3::fmt::hz(P.b[i].freq) : String("off"), Rectangle<float>(c.getX() + 48 * ku, c.getY(), c.getWidth() - 56 * ku, c.getHeight()), v3::font(13.f * ku, on), juce::Justification::centredLeft);
            }
            return;
        }
        const float u = (float)getWidth() / 836.f, w = (float)getWidth() / 8.f; const auto P = proc.readParams(); const auto pal = of::palette();
        for (int i = 0; i < eq::kBands; ++i) {   // touches de bande : LED de la couleur de la bande (allumée = bande active)
            auto c = Rectangle<float>(w * (float)i, 0, w, (float)getHeight()).reduced(3 * u, 2);
            const bool s = i == ed.selected(), on = P.b[i].on;
            kit::pushButton(g, pal, c, false, s, EqEditor::bandColour(i), {});
            if (s) { g.setColour(EqEditor::bandColour(i).withAlpha(0.75f)); g.drawRoundedRectangle(c.expanded(1.5f), 4.f * u, 1.2f); }
            kit::led(g, c.getX() + 11 * u, c.getCentreY(), 3.4f * u, on ? 1.f : 0.f, EqEditor::bandColour(i), kit::LedBezel::None);
            g.setColour(on ? pal.ink : pal.ink.withAlpha(0.55f)); g.setFont(kit::fnt(9.6f * u, true));
            g.drawText(String(i + 1), Rectangle<float>(c.getX() + 20 * u, c.getY(), 12 * u, c.getHeight()), juce::Justification::centred);
            g.setFont(kit::lcdFont(9.6f * u)); g.setColour(on ? pal.screenInk : pal.ink.withAlpha(0.5f));
            g.drawText(on ? fmtHz(P.b[i].freq) : String("OFF"), Rectangle<float>(c.getX() + 34 * u, c.getY(), c.getWidth() - 36 * u, c.getHeight()), juce::Justification::centredLeft);
        }
    }
    void mouseDown(const juce::MouseEvent& e) override {
        const float w = (float)getWidth() / 8.f; const int i = juce::jlimit(0, 7, (int)(e.position.x / w));
        if (e.position.x - w * (float)i < (T != nullptr ? 34.f : 20.f) * (float)getWidth() / (T != nullptr ? 1060.f : 836.f)) { auto* o = proc.par(i, "on"); proc.setReal(o, o->getValue() > 0.5f ? 0.f : 1.f); }
        ed.select(i);
    }
private:
    EqProcessor& proc; EqEditor& ed;
};

// ------------------------------------------------------------------ champ numérique (glisser, molette, double-clic pour saisir)
class EqField : public juce::Component, public juce::SettableTooltipClient {
public:
    const v3::Theme* T = nullptr;
    void setTheme(const v3::Theme* t) {
        T = t;
        if (t != nullptr) { ed.setColour(juce::Label::textColourId, t->ink); ed.setColour(juce::Label::textWhenEditingColourId, t->ink); ed.setColour(juce::TextEditor::backgroundColourId, t->card); ed.setColour(juce::TextEditor::outlineColourId, t->accent); ed.setColour(juce::TextEditor::textColourId, t->ink); ed.setColour(juce::CaretComponent::caretColourId, t->accentText); }
        else { ed.setColour(juce::Label::textColourId, kIce); ed.setColour(juce::Label::textWhenEditingColourId, juce::Colours::white); ed.setColour(juce::TextEditor::backgroundColourId, Colour(0xFF050508)); ed.setColour(juce::TextEditor::outlineColourId, kLilac); ed.setColour(juce::TextEditor::textColourId, juce::Colours::white); ed.setColour(juce::CaretComponent::caretColourId, kLilac); }
        resized(); refresh();
    }
    EqField(const String& cap, const String& tip) : caption(cap) {
        setTooltip(tip); setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
        ed.setJustificationType(juce::Justification::centred); ed.setInterceptsMouseClicks(false, true);
        ed.setColour(juce::Label::textColourId, kIce); ed.setColour(juce::Label::textWhenEditingColourId, juce::Colours::white);
        ed.setColour(juce::TextEditor::backgroundColourId, Colour(0xFF050508)); ed.setColour(juce::TextEditor::outlineColourId, kLilac);
        ed.setColour(juce::TextEditor::textColourId, juce::Colours::white); ed.setColour(juce::CaretComponent::caretColourId, kLilac);
        ed.onTextChange = [this] { if (par) { par->beginChangeGesture(); par->setValueNotifyingHost(par->getValueForText(ed.getText().replace(",", ".").replace(v3::fmt::minus(), "-").replace(juce::String::charToString((juce::juce_wchar)0x00A0), " "))); par->endChangeGesture(); } refresh(); };
        addAndMakeVisible(ed);
    }
    void bind(juce::RangedAudioParameter* p, bool en) { par = p; enabled = en; setInterceptsMouseClicks(en, true); refresh(); }
    void refresh() { if (par && !ed.isBeingEdited()) ed.setText(enabled ? (T != nullptr ? v3::fmt::tidy(par->getText(par->getValue(), 0)) : par->getText(par->getValue(), 0)) : juce::String::fromUTF8("\xE2\x80\x94"), juce::dontSendNotification); ed.setAlpha(enabled ? 1.f : 0.4f); repaint(); }
    void resized() override {
        const float u = (float)getHeight() / 44.f;
        if (T != nullptr) { const float k = (float)getHeight() / 64.f; ed.setFont(v3::font(24.f * k, true)); ed.setBounds(getLocalBounds().withTrimmedTop(int(22 * k)).reduced(6)); return; }
        ed.setFont(kit::mono(13.f * u)); ed.setBounds(getLocalBounds().withTrimmedTop(int(14 * u)).reduced(3));
    }
    void paint(juce::Graphics& g) override {
        const float u = (float)getHeight() / 44.f; auto r = getLocalBounds().toFloat();
        if (T != nullptr) {
            const float k = (float)getHeight() / 64.f;
            g.setColour(T->field); g.fillRoundedRectangle(r.reduced(0.5f), 14 * k); g.setColour(T->fieldEdge); g.drawRoundedRectangle(r.reduced(0.5f), 14 * k, 1.f);
            g.setColour(T->inkMute.withAlpha(enabled ? 1.f : 0.6f)); v3::caps(g, caption, Rectangle<float>(r.getX() + 14 * k, r.getY() + 8 * k, r.getWidth() - 20 * k, 14 * k), 11.f * k, juce::Justification::centredLeft);
            return;
        }
        kit::screenWell(g, of::palette(), r);
        g.setColour(of::palette().mute); kit::tracked(g, caption, Rectangle<float>(r.getX() + 9 * u, r.getY() + 5 * u, r.getWidth() - 12 * u, 9 * u), juce::Justification::centredLeft, 1.3f * u, kit::fnt(7.4f * u, true));
    }
    void mouseDown(const juce::MouseEvent&) override { if (!par || !enabled) return; v0 = par->getValue(); par->beginChangeGesture(); }
    void mouseDrag(const juce::MouseEvent& e) override { if (!par || !enabled) return; par->setValueNotifyingHost(juce::jlimit(0.f, 1.f, v0 - (float)e.getDistanceFromDragStartY() / (e.mods.isShiftDown() ? 1400.f : 170.f))); refresh(); }
    void mouseUp(const juce::MouseEvent&) override { if (par && enabled) par->endChangeGesture(); }
    void mouseDoubleClick(const juce::MouseEvent&) override { if (par && enabled) ed.showEditor(); }
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override {
        if (!par || !enabled) return;
        par->beginChangeGesture(); par->setValueNotifyingHost(juce::jlimit(0.f, 1.f, par->getValue() + w.deltaY * (e.mods.isShiftDown() ? 0.02f : 0.12f))); par->endChangeGesture(); refresh();
    }
private:
    juce::Label ed; String caption; juce::RangedAudioParameter* par = nullptr; bool enabled = true; float v0 = 0.f;
};

// ------------------------------------------------------------------ éditeur
EqEditor::EqEditor(EqProcessor& p)
    : AudioProcessorEditor(&p), proc(p), pal(of::palette()), look(pal), onKey("Active", pal), aKey("A", pal, kIce), bKey("B", pal, kPeach), power(pal) {
    setLookAndFeel(&look);
    graph = std::make_unique<EqGraph>(proc, *this); addAndMakeVisible(*graph);
    chips = std::make_unique<BandChips>(proc, *this); addAndMakeVisible(*chips);
    fFreq = std::make_unique<EqField>(u8("Fr\xC3\xA9quence"), u8("Fr\xC3\xA9quence de la bande. Glisser, molette ou double-clic pour saisir (ex. 2,5k).")); 
    fGain = std::make_unique<EqField>("Gain", u8("Gain de la bande en dB (sans effet sur les filtres passe-haut, passe-bas et notch)."));
    fQ = std::make_unique<EqField>("Q", u8("Facteur de qualit\xC3\xA9 : plus haut, plus \xC3\xA9troit. Molette sur le graphe."));
    for (auto* f : {fFreq.get(), fGain.get(), fQ.get()}) addAndMakeVisible(*f);
    for (int t = 0; t < eq::kTypes; ++t) typeBox.addItem(typeName(t), t + 1);
    typeBox.setTooltip(u8("Type de la bande : cloche, plateaux, passe-haut, passe-bas, notch."));
    typeBox.onChange = [this] {
        if (updating) return; auto* t = proc.par(sel, "t"); t->beginChangeGesture(); proc.setReal(t, (float)(typeBox.getSelectedId() - 1)); t->endChangeGesture();
    };
    addAndMakeVisible(typeBox);
    addAndMakeVisible(onKey);
    onKey.onClick = [this] { if (!updating) proc.setReal(proc.par(sel, "on"), onKey.getToggleState() ? 1.f : 0.f); };
    onKey.setTooltip(u8("Active ou coupe la bande s\xC3\xA9lectionn\xC3\xA9" "e."));
    outPot = std::make_unique<Pot>(proc.apvts, "out", "Sortie", u8("Gain de sortie apr\xC3\xA8s l'\xC3\xA9galiseur."), pal); outPot->setStyle(kit::KnobStyle::Cap); addAndMakeVisible(*outPot);
    rangeSeg = std::make_unique<v3::bridge::Seg>(proc.apvts, "range", juce::StringArray{u8("\xC2\xB1" "12"), u8("\xC2\xB1" "24")}, u8("\xC3\x89" "chelle verticale du graphe en dB."), pal); addAndMakeVisible(*rangeSeg);
    for (auto* k : {&aKey, &bKey}) { k->setClickingTogglesState(false); addAndMakeVisible(*k); }
    aKey.onClick = [this] { proc.switchAB(0); }; bKey.onClick = [this] { proc.switchAB(1); };
    aKey.setTooltip(u8("M\xC3\xA9moire A : compare deux r\xC3\xA9glages complets.")); bKey.setTooltip(u8("M\xC3\xA9moire B : compare deux r\xC3\xA9glages complets."));
    prev.onClick = [this] { proc.applyPreset((proc.currentPreset() + eq::kPresets - 1) % eq::kPresets); };
    next.onClick = [this] { proc.applyPreset((proc.currentPreset() + 1) % eq::kPresets); };
    prev.setTooltip(u8("Preset pr\xC3\xA9" "c\xC3\xA9" "dent")); next.setTooltip("Preset suivant");
    addAndMakeVisible(prev); addAndMakeVisible(next);
    addAndMakeVisible(power);
    power.setTooltip(u8("\xC3\x89galiseur en service. Bascule pour le couper (bypass)."));
    help.install(*this, "Orbe EQ", {u8("\xC3\x89galiseur param\xC3\xA9trique 8 bandes \xC3\xA0 phase minimale."),
        u8("- Graphe : glisser un point r\xC3\xA8gle fr\xC3\xA9quence et gain ; molette : Q ; double-clic : r\xC3\xA9initialiser la bande (ou en ajouter une) ; clic droit : type de bande ; Maj : r\xC3\xA9glage fin."),
        u8("- Touches 1 \xC3\xA0 8 : choisir une bande ; cliquer sur sa LED l'active ou la coupe."),
        u8("- Champs Fr\xC3\xA9quence, Gain, Q : glisser, molette, ou double-clic pour saisir (ex. 2,5k)."),
        u8("- Afficheur du haut : preset (clic pour la liste, fl\xC3\xA8" "ches pour d\xC3\xA9" "filer). A / B : deux m\xC3\xA9moires compl\xC3\xA8tes \xC3\xA0 comparer."),
        u8("- \xC3\x89" "chelle : \xC2\xB1" "12 ou \xC2\xB1" "24 dB pour le graphe.")});
    powerAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, "bypass", power);
    cPrev.onClick = prev.onClick; cNext.onClick = next.onClick; cPrev.setTooltip(prev.getTooltip()); cNext.setTooltip(next.getTooltip());
    addChildComponent(cPrev); addChildComponent(cNext);
    outK3 = std::make_unique<v3::Knob3>(proc.apvts, "out", "Sortie", u8("Gain de sortie apr\xC3\xA8s l'\xC3\xA9galiseur."), v3::KnobSize::S);
    outK3->setBipolar(v3::rk::bipolarOf(proc.apvts, "out")); outK3->setLabelPx(11.f); addChildComponent(*outK3);
    addAndMakeVisible(skin.btn);
    skin.btn.onTheme = [this](v3::Mode m) { setThemeMode(m, true); };
    skin.btn.onScale = [this](float s) { setUiScale(s); };
    setResizable(true, true);
    scale_ = v3::Prefs::scale("eq", 1.f);
    setThemeMode(v3::Prefs::theme("eq", v3::Mode::Light), false);
    refreshFields();
    startTimerHz(20);
}
EqEditor::~EqEditor() { stopTimer(); v3::Prefs::setScale("eq", scale_); setLookAndFeel(nullptr); }

void EqEditor::setThemeMode(v3::Mode m, bool persist) {
    skin.set(m);
    const bool n3 = isV3();
    setLookAndFeel(n3 ? static_cast<juce::LookAndFeel*>(&skin.look3) : static_cast<juce::LookAndFeel*>(&look));
    const v3::Theme* tp = skin.tp();
    graph->setTheme(tp); chips->T = tp; fFreq->setTheme(tp); fGain->setTheme(tp); fQ->setTheme(tp);
    outPot->setVisible(!n3); outK3->setVisible(n3); outK3->setup(&skin.T, &skin.art);
    prev.setVisible(!n3); next.setVisible(!n3); cPrev.T = cNext.T = tp; cPrev.setVisible(n3); cNext.setVisible(n3);
    for (auto* k : {&onKey, &aKey, &bKey}) k->T = tp;
    rangeSeg->T = tp; power.T = tp; help.T = tp;
    if (persist) v3::Prefs::setTheme("eq", m);
    if (v3::rk::fit(*this, n3, {kW3, kH3}, {kW, kH}, 0.5f, 2.f, scale_)) resized();
    sendLookAndFeelChange();
    refreshFields();
    repaint();
}

void EqEditor::setUiScale(float s) {
    const bool n3 = isV3();
    scale_ = juce::jlimit(n3 ? 0.75f : 0.5f, n3 ? 1.5f : 2.f, s);
    setSize(juce::roundToInt((n3 ? kW3 : kW) * scale_), juce::roundToInt((n3 ? kH3 : kH) * scale_));
    v3::Prefs::setScale("eq", scale_);
}

void EqEditor::select(int band) { sel = juce::jlimit(0, eq::kBands - 1, band); refreshFields(); repaint(); }

void EqEditor::refreshFields() {
    const auto P = proc.readParams(); const auto& b = P.b[sel];
    updating = true;
    if (boundBand != sel) { fFreq->bind(proc.par(sel, "f"), true); fQ->bind(proc.par(sel, "q"), true); boundBand = sel; }
    fGain->bind(proc.par(sel, "g"), eq::hasGain(b.type)); fFreq->refresh(); fQ->refresh();
    typeBox.setSelectedId(b.type + 1, juce::dontSendNotification);
    onKey.setToggleState(b.on, juce::dontSendNotification);
    updating = false;
}

void EqEditor::timerCallback() {
    refreshFields();
    aKey.setToggleState(proc.abIndex() == 0, juce::dontSendNotification); bKey.setToggleState(proc.abIndex() == 1, juce::dontSendNotification);
    modified = !proc.matchesPreset();
    if (isV3()) { repaint(Si(300, 0, 360, 76)); chips->repaint(); repaint(Si(20, 508, 840, 40)); return; }
    repaint(Si(370, 0, 260, 50)); chips->repaint();
}

void EqEditor::resized() {
    if (isV3()) resized3(); else resizedClassic();
    skin.btn.scale = scale_;
}

void EqEditor::resizedClassic() {
    u_ = (float)getWidth() / kW; scale_ = u_;
    const auto content = getLocalBounds().toFloat().reduced(18.f * u_, 0);
    power.setBounds(kit::w::inSlot(content, u_).toNearestInt());
    help.layout(kit::w::helpSlot(content, u_, true).toNearestInt(), getLocalBounds());
    graph->setBounds(Si(32, 64, 836, 290)); chips->setBounds(Si(32, 360, 836, 30));
    fFreq->setBounds(Si(32, 424, 128, 44)); fGain->setBounds(Si(168, 424, 116, 44)); fQ->setBounds(Si(292, 424, 100, 44));
    typeBox.setBounds(Si(404, 439, 130, 29)); onKey.setBounds(Si(544, 436, 112, 32));
    outPot->setBounds(Si(674, 416, 104, 70)); rangeSeg->setBounds(Si(786, 440, 82, 28));
    prev.setBounds(Si(352, 13, 20, 24)); next.setBounds(Si(628, 13, 20, 24));
    aKey.setBounds(Si(660, 9, 44, 32)); bKey.setBounds(Si(708, 9, 44, 32));
}

void EqEditor::mouseUp(const juce::MouseEvent& e) {
    if (presetArea().contains(e.position) && !e.mouseWasDraggedSinceMouseDown()) showPresetMenu();
}
juce::Rectangle<float> EqEditor::presetArea() const { return isV3() ? S(364, 30, 250, 40) : Rectangle<float>(378 * u_, 6 * u_, 244 * u_, 38 * u_); }
void EqEditor::showPresetMenu() {
    juce::PopupMenu m;
    for (int i = 0; i < eq::kPresets; ++i) m.addItem(i + 1, EqProcessor::presetName(i), true, i == proc.currentPreset());
    m.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(localAreaToGlobal(presetArea().toNearestInt())).withMinimumWidth(juce::roundToInt(presetArea().getWidth())), [this](int r) { if (r > 0) proc.applyPreset(r - 1); });
}

void EqEditor::paint(juce::Graphics& g) {
    if (isV3()) { paint3(g); return; }
    const float u = u_;
    kit::w::drawFace(g, pal, getLocalBounds(), u, {kit::w::Family::Rack, "Orbe EQ", kit::w::serial::eq});
    auto well = Rectangle<float>(378 * u, 6 * u, 244 * u, 38 * u);
    kit::screenWell(g, pal, well, kit::Display::VFD);
    const int pi = proc.currentPreset();
    g.setColour(pal.mute); g.setFont(kit::mono(7.6f * u));
    g.drawText(u8("PRESET ") + String(pi + 1).paddedLeft('0', 2) + "/" + String(eq::kPresets), Rectangle<float>(well.getX() + 10 * u, well.getY() + 4 * u, 100 * u, 10 * u), juce::Justification::centredLeft);
    g.setColour(modified ? kPeach : kLilac.withAlpha(0.8f));
    g.drawText(modified ? u8("\xE2\x97\x8F MODIFI\xC3\x89") : u8("\xE2\x97\x8F D'ORIGINE"), Rectangle<float>(well.getRight() - 110 * u, well.getY() + 4 * u, 100 * u, 10 * u), juce::Justification::centredRight);
    kit::screenText(g, pal, kit::Display::VFD, EqProcessor::presetName(pi), well.withTrimmedTop(13 * u).reduced(8 * u, 0), juce::Justification::centred, 12.f * u);
    section(g, pal, u, u8("Bande ") + String(sel + 1) + u8(" \xC2\xB7 s\xC3\xA9lection"), 32, 398, 624);
    section(g, pal, u, u8("Sortie \xC2\xB7 \xC3\xA9" "chelle"), 674, 398, 194);
    g.setColour(pal.mute); kit::tracked(g, "Type", Rectangle<float>(408 * u, 427 * u, 100 * u, 9 * u), juce::Justification::centredLeft, 1.3f * u, kit::fnt(8.f * u, true));
    kit::tracked(g, u8("\xC3\x89" "chelle"), Rectangle<float>(790 * u, 427 * u, 70 * u, 9 * u), juce::Justification::centredLeft, 1.3f * u, kit::fnt(8.f * u, true));
}

// ============================================================ v3 « hybride v2 » : coordonnées de conception 1100 x 700
void EqEditor::resized3() {
    u_ = (float)getWidth() / kW3; scale_ = u_;
    help.layout(Si(974, 34, 30, 30), getLocalBounds());
    skin.btn.setBounds(Si(940, 35, 28, 28));
    power.setBounds(Si(1010, 36, 70, 26));
    cPrev.setBounds(Si(326, 30, 34, 40)); cNext.setBounds(Si(618, 30, 34, 40));
    aKey.setBounds(Si(690, 32, 48, 36)); bKey.setBounds(Si(744, 32, 48, 36));
    graph->setBounds(Si(20, 84, 1060, 360)); chips->setBounds(Si(20, 452, 1060, 42));
    fFreq->setBounds(Si(44, 552, 176, 64)); fGain->setBounds(Si(230, 552, 150, 64)); fQ->setBounds(Si(390, 552, 130, 64));
    typeBox.setBounds(Si(552, 566, 170, 38)); onKey.setBounds(Si(738, 566, 112, 38));
    outK3->setScale(u_); outK3->setBounds(Si(886, 540, 100, v3::Knob3::boxHeight(v3::KnobSize::S, false)));
    rangeSeg->setBounds(Si(992, 586, 84, 34)); rangeSeg->px = 12.f * u_;
}

void EqEditor::paint3(juce::Graphics& g) {
    const float u = u_;
    const auto& T = skin.T;
    skin.art.draw(g, String("bd|") + v3::modeKey(skin.mode), getLocalBounds().toFloat(), [this](juce::Graphics& ig, Rectangle<float> b) {
        const float k = b.getWidth() / kW3; const auto& t = skin.T;
        auto R = [k](float x, float y, float w, float h) { return Rectangle<float>(x * k, y * k, w * k, h * k); };
        v3::background(ig, t, b);
        v3::rk::brand(ig, t, k, "Orbe", "EQ");
        v3::hero(ig, t, R(20, 84, 1060, 360), 22.f * k);
        v3::card(ig, t, R(20, 508, 1060, 172), 18.f * k);
        v3::rk::label(ig, t.inkMute, k, "Type", 554, 540, 100); v3::rk::label(ig, t.inkMute, k, u8("\xC3\x89" "chelle"), 994, 564, 80);
        v3::rk::label(ig, t.inkMute, k, u8("Sortie"), 888, 524, 100);
        v3::rk::sep(ig, t, k, 868, 524, 140);
    });
    const int pi = proc.currentPreset();
    auto well = S(364, 30, 250, 40);
    g.setColour(T.inkMute); v3::caps(g, u8("Preset ") + String(pi + 1).paddedLeft('0', 2) + "/" + String(eq::kPresets), S(366, 10, 220, 16), 11.f * u, juce::Justification::centredLeft);
    g.setColour(T.field); g.fillRoundedRectangle(well, 14 * u); g.setColour(T.fieldEdge); g.drawRoundedRectangle(well.reduced(0.5f), 14 * u, 1.f);
    g.setColour(T.ink); g.setFont(v3::font(18.f * u, true)); g.drawFittedText(EqProcessor::presetName(pi), well.reduced(36 * u, 0).toNearestInt(), juce::Justification::centred, 1, 0.8f);
    g.setColour(modified ? T.accent : T.track); g.fillEllipse(Rectangle<float>(9 * u, 9 * u).withCentre({well.getX() + 18 * u, well.getCentreY()}));
    if (modified) { g.setColour(T.accentText); v3::caps(g, u8("Modifi\xC3\xA9"), S(520, 10, 94, 16), 11.f * u, juce::Justification::centredRight); }
    g.setColour(T.inkMute); v3::caps(g, u8("M\xC3\xA9moire"), S(692, 10, 100, 16), 11.f * u, juce::Justification::centredLeft);
    g.setColour(T.inkMute); v3::caps(g, u8("Bande ") + String(sel + 1) + u8(" \xC2\xB7 s\xC3\xA9lection"), S(44, 524, 400, 16), 11.f * u, juce::Justification::centredLeft);
}
