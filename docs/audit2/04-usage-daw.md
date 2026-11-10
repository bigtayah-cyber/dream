# Audit 4 : usage quotidien dans FL Studio et QA hôte

*9 nov. 2026. Lecture du code au commit `154c0f9`. Aucun fichier source n'a été modifié. Pas de build, aucun essai dans FL : les points notés « à vérifier dans FL » dépendent du comportement de l'hôte.*

## Ce qui tient déjà
Bypass avec fondu et chemin sec compensé, latences recalculées à chaque `prepareToPlay`, samples de Pads et Sample **embarqués** dans l'état (projet portable Mac ↔ Windows), double-clic de retour à l'origine, molette, infobulles partout, repeints partiels.

---

## P0 : perte de travail ou son faux

**1. Fredonne ne sauvegarde pas la prise.** `OrbeFredonne/FredonneProcessor.cpp:173-176` n'écrit que l'APVTS. Les notes détectées, `takeStart` et la piste de hauteur disparaissent à la réouverture du projet. En plus, `prepareToPlay` efface l'enregistrement si la fréquence change (l. 46-50). L'artiste qui n'a pas glissé le MIDI perd sa mélodie.
*Correctif* : sérialiser `Take` (notes, bpm, takeStart : quelques Ko) et, en option, l'audio capté en 16 bits, comme `SampleHolder::writeTo`.

**2. Orbe FX baisse le volume de la piste.** `Orbe/OrbeProcessor.cpp:337` : `buffer.applyGain(master)` s'applique aussi à l'audio entrant, car la version FX ne vide pas le tampon (l. 275-277). Avec MASTER à 0,8 par défaut, la piste perd 1,9 dB ; MASTER à 0 la coupe. Sans séquence (sortie l. 281), le gain n'est pas appliqué : niveau incohérent.
*Correctif* : rendre l'aperçu dans un tampon séparé, lui appliquer `master`, puis l'ajouter au signal.

**3. Sortie MIDI d'Orbe : des notes communes sont avalées.** `OrbeProcessor.cpp:318-335` : les note-on du bloc sont ajoutés **avant** les note-off en attente. Si la fin d'une note tombe sur le même échantillon que le début de la suivante à la même hauteur, le synthé de FL reçoit on puis off et la note est coupée net. Cas typiques : notes communes du voice leading, basse legato, accords tenus, chevauchements dus à l'humanisation.
*Correctif* : vider `pend` avant les déclenchements. Avant chaque note-on, émettre le note-off d'une même note (même canal) encore en attente. Avancer les note-off d'un échantillon.

## P1 : majeur

**4. Mode Pattern de FL : Orbe et Beat ne jouent que la longueur du pattern.** `OrbeProcessor.cpp:288-301` et `OrbeBeat/OrbeBeatProcessor.cpp:127-140` se calent sur `ppq`. En mode Pattern, FL donne la position dans le pattern courant. Un pattern vide fait une mesure : seule la mesure 1 des 4 ou 8 tourne en boucle (à vérifier dans FL).
*Correctif* : repérer un retour en arrière du ppq plus court que la boucle et continuer un compteur interne. À défaut, l'écrire dans l'aide : « pattern de 4 ou 8 mesures, ou mode Song ».

**5. Use-after-free possible dans Orbe pendant l'édition.** `OrbeProcessor.h:92` et `.cpp:254` ne gardent que les 16 derniers rendus. Or `setNote` publie à chaque événement de glisser (`OrbeEditor.cpp:329-332`), soit 60 à 120 fois par seconde. Avec un tampon FL de 2048 à 4096 échantillons, ou un thread audio préempté, le rendu en cours de lecture peut être libéré.
*Correctif* : retenir les rendus pendant un temps fixe, comme `osamp::Swap` (au moins 2 s), ou ne publier qu'au rythme du timer.

**6. Paramètres « structurels » automatisables.** Style, Tonique, Gamme et Longueur d'Orbe (`OrbeProcessor.cpp:16-37`), `preset` (808, Cabine, Atelier), `look`, `pat`, `kit`, `capture` et `latmode` apparaissent dans « Browse parameters » de FL.
- Automatiser le style d'Orbe régénère la grille sur le thread message (`applyDirty`, l. 157). Cela écrase les notes éditées et vide l'historique d'annulation.
- Le preset de 808 est appliqué par un timer (`Orbe808Processor.cpp:118-127`) : l'export hors ligne diffère de la lecture.
- Autotune change sa latence dans le thread audio (`AutotuneProcessor.cpp:114`).

*Correctif* : `withAutomatable(false)` sur ces paramètres.

**7. Orbe Sample : automation non déterministe à l'export.** La hauteur, la vitesse, le filtre, les bits et la réverbe sont calculés par un thread de rendu après 110 ms sans changement (`SampleProcessor.cpp:9-20`, `104-117`). En export accéléré, les changements arrivent en retard ou pas du tout. Au chargement du projet, le plugin reste muet tant que le rendu n'est pas prêt.
*Correctif* : en `isNonRealtime()`, attendre la fin du rendu (bloquer est permis hors temps réel). Marquer ces paramètres non automatisables, ou passer le filtre, les bits et la réverbe en temps réel.

**8. MP3, M4A et CAF proposés mais illisibles sous Windows.** `SamplerCommon.cpp:39` appelle `registerBasicFormats()` sans `JUCE_USE_MP3AUDIOFORMAT`. Pourtant, les filtres de fichiers proposent ces formats (`PadsEditor.cpp:110,166`, `SampleEditor.cpp:91,112`). Le beatmaker obtient « Fichier illisible » sur son format le plus courant.
*Correctif* : ajouter `JUCE_USE_MP3AUDIOFORMAT=1` à `ORBE_SUITE_DEFS` et retirer m4a et caf des filtres hors Mac.

**9. Fenêtres plus grandes que l'écran d'un portable Windows.** Tailles d'ouverture :
- Atelier : 1400 × 695 (minimum 1120 × 556, `AtelierEditor.cpp:70-72`) ;
- Cabine : 400 × 760 (`CabineEditor.cpp:61-63`) ;
- Synth : 1100 × 660 ;
- Beat : 1100 × 640.

À 1920 × 1080 et 150 % (1280 × 720 logiques, cas courant), Atelier et Cabine dépassent de l'écran, poignée comprise. La taille n'est mémorisée que dans les plugins un potard (`OneKnobEditor.cpp:163,211`).
*Correctif* : taille initiale = min(défaut, 90 % de `Displays::getPrimaryDisplay()->userArea`), minimums plus bas, taille sauvegardée dans l'état de tous les plugins.

**10. Fredonne est un instrument qui a besoin d'un micro.** Son `plugin.cmake` déclare `IS_SYNTH TRUE` avec une entrée audio. Dans FL, un générateur reçoit difficilement l'audio d'une piste du mixeur (à vérifier).
*Correctif* : une variante « Fredonne FX », sur le modèle d'Orbe FX, à insérer sur la piste micro, avec sortie MIDI.

## P2 : gênant

**11. Compatibilité des anciens projets.** Les états n'ont aucun numéro de version, et un paramètre ajouté prend sa valeur par défaut. Un projet Grain Nord, Gorge, Poupée ou Patine d'avant `01abf18` se recharge donc avec l'Auto-entrée **active** (`OneKnobProcessor.cpp:25`) et sonne autrement. Détect sauve une struct brute en base64 (`DetectProcessor.cpp:106,116`) : toute modification de la struct effacera le résultat.
*Correctif* : propriété `stateVersion`. Pour un état v0, forcer `autoin=false`. Sérialiser Détect champ par champ.

**12. Notes MIDI bloquées.** `OrbeProcessor.cpp:74` remet `nPend` à zéro sans envoyer les note-off. Si l'on change le tampon ou la carte son pendant la lecture, le synthé cible garde des notes bloquées.
*Correctif* : un drapeau « tout couper » traité au bloc suivant (note-off et CC123 sur les 4 canaux).

**13. Trois conventions d'octave.** Pads et Sample : MIDI 36 = « C1 » (`SamplerCommon.cpp:108`). Orbe : 60 = C4 (`OrbeEditor.cpp:9`). FL : 60 = C5. Le pad 1 « C1 » est en C3 dans le piano roll de FL.
*Correctif* : une seule fonction `kit::noteName`, sur la convention de FL.

**14. Latence cumulée d'une chaîne voix.** Latences par plugin :
- Cabine : 16 échantillons ;
- Autotune (mode qualité) : environ 1790 ;
- Looks, Atelier, Voile et un potard : environ 16 ms, plus le suréchantillonnage et, pour Looks et Atelier, le limiteur ;
- Denoiser : 2048.

Le PDC de FL compense au mixage, mais à la prise le chanteur s'entend avec 80 à 110 ms de retard.
*Correctif* : un mode « direct » (décaleur court, sans look-ahead) dans Looks, Atelier et les un potard, comme celui d'Autotune, et un voyant de latence sur la face.

**15. Runtime MSVC et CI Windows.**
- `CMakeLists.txt:8-10` ne fixe pas `CMAKE_MSVC_RUNTIME_LIBRARY` : les DLL dépendent du redistribuable VC++, absent de certains PC neufs.
- `build-windows.yml:25` compile avec `-k 0` : un plugin qui ne compile pas disparaît du zip sans erreur.
- pluginval tourne avec `--skip-gui-tests` et `continue-on-error` (l. 45 et 52) : l'ouverture répétée des éditeurs n'est jamais testée.
- La liste d'empaquetage cite un « Orbe Galet » qui n'existe pas (l. 35).

*Correctif* : runtime statique `MultiThreaded`, échec du job si un `.vst3` attendu manque, pluginval avec les tests d'interface.

**16. Presets d'usine invisibles dans FL.** Seuls Voile, Synth, Autotune, Looks et 808 exposent `getNumPrograms()`. Les presets de Cabine (11), Atelier, Sample (9) et Beat (7 kits) n'en passent pas : les flèches de presets du wrapper de FL ne font rien.
*Correctif* : exposer ces presets comme programmes.

**17. Beat joue sur tout le morceau.** Le séquenceur interne démarre dès que FL lit (`seq` vrai par défaut), quels que soient les patterns placés dans la playlist. Sortie mono recopiée sur les deux voies (`OrbeBeatProcessor.cpp:170-171`).
*Correctif* : un mode « déclenché par notes » (une note tenue lance le pattern A, B, etc.) pour arranger dans la playlist.

## P3 : mineur

- **Saisie de valeur et réglage fin absents.** `Pot` n'a pas de boîte de texte (`Kit/Widgets.h:321`), et JUCE n'a pas de réglage fin à Maj + glisser (Ctrl et Alt basculent en mode vitesse). *Correctif* : Maj = sensibilité × 4 ; Ctrl ou Cmd + clic ouvre un champ qui accepte « -6 dB » ou « 2k ».
- **Clic droit sans menu de l'hôte** : `getHostContextForParameter` inutilisé.
- **Noms de paramètres inégaux** : « Densite », « Sync BPM hote », « Zone debut » sans accents ; volumes d'Orbe affichés « 0.80 » sans unité (`OrbeProcessor.cpp:31,36`).
- **Réglages adaptatifs.** Auto-entrée, tonalité Auto et auto-gain varient selon le point de départ de la lecture ou de l'export. Ajouter un bouton « figer ».
- **CPU par instance (estimation à 48 kHz, un cœur récent)** :
  - un potard et Voile : 2 à 4 %. À lui seul, le YIN 512 × 340 toutes les 5,3 ms coûte environ 33 M MAC/s ; s'y ajoutent 3 décaleurs et le suréchantillonnage × 8 ;
  - Looks, Atelier, Autotune : 3 à 5 % ;
  - Denoiser : 1 à 2 % en moyenne, mais par à-coups (4 FFT de 2048 tous les 512 échantillons ; le code contient trois FFT maison différentes) ;
  - Synth : 3 à 8 % selon l'unisson.

  Gain facile : différence du YIN par FFT (environ 10 fois moins cher). En bypass le calcul continue (voulu) : le dire dans l'aide.
- **Installateurs.** Relance administrateur cassée par une apostrophe dans le chemin (`Installer-dreamaws.bat:5`) ; le `.command` Mac est en quarantaine (documenter clic droit > Ouvrir).
- **Accessibilité et raccourcis.** Ni `AccessibilityHandler` sur `Seg` ni ordre de tabulation. Ctrl+Z n'existe que dans Orbe (`OrbeEditor.cpp:346`) et FL peut l'intercepter (à vérifier).

---

## Fonctions d'usage manquantes, classées par valeur pour l'artiste
1. **Rappel complet** de la prise Fredonne (P0) et de tout ce qu'on édite.
2. **Presets utilisateur** (`Documents/dreamaws/Presets/<plugin>`, flèches ◀ ▶). En attendant, « Save preset as » du wrapper FL (`.fst`) marche : le dire dans l'aide.
3. **Annuler / rétablir** sur les réglages : l'`UndoManager` de l'APVTS est à `nullptr` partout ; seul Orbe a son historique.
4. **A/B** avec « copier A vers B » (seul l'EQ l'a).
5. **Mode direct** faible latence pour enregistrer la voix.
6. **Glisser l'audio** (WAV) depuis Orbe, Beat, Sample et Fredonne.
7. **Entrée sidechain** : DeEsser, ducking dans Master, futur « Colle ». FL route le sidechain VST3 depuis le mixeur.
8. **Saisie de valeur, réglage fin, menu clic droit.**
9. **Taille de fenêtre mémorisée** et zoom 75 / 100 / 125 / 150 %.
10. **Mode « MIDI seul »** pour Orbe et Orbe FX (aperçu coupé quand la sortie MIDI est active) et choix des canaux.
11. **Tonalité et tempo de Détect partagés** avec Orbe, Autotune et les un potard.
12. **MIDI learn interne** : priorité basse, FL a déjà « Link to controller ».

---

## Checklist de test manuel dans FL Studio (15 points)
1. Manage plugins > Find installed plugins : 24 plugins ; les 7 instruments dans Generators, le reste dans Effects.
2. Ouvrir et fermer chaque éditeur 10 fois, puis deux instances ensemble : ni plantage, ni fenêtre noire (Direct2D de JUCE 8).
3. Écran à 125 % et 150 % sous Windows, Retina sur Mac : texte net, fenêtre entière à l'écran (Atelier, Cabine, Synth), coin de redimensionnement accessible. Rouvrir : la taille est-elle gardée ?
4. Orbe en Generator : Wrapper > Settings > MIDI output port = 1, et un FLEX sur le port 1. Activer « Sortie MIDI » : les 4 pistes arrivent, et aucune note commune ou tenue ne saute.
5. Orbe et Beat en mode Pattern (pattern vide, puis pattern de 4 mesures), puis en mode Song : les 4 ou 8 mesures jouent-elles ?
6. Orbe FX sur une piste avec une boucle, aperçu muet : le niveau crête dans le mixeur est le même avec et sans le plugin.
7. Glisser le MIDI d'Orbe, de Beat et de Fredonne dans la playlist et dans le piano roll.
8. Autotune (qualité), Denoiser et Looks sur une voix, caisse claire sur une autre piste : aucun décalage à l'écoute ni à l'export (PDC). Basculer Autotune en basse latence en lecture.
9. Bypass par le bouton IN, par la désactivation du slot dans le mixeur, puis par l'automation de « Bypass » : pas de clic, même niveau.
10. Pendant la lecture, changer le tampon ASIO (128 → 1024) puis la fréquence (44,1 → 48 → 96 kHz) : pas de silence, pas de note bloquée, même son.
11. Exporter en WAV un projet qui automatise Caractère (un potard), Hauteur (Sample) et Style (Orbe), puis comparer avec la lecture en temps réel.
12. Sauver, fermer, rouvrir : on retrouve les réglages, les notes éditées d'Orbe, la prise de Fredonne, les samples de Pads et Sample, le volet avancé et la tonalité des un potard.
13. Dans Pads et Sample, charger un MP3, un WAV 24 bits et un FLAC, par glisser depuis le Browser de FL puis par le bouton.
14. Tools > Last tweaked > Create automation clip sur un potard : il suit la courbe, et les valeurs affichées dans FL ont leurs unités.
15. 8 Gorge, 2 Synth et un Master, tampon de 256 : relever le CPU, ouvrir et fermer les éditeurs en lecture, sans craquement.
