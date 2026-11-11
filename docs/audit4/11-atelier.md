# Audit 4 : Orbe Atelier (chaîne vocale à 8 modules)

*10 nov. 2026. Lecture d'`AtelierParams.h` et d'`OrbeChain.h` (même moteur que Looks). Plages et défauts déjà traités dans l'audit 3. Rien n'a été écouté.*

## Références de la catégorie

- **iZotope Nectar 4** :
  - modules **réordonnables** ;
  - deux EQ de 24 bandes avec modes Dynamic, Follow et **Solo Band** ;
  - deux compresseurs à 4 modes ;
  - Auto-Level, Breath Control et Vocal Unmask (masquage voix/instru, avec sidechain) ;
  - affichages tête haute et fenêtre redimensionnable.
- **FabFilter Pro-C 2** :
  - en haut de l'interface : annuler, rétablir, A/B et copie ;
  - MIDI Learn, plein écran, choix de la taille ;
  - suréchantillonnage, **lookahead** ;
  - panneau sidechain avec EQ et liaison stéréo.

## Constats (au-delà de l'audit 3)

1. **L'ordre de la chaîne est figé** : hauteur, EQ, compresseur, saturation, de-ess, doubles, delay, reverb. Le de-esser arrive **après** le compresseur et la saturation : les sifflantes sont d'abord comprimées puis saturées, et c'est déjà l'une des causes du « snare aigu » (audit 3, A3). Nectar laisse réordonner ; une simple bascule « de-ess d'abord » couvrirait 90 % des cas.
2. **L'EQ n'est pas dynamique**, alors que le problème typique d'une voix rap (boue à 250-400 Hz sur les mots graves, dureté à 3 kHz sur les cris) est dynamique.
3. **Un seul caractère de compresseur**, le crête `onek::Comp`. L'audit 3 demande de brancher opto, FET et vari-mu. Il faut aussi un sélecteur visible, comme les « modes » de Nectar.
4. **Pas de sidechain** : ni ducking des effets par une autre piste, ni masquage voix/beat.
5. **Pas de retour par module** : on ne sait pas lequel travaille, ni de combien.
6. **Le preset est un paramètre automatisable** (`preset`, `AtelierProcessor.cpp:10`). Automatiser un choix de preset dans FL recharge tous les réglages en cours de lecture. Il faudrait le marquer non automatisable.

## Améliorations

| # | Amélioration | Impact | Effort | Risque | Change le son |
|---|---|---|---|---|---|
| AT-1 | Bascule d'ordre « De-ess avant comp » (et « Sat après comp / avant comp ») | 4 | S | Faible | Seulement si activée |
| AT-2 | Bande « dynamique » sur Méd-1 et Méd-2 (seuil relatif, plage ±6 dB), réutilisant le détecteur de `DeEssDyn` | 4 | M | Moyen | Seulement si activée |
| AT-3 | Sélecteur de caractère du compresseur (Crête, Opto, FET, Vari-mu), en fin de liste, l'ancien par défaut pour les anciens états | 4 | S | Moyen | Non pour les anciens états |
| AT-4 | Bande de modules avec, pour chacun, une mini-jauge de son action (EQ ±dB, GR, drive, sifflantes retirées, niveau des doubles et effets), plus solo et delta | 4 | M | Nul | Non |
| AT-5 | `preset` marqué non automatisable (`withAutomatable(false)`), comme `engine` dans les un-potard | 3 | S | Faible | Non |
| AT-6 | Lookahead de 1 à 2 ms sur le compresseur (déjà demandé, A3) affiché comme latence | 3 | S | Faible | Oui |
| AT-7 | « Coller depuis Looks » (voir LO-2) | 3 | S | Nul | Non |

Micro-UX :
- valeurs en unités réelles (jamais « x10 ») ;
- molette : un cran égal à un pas utile (1 dB, 1/12 d'octave) ;
- Alt-clic pour basculer un module ;
- clic droit sur un module : « réinitialiser ce module », « copier » et « coller ».

## Idée signature

**Ducking des effets par syllabe** : le delay et la reverb ne baissent plus sous le niveau de la voix (le « duck » actuel), mais sur les **attaques de syllabes**. Le grain de Gorge détecte déjà les syllabes. Les queues restent entre les mots, et chaque nouveau mot sort net, même dans un flow rapide où le duck de niveau reste fermé en permanence. Comeback Kid suit l'attaque du sec, mais pas la syllabe. Effort S.

## Sources

[Nectar 4, fonctions](https://www.izotope.com/en/products/nectar/features) · [Pro-C 2, aide en ligne FabFilter](https://www.fabfilter.com/help/pro-c/using/overview) · [Comeback Kid, Tape Op](https://tapeop.com/reviews/gear/137/comeback-kid-delay-echo-plug-in)
