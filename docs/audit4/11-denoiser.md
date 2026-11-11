# Audit 4 : Orbe Denoiser

*10 nov. 2026. L'audit 3 (`deesser-denoiser.md`) traite déjà : bruit musical, dilatation du gain, anti-ronflement, profil et fréquence d'échantillonnage, unités. Non répété. Rien n'a été écouté.*

## Références de la catégorie

- **iZotope RX Spectral De-noise** :
  - qualité A à D, et algorithmes Simple, Advanced ou Extreme (moins de bruit musical, plus de latence) ;
  - Artifact Control et Smoothing contre le « gazouillis », Whitening ;
  - réductions **séparées** pour le bruit tonal et le bruit large bande ;
  - courbe de réduction à 25 points ;
  - affichage du résidu.
- **Waves Clarity Vx Pro** :
  - réseau neuronal ;
  - multibande avec solo et **delta** par bande ;
  - **Reflections** (garde la pièce) ;
  - **Ambience Gate** entre les segments de voix ;
  - thèmes clair et sombre.
- **soothe2** : qualité **séparée pour le rendu hors ligne**.

## Constats nouveaux

1. **Latence de 2048 échantillons** (`DenoiserDsp.h:7`, `kLatency = kN`), soit 42,7 ms à 48 kHz. FL la compense à la lecture, mais on ne peut pas débruiter **pendant la prise** : le retour au casque arrive avec 43 ms de retard. RX (algorithme Simple) et Clarity Vx présentent le temps réel comme un usage normal.
2. **Une seule réduction** pour tout le spectre et pour tous les types de bruit. RX sépare tonal et large bande ; Clarity sépare par bande.
3. **Aucun traitement entre les phrases.** Un débruiteur spectral laisse un résidu audible dans les silences, que l'Ambience Gate de Clarity règle à part.
4. **Pas de qualité « rendu »** : le même algorithme tourne en lecture et à l'export, alors que JUCE donne `isNonRealtime()`.
5. **Il y a une écoute du bruit retiré** (`listen`), ce qui est bien et équivaut au delta. Il manque un **Trim** du traité pour l'A/B.

## Améliorations

| # | Amélioration | Impact | Effort | Risque | Change le son |
|---|---|---|---|---|---|
| DN-1 | Mode « Prise » : `kN` = 512, saut de 128 (≈ 11 ms), même algorithme, réduction plafonnée à 12 dB | 3 | M | Moyen | Seulement en mode Prise |
| DN-2 | Réduction en 3 bandes (grave < 200 Hz, médium, aigu > 5 kHz), avec le potard global comme maître | 4 | S | Faible | Non (défaut à plat) |
| DN-3 | « Entre les phrases » : réduction supplémentaire de 0 à 20 dB hors voix, d'après le voisement et l'énergie (détecteur de Relance), avec un relâchement de 150 à 400 ms (l'équivalent de Reflections) | 4 | S | Faible | Non (défaut 0) |
| DN-4 | Qualité hors ligne : à l'export, `kN` doublé et lissage cepstral (si l'algo v2 de l'audit 3 est fait) | 3 | S | Faible | Seulement à l'export |
| DN-5 | Affichage du **résidu** (le spectre du bruit restant), pas seulement du profil | 2 | S | Nul | Non |
| DN-6 | Trim du traité et thème lisible pour l'écoute du retiré | 2 | S | Nul | Non |

## Idée signature

**Respirations protégées** : un classement par trame entre voix, respiration et bruit. La respiration est non voisée, avec un spectre large mais plus fort que le profil de bruit et une durée de 150 à 600 ms. Les briques existent : YIN, le profil de bruit et le détecteur de phrases. On l'applique au potard « entre les phrases » (DN-3) :

- le bruit de fond disparaît entre les phrases ;
- les respirations restent, ou sont baissées d'une quantité qu'on choisit.

Les gates d'ambiance (Clarity) coupent tout ce qui n'est pas de la voix, respirations comprises, et le rap perd sa respiration. C'est le pendant « nettoyage » de l'idée V4 (Respiration) de l'audit 3. Effort M.

## Sources

[RX Spectral De-noise, iZotope](https://s3.amazonaws.com/izotopedownloads/docs/rx9/en/spectral-de-noise/index.html) · [Clarity Vx Pro in Depth, Waves](https://www.waves.com/going-in-depth-clarity-vx-pro) · [Manuel de soothe2](https://oeksound.com/manuals/soothe2/)
