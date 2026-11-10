# Validation des lots 1, 2B et 2A

*Validateur indépendant, 10 nov. 2026, vers 02 h CET, branche `vague2-wip` au commit `da4b87f`. Aucun fichier source modifié, rien de commité.
Méthode allégée : les rapports des agents font foi ; contrôles par sondage seulement. Critères : `08-criteres-validation.md`.
**Non évalués (en cours d'édition)** : `Source/OneKnob/*`, `Source/Suite/OrbeAccroc/*`, `Looks.h`, `Source/Kit/v3/*`, `PluginEditor.*`, `AutotuneEditor.*`.*

## Mesures faites

| Contrôle | Résultat |
|---|---|
| Tests console (g++ -O2, sans JUCE) : tune_core, autotune, galet, dsp2b, orbe808, chain, master, offline (Voile) | **8/8 verts** (« tout est bon », code 0). Chain : latence 885, bypass −400 dB. Master : latence 171, bypass −400 dB |
| pluginval niveau 10 (`--skip-gui-tests`) | **Voile SUCCESS**, **Orbe Autotune SUCCESS** (binaires `/tmp/b_2a`, construits après les dernières sources), **Orbe 808 SUCCESS** (`/tmp/b_2b`), **Orbe Galet SUCCESS** (construit dans `/tmp/b_val`) |
| IDs APVTS (`git diff 154c0f9 main` puis `main HEAD`) | Aucun ID supprimé ni modifié. Ajouts en fin de liste : `bypass` (Voile), `algo`, `expr`, `accid` (Autotune). Vague 2 : aucun changement de paramètre hors Accroc |
| Nouveaux booléens APVTS | Aucun (choix à 2 entrées) |
| Ancien projet Autotune | Sans `algo` → moteur Classique (`AutotuneProcessor.cpp:177-179`) : OK |
| MSVC (grep des constructions à risque) | Rien trouvé dans les fichiers des lots |
| GPL | Aucun en-tête GPL ni nom chowdsp/ChowTape ; provenance écrite (`Saturation.h:2`) |
| Banc NaN de l'audit 7 rejoué sur le code actuel | **Galet : NaN en sortie 3 s après, énergie 0.** (Même chose sur les 4 un-potard, lot en cours) |

## LOT-1 : Autotune Orbe, Galet, correctifs P0

**Verdict : GO SOUS RÉSERVES pour Autotune et les correctifs P0 ; NO-GO pour Galet tant que G2 n'est pas corrigé.**

- Autotune : NaN nettoyé en entrée et en sortie (`TuneCore.h:122,169`).
- **Anomalie 1 (bloquante, Galet)** : aucune protection de l'entrée audio. `GaletProcessor.cpp:158-159` passe le tampon de l'hôte au moteur sans `kit::sanitize` ; `gal::sanitize` (`GaletDsp.h:33`) ne nettoie que la forme. Un NaN le rend muet jusqu'au rechargement.
  *Correction* : `kit::sanitize` sur l'entrée avant `fader.run`, et remise à zéro des états du moteur si la sortie n'est pas finie. *Test à ajouter* dans `galet_test.cpp` § 8 : NaN injecté pendant un bloc, sortie finie et énergie > 0 100 ms après.
- Anomalie 2 (réserve) : le preset « Pompe sidechain » n'a pas d'entrée sidechain. Ajouter le bus **avant** toute diffusion (ajout plus tard = changement de configuration de bus des projets), ou renommer le preset.
- Correctifs P0 : acceptés sur la foi de l'audit 7. Deux n'ont pas de test de non-régression : B2 (Denoiser, note tenue) et G1 (gain de saturation < 0,5 dB). À ajouter.

## LOT-2B : briques Dsp (Compressors, Saturation, DeEssDyn) et 808

**Verdict : GO SOUS RÉSERVES.**

- **Anomalie 3** : les trois briques ne sont **branchées nulle part**. Aucun `#include "Dsp/Compressors.h"`, `Saturation.h` ou `DeEssDyn.h` hors de `Source/Dsp`. Même Accroc, qui annonce des compresseurs lampe, opto et FET, ne les inclut pas (`AccrocDsp.h:9-17` : seulement la bibliothèque standard). *Correction* : soit Accroc utilise `dsp::Compressors`, soit le rapport du lot 2B dit « briques de référence non utilisées ». À trancher au lot Accroc.

## LOT-2A : Splicer, Engine (décaleur, reverb FDN 8 lignes, ADT), OrbeChain

**Verdict : NO-GO administratif.** Il sera levé par une décision écrite, sans code à changer.

- Technique : tests, pluginval et latence de Voile OK.
- **Anomalie 4 (bloquante selon le critère U12)** : le son de **9 plugins** change (Voile, 4 un-potard, Looks, Atelier, plus la reverb partagée de Cabine, Synth, Pads et Sample), sans `stateVersion`, sans mode v1 et sans `CHANGELOG`. Le seul `stateVersion` du dépôt est celui d'Accroc (`AccrocProcessor.cpp:123`).
  *Correction* : l'artiste choisit « accepter et l'écrire dans le CHANGELOG » ou « garder un mode v1 pour les états sans version ». Écrire `stateVersion = 2` dès maintenant.
- Anomalie 5 (réserve) : les doubles ADT ne sont pas dans Looks ni dans Atelier. `OrbeChain.h:257` garde `voile::Shifter tune, dbl1, dbl2`. Le rapport doit le dire, ou il faut brancher `dsp::AdtVoice`.
- Anomalie 6 (réserve, G7) : le coupe-continu `0.995f` est fixe (`Engine.h:581`), donc le son varie avec la fréquence d'échantillonnage. Il faut un coefficient calculé à partir de `sampleRate`.

## Risques résiduels

- La vague 2 **n'a jamais été compilée sous MSVC** (CI sur `main` seulement). Le grep ne remplace pas une compilation.
- pluginval sans tests d'interface.
- Sur 4 un-potard, un NaN en entrée persiste (banc rejoué). À traiter dans le lot un-potard en cours.

## À valider à l'oreille ou dans FL Studio (seulement là)

Les 8 points de `tune-algo.md` § 10 ; le son des anciens projets Voile, Looks et un-potard avant et après la vague 2 ; la nouvelle reverb sur les presets de Cabine, Synth, Pads et Sample ; Galet (« Pompe », ADAA à fort drive) ; le bloom de la 808 ; la checklist FL n° 8 (PDC Autotune), n° 9 (bypass) et n° 12 (rechargement de projet).

## Procédure légère pour les lots suivants (un-potard, presets Looks, Accroc, kit v3 / Voile / Autotune design, Relance)

1. Lire le rapport de l'agent. Le croire sauf incohérence flagrante avec le diff (`git diff <commit précédent> --stat`).
2. `git diff <base> -- Source | grep -E '^[-+].*(ParameterID|AudioParameter)'` : aucune suppression ni modification, ajouts en fin de liste, pas de booléen. Pour les presets, les index existants ne bougent pas.
3. Grep des risques : `sanitize` présent dans `processBlock`, `stateVersion` écrit, `M_PI|__attribute__|ssize_t|\x[0-9A-F]{2}[0-9A-Fa-f]`, GPL, et les briques annoncées réellement incluses.
4. Un test console du lot (`g++ -O2 -std=c++17 -I Source <test>.cpp`) et le banc NaN (`scratchpad/ctl/nan.cpp`).
5. Un pluginval niveau 10 sur un binaire du lot. Avec tests d'interface pour le lot kit v3 / design.
6. Verdict selon `08` § C, en 300 mots maximum : anomalies avec fichier:ligne, puis ce qui reste à écouter.
