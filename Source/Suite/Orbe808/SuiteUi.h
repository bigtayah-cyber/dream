// Éléments d'interface communs à Orbe 808 et Orbe Beat (en plus de Kit.h) : sélecteur segmenté, poignée de glisser-déposer, logo, voyants.
#pragma once
#include <functional>
#include <juce_audio_utils/juce_audio_utils.h>
#include "../../Kit/Kit.h"

namespace suiteui {

inline const juce::Colour kLilac(0xFFCFC6FF), kIce(0xFFA9ECFF), kPeach(0xFFFFC4AB);

// Notes en lettres (C D E F G A B), octave MIDI standard (60 = C4).
inline juce::String noteName(int n) {
    static const char* k[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    return juce::String(k[((n % 12) + 12) % 12]) + juce::String(n / 12 - 1);
}
inline juce::String fr1(float v) { return juce::String(v, 1).replace(".", ","); }

// Sélecteur segmenté (grille de cols colonnes) relié à un paramètre « choix ».
class Segmented : public juce::Component, public juce::SettableTooltipClient {
public:
    Segmented(juce::RangedAudioParameter& p, juce::StringArray labels, const kit::Palette& pal, int cols = 0)
        : labels_(std::move(labels)), P(pal), cols_(cols > 0 ? cols : labels_.size()),
          att(p, [this](float v) { idx = juce::roundToInt(v); repaint(); }) { att.sendInitialUpdate(); }
    void paint(juce::Graphics& g) override {
        auto r = getLocalBounds().toFloat().reduced(0.5f);
        g.setColour(juce::Colours::black.withAlpha(0.6f)); g.fillRoundedRectangle(r, 4.f);
        g.setColour(juce::Colours::white.withAlpha(0.10f)); g.drawRoundedRectangle(r, 4.f, 1.f);
        const int n = labels_.size(), rows = (n + cols_ - 1) / cols_;
        const float w = r.getWidth() / (float)cols_, h = r.getHeight() / (float)rows;
        for (int i = 0; i < n; ++i) {
            const int span = i == n - 1 ? cols_ - i % cols_ : 1;  // la dernière case occupe la fin de la ligne
            auto c = juce::Rectangle<float>(r.getX() + w * (float)(i % cols_), r.getY() + h * (float)(i / cols_), w * (float)span, h).reduced(2.f);
            const bool on = i == idx;
            if (on) {
                g.setGradientFill(juce::ColourGradient(P.accent.withAlpha(0.55f), c.getX(), c.getY(), P.accent.withAlpha(0.22f), c.getX(), c.getBottom(), false));
                g.fillRoundedRectangle(c, 3.f);
                g.setColour(P.accent.withAlpha(0.9f)); g.drawRoundedRectangle(c, 3.f, 1.f);
            }
            g.setColour(on ? juce::Colours::white : P.ink.withAlpha(0.6f));
            kit::tracked(g, labels_[i], c, juce::Justification::centred, 1.1f, kit::fnt(juce::jmin(10.5f, h * 0.38f), true));
        }
    }
    void mouseDown(const juce::MouseEvent& e) override {
        const int n = labels_.size(), rows = (n + cols_ - 1) / cols_;
        const int cx = juce::jlimit(0, cols_ - 1, (int)(e.position.x / ((float)getWidth() / (float)cols_)));
        const int cy = juce::jlimit(0, rows - 1, (int)(e.position.y / ((float)getHeight() / (float)rows)));
        att.setValueAsCompleteGesture((float)juce::jmin(n - 1, cy * cols_ + cx));
    }
private:
    juce::StringArray labels_;
    kit::Palette P;
    int cols_, idx = 0;
    juce::ParameterAttachment att;
};

// Poignée : on la glisse vers une piste du DAW pour y déposer un fichier .mid.
class DragHandle : public juce::Component, public juce::SettableTooltipClient {
public:
    DragHandle(std::function<juce::File()> make, const kit::Palette& pal, juce::String title, juce::String sub)
        : mk(std::move(make)), P(pal), t1(std::move(title)), t2(std::move(sub)) { setMouseCursor(juce::MouseCursor::DraggingHandCursor); }
    void paint(juce::Graphics& g) override {
        auto r = getLocalBounds().toFloat().reduced(1.f);
        const float u = (float)getWidth() / 216.f;
        g.setColour(juce::Colours::black.withAlpha(0.35f)); g.fillRoundedRectangle(r, 8.f);
        const float dash[2] = {5.f, 4.f};
        juce::Path pth; pth.addRoundedRectangle(r.reduced(1.f), 8.f);
        juce::Path dashed; juce::PathStrokeType(1.2f).createDashedStroke(dashed, pth, dash, 2);
        g.setColour(dragging ? P.accent : P.ink.withAlpha(0.35f)); g.fillPath(dashed);
        auto ic = juce::Rectangle<float>(r.getX() + 18 * u, r.getCentreY() - 22 * u, 34 * u, 44 * u);
        juce::Path doc;
        doc.startNewSubPath(ic.getX(), ic.getY()); doc.lineTo(ic.getRight() - 10 * u, ic.getY()); doc.lineTo(ic.getRight(), ic.getY() + 10 * u);
        doc.lineTo(ic.getRight(), ic.getBottom()); doc.lineTo(ic.getX(), ic.getBottom()); doc.closeSubPath();
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xFF4A4A52), ic.getX(), ic.getY(), juce::Colour(0xFF202024), ic.getX(), ic.getBottom(), false));
        g.fillPath(doc);
        g.setColour(juce::Colours::white.withAlpha(0.22f)); g.strokePath(doc, juce::PathStrokeType(1.f));
        g.setColour(P.accent); g.setFont(kit::mono(11.f * u));
        g.drawText(".MID", ic.withTop(ic.getCentreY() - 6 * u).withHeight(12 * u), juce::Justification::centred);
        g.setColour(P.ink.withAlpha(0.9f));
        kit::tracked(g, t1, juce::Rectangle<float>(r.getX() + 62 * u, r.getCentreY() - 16 * u, r.getWidth() - 66 * u, 16 * u), juce::Justification::centredLeft, 1.1f, kit::fnt(11.f * u, true));
        g.setColour(P.mute); g.setFont(kit::fnt(10.5f * u));
        g.drawFittedText(t2, juce::Rectangle<float>(r.getX() + 62 * u, r.getCentreY(), r.getWidth() - 66 * u, 28 * u).toNearestInt(), juce::Justification::topLeft, 2);
    }
    void mouseDrag(const juce::MouseEvent& e) override {
        if (dragging || e.getDistanceFromDragStart() < 6) return;
        dragging = true; repaint();
        auto f = mk();
        if (auto* c = juce::DragAndDropContainer::findParentDragContainerFor(this))
            c->performExternalDragDropOfFiles({f.getFullPathName()}, false, this, [this] { dragging = false; repaint(); });
        else { dragging = false; repaint(); }
    }
    void mouseUp(const juce::MouseEvent&) override { dragging = false; repaint(); }
private:
    std::function<juce::File()> mk;
    kit::Palette P;
    juce::String t1, t2;
    bool dragging = false;
};

// Signature de la marque : sphère éclairée entourée d'un anneau.
inline void logo(juce::Graphics& g, float cx, float cy, float r, float u, juce::Colour core, juce::Colour deep) {
    g.setColour(juce::Colours::black.withAlpha(0.5f)); g.fillEllipse(cx - r - 3 * u, cy - r - 1 * u, (r + 3 * u) * 2, (r + 3 * u) * 2);
    g.setGradientFill(juce::ColourGradient(core, cx - r * 0.5f, cy - r * 0.6f, deep, cx + r, cy + r, true));
    g.fillEllipse(cx - r, cy - r, r * 2, r * 2);
    g.setColour(juce::Colours::white.withAlpha(0.5f)); g.fillEllipse(cx - r * 0.62f, cy - r * 0.78f, r * 0.7f, r * 0.42f);
    g.setColour(kLilac.withAlpha(0.8f)); g.drawEllipse(cx - r * 1.28f, cy - r * 1.28f, r * 2.56f, r * 2.56f, 1.f * u);
}

// Filet gravé horizontal (ombre + reflet).
inline void rule(juce::Graphics& g, float u, float y, float w) {
    g.setColour(juce::Colours::black.withAlpha(0.5f)); g.fillRect(0.f, y * u, w * u, 1.5f);
    g.setColour(juce::Colours::white.withAlpha(0.06f)); g.fillRect(0.f, y * u + 1.5f, w * u, 1.f);
}

// Vumètre à voyants : level 0..1.
inline void ledBar(juce::Graphics& g, juce::Rectangle<float> r, float level, int n, juce::Colour c) {
    const float w = r.getWidth() / (float)n;
    for (int i = 0; i < n; ++i) {
        const bool on = (float)(i + 1) / (float)n <= level + 0.001f;
        const auto col = i >= n - 2 ? juce::Colour(0xFFFF6B6B) : c;
        auto s = juce::Rectangle<float>(r.getX() + w * (float)i + 1.f, r.getY(), w - 2.f, r.getHeight());
        g.setColour(on ? col.withAlpha(0.95f) : juce::Colours::white.withAlpha(0.07f));
        g.fillRoundedRectangle(s, 1.5f);
    }
}

}  // namespace suiteui
