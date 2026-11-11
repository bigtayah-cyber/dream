# Audit 4 : Poupée (un-potard, hard-tune, formants, octave, brillance)

*10 nov. 2026. Lecture de `OneKnobModels.h` (section POUPÉE). Moteur `TuneCore` en mode Direct (le même qu'Autotune). L'audit 3 demande de vérifier à l'oreille une crête à −1 dB pour k ≥ 0,75. Rien n'a été écouté.*

## Références de la catégorie

- **Soundtoys Little AlterBoy** : hauteur et formant indépendants, Drive, Mix. **Robot** verrouille la voix sur une seule note ; **MIDI** laisse l'hôte imposer la note (effet vocodeur, harmonies).
- **Antares Auto-Tune** (Retune Speed, Humanize, Flex-Tune) et **Soundtoys Devil-Loc** : références connues, que je n'ai pas vérifiées en ligne aujourd'hui.
- **Baby Audio Super VHS** : chaque effet a son propre contrôle, et l'on entend tout de suite sa part.

## Constats

- **Le modèle est complet** pour un hyperpop d'un seul bouton : la grille chromatique en Auto, la pédale hauteur + formants, l'octave à formants resserrés, le FET, l'écrêtage ×2 et l'air piloté par le de-esser.
- **Pas de reverb, pour économiser le CPU.** La voix « poupée » sort donc sèche, avec seulement un slap court. Dans le genre, elle est presque toujours noyée dans un halo.
- **L'octave est dosée par la course** (« au milieu de la course »), pas par la musique. Sur un flow rapide, l'octave double chaque consonne et brouille.
- **Pas de note imposée** : Poupée n'a pas d'entrée MIDI. L'ajouter changerait les bus des projets existants (même raison que pour Autotune, voir le CHANGELOG).
- **Défaut « Auto = chromatique »** : c'est bon pour l'effet, mais les tooltips doivent le dire, puisque ce n'est pas le comportement de Grain Nord. C'est déjà fait (`OneKnobEditor.cpp:200`).

## Améliorations

| # | Amélioration | Impact | Effort | Risque | Change le son |
|---|---|---|---|---|---|
| PO-1 | Option « Halo » dans le volet : envoi léger vers la FDN partagée (une seule instance, longueur fixe), désactivable pour le CPU | 4 | S | Faible | Oui si activée |
| PO-2 | Mode **Robot** (une seule note, la tonique ou la note choisie), en option du volet comme Little AlterBoy | 3 | S | Faible | Non (option) |
| PO-3 | Affichage de la note visée en grand (le lecteur `voiceReadout` existe déjà dans Voile) | 3 | S | Nul | Non |
| PO-4 | Bouton « Verrouiller la tonalité » (voir Grain Nord) | 3 | S | Nul | Non |
| PO-5 | Mesure de la crête de sortie à k ≥ 0,75 sur 3 vraies prises, et témoin « limiteur actif » | 3 | S | Nul | Non |

## Idées signature

- **Octave sur les notes tenues** : le double à l'octave ne s'ouvre que lorsqu'une note est tenue plus de 150 à 200 ms (stabilité de hauteur de `TuneCore`). Les refrains hyperpop gagnent l'octave sur les voyelles longues, et le couplet rapide reste net. S.
- **Escalier** : en fin de course, les glissés entre deux notes deviennent des marches d'un demi-ton, ce qui donne le « stair-step » des voix cassées des années 2020. La grille et le moteur Direct existent ; il suffit de quantifier la courbe de glissé. S, à valider à l'oreille.

Le formant rythmé (V5) est déjà proposé dans l'audit 3 ; je ne le répète pas.

## Sources

[Little AlterBoy, Soundtoys](https://store.soundtoys.com/product/little-alterboy) · [Super VHS, Sound On Sound](https://www.soundonsound.com/node/4920937)
