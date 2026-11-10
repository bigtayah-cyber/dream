# Journal des changements

Version courante : branche `vague2-wip` (10 novembre 2026), par rapport à la première livraison du 8 novembre.
Statut : travail en cours, non validé. Les mesures viennent des tests console (sans JUCE) et de pluginval niveau 10 (sans tests d'interface). Rien de ce qui change au son n'a encore été écouté dans FL Studio ; la liste « à écouter » est en bas.

## Attention : changements audibles dans les projets existants

La vague 2A change le son de 9 plugins : **Voile, les 4 plugins à un potard (Grain Nord, Gorge, Poupée, Patine), Looks, Atelier, Cabine, Synth, Pads, Sample**. Un projet enregistré avant cette version sonnera différemment à la réouverture. Il n'existe pas de mode « ancien son » pour ces plugins : le seul numéro de version d'état du dépôt est celui d'Accroc.

Ce qui change :

- **Nouvelle reverb** (réseau de retards à 8 lignes). Elle remplace l'ancienne dans Voile, les un-potard, Looks et Atelier ; Cabine, Synth, Pads et Sample utilisent la même reverb partagée. La queue, le grain et la tonalité de la reverb sont différents. La décroissance suit désormais le réglage au lieu de retomber vers 1,2 s après un changement de tampon ou de fréquence de l'hôte.
- **Nouveau décaleur de hauteur** (partagé) dans Voile et les un-potard. Latence et timbre peuvent différer légèrement.
- **Doubles** : les doubles de type « seconde prise » (ADT) sont dans le moteur de Voile (`Engine.h`). Looks et Atelier gardent leurs anciens doubles : `OrbeChain.h` utilise toujours `voile::Shifter` pour les doubles.
- **Saturation compensée en niveau** : la saturation de Cabine et de Master ne monte plus le volume avec le réglage (l'ancienne forme ajoutait jusqu'à +20 dB à saturation 10). Le potard change la couleur, pas le niveau. Les anciens réglages de saturation sonneront plus bas qu'avant.
- **Coupe-continu de Voile** : son coefficient dépend maintenant de la fréquence d'échantillonnage. À 48 kHz, la réponse est la même qu'avant (écart inférieur à 0,1 dB à 30 Hz) ; à 44,1, 88,2 et 96 kHz, le son devient identique à celui de 48 kHz.

Ce que l'utilisateur doit savoir :

1. **Voile : le bypass change d'identifiant.** Il passe par un paramètre « bypass » du plugin (ajouté en fin de liste). Une automation de bypass enregistrée avant cette version est perdue : il faut la refaire.
2. **Orbe Autotune ouvre les anciens projets en moteur Classique** (pas de paramètre `algo` dans l'ancien état). Le nouveau moteur « Orbe » est à choisir à la main.
3. Aucun identifiant de paramètre n'a été supprimé ou modifié depuis le 8 novembre. Les ajouts sont en fin de liste. Aucun nouveau booléen.
4. Les 4 un-potard rouvrent toujours avec l'auto-entrée active (pas de version d'état).

## Par plugin

### Voile
- Reverb et décaleur nouveaux (voir plus haut). Doubles ADT intégrés au moteur.
- Bypass : paramètre dédié, fondu de 10 ms, moteur qui continue de tourner pendant le bypass. Tests : bypass à −400 dB, latence inchangée.
- NaN ou infini en entrée remplacés par 0 (le plugin ne reste plus muet).

### Plugins à un potard (Grain Nord, Gorge, Poupée, Patine)
- Nouvelle reverb et nouveau décaleur (voir plus haut).
- Une chaîne « Classique » est ajoutée dans le code (`Source/OneKnob/Classic*.h`). Ce chantier est en cours et n'est pas relié ni validé.
- Limite connue : un NaN en entrée peut encore y rester en sortie. Pas encore corrigé.

### Orbe Autotune
- Nouveau moteur « Orbe » (`docs/tune-algo.md`) : détection YIN, Viterbi en ligne, courbe de correction, resynthèse à raccords synchrones. Les mesures sont dans `tests/tune_core_test.cpp` ; les 8 points d'écoute de `tune-algo.md` § 10 n'ont jamais été écoutés.
- Édition note par note (type Melodyne), avec des limites : pas de déplacement dans le temps, pas de sélection multiple, pas d'export MIDI. Elle est liée à la position sur la timeline.
- Bémols en plus des dièses (paramètre `accid`). Nouveaux paramètres `algo` et `expr`.
- Correction d'un « fantôme » de sortie (−321 dB dans les 8 combinaisons testées).
- NaN nettoyé en entrée et en sortie.

### Orbe Looks
- Les 10 looks d'origine gardent leur index. Des looks Rap et Pop électro sont ajoutés à la suite (par exemple Obsidienne, Poussière, Givre, Brume, Mirage). Leurs valeurs sont des hypothèses à juger à l'oreille.
- Catégories affichées dans la liste.
- Nouvelle reverb (voir plus haut).

### Orbe Atelier
- Nouvelle reverb (voir plus haut). Garde d'entrée NaN/infini.

### Orbe Cabine
- Saturation compensée en niveau (voir plus haut). Reverb partagée nouvelle. Garde d'entrée NaN/infini.

### Orbe Master
- Saturation compensée en niveau. Garde d'entrée NaN/infini.

### Orbe Synth, Orbe Pads, Orbe Sample
- Reverb partagée nouvelle. Pads et Sample lisent aussi le MP3 sous Windows (M4A et CAF restent réservés au Mac).

### Orbe Denoiser
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

## Nouveaux plugins

- **Orbe Galet** : courbes et distorsion lisse. 16 préréglages, distorsion sur-échantillonnée ×4 (ADAA), latence 71 échantillons, forme sauvegardée dans l'état. pluginval niveau 10 : réussi.
- **Orbe Accroc** (en cours, non évalué par le validateur) : séquenceur d'accrocs (bégaiement, stutter, rampe, tape-stop). Les compresseurs lampe, opto et FET annoncés ne sont pas utilisés par ce plugin pour l'instant : les briques `Source/Dsp/Compressors.h`, `Saturation.h` et `DeEssDyn.h` existent et sont testées, mais aucun plugin ne les inclut.

## À écouter dans FL Studio (non vérifié)

Les 8 points de `docs/tune-algo.md` § 10 ; le son des anciens projets Voile, Looks et un-potard avant et après la vague 2 ; la nouvelle reverb sur les presets de Cabine, Synth, Pads et Sample ; Galet (« Pompe », distorsion à fort drive) ; le bloom de la 808 ; la liste de contrôle FL n° 8 (compensation de latence Autotune), n° 9 (bypass) et n° 12 (rechargement de projet).
La vague 2 n'a jamais été compilée sous MSVC (la CI ne tourne que sur `main`).
