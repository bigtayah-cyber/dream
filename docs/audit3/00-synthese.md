# Audit 3 : synthèse (regard d'un ingénieur DSP, sound designer et designer produit)

*10 novembre 2026, 12 h–14 h 30 (Paris). Lecture seule du code : aucun fichier source modifié, rien de commité, aucun build de plugin. Mesures faites avec des bancs hors ligne sans JUCE (`/tmp/b_audit3/*.cpp`, g++ -O2, un seul cœur) sur une voix synthétique. Captures d'éditeurs : `scratchpad/v3/`, `rel_*.png`, `looks_a.png`, `galet_a.png`, `cab.png`, `onek_all.png`. **Rien n'a été écouté.** Orbe Mix est exclu (deux agents dessus). Le correctif des « claques » de Looks était en cours dans `OrbeChain.h` pendant l'audit.*

## Verdict

La suite n'a plus de défaut grossier : suréchantillonnage, ADAA, EQ appairé, reverb FDN saine, décaleur synchrone à la période. Le moteur « Actuel » des un-potard est de niveau pro (niveau constant à ±0,5 dB sur toute la course). **Elle sonne « cheap » à cause de deux vitesses** : les briques neuves (`Dsp/Compressors.h`, `Saturation.h`, `DeEssDyn.h`) ne sont branchées que dans les un-potard et Relance. Les plugins que l'artiste utilise le plus (Looks, Atelier, Cabine, Master) tournent encore sur le compresseur et le de-esser figés de la vague 1. Elle est « difficilement réglable » parce qu'il manque les outils de base d'un produit (presets utilisateur, annuler, A/B, niveau égal) et que certaines plages sont mal échelonnées.

## 1. Rendus « cheap » : ce que disent les mesures (détail : `01-rendus.md`)

| Constat | Mesure | Correction |
|---|---|---|
| **Looks : attaques accentuées** (le « snare aigu ») | Rapport attaque/corps relevé de **+4,3 dB** (Gueulante), +3,5 (Verre Pilé, Paillettes), +2,7 (Givre) ; jusqu'à **+3,6 dB au-dessus de 6 kHz** | Anticipation de 1 à 2 ms sur le compresseur, de-ess **avant** le shelf aigu, shelf appairé à la place du RBJ, attaque ≥ 10 ms sur ces 3 looks. Le correctif en cours traite les échos et la reprise, pas l'attaque sèche |
| **Compresseur crête commun** (`onek::Comp`, Looks, Atelier, Cabine, Master) | DHT **0,63 %** à 110 Hz (réglages de Verre Pilé), contre **0,06 %** pour `OptoComp`, déjà écrit | Brancher `OptoComp`/`FetComp`/`VariMuComp` |
| **Niveau automatique de Looks** dynamique (×0,1 à ×3, RMS de 150 ms) | Lu dans `OrbeChain.h:203` | Gain statique tabulé par look et par macro ; AGC ±3 dB maximum |
| **Doubleur de Cabine** = chorus à LFO sinus | AM **0,168**, contre 0,104 pour l'ADT de Voile | `dsp::AdtVoice` (Cabine, Looks, Atelier) |
| **Saturation de Cabine** ×2 | Repliement **−48 dBc** à sat 10 (−73 dBc jusqu'à sat 5) | ×4 + ADAA (brique de Galet) |
| **Master** : compresseur de bus à détecteur crête, relâche fixe de 200 ms ; compensation de saturation dynamique | `MasterDsp.h:234,258` | Vari-mu ou RMS avec relâche auto ; compensation statique ; écrêteur doux ×4 avant le limiteur |
| **Orbe Beat** : charleston = bruit et un seul biquad, kick tanh sans suréchantillonnage | `OrbeBeatEngine.h:315-333` | Charlestons métalliques (6 carrés), clap stéréo, ADAA |
| Mono forcé dans OrbeChain ; coefficients d'EQ non lissés (zipper sous automation) ; espace de Cabine figé | `OrbeChain.h:135` ; `CabineDsp.h:148-155` | Stéréo lié ; lissage de 20 ms comme Orbe EQ ; sélecteurs Espace et Division |

Ce qui est **sain** (à ne pas retoucher maintenant) : la reverb FDN (queue diffuse, persistance modale de 0,12 à 0,20, RT60 juste à 10 % près ; seul le début est un peu granuleux, NED 0,64 à 30 ms), la saturation ×8 de Looks (−86 dBc), le Synth (PolyBLEP, ADAA) et l'EQ appairé.

## 2. Réglabilité (détail : `02-reglabilite.md`)

- **Manque à toute la suite** : presets utilisateur (aucun plugin d'effet ne sait enregistrer), annuler/rétablir (aucun `UndoManager`), A/B (seulement dans Orbe EQ), bouton « niveau égal », assistant « écouter 5 s » pour placer seuils et gain d'entrée. Tout se fait **une seule fois dans le kit v3**.
- **Plages mal échelonnées** : ratio d'Atelier de 1 à 20 en linéaire (la moitié de la course au-dessus de 10:1) ; Q en linéaire ; doubles d'Atelier en demi-tons au pas de 0,01 (les 30 cents utiles occupent 1,25 % de la course) ; longueur et vitesse du punch de la 808 en linéaire ; graines d'Accroc et de Relance en flottant de 1 à 999 sur un potard ; seuils de de-ess absolus ; unités « x10 » et saturation sans unité.
- **Défauts non musicaux** : un module d'Atelier qu'on allume ne fait rien (seuil du compresseur à 0 dB).
- Bien : les potards v3 (Maj pour le fin, double-clic, saisie, molette) et les skews de Cabine, Master et Autotune.
- Attention : ne changer que les **skews** et pas les bornes, sinon l'automation existante change.

## 3. Design (détail : `03-design.md`)

Le kit v3 (Voile, Autotune, Relance) a le niveau d'un produit 2025. Ce qui fait amateur : **trois langages visuels** (v3, rack « Classique » à vis et chromes, panneaux un-potard), et un menu qui propose encore « Classique » ; des écrans sans hiérarchie (Galet 23 potards, Cabine 15, Atelier 38) ; des retours trop grossiers (réduction de gain sur 7 LED dans Cabine, pas de LUFS dans Master) ; des doublons (« Note à zéro » et « Notes à zéro » dans Autotune) ; la virgule et le point décimal mélangés. Il manque une **barre supérieure unique** (presets, A/B, annuler, niveau égal, taille), un tiroir « avancé », des retours qui expliquent ce que fait chaque macro.

## 4. Identité (détail : `04-identite.md`)

Fil rouge : **la suite écoute la voix et la tonalité, et tout s'y accroche.** 21 idées réalistes avec le moteur actuel. Les plus rentables (S, 2 jours chacune) : **Double fantôme** (le double suit la hauteur brute pendant que la lead est accordée), **De-ess vers l'espace** (la sibilance retirée part dans la reverb), **Lancer de fin de phrase** (le delay ne s'ouvre que sur le dernier mot), **Pompe sans sidechain** (Galet détecte le kick qui le traverse), **Coupes sur la voix** (Accroc déclenché en fin de phrase), **808 qui suit les accords**, **Flip dans la tonalité** (Sample).

## 5. Qualité logicielle (détail : `05-logiciel.md`)

- **23 plugins sur 26 n'écrivent pas `stateVersion`**, alors que le son de Looks, Atelier, Cabine et Master va changer. C'est à faire avant tout le reste.
- Pads/Sample : les voix gardent un pointeur brut sur une banque libérée 30 s après remplacement. Crash possible sur une tranche « gate » longue.
- CI : 3 tests console sur une quinzaine ; pluginval « latest » non figé et sans tests d'interface alors que les éditeurs sont neufs.
- Signature ad hoc et `xattr -cr` : suffisant pour l'artiste, pas pour une diffusion.
- Bien : restauration d'état défensive (Denoiser, Autotune), thread audio sans verrou bloquant, gardes NaN.

## 6. Feuille de route (détail : `06-feuille-de-route.md`)

| Livraison | Chantiers | Impact | Effort |
|---|---|---|---|
| **2e : « ça ne fait plus cheap »** | A1 `stateVersion` partout · A2 tranche voix commune (Opto/FET, de-ess relatif, Saturator, EQ appairé lissé) · A3 attaques de Looks · A4 niveau statique de Looks · A5 doubles ADT · A6 Cabine ×4 et Master (bus, compensation) · A7 skews et unités · A8 presets utilisateur, annuler, A/B, niveau égal · A9 CI complète · A10 banques de Pads · **A11 écoute réelle** | Très fort | ≈ 3 semaines |
| **3e : « ça a une identité »** | B1 v3 partout, « Classique » retiré · B2 hiérarchie (tiroirs, GR continue, LUFS) · B3 Beat refait · B4 sept signatures S · B5 macros Atelier, Cabine, Master · B6 aperçu d'Orbe sur Synth · B7 version visible, signature | Fort | ≈ 3 semaines |
| **Plus tard** | Lien entre instances (« Fil ») et ce qu'il ouvre (gamme qui suit l'accord, rapport voix/instru, conflit spectral), Beatbox vers Beat, chops sur syllabes, 3 algorithmes de reverb | Fort | L |
| **À écarter** | Nouveaux looks ou presets avant A2-A4 ; un mode « ancien son » par moteur ; IA générative ; talkbox avec sidechain ; retouche de la reverb maintenant | — | — |

Ordre conseillé : **A1 → A3 → A2 → A4/A5 → A8**, puis écouter (A11) avant de continuer.

## 7. Ce que je n'ai pas pu vérifier

L'écoute et FL Studio (aucun son joué). Le CPU réel. Les interfaces en direct (redimensionnement, focus, rendu à 150 %). Le repliement de la 808, de Beat et du Synth. Le time-stretch de Pads et Sample. Les fréquences autres que 48 kHz. Orbe Mix. La voix de test est synthétique : une vraie prise rap (plosives graves, sifflantes) peut donner des écarts plus grands ou plus petits sur les attaques de Looks. Il faut rejouer `looks.cpp` sur 3 vraies prises.

## Fichiers

`01-rendus.md` (mesures et 26 plugins) · `02-reglabilite.md` (plages, courbes, macros) · `03-design.md` · `04-identite.md` (21 idées) · `05-logiciel.md` (risques et tests) · `06-feuille-de-route.md` (tableau impact, effort, risque).
