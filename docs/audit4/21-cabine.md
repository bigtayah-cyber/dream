# Audit 4 · Orbe Cabine (chaîne vocale par piste)

*Lecture de `CabineDsp.h`. Saturation et doubleur déjà mesurés dans l'audit 3. Rien n'a été écouté.*

## Références
- **iZotope Nectar 4** : *Vocal Assistant* avec des intentions (Shape, Intensity, FX, Width, Voices) ; détection de la tessiture et de la tonalité ; de-esser ; *Breath Control* ; *Backer* (chœurs) ; Pitch ; *Dimension* ; *Vocal Unmask*.
- CLA Vocals, Scheps Omni Channel, VMR (connaissance générale) : peu de gestes, une couleur de compresseur nommée, un **de-esser toujours présent**, des espaces courts ou longs.

## Constats (au-delà de l'audit 3)
1. **Pas de de-esser** dans la chaîne (HP → mud → présence → air → LP → saturation → compresseur → effets). « Voix lead » (+2 dB d'air, +2,5 dB de présence) et « Aérienne » (+5 dB d'air) **poussent les sifflantes** sans rien pour les tenir. `Dsp/DeEssDyn.h` existe déjà.
2. **Saturation avant le compresseur**, sans choix de l'ordre.
3. **Pas de gain d'entrée** : le seuil de −22 dB des presets suppose une voix vers −12 dBFS RMS. Une prise à −24 dBFS ne déclenche jamais le compresseur.
4. **Presets en double** : « Beat » et « Brut » sont identiques.
5. Doubleur sur le mid seul, avec des pans fixes à ±0,85 : pas de réglage de largeur.
6. Toujours vrai depuis l'audit 3 : espaces figés, compresseur crête.

## Améliorations
| # | Amélioration | Impact | Effort | Risque |
|---|---|---|---|---|
| C1 | **De-esser relatif** (`DeEssDyn`) en tête, potard « Sifflantes » | 5 | S | Faible (coupé pour les états v1) |
| C2 | Gain d'entrée avec zone verte, et « Écouter 5 s » qui le règle | 5 | S | Nul |
| C3 | `OptoComp` (« Doux ») et `FetComp` (« Devant ») à la place de `onek::Comp` (A2 de l'audit 3) | 5 | M | Moyen |
| C4 | Ordre couleur / compresseur au choix | 3 | S | Faible |
| C5 | 3 reverbs (pièce, plaque, hall) ; delay en 1/4, 3/16 ou 1/8, ping-pong | 4 | S | Faible |
| C6 | Doubleur `AdtVoice` avec réglage de largeur | 4 | S | Moyen |
| C7 | Remplacer le doublon « Beat » par un vrai preset (« Voix sur beat chargé »), au même index | 2 | S | Faible |
| C8 | GR continue et courbe de transfert | 3 | S | Nul |

Micro-UX : **Delta** de la saturation (entendre seulement les harmoniques) ; solo des effets pour régler les envois ; infobulles en Hz et en dB pour chaque macro.

## Signature
**« Passe-haut qui suit la voix »** : le suivi de hauteur (YIN d'Autotune) place le passe-haut à 0,7 × f0, lissé sur 100 ms et borné entre 60 et 250 Hz. Sur les passages graves, il descend et garde le corps de la voix. Sur les aigus et les ad-libs, il remonte et retire plosives et grondement. Nectar mesure la tessiture une seule fois ; ici, le suivi est continu. Effort M, avec des briques qui existent déjà.

## Stabilité
- Le delay suit le tempo par glissement (`CabineDsp.h:152`). Un changement de BPM fait glisser la hauteur de l'écho pendant environ 40 ms (effet bande). C'est acceptable, mais il faut le documenter, ou passer à un fondu entre deux têtes de lecture.
- `revTail` passe de 1 à 0 au seuil de 1e-6. Vérifier qu'une queue très longue ne se coupe pas d'un coup.

## Sources
- Nectar 4 : https://www.izotope.com/en/products/nectar/features
- CLA Vocals, Scheps Omni, VMR : connaissance générale.
