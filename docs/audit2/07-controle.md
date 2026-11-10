# Audit 7 : contrôle indépendant (chef de projet / QA audio) et idées ajoutées

*Constat du **10 nov. 2026, 01 h 20 CET**, au commit **`10cbaf9`** (branche `vague2-wip`, « WIP vague 2, non valide »).
En cours d'édition au moment du constat (non commité) : `.github/workflows/build-{macos,windows}.yml` et `Source/Kit/Widgets.h`
(ajout d'un futur plugin **« Orbe Accroc »**, série DW-706), `docs/audit2/06-visuel-moderne.md` et `docs/audit2/visuel/` (nouveaux).
Un agent compilait les VST3 de Voile, des 4 un-potard, Looks, Atelier et Autotune dans `/tmp/b_2a`.*

*Méthode : lecture du code (je n'ai pas fait confiance aux rapports), puis exécution de 14 tests console sans JUCE, compilés
avec g++ hors du dépôt. Les 14 passent : dsp2b, tune_core, onek (avec `prise.wav`), offline (Voile), galet, autotune,
denoiser, cabine, master, chain, eq, deesser, 808, kit_vu. J'ai aussi écrit trois bancs jetables, dans
`/tmp/claude-0/-home-claude/12ae9892-af71-5ca8-a97b-93f9fd958b0b/scratchpad/ctl/` : `b2.cpp` (note tenue au Denoiser),
`nan.cpp` (NaN en entrée) et `cpu.cpp` (coût des un-potard).
Pas de build CMake JUCE : un agent compilait déjà en `-j1`, et les moteurs se testent sans JUCE. La seule compilation
Mac et Windows est prévue à la fin.
Aucun fichier source modifié, rien de commité.*

Légende : **F✓** fait et vérifié · **F?** fait mais non testé · **PART** partiel · **NON** pas fait · **REP** reporté volontairement · **ABAN** abandonné à tort.

---

## 1. Demandes de l'artiste

| Demande | Statut | Preuve et ce qui manque |
|---|---|---|
| Autotune « à moi », meilleur que l'ancien | **F✓** (mesures), jamais écouté | `TuneCore.h` ; tune_core [2] à [9] : vibrato ×1,02 au lieu de ×0,98 avec un centre faux de +27 ct, glissando 0,6 ct au lieu de 4,9, formants 2,3 dB au lieu de 14,4. Les 8 points de `tune-algo.md` §10 n'ont **jamais été écoutés** dans FL. |
| Édition note par note façon Melodyne | **F✓**, avec des limites | `NoteEdit.h` ; tune_core [10] : note éditée 62,2 → 64,00, état de 88 + 1124 octets. Manquent : déplacement dans le temps, sélection multiple, export MIDI. L'édition est liée à la position sur la timeline. |
| Bémols en plus des dièses | **F✓** | Paramètre `accid` (`AutotuneProcessor.cpp:40`) ; test : F majeur → Bb, G majeur → F#. |
| Plus de presets pour Looks | **NON** | Toujours 10 looks (`OrbeChain/Looks.h`). |
| Orbe Galet (courbes, distorsion lisse) | **F✓**, un défaut | 16 presets, ADAA ×4, latence 71, forme sauvegardée (`GaletProcessor.cpp:122-129`), galet_test OK. **Un NaN en entrée rend le plugin NaN pour toujours** (banc `nan.cpp`). Le preset « Pompe sidechain » n'a **pas d'entrée sidechain**. |
| Compresseurs lampe, opto et FET réalistes | **PART** | Brique `Dsp/Compressors.h` testée (opto : attaque 10,3 ms, relâche en deux temps de 69 ms puis 3,4 s ; FET 20 µs ; vari-mu). **Branchée nulle part.** Le plugin « Orbe Accroc » n'existe encore que dans la CI et dans `Widgets.h`. |
| Refonte sérieuse des 4 un-potard | **NON** | `OneKnob.h` n'a pas bougé depuis `01abf18`. Gain indirect seulement : nouveau décaleur et nouvelle reverb dans `Engine.h`. Voir § 4. |
| Corriger les bugs des effets | **PART** | B1, B2, B3, G1, G3 et G4 corrigés. G2 partiel. G5 à G7 et M1 à M8 non corrigés. Voir § 3. |
| Plugin Relance (ad-libs) | **NON** | Aucun code. C'est une demande explicite de l'artiste, donc hors du report de Fil et Rémanence. |
| Paquets Mac et Windows | **PART** | La CI existe. Une seule compilation est prévue à la fin, mais la CI ne se lance que sur `main`. Voir § 6. |
| Notes affichées en lettres | **F✓** | Aucun solfège dans l'interface. En revanche, **trois conventions d'octave** coexistent (§ 3, audit 4 n° 13). |
| Réponse en français | **F✓** | Docs et interface en français. Quelques noms sans accent (« Tonalite », « Densite », « Suivre l'hote »). |
| Mue (synthé phare) | **REP** | Hors périmètre, comme décidé. |

## 2. Audit 1, § 5 (lots A à J)

| Lot | Statut | Preuve |
|---|---|---|
| A Bypass + fondu | **F✓** | `bypassfx::Fader` dans `OrbeFace.h`, `OneKnobProcessor.cpp:95` et Galet. Tests : erreur −400 dB, saut −67 dBFS. |
| B Kit v2 | **F✓** | `c307654` ; kit_vu_test OK ; polices OFL dans `assets/fonts`. |
| C Fusion des mini-kits | **PART** | `suiteui` (`Orbe808/SuiteUi.h`) et `ou` (`OrbeAnalysis/OrbeUi.h`) existent encore. L'audit 6 en fait un frein. |
| D Châssis et séries uniques | **F✓** | `Widgets.h:50-55`, 24 séries distinctes. |
| E Faces un-potard + volet | **F?** | `01abf18`. L'auto-entrée pompe toujours (T3). |
| F Annuler, presets, A/B | **REP** | Choix de l'artiste. |
| G Orbe : édition | **F?** | `6d4ffec`. Non retesté par moi. |
| H Suréchantillonnage + Autotune basse latence | **F✓** | offline [6c] : repliement −76,9 dB ; Autotune Direct à 221 échantillons. |
| I EQ lissé, Orbe sans allocation | **F✓** | eq_test OK, `OrbeProcessor.h:99`. |
| J Faces par famille, Atelier à 1400 | **F?** | `AtelierEditor.cpp:9`. La fenêtre peut dépasser de l'écran (audit 4 n° 9). |

## 3. Audits 2-01 (DSP) et 2-04 (usage dans FL)

| Point | Statut | Preuve |
|---|---|---|
| B1 Fantôme Autotune | **F✓** | tune_core [11] : −321 dB dans les 8 combinaisons. |
| B2 Note tenue avalée par le Denoiser | **F✓** | Garde de tonalité (`DenoiserDsp.h:10`). Mon banc : −0,06 dB à 7 s. **Pas de test de non-régression** dans `denoiser_test`. |
| B3 Fantôme du bypass de Voile | **F?** | Bypass en choix, ajouté en dernier ; le moteur tourne pendant le bypass (`PluginProcessor.cpp:37,65-71`). Le banc `vbyp` n'a pas été rejoué. |
| G1 Gain de saturation (Cabine, Master) | **F?** | `tanh(kx)/k` (`CabineDsp.h:80`, `MasterDsp.h:226`). Le critère « < 0,5 dB sur toute la course » n'est vérifié par aucun test. |
| G2 Un NaN rend le moteur muet ou NaN | **PART** | Corrigé dans Voile, FxProcessor, Chain, Cabine et Master. **Toujours présent dans les 4 un-potard et dans Galet** : NaN encore en sortie 3 s plus tard (banc `nan.cpp`). Aucun `kit::sanitize` dans `OneKnobProcessor.cpp` ni dans `GaletProcessor.cpp`. |
| G3 Reverb qui retombe à 1,2 s | **F✓** | `Engine.h:503-505`. |
| G4 Allocations | **F✓** | Pointeurs préchargés (`MasterProcessor.cpp:48`, Autotune). |
| G5 Crête vraie de Mix | **NON** | Toujours en Catmull-Rom (`OrbeAnalysis.cpp:265-273`). |
| G6 Slapback fantôme | **NON** | `OneKnob.h:312-313` : le tampon n'est écrit que si `slapMix > 0`. |
| G7 Son qui change avec la fréquence d'échantillonnage | **NON** | Coupe-continu `0.995f` fixe (`Engine.h:581`). |
| M1 à M8 | **NON** | M1 `OneKnob.h:338` ; M5 compteur `int` (`DenoiserDsp.h:60`) ; M7 `DetectProcessor.cpp:56` ; M8 latence mesurée 815 pour 814 déclarés (onek [1d]). |
| P0-1 Prise de Fredonne non sauvegardée | **F?** | `FredonneProcessor.cpp:184-211`. |
| P0-2 Orbe FX baisse le volume | **F?** | Le gain master ne touche plus que l'aperçu (`OrbeProcessor.cpp`, rendu du synthé). |
| P0-3 Notes MIDI avalées | **F?** | Note-off en attente vidé avant le note-on (`OrbeProcessor.cpp:330-335`). |
| P1-4 Mode Pattern de FL | **NON** | |
| P1-5 Use-after-free dans Orbe | **F✓** | Rendus gardés 2 s (`OrbeProcessor.h:93-96`, `.cpp:255-259`). |
| P1-6 Paramètres structurels automatisables | **NON** | Aucun `withAutomatable(false)` dans tout le code. |
| P1-7 Export non déterministe de Sample | **NON** | Aucun `isNonRealtime()`. |
| P1-8 MP3 sous Windows | **F✓** | Pads et Sample `plugin.cmake:20` ; M4A et CAF réservés au Mac (`SamplerCommon.h:14`). |
| P1-9 Fenêtres trop grandes | **NON** | Aucun `getPrimaryDisplay` dans le code. |
| P1-10 Fredonne en version FX | **NON** | |
| P2-11 Version d'état | **NON** | Pas de `stateVersion`. Les anciens projets un-potard se rouvrent avec l'auto-entrée active. |
| P2-12 Notes bloquées au `prepare` | **NON** | `OrbeProcessor.cpp:74` : `nPend = 0` sans note-off. |
| P2-13 Conventions d'octave | **NON** | `n/12-1` dans Orbe, le Kit et Synth ; `n/12-2` dans Sampler. |
| P2-14 Mode direct partout | **PART** | Seul Autotune l'a. |
| P2-15 CI Windows | **PART** | Galet ajouté à la liste. Mais : pas de runtime statique, `-k 0`, pluginval avec `continue-on-error` et `--skip-gui-tests`. |
| P2-16 Presets visibles dans FL | **PART** | Cabine, Atelier, EQ et Galet : oui. Sample (9) et Beat (7 kits) : non. |
| P2-17 Beat déclenché par notes | **NON** | |
| P3 (réglage fin, menu de l'hôte, installeur, accessibilité) | **NON** | `Installer-dreamaws.bat:5` : toujours `'%~f0'` (casse si le chemin contient une apostrophe). |

## 4. Audits 2-02 (son), 2-03 (un potard), 2-05 et 2-06

| Point | Statut | Preuve |
|---|---|---|
| 02-1 Splicer partout + Detector d'Autotune | **PART** (WIP) | `Engine.h:246-252` et `OrbeChain.h:69` utilisent `dsp::VoiceShifter`. Mais `voile::Yin` décime toujours sans passe-bas : le Detector n'est pas repris. |
| 02-2 Reverb 8 lignes | **F✓** (WIP) | `Engine.h:340-346`. Elle est partagée : elle change aussi le son de **Cabine, Synth, Pads et Sample**. |
| 02-3 Un-potard : ordre de la chaîne, plafond de −8 dB | **NON** | `OneKnob.h:307-310` : compresseur après l'espace, `0.6f`. |
| 02-4 PSOLA avec formants | **PART** | Autotune Orbe (LPC et raccords) : oui. Ailleurs : formants sur `tune` seulement. |
| 02-5 Compresseurs FET et opto | **PART** | Brique testée mais non branchée. |
| 02-6 Doubles ADT | **PART** | `dsp::AdtVoice` dans Voile et les un-potard (`Engine.h:768`). Pas dans Looks ni Atelier (`OrbeChain` garde `Shifter`). |
| 02-7 Bloom de la 808 | **F?** | `ee4cef5`, saturation après l'enveloppe ; 808_test OK. Pas écouté. |
| 02-8 De-esser relatif, air dynamique | **PART** | `Dsp/DeEssDyn.h` testé, branché nulle part. |
| 02-9 Orbe : rolls, ♭II, micro-timing | **NON** | |
| 02-10 Emphase, biais, clipper du Master | **PART** | `Dsp/Saturation.h` testé (emphase : −7 dB à 6 kHz à fort niveau), branché nulle part. Pas de clipper. |
| 03 Phase 0 à 3 (T1 à T11) | **NON** | onek : à k = 0, centroïde 1171 Hz contre 937 Hz à l'original (toujours coloré) ; latence de 814 échantillons même dans Gorge et Patine (T5) ; tests de la phase 3 absents. |
| 05 Fil, Rémanence, autres | **REP** | Relance : voir § 1. |
| 06 Visuel (K1 à K10) | **NON** (audit déposé ce soir) | |

**Briques livrées mais non branchées** : `Dsp/Compressors.h`, `Dsp/Saturation.h`, `Dsp/DeEssDyn.h`, `mas::TruePeak` (non réutilisé par Mix), `atn::Params::forcedNote` (`tune-algo.md` §9).

## 5. Régressions possibles

- **IDs de paramètres** : OK. `bypass` de Voile, `algo`, `expr` et `accid` sont ajoutés en fin de liste. Un ancien projet Autotune se rouvre en moteur « Classique ».
- **Booléens APVTS** : aucun nouveau. Les nouveaux paramètres sont des choix (Galet `onOff()`, bypass de Voile). Les anciens booléens restent.
- **Latence** : Voile 814 = 814 ; Autotune 221 / 1729 exact ; Galet 71 ; un-potard **815 mesurés pour 814 déclarés** (M8).
- **Accents** : passent par `kit::u8`, et `/utf-8` est actif sous MSVC (`CMakeLists.txt:9`).
- **Son des anciens projets** : la vague 2 change le décaleur, la reverb et les doubles de **9 plugins**, sans version d'état. Les projets existants sonneront autrement. Autotune, lui, garde son ancien moteur pour les anciens projets. Il faut une décision explicite : accepter ce changement (et le dire dans les notes de version) ou garder un mode « v1 ».
- **CPU** : un-potard à 48 kHz, tampon de 256, un cœur du conteneur : **7,4 à 10,6 %** par instance. L'audit 3 mesurait environ 4,5 %. La machine était chargée par un autre build : **à remesurer sur le Mac**. Le scénario « 8 Gorge » de la checklist FL devient risqué.
- **Presets des instruments** : la nouvelle reverb change le son des presets de Synth, Pads, Sample et Cabine sans réécoute.

## 6. Risques Windows (MSVC)

1. **Redistribuable VC++ absent** sur un PC neuf : pas de `CMAKE_MSVC_RUNTIME_LIBRARY MultiThreaded`.
2. `-k 0` et `|| true` dans la CI : un plugin qui ne compile pas **disparaît du zip sans erreur**. « Orbe Accroc » est déjà listé alors qu'il n'existe pas.
3. pluginval avec `continue-on-error` et `--skip-gui-tests` : l'ouverture des fenêtres (Direct2D de JUCE 8) n'est jamais testée.
4. La CI ne se lance que sur `main` : la branche `vague2-wip` **n'a jamais été compilée sous MSVC**. `Dsp/*.h` et `TuneCore.h` sont nouveaux.
5. Écrans à 150 % : Atelier (1400 px) et Cabine (760 px de haut) dépassent.
6. Installeur `.bat` : chemin avec apostrophe ; pas de désinstalleur.
7. Mac en miroir : signature ad hoc seulement (`codesign -s -`), pas de notarisation. Gatekeeper bloque, il faut documenter clic droit > Ouvrir.

## 7. Ce qu'un professionnel ajouterait avant livraison

- **Versions** : tout est en `0.2.0` (`CMakeLists.txt:2`). Il faut un numéro visible dans chaque plugin (panneau « À propos »), un `CHANGELOG`, et le hash du build.
- **Licences** : textes OFL **dans les zips** (les polices sont embarquées : l'OFL demande de joindre la licence). Licence JUCE 8 : vérifier le palier (`README.md:28`). Licence du SDK VST3 et logo : à vérifier. Fichier `THIRD-PARTY.txt`. Rien de GPL n'a été copié (`recherche-algos.md`) : à confirmer par un `grep` des en-têtes avant la sortie.
- **Noms** : « Orbe », « Galet », « Accroc », « Relance » : vérifier sur KVR, à l'INPI et à l'EUIPO (audit 5).
- **Documentation minimale** : le README parle encore de « deux suites, macOS ». Il faut une page par plugin (rôle, 3 réglages clés, latence, pièges dans FL), plus une page d'installation Mac et Windows.
- **Tests « golden »** : niveau et empreinte de chaque preset d'usine sur des signaux fixes. Un changement de son devient alors une décision, pas un accident.

---

## 8. Idées ajoutées et idées complétées

Barème : intérêt **F**ort / **M**oyen / **f**aible ; effort S / M / L ; reco **V** = cette version, **A** = après.

### 8a. Idées nouvelles (vues par aucun des 6 audits)

| # | Idée | Pourquoi | Intérêt | Effort | Dépendances, risque | Reco |
|---|---|---|---|---|---|---|
| N1 | **Bouton « Juste » (A/B à sonie égale)** dans le Fader commun : maintenu, il compare traité et sec à LUFS court terme égal | Le piège n° 1 (G1) est de juger « plus fort = mieux ». Un seul code pour 25 plugins | F | S | `bypassfx::Fader`, `odsp` LUFS ; aucun risque d'état | **V** |
| N2 | **Voyant « tu enregistres avec X ms »** : lit `isRecording` de l'hôte et allume un avertissement si la latence dépasse 10 ms, avec un bouton qui bascule en Direct | Rend visible le problème de latence en prise (audit 4 n° 14) sans changer la latence pendant la prise | F | S | Playhead de JUCE ; FL renvoie-t-il `isRecording` ? À vérifier | **V** |
| N3 | **Fiche de chaîne** : clic droit > « Copier les réglages en texte » (plugin, version, latence, valeurs) | Pour envoyer une session à un ingé son, au studio ou au co-fondateur du label | M | S | Aucun | **V** |
| N4 | **Presse-papier de tonalité** (avant Fil) : Détect copie « A mineur, 92 BPM » ; Autotune, Voile, un-potard et Orbe le collent par clic droit | 80 % de la valeur de Fil pour 5 % de l'effort, sans mémoire partagée | F | S | Format texte commun ; aucun risque de bus | **V** |
| N5 | **Écoute à l'aveugle intégrée** (cachée, Alt-clic sur le logo) : Autotune tire au hasard Classique ou Orbe, l'artiste vote, le plugin affiche le score | Prouve « meilleur que l'ancien » à l'oreille de l'artiste, pas seulement par mesure | M | S | Aucun | **V** |
| N6 | **Carte de justesse** dans l'édition d'Autotune : notes colorées par l'écart moyen, et un résumé (« fins de phrase 14 ct trop bas ») | Coaching vocal à partir d'une capture qui existe déjà | M | S | `NoteEdit.h` | **V** |
| N7 | **Décalage de capture** dans l'édition d'Autotune : glisser toute l'édition de ±N temps quand la prise a été déplacée | Corrige le piège documenté (« si la prise est déplacée, refaire la capture ») | F | S | Position en ppq déjà stockée | **V** |
| N8 | **Entrée sidechain dans Galet** (déclenchement sur les attaques du kick) | Un preset s'appelle « Pompe sidechain » sans sidechain. Ajouter un bus **avant** la première sortie évite de casser des projets plus tard | F | M | Configuration de bus ; FL route le sidechain VST3 | **V** (avant toute diffusion) |
| N9 | **Galet : forme tirée d'un son** : glisser une boucle et extraire son enveloppe en 16 points | Transforme un groove existant en forme, sans dessiner | M | M | Chargement de fichier (Sampler) | A |
| N10 | **Point favori sur les un-potard** : Alt-clic marque une position, le double-clic y revient (au lieu de 0) | Un potard unique se règle une fois par voix : on y revient vite | M | S | Une propriété d'état | **V** |
| N11 | **Mode « figer l'adaptatif »** (tonalité auto, auto-entrée, auto-gain) automatique en export hors ligne, et bouton manuel | L'export doit sonner comme la lecture (audit 4, P3). C'est un défaut de fiabilité, pas une option | F | S | `isNonRealtime()` | **V** |
| N12 | **Journal local** (`Documents/dreamaws/logs`) : NaN rencontré, xrun estimé, changement de fréquence | Le support à distance sans réseau : l'artiste envoie un fichier | M | S | Écriture hors du thread audio | **V** |
| N13 | **Projet FL de démonstration** « chaîne dreamaws » : voix du label, routage, Orbe → Synth, Autotune → Looks → Galet | Prise en main en 2 minutes ; sert aussi de banc de test manuel | F | S | Build final | **V** |
| N14 | **Looks croisés** : un potard « entre deux looks » qui interpole les paramètres (EQ, comp, espace) de deux looks | Couplet vers refrain en une automation. Fondu des coefficients déjà requis par M2 | F | M | M2 (lissage) | A (après M2) |
| N15 | **Tests « golden » d'état** : états sauvegardés de v0.2 versionnés dans `tests/states`, rechargés en CI avec comparaison de niveau et d'empreinte | Protège les projets de l'artiste à chaque version | F | M | `stateVersion` (P2-11) | **V** |
| N16 | **« Réécouter la dernière phrase »** dans la chaîne voix : tampon de 10 s, touche de rejeu sans relancer FL | En prise, réécouter ce qu'on vient de dire avec l'effet | M | M | Pas de son en sortie pendant la lecture de l'hôte : seulement à l'arrêt | A |

### 8b. Idées existantes : ce qui manque pour qu'elles soient finies

**Autotune maison et édition** (intérêt F) :
- Sélection multiple, déplacement dans le temps et réglage du glissé entre deux notes (durée de la transition). Effort M, **V**.
- Export MIDI des notes : la base de Fredonne existe. S, **V**.
- Annuler persistant après réouverture. S, A.
- Bémols : vérifier aussi l'afficheur à 7 segments et le menu Tonalité en mode Auto. S, **V**.
- Cas limites à tester : boucle de FL en mode Pattern (même cause que P1-4), changement de tempo pendant la capture, 96 kHz avec plusieurs instances (LPC d'ordre 80), capture plus longue que 90 s.
- Piège : en tonalité Auto, la correction peut changer entre la lecture et l'export (voir N11).

**Looks** (F) :
- Passer à 24–30 looks rangés par usage : rap brut, mélodique, ad-lib, backs, hyperpop, R&B, drill, radio. Chacun en 3 intensités, avec une ligne d'usage. S–M, **V**.
- Réécouter les 10 looks existants après le changement de décaleur, de reverb et d'ADT : leur son a changé sans validation.
- LED d'activité réelles (audit 1). S.
- Fondu au changement de look (M2). S, **V**.

**Galet** (F) :
- Garde NaN (G2). S, **V**.
- Sidechain (N8).
- Retour visuel du drive réel, la valeur `driveDbNow` existe déjà. S.
- Aléa par mesure avec graine, et swing de la forme. S, A.
- Tester les 16 presets sur 808, voix et bus entier ; tester « Départ = Note » dans FL (routage MIDI vers un effet : à vérifier).

**Compresseurs (Accroc)** (F) :
- Détection sur la voix seule : le détecteur de voix de l'audit 3 évite de pomper le souffle.
- Sidechain HP réglable, couplage stéréo, mélange parallèle à **pleine course** (sans le plafond de −8 dB des un-potard), compensation de niveau par mesure.
- Un historique de réduction de gain sur 3 s plutôt qu'une simple aiguille.
- Presets de chaîne : « FET puis opto », « opto seul couplet ».
- Latence 0 annoncée et testée.
- Effort M. À faire **V**, puisque le plugin est déjà commencé.

**Refonte des un-potard** (F) :
- Au minimum, cette version : les phases 0 et 3 de l'audit 3 (ordre de la chaîne, plafond retiré, Gorge et Patine sans moteur de hauteur ni latence, auto-entrée figée hors voix, G6, M1, M8), plus les 6 tests de la phase 3. M, **V**.
- Les « uniques » (brume tonale, râle, courbe cible) : L, A.
- Piège : changer le son par défaut casse la promesse « même projet, même son ». Il faut `stateVersion` avant.

**Relance** (F) :
- Rejouer seulement dans les silences de la voix principale.
- Rester dans la tonalité (N4, puis Fil).
- Pouvoir **glisser le résultat en WAV** pour l'éditer.
- Bouton « relancer cette phrase » avec graine, et densité par section via la position de l'hôte.
- Latence 0, puisque la réponse arrive après la phrase.
- Effort M. Recommandation : **V** si l'artiste maintient sa demande, sinon A.

**Paquets** (F) :
- Runtime statique, échec du job si un `.vst3` attendu manque, pluginval avec tests d'interface, CI déclenchable sur la branche, OFL dans les zips, page d'installation. S, **V**.
- Notarisation Mac : compte développeur payant. A, mais documenter le contournement dès **V**.

**Bugs restants** (F) :
- G2 dans les un-potard et Galet, G5, G6, G7, M5 et P2-12 : tous de taille S, **V**.
- M2 et M3 : M, **V** pour Looks et Master.

---

## 9. Conclusion

### (a) Reste à faire pour livrer « sans y retourner », par priorité
1. G2 : `kit::sanitize` dans OneKnob et Galet. Puis G6, M1, M5, P2-12.
2. `stateVersion` dans tous les états + tests golden (N15). Décision sur le son des anciens projets touchés par la vague 2.
3. Valider la vague 2 (Splicer, reverb, ADT) par une écoute et des presets réécoutés, puis la commiter hors WIP.
4. `withAutomatable(false)` sur les paramètres structurels ; mode figé en export (N11, P1-7).
5. Refonte minimale des un-potard (phases 0 et 3 de l'audit 3).
6. Brancher les compresseurs (Accroc), le de-esser relatif et la saturation dans au moins un plugin chacun, ou les retirer de la livraison.
7. Presets de Looks (24–30).
8. Relance, si elle est maintenue.
9. CI Windows durcie, sidechain de Galet (N8), G5 et M8.
10. README, pages d'aide, licences, numéros de version, projet de démo.
11. Une compilation Mac + Windows, puis la checklist FL de 15 points et les 8 points d'écoute de `tune-algo.md` §10.

### (b) Les 5 plus gros risques
1. **Projets existants qui sonnent différemment** : vague 2 sans version d'état.
2. **Jamais écouté dans FL** : autotune, vague 2, Galet, 808. Toutes les validations sont des mesures.
3. **Build Windows de la vague 2 jamais tenté**, et une CI qui masque les échecs.
4. **Un NaN tue les un-potard et Galet** jusqu'au rechargement, et peut faire taire le bus de FL.
5. **CPU des un-potard environ doublé** (7–11 %) et beaucoup de briques ajoutées en fin de cycle : dérive du périmètre.

### (c) Checklist de sortie (go / no-go)

| # | Critère | Go si |
|---|---|---|
| 1 | Tests console | 100 % OK sur le commit livré |
| 2 | pluginval niveau 10 **avec** tests d'interface, Mac (VST3 + AU) et Windows | 0 échec, aucun `continue-on-error` |
| 3 | Zips | Nombre de `.vst3` et `.component` = nombre attendu ; OFL et THIRD-PARTY inclus |
| 4 | Latence déclarée = mesurée | Écart ≤ 0 échantillon dans tous les plugins |
| 5 | NaN en entrée | Sortie finie 100 ms après, dans tous les plugins |
| 6 | Bypass | Erreur < −120 dB, saut < −60 dBFS (tests) ; vérifié dans FL (checklist n° 9) |
| 7 | Anciens états v0.2 | Rechargés et identiques, ou différence annoncée dans le CHANGELOG |
| 8 | Checklist FL de 15 points | 15/15 sur Mac et sur Windows |
| 9 | Points d'écoute (`tune-algo` §10, presets de Looks, un-potard, Galet) | Validés par l'artiste |
| 10 | CPU | Session « 8 Gorge, 2 Synth, Master », tampon de 256, sans craquement |
| 11 | Version | Numéro visible dans chaque plugin, CHANGELOG, tag git |
| 12 | Documentation | Page d'installation et une page par plugin |
| 13 | Noms | Vérifiés sur KVR, à l'INPI et à l'EUIPO |
