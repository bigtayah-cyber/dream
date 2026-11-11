# Audit 4 : Voile

*10 nov. 2026. Lecture du code et banc hors ligne `/tmp/b_aud4v/voile.cpp` (`Engine.h` seul, sans JUCE, 48 kHz). Voix synthétique : 8 notes avec vibrato et sifflantes. Rien n'a été écouté.*

## Références

- **Little AlterBoy** (Soundtoys) : hauteur, formant, drive (celui de Decapitator) et mix. Modes Robot et MIDI.
- **MicroShift** : 3 styles de doubles (H3000, AMS DMX). Le **Focus** n'élargit qu'au-dessus d'une fréquence : le grave reste au centre.
- **Comeback Kid** (Baby Audio) : un Ducker qui suit l'attaque, double-clic pour revenir au défaut, sorties sec et traité séparées.
- **VintageVerb** (Valhalla) : 20 modes, du transparent au « sale » voulu.

## Mesures nouvelles

| Mesure | Résultat |
|---|---|
| Traité moins sec, voix seule, preset Défaut | **+7,9 dB** (entrée à −6 dBFS), **+8,5 dB** (à −18 dBFS) |
| Mordant seul à 0,3 / 0,6 / 1 | +8,0 / +9,6 / +9,7 dB à −6 dBFS ; +8,5 / +11,0 / **+12,3 dB** à −18 dBFS |
| Les 8 presets | De −0,1 dB (Tension pure) à **+8,7 dB** (Mâchoire) |
| Corrélation G/D, Halo = 1 | **0,991** (de 0,94 à 0,999 selon les presets) |
| Latence | 814 échantillons (17 ms) |

**Mordant n'est pas compensé**, et son gain dépend du niveau d'entrée : un trim ne suffit pas, et l'A/B est faussé de 8 dB. **Halo est presque mono** : les deux doubles vont dans les deux canaux (poids 0,4 et 0,8). Un doubleur pro vise une corrélation de 0,3 à 0,7 sur la partie traitée.

## Constats dans le code

- **Un preset efface la tonalité** : les 8 `kPresets` imposent `key = C` et une gamme (`PluginProcessor.cpp:7-16`). Sur un morceau en F mineur, parcourir les presets force la correction en C.
- **L'entrée est sommée en mono**, y compris le sec du Mix.
- **Écrêteur doux caché** au-dessus de −0,9 dBFS en sortie, sans témoin.
- **Brume règle tout ensemble** : delay (croche fixe), reverb (0,8 à 2,5 s) et pré-delay (20 ms fixes). On ne peut pas avoir l'écho sans la salle.
- La vitesse de correction est tirée de Tension et n'est pas exposée.

## Améliorations

| # | Amélioration | Impact | Effort | Risque | Son |
|---|---|---|---|---|---|
| V-1 | Compensation de Mordant : gain statique tabulé selon m et le RMS lent (comme les un-potard, ou l'Auto de Decapitator) | 5 | S | Moyen | Oui |
| V-2 | Les presets ne touchent plus `key` ni `scale` | 4 | S | Nul | Non |
| V-3 | Halo vraiment stéréo : d1 à gauche, d2 à droite, Focus passe-haut à environ 250 Hz | 4 | S | Faible | Oui |
| V-4 | Bouton niveau égal (ou V-1 suffit) | 4 | S | Nul | Non |
| V-5 | Tiroir : division du delay, Écho et Salle séparés, vitesse de correction en ms, pré-delay | 3 | S | Nul | Non |
| V-6 | Entrée stéréo respectée (analyse sur la somme) | 3 | S | Faible | Oui (sources stéréo) |
| V-7 | Mode « Prise » à faible latence (saturation ×2, fenêtre courte) | 3 | M | Moyen | Mode seul |
| V-8 | Témoin d'écrêtage en sortie | 2 | S | Nul | Non |

Micro-UX :
- valeur au survol (« Brume : 1,6 s · écho 1/8 ») ;
- « Verrouiller la tonalité détectée » ;
- astérisque sur un preset modifié.

## Idées signature

- **Mordant sur les voyelles** : la saturation suit le voisement YIN, déjà calculé. Les « s » ne grésillent plus. S.
- **Halo qui respire** : les doubles restent serrés sur un flow rapide et s'ouvrent sur les notes tenues (stabilité de hauteur). S.

## Sources

[Little AlterBoy](https://store.soundtoys.com/product/little-alterboy) · [MicroShift, SOS](https://www.soundonsound.com/reviews/soundtoys-microshift) · [Comeback Kid, Tape Op](https://tapeop.com/reviews/gear/137/comeback-kid-delay-echo-plug-in) · [VintageVerb, modes](https://valhalladsp.com/?p=634677) · [Decapitator, Splice](https://splice.com/blog/get-creative-distortion-using-soundtoys-decapitator)
