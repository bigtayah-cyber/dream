# Audit 2, n° 1 : bugs DSP (point de vue ingénieur DSP)

*9 nov. 2026. Lecture seule : aucun fichier source n'a été modifié, rien n'a été commité.*
*Méthode : lecture des moteurs et des processeurs, exécution des 10 tests console, puis bancs de mesure jetables, compilés hors du dépôt dans `/tmp/claude-0/-home-claude/12ae9892-af71-5ca8-a97b-93f9fd958b0b/scratchpad/au1/` (`ghost`, `dnhold`, `dnvib`, `vbyp`, `rt2`, `satgain`, `nan`, `tp`, `slap`, `slap2`, `lowcut`, `eqblk`). Build : `/tmp/b_au1`, avec `-j1`.*

## 0. État de départ

**Tests existants** : les 10 passent (`voile_offline_test`, `onek_test`, `orbeeq_test`, `orbedeesser_test`, `orbedenoiser_test`, `orbecabine_test`, `orbemaster_test`, `orbechain_test`, `orbeautotune_test`, `orbe_analysis_test`).

**Corrigé depuis l'audit 1** (vérifié) :
- bypass honoré dans les 14 plugins, sortie exacte et sans clic ;
- EQ lissé (aucun zipper mesuré, même avec des blocs de 1, 7 ou 13 échantillons) ;
- saturations suréchantillonnées ;
- plus d'allocation dans Orbe ;
- latences mesurées égales aux latences déclarées.

**Toujours pas corrigé** : filtres des plugins un potard recalculés sans lissage. Voir M2, qui élargit ce constat à Master, Cabine, DeEsser et Looks.

Les bugs que l'artiste a probablement entendus sont B1, B2 et B3 : des sons « fantômes » et des notes qui disparaissent.

---

## BLOQUANT

### B1. Orbe Autotune : l'harmonie et le doublage rejouent un vieux fragment au début de chaque phrase

- **Fichier** : `Source/Suite/OrbeAutotune/AutotuneDsp.h:478-486` et `:498` (mode Qualité), `:466-475` (doublage en mode Direct).
- **Symptôme** : au début de chaque phrase, l'harmonie ou le doublage fait entendre pendant 20 à 40 ms un morceau de la phrase précédente, souvent une sifflante ou une respiration. On l'entend comme un « flam » ou un « tchk » parasite à chaque reprise.
- **Cause** : `hvT` et `dblS` passent à 0 dès 6 trames non voisées (`:591`, `:595`), soit une consonne ou un silence de plus de 32 ms. Ensuite `hAct` et `dAct` deviennent faux, et en mode Qualité on n'appelle plus `shH`, `fh` (STFT 1024), `dsh` et `ddl`. Leurs mémoires restent figées. À la reprise, elles sont relues pendant que `hvS` remonte en 12 ms. En mode Direct, `splH` et `splD` sont nourris par `feed()`, mais `ddl` ne l'est pas.
- **Mesure** (`ghost`) : voix à 220 Hz, puis 400 ms de souffle non voisé, 600 ms de silence, puis reprise. On compare avec la même entrée où le souffle est remplacé par du silence.

  | Mode | Fantôme dans les 80 ms après la reprise |
  |---|---|
  | Qualité, harmonie à l'octave | **−8,8 dB** sous la voix (crête 0,19) |
  | Qualité, doublage 50 % | **−15,0 dB** |
  | Direct, doublage 50 % | −28,6 dB |
  | Direct, harmonie | −300 dB (pas de fantôme) |
  | Qualité, harmonie et doublage coupés (témoin) | −300 dB |

- **Correctif** : au front montant de `hAct` ou `dAct`, vider les mémoires : `Formant::in`, `acc`, la `line` des `Shifter` et `ddl`. Cela se fait sans allocation, avec `std::fill`. Autre solution : continuer à les nourrir comme en mode Direct, ce qui coûte environ deux STFT de plus.

### B2. Orbe Denoiser : une note tenue plus de 2 s est retirée comme du bruit (réglages d'usine)

- **Fichier** : `Source/Suite/OrbeDenoiser/DenoiserDsp.h:100-109` (minima statistiques sur `kHist`×`subLen` = 8 × 0,25 s) et `:123-128` (`N = 2·min`).
- **Symptôme** : par défaut (mode adaptatif, aucun profil appris), une note tenue chute de 18 dB, toute la valeur de RÉDUCTION, environ 2 s après son début. Sont touchés les fins de phrases longues (« ooooh »), les voix déjà passées dans l'Autotune, les pads et les 808.
- **Cause** : le plancher de bruit est le minimum de la puissance lissée sur environ 2 s. Un partiel stable plus long que la fenêtre devient donc son propre plancher. Avec la sur-soustraction `alpha` = 2, on obtient γ ≈ 0,25 et le gain est plafonné par `floorG`. Le garde-fou « +6 dB maximum » n'existe qu'avec un profil appris (`:128`).
- **Mesure** (`dnhold`, `dnvib`) : La 440 Hz à −20 dBFS dans un bruit à −50 dBFS. Sortie : 0,0 dB à t = 1,5 s et 2,5 s, puis **−18,0 dB** de 3,5 s à 7,5 s. Résultat identique avec un vibrato de 5,5 Hz à ±25 cents.
- **Correctif** :
  1. Garde de tonalité : ne pas mettre à jour `subMin[k]` quand `P[k]` dépasse de plus de 10 dB la médiane de ses voisins (±8 bins).
  2. Allonger l'historique à 5 ou 6 s, ou faire monter le bruit adaptatif lentement (≤ 3 dB/s).
  3. Sans profil, plafonner `N` au minimum à long terme.

  Test d'acceptation : sinus tenu 8 s, perte inférieure à 1 dB.

### B3. Voile : après un bypass de l'hôte, d'anciennes queues de reverb et de delay reviennent, avec un clic

- **Fichier** : `Source/PluginProcessor.cpp:59-72`.
- **Symptôme** : on coupe Voile dans FL (bypass de la piste ou du slot), puis on le remet, même plusieurs minutes plus tard. On entend alors le delay ping-pong, la reverb et les têtes du décaleur **tels qu'ils étaient au moment de la coupure**. Le passage se fait aussi sans fondu, donc avec un clic.
- **Cause** : Voile n'a pas de paramètre `bypass`. JUCE appelle donc `processBlockBypassed`, qui ne fait tourner que la ligne de retard. Le moteur n'est plus appelé et ses tampons restent figés (delay de 2,1 s, FDN, pré-delay, ligne sèche). Les 14 autres plugins ont été corrigés au lot A, mais pas Voile.
- **Mesure** (`vbyp`) : preset proche de « Vapeur », 2 s de voix, bypass de 10 s, reprise sur du silence. Queue fantôme de crête **0,72, soit −2,8 dBFS**, pendant la première seconde.
- **Correctif** : aligner Voile sur les autres plugins. Déclarer un `AudioParameterBool "bypass"`, ajouter `getBypassParameter()` et passer par `bypassfx::Fader` dans `processBlock`, comme dans `OneKnobProcessor.cpp:95`. Le moteur continue de tourner et le fondu dure 10 ms.

---

## GÊNANT

### G1. Cabine et Master : la saturation est surtout une montée de volume (+14 à +20 dB)

- **Fichiers** : `Source/Suite/OrbeCabine/CabineDsp.h:79` et `:120` (`tanh(kx)/tanh(k)`), `Source/Suite/OrbeMaster/MasterDsp.h:217` et `:229`.
- **Symptôme** : tourner SATURATION ou DRIVE fait d'abord monter le niveau. On croit que « ça sonne mieux » parce que c'est plus fort. En plus, le compresseur et le limiteur placés après réagissent autrement, et la comparaison A/B est faussée.
- **Cause** : la courbe est normalisée à 1 en x = 1, pas à pente 1 en zéro. Le gain en petits signaux vaut `k/tanh(k)`. Dans Cabine, `P.sat` n'est en plus **pas lissé** (marches de gain à l'automation).
- **Mesure** (`satgain`, sinus 1 kHz, compresseur et envois coupés) :

  | Plugin | Entrée | Réglage | Gain |
  |---|---|---|---|
  | Cabine | −30 dBFS | sat 1 / 2 / 5 / 10 | +2,4 / +6,3 / **+13,9** / **+19,8 dB** |
  | Cabine | −6 dBFS | sat 10 | +8,4 dB |
  | Master | −30 dBFS | sat ON, réglage d'usine (drive 0,25, mix 0,3) | **+5,1 dB** |
  | Master | −30 dBFS | drive 1, mix 1 | **+20,1 dB** |

- **Correctif** :
  - Cabine : `tanh(k·x)/k`, pente 1 en zéro, suivi d'une compensation de niveau (RMS lent) ; lisser `sat` sur 20 ms.
  - Master : même forme, puis corriger le mélange `dry·(1−0,6·mix)` hérité du prototype pour obtenir un gain unité à bas niveau.
  - Test d'acceptation : sinus −30 dBFS, |gain| < 0,5 dB sur toute la course.

### G2. Un seul échantillon NaN rend six moteurs muets ou NaN pour toujours

- **Fichiers** : `Source/Engine.h:469` (Voile), `Source/OneKnob/OneKnob.h:285-345`, `Source/Suite/OrbeChain/OrbeChain.h:205` (`fin()` masque la sortie mais pas l'état interne), `EqDsp.h`, `DeEsserDsp.h`, `MasterDsp.h`.
- **Symptôme** : si un plugin placé avant envoie un seul NaN ou Inf (glitch, plugin buggé), le plugin reste silencieux ou envoie des NaN jusqu'au prochain `prepareToPlay`. Dans FL, des NaN peuvent faire taire tout le bus.
- **Mesure** (`nan`, NaN au bloc 10, puis 4 s de sinus) :
  - Voile, un potard, EQ, DeEsser et Master : **sortie NaN définitivement** ;
  - Looks et Atelier : **sortie muette définitivement** ;
  - Cabine (qui a un `reset()`), Autotune et Denoiser résistent.
- **Correctif** : dans chaque `processBlock`, nettoyer l'entrée. Remplacer les valeurs non finies par 0, de préférence dans une fonction commune `kit::sanitize(buffer)` appelée avant le Fader. En plus, sur le modèle de Cabine, faire un `reset()` du moteur quand la sortie devient non finie.

### G3. Voile et les un potard : la reverb retombe à 1,2 s après chaque `prepareToPlay`

- **Fichier** : `Source/Engine.h:376` (`rev.setDecay(1.2f)` dans `prepare`) et `:417`. `lastRt` n'est pas remis à zéro dans `prepare`.
- **Symptôme** : quand l'hôte rappelle `prepareToPlay` (changement de buffer ou de fréquence, réactivation), la reverb devient plus courte. Elle reste ainsi tant que BRUME ne bouge pas de plus de 0,02. Le même projet sonne donc différemment d'une session à l'autre.
- **Mesure** (`rt2`) : Brume à 100 %, RT60 attendu 2,5 s. **2,50 s avant**, **1,20 s après** un second `prepare` (à 48 et à 96 kHz).
- **Correctif** : dans `prepare`, mettre `lastRt = -1.f`, ou appeler directement `rev.setDecay(0.8f + 1.7f * P.brume)`. OrbeChain fait déjà correctement (`lastValid=false`).

### G4. Allocations dans le thread audio (nouvelles depuis l'audit 1)

- **Fichiers** : `Source/Suite/OrbeMaster/MasterProcessor.cpp:57`, où `"eq" + String(i)` alloue 7 chaînes par bloc ; `Source/Suite/OrbeAutotune/AutotuneProcessor.cpp:58`, où `"x" + String(i)` en alloue 12 par bloc ; `:98` et `:108`, où `scratch.setSize` alloue si l'hôte envoie un bloc plus grand qu'annoncé (ce que FL fait).
- **Symptôme** : craquements (xruns) intermittents à petit buffer, surtout quand plusieurs instances tournent.
- **Correctif** : précharger les `std::atomic<float>*` dans le constructeur (comme `EqProcessor::rawPtr`) et dimensionner `scratch` à 8192 dans `prepareToPlay`.

### G5. Orbe Mix sous-estime la crête vraie jusqu'à 1,07 dB

- **Fichier** : `Source/Suite/OrbeAnalysis/OrbeAnalysis.cpp:265-273`. L'interpolation se fait en Catmull-Rom sur 4 points, un calcul qui n'est pas limité en bande.
- **Symptôme** : Mix affiche par exemple −1,0 dBTP alors que la crête vraie est à 0. Un master validé « −1 dBTP » peut écrêter après encodage.
- **Mesure** (`tp`) : sinus à fs/4 et phase de 45°, crête vraie 0,00 dBTP. **Mix affiche −1,07 dBTP** à 44,1 et à 48 kHz.
- **Correctif** : réutiliser `mas::TruePeak` (FIR polyphasé 4× de 24 coefficients, déjà validé à 0,06 dB près par `master_test`).

### G6. Un potard (Poupée, Gorge) : un écho fantôme quand le potard repasse le seuil du slapback

- **Fichier** : `Source/OneKnob/OneKnob.h:312-316`. Le tampon `slapL/R` n'est écrit que si `slapMix > 0`.
- **Symptôme** : on automatise Poupée sous 5, puis on remonte. On entend alors un écho de la dernière voix jouée avec le slapback actif, même si c'était il y a plusieurs minutes.
- **Mesure** (`slap`, `slap2`) : Poupée à 9 sur une voix, puis 4 s de silence à 3, puis retour à 9 sur du silence. Crête de sortie de **0,033 (−29,6 dBFS)** de vieil audio. Témoin (phase intermédiaire à 5,2, slapback actif) : −228 dBFS.
- **Correctif** : toujours écrire dans `slapL/R` (deux écritures par échantillon, négligeable), et ne conditionner que la lecture.

### G7. Le son change avec la fréquence d'échantillonnage

- **Fichiers** :
  - `Source/Engine.h:450`, coupe-continu `0,995` fixe : fc ≈ 35 Hz à 44,1 kHz, 76 Hz à 96 kHz. Il est appliqué au signal traité même quand tous les potards sont à 0 ;
  - `Engine.h:444-445` (maintien de Mordant), `OneKnob.h:318` (lo-fi `hn`) et `OrbeChain.h:156-162` (crush et coupe-continu) comptent en échantillons et non en secondes ;
  - `Engine.h:427` (constantes du suiveur `lvl`, donc du ducking).
- **Symptôme** : à 96 kHz, Voile perd du bas (voir la mesure). Le lo-fi de Grain Nord à fond et Mordant ou Crush perdent leur grain, parce que les images montent au-dessus de 20 kHz. Le ducking réagit deux fois plus vite.
- **Mesure** (`lowcut`, Voile tous potards à 0, Mix 100 %) :

  | Fréquence | 44,1 kHz | 96 kHz |
  |---|---|---|
  | 80 Hz | −0,75 dB | **−2,80 dB** |
  | 60 Hz | −1,26 dB | **−4,18 dB** |

- **Correctif** : coefficient `exp(−2π·20/sr)`, maintien en secondes (`h = max(1, round(m²·4·sr/44100))`), constantes du suiveur en millisecondes.

---

## MINEUR

**M1. Un potard : le « limiteur de sécurité » colore en permanence.**
- `OneKnob.h:338-339` : `tanh(0,9x)/0,9·0,95` est appliqué à tout le signal.
- Calcul (sinus 1 kHz) :

  | Niveau d'entrée | Gain | THD |
  |---|---|---|
  | −18 dBFS | −0,47 dB | −60 dB |
  | −6 dBFS | −0,87 dB | **−36 dB** |
  | −2 dBFS | −1,46 dB | −28 dB |

- Correctif : laisser passer à l'identique sous −6 dBFS (genou), et n'appliquer le tanh qu'au-dessus.

**M2. Paramètres non lissés (clics ou marches à l'automation).**
- Un potard : filtres de CARACTÈRE et de TON (non corrigé depuis l'audit 1).
- Master : bandes d'EQ, makeup et seuil du compresseur, bascules ON (`MasterDsp.h:247-258`).
- Cabine : EQ, `sat`, seuil.
- DeEsser : fréquence, mode Split/Large, ÉCOUTE (`DeEsserDsp.h:22`, `:51`).
- Looks : changement de look, qui fait sauter d'un coup les coefficients d'EQ et les rapports des doubles.
- Correctif : appliquer le schéma `glide`/`retarget` de l'EQ (rampe des coefficients toutes les 16 éch.) ou un fondu enchaîné de 20 ms, à la manière de `eq::Engine`.

**M3. États figés à la réactivation d'un module.**
- OrbeChain : avec l'EQ coupé, seul `hpF` est nourri (`OrbeChain.h:143`) ; le coupe-continu de la saturation est figé (`:162`).
- Voile : les filtres demi-bande de `sat` sont figés sous Mordant 0,0005.
- Cabine : `lp` est figé quand on le coupe.
- Effet : petit transitoire à la réactivation, atténué par la rampe. Correctif : nourrir les filtres en permanence, ce qui coûte peu.

**M4. Denoiser : profil appris lié à la fréquence d'échantillonnage.**
- Le profil (1025 bins) est enregistré sans la fréquence (`DenoiserProcessor.cpp:37-48`). Rouvert à 96 kHz, il s'applique aux mauvaises fréquences.
- L'apprentissage ne lit que la voie gauche (`DenoiserDsp.h:118`).
- Correctif : enregistrer `sr` avec le profil, l'invalider (ou le rééchantillonner) s'il ne correspond pas, et faire la moyenne des deux voies pendant l'apprentissage.

**M5. Compteurs `int` qui débordent après 12,4 h à 48 kHz.**
- Concerne `Chan::count` du Denoiser (`DenoiserDsp.h:87`, `:104`) et `odsp::Analyzer::wpos`. FL fait tourner les plugins en permanence, même à l'arrêt.
- C'est un comportement indéfini en C++. En pratique, après le débordement, `c.count <= kHop` reste vrai pendant 12 h : le lissage des minima est désactivé et le débruitage devient instable.
- Correctif : compteur modulo `kHop` et `uint32_t`.

**M6. Autotune : le changement de mode Direct/Qualité coupe net le son.**
- Le comportement est assumé dans le commentaire (`AutotuneDsp.h:438-446`), mais les lignes sèches sont remises à zéro, sans fondu.
- Correctif : fondu de 10 ms entre les deux sorties pendant la bascule.

**M7. Détect : les blocs silencieux sont jetés de la capture.**
- `DetectProcessor.cpp:56` : `pk < 1e-4` saute le bloc.
- Un break ou un silence d'une mesure dans un beat est retiré, ce qui décale la grille d'attaques et fausse le BPM.
- Correctif : ne pas capturer quand le transport est arrêté, mais garder les silences pendant la lecture.

**M8. Un potard : décalage de ≈ 1 échantillon entre traité et sec.**
- Les tests le montrent : latence mesurée 815 ou 816, déclarée 814. Le retard vient de l'ADAA (½ échantillon) et du drive de Patine (½ échantillon de plus).
- Avec MIX à 50 % et le drive de Patine (≈ 1 échantillon), on obtient un filtre en peigne de −2 dB à 10 kHz (calcul).
- Correctif : compenser ce retard fractionnaire sur la ligne sèche (lecture en `latency + 0,5` ou `latency + 1`).

---

## Priorités

B1, B2, B3, puis G2 et G4 (robustesse), puis G1 et G3. Chaque banc du dossier `au1` peut servir de test de non-régression.
