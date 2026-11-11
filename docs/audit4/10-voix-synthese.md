# Audit 4, famille VOIX : synthèse

*10 nov. 2026, 18 h–20 h 30 (Paris). Périmètre : Voile, Grain Nord, Gorge, Poupée, Patine, Orbe Looks, Atelier, DeEsser, Denoiser, Relance, Detect, Fredonne. Autotune est audité à part.*

*Lecture seule du code, un banc hors ligne (`/tmp/b_aud4v/voile.cpp`) et des recherches en ligne (sources dans chaque `11-*.md`). Ce que contiennent déjà les audits 2 et 3 n'est pas répété. Rien n'a été écouté.*

## Verdict

La famille voix a un vrai socle :
- moteurs de hauteur sains ;
- un-potard étalonnés ;
- idées originales (Relance, brume tonale, râle).

L'écart avec Waves, FabFilter, Soundtoys ou iZotope se trouve moins dans le DSP que dans trois choses.

1. **Des comparaisons honnêtes.** Le niveau égal, un delta partout où c'est possible et un A/B sans piège. On l'apprend vite avec les outils pro (soothe2 : Delta, Trim du traité, bypass doux).
2. **Des réglages qui respectent l'artiste.** Un preset ne doit pas effacer sa tonalité, un changement de look ne doit pas effacer ses macros, et l'on doit voir ce que fait chaque bouton.
3. **Les contrôles « métier » standard** : lookahead réglable, stéréo et M/S, sidechain, qualité de rendu hors ligne, mode faible latence pour la prise.

## Constats nouveaux (non présents dans les audits 2 et 3)

| Constat | Preuve |
|---|---|
| **Voile : Mordant non compensé**. Le traité est **+7,9 dB** au-dessus du sec avec le preset Défaut, et l'écart va de −0,1 à **+8,7 dB** selon le preset. Mordant seul ajoute **+8 à +12 dB**, et davantage quand l'entrée est faible | Banc `voile.cpp`, voix seule, 48 kHz |
| **Voile : Halo presque mono**, corrélation G/D de **0,991** à Halo = 1 (les deux doubles vont des deux côtés) | Banc, et `Engine.h` (`process`) |
| **Les presets de Voile imposent la tonalité C** et une gamme | `PluginProcessor.cpp:7-16` |
| **Looks : un changement de look remet les 4 macros à leurs valeurs d'usine** | `LooksProcessor.cpp:54` |
| **Relance : l'harmonie est une quinte juste fixe**, donc hors gamme sur le 7e degré (Si → Fa# en Do majeur) | `RelanceDsp.h:287` |
| **Denoiser : latence de 42,7 ms** à 48 kHz, impossible à utiliser pendant la prise | `DenoiserDsp.h:7` |
| **Atelier : le preset est un paramètre automatisable**, donc une automation recharge toute la chaîne | `AtelierProcessor.cpp:10` |
| **Atelier : de-ess après le compresseur et la saturation**, dans un ordre figé | `OrbeChain.h:2` |
| **Fredonne : la vélocité diffère** entre la sortie directe (106 partout) et le fichier `.MID` (tirée du niveau) | `FredonneProcessor.cpp:112`, `OrbeAnalysis.cpp:518` |
| Voile, un-potard et Looks **replient l'entrée en mono** | `Engine.h`, `OneKnobModels.h:2`, audit 3 |

## Ce que le kit v3 doit offrir (complément précis de l'audit 3, A8)

Repris des références et ajusté à FL Studio :

- **A/B** avec copie A→B, **annuler/rétablir séparés pour A et B** (soothe2) et indicateur « modifié » (astérisque).
- **Navigateur de presets** :
  - presets utilisateur ;
  - favoris, catégories et recherche ;
  - « Annuler » pendant qu'on parcourt ;
  - défaut utilisateur ;
  - la tonalité et la gamme ne sont **jamais** dans un preset.
- **Delta** partout où le traitement est additif : Gorge, Patine, DeEsser, Denoiser (déjà fait), Looks et Atelier. Le delta reste actif quand on change de preset.
- **Trim du traité** pour comparer à niveau égal, plus la compensation statique (Voile).
- **Qualité hors ligne** via `isNonRealtime()` : suréchantillonnage ou FFT plus grands à l'export, comme soothe2. Pour FL, c'est gratuit.
- **Mode « Prise »** à faible latence pour les plugins qu'on met pendant l'enregistrement : Voile, Denoiser, DeEsser.
- **Paramètres de navigation non automatisables** (preset, look), comme `engine`.
- Micro-UX communs :
  - molette par pas utiles, Maj pour le pas fin ;
  - double-clic pour revenir au défaut, saisie au clavier ;
  - clic droit : copier, coller, réinitialiser ;
  - valeurs dans leurs vraies unités ;
  - aide « ? » qui montre ce que fait chaque macro ;
  - « Verrouiller la tonalité détectée » sur chaque menu Tonalité ;
  - « Coller la tonalité » venue de Detect.

## Tableau priorisé

Impact de 1 à 5 · effort S (≤ 1 j), M (2 à 4 j), L · « Son » : change le rendu des projets existants (il faut alors une version d'état).

| Rang | ID | Plugin | Chantier | Impact | Effort | Risque | Son |
|---|---|---|---|---|---|---|---|
| 1 | V-2 | Voile | Les presets ne touchent plus la tonalité | 4 | S | Nul | Non |
| 2 | LO-4 | Looks | Macros gardées quand on parcourt les looks, indicateur « modifié » | 4 | S | Faible | Non |
| 3 | V-1 | Voile | Compensation statique de Mordant (−8 à −12 dB) | 5 | S | Moyen | Oui |
| 4 | AT-5 | Atelier | `preset` (et `look`) non automatisables | 3 | S | Faible | Non |
| 5 | RE-1 | Relance | Harmonie diatonique (tonalité Auto ou imposée) | 4 | S | Faible | Oui |
| 6 | V-3 | Voile | Halo vraiment stéréo + Focus passe-haut | 4 | S | Faible | Oui |
| 7 | DE-3 | Detect → voix | Copier/coller la tonalité entre plugins | 4 | S | Nul | Non |
| 8 | GO-1, PA-3, LO-5 | Gorge, Patine, Looks | Écoute delta | 3-4 | S | Nul | Non |
| 9 | AT-1 | Atelier | Bascule « de-ess avant comp » | 4 | S | Faible | Option |
| 10 | DS-5 | DeEsser | Auto-seuil 5 s (avec le seuil relatif de l'audit 3) | 4 | S | Faible | Non |
| 11 | DN-3 | Denoiser | Réduction « entre les phrases » | 4 | S | Faible | Non (défaut 0) |
| 12 | DN-2 | Denoiser | Réduction sur 3 bandes | 4 | S | Faible | Non |
| 13 | GN-1, PA-1 | Un-potard | Afficher ce que fait le potard (étapes, courbe cible) | 4 | S | Nul | Non |
| 14 | LO-3 | Looks | Jauge du niveau auto | 4 | S | Faible | Non |
| 15 | DE-2 | Detect | Accordage du beat en cents | 4 | S | Faible | Non |
| 16 | FR-2 | Fredonne | Force de quantification et swing | 4 | S | Faible | Non |
| 17 | AT-3 | Atelier | Caractère du compresseur (opto, FET, vari-mu) | 4 | S | Moyen | Non pour les anciens états |
| 18 | LO-1 | Looks | Navigateur : favoris, recherche, Annuler | 4 | M | Nul | Non |
| 19 | AT-4 | Atelier | Mini-jauges par module, solo et delta | 4 | M | Nul | Non |
| 20 | RE-2 | Relance | Ligne de temps des réponses, relancer un seul dé | 4 | M | Faible | Non |
| 21 | DE-1 | Detect | Banc de justesse (score MIREX) en CI | 4 | M | Nul | Non |
| 22 | FR-3 | Fredonne | Édition des notes | 4 | M | Faible | Non |
| 23 | DS-1, DS-2 | DeEsser | Lookahead réglable, liaison stéréo et M/S | 3 | S-M | Faible | Oui (DS-1) |
| 24 | DN-1, V-7 | Denoiser, Voile | Mode Prise à faible latence | 3 | M | Moyen | Mode seul |
| 25 | AT-2 | Atelier | Bandes d'EQ dynamiques | 4 | M | Moyen | Option |
| 26 | PO-1 | Poupée | Option halo (reverb partagée) | 4 | S | Faible | Option |
| 27 | DS-3 | DeEsser | Sidechain externe | 3 | M | Moyen (bus) | Non |

Les rangs 1 à 8 tiennent en une semaine environ. Ils corrigent ce qu'un ingé repère dans les 5 premières minutes.

## Idées signature (moteur existant, non proposées dans l'audit 3)

| Idée | Plugin | Principe | Effort |
|---|---|---|---|
| **Mordant sur les voyelles** | Voile | La saturation suit le voisement YIN : les consonnes et les « s » restent propres | S |
| **Halo qui respire** | Voile | Les doubles s'ouvrent sur les notes tenues et restent serrés sur le flow | S |
| **Slap qui suit le flow** | Gorge | Le slap baisse quand le débit syllabique monte | S |
| **Octave sur les notes tenues** | Poupée | L'octave ne s'ouvre qu'au-delà de 150 à 200 ms de note stable | S |
| **Ducking par syllabe** | Atelier (et Looks) | Les effets se retirent sur les attaques de mots, pas sur le niveau | S |
| **Gel de fin de phrase** | Grain Nord | Les résonateurs tiennent le dernier accord chanté sous le silence | S-M |
| **Cible apprise** | Patine | La courbe cible est apprise sur 10 à 20 s d'une voix de référence | S-M |
| **« S » et « CH » séparés** | DeEsser | Deux détecteurs relatifs, dont un pour le « ch » et le « j » français | M |
| **Respirations protégées** | Denoiser | Classement voix, respiration, bruit : le silence est nettoyé, le souffle gardé | M |
| **Fondu de looks** | Looks | Une macro « Vers » fait passer d'un look A à un look B (couplet → refrain) | M |
| **Glisser les relances** | Relance | Export `.WAV` des réponses, à monter dans la playlist FL | M |
| **Carte des fausses notes** | Detect | La voix est jugée par rapport à la tonalité du beat, note par note | M |
| **Fredonner la 808** | Fredonne | Les glissés fredonnés deviennent des slides de 808, sur l'accordage donné par Detect | M |

Mes trois préférées pour l'identité : **Carte des fausses notes**, **S/CH séparés** et **Fondu de looks**. Elles prolongent le fil rouge de l'audit 3 (« la suite écoute la voix et la tonalité ») et sont introuvables chez les concurrents cités.

## Ordre conseillé

1. Rangs 1, 2 et 4 (aucun changement de son), en même temps que A1 (`stateVersion`) de l'audit 3.
2. Rang 3 et V-3 avec A2 à A5, puisqu'ils changent tous le son : une seule version d'état et une seule séance d'écoute (A11).
3. Le kit pro : A/B, presets, delta, trim, qualité hors ligne.
4. Ensuite seulement, les signatures S, puis M.

## Ce que je n'ai pas vérifié

- **L'écoute et FL Studio** : rien n'a été joué.
- **La voix de test est synthétique** : le +8 dB de Mordant est à confirmer sur 3 vraies prises.
- **Certains produits n'ont pas été consultés en ligne** (Auto-Tune, Harmony Engine, Smooth Operator, GiantSteps) : c'est signalé là où ils sont cités.
- **Le CPU** n'a pas été mesuré.
- **Les autres plugins** n'ont pas été remesurés : je me suis appuyé sur les audits 2 et 3.

## Fichiers

`11-voile.md` · `11-grain-nord.md` · `11-gorge.md` · `11-poupee.md` · `11-patine.md` · `11-looks.md` · `11-atelier.md` · `11-deesser.md` · `11-denoiser.md` · `11-relance.md` · `11-detect.md` · `11-fredonne.md`. Banc : `/tmp/b_aud4v/voile.cpp`, temporaire.
