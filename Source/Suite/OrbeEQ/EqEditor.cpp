#include "EqEditor.h"

using namespace of;
using kit::u8;
using juce::Point;

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

    void paint(juce::Graphics& g) override {
        const float u = scale(); const auto pal = of::palette();
        kit::screenWell(g, pal, getLocalBounds().toFloat());
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
            g.setFont(kit::mono(9.f * u)); const float w = g.getCurrentFont().getStringWidthFloat(t) + 14 * u;
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
    void paint(juce::Graphics& g) override {
        const float u = (float)getWidth() / 836.f, w = (float)getWidth() / 8.f; const auto P = proc.readParams(); const auto pal = of::palette();
        for (int i = 0; i < eq::kBands; ++i) {
            auto c = Rectangle<float>(w * (float)i, 0, w, (float)getHeight()).reduced(2 * u, 1);
            const bool s = i == ed.selected(), on = P.b[i].on;
            g.setColour(juce::Colours::black.withAlpha(s ? 0.65f : 0.4f)); g.fillRoundedRectangle(c, 4 * u);
            g.setColour(s ? EqEditor::bandColour(i).withAlpha(0.7f) : juce::Colours::white.withAlpha(0.08f)); g.drawRoundedRectangle(c, 4 * u, s ? 1.4f : 1.f);
            kit::led(g, c.getX() + 11 * u, c.getCentreY(), 3.4f * u, on, EqEditor::bandColour(i));
            g.setColour(on ? pal.ink : pal.mute); g.setFont(kit::fnt(9.4f * u, true));
            g.drawText(String(i + 1), Rectangle<float>(c.getX() + 20 * u, c.getY(), 12 * u, c.getHeight()), juce::Justification::centred);
            g.setFont(kit::mono(9.f * u)); g.setColour(on ? pal.screenInk : pal.mute);
            g.drawText(fmtHz(P.b[i].freq), Rectangle<float>(c.getX() + 34 * u, c.getY(), c.getWidth() - 36 * u, c.getHeight()), juce::Justification::centredLeft);
        }
    }
    void mouseDown(const juce::MouseEvent& e) override {
        const float w = (float)getWidth() / 8.f; const int i = juce::jlimit(0, 7, (int)(e.position.x / w));
        if (e.position.x - w * (float)i < 20.f * (float)getWidth() / 836.f) { auto* o = proc.par(i, "on"); proc.setReal(o, o->getValue() > 0.5f ? 0.f : 1.f); }
        ed.select(i);
    }
private:
    EqProcessor& proc; EqEditor& ed;
};

// ------------------------------------------------------------------ champ numérique (glisser, molette, double-clic pour saisir)
class EqField : public juce::Component, public juce::SettableTooltipClient {
public:
    EqField(const String& cap, const String& tip) : caption(cap) {
        setTooltip(tip); setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
        ed.setJustificationType(juce::Justification::centred); ed.setInterceptsMouseClicks(false, true);
        ed.setColour(juce::Label::textColourId, kIce); ed.setColour(juce::Label::textWhenEditingColourId, juce::Colours::white);
        ed.setColour(juce::TextEditor::backgroundColourId, Colour(0xFF050508)); ed.setColour(juce::TextEditor::outlineColourId, kLilac);
        ed.setColour(juce::TextEditor::textColourId, juce::Colours::white); ed.setColour(juce::CaretComponent::caretColourId, kLilac);
        ed.onTextChange = [this] { if (par) { par->beginChangeGesture(); par->setValueNotifyingHost(par->getValueForText(ed.getText())); par->endChangeGesture(); } refresh(); };
        addAndMakeVisible(ed);
    }
    void bind(juce::RangedAudioParameter* p, bool en) { par = p; enabled = en; setInterceptsMouseClicks(en, true); refresh(); }
    void refresh() { if (par && !ed.isBeingEdited()) ed.setText(enabled ? par->getText(par->getValue(), 0) : juce::String::fromUTF8("\xE2\x80\x94"), juce::dontSendNotification); ed.setAlpha(enabled ? 1.f : 0.4f); repaint(); }
    void resized() override { const float u = (float)getHeight() / 44.f; ed.setFont(kit::mono(13.f * u)); ed.setBounds(getLocalBounds().withTrimmedTop(int(14 * u)).reduced(3)); }
    void paint(juce::Graphics& g) override {
        const float u = (float)getHeight() / 44.f; auto r = getLocalBounds().toFloat();
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
    outPot = std::make_unique<Pot>(proc.apvts, "out", "Sortie", u8("Gain de sortie apr\xC3\xA8s l'\xC3\xA9galiseur.")); addAndMakeVisible(*outPot);
    rangeSeg = std::make_unique<Seg>(proc.apvts, "range", juce::StringArray{u8("\xC2\xB1" "12"), u8("\xC2\xB1" "24")}, u8("\xC3\x89" "chelle verticale du graphe en dB."), pal); addAndMakeVisible(*rangeSeg);
    for (auto* k : {&aKey, &bKey}) { k->setClickingTogglesState(false); addAndMakeVisible(*k); }
    aKey.onClick = [this] { proc.switchAB(0); }; bKey.onClick = [this] { proc.switchAB(1); };
    aKey.setTooltip(u8("M\xC3\xA9moire A : compare deux r\xC3\xA9glages complets.")); bKey.setTooltip(u8("M\xC3\xA9moire B : compare deux r\xC3\xA9glages complets."));
    prev.onClick = [this] { proc.applyPreset((proc.currentPreset() + eq::kPresets - 1) % eq::kPresets); };
    next.onClick = [this] { proc.applyPreset((proc.currentPreset() + 1) % eq::kPresets); };
    prev.setTooltip(u8("Preset pr\xC3\xA9" "c\xC3\xA9" "dent")); next.setTooltip("Preset suivant");
    addAndMakeVisible(prev); addAndMakeVisible(next);
    addAndMakeVisible(power);
    power.setTooltip(u8("\xC3\x89galiseur en service. Appuie pour le couper (bypass)."));
    powerAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, "bypass", power);
    setResizable(true, true);
    setResizeLimits(450, 274, 1800, 1096);
    getConstrainer()->setFixedAspectRatio((double)kW / kH);
    setSize((int)kW, (int)kH);
    refreshFields();
    startTimerHz(20);
}
EqEditor::~EqEditor() { stopTimer(); setLookAndFeel(nullptr); }

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
    repaint(Si(430, 20, 300, 48)); chips->repaint();
}

void EqEditor::resized() {
    u_ = (float)getWidth() / kW;
    plate = kit::brushedPlate(getWidth(), getHeight(), pal);
    graph->setBounds(Si(32, 104, 836, 290)); chips->setBounds(Si(32, 402, 836, 28));
    fFreq->setBounds(Si(32, 466, 128, 44)); fGain->setBounds(Si(168, 466, 116, 44)); fQ->setBounds(Si(292, 466, 100, 44));
    typeBox.setBounds(Si(404, 481, 130, 29)); onKey.setBounds(Si(544, 478, 112, 32));
    outPot->setBounds(Si(674, 458, 100, 70)); rangeSeg->setBounds(Si(786, 484, 82, 26));
    prev.setBounds(Si(406, 33, 20, 24)); next.setBounds(Si(698, 33, 20, 24));
    aKey.setBounds(Si(734, 30, 40, 28)); bKey.setBounds(Si(778, 30, 40, 28));
    power.setBounds(Si(826, 20, 58, 52));
}

void EqEditor::mouseUp(const juce::MouseEvent& e) {
    if (Rectangle<float>(436 * u_, 26 * u_, 256 * u_, 40 * u_).contains(e.position) && !e.mouseWasDraggedSinceMouseDown()) showPresetMenu();
}
void EqEditor::showPresetMenu() {
    juce::PopupMenu m;
    for (int i = 0; i < eq::kPresets; ++i) m.addItem(i + 1, EqProcessor::presetName(i), true, i == proc.currentPreset());
    m.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(localAreaToGlobal(Si(436, 26, 256, 40))).withMinimumWidth(juce::roundToInt(256 * u_)), [this](int r) { if (r > 0) proc.applyPreset(r - 1); });
}

void EqEditor::paint(juce::Graphics& g) {
    const float u = u_;
    drawFace(g, pal, plate, u, kW, kH, "Orbe EQ", u8("\xC3\x89galiseur param\xC3\xA9trique 8 bandes \xC2\xB7 phase minimale"), "DW-702");
    auto well = Rectangle<float>(436 * u, 26 * u, 256 * u, 40 * u);
    kit::screenWell(g, pal, well);
    const int pi = proc.currentPreset();
    g.setColour(pal.mute); g.setFont(kit::mono(7.6f * u));
    g.drawText(u8("PRESET ") + String(pi + 1).paddedLeft('0', 2) + "/" + String(eq::kPresets), Rectangle<float>(well.getX() + 10 * u, well.getY() + 4 * u, 100 * u, 10 * u), juce::Justification::centredLeft);
    g.setColour(modified ? kPeach : kLilac.withAlpha(0.8f));
    g.drawText(modified ? u8("\xE2\x97\x8F MODIFI\xC3\x89") : u8("\xE2\x97\x8F D'ORIGINE"), Rectangle<float>(well.getRight() - 110 * u, well.getY() + 4 * u, 100 * u, 10 * u), juce::Justification::centredRight);
    g.setColour(pal.screenInk); g.setFont(kit::mono(14.f * u));
    g.drawText(kit::upper(EqProcessor::presetName(pi)), well.withTrimmedTop(13 * u).reduced(8 * u, 0), juce::Justification::centred);
    section(g, pal, u, u8("Bande ") + String(sel + 1) + u8(" \xC2\xB7 s\xC3\xA9lection"), 32, 440, 624);
    section(g, pal, u, u8("Sortie \xC2\xB7 \xC3\xA9" "chelle"), 674, 440, 194);
    g.setColour(pal.mute); kit::tracked(g, "Type", Rectangle<float>(408 * u, 470 * u, 100 * u, 9 * u), juce::Justification::centredLeft, 1.3f * u, kit::fnt(7.4f * u, true));
    kit::tracked(g, u8("\xC3\x89" "chelle"), Rectangle<float>(790 * u, 470 * u, 70 * u, 9 * u), juce::Justification::centredLeft, 1.3f * u, kit::fnt(7.4f * u, true));
    g.setFont(kit::mono(8.f * u));
    g.drawText(u8("Glisser : fr\xC3\xA9quence / gain  \xC2\xB7  Molette : Q  \xC2\xB7  Double-clic : r\xC3\xA9initialiser  \xC2\xB7  Clic droit : type de bande  \xC2\xB7  Maj : fin"),
               Rectangle<float>(32 * u, 522 * u, 836 * u, 12 * u), juce::Justification::centredLeft);
}
