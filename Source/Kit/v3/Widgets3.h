// Kit v3 « hybride v2 » : composants et primitives de dessin.
//  - Look3 : listes, menus, boutons, bascules, curseurs, info-bulles aux couleurs du thème (hérite de kit::Look) ;
//  - Knob3 : potard haut de gamme (capuchon satiné en cache ×2, arc de valeur fin lumineux, arc de modulation,
//            point incrusté, 3 tailles S/M/L, survol, glissement, désactivé, double-clic, molette, Maj = fin, saisie) ;
//  - Trace : courbe lissée (Catmull-Rom + lissage aller-retour), glow doux, remplissage dégradé, tête lumineuse ;
//  - card(), hero(), gauge(), vu(), segments(), InSwitch, ThemeMenu, Chevron, Backdrop (cache des fonds).
// Additif : n'altère ni Kit.h ni Widgets.h. Aucune allocation par image une fois les caches remplis.
#pragma once
#include <functional>
#include <map>
#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"
#include "../Widgets.h"   // lecture seule : les ponts ci-dessous dérivent des composants classiques

namespace kit::v3 {
using juce::Graphics, juce::Rectangle, juce::String, juce::Image;

inline float physScale(Graphics& g) { return juce::jlimit(1.f, 4.f, g.getInternalContext().getPhysicalPixelScaleFactor()); }
inline constexpr float kA0 = juce::MathConstants<float>::pi * 1.25f, kA1 = juce::MathConstants<float>::pi * 2.75f;   // 7 h 30 -> 4 h 30, sens horaire
inline float angleOf(float t) { return kA0 + (kA1 - kA0) * juce::jlimit(0.f, 1.f, t); }
inline juce::Point<float> polar(float cx, float cy, float r, float a) { return {cx + r * std::sin(a), cy - r * std::cos(a)}; }

// ------------------------------------------------------------ cache d'images (fonds, capuchons) à l'échelle physique
// Clé libre (chaîne courte) ; l'image est rendue une fois à la résolution de l'écran (×2 Retina), puis posée sans
// rééchantillonnage (translation entière en pixels physiques) : aucune dentelure, aucun flou.
class ArtCache {
public:
    template <class F>
    void draw(Graphics& g, const String& key, Rectangle<float> dest, F&& render) {
        const float s = physScale(g);
        const int w = juce::jmax(1, juce::roundToInt(dest.getWidth() * s)), h = juce::jmax(1, juce::roundToInt(dest.getHeight() * s));
        const String k = key + "|" + String(w) + "x" + String(h);
        auto it = map.find(k);
        if (it == map.end()) {
            if (map.size() > 96) map.clear();
            Image img(Image::ARGB, w, h, true);
            { Graphics ig(img); ig.addTransform(juce::AffineTransform::scale((float)w / dest.getWidth(), (float)h / dest.getHeight())); render(ig, Rectangle<float>(dest.getWidth(), dest.getHeight())); }
            it = map.emplace(k, img).first;
        }
        const float x = std::round(dest.getX() * s), y = std::round(dest.getY() * s);
        Graphics::ScopedSaveState st(g);
        g.addTransform(juce::AffineTransform::scale(1.f / s));
        g.drawImageAt(it->second, (int)x, (int)y);
    }
    void clear() { map.clear(); }
private:
    std::map<String, Image> map;
};

// ------------------------------------------------------------ surfaces
inline void background(Graphics& g, const Theme& T, Rectangle<float> r) {
    g.setGradientFill(juce::ColourGradient(T.bgTop, r.getX(), r.getY(), T.bgBot, r.getX(), r.getBottom(), false));
    g.fillRect(r);
}
// Carte : ombre portée douce (deux couches), fond, liseré. À rendre dans un cache (ArtCache / Backdrop).
inline void card(Graphics& g, const Theme& T, Rectangle<float> r, float rad) {
    juce::Path p; p.addRoundedRectangle(r, rad);
    juce::DropShadow(T.shadow.withMultipliedAlpha(0.55f), juce::roundToInt(rad * 1.6f), {0, juce::roundToInt(rad * 0.35f)}).drawForPath(g, p);
    juce::DropShadow(T.shadow.withMultipliedAlpha(0.6f), juce::roundToInt(rad * 0.35f), {0, 1}).drawForPath(g, p);
    g.setColour(T.card); g.fillPath(p);
    if (T.dark) { g.setGradientFill(juce::ColourGradient(juce::Colours::white.withAlpha(0.035f), r.getX(), r.getY(), juce::Colours::transparentWhite, r.getX(), r.getY() + rad * 4.f, false)); g.fillPath(p); }
    g.setColour(T.cardEdge); g.strokePath(p, juce::PathStrokeType(1.f));
}
// Écran héros : dégradé bleu nuit, halo violet, chaleur d'accent en haut, ombre portée (rendu en cache).
inline void hero(Graphics& g, const Theme& T, Rectangle<float> r, float rad) {
    juce::Path p; p.addRoundedRectangle(r, rad);
    juce::DropShadow(juce::Colours::black.withAlpha(T.dark ? 0.55f : 0.22f), juce::roundToInt(rad * 1.5f), {0, juce::roundToInt(rad * 0.4f)}).drawForPath(g, p);
    g.setGradientFill(juce::ColourGradient(T.scrTop, r.getX(), r.getY(), T.scrBot, r.getX(), r.getBottom(), false)); g.fillPath(p);
    Graphics::ScopedSaveState st(g);
    g.reduceClipRegion(p);
    g.setGradientFill(juce::ColourGradient(T.accent.withAlpha(0.16f), r.getX() + r.getWidth() * 0.25f, r.getY(), T.accent.withAlpha(0.f), r.getX() + r.getWidth() * 0.25f, r.getY() + r.getHeight() * 0.55f, false));
    g.fillRect(r);
    juce::ColourGradient halo(T.scrHalo.withAlpha(0.28f), r.getCentreX(), r.getBottom(), T.scrHalo.withAlpha(0.f), r.getCentreX() + r.getWidth() * 0.45f, r.getBottom() - r.getHeight() * 0.2f, true);
    g.setGradientFill(halo); g.fillRect(r);
    g.setColour(juce::Colours::white.withAlpha(0.07f)); g.strokePath(p, juce::PathStrokeType(1.f));
}

// ------------------------------------------------------------ courbes lissées (hauteur, onde, enveloppe)
// Construit une courbe continue à partir d'une série échantillonnée (valeur absente : NaN ou ≤ absentBelow).
// Les échantillons sont regroupés (au plus maxPts points), lissés par un filtre aller-retour (sans déphasage),
// puis reliés par des Catmull-Rom (cubiques). Les Path et tampons sont réutilisés : aucune allocation par image.
class Trace {
public:
    // bridge : trous courts (en points) comblés par interpolation (micro-coupures du détecteur) ; les vrais silences restent
    struct Opts { int maxPts = 320; float smooth = 1.2f; float breakJump = 1e9f; float absentBelow = 0.f; int bridge = 6; };
    template <class FX, class FY>
    void build(const float* v, int n, FX fx, FY fy, float baseY, const Opts& o) {
        line.clear(); area.clear(); hasHead = false;
        const int nb = juce::jlimit(2, juce::jmax(2, o.maxPts), n);
        bx.resize((size_t)nb); bv.resize((size_t)nb); ok.resize((size_t)nb);
        const float step = (float)n / (float)nb;
        for (int b = 0; b < nb; ++b) {   // regroupement (moyenne des valeurs présentes)
            const int i0 = (int)((float)b * step), i1 = juce::jmax(i0 + 1, (int)((float)(b + 1) * step));
            float s = 0.f; int c = 0;
            for (int i = i0; i < i1 && i < n; ++i) if (std::isfinite(v[i]) && v[i] > o.absentBelow) { s += v[i]; ++c; }
            ok[(size_t)b] = (char)(c > 0 && c * 3 >= (i1 - i0));
            bv[(size_t)b] = c > 0 ? s / (float)c : 0.f;
            bx[(size_t)b] = fx(0.5f * (float)(i0 + i1 - 1));
        }
        for (int b = 1; b < nb; ++b) {   // micro-trous comblés
            if (ok[(size_t)b] || !ok[(size_t)b - 1]) continue;
            int e = b; while (e < nb && !ok[(size_t)e]) ++e;
            if (e < nb && e - b <= o.bridge) {
                const float v0 = bv[(size_t)b - 1], v1 = bv[(size_t)e];
                for (int k = b; k < e; ++k) { bv[(size_t)k] = v0 + (v1 - v0) * (float)(k - b + 1) / (float)(e - b + 1); ok[(size_t)k] = 1; }
            }
            b = e;
        }
        for (int b = 1; b < nb; ++b)   // coupure aux trous et aux sauts trop grands
            if (ok[(size_t)b] && ok[(size_t)b - 1] && std::fabs(bv[(size_t)b] - bv[(size_t)b - 1]) > o.breakJump) ok[(size_t)b - 1] = 2;   // 2 = fin de segment
        if (o.smooth > 0.f) {   // lissage aller-retour par segment (one-pole, constante en points)
            const float a = 1.f - std::exp(-1.f / o.smooth);
            int s0 = 0;
            while (s0 < nb) {
                while (s0 < nb && !ok[(size_t)s0]) ++s0;
                int s1 = s0;
                while (s1 < nb && ok[(size_t)s1] == 1) ++s1;
                if (s1 < nb && ok[(size_t)s1] == 2) ++s1;
                for (int k = s0 + 1; k < s1; ++k) bv[(size_t)k] += (bv[(size_t)k - 1] - bv[(size_t)k]) * (1.f - a);
                for (int k = s1 - 2; k >= s0; --k) bv[(size_t)k] += (bv[(size_t)k + 1] - bv[(size_t)k]) * (1.f - a);
                s0 = s1;
            }
        }
        int s0 = 0;
        while (s0 < nb) {
            while (s0 < nb && !ok[(size_t)s0]) ++s0;
            int s1 = s0;
            while (s1 < nb && ok[(size_t)s1] == 1) ++s1;
            if (s1 < nb && ok[(size_t)s1] == 2) ++s1;
            if (s1 - s0 >= 2) segment(s0, s1, fy, baseY);
            s0 = s1;
        }
    }
    // Trait arrondi avec glow doux (trois passes), remplissage dégradé facultatif, tête lumineuse facultative.
    void draw(Graphics& g, juce::Colour c, float width, float glow = 1.f, float fillAlpha = 0.f, float fillBottom = 0.f, bool head = false) const {
        if (line.isEmpty()) return;
        if (fillAlpha > 0.f) {
            const auto b = area.getBounds();
            g.setGradientFill(juce::ColourGradient(c.withAlpha(fillAlpha), 0, b.getY(), c.withAlpha(0.f), 0, juce::jmax(b.getY() + 1.f, fillBottom), false));
            g.fillPath(area);
        }
        const juce::PathStrokeType st(width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
        if (glow > 0.f) {
            g.setColour(c.withAlpha(0.07f * glow)); g.strokePath(line, juce::PathStrokeType(width * 6.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour(c.withAlpha(0.16f * glow)); g.strokePath(line, juce::PathStrokeType(width * 3.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        g.setColour(c); g.strokePath(line, st);
        if (head && hasHead) {
            const float r = width * 2.6f;
            g.setColour(c.withAlpha(0.22f)); g.fillEllipse(Rectangle<float>(r * 4.f, r * 4.f).withCentre(headPt));
            g.setColour(c); g.fillEllipse(Rectangle<float>(r * 2.f, r * 2.f).withCentre(headPt));
            g.setColour(juce::Colours::white); g.fillEllipse(Rectangle<float>(r * 1.2f, r * 1.2f).withCentre(headPt));
        }
    }
    const juce::Path& path() const { return line; }
    bool hasHead = false;
    juce::Point<float> headPt;
    float headValue = 0.f;

private:
    template <class FY>
    void segment(int s0, int s1, FY fy, float baseY) {
        auto P = [&](int k) { k = juce::jlimit(s0, s1 - 1, k); return juce::Point<float>(bx[(size_t)k], fy(bv[(size_t)k])); };
        line.startNewSubPath(P(s0)); area.startNewSubPath(P(s0).x, baseY); area.lineTo(P(s0));
        for (int k = s0; k < s1 - 1; ++k) {
            const auto p0 = P(k - 1), p1 = P(k), p2 = P(k + 1), p3 = P(k + 2);
            const auto c1 = p1 + (p2 - p0) / 6.f, c2 = p2 - (p3 - p1) / 6.f;
            line.cubicTo(c1, c2, p2); area.cubicTo(c1, c2, p2);
        }
        area.lineTo(P(s1 - 1).x, baseY); area.closeSubPath();
        hasHead = true; headPt = P(s1 - 1); headValue = bv[(size_t)s1 - 1];
    }
    juce::Path line, area;
    std::vector<float> bx, bv;
    std::vector<char> ok;
};

// ------------------------------------------------------------ jauge à aiguille fine (dans un écran)
inline void gauge(Graphics& g, const Theme& T, Rectangle<float> r, float v, float lo, float hi, const String& loTxt, const String& midTxt, const String& hiTxt, float px) {
    const float cx = r.getCentreX(), R = juce::jmin(r.getWidth() * 0.36f, r.getHeight() * 0.62f), cy = r.getY() + R + px * 1.6f;
    const float span = juce::MathConstants<float>::pi * 0.36f;
    auto ang = [&](float x) { return -span + 2.f * span * (juce::jlimit(lo, hi, x) - lo) / (hi - lo); };
    juce::Path arc; arc.addCentredArc(cx, cy, R, R, 0, -span, span, true);
    g.setColour(T.scrMute.withAlpha(0.8f)); g.strokePath(arc, juce::PathStrokeType(juce::jmax(1.f, R * 0.02f)));
    for (int i = 0; i <= 10; ++i) {
        const float a = -span + 2.f * span * (float)i / 10.f, l = i % 5 == 0 ? 0.12f : 0.07f;
        g.drawLine(juce::Line<float>(polar(cx, cy, R, a), polar(cx, cy, R * (1.f - l), a)), i % 5 == 0 ? 1.4f : 1.f);
    }
    g.setFont(font(px));
    g.setColour(T.scrMute);
    const auto pl = polar(cx, cy, R * 1.12f, -span), pr = polar(cx, cy, R * 1.12f, span), pm = polar(cx, cy, R * 1.1f, 0);
    g.drawText(loTxt, Rectangle<float>(px * 4.f, px * 1.4f).withCentre(pl).translated(-px * 1.2f, 0), juce::Justification::centred, false);
    g.drawText(hiTxt, Rectangle<float>(px * 4.f, px * 1.4f).withCentre(pr).translated(px * 1.2f, 0), juce::Justification::centred, false);
    g.drawText(midTxt, Rectangle<float>(px * 4.f, px * 1.4f).withCentre(pm).translated(0, -px * 0.4f), juce::Justification::centred, false);
    const float a = ang(v);
    const auto tip = polar(cx, cy, R * 0.98f, a), base = polar(cx, cy, R * 0.12f, a);
    g.setColour(T.out.withAlpha(0.25f)); g.drawLine(juce::Line<float>(base, tip), juce::jmax(3.f, R * 0.05f));
    g.setColour(T.out); g.drawLine(juce::Line<float>(base, tip), juce::jmax(1.4f, R * 0.022f));
    g.fillEllipse(Rectangle<float>(R * 0.09f, R * 0.09f).withCentre({cx, cy}));
}

// ------------------------------------------------------------ vumètre fin redessiné (clair : cadran crème ; sombre : cadran nuit)
inline void vu(Graphics& g, const Theme& T, Rectangle<float> r, float vuDb, bool peak, float px) {
    const float rad = r.getHeight() * 0.08f;
    const juce::Colour faceTop = T.dark ? juce::Colour(0xFF1C1F28) : juce::Colour(0xFFFBF4E4), faceBot = T.dark ? juce::Colour(0xFF151820) : juce::Colour(0xFFF1E3C3);
    const juce::Colour ink = T.dark ? juce::Colour(0xFFE9DFC8) : juce::Colour(0xFF2A2420), red = T.dark ? juce::Colour(0xFFFF6A4A) : juce::Colour(0xFFAE2A15);
    g.setGradientFill(juce::ColourGradient(faceTop, r.getX(), r.getY(), faceBot, r.getX(), r.getBottom(), false)); g.fillRoundedRectangle(r, rad);
    g.setColour(T.dark ? juce::Colours::white.withAlpha(0.10f) : juce::Colour(0x332A2420)); g.drawRoundedRectangle(r.reduced(0.5f), rad, 1.f);
    const juce::Point<float> pv(r.getCentreX(), r.getBottom() + r.getHeight() * 0.18f);
    const float R = (pv.y - r.getY()) * 0.80f, span = 0.78f;
    auto angOf = [&](double db) { return -span + 2.f * span * (float)VuBallistics::needleFraction(db); };
    juce::Path arc, redArc;
    arc.addCentredArc(pv.x, pv.y, R, R, 0, angOf(-20), angOf(0), true);
    redArc.addCentredArc(pv.x, pv.y, R, R, 0, angOf(0), angOf(3), true);
    g.setColour(ink.withAlpha(0.85f)); g.strokePath(arc, juce::PathStrokeType(juce::jmax(1.f, R * 0.012f)));
    g.setColour(red); g.strokePath(redArc, juce::PathStrokeType(juce::jmax(2.f, R * 0.035f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setFont(font(px));
    const struct { float db; const char* t; } marks[] = {{-20, "20"}, {-10, "10"}, {-5, "5"}, {0, "0"}, {3, "+3"}};
    for (auto& m : marks) {
        const float a = angOf(m.db);
        g.setColour(m.db > 0 ? red : ink.withAlpha(0.85f));
        g.drawLine(juce::Line<float>(polar(pv.x, pv.y, R, a), polar(pv.x, pv.y, R * 1.06f, a)), 1.2f);
        g.drawText(m.t, Rectangle<float>(px * 2.6f, px * 1.3f).withCentre(polar(pv.x, pv.y, R * 0.88f, a)), juce::Justification::centred, false);
    }
    g.setColour(ink); g.setFont(font(px * 1.15f, true));
    g.drawText("VU", Rectangle<float>(r.getX() + r.getWidth() * 0.07f, r.getBottom() - px * 2.2f, px * 3.f, px * 1.6f), juce::Justification::centredLeft, false);
    if (peak) { g.setColour(red.withAlpha(0.3f)); g.fillEllipse(Rectangle<float>(px * 1.2f, px * 1.2f).withCentre({r.getRight() - px * 1.4f, r.getY() + px * 1.4f})); }
    g.setColour(peak ? red : ink.withAlpha(0.35f)); g.fillEllipse(Rectangle<float>(px * 0.6f, px * 0.6f).withCentre({r.getRight() - px * 1.4f, r.getY() + px * 1.4f}));
    Graphics::ScopedSaveState st(g);
    g.reduceClipRegion(r.toNearestInt());
    const float a = angOf(vuDb);
    g.setColour(ink); g.drawLine(juce::Line<float>(polar(pv.x, pv.y, R * 0.25f, a), polar(pv.x, pv.y, R * 1.04f, a)), juce::jmax(1.2f, R * 0.012f));
}

// ------------------------------------------------------------ segments en pilule (Moteur, Latence…)
inline void segments(Graphics& g, const Theme& T, Rectangle<float> r, const juce::StringArray& labels, int sel, int hover, float px) {
    const float rad = r.getHeight() * 0.5f;
    g.setColour(T.field); g.fillRoundedRectangle(r, rad);
    g.setColour(T.fieldEdge); g.drawRoundedRectangle(r.reduced(0.5f), rad, 1.f);
    const int n = juce::jmax(1, labels.size());
    const float w = r.getWidth() / (float)n;
    for (int i = 0; i < n; ++i) {
        auto c = Rectangle<float>(r.getX() + w * (float)i, r.getY(), w, r.getHeight()).reduced(r.getHeight() * 0.09f);
        if (i == sel) {
            juce::DropShadow(T.shadow, juce::roundToInt(c.getHeight() * 0.25f), {0, 1}).drawForRectangle(g, c.toNearestInt());
            g.setColour(T.dark ? T.card.brighter(0.25f) : T.card); g.fillRoundedRectangle(c, c.getHeight() * 0.5f);
            g.setColour(T.accent); g.fillRoundedRectangle(Rectangle<float>(c.getWidth() * 0.28f, juce::jmax(2.f, px * 0.18f)).withCentre({c.getCentreX(), c.getBottom() - px * 0.45f}), 1.f);
        } else if (i == hover) { g.setColour(T.ink.withAlpha(0.05f)); g.fillRoundedRectangle(c, c.getHeight() * 0.5f); }
        g.setColour(i == sel ? T.ink : T.inkMute);
        caps(g, labels[i], c.withTrimmedBottom(px * 0.2f), px, juce::Justification::centred, i == sel);
    }
}

// ------------------------------------------------------------ potard haut de gamme
enum class KnobSize { S, M, L };
inline float knobRadius(KnobSize s) { return s == KnobSize::S ? 22.f : s == KnobSize::M ? 30.f : 40.f; }   // rayon de l'arc (unités)

struct KnobLook {
    float pos = 0.f;                 // position affichée (0..1)
    float modFrom = -1.f, modTo = -1.f, modAlpha = 0.f;   // arc de modulation / automation
    bool peach = false, bipolar = false, hover = false, focus = false, enabled = true, dragging = false;
};
// Socle statique (ombre portée, anneau de base, graduations, capuchon satiné, liseré) : rendu en cache.
inline void knobBase(Graphics& g, const Theme& T, Rectangle<float> box, bool peach) {
    const float cx = box.getCentreX(), cy = box.getCentreY(), r = box.getWidth() / 2.f / 1.30f;
    const float capR = r * 0.72f;
    // graduations fines et régulières, juste au-delà de l'arc
    g.setColour(T.tick);
    for (int i = 0; i <= 20; ++i) {
        const float a = angleOf((float)i / 20.f), l = i % 10 == 0 ? 0.13f : i % 2 == 0 ? 0.09f : 0.06f;
        g.drawLine(juce::Line<float>(polar(cx, cy, r * 1.10f, a), polar(cx, cy, r * (1.10f + l), a)), juce::jmax(0.75f, r * 0.018f));
    }
    // anneau de base (léger creux dans la plaque)
    auto base = Rectangle<float>(capR * 2.24f, capR * 2.24f).withCentre({cx, cy});
    g.setGradientFill(juce::ColourGradient(T.dark ? juce::Colour(0xFF0B0C10) : juce::Colour(0xFFD9DEE6), base.getX(), base.getY(),
                                           T.dark ? juce::Colour(0xFF2A2E38) : juce::Colour(0xFFFBFCFD), base.getRight(), base.getBottom(), false));
    g.fillEllipse(base);
    // ombre portée douce (lumière en haut à gauche -> ombre vers le bas, légèrement à droite)
    juce::Path cap; cap.addEllipse(Rectangle<float>(capR * 2.f, capR * 2.f).withCentre({cx, cy}));
    juce::DropShadow(juce::Colours::black.withAlpha(T.dark ? 0.55f : 0.20f), juce::roundToInt(capR * 0.30f), {juce::roundToInt(capR * 0.04f), juce::roundToInt(capR * 0.10f)}).drawForPath(g, cap);
    juce::DropShadow(juce::Colours::black.withAlpha(T.dark ? 0.45f : 0.12f), juce::roundToInt(capR * 0.06f), {0, juce::roundToInt(capR * 0.02f) + 1}).drawForPath(g, cap);
    // capuchon satiné : dégradé diagonal + reflet diffus (pas de brillant plastique)
    const auto top = peach ? T.capPeachTop : T.capTop, bot = peach ? T.capPeachBot : T.capBot;
    auto cr = cap.getBounds();
    g.setGradientFill(juce::ColourGradient(top, cr.getX() + cr.getWidth() * 0.2f, cr.getY(), bot, cr.getRight() - cr.getWidth() * 0.2f, cr.getBottom(), false));
    g.fillPath(cap);
    juce::ColourGradient sheen(juce::Colours::white.withAlpha(T.dark ? 0.07f : 0.45f), cx - capR * 0.35f, cy - capR * 0.45f, juce::Colours::white.withAlpha(0.f), cx - capR * 0.35f + capR * 0.95f, cy - capR * 0.45f, true);
    g.setGradientFill(sheen); g.fillPath(cap);
    // face supérieure très légèrement en retrait : anneau de chanfrein
    auto face = cr.reduced(capR * 0.10f);
    g.setGradientFill(juce::ColourGradient(bot.interpolatedWith(top, 0.35f), face.getX(), face.getY(), top.interpolatedWith(bot, 0.15f), face.getRight(), face.getBottom(), false));
    g.fillEllipse(face);
    g.setGradientFill(juce::ColourGradient(juce::Colours::black.withAlpha(T.dark ? 0.35f : 0.10f), face.getX(), face.getY(), juce::Colours::white.withAlpha(T.dark ? 0.06f : 0.5f), face.getRight(), face.getBottom(), false));
    g.drawEllipse(face, juce::jmax(0.6f, capR * 0.018f));
    // liseré : clair en haut à gauche, sombre en bas à droite
    g.setGradientFill(juce::ColourGradient(juce::Colours::white.withAlpha(T.dark ? 0.16f : 0.95f), cr.getX(), cr.getY(), juce::Colours::black.withAlpha(T.dark ? 0.45f : 0.16f), cr.getRight(), cr.getBottom(), false));
    g.drawEllipse(cr.reduced(0.5f), juce::jmax(0.8f, capR * 0.022f));
}

// Partie vivante : piste, arc de valeur lumineux, arc de modulation, point incrusté, survol, focus.
inline void knobLive(Graphics& g, const Theme& T, Rectangle<float> box, const KnobLook& k) {
    const float cx = box.getCentreX(), cy = box.getCentreY(), r = box.getWidth() / 2.f / 1.30f, capR = r * 0.72f;
    const float w = juce::jmax(1.6f, r * 0.055f);
    const juce::PathStrokeType st(w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    juce::Path track; track.addCentredArc(cx, cy, r, r, 0, kA0, kA1, true);
    g.setColour(k.hover || k.dragging ? T.track.interpolatedWith(T.inkMute, 0.30f) : T.track); g.strokePath(track, st);
    const auto acc = k.enabled ? T.accent : T.inkMute.withAlpha(0.6f);
    const float a0 = k.bipolar ? angleOf(0.5f) : kA0, a1 = angleOf(k.pos);
    if (std::fabs(a1 - a0) > 0.002f) {
        juce::Path val; val.addCentredArc(cx, cy, r, r, 0, std::min(a0, a1), std::max(a0, a1), true);
        if (k.enabled) { g.setColour(acc.withAlpha(0.16f)); g.strokePath(val, juce::PathStrokeType(w * 3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded)); }
        g.setColour(acc); g.strokePath(val, st);
    }
    if (k.modAlpha > 0.01f && k.modFrom >= 0.f && std::fabs(k.modTo - k.modFrom) > 0.002f) {
        const float rm = r * 1.30f, b0 = angleOf(k.modFrom), b1 = angleOf(k.modTo);
        juce::Path m; m.addCentredArc(cx, cy, rm, rm, 0, std::min(b0, b1), std::max(b0, b1), true);
        g.setColour(T.mod.withAlpha(k.modAlpha)); g.strokePath(m, juce::PathStrokeType(w * 0.85f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.fillEllipse(Rectangle<float>(w * 2.2f, w * 2.2f).withCentre(polar(cx, cy, rm, b1)));
    }
    // repère : point lumineux incrusté dans le capuchon
    const auto p = polar(cx, cy, capR * 0.70f, a1);
    const float d = juce::jmax(3.f, capR * 0.15f);
    g.setColour(juce::Colours::black.withAlpha(T.dark ? 0.55f : 0.16f)); g.fillEllipse(Rectangle<float>(d * 1.5f, d * 1.5f).withCentre(p.translated(0, d * 0.08f)));
    g.setColour(acc.withAlpha(0.30f)); g.fillEllipse(Rectangle<float>(d * 1.9f, d * 1.9f).withCentre(p));
    g.setColour(acc); g.fillEllipse(Rectangle<float>(d, d).withCentre(p));
    g.setColour(juce::Colours::white.withAlpha(0.55f)); g.fillEllipse(Rectangle<float>(d * 0.38f, d * 0.38f).withCentre(p.translated(-d * 0.12f, -d * 0.12f)));
    if (k.focus) { g.setColour(T.accent.withAlpha(0.55f)); g.drawEllipse(Rectangle<float>(capR * 2.5f, capR * 2.5f).withCentre({cx, cy}), 1.6f); }
}

// Suivi de modulation / automation : arc bleu entre la valeur de départ et la valeur courante, tant que l'hôte la fait bouger.
struct ModTrail {
    float base = -1.f, last = -1.f, alpha = 0.f, idle = 0.f;
    void reset(float v) { base = last = v; alpha = 0.f; idle = 0.f; }
    void update(float v, bool userGesture, float dt) {
        if (base < 0.f) { reset(v); return; }
        const bool moved = std::fabs(v - last) > 1e-4f;
        last = v;
        if (userGesture) { reset(v); return; }
        if (moved) { alpha = 1.f; idle = 0.f; }
        else if ((idle += dt) > 0.8f) { alpha = juce::jmax(0.f, alpha - dt * 2.5f); if (alpha <= 0.f) base = v; }
    }
};

// Look v3 : hérite du Look classique et ne redéfinit que l'apparence.
class Look3 : public kit::Look {
public:
    explicit Look3(const Theme& t) : kit::Look(kit::Palette{}) { setTheme(t); }
    void setTheme(const Theme& t) {
        T = t;
        setColour(juce::PopupMenu::backgroundColourId, T.card);
        setColour(juce::PopupMenu::textColourId, T.ink);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, T.accent.withAlpha(0.14f));
        setColour(juce::PopupMenu::highlightedTextColourId, T.ink);
        setColour(juce::PopupMenu::headerTextColourId, T.inkMute);
        setColour(juce::TooltipWindow::backgroundColourId, T.dark ? juce::Colour(0xFF262A34) : juce::Colour(0xFF15171C));
        setColour(juce::TooltipWindow::textColourId, juce::Colour(0xFFF2F3F6));
        setColour(juce::TooltipWindow::outlineColourId, juce::Colours::transparentBlack);
        setColour(juce::ComboBox::textColourId, T.ink);
        setColour(juce::Label::textColourId, T.ink);
        setColour(juce::Slider::textBoxTextColourId, T.ink);
        setColour(juce::Slider::textBoxBackgroundColourId, T.field);
        setColour(juce::Slider::textBoxOutlineColourId, T.fieldEdge);
        setColour(juce::TextEditor::backgroundColourId, T.card);
        setColour(juce::TextEditor::textColourId, T.ink);
        setColour(juce::TextEditor::highlightColourId, T.accent.withAlpha(0.25f));
        setColour(juce::TextEditor::outlineColourId, T.fieldEdge);
        setColour(juce::TextEditor::focusedOutlineColourId, T.accent);
        setColour(juce::CaretComponent::caretColourId, T.accentText);
    }
    const Theme& theme() const { return T; }

    void drawRotarySlider(Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider& s) override {
        const float d = (float)juce::jmin(w, h);
        auto box = Rectangle<float>(d, d).withCentre({(float)x + (float)w * 0.5f, (float)y + (float)h * 0.5f});
        knobBase(g, T, box, false);
        KnobLook k; k.pos = pos; k.hover = s.isMouseOverOrDragging(); k.enabled = s.isEnabled();
        knobLive(g, T, box, k);
    }
    void drawLinearSlider(Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider::SliderStyle st, juce::Slider& s) override {
        if (st != juce::Slider::LinearHorizontal) { kit::Look::drawLinearSlider(g, x, y, w, h, pos, 0, 0, st, s); return; }
        const float cy = (float)y + (float)h * 0.5f, th = juce::jmax(3.f, (float)h * 0.16f);
        auto tr = Rectangle<float>((float)x + 4.f, cy - th * 0.5f, (float)w - 8.f, th);
        g.setColour(T.track); g.fillRoundedRectangle(tr, th * 0.5f);
        g.setColour(s.isEnabled() ? T.accent : T.inkMute); g.fillRoundedRectangle(tr.withRight(juce::jmax(tr.getX() + th, pos)), th * 0.5f);
        const float kr = juce::jmax(5.f, (float)h * 0.30f);
        auto thumb = Rectangle<float>(kr * 2.f, kr * 2.f).withCentre({pos, cy});
        juce::DropShadow(juce::Colours::black.withAlpha(0.3f), juce::roundToInt(kr * 0.6f), {0, 1}).drawForRectangle(g, thumb.toNearestInt());
        g.setColour(T.dark ? T.capTop : juce::Colours::white); g.fillEllipse(thumb);
        g.setColour(T.accent); g.fillEllipse(thumb.reduced(kr * 0.55f));
    }
    void drawButtonBackground(Graphics& g, juce::Button& b, const juce::Colour&, bool hover, bool down) override {
        auto r = b.getLocalBounds().toFloat().reduced(1.f);
        const float rad = juce::jmin(r.getHeight() * 0.5f, 10.f);
        g.setColour(down ? T.field.darker(0.05f) : hover ? T.field.brighter(0.04f) : T.field); g.fillRoundedRectangle(r, rad);
        g.setColour(b.getToggleState() ? T.accent : T.fieldEdge); g.drawRoundedRectangle(r.reduced(0.5f), rad, 1.f);
    }
    void drawButtonText(Graphics& g, juce::TextButton& b, bool, bool) override {
        g.setColour(!b.isEnabled() ? T.inkFaint : b.getToggleState() ? T.accentText : T.ink);
        caps(g, b.getButtonText(), b.getLocalBounds().toFloat(), juce::jlimit(9.f, 13.f, (float)b.getHeight() * 0.40f), juce::Justification::centred);
    }
    void drawToggleButton(Graphics& g, juce::ToggleButton& b, bool hover, bool) override {
        auto r = b.getLocalBounds().toFloat();
        const float px = juce::jlimit(9.f, 15.f, r.getHeight() * 0.62f), d = px * 0.62f;
        const auto dot = Rectangle<float>(d, d).withCentre({r.getX() + d, r.getCentreY()});
        const bool on = b.getToggleState();
        if (on) { g.setColour(T.accent.withAlpha(0.25f)); g.fillEllipse(dot.expanded(d * 0.35f)); }
        g.setColour(on ? T.accent : T.track); g.fillEllipse(dot);
        g.setColour(!b.isEnabled() ? T.inkFaint : hover ? T.ink : T.ink.withAlpha(0.92f)); g.setFont(font(px));
        g.drawText(b.getButtonText(), r.withTrimmedLeft(d * 2.4f), juce::Justification::centredLeft, true);
    }
    void drawComboBox(Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& cb) override {
        if ((bool)cb.getProperties()["v3bare"]) {
            if (cb.isMouseOver(true)) { g.setColour(T.ink.withAlpha(0.05f)); g.fillRoundedRectangle(Rectangle<float>((float)w, (float)h), (float)h * 0.3f); }
            return;
        }
        auto r = Rectangle<float>(0, 0, (float)w, (float)h).reduced(0.5f);
        const float rad = juce::jmin(r.getHeight() * 0.32f, 12.f);
        g.setColour(T.field); g.fillRoundedRectangle(r, rad);
        g.setColour(cb.isMouseOver(true) ? T.inkMute.withAlpha(0.6f) : T.fieldEdge); g.drawRoundedRectangle(r, rad, 1.f);
        const float cx = (float)w - (float)h * 0.55f, cy = (float)h * 0.5f, t = juce::jlimit(3.f, 6.f, (float)h * 0.13f);
        juce::Path ch; ch.startNewSubPath(cx - t, cy - t * 0.45f); ch.lineTo(cx, cy + t * 0.55f); ch.lineTo(cx + t, cy - t * 0.45f);
        g.setColour(T.ink.withAlpha(0.85f)); g.strokePath(ch, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    juce::Font getComboBoxFont(juce::ComboBox& cb) override {
        const bool bare = (bool)cb.getProperties()["v3bare"];   // liste « nue » (nom de preset dans une pilule)
        return font(juce::jlimit(9.f, bare ? 20.f : 17.f, (float)cb.getHeight() * (bare ? 0.62f : 0.5f)), bare);
    }
    void positionComboBoxText(juce::ComboBox& cb, juce::Label& l) override {
        if ((bool)cb.getProperties()["v3bare"]) { l.setBounds(0, 0, cb.getWidth(), cb.getHeight()); l.setJustificationType(juce::Justification::centred); l.setFont(getComboBoxFont(cb)); return; }
        l.setBounds(juce::roundToInt((float)cb.getHeight() * 0.35f), 1, cb.getWidth() - cb.getHeight() - juce::roundToInt((float)cb.getHeight() * 0.35f), cb.getHeight() - 2);
        l.setFont(getComboBoxFont(cb));
    }
    juce::Font getPopupMenuFont() override { return font(14.f); }
    void drawPopupMenuBackground(Graphics& g, int w, int h) override {
        g.fillAll(T.card);
        g.setColour(T.fieldEdge); g.drawRect(0, 0, w, h, 1);
    }
    juce::Font getTextButtonFont(juce::TextButton&, int h) override { return font(juce::jlimit(9.f, 13.f, (float)h * 0.40f), true); }
    void drawTooltip(Graphics& g, const String& text, int w, int h) override {
        g.setColour(findColour(juce::TooltipWindow::backgroundColourId)); g.fillRoundedRectangle(Rectangle<float>((float)w, (float)h), 5.f);
        g.setColour(findColour(juce::TooltipWindow::textColourId));
        juce::AttributedString s; s.setJustification(juce::Justification::centredLeft); s.append(text, font(13.f), findColour(juce::TooltipWindow::textColourId));
        juce::TextLayout tl; tl.createLayoutWithBalancedLineLengths(s, 380.f); tl.draw(g, Rectangle<float>((float)w, (float)h).reduced(10.f, 6.f));
    }
    Rectangle<int> getTooltipBounds(const String& text, juce::Point<int> pos, Rectangle<int> parent) override {
        juce::AttributedString s; s.append(text, font(13.f)); juce::TextLayout tl; tl.createLayoutWithBalancedLineLengths(s, 380.f);
        const int w = (int)tl.getWidth() + 22, h = (int)tl.getHeight() + 14;
        return Rectangle<int>(pos.x > parent.getCentreX() ? pos.x - (w + 12) : pos.x + 12, pos.y > parent.getCentreY() ? pos.y - (h + 6) : pos.y + 6, w, h).constrainedWithin(parent);
    }
    void drawCornerResizer(Graphics& g, int w, int h, bool over, bool drag) override {
        g.setColour(T.inkMute.withAlpha(over || drag ? 0.8f : 0.4f));
        for (int i = 0; i < 3; ++i) for (int j = 0; j <= i; ++j) g.fillEllipse((float)w - 4.f - 4.f * (float)j, (float)h - 4.f - 4.f * (float)(i - j), 2.f, 2.f);
    }
private:
    Theme T;
};

// Potard v3 : un juce::Slider qui dessine lui-même potard, libellé, valeur et sous-titre.
class Knob3 : public juce::Slider {
public:
    using Formatter = std::function<String(double)>;
    Knob3(juce::AudioProcessorValueTreeState& a, const String& id, const String& label_, const String& tip, KnobSize sz = KnobSize::M)
        : label(label_), size(sz), par(a.getParameter(id)) {
        setSliderStyle(juce::Slider::RotaryVerticalDrag); setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        setMouseDragSensitivity(kDrag); setScrollWheelEnabled(true);
        setTooltip(tip + u8(" Double-clic : valeur d'origine. Maj : réglage fin. Clic droit : saisir une valeur."));
        setTitle(label); setDescription(tip);
        att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(a, id, *this);
        if (par != nullptr) setDoubleClickReturnValue(true, par->convertFrom0to1(par->getDefaultValue()));
        disp = (float)valueToProportionOfLength(getValue());
        vblank = std::make_unique<juce::VBlankAttachment>(this, [this](double t) { animate(t); });
    }
    ~Knob3() override { vblank.reset(); }
    void setup(const Theme* t, ArtCache* c) { T = t; cache = c; repaint(); }
    void setScale(float u_) { u = u_; repaint(); }
    void setSub(const String& s) { sub = s; repaint(); }
    void setFormatter(Formatter f) { formatter = std::move(f); repaint(); }
    void setPeach(bool b) { peach = b; }
    void setBipolar(bool b) { bipolar = b; }
    void setScaleLabels(const String& lo, const String& hi) { scaleLo = lo; scaleHi = hi; }
    void setHideLabel(bool h) { hideLabel = h; }
    void setLabelPx(float px) { labelPx = px; repaint(); }
    void setKnobSize(KnobSize s) { size = s; repaint(); }
    void tickMod(float dt) { trail.update((float)valueToProportionOfLength(getValue()), isMouseButtonDown() || entry != nullptr, dt); if (trail.alpha > 0.f) repaint(); }
    void resetMod() { trail.reset((float)valueToProportionOfLength(getValue())); }
    String valueText() const { return formatter ? formatter(getValue()) : fmt::param(par); }
    // hauteur totale conseillée pour une taille donnée (unités)
    static float boxHeight(KnobSize s, bool withSub) { return knobRadius(s) * 2.f * 1.30f + 46.f + (withSub ? 18.f : 0.f); }

    void paint(Graphics& g) override {
        if (T == nullptr) return;
        const auto& t = *T;
        const float r = knobRadius(size) * u, d = r * 2.f * 1.30f;
        auto box = Rectangle<float>(d, d).withCentre({(float)getWidth() * 0.5f, d * 0.5f});
        Graphics::ScopedSaveState st(g);
        if (!isEnabled()) g.setOpacity(0.55f);
        if (cache != nullptr) cache->draw(g, String("knob|") + modeKey(t.mode) + (peach ? "|p" : "|n") + "|" + String(t.accent.getARGB()), box, [&](Graphics& ig, Rectangle<float> b) { knobBase(ig, t, b, peach); });
        else knobBase(g, t, box, peach);
        KnobLook k; k.pos = disp; k.peach = peach; k.bipolar = bipolar; k.hover = isMouseOver(true); k.dragging = isMouseButtonDown();
        k.focus = hasKeyboardFocus(false); k.enabled = isEnabled();
        if (trail.alpha > 0.f) { k.modFrom = trail.base; k.modTo = trail.last; k.modAlpha = trail.alpha; }
        knobLive(g, t, box, k);
        const float px = 11.f * u;
        if (scaleLo.isNotEmpty()) {
            g.setColour(t.inkMute); g.setFont(font(px));
            g.drawText(scaleLo, Rectangle<float>(px * 3.f, px * 1.3f).withCentre(polar(box.getCentreX(), box.getCentreY(), r * 1.36f, kA0).translated(-px * 0.2f, px * 0.5f)), juce::Justification::centred, false);
            g.drawText(scaleHi, Rectangle<float>(px * 3.f, px * 1.3f).withCentre(polar(box.getCentreX(), box.getCentreY(), r * 1.36f, kA1).translated(px * 0.2f, px * 0.5f)), juce::Justification::centred, false);
        }
        float y = d + 4.f * u;
        if (!hideLabel) { g.setColour(isEnabled() ? t.ink : t.inkMute); caps(g, label, {0, y, (float)getWidth(), 16.f * u}, labelPx * u, juce::Justification::centred); y += 18.f * u; }
        const auto vt = valueText();
        if (isMouseButtonDown() && entry == nullptr) {   // glissement : valeur agrandie en pastille
            const auto f = font(19.f * u, true);
            const float w = tabularWidth(vt, f) + 20.f * u;
            auto pill = Rectangle<float>(w, 26.f * u).withCentre({(float)getWidth() * 0.5f, y + 12.f * u});
            g.setColour(t.dark ? t.card.brighter(0.3f) : t.ink); g.fillRoundedRectangle(pill, 13.f * u);
            g.setColour(t.dark ? t.ink : t.card); tabular(g, vt, pill, f, juce::Justification::centred);
        } else if (entry == nullptr) {
            g.setColour(t.ink); tabular(g, vt, {0, y, (float)getWidth(), 24.f * u}, font(19.f * u, true), juce::Justification::centred);
        }
        y += 26.f * u;
        if (sub.isNotEmpty()) { g.setColour(t.inkMute); g.setFont(font(13.f * u)); g.drawText(sub, Rectangle<float>(0, y, (float)getWidth(), 16.f * u), juce::Justification::centred, true); }
    }
    void mouseDown(const juce::MouseEvent& e) override {
        if (e.mods.isPopupMenu()) { openEntry(); return; }
        setMouseDragSensitivity(e.mods.isShiftDown() ? kDrag * 6 : kDrag);
        juce::Slider::mouseDown(e); repaint();
    }
    void mouseUp(const juce::MouseEvent& e) override { juce::Slider::mouseUp(e); setMouseDragSensitivity(kDrag); repaint(); }
    void mouseEnter(const juce::MouseEvent& e) override { juce::Slider::mouseEnter(e); repaint(); }
    void mouseExit(const juce::MouseEvent& e) override { juce::Slider::mouseExit(e); repaint(); }
    void valueChanged() override { if (!isShowing() || isMouseButtonDown()) disp = (float)valueToProportionOfLength(getValue()); repaint(); }
    void focusGained(FocusChangeType) override { repaint(); }
    void focusLost(FocusChangeType) override { repaint(); }
    void resized() override { if (entry != nullptr) placeEntry(); }

private:
    static constexpr int kDrag = 220;
    void animate(double) {   // lissage visuel de l'index (la valeur réelle n'est pas touchée)
        const float tgt = (float)valueToProportionOfLength(getValue());
        if (std::fabs(tgt - disp) < 0.0005f) { if (disp != tgt) { disp = tgt; repaint(); } return; }
        disp += (tgt - disp) * 0.35f; repaint();
    }
    void placeEntry() {
        const float r = knobRadius(size) * u, d = r * 2.f * 1.30f;
        entry->setBounds(Rectangle<float>(juce::jmin((float)getWidth(), 90.f * u), 24.f * u).withCentre({(float)getWidth() * 0.5f, d + (hideLabel ? 16.f : 34.f) * u}).toNearestInt());
    }
    void openEntry() {
        entry = std::make_unique<juce::TextEditor>();
        entry->setJustification(juce::Justification::centred); entry->setFont(font(15.f * u, true));
        entry->setText(valueText().replace(juce::String::charToString((juce::juce_wchar)0x00A0), " "), false); entry->selectAll();
        addAndMakeVisible(*entry); placeEntry(); entry->grabKeyboardFocus();
        juce::Component::SafePointer<Knob3> self(this);
        auto done = [this, self](bool apply) {
            if (entry == nullptr) return;
            if (apply) {
                auto s = entry->getText().replace(",", ".").replace(fmt::minus(), "-").retainCharacters("0123456789.-+");
                if (s.containsAnyOf("0123456789")) setValue(juce::jlimit(getMinimum(), getMaximum(), getValueFromText(s)), juce::sendNotificationSync);
            }
            juce::MessageManager::callAsync([safe = self] { if (safe != nullptr) { safe->entry.reset(); safe->repaint(); } });
        };
        entry->onReturnKey = [done] { done(true); };
        entry->onEscapeKey = [done] { done(false); };
        entry->onFocusLost = [done] { done(true); };
        repaint();
    }
    const Theme* T = nullptr;
    ArtCache* cache = nullptr;
    String label, sub, scaleLo, scaleHi;
    KnobSize size;
    juce::RangedAudioParameter* par;
    Formatter formatter;
    float u = 1.f, disp = 0.f, labelPx = 12.5f;
    bool peach = false, bipolar = false, hideLabel = false;
    ModTrail trail;
    std::unique_ptr<juce::TextEditor> entry;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    std::unique_ptr<juce::VBlankAttachment> vblank;
};

// ------------------------------------------------------------ interrupteur IN (paramètre bypass : 0 = en service)
class InSwitch : public juce::Component, public juce::SettableTooltipClient {
public:
    InSwitch(juce::RangedAudioParameter& p) : att(p, [this](float v) { on = v < 0.5f; repaint(); }, nullptr) {
        att.sendInitialUpdate(); setTooltip(u8("En service / contourné (bypass)."));
        setTitle("IN"); setWantsKeyboardFocus(true); setMouseCursor(juce::MouseCursor::PointingHandCursor);
    }
    void setup(const Theme* t) { T = t; repaint(); }
    void paint(Graphics& g) override {
        if (T == nullptr) return;
        auto r = getLocalBounds().toFloat();
        const float h = r.getHeight(), px = juce::jmax(9.f, h * 0.48f);
        auto pill = r.removeFromRight(h * 1.9f).reduced(h * 0.08f);
        g.setColour(T->ink); caps(g, "IN", r.withTrimmedRight(h * 0.3f), px, juce::Justification::centredRight);
        g.setColour(on ? T->accent : T->track); g.fillRoundedRectangle(pill, pill.getHeight() * 0.5f);
        const float d = pill.getHeight() * 0.76f;
        auto th = Rectangle<float>(d, d).withCentre({on ? pill.getRight() - pill.getHeight() * 0.5f : pill.getX() + pill.getHeight() * 0.5f, pill.getCentreY()});
        juce::DropShadow(juce::Colours::black.withAlpha(0.3f), juce::roundToInt(d * 0.2f), {0, 1}).drawForRectangle(g, th.toNearestInt());
        g.setColour(juce::Colours::white); g.fillEllipse(th);
        if (hasKeyboardFocus(false)) { g.setColour(T->accent.withAlpha(0.5f)); g.drawRoundedRectangle(pill.expanded(2.f), pill.getHeight() * 0.5f + 2.f, 1.5f); }
    }
    void mouseDown(const juce::MouseEvent&) override { att.setValueAsCompleteGesture(on ? 1.f : 0.f); }
    bool keyPressed(const juce::KeyPress& k) override { if (k == juce::KeyPress::spaceKey || k == juce::KeyPress::returnKey) { att.setValueAsCompleteGesture(on ? 1.f : 0.f); return true; } return false; }
private:
    const Theme* T = nullptr; bool on = true; juce::ParameterAttachment att;
};

// ------------------------------------------------------------ sélecteur discret thème + taille (en-tête)
class ThemeMenu : public juce::Button {
public:
    ThemeMenu() : juce::Button(u8("Thème")) { setTooltip(u8("Apparence : Clair, Sombre ou Classique ; taille de la fenêtre.")); setTitle(u8("Thème et taille")); }
    std::function<void(Mode)> onTheme;
    std::function<void(float)> onScale;
    Mode current = Mode::Light; float scale = 1.f;
    const Theme* T = nullptr;
    void paintButton(Graphics& g, bool hover, bool down) override {
        auto r = getLocalBounds().toFloat().reduced(1.f);
        const float d = juce::jmin(r.getWidth(), r.getHeight());
        auto c = Rectangle<float>(d, d).withCentre(r.getCentre()).translated(0, down ? 0.5f : 0.f);
        juce::Colour ring, ink;
        if (current == Mode::Classic || T == nullptr) {   // capuchon sombre de la face classique
            g.setColour(juce::Colours::black.withAlpha(0.6f)); g.fillEllipse(c.expanded(1.f));
            g.setGradientFill(juce::ColourGradient(juce::Colour(hover ? 0xFF55555C : 0xFF46464C), c.getX(), c.getY(), juce::Colour(0xFF151517), c.getX(), c.getBottom(), false)); g.fillEllipse(c);
            ink = juce::Colour(0xFFEDEAF2);
        } else {
            g.setColour(hover ? T->field.brighter(0.05f) : T->field); g.fillEllipse(c);
            g.setColour(T->fieldEdge); g.drawEllipse(c.reduced(0.5f), 1.f);
            ink = T->ink;
        }
        auto ic = c.reduced(d * 0.28f);   // icône demi-disque
        g.setColour(ink); g.drawEllipse(ic, juce::jmax(1.f, d * 0.06f));
        juce::Path half; half.addPieSegment(ic, 0.f, juce::MathConstants<float>::pi, 0.f); g.fillPath(half);
        juce::ignoreUnused(ring);
    }
    void clicked() override {
        juce::PopupMenu m;
        m.addSectionHeader(u8("Thème"));
        for (auto md : {Mode::Light, Mode::Dark, Mode::Classic}) m.addItem(1 + (int)md, modeName(md), true, md == current);
        m.addSectionHeader("Taille");
        const float sizes[] = {0.75f, 1.f, 1.25f, 1.5f};
        for (int i = 0; i < 4; ++i) m.addItem(10 + i, String(juce::roundToInt(sizes[i] * 100.f)) + juce::String::charToString((juce::juce_wchar)0x00A0) + "%", true, std::fabs(scale - sizes[i]) < 0.04f);
        juce::Component::SafePointer<ThemeMenu> self(this);
        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this), [sp = self](int r) {
            if (sp == nullptr || r == 0) return;
            const float sz[] = {0.75f, 1.f, 1.25f, 1.5f};
            if (r >= 1 && r <= 3) { if (sp->onTheme) sp->onTheme((Mode)(r - 1)); }
            else if (r >= 10 && r <= 13) { if (sp->onScale) sp->onScale(sz[r - 10]); }
        });
    }
};

// ------------------------------------------------------------ chevron (presets précédent / suivant)
class Chevron : public juce::Button {
public:
    Chevron(bool left_) : juce::Button(left_ ? u8("Précédent") : "Suivant"), left(left_) {}
    const Theme* T = nullptr;
    void paintButton(Graphics& g, bool hover, bool down) override {
        if (T == nullptr) return;
        auto r = getLocalBounds().toFloat();
        const float s = juce::jmin(r.getWidth(), r.getHeight()) * 0.22f, cx = r.getCentreX() + (down ? 0.5f : 0.f), cy = r.getCentreY();
        juce::Path p; p.startNewSubPath(cx + (left ? s * 0.5f : -s * 0.5f), cy - s); p.lineTo(cx + (left ? -s * 0.5f : s * 0.5f), cy); p.lineTo(cx + (left ? s * 0.5f : -s * 0.5f), cy + s);
        g.setColour(hover ? T->accentText : T->ink); g.strokePath(p, juce::PathStrokeType(juce::jmax(1.6f, s * 0.28f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
private:
    bool left;
};

// ------------------------------------------------------------ ponts : composants classiques qui savent aussi se dessiner en v3
// Même objet, mêmes attachements : en Classique (T nul ou mode Classic), rendu d'origine à l'identique.
namespace bridge {
inline bool v3(const Theme* T) { return T != nullptr && !T->isClassic(); }

class HelpKey : public kit::w::HelpKey {
public:
    const Theme* T = nullptr;
    void paintButton(Graphics& g, bool hover, bool down) override {
        if (!v3(T)) { kit::w::HelpKey::paintButton(g, hover, down); return; }
        auto r = getLocalBounds().toFloat().reduced(1.f);
        const float d = juce::jmin(r.getWidth(), r.getHeight());
        auto c = Rectangle<float>(d, d).withCentre(r.getCentre());
        g.setColour(hover ? T->field.brighter(0.05f) : T->field); g.fillEllipse(c);
        g.setColour(T->fieldEdge); g.drawEllipse(c.reduced(0.5f), 1.f);
        g.setColour(T->ink); g.setFont(font(d * 0.5f, true)); g.drawText("?", c.translated(0, down ? 0.5f : 0.f), juce::Justification::centred, false);
    }
};

class PushKey : public kit::w::PushKey {
public:
    using kit::w::PushKey::PushKey;
    const Theme* T = nullptr;
    bool onScreen = false;   // posé dans un écran sombre : couleurs d'écran quel que soit le thème
    void paintButton(Graphics& g, bool hover, bool down) override {
        if (!v3(T)) { kit::w::PushKey::paintButton(g, hover, down); return; }
        auto r = getLocalBounds().toFloat().reduced(1.f);
        const bool on = getToggleState() || lit;
        const float rad = juce::jmin(r.getHeight() * 0.3f, 12.f);
        const auto bg = onScreen ? juce::Colour(0xFF1E2446) : T->card, edge = onScreen ? juce::Colour(0x33FFFFFF) : T->fieldEdge;
        const auto ink = onScreen ? T->scrInk : T->ink, acc = onScreen ? T->out : T->accentText;
        Graphics::ScopedSaveState st(g);
        if (!isEnabled()) g.setOpacity(0.5f);
        if (!onScreen) juce::DropShadow(T->shadow, juce::roundToInt(rad), {0, 2}).drawForRectangle(g, r.reduced(2.f).toNearestInt());
        g.setColour(down ? bg.darker(0.05f) : hover ? bg.brighter(0.04f) : bg); g.fillRoundedRectangle(r, rad);
        g.setColour(on ? (onScreen ? T->out : T->accent) : edge); g.drawRoundedRectangle(r.reduced(0.5f), rad, on ? 1.5f : 1.f);
        g.setColour(on ? acc : ink);
        caps(g, getButtonText(), r, juce::jlimit(9.f, 14.f, r.getHeight() * 0.36f), juce::Justification::centred);
    }
};

// Sélecteur à touches classique (kit::w::Seg) dessiné en segments-pilule en v3.
class Seg : public kit::w::Seg {
public:
    Seg(juce::AudioProcessorValueTreeState& a, const String& id, juce::StringArray names, const String& tip, const kit::Palette& pal)
        : kit::w::Seg(a, id, names, tip, pal), labels(std::move(names)) {}
    const Theme* T = nullptr;
    float px = 12.f;
    void paint(Graphics& g) override {
        if (!v3(T)) { kit::w::Seg::paint(g); return; }
        segments(g, *T, getLocalBounds().toFloat().reduced(1.f), labels, selected(), hover, px);
    }
    void mouseMove(const juce::MouseEvent& e) override { const int h = juce::jlimit(0, labels.size() - 1, (int)(e.position.x / ((float)getWidth() / (float)labels.size()))); if (h != hover) { hover = h; repaint(); } }
    void mouseExit(const juce::MouseEvent&) override { hover = -1; repaint(); }
private:
    juce::StringArray labels; int hover = -1;
};

class Power : public kit::w::Power {
public:
    using kit::w::Power::Power;
    const Theme* T = nullptr;
    void paintButton(Graphics& g, bool hover, bool down) override {
        if (!v3(T)) { kit::w::Power::paintButton(g, hover, down); return; }
        auto r = getLocalBounds().toFloat();
        const float h = juce::jmin(r.getHeight(), r.getWidth() / 2.9f), px = juce::jmax(9.f, h * 0.5f);
        auto pill = Rectangle<float>(h * 1.9f, h).withRightX(r.getRight()).withCentre({r.getRight() - h * 0.95f, r.getCentreY()}).reduced(h * 0.06f);
        const bool on = !getToggleState();   // état basculé = contourné
        g.setColour(T->ink); caps(g, "IN", r.withRight(pill.getX() - h * 0.3f), px, juce::Justification::centredRight);
        g.setColour(on ? T->accent : T->track); g.fillRoundedRectangle(pill, pill.getHeight() * 0.5f);
        const float d = pill.getHeight() * 0.76f;
        auto th = Rectangle<float>(d, d).withCentre({on ? pill.getRight() - pill.getHeight() * 0.5f : pill.getX() + pill.getHeight() * 0.5f, pill.getCentreY()});
        juce::DropShadow(juce::Colours::black.withAlpha(0.3f), juce::roundToInt(d * 0.2f), {0, 1}).drawForRectangle(g, th.toNearestInt());
        g.setColour(juce::Colours::white); g.fillEllipse(th);
        if (hover) { g.setColour(T->ink.withAlpha(0.04f)); g.fillRoundedRectangle(r, 6.f); }
    }
};
}  // namespace bridge

}  // namespace kit::v3
