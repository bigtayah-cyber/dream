#pragma once
#include "../OrbeChain/ChainUi.h"

namespace atl {
enum Kind { F, B, C };
struct PDef { const char* id; const char* name; Kind kind; float lo, hi, step, centre; const char* unit; float oc::S::*field; const char* tip; };

// Table de tous les réglages de modules. Les interrupteurs (B) valent 0/1, les choix (C) sont des indices.
inline const PDef kP[] = {
    {"tnOn", "Tune", B, 0, 1, 1, 0, "", &oc::S::tnOn, "Correction de hauteur et décalage."},
    {"tnAmt", "Force", F, 0, 1, 0.01f, 0, "%", &oc::S::tnAmt, "Part de l'écart de hauteur corrigée : 100 % = note collée à la gamme."},
    {"tnSpeed", "Vitesse", F, 1, 300, 1, 30, "ms", &oc::S::tnSpeed, "Temps de rattrapage : 1 à 5 ms = effet dur, 10 à 25 ms = mélodique, plus de 50 ms = discret."},
    {"shift", "Décalage", F, -12, 12, 0.01f, 0, "st", &oc::S::shift, "Décalage de hauteur de la voix principale, en demi-tons."},
    {"key", "Tonalité", C, 0, 12, 1, 0, "", &oc::S::key, "Tonalité de la correction. Auto : aucune correction tant que la tonalité n'est pas sûre."},
    {"scale", "Mode", C, 0, 1, 1, 0, "", &oc::S::scale, "Mode utilisé quand la tonalité est choisie à la main."},
    {"eqOn", "EQ", B, 0, 1, 1, 0, "", &oc::S::eqOn, "Égaliseur : passe-haut et quatre bandes."},
    {"hp", "Passe-haut", F, 20, 400, 1, 90, "Hz", &oc::S::hp, "Passe-haut : retire les graves inutiles."},
    {"lsF", "Graves Hz", F, 60, 400, 1, 150, "Hz", &oc::S::lsF, "Fréquence du plateau grave."},
    {"lsG", "Graves dB", F, -15, 15, 0.1f, 0, "dB", &oc::S::lsG, "Gain du plateau grave."},
    {"b1F", "Méd-1 Hz", F, 150, 2000, 1, 400, "Hz", &oc::S::b1F, "Fréquence de la cloche bas-médium (le « câble » de la voix)."},
    {"b1G", "Méd-1 dB", F, -15, 15, 0.1f, 0, "dB", &oc::S::b1G, "Gain de la cloche bas-médium."},
    {"b1Q", "Méd-1 Q", F, 0.3f, 6, 0.01f, 1, "q", &oc::S::b1Q, "Largeur de la cloche bas-médium : grand Q = étroit."},
    {"b2F", "Méd-2 Hz", F, 1000, 9000, 1, 3000, "Hz", &oc::S::b2F, "Fréquence de la cloche haut-médium (présence)."},
    {"b2G", "Méd-2 dB", F, -15, 15, 0.1f, 0, "dB", &oc::S::b2G, "Gain de la cloche haut-médium."},
    {"b2Q", "Méd-2 Q", F, 0.3f, 6, 0.01f, 1, "q", &oc::S::b2Q, "Largeur de la cloche haut-médium."},
    {"hsF", "Aigus Hz", F, 4000, 16000, 10, 9000, "Hz", &oc::S::hsF, "Fréquence du plateau aigu."},
    {"hsG", "Aigus dB", F, -15, 15, 0.1f, 0, "dB", &oc::S::hsG, "Gain du plateau aigu : air ou voile."},
    {"cpOn", "Compresseur", B, 0, 1, 1, 0, "", &oc::S::cpOn, "Compresseur de voix."},
    {"thr", "Seuil", F, -60, 0, 0.1f, 0, "dB", &oc::S::thr, "Niveau à partir duquel le compresseur agit."},
    {"ratio", "Ratio", F, 1, 20, 0.1f, 4, ":1", &oc::S::ratio, "Taux de compression."},
    {"atk", "Attaque", F, 0.5f, 100, 0.1f, 10, "ms", &oc::S::atk, "Vitesse d'attaque : lente = plus de punch."},
    {"rel", "Relâche", F, 10, 800, 1, 120, "ms", &oc::S::rel, "Vitesse de relâchement."},
    {"mk", "Gain", F, -6, 24, 0.1f, 0, "dB", &oc::S::mk, "Gain de compensation après compression."},
    {"cpMix", "Parallèle", F, 0, 1, 0.01f, 0, "%", &oc::S::cpMix, "Part du signal comprimé mélangée au sec."},
    {"saOn", "Saturation", B, 0, 1, 1, 0, "", &oc::S::saOn, "Saturation douce et dégradation numérique."},
    {"drive", "Drive", F, 0, 1, 0.01f, 0, "x10", &oc::S::drive, "Quantité de saturation."},
    {"bias", "Biais", F, 0, 1, 0.01f, 0, "x10", &oc::S::bias, "Asymétrie de la saturation : harmoniques paires."},
    {"crush", "Crush", F, 0, 1, 0.01f, 0, "x10", &oc::S::crush, "Réduction de résolution et de fréquence d'échantillonnage."},
    {"saMix", "Mix", F, 0, 1, 0.01f, 0, "%", &oc::S::saMix, "Part du signal saturé."},
    {"dsOn", "De-ess", B, 0, 1, 1, 0, "", &oc::S::dsOn, "Atténue les sifflantes."},
    {"dsF", "Fréquence", F, 3000, 12000, 10, 6500, "Hz", &oc::S::dsF, "Fréquence de coupure de la détection des sifflantes."},
    {"dsAmt", "Force", F, 0, 1, 0.01f, 0, "%", &oc::S::dsAmt, "Force de l'atténuation."},
    {"dsThr", "Seuil", F, -50, -10, 0.1f, 0, "dB", &oc::S::dsThr, "Niveau des aigus à partir duquel le de-esser agit."},
    {"dbOn", "Doubleur", B, 0, 1, 1, 0, "", &oc::S::dbOn, "Deux voix décalées en hauteur, ouvertes en stéréo."},
    {"dbMix", "Niveau", F, 0, 1, 0.01f, 0, "%", &oc::S::dbMix, "Niveau des doubles."},
    {"dbA", "Voix A", F, -12, 12, 0.01f, 0, "st", &oc::S::dbA, "Décalage de la voix A en demi-tons : petit = épaississement, 12 = octave."},
    {"dbB", "Voix B", F, -12, 12, 0.01f, 0, "st", &oc::S::dbB, "Décalage de la voix B en demi-tons."},
    {"dbWide", "Largeur", F, 0, 1, 0.01f, 0, "%", &oc::S::dbWide, "Ouverture stéréo des doubles."},
    {"dyOn", "Delay", B, 0, 1, 1, 0, "", &oc::S::dyOn, "Écho ping-pong, atténué sous la voix."},
    {"dyMs", "Temps", F, 20, 1500, 1, 250, "ms", &oc::S::dyMs, "Temps d'écho libre (utilisé quand Division est sur Libre)."},
    {"dyDiv", "Division", C, 0, 5, 1, 0, "", &oc::S::dyDiv, "Division du tempo de l'hôte. Libre : le temps en ms."},
    {"dyFb", "Feedback", F, 0, 0.9f, 0.01f, 0, "%", &oc::S::dyFb, "Nombre de répétitions."},
    {"dyMix", "Niveau", F, 0, 1, 0.01f, 0, "%", &oc::S::dyMix, "Niveau des échos."},
    {"dyDuck", "Duck", F, 0, 1, 0.01f, 0, "%", &oc::S::dyDuck, "Les échos baissent quand la voix chante."},
    {"rvOn", "Reverb", B, 0, 1, 1, 0, "", &oc::S::rvOn, "Réverbération à réseau de retards."},
    {"rvSize", "Durée", F, 0.2f, 8, 0.01f, 1.5f, "s", &oc::S::rvSize, "Durée de la queue de réverbération."},
    {"rvPre", "Pré-delay", F, 0, 150, 1, 0, "ms", &oc::S::rvPre, "Retard avant la réverbération : garde la voix devant."},
    {"rvDamp", "Amorti", F, 0, 0.95f, 0.01f, 0, "%", &oc::S::rvDamp, "Absorption des aigus : haut = salle sombre."},
    {"rvMix", "Niveau", F, 0, 1, 0.01f, 0, "%", &oc::S::rvMix, "Niveau de la réverbération."},
    {"lmOn", "Limiteur", B, 0, 1, 1, 0, "", &oc::S::lmOn, "Limiteur de sortie à anticipation : aucun dépassement du plafond."},
    {"lmCeil", "Plafond", F, -12, 0, 0.1f, 0, "dB", &oc::S::lmCeil, "Niveau maximal de sortie."},
    {"mix", "Mix", F, 0, 1, 0.01f, 0, "%", &oc::S::mix, "Sec / traité : 100 % = voix entièrement traitée."},
    {"out", "Sortie", F, -24, 12, 0.1f, 0, "dB", &oc::S::out, "Niveau de sortie, avant le limiteur."},
};
inline const PDef* find(const juce::String& id) { for (auto& d : kP) if (id == d.id) return &d; return nullptr; }
inline juce::StringArray choices(const juce::String& id) {
    if (id == "key") { juce::StringArray a; a.add("Auto"); for (auto* n : oc::ui::kNotes) a.add(n); return a; }
    if (id == "scale") return {kit::u8("Majeur"), kit::u8("Mineur")};
    return {"Libre", "1/16", "1/8", kit::u8("1/8 pointée"), "1/4", "1/2"};
}
inline juce::String fmt(const PDef& d, float v) {
    using juce::String; const String u(d.unit);
    if (u == "Hz") return v >= 1000.f ? String(v / 1000.f, 2) + " kHz" : String(juce::roundToInt(v)) + " Hz";
    if (u == "dB") return String(v, 1) + " dB";
    if (u == "ms") return v >= 100.f ? String(juce::roundToInt(v)) + " ms" : String(v, 1) + " ms";
    if (u == ":1") return String(v, 1) + ":1";
    if (u == "s") return String(v, 2) + " s";
    if (u == "%") return String(juce::roundToInt(v * 100.f)) + " %";
    if (u == "st") return String(v, 2) + " st";
    if (u == "q") return "Q " + String(v, 2);
    return String(v * 10.f, 1);
}
// Presets : 0 = neutre (tout coupé), 1..10 = les looks d'Orbe Looks aux macros d'origine.
inline int numPresets() { return oc::numLooks() + 1; }
inline oc::S presetSettings(int i) { oc::S s; if (i > 0) s = oc::lookDefaults(i - 1); return s; }
inline juce::String presetName(int i) { return i <= 0 ? kit::u8("Neutre") : kit::u8(oc::looks()[(size_t)juce::jlimit(1, oc::numLooks(), i) - 1].name); }
inline juce::String presetTag(int i) { return i <= 0 ? kit::u8("Tous les modules coupés") : kit::u8(oc::looks()[(size_t)juce::jlimit(1, oc::numLooks(), i) - 1].tag); }
inline bool presetSkips(const juce::String& id) { return id == "mix" || id == "key" || id == "scale"; }  // réglages de session, jamais écrasés par un preset
}  // namespace atl
