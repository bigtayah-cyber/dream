# Audit 4 · Studio et instruments : synthèse (13 plugins)

*10 novembre 2026. Périmètre : Cabine, Master, EQ, Mix, Orbe, Orbe FX, Galet, Accroc, Synth, 808, Beat, Pads, Sample. Méthode : lecture du code (branche `vague2-wip`), bancs hors ligne sans JUCE dans `/tmp/b_aud4s/` (g++ -O2, un cœur, 48 kHz), recherche web sur les références. Aucun fichier source modifié, aucun build. **Rien n'a été écouté.** Ce document ne répète pas les audits 2 et 3 : il les complète.*

## Verdict

Les bases DSP « studio » sont saines (EQ appariée, LUFS juste, saturation de la 808 propre). Ce qui sépare encore la suite d'un niveau pro, ce sont **trois familles de manques que l'audit 3 n'avait pas mesurées** :

1. **Les transitoires** : le time-stretch de Pads et Sample ramollit les attaques et les double. Le limiteur de Master distord les graves, et la 808 relance ses notes en retard.
2. **Le routage** : aucune sortie séparée dans Beat et Pads, pas d'entrée MIDI dans Orbe, et Orbe FX n'écoute pas l'audio qu'il reçoit.
3. **La modulation des instruments** : le Synth n'a ni LFO, ni glide, ni filtre « qui chante ». Galet n'a qu'une forme pour 4 modules, et Accroc ne compose pas ses effets.

Le crash possible de Pads et Sample (A10 de l'audit 3) est **toujours ouvert**.

## Ce qui est mesuré (nouveau)

| Plugin | Mesure | Résultat | Lecture |
|---|---|---|---|
| Master | LUFS momentané, 997 Hz à −20 dBFS | −20,00 | Juste |
| Master | Limiteur, 40 Hz, 8 dB de GR | **DHT 1,16 %** (0,05 % à 100 Hz) | Distorsion des graves (fenêtre 3 ms, relâche fixe) |
| Master | Plafond −1 dBTP | Sortie à **−0,68 dBTP** | Dépassement de 0,32 dB |
| Master | Pompage du bus (kick + nappe) | −1,1 dB à 210 ms | Modéré : nuance l'audit 3 |
| Mix | Pic vrai, sinus 12 kHz | **−0,66 dB** sous la vraie valeur | L'interpolation cubique ne suffit pas |
| Synth | Repliement scie PolyBLEP | −47 dB (A4) → **−38,5 dB (E7)** | Audible sur leads aiguës |
| Synth | Phase d'unisson | Toutes les voix à 0 | Même attaque « flanger » à chaque note |
| 808 | Douce et Écrêtée, drive 1 | −84 dB | Propre (ferme le point ouvert de l'audit 3) |
| 808 | Repliée, drive 1 | **−27 dB** inharmonique | Vrai repliement numérique |
| 808 | Note relancée | **2,5 à 4 ms** de retard | Gigue dans les roulements |
| Pads/Sample | WSOLA ×1,6 | Doublon à **−13 dB**, pré-écho à −11,5 dB | « Flam » sur la batterie |
| Pads/Sample | WSOLA ×0,7–0,9 | Crêtes à **−5 à −9 dB** | Attaques ramollies |

## Tableau priorisé

Impact : 5 = majeur. Effort : S ≤ 2 j, M ≤ 1 semaine, L > 1 semaine. Risque : casser des projets ou régresser. Les numéros renvoient aux fichiers `21-*.md`.

| Rang | ID | Plugin | Chantier | Impact | Effort | Risque |
|---|---|---|---|---|---|---|
| 1 | P1/S1 | Pads, Sample | **Durée de vie des banques par générations** (corrige le crash) | 5 | S | Faible |
| 2 | H1 | 808 | Relance immédiate de note (2 voix, fondu de 2 à 3 ms) | 5 | S | Faible |
| 3 | M2, X1 | Master, Mix | Pic vrai conforme et **un seul code** de mesure (`mas::TruePeak` ×4 polyphase, `oa::MixMeter`) | 4 | S | Nul |
| 4 | M3, M4 | Master | Intégré, court terme, LRA, cible LUFS, **écoute à niveau égal, Delta** | 5 | S | Nul |
| 5 | C1, C2 | Cabine | **De-esser** en tête ; gain d'entrée avec « Écouter 5 s » | 5 | S | Faible |
| 6 | Y3 | Synth | Mono, legato, glide ; bend réglable | 5 | S | Faible |
| 7 | H3 | 808 | Glissé synchro ou en ms, modes, courbe | 5 | S | Faible |
| 8 | A1 | Accroc | **Traîne** d'écho et de reverb après l'accident | 5 | S | Faible |
| 9 | F1 | Orbe FX | Écoute de l'entrée : tonalité proposée | 5 | S | Nul |
| 10 | M1 | Master | Limiteur : relâche adaptative, DHT < 0,3 % à 40 Hz | 5 | M | Moyen |
| 11 | P2/S2 | Pads, Sample | Stretch **à transitoires préservés** | 5 | M | Moyen |
| 12 | Y2 | Synth | Filtre ZDF non linéaire, auto-oscillation, 4 modes | 5 | M | Moyen |
| 13 | Y4 | Synth | 2 LFO et une matrice de 4 emplacements | 5 | M | Faible |
| 14 | G1, G2 | Galet | **Une forme par module** ; déclenchement par l'audio | 5 | M | Moyen |
| 15 | T1–T3 | Beat | Sorties séparées ; réglages par piste ; one-shots déposés | 5 | M | Moyen |
| 16 | O1 | Orbe | **Jouer au clavier** (entrée MIDI) : à faire en nouveau plugin ou sous `stateVersion` | 5 | M | Moyen |
| 17 | O2, O3, O7 | Orbe | Édition d'un accord ; verrous ; annuler les tirages | 5 | M | Faible |
| 18 | E1–E3 | EQ | Pentes 12/24/48 ; solo de bande ; auto-gain | 4 | S | Faible |
| 19 | E4 | EQ | Bandes dynamiques | 5 | M | Faible |
| 20 | X2 | Mix | Référence audio déposée comme cible | 5 | M | Faible |
| 21 | A2, A5 | Accroc | Rampes (filtre, volume, hauteur) ; gestes commutables par MIDI | 5 | M | Faible |
| 22 | Y1, H2 | Synth, 808 | Oscillateurs ×2 et BLAMP ; « Pliée » ≤ −60 dB | 4 | M/S | Moyen |
| 23 | C3–C6 | Cabine | Compresseurs nommés, ordre, espaces, ADT (rejoint A2/A5 de l'audit 3) | 5 | M | Moyen |
| 24 | P3, S3, S4 | Pads, Sample | ADSR et vélocité ; MIDI complet ; « Trouver la boucle » | 4 | S | Faible |
| 25 | G6 | Galet | Module Temps (moteur de relecture d'Accroc) | 5 | L | Moyen |

**Ordre conseillé.** D'abord 1 à 3 (stabilité et justesse des mesures : une semaine). Ensuite 4 à 9 : six gains « S » très visibles, qui donnent l'impression d'un produit fini. Puis 10 à 17 : les chantiers M qui changent le son, groupés sous `stateVersion` (A1 de l'audit 3) et suivis d'une session d'écoute (A11). Tout changement audible attend l'écoute.

## Idées signature (une par plugin, réalisables avec le moteur existant)

Le fil rouge est celui de l'audit 3 (la suite écoute la voix et la tonalité), étendu au **tempo** et aux **transitoires** :

- **Master, « le grave suit la 808 »** : la coupure de la mono des basses et le filtre de détection du limiteur suivent la fondamentale (YIN).
- **EQ, « EQ dans la tonalité »** : aimantation aux notes, et coupes proposées sur les résonances hors gamme.
- **Cabine, « passe-haut qui suit la voix »** : un passe-haut à 0,7 × f0 en continu.
- **Mix, « le refrain monte-t-il ? »** : comparaison LU, largeur et brillance par blocs de 8 mesures.
- **Orbe, « réponse à la voix »** : des accords qui soutiennent les notes fredonnées, et une lead qui répond dans les silences.
- **Orbe FX, « Orbe écoute le sample »** : la grille d'accords du sample, puis 808 et lead générées par-dessus.
- **Galet, « capturer le groove »** : l'enveloppe d'une mesure de batterie devient la forme.
- **Accroc, « rampe dans la gamme »** : les répétitions montent par degrés de la gamme.
- **Synth, « glide dans la gamme »** : le portamento traverse les notes de la gamme, synchro au tempo.
- **808, « glissé synchro en marches »** : glissés calés au tempo et en escalier sur la gamme.
- **Beat, « groove de la voix »** : charlestons et ghost notes placés sur le flow d'une phrase rappée.
- **Pads, « pads accordés »** : chaque chop est transposé vers le degré le plus proche.
- **Sample, « flip à la syllabe »** : 4 flips tout faits (re-découpe, ordre, degrés, swing).

Toutes demandent une **tonalité et un tempo partagés**. Un petit module commun `Kit/Context.h` (tonique, gamme, BPM, PPQ, dièses ou bémols) éviterait 13 réimplémentations, et préparerait le lien entre instances (« Fil ») de l'audit 3.

## Micro-UX communes (au-delà de A8 de l'audit 3)

Le kit v3 a déjà : double-clic pour remettre à zéro, Maj pour le réglage fin, molette, saisie numérique, redimensionnement.

À ajouter **une fois, dans le kit** :
- infobulles avec la valeur et son unité (seuls Master et Accroc en ont) ;
- **Delta** et écoute à niveau égal sur les effets ;
- Ctrl-clic pour remettre à zéro **un seul axe** d'une poignée ;
- annuler et rétablir pour les **éditions graphiques** (formes, grilles, accords) ;
- pré-écoute des presets ;
- carte MIDI visible ;
- noms de notes en lettres (A, B, C…) à côté des fréquences.

## Stabilité

| Risque | Plugins | État |
|---|---|---|
| Pointeur brut sur une banque ou un rendu, libéré 30 s après son remplacement, alors qu'une voix peut durer 240 s | Pads, Sample | **Ouvert** (P1/S1) |
| GR affiché qui dépend de la taille de bloc | Master | À corriger (crête tenue 50 ms) |
| Changement de bus (sorties séparées, entrée MIDI) ; analyse d'entrée (Orbe FX) à faire hors du thread audio | Beat, Pads, Orbe, Orbe FX | À concevoir et tester dans FL |
| Échange de forme sans attente (Galet) ; tirages à graine par pas (Accroc, Orbe) | Galet, Accroc, Orbe | Sains : à conserver |

## Ce qui n'a pas été vérifié

L'écoute et FL Studio. Le CPU réel. Les fréquences autres que 48 kHz (44,1 kHz pour le WSOLA). La qualité perçue de la reverb partagée dans Synth, Pads et Sample. Le routage MIDI d'un effet dans FL (Orbe FX). Les pages de produits non consultées (CLA Vocals, Kickstart, LFOTool, 808 Studio, Pigments, Massive X, Vital, Arcade, Portal, Battery, FPC, Stutter Edit 2) sont citées **de mémoire** et marquées comme telles dans les fichiers.

## Fichiers

- `21-master.md`, `21-eq.md`, `21-cabine.md`, `21-mix.md`
- `21-orbe.md`, `21-orbe-fx.md`
- `21-galet.md`, `21-accroc.md`
- `21-synth.md`, `21-808.md`, `21-beat.md`, `21-pads.md`, `21-sample.md`

Bancs : `/tmp/b_aud4s/` (`master.cpp`, `pump.cpp`, `mixtp.cpp`, `synth.cpp`, `b808.cpp`, `retrig.cpp`, `wsola.cpp`, `wsola2.cpp`, `fftutil.h`).

## Sources

Les liens sont cités dans chaque fichier `21-*.md` : pages de FabFilter (Pro-L 2, Pro-Q 3), iZotope (Ozone 11, Nectar 4, TBC 2), KVR (ShaperBox 3), Image-Line (Gross Beat), MusicRadar (Effectrix 2, Serato Sample 2), Plugin Boutique (Scaler 3), RouteNote (Serum 2), MusicTech (comparatif des kicks 2026) et Best Service (XO).
