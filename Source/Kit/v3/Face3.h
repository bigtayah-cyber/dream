// Kit v3 : face d'un plugin conçu directement en « hybride v2 » (Orbe Rythme, Orbe FX), sans seconde face à maintenir.
// Thèmes : Clair (défaut), Sombre, et « Classique » = graphite du rack Orbe, avec les mêmes composants v3 (comme Orbe Relance).
// Regroupe ce que chaque éditeur v3 refait : thème, Look3, cache d'images, bouton de thème, échelle mémorisée (Prefs, hors apvts),
// ratio fixe, limites de taille (menu 0,75 à 1,5 ; poignée à partir de 0,6). Additif : ne modifie rien d'existant.
#pragma once
#include "Widgets3.h"

namespace kit::v3 {

struct Face {
    Face(const juce::String& k, juce::Colour acc, float w, float h) : key(k), accent(acc), bw(w), bh(h) { T = make(Mode::Light, acc); }
    juce::String key; juce::Colour accent; float bw, bh;
    Mode mode = Mode::Light;              // choix de l'utilisateur (Classique compris)
    Theme T;                              // jetons réellement dessinés : jamais en mode Classic, pour que tous les composants restent en v3
    Look3 look3{T};
    ArtCache art;
    ThemeMenu themeBtn;
    float scale = 1.f;
    std::function<void()> onMode;         // l'éditeur ré-attribue (T, art) à ses potards et ponts ici

    // Fin de constructeur : préférences, Look, taille.
    void init(juce::AudioProcessorEditor& e) {
        ed = &e;
        themeBtn.onTheme = [this](Mode m) { setMode(m, true); };
        themeBtn.onScale = [this](float s) { setScale(s); };
        scale = Prefs::scale(key, 1.f);
        ed->setResizable(true, true);
        setMode(Prefs::theme(key, Mode::Light), false);
    }
    void setMode(Mode m, bool persist) {
        mode = m;
        if (m == Mode::Classic) {   // graphite du rack : fonds sombres, capuchons gris, accent conservé
            T = make(Mode::Dark, accent);
            T.bgTop = juce::Colour(0xFF34343A); T.bgBot = juce::Colour(0xFF202024); T.card = juce::Colour(0xFF2A2A2F); T.cardEdge = juce::Colour(0x22FFFFFF);
            T.field = juce::Colour(0xFF1E1E22); T.fieldEdge = juce::Colour(0xFF46464E); T.track = juce::Colour(0xFF3C3C44);
            T.capTop = juce::Colour(0xFF55555C); T.capBot = juce::Colour(0xFF1A1A1D); T.capRim = juce::Colour(0xFF5A5A62);
            T.accentText = ensureContrast(accent.brighter(0.25f), T.card);
            T.inkMute = ensureContrast(T.inkMute, T.card); T.inkFaint = ensureContrast(T.inkFaint, T.card);
        } else {
            T = make(m, accent);
        }
        look3.setTheme(T); art.clear();
        ed->setLookAndFeel(&look3);
        themeBtn.T = &T; themeBtn.current = m;
        if (persist) Prefs::setTheme(key, m);
        if (onMode) onMode();
        ed->setResizeLimits(juce::roundToInt(bw * 0.6f), juce::roundToInt(bh * 0.6f), juce::roundToInt(bw * 1.5f), juce::roundToInt(bh * 1.5f));
        ed->getConstrainer()->setFixedAspectRatio(bw / bh);
        applySize();
        ed->sendLookAndFeelChange();
        ed->repaint();
    }
    void setScale(float s) { scale = juce::jlimit(0.75f, 1.5f, s); Prefs::setScale(key, scale); applySize(); }
    // resized() : u = largeur / largeur de référence ; mémorise l'échelle tirée à la poignée.
    float onResized() { const float u = (float)ed->getWidth() / bw; scale = u; themeBtn.scale = u; return u; }
    void shutdown() { Prefs::setScale(key, scale); if (ed != nullptr) ed->setLookAndFeel(nullptr); }
    const char* artKey() const { return modeKey(mode); }

private:
    void applySize() {
        scale = juce::jlimit(0.75f, 1.5f, scale);
        const int w = juce::roundToInt(bw * scale), h = juce::roundToInt(bh * scale);
        if (ed->getWidth() != w || ed->getHeight() != h) ed->setSize(w, h); else ed->resized();
    }
    juce::AudioProcessorEditor* ed = nullptr;
};

}  // namespace kit::v3
