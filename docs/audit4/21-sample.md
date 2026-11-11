# Audit 4 · Orbe Sample (boucle et flip)

*Lecture de `SampleProcessor.cpp/h`, `TransformEngine.h` et `SamplerDsp.h`. Les mesures WSOLA sont dans `21-pads.md` (même moteur). Rien n'a été écouté.*

## Références
- **Serato Sample 2** : *Find Samples*, changement de tonalité, synchro au tempo, Pitch 'n Time de très bonne qualité. Il n'a pas de boucle simple et peu d'effets.
- Portal, Arcade, RC-20 (connaissance générale) : textures lo-fi en modules réglées par une **dose globale** ; jeu dans la tonalité.
- Edison, Slicex (connaissance générale) : boucle calée au tempo, découpe sur les temps.

## État
- Mode « Liés (vinyle) » ou « Indépendants » (WSOLA) ; ±12 dt ; vitesse de 50 à 200 % ; synchro au BPM.
- Filtre, bits, échantillonnage, craquements, pleurage, reverb, inversion, arrêt de bande.
- Découpes déclenchées en MIDI ; zone A–B.
- **Rendu dans un thread de travail** avec cache et annulation : une bonne architecture.

## Constats
1. **Même risque de crash que Pads.** `Play` et `Slice` gardent un `const Rendered*` brut, libéré 30 s après son remplacement. Une boucle de 120 s jouée à 50 % dure 240 s.
2. **Transitoires** ramollis ou doublés : voir les mesures de `21-pads.md`. Ici, c'est le cas d'usage principal (caler une boucle au tempo).
3. **MIDI pauvre** : la vélocité est ignorée (`startSlice(note)`), pas de note-off, seulement 2 voix de découpe, pas de pitch bend.
4. Un `softClip` est appliqué à la somme sans être affiché : il colore les boucles fortes.
5. Les points de boucle ne sont pas cherchés automatiquement.

## Améliorations
| # | Amélioration | Impact | Effort | Risque |
|---|---|---|---|---|
| S1 | Durée de vie de `Rendered` gérée par générations, comme P1 | 5 (crash) | S | Faible |
| S2 | Stretch à transitoires préservés, plus un mode « haute qualité » hors ligne (le rendu est déjà en fond) | 5 | M | Moyen |
| S3 | Vélocité, note-off avec relâche, 4 voix, pitch bend ±2 | 4 | S | Faible |
| S4 | **« Trouver la boucle »** : 1, 2 ou 4 mesures sur le tempo détecté, passages par zéro, raccord le plus semblable | 4 | S | Faible |
| S5 | Limiteur doux à −1 dB avec témoin, à la place du `softClip` | 3 | S | Moyen (son des projets) |
| S6 | Macro « Usure » qui règle ensemble bits, échantillonnage, craquements, pleurage et filtre | 4 | S | Nul |

Micro-UX : BPM et tonalité détectés affichés avec leur **confiance**, et des boutons ×2 / ÷2 ; repères de mesure sur la forme d'onde ; glisser la boucle rendue vers FL ; témoin « rendu en cours ».

## Signature
**« Flip à la syllabe »** : dans la continuité de « Flip dans la tonalité » (I4, audit 3), Sample propose 4 flips prêts à jouer. La boucle est redécoupée en 8 sur ses transitoires, l'ordre est remélangé par une graine, chaque tranche est transposée vers un degré de la gamme, et le swing du projet est appliqué. On passe ainsi d'une boucle simplement calée à un vrai flip façon MPC. Effort M, avec des briques existantes.

## Sources
- Serato Sample 2 : https://www.musicradar.com/reviews/serato-sample-2-review
- Portal, Arcade, RC-20, Edison : connaissance générale.
