# Audit 3, n° 2 : réglabilité (« difficilement réglable »)

*Source : les tableaux de paramètres de chaque `*Processor.cpp`, `AtelierParams.h` et `Kit/v3/Widgets3.h`. Rien n'a été testé à la souris dans un hôte.*

## 0. Ce qui est déjà bien

- Les potards v3 (`Widgets3.h:459-583`) ont Maj pour le réglage fin (×6), le double-clic pour revenir au défaut, la saisie au clavier et la molette. C'est le niveau attendu.
- Cabine, Atelier, Master, Autotune, DeEsser (attaque, relâche), Galet et Accroc utilisent `setSkewForCentre` sur les fréquences et les temps.
- Le moteur « Actuel » des un-potard garde un niveau constant (mesuré : −0,1 à +0,5 dB d'un bout à l'autre de la course).

## 1. Ce qui manque à toute la suite (par valeur pour l'artiste)

| Manque | Constat | Proposition |
|---|---|---|
| **Presets utilisateur** | Aucun plugin d'effet ne sait enregistrer un réglage (les seuls accès fichiers sont dans Orbe, Fredonne et Beat) | Bouton « Enregistrer » dans la barre de presets du kit v3 : fichier XML de l'état APVTS dans `~/Documents/dreamaws/<Plugin>/`, liste « Usine / Les miens ». Une seule implémentation dans le kit pour 26 plugins |
| **Annuler / rétablir** | Aucun `UndoManager` (seule l'édition de notes d'Autotune a un annuler) | Passer `&undo` au constructeur `AudioProcessorValueTreeState`, avec Ctrl/Cmd+Z dans le kit. Une heure par plugin |
| **A/B** | Seulement dans Orbe EQ (`EqProcessor.cpp:88`) | Généraliser A/B avec copie A→B dans le kit v3 (deux `ValueTree`) |
| **Comparaison à niveau égal** | Les un-potard la font (constant). Looks utilise un AGC dynamique. Les autres plugins n'ont rien | Bouton « Niveau égal » (gain de sortie statique calculé à l'ouverture du preset et à chaque relâchement de potard, à partir du RMS entrée/sortie sur 3 s). C'est ce qui fait juger un réglage « meilleur » et pas « plus fort » |
| **Valeurs affichées** | Atelier montre Drive, Biais et Crush en « x10 » (`AtelierParams.h:36-38`). Cabine affiche la saturation de 0 à 10 sans unité | Saturation en dB de drive ou en « % de couleur » ; jamais de « x10 » |
| **Pas de défaut musical** | Atelier : tout module est éteint et le seuil du compresseur vaut 0 dB par défaut (`AtelierParams.h:29`) : on allume le compresseur et il ne fait rien | Défauts « voix rap » quand on allume un module : seuil −18 dB, ratio 3, attaque 10, relâche 120, de-ess −6 dB relatif |
| **Assistants** | Aucun réglage automatique de seuil ni d'entrée | « Écouter 5 s » : place le seuil du compresseur pour 4 à 6 dB de réduction, le de-ess pour 3 dB et le gain d'entrée à −18 dBFS RMS. La brique `vd.gDb` (gain d'entrée automatique des un-potard) existe déjà |

## 2. Plugin par plugin : plages et courbes

| Plugin | Paramètre (fichier:ligne) | Problème | Proposition |
|---|---|---|---|
| Atelier | `ratio` 1–20, linéaire (`AtelierParams.h:30`) | La moitié de la course tombe entre 10 et 20:1. Un ratio voix se règle entre 2 et 6 | Centre à 4 (`setSkewForCentre(4)`), ou plage 1–10 plus une position « ∞ » |
| Atelier | `b1Q`, `b2Q` 0,3–6, linéaires | Les Q utiles (0,5–2) ne prennent qu'un quart de la course | Centre à 1 |
| Atelier | `mk` −6 à +24 dB | +24 dB de gain de compensation est une plage dangereuse ; et pas de gain automatique | −6 à +12, plus un bouton « auto » (`staticGrDb(-12)` existe déjà dans `Compressors.h:118`) |
| Atelier | `dsThr` −50 à −10 dBFS, absolu | Dépend du niveau d'entrée | Relatif (voir `DeEssDyn.h`) |
| Atelier | `dbA`/`dbB` −12 à +12 dt au pas de 0,01 | Doubler, c'est 3 à 15 cents. Ces 30 cents utiles occupent 1,25 % de la course | Deux paramètres : « Désaccord » en cents (0–30) et « Intervalle » (choix : unisson, octave, quinte, tierce) |
| Cabine | `ratio` 1–10, linéaire ; `sat` 0–10 sans unité ni centre | Même défaut ; la saturation se joue sur les 3 premiers dixièmes de la course | Ratio centré à 3 ; sat centrée à 3 |
| Cabine | Reverb et delay : seulement « envoi % » | Aucune action sur l'espace lui-même (voir 01) | Sélecteurs « Espace » et « Division » |
| Master | `cRatio` 1–6, `cAtt` 1–80, relâche **non exposée** (fixée à 0,2 s) | Pas de relâche auto ni réglable sur un compresseur de bus | Relâche Auto/100/300/600 ms |
| Master | `lGain` −3 à +18 dB, sans mesure intégrée | Pas d'objectif de loudness | Afficher LUFS court terme et intégré, PLR ; objectif « streaming −14 / club −9 » qui positionne le gain |
| DeEsser | `freq` 4–10 kHz, linéaire | Ça passe sur 1,3 octave, mais un pro attend une course logarithmique | Centre à 6,5 kHz |
| DeEsser | `thr` −60 à 0 dBFS | Absolu | Relatif |
| 808 | `len` 0,2–4 s, `pt` 10–200 ms, linéaires (`Orbe808Processor.cpp:21`, sans skew) | Les valeurs trap (0,4–1,2 s ; 20–60 ms) n'occupent qu'un quart de la course | Centres à 0,9 s et 40 ms ; Longueur synchronisable au tempo (1/4, 1/2, 1 temps) |
| 808 | `cut` 0..1 affiché en Hz (120·100^v) | Correct, mais la valeur stockée n'est pas en Hz : l'automation est illisible | Paramètre directement en Hz, avec skew |
| Accroc, Relance | `seed` 1–999 en **flottant** sur un potard | Une graine n'est pas une grandeur continue : tourner le potard fait défiler 999 variantes sans ordre | Bouton dé (« relancer ») plus un champ numérique ; paramètre entier |
| Relance | `len` 200–600 ms, `thresh` −60 à −20 dB : bien bornés | « Audace » et « Densité » sont deux % abstraits | Exemples audibles dans l'aide, et un « Densité » exprimé en « 1 réponse toutes les N phrases » |
| Galet | `hz` 0,02–40 Hz centré à 1 : bien | 23 potards de même poids visuel | Voir design : panneau « avancé » repliable |
| Autotune | `speed` 0–400 ms centré à 60 : bien. `form` −6 à +6 dt au pas de 0,5 | Le pas de 0,5 dt est trop grossier pour un réglage de formant fin | Pas de 0,05 |
| Voile | Macros 0–1 affichées 0–10 | Très bien pour la cible. Il manque une « Sortie auto » | Bouton niveau égal |
| Looks | 4 macros + Mix + Sortie | Bien comme principe. Il manque une vue « ce que fait la macro » | Sous chaque macro, une ligne qui change : « comp 4:1 · air +5 dB » (les valeurs viennent de `S`) |
| Pads | `Attaque` 0–500 ms centré à 0,4, `Relâche` 0–2 000 ms centré à 0,35 | Correct | — |
| Sample | `rate` et `cut` de 0 à 1 | Les unités réelles ne sont pas exposées à l'automation | Hz et kHz avec skew |

## 3. Macros à ajouter (avec le moteur actuel)

- **Atelier** : 4 macros au-dessus des modules (« Proche », « Brillance », « Colle », « Espace »), chacune pilotant plusieurs paramètres réels. Le moteur de Looks le fait déjà (`Look::map`). On peut afficher les valeurs dans les modules sans les verrouiller.
- **Cabine** : une macro « Devant / Derrière » (présence, compression, envois) : c'est la décision n° 1 d'un mixeur rap.
- **Master** : une macro « Énergie » (compresseur, saturation et gain du limiteur ensemble) avec la courbe GR/loudness en retour.
- **808 / Beat** : la molette de modulation MIDI (CC1) mappée par défaut sur une macro de timbre (drive et filtre pour la 808). Synth gère déjà CC1, CC64 et le pitch bend (`OrbeSynthProcessor.cpp:82-84`) ; 808 ne gère que le pitch bend ; Beat ne gère aucun contrôleur.
