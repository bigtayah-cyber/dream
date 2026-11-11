# Audit 4 : Grain Nord (un-potard, retune, doubles, brume tonale)

*10 nov. 2026. Lecture de `OneKnobModels.h`, `OneKnob.h` et `OneKnobEditor.cpp`. Le moteur « Actuel » a déjà été mesuré (audit 2 n° 3, audit 3 n° 1 : niveau constant à ±0,5 dB). Aucune nouvelle mesure ici. Rien n'a été écouté.*

## Références de la catégorie

- **Baby Audio Super VHS** : un seul contrôle par effet. Sound On Sound regrette l'absence d'une « page arrière » pour les réglages cachés, et un chorus à une seule intensité.
- **Soundtoys MicroShift** : doubles à 3 caractères. Le Focus garde le grave au centre.
- **Soundtoys Little AlterBoy** : hauteur et formant séparés, avec un mix pour fabriquer une harmonie instantanée.
- **Valhalla VintageVerb** : chaque mode de la reverb a sa propre modulation, du chorus au pleurage de bande.

Ce qui fait leur réputation à un potard, c'est la **lisibilité**. Grain Nord a déjà la page arrière (le volet), l'auto-gain et une course étalonnée.

## Ce qui reste bâclé ou manque

1. **Le traitement est mono** (« chaque modèle traite la voix en MONO », `OneKnobModels.h:2`). Une source stéréo est repliée. Le bus d'espace est stéréo, mais pas la voix.
2. **On ne voit pas ce que fait le potard.** L'anneau et la réduction s'affichent, mais pas les étapes : retune, doubles, brume, bande usée. Super VHS fonctionne parce que chaque effet a son étiquette.
3. **La brume tonale se désactive sans prévenir** quand la tonalité n'est pas fiable (elle redevient une reverb normale). Seul l'afficheur de tonalité clignote.
4. **L'écho est une croche fixe** (`tgtD`, ligne 136), quel que soit le tempo. À 160 BPM en trap, une croche fait 187 ms, ce qui est souvent trop serré.
5. Le volet n'offre aucun réglage de **vitesse de retune** : on ne peut pas passer de « naturel » à « dur » sans changer de plugin.

## Améliorations

| # | Amélioration | Impact | Effort | Risque | Change le son |
|---|---|---|---|---|---|
| GN-1 | Sous le potard, une ligne d'étapes qui s'allument au fil de la course (« retune · doubles · brume · bande »), avec la valeur en cours | 4 | S | Nul | Non |
| GN-2 | Bouton « Verrouiller » sur la tonalité détectée (Auto → tonalité fixe, enregistrée dans le projet FL) | 4 | S | Nul | Non |
| GN-3 | Témoin « brume tonale active / en attente de tonalité » | 3 | S | Nul | Non |
| GN-4 | Choix de la division de l'écho dans le volet (1/8, 1/8 pointée, 1/4) | 3 | S | Faible | Non (défaut 1/8) |
| GN-5 | Voix traitée en stéréo lié (analyse sur la somme, gain et hauteur identiques sur G et D) | 3 | M | Moyen | Oui |
| GN-6 | Trois mémoires de position du potard (comme les instantanés A/B), rappelées par clic | 2 | S | Nul | Non |

Micro-UX propres aux un-potard :
- Molette : un cran = 0,1. Avec Maj : 0,01.
- Double-clic : retour à la valeur d'usine du modèle (déjà fait).
- Clic droit : « copier la valeur », « coller », « entrer une valeur ».
- Lisibilité de l'échelle : afficher la course de 0 à 10 avec des repères aux points d'arrêt réels de la courbe, au lieu de graduations régulières.

## Idée signature

**Gel de fin de phrase** : à la fin d'une phrase (le détecteur de Relance est déjà écrit), les résonateurs accordés de la brume tonale tiennent le dernier accord chanté. Ils forment un pad très bas qui s'éteint quand la phrase suivante commence. La voix laisse une nappe dans la tonalité, ce qu'aucun produit cité ne fait en un bouton. Briques : `Relance` (segmentation) et les résonateurs de Grain Nord. Effort S à M.

## Sources

[Super VHS, Sound On Sound](https://www.soundonsound.com/node/4920937) · [MicroShift, Sound On Sound](https://www.soundonsound.com/reviews/soundtoys-microshift) · [Little AlterBoy](https://store.soundtoys.com/product/little-alterboy) · [VintageVerb, les modes](https://valhalladsp.com/?p=634677)
