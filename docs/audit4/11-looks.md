# Audit 4 : Orbe Looks (27 looks × 4 macros)

*10 nov. 2026. Lecture de `Looks.h`, `OrbeChain.h` et `LooksProcessor.cpp`. Les rendus sont déjà mesurés dans l'audit 3 (A2 à A5). Rien n'a été écouté.*

## Références de la catégorie

- **iZotope Nectar 4** :
  - le **Vocal Assistant** génère un point de départ, puis donne des intentions (forme, intensité, effets, largeur, voix, choristes) ;
  - modules réordonnables ;
  - affichages tête haute (« heads-up displays ») ;
  - **Auto-Level** comme alternative au compresseur, avec force, plage et forme d'onde affichée.
- **Waves Vocal Rider** : niveau automatique avec une cible et une plage, une entrée sidechain « musique » pour suivre le beat, et l'**écriture de l'automation** dans la piste pour pouvoir la retoucher.

## Constats (au-delà de l'audit 3)

1. **27 looks dans une liste**, avec des catégories mais sans favoris, sans recherche, sans tags et sans aperçu. À partir d'une vingtaine d'éléments, les navigateurs pro (soothe2, Nectar) ajoutent tags, recherche et favoris. Avec soothe2, « Annuler » rétablit le réglage d'avant quand on parcourt les presets.
2. **Changer de look remet les 4 macros à leurs valeurs d'usine** (`LooksProcessor.cpp:54`). Si l'artiste a réglé « Espace 2 » pour son morceau, il perd ce réglage à chaque look essayé. Il ne peut donc pas comparer les looks avec **ses** macros. Le changement de look fait aussi sauter le gain calibré en 20 ms (`OrbeChain.h:110`). Ce qui est déjà bien : le plugin sait si les macros ont été modifiées (`:58`), donc l'indicateur « modifié » est facile à afficher.
3. **Les macros affichent « 0 à 10 », sans unité ni effet visible.** Ce point est déjà relevé dans l'audit 3 (ligne de valeurs sous chaque macro).
4. **On ne peut pas ouvrir un look dans l'Atelier.** Les presets de l'Atelier sont pourtant les looks aux macros d'usine (`AtelierParams.h:84-86`). Il manque le chemin « j'aime ce look, je veux le retoucher » avec les macros **courantes**.
5. **Le niveau automatique est opaque.** Rien ne montre de combien il corrige. Vocal Rider et l'Auto-Level de Nectar affichent leur action.

## Améliorations

| # | Amélioration | Impact | Effort | Risque | Change le son |
|---|---|---|---|---|---|
| LO-1 | Navigateur de looks : favoris (étoile), filtre par catégorie, recherche, « Annuler » pendant le parcours, flèches qui ne parcourent que les favoris si l'on veut | 4 | M | Nul | Non |
| LO-2 | « Ouvrir dans l'Atelier » : copie l'état complet `oc::S` du look et des macros courantes dans le presse-papiers, avec « Coller » dans l'Atelier | 4 | S | Nul | Non |
| LO-3 | Jauge du niveau automatique (±dB) à côté de la sortie, et option « Figer » (gain statique, voir A4) | 4 | S | Faible | Non |
| LO-4 | Cadenas « Garder mes macros » : quand on parcourt les looks, les macros de l'utilisateur restent en place ; un astérisque signale un look modifié | 4 | S | Faible | Non |
| LO-5 | Écoute « delta » globale (traité moins sec aligné), pour entendre ce que le look ajoute | 3 | S | Nul | Non |
| LO-6 | Rangée de modules allumés (E C S T D 2 Y R, déjà gravés dans `mods`) devenue cliquable : solo et coupure d'un module, sans quitter Looks | 3 | M | Faible | Non |

## Idée signature

**Fondu de looks** : un cinquième contrôle, « Vers », fait glisser d'un look A à un look B. Par exemple, Béton au couplet et Turbine au refrain, avec une seule automation dans FL.

- Briques : chaque look produit un `oc::S` (`Look::map`). On interpole les grandeurs continues, et les interrupteurs basculent au milieu avec un fondu de 20 ms.
- Nectar change de style, il ne fond pas deux chaînes.
- Effort M. Il faudra des règles de bascule pour certaines combinaisons (doubles à +12 contre doubles fins).

## Sources

[Nectar 4, fonctions](https://www.izotope.com/en/products/nectar/features) · [Vocal Rider in Depth, Waves](https://www.waves.com/vocal-rider-in-depth) · [Manuel de soothe2 (presets, annuler)](https://oeksound.com/manuals/soothe2/)