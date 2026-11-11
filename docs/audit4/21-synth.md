# Audit 4 · Orbe Synth

*Banc : `/tmp/b_aud4s/synth.cpp` (48 kHz, une voix, filtre ouvert, effets coupés). Lecture de `OrbeSynthEngine.h`. Rien n'a été écouté.*

## Références
- **Xfer Serum 2** : 3 oscillateurs et 5 modes (table d'ondes, multi-échantillon, échantillon, granulaire, spectral) ; **2 filtres** en série ou en parallèle, avec un *Drive* ; **10 LFO** dessinables, 4 enveloppes, 8 macros ; une matrice de modulation en glisser-déposer avec retour visuel ; séquenceur de clips, arpégiateur, 13 effets ; navigateur de presets avec pré-écoute.
- Pigments, Massive X, Vital (connaissance générale) : filtres non linéaires, modulation visible sur les potards, macros.

## Mesures (nouveau)
Repliement (énergie hors harmoniques / harmoniques, 20 Hz–20 kHz) :

| Onde | A4 | A5 | A6 | E7 |
|---|---|---|---|---|
| Scie (PolyBLEP) | −47,1 dB | −43,9 dB | −40,6 dB | **−38,5 dB** |
| Carré (PolyBLEP) | −49,0 dB | −45,8 dB | −42,5 dB | −40,8 dB |
| Triangle (naïf) | −65,2 dB | −56,1 dB | −46,9 dB | −41,7 dB |

Un PolyBLEP à 2 points laisse −40 dB de repliement dès A6. On l'entend sur les leads aiguës, les plucks et les sweeps de hauteur. Les références sont sous −80 dB.

## Constats (code)
1. **Filtre** : un biquad RBJ passe-bas, recalculé tous les 16 échantillons. Q maximal de 5,3 (+14,5 dB). Il n'auto-oscille pas, n'a pas de saturation interne et n'existe qu'en un seul type. Résultat : un pic propre, mais pas le « chant » analogique attendu en pop électro.
2. L'enveloppe de filtre monte **en Hz linéaires** au lieu d'octaves : l'attaque paraît brutale, puis plate.
3. **Pas de LFO, de glide, de mono/legato, ni de vélocité vers le filtre.** Le pitch bend est fixe à ±2 demi-tons ; la molette ouvre la coupure de +3 octaves.
4. **L'unisson démarre en phase 0** sur toutes ses voix (`v = Voice()` dans `noteOn`) : chaque note commence par un pic en phase, avec la même attaque « flanger ». Le désaccord est plafonné à 50 cents (entier), alors qu'un supersaw « rage » en demande 60 à 100.

## Améliorations
| # | Amélioration | Impact | Effort | Risque |
|---|---|---|---|---|
| Y1 | Oscillateurs ×2 avec PolyBLEP à 4 points (ou mipmaps), BLAMP pour le triangle ; cible ≤ −70 dB à A6 | 4 | M | Moyen (`stateVersion`) |
| Y2 | **Filtre ZDF** (SVF ou ladder) avec tanh dans la boucle, auto-oscillation, modes LP12, LP24, BP, HP, coupure modulée à chaque échantillon | 5 | M | Moyen |
| Y3 | **Mono, legato, glide** ; bend réglable de ±1 à ±12 | 5 | S | Faible |
| Y4 | 2 LFO et une matrice de 4 emplacements (LFO, enveloppe, vélocité, molette → hauteur, coupure, volume, pan, FM) | 5 | M | Faible |
| Y5 | Enveloppe de filtre en octaves ; vélocité → coupure | 3 | S | Moyen (presets) |
| Y6 | **Phase d'unisson aléatoire** (graine par note, donc reproductible) ; désaccord jusqu'à 100 ct ; largeur | 4 | S | Faible |
| Y7 | 4 macros : Brillance, Mouvement, Espace, Grain | 4 | S | Nul |

Micro-UX : anneau de modulation sur chaque potard ; oscilloscope ; note jouée affichée ; presets pré-écoutés sur une phrase du style ; preset « Init » propre.

## Signature
**« Glide dans la gamme »** : en legato, le portamento ne glisse pas en continu. Il **traverse les notes de la gamme** entre les deux notes, en une course synchro (1/32 ou 1/64), comme les « runs » des leads plugg et rage, qu'on écrit aujourd'hui à la main. Il suffit de la tonalité et de Y3. Effort S.

## Stabilité
- Le vol de voix fait un fondu de 5 ms (24 emplacements pour 16 voix) : c'est sain. À tester avec la pédale tenue sur plus de 24 notes, pour vérifier qu'aucune voix ne reste bloquée en état « Fast ».
- Avec Y2, il faut lisser la coupure à chaque échantillon : le pas de 16 échantillons produirait du zipper.

## Sources
- RouteNote, Serum 2 : https://create.routenote.com/blog/serum-2-everything-you-need-to-know/
- Pigments, Massive X, Vital : connaissance générale.
