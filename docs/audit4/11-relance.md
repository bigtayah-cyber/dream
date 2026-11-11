# Audit 4 : Orbe Relance (réponses automatiques de fin de phrase)

*10 nov. 2026. Lecture de `RelanceDsp.h` et `RelanceProcessor.cpp`. Le moteur est sans allocation, avec 16 préréglages et une version d'état écrite. L'audit 3 a déjà relevé la graine en flottant et les « % abstraits », et proposé les ad-libs auto (V8). Rien n'a été écouté.*

## Références de la catégorie

- **iZotope Stutter Edit 2** :
  - des centaines de « Gestures » rangés en banques ;
  - **Auto Mode** (un bouton déclenche un geste) et **déclenchement MIDI** (on joue les effets au clavier) ;
  - calage sur le tempo (Rate, Quantization) ;
  - 12 courbes, plus des courbes à dessiner.
- **Nectar 4** : Voices (jusqu'à 8 voix d'harmonie, 11 styles, mode MIDI) et Backer (choristes, 8 personnages).
- **Comeback Kid** : Destiny (du hasard dosé), sorties sec et traité séparées.

Relance est **original** : aucune des références ne répond *aux fins de phrase* par des fragments de la phrase elle-même. Ce qui le sépare d'un produit pro, c'est le **contrôle** et la **justesse**.

## Constats nouveaux

1. **L'harmonie sort de la gamme.** Le type Harmonie applique une quinte juste fixe, ou l'octave (`RelanceDsp.h:287`, `exp2(7/12)`). En Do majeur, la quinte d'un Si donne un **Fa#**, hors gamme : une phrase qui finit sur le 7e degré reçoit une harmonie fausse. Les harmoniseurs (Voices, Harmony Engine) raisonnent en intervalles de la gamme. Relance n'a ni tonalité ni détection.
2. **On ne voit pas les décisions.** Le hasard choisit à chaque phrase le caractère, le retard et les répétitions. Si une réponse est ratée, on ne peut que changer toute la graine.
3. **Pas de déclenchement manuel.** Stutter Edit se joue au MIDI. Relance n'a pas d'entrée MIDI ; dans FL, le moyen le plus simple serait un paramètre automatisable « Relancer maintenant ».
4. **Les relances ne sont pas exportables.** Dans FL, l'artiste voudra déplacer ou couper une relance dans la playlist. Le mode « Relances seules » existe et c'est bien pour le router vers le mixer, mais il reste temps réel.

## Améliorations

| # | Amélioration | Impact | Effort | Risque | Change le son |
|---|---|---|---|---|---|
| RE-1 | Harmonie diatonique : tierce, quinte ou sixte de la gamme, selon une tonalité Auto (histogramme YIN, la méthode de Voile) ou imposée (`key`, `scale` en fin de liste) | 4 | S | Faible | Oui (version d'état 2) |
| RE-2 | Ligne de temps des 8 dernières phrases, avec la réponse choisie pour chacune (icône du caractère, retard), et clic sur une phrase pour **relancer ce dé seulement** (graine par phrase gardée dans l'état) | 4 | M | Faible | Non |
| RE-3 | Paramètre « Forcer » automatisable (choix à 2 états, pas un booléen, selon la règle de la suite) : la prochaine fin de phrase répond à coup sûr | 3 | S | Nul | Non |
| RE-4 | Graine : bouton dé et champ entier (déjà demandé dans l'audit 3, A7) | 3 | S | Faible | Non |
| RE-5 | « Écouter le prochain » : prévisualisation de la réponse sur la dernière phrase capturée, lecture arrêtée | 2 | M | Faible | Non |

## Idée signature

**Glisser les relances** : comme le `.MID` de Fredonne, une poignée « .WAV » exporte les relances de la dernière lecture dans un fichier aligné sur la mesure, en relances seules. On le glisse dans la playlist de FL pour les couper, les déplacer et les automatiser.

- Les briques existent : la capture interne et le glisser de fichiers (`performExternalDragDropOfFiles`, dans Fredonne).
- Le hasard devient une **proposition d'ad-libs** que l'artiste monte ensuite, ce qu'aucun générateur cité ne propose. Stutter Edit et Voices restent temps réel.
- Effort M (tampon de lecture et écriture WAV hors du fil audio).

## Sources

[Stutter Edit 2, fonctions (iZotope)](https://www.izotope.com/en/products/stutter-edit/features) · [Nectar 4, fonctions](https://www.izotope.com/en/products/nectar/features) · [Comeback Kid, Tape Op](https://tapeop.com/reviews/gear/137/comeback-kid-delay-echo-plug-in)
