// Kit v3 : socle commun des éditeurs d'instruments (Orbe, Synth, 808, Beat, Pads, Sample, Fredonne).
//  - Skin : thème (Clair / Sombre / Classique), Look3, cache d'images, sélecteur de thème, échelle mémorisée ;
//  - brand(), label() : en-tête et libellés v3 ; GridSeg : sélecteur en grille de pilules ; DragHandle3 : poignée MIDI ;
//  - meterBar() : barre de niveau fine. Additif : ne modifie rien d'existant.
#pragma once
#include "Widgets3.h"

namespace kit::v3 {

struct Skin {
    Skin(const juce::String& k, juce::Colour acc) : key(k), accent(acc) { T = make(Mode::Light, acc); }
    juce::String key; juce::Colour accent;
    Mode mode = Mode::Light;
    Theme T;
    Look3 look3{T};
    ArtCache art;
    ThemeMenu themeBtn;
    float scale = 1.f;
    std::function<void()> onMode;   // l'éditeur bascule ses composants (potards, ponts) ici

    bool on() const { return mode != Mode::Classic; }
    const Theme* tp() const { return on() ? &T : nullptr; }

    // À appeler en fin de constructeur : lit les préférences, installe le Look, dimensionne.
    void init(juce::AudioProcessorEditor& e, juce::LookAndFeel& classicLook, float w, float h) {
        ed = &e; classic = &classicLook; bw = w; bh = h;
        themeBtn.onTheme = [this](Mode m) { setMode(m, true); };
        themeBtn.onScale = [this](float s) { setScale(s); };
        scale = Prefs::scale(key, 1.f);
        setMode(Prefs::theme(key, Mode::Light), false);
    }
    void setMode(Mode m, bool persist) {
        mode = m;
        T = make(m == Mode::Classic ? Mode::Light : m, accent); T.mode = m;
        look3.setTheme(T); art.clear();
        ed->setLookAndFeel(on() ? static_cast<juce::LookAndFeel*>(&look3) : classic);
        themeBtn.T = &T; themeBtn.current = m;
        if (persist) Prefs::setTheme(key, m);
        if (onMode) onMode();
        applySize();
        ed->sendLookAndFeelChange();
        ed->repaint();
    }
    void setScale(float s) { scale = juce::jlimit(0.75f, 1.5f, s); Prefs::setScale(key, scale); applySize(); }
    void shutdown() { Prefs::setScale(key, scale); if (ed != nullptr) ed->setLookAndFeel(nullptr); }

private:
    void applySize() {
        const float s = juce::jlimit(0.75f, 1.5f, scale);
        const int w = juce::roundToInt(bw * s), h = juce::roundToInt(bh * s);
        if (ed->getWidth() != w || ed->getHeight() != h) ed->setSize(w, h);
        else ed->resized();
    }
    juce::AudioProcessorEditor* ed = nullptr; juce::LookAndFeel* classic = nullptr; float bw = 1000.f, bh = 600.f;
};

// Outils de capture : DSG_THEME=clair|sombre|classique impose le thème avant la création de l'éditeur.
inline void snapshotTheme(const char* key) {
    const char* e = std::getenv("DSG_THEME");
    if (e == nullptr) return;
    for (auto m : {Mode::Light, Mode::Dark, Mode::Classic}) if (juce::String(e) == modeKey(m)) Prefs::setTheme(key, m);
}

// En-tête : « dreamaws », nom en grand, mot d'accent, filet d'accent. Coordonnées de conception, échelle u.
inline void brand(Graphics& g, const Theme& T, float u, const String& name, const String& variant, float x = 28.f, float y = 12.f) {
    g.setColour(T.inkMute); caps(g, "dreamaws", Rectangle<float>(x * u, y * u, 160.f * u, 16.f * u), 12.f * u, juce::Justification::centredLeft);
    const auto big = font(30.f * u, true);
    g.setColour(T.ink); tracked(g, name, Rectangle<float>(x * u, (y + 16.f) * u, 200.f * u, 34.f * u), juce::Justification::centredLeft, 2.6f * u, big);
    if (variant.isNotEmpty()) {
        const float ow = tabularWidth(name.toUpperCase(), big) + (float)(name.length() - 1) * 2.6f * u + 14.f * u;
        g.setColour(T.accentText); tracked(g, variant, Rectangle<float>(x * u + ow, (y + 16.f) * u, 260.f * u, 34.f * u), juce::Justification::centredLeft, 2.6f * u, big);
    }
    g.setColour(T.accent); g.fillRoundedRectangle(Rectangle<float>((x + 2.f) * u, (y + 52.f) * u, 30.f * u, 3.f * u), 1.5f * u);
}
inline void label(Graphics& g, const Theme& T, const String& s, Rectangle<float> r, float px, juce::Justification j = juce::Justification::centredLeft) {
    g.setColour(T.inkMute); caps(g, s, r, px, j);
}
// Barre de niveau fine (fraction 0..1), piste + remplissage d'accent.
inline void meterBar(Graphics& g, const Theme& T, Rectangle<float> r, float v, juce::Colour c) {
    g.setColour(T.track); g.fillRoundedRectangle(r, r.getHeight() * 0.5f);
    v = juce::jlimit(0.f, 1.f, v);
    if (v > 0.01f) { g.setColour(c); g.fillRoundedRectangle(r.withWidth(juce::jmax(r.getHeight(), r.getWidth() * v)), r.getHeight() * 0.5f); }
}

// Sélecteur en grille de pilules (n lignes / colonnes ; icônes d'onde facultatives) : dessin partagé.
inline void segGrid(Graphics& g, const Theme& T, Rectangle<float> r, const juce::StringArray& labs, int sel_, int hover, int cols_, bool waveIcons, float px) {
    const int n = labs.size(), rows = (n + cols_ - 1) / cols_;
    if (rows == 1 && !waveIcons) { segments(g, T, r, labs, sel_, hover, px); return; }
    const float w = r.getWidth() / (float)cols_, h = r.getHeight() / (float)rows;
    for (int i = 0; i < n; ++i) {
        const int span = (i == n - 1) ? cols_ - i % cols_ : 1;
        auto c = Rectangle<float>(r.getX() + w * (float)(i % cols_), r.getY() + h * (float)(i / cols_), w * (float)span, h).reduced(2.f);
        const bool sel = i == sel_;
        const float rad = juce::jmin(c.getHeight() * 0.5f, 14.f);
        if (sel) juce::DropShadow(T.shadow, juce::roundToInt(rad * 0.5f), {0, 1}).drawForRectangle(g, c.toNearestInt());
        g.setColour(sel ? (T.dark ? T.card.brighter(0.25f) : T.card) : (i == hover ? T.field.brighter(0.04f) : T.field)); g.fillRoundedRectangle(c, rad);
        g.setColour(sel ? T.accent : T.fieldEdge); g.drawRoundedRectangle(c.reduced(0.5f), rad, sel ? 1.5f : 1.f);
        if (waveIcons) {
            juce::Path p; const auto a = Rectangle<float>(c.getWidth() * 0.5f, c.getHeight() * 0.38f).withCentre(c.getCentre());
            auto pt = [&](float t, float v) { return juce::Point<float>(a.getX() + a.getWidth() * t, a.getCentreY() - a.getHeight() * 0.5f * v); };
            auto poly = [&](std::initializer_list<std::pair<float, float>> q) { bool f = true; for (auto& s : q) { if (f) p.startNewSubPath(pt(s.first, s.second)); else p.lineTo(pt(s.first, s.second)); f = false; } };
            if (i == 0) { for (int k = 0; k <= 48; ++k) { const auto q = pt((float)k / 48.f, std::sin(juce::MathConstants<float>::twoPi * 2.f * (float)k / 48.f)); if (k) p.lineTo(q); else p.startNewSubPath(q); } }
            else if (i == 1) poly({{0, 0}, {.125f, 1}, {.375f, -1}, {.625f, 1}, {.875f, -1}, {1, 0}});
            else if (i == 2) poly({{0, -1}, {.5f, 1}, {.5f, -1}, {1, 1}});
            else poly({{0, 1}, {.25f, 1}, {.25f, -1}, {.5f, -1}, {.5f, 1}, {.75f, 1}, {.75f, -1}, {1, -1}});
            g.setColour(sel ? T.accent : T.inkMute);
            g.strokePath(p, juce::PathStrokeType(juce::jmax(1.4f, px * 0.14f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        } else { g.setColour(sel ? T.ink : T.inkMute); caps(g, labs[i], c, px, juce::Justification::centred, sel); }
    }
}

class GridSeg : public kit::w::Seg {
public:
    GridSeg(juce::RangedAudioParameter& p, juce::StringArray names, const kit::Palette& pal, int cols = 0, bool wave = false)
        : kit::w::Seg(p, names, pal, cols), labs(names), cols_(cols > 0 ? cols : names.size()), waveIcons(wave) {}
    const Theme* T = nullptr;
    float px = 12.f;
    void paint(Graphics& g) override {
        if (!bridge::v3(T)) { kit::w::Seg::paint(g); return; }
        segGrid(g, *T, getLocalBounds().toFloat().reduced(1.f), labs, selected(), hover, cols_, waveIcons, px);
    }
    void mouseMove(const juce::MouseEvent& e) override {
        const int rows = (labs.size() + cols_ - 1) / cols_;
        const int cx = juce::jlimit(0, cols_ - 1, (int)(e.position.x / ((float)getWidth() / (float)cols_))), cy = juce::jlimit(0, rows - 1, (int)(e.position.y / ((float)getHeight() / (float)rows)));
        const int h = juce::jmin(labs.size() - 1, cy * cols_ + cx);
        if (h != hover) { hover = h; repaint(); }
    }
    void mouseExit(const juce::MouseEvent&) override { hover = -1; repaint(); }
private:
    juce::StringArray labs; int cols_; bool waveIcons; int hover = -1;
};

// Poignée de glisser-déposer MIDI en v3 (carte à liseré pointillé) ; Classique : rendu d'origine.
class DragHandle3 : public kit::w::DragHandle {
public:
    using kit::w::DragHandle::DragHandle;
    const Theme* T = nullptr;
    juce::String title, sub;
    void paint(Graphics& g) override {
        if (!bridge::v3(T)) { kit::w::DragHandle::paint(g); return; }
        auto r = getLocalBounds().toFloat().reduced(1.f);
        const float u = juce::jmin((float)getWidth() / 216.f, (float)getHeight() / 64.f), rad = 12.f * u;
        g.setColour(hot ? T->field.brighter(0.04f) : T->field); g.fillRoundedRectangle(r, rad);
        juce::Path pth; pth.addRoundedRectangle(r.reduced(1.f), rad);
        const float dash[2] = {5.f * u, 4.f * u}; juce::Path d; juce::PathStrokeType(1.2f).createDashedStroke(d, pth, dash, 2);
        g.setColour(hot ? T->accent : T->tick); g.fillPath(d);
        auto ic = Rectangle<float>(r.getX() + 14 * u, r.getCentreY() - 17 * u, 26 * u, 34 * u);
        juce::Path doc; doc.startNewSubPath(ic.getX(), ic.getY()); doc.lineTo(ic.getRight() - 8 * u, ic.getY()); doc.lineTo(ic.getRight(), ic.getY() + 8 * u); doc.lineTo(ic.getRight(), ic.getBottom()); doc.lineTo(ic.getX(), ic.getBottom()); doc.closeSubPath();
        g.setColour(T->card); g.fillPath(doc); g.setColour(T->inkMute); g.strokePath(doc, juce::PathStrokeType(1.f));
        g.setColour(T->accentText); g.setFont(font(9.5f * u, true)); g.drawText("MID", ic.withTop(ic.getCentreY() - 6 * u).withHeight(12 * u), juce::Justification::centred, false);
        g.setColour(T->ink); caps(g, title, Rectangle<float>(r.getX() + 50 * u, r.getCentreY() - 17 * u, r.getWidth() - 54 * u, 16 * u), 10.5f * u, juce::Justification::centredLeft);
        g.setColour(T->inkMute); g.setFont(font(11.f * u)); g.drawFittedText(sub, Rectangle<float>(r.getX() + 50 * u, r.getCentreY() + 1 * u, r.getWidth() - 54 * u, 26 * u).toNearestInt(), juce::Justification::topLeft, 2);
    }
    void mouseEnter(const juce::MouseEvent& e) override { hot = true; repaint(); kit::w::DragHandle::mouseEnter(e); }
    void mouseExit(const juce::MouseEvent& e) override { hot = false; repaint(); kit::w::DragHandle::mouseExit(e); }
private:
    bool hot = false;
};

}  // namespace kit::v3
