# Audit 4 · Orbe Galet (mise en forme par le temps)

*Lecture de `GaletDsp.h`, `GaletProcessor.cpp` et `GaletEditor.cpp`. Distorsion ×4 avec ADAA validée dans l'audit 3. Rien n'a été écouté.*

## Références
- **ShaperBox 3** : 9 modules (Volume, Time, Drive, Filter, Crush, Noise, Pan, Width, Liquid), **une forme par module**, multibande (3 bandes) ; **déclenchement par l'audio** (3 algorithmes, entrée sidechain) ; déclenchement MIDI ; éditeur avec rampes, pentes et pas ; navigateur avec variations.
- Kickstart 2, LFOTool (connaissance générale) : un seul geste et **latence nulle**.
- **Gross Beat** (natif FL) : 36 emplacements « temps » et 36 « volume » commutables par MIDI ; attaque (0–500 ms) et relâche (0–1 000 ms) avec tension.

## Constats
1. **Une seule forme pour 4 modules** : impossible d'avoir un volume en 1/4 et un filtre en 1/2 en même temps.
2. **Pas de déclenchement par l'audio** (Départ : Hôte, Note ou Libre).
3. **Pas de module Temps** (demi-vitesse, retour) : c'est le plus utilisé en trap. Le moteur de relecture existe dans Accroc.
4. **Pas d'emplacements de formes** commutables par MIDI.
5. **Latence constante**, même distorsion coupée, alors qu'un ducker devrait être à zéro.
6. **Éditeur** sans annuler, copier/coller, inverser ni décaler.
7. 23 potards à l'écran (audit 3) ; l'étiquette « Drive max » prête à confusion.

## Améliorations
| # | Amélioration | Impact | Effort | Risque |
|---|---|---|---|---|
| G1 | **Une forme par module** (4 tables, 4 phases), avec un lien optionnel. Un ancien état s'ouvre avec les 4 liés | 5 | M | Moyen (`stateVersion`) |
| G2 | Déclenchement audio : détecteur de transitoires qui remet la phase à zéro, avec sensibilité et temps mort | 5 | M | Faible |
| G3 | 4 emplacements de forme commutables par note (C1 à D#1), fondu de 5 ms | 4 | S | Faible |
| G4 | Annuler, copier, coller, inverser, miroir, décaler ±1 pas, aléatoire doux | 4 | S | Nul |
| G5 | Mode « latence nulle » choisi à l'instanciation (ADAA seul) | 3 | S | Faible |
| G6 | Module **Temps** sur le moteur d'Accroc | 5 | L | Moyen |
| G7 | Tiroir « avancé » ; par défaut, montrer seulement forme, division, profondeur et mix | 4 | S | Nul |

Micro-UX : valeur en dB ou en Hz sous le pointeur ; points déplaçables aux flèches ; Maj pour libérer de la grille ; vignette de la forme dans la liste des presets.

## Signature
**« Capturer le groove »** : on joue une mesure de batterie dans Galet et on appuie sur « Capturer ». L'enveloppe RMS (sur 5 ms) est simplifiée en 16 à 32 points (Douglas-Peucker) et **devient la forme**. Pad, voix ou 808 respirent alors avec la batterie du morceau, sans sidechain ni dessin. ShaperBox et Gross Beat ne transforment pas l'audio en forme. Effort S/M.

## Stabilité
- La forme passe de l'interface à l'audio par un drapeau `busy` (lignes 232 à 249). L'audio ne l'attend jamais : s'il ne peut pas lire, il réessaie au bloc suivant. C'est sûr ; garder ce schéma avec G1.
- Départ « Note » : vérifier les notes superposées (une note-off ne doit pas figer la phase).

## Sources
- ShaperBox 3 : https://www.kvraudio.com/news/cableguys-release-shaperbox-3-new-audio-triggering-liquidshaper-and-more-56153
- Gross Beat : https://www.image-line.com/support/FLHelp/html/plugins/Gross%20Beat.htm
- Kickstart 2, LFOTool : connaissance générale.
