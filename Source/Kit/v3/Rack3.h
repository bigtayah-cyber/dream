// Kit v3 : socle commun des éditeurs de la suite Orbe migrés en « hybride v2 » (Cabine, Master, EQ, Galet, Accroc, Détect, Mix, Orbe).
// Additif : n'altère ni Kit.h, ni Widgets.h, ni Widgets3.h. Chaque éditeur garde son rendu Classique (T nul) et ajoute Clair / Sombre.
#pragma once
#include "Widgets3.h"

namespace kit::v3::rk {
using juce::Graphics, juce::Rectangle, juce::String, juce::Colour;

// Peau d'un éditeur : thème courant, looks (page / écran sombre), cache de fonds, bouton de thème.
struct Skin {
    Mode mode = Mode::Light;
    Theme T;
    Look3 look3{T}, screenLook{T};
    ArtCache art;
    ThemeMenu btn;
    String key;
    Colour accent;
    Skin(const String& k, Colour a) : key(k), accent(a) {}
    bool v3() const { return mode != Mode::Classic; }
    const Theme* tp() const { return v3() ? &T : nullptr; }
    void set(Mode m) {
        mode = m;
        T = make(m == Mode::Classic ? Mode::Light : m, accent); T.mode = m;
        look3.setTheme(T); screenLook.setTheme(make(Mode::Dark, accent));
        art.clear(); btn.T = &T; btn.current = m;
    }
};

// Taille de fenêtre selon le mode : v3 (d3, échelle 0,75 à 1,5) ou Classique (dc, échelle loC à hiC). Renvoie vrai si la taille est inchangée (appeler resized()).
struct Dims { float w, h; };
inline bool fit(juce::AudioProcessorEditor& ed, bool n3, Dims d3, Dims dc, float loC, float hiC, float& scale) {
    const Dims d = n3 ? d3 : dc;
    const float lo = n3 ? 0.75f : loC, hi = n3 ? 1.5f : hiC;
    ed.setResizeLimits(juce::roundToInt(d.w * lo * 0.8f), juce::roundToInt(d.h * lo * 0.8f), juce::roundToInt(d.w * hi), juce::roundToInt(d.h * hi));
    ed.getConstrainer()->setFixedAspectRatio(d.w / d.h);
    scale = juce::jlimit(lo, hi, scale);
    const int w = juce::roundToInt(d.w * scale), h = juce::roundToInt(d.h * scale);
    if (ed.getWidth() == w && ed.getHeight() == h) return true;
    ed.setSize(w, h); return false;
}

// Marque : « dreamaws » en petites capitales, nom du produit (premier mot en encre, suite en accent), filet d'accent.
inline void brand(Graphics& g, const Theme& T, float u, const String& first, const String& second, float x = 28.f, float y = 30.f, float px = 30.f) {
    g.setColour(T.inkMute); caps(g, "dreamaws", Rectangle<float>((x + 1) * u, (y - 16) * u, 160 * u, 16 * u), 12.f * u, juce::Justification::centredLeft);
    const auto big = font(px * u, true);
    g.setColour(T.ink); tracked(g, first, Rectangle<float>(x * u, y * u, 140 * u, 34 * u), juce::Justification::centredLeft, 2.6f * u, big);
    const float ow = tabularWidth(first.toUpperCase(), big) + (float)first.length() * 2.6f * u + 12.f * u;
    g.setColour(T.accentText); tracked(g, second, Rectangle<float>(x * u + ow, y * u, 300 * u, 34 * u), juce::Justification::centredLeft, 2.6f * u, big);
    g.setColour(T.accent); g.fillRoundedRectangle(Rectangle<float>((x + 2) * u, (y + 36) * u, 30 * u, 3 * u), 1.5f * u);
}
// Légende de section (petites capitales, couleur discrète).
inline void label(Graphics& g, const Colour& c, float u, const String& s, float x, float y, float w, float px = 11.f, juce::Justification j = juce::Justification::centredLeft) {
    g.setColour(c); caps(g, s, Rectangle<float>(x * u, y * u, w * u, 16 * u), px * u, j);
}
// Filet vertical discret entre deux sections d'une carte.
inline void sep(Graphics& g, const Theme& T, float u, float x, float y, float h) {
    g.setColour(T.dark ? juce::Colours::white.withAlpha(0.07f) : Colour(0x14101828)); g.fillRect(Rectangle<float>(x * u, y * u, 1.f, h * u));
}
// Texte tabulaire simple dans un rectangle déjà à l'échelle.
inline void num(Graphics& g, const Colour& c, const String& s, Rectangle<float> r, float px, juce::Justification j = juce::Justification::centred, bool bold = true) {
    g.setColour(c); tabular(g, s, r, font(px, bold), j);
}

// Potard v3 dont le double-clic revient à une valeur de référence (preset) plutôt qu'à la valeur d'origine.
class RefKnob : public Knob3 {
public:
    using Knob3::Knob3;
    std::function<double()> resetTo;
    void mouseDoubleClick(const juce::MouseEvent& e) override {
        if (resetTo) setValue(resetTo(), juce::sendNotificationSync); else Knob3::mouseDoubleClick(e);
    }
};

// Touche à verrouillage (kit::w::Key) dessinée en v3 : pastille à liseré d'accent quand elle est active.
class Key : public kit::w::Key {
public:
    Key(const String& text, const kit::Palette& p, Colour led = kit::w::kLilac) : kit::w::Key(text, p, led), ac(led) {}
    const Theme* T = nullptr;
    void paintButton(Graphics& g, bool hover, bool down) override {
        if (!bridge::v3(T)) { kit::w::Key::paintButton(g, hover, down); return; }
        auto r = getLocalBounds().toFloat().reduced(1.f);
        const bool on = getToggleState();
        const float rad = juce::jmin(r.getHeight() * 0.5f, 14.f);
        Graphics::ScopedSaveState st(g);
        if (!isEnabled()) g.setOpacity(0.5f);
        g.setColour(down ? T->field.darker(0.05f) : hover ? T->field.brighter(0.04f) : T->field); g.fillRoundedRectangle(r, rad);
        g.setColour(on ? T->accent : T->fieldEdge); g.drawRoundedRectangle(r.reduced(0.5f), rad, on ? 1.5f : 1.f);
        const float px = juce::jlimit(9.f, 13.f, r.getHeight() * 0.40f);
        g.setColour(on ? T->accentText : T->ink); caps(g, getButtonText(), r, px, juce::Justification::centred);
    }
private:
    Colour ac;
};

// Vrai si le paramètre est centré sur zéro (arc de valeur tracé depuis le milieu).
inline bool bipolarOf(juce::AudioProcessorValueTreeState& a, const String& id) {
    auto* p = a.getParameter(id);
    if (p == nullptr) return false;
    if (auto* r = dynamic_cast<juce::RangedAudioParameter*>(p)) {
        const auto& nr = r->getNormalisableRange();
        return nr.start < 0.f && nr.end > 0.f && std::fabs(r->convertTo0to1(0.f) - 0.5f) < 0.02f;
    }
    return false;
}

// Outils de capture : thème demandé par la variable d'environnement ORBE_THEME (clair | sombre | classique ; défaut classique), échelle ORBE_SCALE.
inline Mode envMode() {
    const juce::String t = juce::SystemStats::getEnvironmentVariable("ORBE_THEME", "classique");
    return t == "clair" ? Mode::Light : t == "sombre" ? Mode::Dark : Mode::Classic;
}
inline float envScale() { return juce::SystemStats::getEnvironmentVariable("ORBE_SCALE", "1").getFloatValue(); }

// Case à cocher (juce::ToggleButton) : rien à faire, Look3 la dessine en pastille ; ce helper aligne seulement le texte.
}  // namespace kit::v3::rk
