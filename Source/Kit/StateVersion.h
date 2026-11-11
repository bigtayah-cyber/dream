// Numéro de version de l'état sauvegardé (propriété « stateVersion » de l'arbre APVTS). Aucun identifiant de paramètre n'est touché.
//
// Patron : à l'écriture, copier l'arbre (ou apvts.state dans beforeSave) et appeler kit::stampState ; à la lecture, relever
// kit::stateVersionOf(arbre) AVANT replaceState si une migration est nécessaire. Absente (0) = projet enregistré avant la
// vague 2 ; 2 = vague 2 (nouvelle reverb, saturation compensée). Le prochain changement audible passera à 3 et testera
// « version < 3 » pour garder l'ancien chemin, comme le fait onek::classicForStateVersion pour les un-potard.
// Un état ancien se relit toujours : l'absence de propriété n'est pas une erreur.
#pragma once
#include <juce_data_structures/juce_data_structures.h>

namespace kit {

constexpr int kStateVersion = 2;
// Version « DSP » 3 (lot E : studio et instruments) : les nouvelles instances de Synth, 808, Cabine, Master... l'écrivent ; un état d'avant
// (absente ou 2) garde l'ancien chemin audio là où le son change (voir docs/audit4/lotE-etat.md). N'affecte aucun autre plugin.
constexpr int kStateVersionDsp = 3;

inline void stampState(juce::ValueTree& tree, int version = kStateVersion) { tree.setProperty("stateVersion", version, nullptr); }
inline int stateVersionOf(const juce::ValueTree& tree) { return (int)tree.getProperty("stateVersion", 0); }

}  // namespace kit
