# Maquette HYBRIDE v2 : clair d'abord, B + C pour la modernité

Fichiers : `hybride2-voile-clair` (principal), `-voile-sombre`, `-autotune-clair`, `-autotune-sombre`.

## D'où vient quoi
- **A** : thème clair perle comme thème principal, couleurs de famille, tabulaires sans puits, presets en pilule.
- **B** : grand écran héros central (hauteur, cible, chiffres géants « −0,09 dt », « E4 −04 ct »), modules en cartes, ombres douces, potards plats à arc fin.
- **C** : éclat lumineux dans l'écran seulement : glow du tracé, dégradé bleu nuit et halo, tête de courbe lumineuse, forme d'onde, lueur des curseurs.

## Gardé
Accent orange #FF5A36 (Voile et Orbe), noms, places logiques (4 potards à gauche, MIX/SORTIE à capuchon pêche, tonalité et gamme à droite, VU, clavier de masque, sections d'Autotune), échelle 0–10, légende chanté/sortie/cible. Aiguilles en trait fin ; vis, rivets et moletage supprimés.

## Tokens
| Rôle | Clair | Sombre |
|---|---|---|
| Fond / carte | #F6F7F9→#E9ECF1 / #FFFFFF | #12141A→#0C0E12 / #1A1D25 |
| Texte / étiquette | #15171C / #515662 (7,3 : 1) | #F2F3F6 / #A9AFBD (7,7 : 1) |
| Accent graphique / texte | #FF5A36 / #B5320E (5,2 : 1) | #FF5A36 / #FF7A5A (7,2 : 1) |
| Modulation | #2D6CDF | #7FD8FF |
| Écran (identique) | #161B3A→#0A0D1C, halo #7A5CFF | idem |
| Tracé | chanté #6FD6FF, sortie #FF8A5C, cible #E8EBF7 | idem |

Typo : IBM Plex Sans Condensed (Regular, SemiBold), chiffres tabulaires, 10 à 70 px. Textes ≥ 4,5 : 1.

## Risque de dépaysement
- **Moyen** : plaque lisse sans vis, potards plats, écran héros plus grand que l'actuel.
- **Moyen à fort pour Voile** : l'écran de hauteur est nouveau (il n'en avait pas), et le cadran de correction passe dans le héros.
- **Faible** : couleurs, noms, positions, VU.

## Impact sur le kit
- Palette deux thèmes plus tokens `screen` (toujours sombre), `mod`, `accentText`.
- Nouveaux composants : héros avec glow (blur + dégradé), potard plat à arc fin et arc de modulation, carte à ombre douce, sélecteur et segment en pilule.
- Coût CPU du glow à mesurer.
- Voile gagne un écran : nouvelle zone à développer.
