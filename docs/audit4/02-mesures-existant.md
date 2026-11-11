# Orbe Autotune : mesures de l'existant (audit 4)

*10 nov. 2026. Branche `vague2-wip`. Rien n'a été modifié dans le dépôt. Bancs hors dépôt : `/tmp/b_at4/bench.cpp`, `bench2.cpp`,
`bench3.cpp`, `bench4.cpp` (g++ -O2, en-têtes sans JUCE, chemin complet `atn::Engine` comme dans le plugin). Sorties brutes :
`/tmp/b_at4/out1.txt` à `out4.txt`.*

## 0. Ce qui a été mesuré, et ce qui ne l'a pas été

- **Signal de test plus dur que celui de `tests/tune_core_test.cpp`.** Les tests du dépôt utilisent une somme d'harmoniques
  parfaitement périodique. Ici, voix de synthèse « source-filtre » : impulsion glottique (Rosenberg), **jitter 0,6 %**, **shimmer
  4 %**, **souffle** modulé par l'ouverture glottique, 4 formants en cascade qui glissent de /a/ à /i/, notes décalées de ±15 à
  ±40 ct, **scoop d'attaque** (−80 ct, τ 30 ms), vibrato 5,6 Hz ±35 ct sur les notes longues, sifflantes entre certaines notes.
- **Mesure de hauteur de sortie** : YIN plein débit hors-ligne (fenêtre 1024 ou 512, pas 128 ou 64). Plancher de mesure vérifié :
  une voix déjà juste donne un « temps d'accroche » de 9 ms (médiane), un signal qui alterne deux notes 11 fois/s donne 11,0 bascules/s.
- **Pas mesuré** : rien n'a été écouté (le point A11 d'audit 3 reste ouvert) ; aucune vraie prise (pas de fichier WAV fourni) ;
  aucune comparaison chiffrée avec MetaTune ou Auto-Tune (pas de binaire ici). Les écarts « vs MetaTune » de ce document sont
  des écarts aux **fonctions documentées** de MetaTune (voir 01), pas à son son mesuré.
- Configurations : `Classique` (ancien moteur), `Orbe Précis`, `Satin`, `Robot`, chacune en Direct (221 éch., 5 ms) et Qualité
  (1729 éch., 39 ms), gamme chromatique, tonalité C, sauf mention.

## 1. Résultats

### [A] Mélodie réaliste, vitesse 0 (sauf Satin : glissement 25 ms)

| Config | % trames ≤ 10 ct | erreur médiane / p90 (ct) | gigue sur note tenue (ct) |
|---|---|---|---|
| Classique Qualité | 84,3 | 2,4 / 19,4 | 1,9 |
| Classique Direct | 66,9 | 6,0 / 23,0 | 4,1 |
| **Orbe Précis / Robot Qualité** | **90,0** | 2,5 / 10,0 | 1,9 |
| **Orbe Précis / Robot Direct** | **87,0** | 3,5 / 13,0 | 3,8 |
| **Satin Qualité (vitesse 0)** | **28,6** | 21,2 / 39,6 | 5,2 |
| **Satin Direct (vitesse 0)** | **28,1** | 22,0 / 41,1 | 4,8 |

Lecture : le moteur Orbe en Précis/Robot est le meilleur du dépôt. **Mais en rendu Satin, même à vitesse 0, la voix n'est pas
« posée »** : 28 % des trames seulement à 10 ct de la grille. C'est voulu pour Satin (vibrato gardé, glissement du 2e ordre,
expression ≥ 0,8), **mais c'est le rendu par défaut d'une nouvelle instance** (voir constat 1 ci-dessous).

### [A'] Temps d'accroche de l'attaque (scoop −80 ct), `bench4`

| Config | vitesse 0 | vitesse 12 |
|---|---|---|
| plancher de mesure (voix déjà juste) | 9 ms | |
| Classique Qualité | 10,5 ms | 68,6 ms |
| Classique Direct | 68,6 ms | 72,9 ms |
| **Orbe Qualité** | **10,5 ms** (≈ immédiat) | 29,0 ms |
| **Orbe Direct** | **33,3 ms** (≈ 24 ms d'attaque non corrigée) | 30,4 ms |
| Satin Qualité / Direct | 38,7 / 40,6 ms, et certaines notes jamais à 10 ct | idem |

Lecture : en **Direct** (mode par défaut), les ~20 à 25 premières ms de chaque note passent **non corrigées** même à vitesse 0 :
la fenêtre d'analyse (768 éch.) doit se remplir et le Viterbi paie un coût de bascule non voisé → voisé. Le « coup de glotte »
accordé dès l'attaque, signature du hard-tune, est donc absent en Direct. MetaTune annonce ≈ 3 ms en Low Latency et une vitesse
« < 0 ms, sans anticipation » (SOS) : c'est précisément sur l'attaque que se joue la différence.

### [B] Glissando A3 → C#4 en 400 ms, vitesse 0 : netteté de l'escalier

| Config | trames « entre deux notes » (> 25 ct) | paliers (attendus ≈ 4-5) |
|---|---|---|
| Classique Q / D | 5,4 % / 8,3 % | 8 / 7 (rebonds) |
| Robot Q / D | **0,8 % / 1,2 %** | 4 / 4 |
| Satin D | 55,3 % | 5 |

L'escalier du robot Orbe est net : bon point.

### [C] Voix tenue à mi-chemin (A3 + 50 ct) avec vibrato ±25 ct : « warble » et raccords de transition

- **Bascules de note** : la sortie alterne 57 / 58 environ **10 fois par seconde** (relevé de trace), dans tous les modes non-Satin.
  L'hystérésis de 0,2 dt sur `c1` (passe-bas 30 ms) n'arrête pas un vibrato qui chevauche la frontière. C'est exactement le
  « warble » d'Auto-Tune classique (recherché par certains) que MetaTune supprime avec **Note Stabilizer** (40 / 80 / 200 ms)
  et qu'Auto-Tune traite par « Targeting ignores vibrato ». **Orbe n'a ni l'un ni l'autre, ni un réglage pour choisir.**
- **Trames irrégulières à chaque bascule** (`bench3`, sortie non périodique ou en sous-harmonique alors que l'entrée est voisée) :

| Config | warble (C') | sauts nets de 2 dt toutes les 150 ms (B') |
|---|---|---|
| entrée | 0 % | 0,6 % |
| Classique Q / D | 10,2 % / 11,3 % | 3,7 % / 2,8 % |
| **Orbe Précis Q / D** | **12,2 % / 12,9 %** (≈ 11 ms par bascule) | 2,2 % / 4,0 % |
| Satin Q / D | 0 % / 0 % | — / 4,2 % |

Lecture : **chaque changement de note en hard-tune produit ≈ 10 ms de signal non périodique** (raccord du lecteur pendant que le
rapport saute de ~6 %). Sur un saut net d'entrée, la sortie est 4 à 7 fois plus irrégulière que l'entrée. C'est un candidat
sérieux pour le « grain » / « incomplet » entendu, **à confirmer à l'oreille** (la mesure YIN d'une transition est elle-même
imparfaite ; l'entrée de contrôle donne 0 à 0,6 %).

Instrumentation de `TuneCore` seul (`bench5`, Direct, vitesse 0) : **formants préservés ou non, même résultat** (13,1 % / 12,8 %) :
la LPC n'est pas en cause, c'est le **lecteur à raccords au moment où le rapport saute** (le décalage appliqué passe de +0,82 à
−0,11 dt en deux trames de 128 échantillons). Même ordre de grandeur pour l'ancien `dsp::Splicer` (Classique Direct 11 %).
Le journal montre aussi que, pendant le warble, la note visée reste à 58 alors que la voix est à 57,17 (correction de +83 ct) :
`c1` (passe-bas 30 ms) + hystérésis retardent la bascule, puis elle tombe d'un coup. Le mécanisme exact du raccord fautif
(déclenchement forcé par le déplacement de la fenêtre `lo`, `TuneCore.h:521`, ou fondu trop court) **reste à instrumenter**.

### [D] Peigne du mélange partiel (Précis, correction de 0,5 à 3 ct)

Voix déjà juste à +2 ct, écart d'enveloppe sortie / voix de référence (harmoniques 200-4000 Hz) :

| Config | +2 ct | +30 ct |
|---|---|---|
| **Orbe Précis Q / D** | **2,5 dB RMS, pire harmonique 5,9 / 7,0 dB** | 1,0 dB |
| Classique Direct | 2,1 dB (pire 4,9) | 1,3 dB |
| Satin Direct | **0,14 dB** | 1,0 dB |

Cause, dans le code : `TuneCore.h:153`, `a = sat((cents − 0,5) × 0,4) × wetG` mélange **sec (retard L)** et **traité (retard
oscillant entre L − T0/2 et L + T0/2)** quand la correction est faible. Deux copies à retards différents = filtre en peigne. Satin
l'évite déjà (`a = wetG`). En pratique, une voix bien chantée passe une bonne partie de ses trames dans cette zone : **coloration
« tubulaire / phasée » sur les notes justes**. Mesuré, cause identifiée, correction simple.

### [E] Formants et pente spectrale en transposition (voyelle /a/, voix grave D3 147 Hz)

Erreur d'enveloppe (dB RMS, après retrait de la moyenne) et pente ajoutée (dB/octave) — Orbe, identique Direct / Qualité :

| Transposition | erreur | pente ajoutée | résidu hors pente |
|---|---|---|---|
| −7 dt | 13,6 dB | **−9,6 dB/oct** | 10,0 dB |
| −3 dt | 5,0 dB | −2,7 dB/oct | 4,3 dB |
| −1 dt | — | −0,4 dB/oct | 1,7 dB |
| +1 dt | — | −0,1 dB/oct | 0,8 dB |
| +3 / +7 dt | 1,4 / 1,3 dB | +0,4 / +0,6 dB/oct | 1,4 / 1,3 dB |
| (Classique Q / D à ±3 dt) | 17-18 / 7-10 dB | | |

Lecture : **vers le haut, les formants Orbe sont excellents ; vers le bas, la voix s'assombrit vite** (−2,7 dB/oct à −3 dt, voix
« étouffée » à −7). Cause probable (cohérente avec `tune-algo.md` § 9) : le lecteur **rééchantillonne le résidu**, donc étire
l'impulsion glottique quand on descend. Impact : faible pour la correction (± 1 dt), fort pour Transposer vers le bas, la
« Grosse voix », l'harmonie basse, l'octave basse.

### [F] Doublage et harmonie : modulation d'amplitude ajoutée (note tenue stable)

| | AM (écart-type 10 ms, dB) |
|---|---|
| entrée | 0,21 |
| Orbe Q doublage 50 % / harmonie 50 % | 0,64 / 0,44 |
| Orbe D doublage / harmonie | 0,72 / 0,61 |
| Classique Q doublage / harmonie | 2,20 / 2,16 |

Lecture : correct en Orbe, mais le doublage reste **deux copies à désaccord fixe (±5 à ±15 ct) et retards fixes (11 / 17 ms)**
(`AutotuneDsp.h:494-500`) : battements périodiques, aucune variation de temps, de hauteur ou de timbre (ce n'est pas un ADT).
L'harmonie Qualité passe par le décaleur à deux têtes (`voile::HeadShifter`, granulaire), l'harmonie Direct par le `Splicer`
**sans formants** (une tierce au-dessus en Direct = timbre « poupée »). Une seule voix d'harmonie, au centre.

### [G] Voix très soufflée / chuchotée (souffle ×8, jitter 1,5 %), A3 + 35 ct, robot

Trames voisées en entrée corrigées à ≤ 15 ct en sortie : Classique Q / D 13,7 / 12,8 % ; **Orbe Q / D 11,4 / 4,8 %**.
Lecture : sur une voix très soufflée (type ad-lib chuchoté, Carti « baby voice » soufflée), **le moteur la classe non voisée et
la laisse passer sèche** (porte `0,42 − hf`, périodicité > 0,5). Aucun réglage « Suivi / Tracking » comme chez Auto-Tune.
Prudence : le cas de test est extrême (souffle très fort) ; le seuil réel où ça décroche n'a pas été cherché.

### [I] Le souffle est-il « répété » (bourdonnement) ? (`bench6`, test d'une hypothèse de `04`)

Voix tenue avec souffle (0,15 et 0,40), part d'énergie **hors harmoniques** (|f − h·f0| > 20 Hz, 150-6000 Hz). Si le lecteur
répétait le souffle avec les périodes, cette part baisserait nettement (le bruit deviendrait harmonique).

| Souffle | référence juste (A3 / B3) | Orbe Q corr. −35 ct | Orbe D corr. −35 ct | Orbe D +2 dt | Classique D corr. |
|---|---|---|---|---|---|
| 0,15 | −16,3 / −14,1 dB | −15,2 | −14,5 | −13,6 | −15,2 |
| 0,40 | −8,3 / −6,6 dB | −7,8 | −6,6 | −7,3 | −7,7 |

Lecture : **pas de preuve de bruit répété** sur ce signal (écarts ≤ 1,7 dB, et dans le sens « plus de bruit », pas moins). Un
lecteur qui ne raccorde que toutes les T0/|1 − r| périodes ne répète qu'une petite part du signal. Formants libres ou préservés :
même résultat (≤ 0,6 dB d'écart). L'hypothèse (b) de `04` n'est donc pas confirmée ici ; elle pourrait l'être sur de grands
décalages ou de vraies voix soufflées (non testé).

### [H] CPU et latence

| Config | seul | + doublage + harmonie |
|---|---|---|
| Classique Q / D | 2,25 % / 0,51 % | 4,82 % / 0,91 % |
| **Orbe Q / D** | **3,34 % / 3,12 %** | 5,60 % / 4,37 % |

(1 cœur, 44,1 kHz, stéréo, `-O2`, machine partagée : ordre de grandeur.) Latences : Direct 221 éch. (5 ms), Qualité 1729 éch.
(39 ms). MetaTune : ≈ 3 ms (SOS) ; Waves Tune RT : 0 déclaré + ≤ 4 ms internes. **Marge CPU confortable** pour un moteur plus
riche (×3 reste < 10 %).

## 2. Constats issus de la lecture du code (non mesurés, vérifiables)

1. **Défaut d'usine qui tue le hard-tune.** Nouvelle instance : `rendu = Satin` (`AutotuneProcessor.cpp:42`), `latmode =
   Direct` (`:35`). Les presets « Robot », « Trap moderne », « Hyperpop », « Pop », « Naturel », « Grosse voix », « Harmonie »
   ont `render = −1` (`AutotuneDsp.h:56-62`) et **ne changent pas le rendu** (`AutotuneProcessor.cpp:89`). Charger « Robot » sur
   une instance neuve donne donc **Satin à vitesse 0** : 28 % de trames posées au lieu de 87 % ([A]). L'artiste qui « essaie le
   preset Robot » n'entend pas le hard-tune.
2. **Pas de « Humanize / Sustain »** (vitesse qui dépend de l'âge de la note), **pas de Note Stabilizer**, **pas de Flex-Tune**
   (la Tolérance fait l'inverse : zone morte près de la note), **pas de vitesse négative**, **pas de « Classic »** distinct.
3. **Rendu = 3 modes exclusifs** (Satin / Précis / Robot) + Vitesse + Expression + Glissement : chevauchements (Robot = Précis à
   vitesse 0), et Satin change aussi le sens de Mix (dose de correction). Lisible pour nous, pas pour un rappeur.
4. **Aucun effet de couleur intégré** (saturation, air, de-esseur, compression, delay, reverb). Ni MetaTune ni Auto-Tune Pro
   n'en ont (MetaTune : doubleur seul), mais Little AlterBoy a un Drive, Graillon 3 un préampli, compresseur, chorus, doubleur.
5. **Note forcée par MIDI non branchée** (choix assumé, `tune-algo.md` § 9) : pas de mode « Robot joué au clavier » (Little
   AlterBoy, Waves Tune RT, Auto-Tune l'ont).
6. **Tonalité Auto** : profils de Krumhansl sur la voix seule, verrouillage lent (≥ 5 évaluations de ~85 ms, poids ≥ 150) ; tant
   que non verrouillé, **aucune correction** (`mode == 2 → mask 0`). Début de morceau en Auto = voix non accordée.
7. **Édition de notes** : capture liée à la timeline, pas d'ARA, pas de multi-sélection, pas de glisser en temps, pas d'export MIDI
   (déjà listé § 9 de `tune-algo.md`). Correct pour une v1, loin de Melodyne.
8. **Formants volontaires (« Formants » ±6 dt) seulement en Qualité** (étage STFT) ; pas de « Gorge / Throat » distinct en Direct.

## 3. Ce qui va bien (à garder)

- Escalier net en robot ([B]), justesse ≤ 0,2 ct sur voix stable, vibrato préservé en naturel (tests du dépôt), formants vers le
  haut ([E]), consonnes et souffle rendus intacts (sec exact), zéro allocation, CPU modeste, latence Direct 5 ms.
- Séparation centre / expression de la courbe de correction : bonne base pour Humanize, Flex et Stabilizer.
