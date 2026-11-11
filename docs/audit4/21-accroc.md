# Audit 4 · Orbe Accroc (glitch)

*Lecture de `AccrocDsp.h` et `AccrocProcessor.cpp`. Les fondus ont été vus dans l'audit 3. Rien n'a été écouté.*

## Références
- **Effectrix 2** : 14 modules réordonnables (Looper A et B, Grain, Vinyl, Tonalizer, Crush, Filter, Delay, Reverb…), chacun avec son dry/wet. **Grille peinte**, une ligne par effet, avec swing, triolets et notes pointées. **Ligne de modulation par pas** (suiveur d'enveloppe, LFO, aléatoire). **12 séquences par preset**, choisies par MIDI.
- **Gross Beat** (natif FL) : tampon de 2 mesures, enveloppes de temps et de volume dessinées, 36 emplacements, scratch.
- Stutter Edit 2, Glitchmachines (connaissance générale) : des **gestes** (stutter accompagné de rampes de filtre, de hauteur et de volume) et une **traîne d'écho** après le geste.

## Constats
1. **Un seul accident à la fois.** On ne peut pas superposer « stutter + filtre qui se ferme » ni « retour + écho ». C'est pourtant ce qui fait un geste pro.
2. **Pas de filtre ni d'écho** dans l'accident. Seul le stutter a une rampe, et uniquement de hauteur (±12 dt).
3. **Pas de traîne** : on revient au signal sec d'un coup.
4. **Séquenceur sur 16 pas, une seule ligne**, sans probabilité ni vélocité par pas, sans swing.
5. La graine est un flottant de 1 à 999 (audit 3).
6. Le MIDI est déjà tenu (l'accident dure tant que la touche est enfoncée) et la vélocité règle l'intensité (`AccrocDsp.h:262-267`). Mais l'interface ne le dit pas : il n'y a **pas de carte des touches**.

## Améliorations
| # | Amélioration | Impact | Effort | Risque |
|---|---|---|---|---|
| A1 | **Traîne** : écho synchro (1/8, 3/16) et petite reverb (`PingPong`, `Reverb`), avec durée réglable | 5 | S | Faible |
| A2 | **Rampes par accident** : filtre (LP ou HP balayé), volume, hauteur, pour tous les types | 5 | M | Faible |
| A3 | Carte des touches affichée (C = Stutter … G = Trous), avec surbrillance de l'accident en cours | 3 | S | Nul |
| A4 | Probabilité et vélocité par pas, swing, 32 pas | 4 | S | Faible (`stateVersion`) |
| A5 | **Gestes** : accident + rampes, en 8 emplacements choisis par MIDI | 4 | M | Faible |
| A6 | Graine entière et bouton « relancer » | 3 | S | Faible (garder l'ID et la plage) |
| A7 | Fondu de fin en cosinus ; au moins 32 échantillons pour le 1/64 (audit 3) | 3 | S | Faible |

Micro-UX : peindre la grille en glissant ; clic droit sur un pas pour choisir son type ; tête de lecture visible ; nom de l'accident en cours affiché en gros ; Ctrl-clic pour effacer une ligne.

## Signature
**« Rampe dans la gamme »** : le stutter et le bégaiement montent ou descendent **par degrés de la gamme** du morceau, pas en demi-tons continus. Une répétition de voix devient une petite mélodie juste. Le décaleur partagé fait le travail ; il ne reste qu'à écrire la table des degrés. Effort S. Avec les « Coupes sur la voix » de l'audit 3 (S5), Accroc devient un outil d'ad-libs chantés.

## Stabilité
- Le tirage dépend de la graine et du numéro de pas, quelle que soit la taille de bloc. Les exports sont donc reproductibles. Garder ce même tirage pour A4.
- Avec A1, vérifier que l'historique suffit pour un stutter de 1/4 à 60 BPM avec la traîne (plus d'une seconde).

## Sources
- Effectrix 2 : https://musicradar.com/reviews/sugar-bytes-effectrix2-review
- Gross Beat : https://www.image-line.com/support/FLHelp/html/plugins/Gross%20Beat.htm
- Stutter Edit 2, Glitchmachines : connaissance générale.
