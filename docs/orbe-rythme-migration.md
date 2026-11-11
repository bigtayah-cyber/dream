# Orbe, Orbe Rythme et Orbe FX : ce qui change

## Orbe devient deux plugins

- **Orbe** : accords, lead et basse (MIDI). Même identifiant de plugin (`Orbe`, `com.dreamaws.orbe`), mêmes identifiants apvts, même moteur. Les anciens états se relisent sans changement. Il n'envoie plus de batterie : la piste 3 (batterie) reste dans le moteur et dans l'état (`n3`, `vol3`, `mute3`) pour que rien ne se perde, mais elle n'est ni jouée, ni affichée, ni exportée. Le MIDI sort sur les canaux 1, 2 et 3 (accords, lead, basse).
- **Orbe Rythme** (nouveau, `ORyt`, `com.dreamaws.orberythme`) : batterie et rythmes en MIDI (canal 10, notes GM), aperçu sonore, grille éditable, glisser du fichier MIDI. Le moteur est celui d'Orbe (`OrbeCore.h`, `OrbeSynth.h`).

## Migration des batteries d'un ancien état

Orbe Rythme relit l'état d'un ancien Orbe (balise `ORBE`) : style, mesures, humain, `vol3` (devient Niveau), `mute3` (devient Muet), sortie MIDI, master, et le contexte harmonique (tonique, gamme, septième, add9, emprunts, arpège, densité, graine, progression). La batterie du moteur dépend du tirage des accords et de la basse (même flux aléatoire) : Orbe Rythme garde donc ce contexte pour régénérer la **même** batterie. Si la batterie avait été modifiée à la main ou verrouillée, les frappes enregistrées (`n3`) sont reprises telles quelles.

Vérifié par `orberythme_test` : 18 combinaisons style / mesures redonnent une batterie identique frappe pour frappe.

Limites : le jitter d'humanisation est recalculé à la lecture (même formule), l'automation de l'ancien `vol3` / `mute3` dans le DAW ne suit pas (nouveaux paramètres `level` / `mute`). Un état d'un autre plugin est ignoré proprement.

## Orbe FX : un vrai effet audio

L'ancien « Orbe FX » était le moteur d'Orbe déclaré comme effet : il ignorait l'audio reçu. Il est remplacé (mêmes `OrbF` et `com.dreamaws.orbefx`, catégorie Fx / AU Effect). **Un ancien état d'Orbe FX est ignoré** (le plugin garde ses réglages d'usine) : il n'avait aucun réglage de son à conserver.

Chaîne, dans l'ordre : filtre (passe-bas / bande / haut, résonance, suivi d'enveloppe) -> saturation (doux / dur / asymétrique, anti-repliement ADAA, ton, mix) -> chorus d'ensemble à 3 voix (vitesse, profondeur, largeur, mix) -> delay calé sur le tempo (stéréo ou ping-pong, réinjection bornée, ton) -> réverbe FDN à 8 lignes (taille, durée, pré-retard, amorti). Puis Mix et Sortie.

Macros (s'ajoutent aux modules, n'écrivent aucun paramètre) : Couleur (bipolaire, tilt et filtre), Grain, Mouvement, Espace. 12 préréglages (un préréglage remet aussi les macros à zéro). Bypass : paramètre « bypass » (choix à 2 états), fondu de 10 ms, latence 0, mix et sortie après la chaîne. DSP original (Cytomic / Zavalishin pour le filtre), sans code sous licence GPL.

## Intégration

CI : « Orbe Rythme » ajouté aux listes de vérification et au paquet des instruments (macOS et Windows) ; le garde passe de 26 à 27 plugins. Les installateurs (`installer/`) copient tous les `*.vst3` / `*.component` : rien à modifier.
