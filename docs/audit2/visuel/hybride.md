# Maquette hybride : A + B + C

Fichiers : `hybride-voile`, `hybride-autotune` (.svg et .png ×2) et leurs variantes `-clair.png`.

## Recette
**A (base)** : disposition, vis, rails, plaque DW, VU et cadran crème, potards à jupe de Voile, potards moletés d'Autotune, clavier, VFD rouge, couleurs de famille. Finition neuve : arc de valeur fin, arc de modulation bleu, valeurs tabulaires sans puits, virgule française, presets en pilule, une seule famille de police, thème clair (écrans, VFD, VU et touches inchangés).

**B (écran héros seulement)** : l'écran de hauteur d'Autotune gagne en lisibilité (grille à 6 %, notes en 11 px, tonique en accent, cible en trait fin) et une bulle « E4 −4 ct » en grands chiffres tabulaires suit la tête du tracé. Voile n'a pas d'écran : sa lecture flottante « −0,09 dt » devient un petit écran « HAUTEUR », de la taille des cadrans. Optionnel : sans lui, Voile = A pur.

**C (éclat intérieur seulement)** : glow du tracé, dégradé de fond bleu nuit, tête lumineuse. Tout est découpé dans l'écran : rien sur la plaque, rien autour de la fenêtre, LED sans halo.

## Gardé de l'existant
Place de chaque commande, sections, couleurs chanté (cyan), sortie (pêche) et cible (blanc), DSEG7 du VFD, cadran à aiguille, capuchons pêche de MIX et SORTIE, échelle 0–10.

## Tokens
| Rôle | Sombre | Clair |
|---|---|---|
| Plaque | #1E1E22 → #16161A | #F1EEE7 → #E3DED4 |
| Rails | #101012 | #CFC9BE |
| Texte / étiquettes | #F2EFE8 / #A4A1AE | #18181A / #5F5C66 |
| Accent (orange mesuré) | #FF5A36 (5,8 : 1) | #AE2F10 (4,9 : 1) |
| Modulation | #7FD8FF | #2D6CDF |
| Écran | #10131C → #06070B | identique |
| Glow | #5FD3F5, #FF8A5C (flou 3 px) | identique |

Typographie : IBM Plex Sans Condensed Regular et SemiBold, chiffres tabulaires, échelle 10 / 11 / 12 / 14 / 19 / 26 px. DSEG7 réservé au VFD, Plex Mono à la plaque série.

## Risque de dépaysement
- **Faible** : tout ce qui vient de A.
- **Faible à moyen** : bulle de note et glow dans l'écran d'Autotune (plus lumineux, même cadre).
- **Moyen, optionnel** : mini écran de Voile, seul élément nouveau.
- Le bleu de modulation et le cyan « chanté » se ressemblent, mais ne se côtoient jamais (plaque contre écran).

Aucune commande déplacée, aucun VU supprimé.
