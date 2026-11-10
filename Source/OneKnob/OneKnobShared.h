// Types partagés par la chaîne actuelle (OneKnob.h) et la chaîne « Classique » (ClassicChain.h). C++ pur, sans JUCE.
#pragma once

namespace onek {

// Version de l'état sauvegardé (propriété « stateVersion » de l'arbre APVTS). Absente (0) = projet enregistré avant la refonte :
// il se rouvre avec le moteur « Classique » (ancien son, ancienne latence). Les nouvelles instances démarrent sur le moteur actuel.
constexpr int kStateVersion = 2;
inline bool classicForStateVersion(int version) { return version < 2; }

// Réglages du volet avancé (hors potard principal).
struct Extras {
    float trimDb = 0.f;      // entrée, ±12 dB
    bool autoIn = true;      // auto-entrée : niveau de la voix ramené vers -18 dBFS RMS avant les seuils fixes, rendu en sortie
    float toneDb = 0.f;      // inclinaison grave/aigu, ±6 dB
    float mix = 1.f;         // 0 = sec (retardé de la latence), 1 = traité
    float outDb = 0.f;       // sortie, ±12 dB
    float special = -1.f;    // 0..1 ; < 0 : valeur d'origine du modèle
    bool autoGain = true;    // compensation de volume de sortie
    float option = -1.f;     // 0..1 : option propre au modèle (râle, brume tonale, octave, cible) ; < 0 : valeur d'origine
};

}  // namespace onek
