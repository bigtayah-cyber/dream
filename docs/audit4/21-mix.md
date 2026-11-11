# Audit 4 · Orbe Mix (analyseur)

*L'audit 3 avait exclu Orbe Mix. Lecture de `OrbeAnalysis.cpp` et `MixProcessor.cpp`. Banc : `/tmp/b_aud4s/mixtp.cpp`. Rien n'a été écouté.*

## Références
- **Tonal Balance Control 2** : courbes cibles par style ; cibles **personnalisées** depuis un fichier ou un dossier ; jauge du **facteur de crête des graves** (20–250 Hz) ; vues large et fine ; solo de bande.
- **Ozone 11** : Match EQ ; prévisualisation des codecs.
- Youlean, LEVELS, REFERENCE (connaissance générale) : historique LUFS, cibles de plateforme, référence écoutée à niveau égal.

## Mesures
- **Intégré** : portes −70 LUFS et −10 LU. **LRA** : court terme, porte −20 LU, percentiles 10 et 95. Conforme à EBU Tech 3341/3342, d'après le code.
- **Pic vrai** sur un sinus de −3,10 dBTP réels : 1 kHz → −3,10 ; 6 kHz → −3,05 ; **12 kHz → −3,76** ; 15 kHz → −3,14. L'interpolation cubique **sous-estime de 0,66 dB** vers 12 kHz, alors que BS.1770-4 demande un polyphase ×4. Un mix affiché à −1,0 dBTP peut donc être à −0,3.

## Ce qui manque
1. **Pas de référence audio** : on ne compare qu'à des profils de style figés.
2. **Pas d'historique** : on ne voit pas si le refrain monte.
3. Pas de facteur de crête des graves (le problème typique de la 808).
4. Pas d'écoute mono, côté seul ou solo de bande.
5. Les diagnostics (`Diag`) sont bons mais **pas actionnables** (rien ne relie à EQ ou à Master).

## Améliorations
| # | Amélioration | Impact | Effort | Risque |
|---|---|---|---|---|
| X1 | Pic vrai polyphase ×4 : reprendre `mas::TruePeak`, avec le même code dans Master | 3 | S | Nul |
| X2 | **Référence déposée** (WAV/MP3, décodeur de Pads) : 7 bandes, LUFS et PLR calculés hors du thread audio, utilisés comme cible | 5 | M | Faible |
| X3 | Historique LUFS-S sur 3 min, avec marqueurs de section | 4 | S | Nul |
| X4 | Facteur de crête 20–250 Hz, avec une cible par style | 4 | S | Nul |
| X5 | Écoutes : mono, côté seul, solo de bande | 3 | S | Faible (le passe-plat s'arrête quand une écoute est active) |
| X6 | Export du rapport (texte et PNG) | 2 | S | Nul |

Micro-UX : clic sur un diagnostic pour surligner la bande concernée ; bouton « remise à zéro de l'intégré » ; cible de plateforme visible sur le compteur.

## Signature
**« Le refrain monte-t-il ? »** : Orbe Mix lit la PPQ et découpe le morceau en blocs de 8 mesures. Il compare d'un bloc à l'autre le LUFS court terme, l'énergie du côté et la bande 5–10 kHz, et conclut par exemple : « refrain à +0,4 LU seulement : vise +1 à +2 LU, élargis les chœurs ». C'est une mesure d'**arrangement**, introuvable chez les analyseurs grand public. Effort M.

## Stabilité
- Correctifs récents : molette (`fc7fd89`), verrou `prepare`/`process`, `clean()` contre les NaN. Le `tryLock` saute la mesure d'un bloc pendant `prepare` : c'est correct.
- L'histogramme est alloué dans `prepare`, sans allocation dans le thread audio : à conserver. Les compteurs `uint32` ne débordent pas avant des années de mesure.

## Sources
- TBC 2 : https://s3.amazonaws.com/izotopedownloads/docs/tbc2/en/targets-and-views/index.html
- Ozone 11 : https://www.izotope.com/en/products/ozone/features
- BS.1770-4, EBU Tech 3341/3342 : cités de mémoire.
