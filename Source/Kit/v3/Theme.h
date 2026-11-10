// Kit v3 « hybride v2 » : jetons de thème (Clair, Sombre ; Classique = ancien rendu, géré par l'éditeur),
// accent de famille, contraste WCAG, typographie unique (IBM Plex Sans Condensed) à chiffres tabulaires,
// réglages d'interface mémorisés hors apvts (fichier utilisateur, tolérant aux erreurs).
// Additif : ne modifie ni Kit.h ni Widgets.h ; s'appuie seulement sur leurs polices et leurs utilitaires.
#pragma once
#include "../Kit.h"
#include "Format.h"

namespace kit::v3 {
using juce::Colour;

enum class Mode { Light = 0, Dark = 1, Classic = 2 };
inline const char* modeKey(Mode m) { return m == Mode::Light ? "clair" : m == Mode::Dark ? "sombre" : "classique"; }
inline juce::String modeName(Mode m) { return u8(m == Mode::Light ? "Clair" : m == Mode::Dark ? "Sombre" : "Classique"); }

// Couleurs d'accent de famille (graphiques) ; la version « texte » est dérivée pour tenir 4,5 : 1.
namespace accents {
inline const Colour orange(0xFFFF5A36);   // Voile, Orbe, Autotune
inline const Colour lilac(0xFF8F7CFF), ice(0xFF3FB6E8), mint(0xFF2FBF87), peach(0xFFFF8A5C), amber(0xFFF2A516);
}

// ------------------------------------------------------------ contraste WCAG 2.x
inline float luminance(Colour c) {
    auto ch = [](float v) { return v <= 0.03928f ? v / 12.92f : std::pow((v + 0.055f) / 1.055f, 2.4f); };
    return 0.2126f * ch(c.getFloatRed()) + 0.7152f * ch(c.getFloatGreen()) + 0.0722f * ch(c.getFloatBlue());
}
inline float contrast(Colour a, Colour b) {
    const float la = luminance(a), lb = luminance(b);
    return (std::max(la, lb) + 0.05f) / (std::min(la, lb) + 0.05f);
}
// Assombrit (fond clair) ou éclaircit (fond sombre) fg jusqu'à atteindre `ratio` sur bg.
inline Colour ensureContrast(Colour fg, Colour bg, float ratio = 4.6f) {
    const bool darken = luminance(bg) > 0.18f;
    for (int i = 0; i < 40 && contrast(fg, bg) < ratio; ++i) fg = darken ? fg.darker(0.06f) : fg.brighter(0.06f);
    return fg;
}

// ------------------------------------------------------------ jetons
struct Theme {
    Mode mode = Mode::Light;
    bool dark = false;
    Colour bgTop, bgBot, card, cardEdge, shadow;
    Colour ink, inkMute, inkFaint;                  // texte : principal, étiquettes, unités (tous ≥ 4,5 : 1 sur carte et fond)
    Colour accent, accentText, mod;                 // accent graphique, accent pour du texte, modulation / automation
    Colour track, tick, field, fieldEdge;           // piste des arcs, graduations, champs (listes, pilules)
    Colour capTop, capBot, capRim, capPeachTop, capPeachBot;   // capuchons de potards (satiné ; « pêche » pour MIX/SORTIE)
    // écran héros : toujours sombre (identité), identique dans les deux thèmes
    Colour scrTop{0xFF161B3A}, scrBot{0xFF0A0D1C}, scrHalo{0xFF7A5CFF}, scrInk{0xFFE8EBF7}, scrMute{0xFFA9B0D0}, scrGrid{0x1AFFFFFF};
    Colour sung{0xFF6FD6FF}, out{0xFFFF8A5C}, target{0xFFE8EBF7};
    bool isClassic() const { return mode == Mode::Classic; }
};

inline Theme make(Mode m, Colour accent = accents::orange) {
    Theme t; t.mode = m; t.dark = m == Mode::Dark; t.accent = accent;
    if (!t.dark) {
        t.bgTop = Colour(0xFFF6F7F9); t.bgBot = Colour(0xFFE9ECF1); t.card = Colour(0xFFFFFFFF); t.cardEdge = Colour(0x14101828); t.shadow = Colour(0x22101828);
        t.ink = Colour(0xFF15171C); t.inkMute = Colour(0xFF515662); t.inkFaint = Colour(0xFF60656F);
        t.mod = Colour(0xFF2D6CDF); t.track = Colour(0xFFDDE1E8); t.tick = Colour(0xFFB4BAC5); t.field = Colour(0xFFF4F6F9); t.fieldEdge = Colour(0xFFD3D8E0);
        t.capTop = Colour(0xFFFFFFFF); t.capBot = Colour(0xFFE3E7EE); t.capRim = Colour(0xFFC9CFD8);
        t.capPeachTop = Colour(0xFFFFE3D8); t.capPeachBot = Colour(0xFFF6B39C);
    } else {
        t.bgTop = Colour(0xFF12141A); t.bgBot = Colour(0xFF0C0E12); t.card = Colour(0xFF1A1D25); t.cardEdge = Colour(0x14FFFFFF); t.shadow = Colour(0x80000000);
        t.ink = Colour(0xFFF2F3F6); t.inkMute = Colour(0xFFA9AFBD); t.inkFaint = Colour(0xFF9399A8);
        t.mod = Colour(0xFF7FD8FF); t.track = Colour(0xFF2C303B); t.tick = Colour(0xFF4A505E); t.field = Colour(0xFF14161C); t.fieldEdge = Colour(0xFF2F3440);
        t.capTop = Colour(0xFF3A3F4B); t.capBot = Colour(0xFF1D2028); t.capRim = Colour(0xFF4C5260);
        t.capPeachTop = Colour(0xFF8A4632); t.capPeachBot = Colour(0xFF4A231A);
    }
    // texte d'accent et textes discrets : contraste mesuré, garanti sur la carte ET sur le fond le plus défavorable
    auto fix = [&](Colour c) { c = ensureContrast(c, t.card); c = ensureContrast(c, t.bgBot); return ensureContrast(c, t.field); };
    t.accentText = fix(t.dark ? accent.brighter(0.25f) : accent.darker(0.55f));
    t.inkMute = fix(t.inkMute); t.inkFaint = fix(t.inkFaint);
    return t;
}

// ------------------------------------------------------------ typographie : une seule famille
// px = hauteur de police JUCE en pixels logiques ; jamais sous 9 px.
inline juce::Font font(float px, bool bold = false) {
    auto* F = Fonts::getInstance();
    return typefaceFont(bold ? F->sansBold : F->sans, juce::jmax(9.f, px));
}
// Chiffres tabulaires : chaque chiffre occupe la chasse du « 0 » (les valeurs ne dansent pas).
inline float tabularWidth(const juce::String& s, const juce::Font& f) {
    const float dw = glyphWidth(f, '0');
    float w = 0.f;
    for (auto c : s) w += (c >= '0' && c <= '9') ? dw : glyphWidth(f, c);
    return w;
}
inline void tabular(juce::Graphics& g, const juce::String& s, juce::Rectangle<float> r, const juce::Font& f, juce::Justification j) {
    g.setFont(f);
    const float dw = glyphWidth(f, '0'), w = tabularWidth(s, f);
    float x = j.testFlags(juce::Justification::right) ? r.getRight() - w : j.testFlags(juce::Justification::horizontallyCentred) ? r.getCentreX() - w * 0.5f : r.getX();
    for (auto c : s) {
        const bool dig = c >= '0' && c <= '9';
        const float cw = dig ? dw : glyphWidth(f, c);
        g.drawText(juce::String::charToString(c), juce::Rectangle<float>(x - 4.f, r.getY(), cw + 8.f, r.getHeight()), juce::Justification::centred, false);
        x += cw;
    }
}
// Petites capitales espacées (titres, libellés de potards) : espacement proportionnel, jamais serré.
inline void caps(juce::Graphics& g, const juce::String& s, juce::Rectangle<float> r, float px, juce::Justification j, bool bold = true) {
    const auto f = font(px, bold);
    tracked(g, s, r, j, juce::jmax(0.6f, f.getHeight() * 0.14f), f);
}

// ------------------------------------------------------------ réglages d'interface (hors apvts)
// Fichier utilisateur « dreamaws/ui.settings » : thème et échelle par plugin. Toute erreur -> valeurs par défaut.
class Prefs {
public:
    static Mode theme(const juce::String& plugin, Mode def = Mode::Light) {
        const auto s = get(plugin + ".theme");
        for (auto m : {Mode::Light, Mode::Dark, Mode::Classic}) if (s == modeKey(m)) return m;
        return def;
    }
    static void setTheme(const juce::String& plugin, Mode m) { set(plugin + ".theme", modeKey(m)); }
    static float scale(const juce::String& plugin, float def = 1.f) {
        const auto v = get(plugin + ".scale").getFloatValue();
        return (std::isfinite(v) && v >= 0.5f && v <= 2.5f) ? v : def;
    }
    static void setScale(const juce::String& plugin, float s) { set(plugin + ".scale", juce::String(s, 3)); }
    // Écriture des notes (dièses / bémols) d'un plugin sans tonalité propre : réglage d'interface, pas d'état de projet. Défaut : dièses.
    static bool flats(const juce::String& plugin) { return get(plugin + ".flats") == "1"; }
    static void setFlats(const juce::String& plugin, bool f) { set(plugin + ".flats", f ? "1" : "0"); }

private:
    // Fichier ouvert à la demande (appels rares : ouverture de l'éditeur, changement de thème, fermeture) :
    // aucune instance statique, donc rien à détruire après l'arrêt de JUCE.
    static juce::PropertiesFile::Options options() {
        juce::PropertiesFile::Options o;
        o.applicationName = "ui"; o.filenameSuffix = ".settings"; o.folderName = "dreamaws";
        o.osxLibrarySubFolder = "Application Support"; o.storageFormat = juce::PropertiesFile::storeAsXML;
        o.millisecondsBeforeSaving = -1;   // sauvegarde explicite
        return o;
    }
    static juce::String get(const juce::String& k) { juce::PropertiesFile f(options()); return f.isValidFile() ? f.getValue(k) : juce::String(); }
    static void set(const juce::String& k, const juce::String& v) {
        juce::PropertiesFile f(options());
        if (!f.isValidFile() || f.getValue(k) == v) return;
        f.setValue(k, v); f.saveIfNeeded();   // échec d'écriture (disque plein, droits) : ignoré, le réglage vaut pour la session
    }
};

}  // namespace kit::v3
