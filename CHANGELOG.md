# Journal des changements

Version courante : branche `vague2-wip` (10 novembre 2026), par rapport à la première livraison du 8 novembre.
Statut : travail en cours, non validé. Les mesures viennent des tests console (sans JUCE) et de pluginval niveau 10 (sans tests d'interface). Rien de ce qui change au son n'a encore été écouté dans FL Studio ; la liste « à écouter » est en bas.
CI du 10 novembre (branche `main`, qui contient la vague 2) : Windows, 25 VST3 sur 25 réussis à pluginval ; macOS, 25 VST3 sur 25 réussis, mais 0 AU sur 25. Les échecs AU venaient de la CI, pas des plugins : voir « CI et paquets » plus bas.

## Orbe scindé, Orbe Rythme, Orbe FX réel (vague 2, 11 novembre)

- **Orbe** ne joue plus la batterie (accords, lead, basse) ; identifiants et états inchangés.
- **Orbe Rythme** (nouveau) : la batterie, avec migration des anciens états d'Orbe (voir `docs/orbe-rythme-migration.md`).
- **Orbe FX** devient un vrai effet audio (filtre, saturation, chorus, delay, réverbe, 4 macros, 12 préréglages). Un ancien état d'Orbe FX est ignoré.
- CI : 27 plugins vérifiés. Non écouté dans FL Studio ; AU non validé.

## Lot E : studio et instruments (11 novembre)

- **Synth** : repliement de −44 à −85 dB, unisson à phases aléatoires, filtre 2, 2 LFO, glide, mode mono/legato. **808** : relance sans trou (fondu de 2,5 ms), mode « Pliée » moins repliée. **Sample** : étirement qui garde les attaques. Ces trois-là, et **Cabine**, écrivent la version d'état 3 : un projet d'avant garde son ancien son.
- **Beat** et **Pads** : sorties auxiliaires par piste ou par pad (désactivées par défaut). **Cabine** : de-esser, gain d'entrée, presets tous distincts. **Galet** : une forme par module. **Accroc** : traîne d'écho. **Master** : cible LUFS, écoute à niveau égal, Delta. **EQ** : pentes 12/24/48 dB/oct.
- Identifiants existants inchangés, nouveaux en fin, neutres par défaut. Détail et mesures : `docs/audit4/lotE-etat.md`. Rien n'a été écouté dans FL Studio.

## Attention : changements audibles dans les projets existants

La vague 2A change le son de **Voile, Looks, Atelier, Cabine, Synth, Pads et Sample**. Un projet enregistré avant cette version sonnera différemment à la réouverture : ces 7 plugins n'ont pas de mode « ancien son ».
Les 4 plugins à un potard (Grain Nord, Gorge, Poupée, Patine) changent aussi de son, mais seulement pour les **nouvelles instances**. Un projet enregistré avant cette version se rouvre avec le moteur « Classique », une copie figée de l'ancien moteur (voir plus bas). Numéros de version d'état écrits : un-potard (2) et Accroc (1).

Ce qui change :

- **Nouvelle reverb** (réseau de retards à 8 lignes). Elle remplace l'ancienne dans Voile, les un-potard, Looks et Atelier ; Cabine, Synth, Pads et Sample utilisent la même reverb partagée. La queue, le grain et la tonalité de la reverb sont différents. La décroissance suit désormais le réglage au lieu de retomber vers 1,2 s après un changement de tampon ou de fréquence de l'hôte.
- **Nouveau décaleur de hauteur** (partagé) dans Voile et dans le moteur « Actuel » des un-potard. Latence et timbre peuvent différer légèrement.
- **Doubles** : les doubles de type « seconde prise » (ADT) sont dans le moteur de Voile (`Engine.h`). Looks et Atelier gardent leurs anciens doubles : `OrbeChain.h` utilise toujours `voile::Shifter` pour les doubles.
- **Saturation compensée en niveau** : la saturation de Cabine et de Master ne monte plus le volume avec le réglage (l'ancienne forme ajoutait jusqu'à +20 dB à saturation 10). Le potard change la couleur, pas le niveau. Les anciens réglages de saturation sonneront plus bas qu'avant.
- **Coupe-continu de Voile** : son coefficient dépend maintenant de la fréquence d'échantillonnage. À 48 kHz, la réponse est la même qu'avant (écart inférieur à 0,1 dB à 30 Hz) ; à 44,1, 88,2 et 96 kHz, le son devient identique à celui de 48 kHz.

Ce que l'utilisateur doit savoir :

1. **Voile : le bypass change d'identifiant.** Il passe par un paramètre « bypass » du plugin (ajouté en fin de liste). Une automation de bypass enregistrée avant cette version est perdue : il faut la refaire.
2. **Orbe Autotune ouvre les anciens projets en moteur Classique** (pas de paramètre `algo` dans l'ancien état). Le nouveau moteur « Orbe » est à choisir à la main.
3. Aucun identifiant de paramètre n'a été supprimé ou modifié depuis le 8 novembre. Les ajouts sont en fin de liste : `bypass` (Voile), `algo`, `expr`, `accid` (Autotune), `opt` et `engine` (un-potard). Aucun nouveau booléen.
4. Un-potard : un état sans numéro de version (projet d'avant cette version) se rouvre avec le moteur « Classique » (`engine` = Classique), avec l'ancien son et l'ancienne latence. Les nouvelles instances démarrent sur le moteur « Actuel ».

## Par plugin

### Voile
- Reverb et décaleur nouveaux (voir plus haut). Doubles ADT intégrés au moteur.
- Bypass : paramètre dédié, fondu de 10 ms, moteur qui continue de tourner pendant le bypass. Tests : bypass à −400 dB, latence inchangée.
- NaN ou infini en entrée remplacés par 0 (le plugin ne reste plus muet).

### Plugins à un potard (Grain Nord, Gorge, Poupée, Patine)
- Deux moteurs, choisis par le paramètre `engine` :
  - **Actuel** (nouvelles instances) : un modèle par plugin (`OneKnobModels.h`), dynamique avant l'espace, latence propre au modèle (0 pour Gorge et Patine). Il utilise les briques `Dsp/Compressors.h`, `Saturation.h`, `DeEssDyn.h` et le décaleur partagé. Poupée utilise le moteur d'Autotune (`TuneCore`, mode Direct).
  - **Classique** (anciens projets) : `ClassicEngine.h` et `ClassicChain.h`, copie figée du moteur de la vague 1 (commit `437bc8e`). onek_test [13] vérifie l'identité avec l'ancienne chaîne à l'échantillon près et la latence d'origine. Seul écart voulu : le slapback fantôme (G6) est corrigé.
- Nouveau paramètre `opt` (option propre au modèle). Version d'état 2.
- NaN ou infini en entrée remplacés par 0 (`OneKnobProcessor.cpp`, `OneKnob.h`).
- CPU de Poupée : l'autocorrélation LPC de `TuneCore` est calculée par blocs de 8 retards. La sortie est identique au bit près : même empreinte de sortie avant et après, sur Poupée comme sur TuneCore seul. Le nombre d'instructions de Poupée baisse de 20 % et celui de TuneCore seul de 23 % (callgrind) ; le temps de TuneCore baisse d'environ 30 %. Le gain s'applique aussi à Autotune en moteur Orbe.

### Orbe Autotune
- Note forcée par MIDI : pas branchée (le plugin n'a pas d'entrée MIDI ; l'ajouter changerait le type AU et les bus des projets existants). Détails dans `docs/tune-algo.md` § 9.
- Nouveau moteur « Orbe » (`docs/tune-algo.md`) : détection YIN, Viterbi en ligne, courbe de correction, resynthèse à raccords synchrones. Les mesures sont dans `tests/tune_core_test.cpp` ; les 8 points d'écoute de `tune-algo.md` § 10 n'ont jamais été écoutés.
- Édition note par note (type Melodyne), avec des limites : pas de déplacement dans le temps, pas de sélection multiple, pas d'export MIDI. Elle est liée à la position sur la timeline.
- Bémols en plus des dièses (paramètre `accid`). Nouveaux paramètres `algo` et `expr`.
- Correction d'un « fantôme » de sortie (−321 dB dans les 8 combinaisons testées).
- NaN nettoyé en entrée et en sortie.

### Orbe Looks
- Les 10 looks d'origine gardent leur index. **17 looks** sont ajoutés à la suite (index 11 à 27) : 8 « Rap » et 9 « Pop électro ». La liste et les descriptions sont dans `docs/looks-voix.md` § 2 bis. Leurs valeurs sont des hypothèses à juger à l'oreille.
- Catégories affichées dans la liste.
- Nouvelle reverb (voir plus haut).

### Orbe Atelier
- Nouvelle reverb (voir plus haut). Garde d'entrée NaN/infini.

### Orbe Cabine
- Saturation compensée en niveau (voir plus haut). Reverb partagée nouvelle. Garde d'entrée NaN/infini.

### Orbe Master
- Saturation compensée en niveau. Garde d'entrée NaN/infini.
- Lot A : limiteur avec **maintien de 30 ms** avant la relâche (DHT à 40 Hz, 8 dB de réduction : 1,16 % avant, 0,000 % après) ; **pic vrai** ×4 polyphasé de 48 coefficients (`Dsp/TruePeak.h`), marge de 0,15 dB : plafond −1 dBTP tenu (avant : −0,68). Latence totale 183 échantillons à 48 kHz (171 avant), toujours déclarée à l'hôte. Le son du limiteur change légèrement sur les graves : à écouter.
- LUFS **court terme** (3 s) et **intégré** (portes −70 LUFS et −10 LU) de sortie dans l'écran ; double-clic sur les LUFS : remise à zéro de l'intégré.

### Orbe Synth, Orbe Pads, Orbe Sample
- Reverb partagée nouvelle. Pads et Sample lisent aussi le MP3 sous Windows (M4A et CAF restent réservés au Mac).
- Lot A, **crash corrigé (A10 de l'audit 3)** : Pads et Sample lisaient un pointeur brut vers une banque libérée 30 s après son remplacement, alors qu'une voix peut durer 240 s. Les banques vivent maintenant tant qu'une voix les lit (`OrbeSampler/Swap.h`, par générations). Test ASan, UBSan et TSan dans `sampler_test`.

### Orbe Mix
- Lot A : **pic vrai** ×4 polyphasé (même code que Master). Sinus 12 kHz à −3,10 dBTP : −3,76 affiché avant, −3,10 après ; écart maximal à la référence ×16 : 0,07 dB.

### Version d'état (lot A)
- Propriété `stateVersion` = 2 écrite par Orbe et Orbe FX, Master, Mix, Cabine, Detect, EQ, Galet, Synth, 808, Beat, Pads et Sample (aucun identifiant de paramètre touché, anciens états relus à l'identique). Patron : `docs/audit4/lotA-etat.md`.

### Orbe DeEsser
- Un NaN ou un infini en entrée ne rend plus la sortie NaN pour toujours (G2 de l'audit 2, encore ouvert pour ce plugin). Test ajouté.

### Orbe Denoiser
- Garde de tonalité à deux côtés (audit 3) : la garde à fenêtre unique sous-estimait le bruit grave en pente (bruit de pièce, grondement). Bruit de pièce, réduction mesurée de 80 Hz à 16 kHz : 6,1 dB avant, 16,9 dB après (18 dB demandés). Note tenue toujours préservée (−0,01 dB à 7 s). Tests de non-régression ajoutés (note tenue, bruit grave).
- Un NaN ou un infini en entrée ne rend plus la sortie NaN pour toujours (avant : 100 % d'échantillons NaN après un seul glitch). L'apprentissage du profil moyenne les deux voies (avant : voie gauche seule). Compteur de trames sans débordement après 12 h (M5).
- Garde de tonalité : une note tenue (partiel stable, pad, 808) n'est plus avalée comme du bruit. Mesure : −0,06 dB à 7 s sur une note tenue. Il n'y a pas encore de test de non-régression pour ce point.

### Orbe 808
- Saturation placée après l'enveloppe : le taux d'harmoniques suit le niveau (« bloom » : attaque plus grasse, queue qui redevient sinus). Au maximum de l'enveloppe, le son est identique à l'ancien moteur ; le niveau reste compensé. À valider à l'oreille.

### Orbe (générateur) et Orbe FX
- Le gain master ne touche plus que l'aperçu du synthé (il baissait le volume de l'Orbe FX).
- Notes MIDI : le note-off en attente est vidé avant le note-on (notes qui étaient avalées).
- Rendus gardés 2 secondes pour éviter un accès à de la mémoire libérée.

### Orbe Fredonne
- La prise n'est plus effacée par un changement de fréquence ou de tampon de l'hôte ; elle est analysée à sa propre fréquence.

### Orbe Galet
- Garde d'entrée : NaN et infini sont remplacés par 0 avant le moteur. Test : la sortie redevient finie, avec une énergie non nulle ensuite.
- Le préréglage d'usine « Pompe sidechain » est renommé **« Pompe rythmique »** (même position, mêmes réglages). Galet n'a pas d'entrée sidechain.

### Tous les plugins concernés par le bypass
- Bypass honoré avec fondu de 10 ms (14 plugins, depuis le 8 novembre).

## Dièses / bémols (♯ / ♭)

- Nouveau réglage « Altérations » (# / ♭) pour les plugins qui affichent des notes, tonalités, gammes ou accords. Les enharmoniques ne changent que les noms, jamais le son. Défaut : dièses (comportement d'avant). Aide commune : `Source/Kit/v3/NoteNames.h`.
- Paramètre `accid`, ajouté en fin de liste, choix à 2 valeurs (pas un booléen), défaut dièses : Voile, Grain Nord et Poupée (seulement s'ils ont une hauteur), Orbe et Orbe FX, Looks, Atelier, Beat, Fredonne. Aucun identifiant ni état existant ne change.
- Sans tonalité propre (affichage seulement) : Detect, 808, Pads, Sample. Réglage d'interface mémorisé par plugin dans les préférences de l'utilisateur (`Prefs`, kit v3), pas dans le projet.
- Non touchés : Gorge, Patine (pas de notes), Relance, Synth, Accroc, Mix, Master, Cabine, EQ, Galet, De-esser, Denoiser. `TuneCore.h` et les moteurs DSP sont intacts.
- Limites : le texte d'automation hôte du paramètre `key` (Looks, Atelier) reste en dièses ; Orbe garde sa règle d'armure pour les noms d'accords tant que « dièses » est choisi.

## Nouveaux plugins

- **Orbe Galet** : courbes et distorsion lisse. 16 préréglages, distorsion sur-échantillonnée ×4 (ADAA), latence 71 échantillons, forme sauvegardée dans l'état. pluginval niveau 10 : réussi.
- **Orbe Accroc** (en cours, non évalué par le validateur) : séquenceur d'accrocs (bégaiement, stutter, rampe, tape-stop). Les compresseurs lampe, opto et FET annoncés ne sont pas utilisés par Accroc pour l'instant (`AccrocDsp.h` n'inclut que la bibliothèque standard). Les briques `Source/Dsp/Compressors.h`, `Saturation.h` et `DeEssDyn.h` servent aujourd'hui au moteur « Actuel » des un-potard.

## CI et paquets

- **AU sur macOS** : les 25 `.component` échouaient à pluginval (« 1 test failed, out of a total of 1 ») parce qu'ils étaient validés depuis `dist/`. macOS ne trouve un AU que s'il est installé dans un dossier `Components`, donc le scan échouait avant tout test. Les bundles eux-mêmes sont corrects : `Info.plist` contient bien `AudioComponents`, avec les codes `aufx`/`Drmw`/`Voil` pour Voile, et les codes plugin sont uniques. La CI installe maintenant les AU dans `~/Library/Audio/Plug-Ins/Components`, relance `AudioComponentRegistrar`, liste les AU avec `auval -a`, puis les valide. Le journal complet de chaque validation est joint (`pluginval-logs-macOS.zip`). À confirmer au prochain lancement de la CI macOS.
- **Garde-fou** : sur Windows et sur macOS, le job échoue si l'un des 25 plugins attendus manque dans les paquets (liste explicite ; sur Mac, VST3 et AU). `-k 0` ne peut plus cacher un plugin absent.

## À écouter dans FL Studio (non vérifié)

Les 8 points de `docs/tune-algo.md` § 10 ; le son des anciens projets Voile, Looks et un-potard avant et après la vague 2 ; la nouvelle reverb sur les presets de Cabine, Synth, Pads et Sample ; Galet (« Pompe », distorsion à fort drive) ; le bloom de la 808 ; la liste de contrôle FL n° 8 (compensation de latence Autotune), n° 9 (bypass) et n° 12 (rechargement de projet).
La vague 2 a été compilée sous MSVC par la CI du 10 novembre (25 VST3, pluginval niveau 10 réussi, sans tests d'interface). Les AU n'ont pas encore été validés (voir « CI et paquets »).

## Lot D : famille voix (audit 4), branche `vague2-wip`

Rien de ce qui change au son n'a été écouté dans FL Studio. Mesures : tests console. Aucun identifiant de paramètre existant n'a changé ; les nouveaux sont en fin de liste.

- **Voile** : Mordant compensé en niveau (traité à -0,1 à -0,4 dB du sec, contre +3,7 à +10,9 dB avant), Halo réellement stéréo (un double par côté, graves laissés au centre ; corrélation G/D 0,985 -> 0,917), les 8 presets ne touchent plus la tonalité ni la gamme et sortent à 0 dB. Un état sans propriété `dspv` (projet d'avant) garde l'ancien son.
- **Orbe Looks** : « Garder mes macros » (paramètre `keep`, allumé par défaut : changer de look ne remet plus les 4 macros à zéro), `look` non automatisable, jauge « Niveau auto », niveau automatique v2 (montée 2 s, descente 0,8 s, borné à ±3 dB autour du gain statique du look) ; Verre Pilé recalé (+2,1 dB -> +1,3 dB). Un état sans propriété `lkv` garde le niveau automatique d'origine.
- **Orbe Atelier** : `preset` non automatisable ; « De-ess avant comp » (paramètre `dsFirst`, réglage de session hors presets).
- **Orbe Relance** : harmonie dans la gamme (quinte diatonique : Si en Do majeur reçoit un Fa, plus un Fa dièse) ; tonalité Auto (histogramme des notes chantées, quinte juste tant qu'elle n'est pas sûre) ou imposée (`hkey`, `hscale`). Un état sans `stateVersion` 2 garde la quinte juste fixe.
- **Orbe Denoiser** : mode Prise (`live`, FFT 512, latence 512 échantillons au lieu de 2048, réduction plafonnée à 12 dB), « Entre les phrases » (`gap`, 0 à 20 dB, voisement sur 100 Hz-4 kHz), réduction par bande (`blo`, `bhi`), l'adaptatif continue pendant l'apprentissage, la fréquence d'échantillonnage est enregistrée avec le profil (recalage en Hz à la relecture).
- **Orbe DeEsser** : lookahead réglable de 0 à 10 ms (`look`, 1 ms par défaut), liaison stéréo (`link`), détecteur « CH » séparé 2,5-5 kHz avec garde de voisement (`chamt`), bouton Auto-seuil (5 s).
- **Orbe Fredonne** : la vélocité du MIDI en direct est celle du fichier .MID (tirée du niveau de la note), plus 106 partout.
- **Orbe Détect** : carte des fausses notes (bouton « Carte voix » : part du temps chanté hors gamme, justesse par note en cents, ligne de temps) ; clic droit sur la tonalité : copier (« Fm »), coller une tonalité de référence. Voile, Looks, Atelier et Relance : clic droit sur le menu Tonalité pour coller.
- Tests : `orbechain_test`, `orbedenoiser_test`, `orbedeesser_test`, `orbe_relance_test`, `voile_offline_test`, `orbedetect_test` (nouveau).
