# Audit 3, n° 5 : qualité logicielle

*Lecture du code et des workflows CI. Aucun build ni pluginval relancé : la machine était saturée. Orbe Mix exclu (deux agents en cours).*

## 1. Ce qui est solide

- **Restauration d'état défensive** là où c'est risqué. Profil du Denoiser : taille vérifiée et valeurs non finies remises à zéro (`DenoiserProcessor.cpp:50-56`). Notes d'Autotune : format versionné, champ par champ, bornes vérifiées (`NoteEdit.h:155-190`). Le format est portable entre Mac et Windows.
- **Thread audio.** Autotune lit les éditions avec un `ScopedTryLock`, jamais bloquant (`AutotuneProcessor.cpp:132`). Pads et Sample publient la banque par pointeur atomique (`SamplerCommon.h:26-45`) ; les mutex ne sont pris que côté interface et chargement.
- **Garde-fous NaN** ajoutés partout (vague 2). Liste explicite des plugins attendus dans la CI.

## 2. Risques (par gravité)

| # | Risque | Preuve | Correction |
|---|---|---|---|
| R1 | **Version d'état absente sur 23 plugins sur 26.** Seuls OneKnob, Accroc et Relance écrivent `stateVersion`. Or le son de Looks, Atelier, Cabine et Master va encore changer (briques Dsp, niveau statique de Looks) | `grep stateVersion` | Écrire `stateVersion = 2` **dès maintenant** dans les 23 autres, avant la 2e livraison. Sinon, impossible de reconnaître plus tard un « projet d'avant » |
| R2 | **Pads/Sample : un objet libéré peut encore être lu.** Les voix gardent un `const Bank*` brut (`PadEngine.h:114`). L'historique ne garde les anciennes banques que 30 s, et le ménage se fait au `publish` suivant. Scénario : une tranche longue en mode « gate » tenue plus de 30 s pendant qu'on charge deux autres sons | `SamplerCommon.h:33` | Faire le ménage seulement si aucune voix ne référence la banque (compteur atomique de voix par banque), ou passer à 120 s et couper les voix d'une banque retirée |
| R3 | **CI : 3 tests console sur une quinzaine.** `offline`, `orbe_test` et `onek_test` tournent ; `tune_core_test`, `galet_test`, `chain_test`, `master_test`, `eq_test`, `denoiser_test`, `accroc_test`, `relance_test`, `test_orbe808`, `orbesynth_test` et `dsp2b_test` ne tournent pas | `build-macos.yml:16-18` | Ajouter une étape « tous les tests console » (ils sont sans JUCE, quelques secondes chacun) sur Mac **et** sur Windows (MSVC détecte d'autres erreurs) |
| R4 | **pluginval « latest » non figé, et lancé sans tests d'interface** | `build-*.yml:63/76` (`releases/latest`) | Figer la version (par ex. v1.0.4). Lancer les tests d'interface sous `xvfb` sur Linux, ou au moins sous Windows. Les éditeurs v3 sont neufs et les crashs d'éditeur sont les plus fréquents dans FL |
| R5 | **Signature et installation.** Signature ad hoc sur Mac (`codesign -s -`). L'installeur fait `xattr -cr` (`Installer-dreamaws.command:13`). Windows : pas de signature | Workflows et installeur | Pour l'artiste seul, ça suffit. **Avant toute diffusion hors du label** : Developer ID, notarisation (`notarytool`) et un `.pkg` ; Authenticode sous Windows (SmartScreen sinon) |
| R6 | **Une seule version produit (0.2.0) pour 26 plugins**, et aucun affichage dans l'éditeur | `CMakeLists.txt:2` | Afficher version et commit dans l'aide (« ? »). Indispensable pour trier les retours de bugs (« quel build ? ») |
| R7 | **FL Studio : blocs plus grands qu'annoncé.** Corrigé dans Autotune (`scratch` à 8192 minimum). [?] Non vérifié plugin par plugin ailleurs. Les un-potard découpent par `maxN` (`OneKnob.h:127`) : bien | `AutotuneProcessor.cpp:103` | Test console commun : `prepare(512)` puis des blocs de 4096, 1 et 0 échantillon, pour chaque moteur |
| R8 | **Bypass hétérogène.** Booléen dans la plupart des plugins, « choix » à 2 états dans Accroc et Relance, et base commune `OrbeFace.h` pour d'autres | `grep getBypassParameter` | Vérifier dans FL, pour les trois variantes, que le bouton bypass de la console pilote bien le paramètre (checklist FL n° 9) |
| R9 | **CPU de l'interface** : 21 éditeurs rafraîchis à 30 Hz, ombres `DropShadow` mises en cache (`ArtCache`) | `Widgets3.h:26,58` | [?] À mesurer avec 20 instances ouvertes dans FL. Éditeur caché : `stopTimer` sur `visibilityChanged` |
| R10 | **OrbeChain modifié par plusieurs agents en même temps** (correctif des claques pendant cet audit) | Diff de `scratchpad/claque/` | Un seul propriétaire par fichier moteur et par lot. Rejouer `chain_test` après chaque lot |

## 3. Tests qui manquent (les plus rentables)

1. **Niveau en fonction du preset** : pour chaque preset de Looks, Atelier, Cabine, Master et des un-potard, mesurer sur la voix de test l'écart RMS sortie/entrée, la crête et le rapport attaque/corps (le banc `looks.cpp` de cet audit fait 80 % du travail). Limite proposée : |ΔRMS| < 1,5 dB, attaque/corps sans dépasser l'entrée de plus de 1 dB.
2. **Repliement** : sinus de 4,5 kHz et 9 kHz à −6 dBFS, saturation au maximum, sur chaque plugin qui sature (Cabine, Master, Looks, 808, Beat, Synth, Galet). Limite : non-harmonique < −70 dBc.
3. **Automation** : balayage de chaque paramètre en 100 ms, pour vérifier l'absence de clics (énergie au-dessus de 10 kHz sur une sinusoïde de 200 Hz).
4. **Rechargement d'état** : `getState` puis `setState` dans une instance neuve, et sortie identique au bit près (déjà fait pour certains).
5. **Fréquences d'échantillonnage** : 44,1, 48, 88,2 et 96 kHz, avec un son « identique » à 0,5 dB près (G7 de l'audit 2, à généraliser).
