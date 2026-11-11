# Orbe Autotune : architecture cible et plan (audit 4)

*10 nov. 2026. S'appuie sur `02-mesures-existant.md` (preuves), `01-recherche-web.md`, et les deux rapports de recherche
`04-algos-pitch-formants.md` et `05-son-cible-et-chaines.md` (voir § 8). Rien n'est implémenté. Effort : S ≤ 1 j, M 2-4 j,
L 5-10 j (une personne + Claude, bancs compris).*

## 1. Principe : ne pas tout jeter

Le cœur Orbe (YIN décimé + Viterbi + courbe centre/expression + résidu LPC à raccords synchrones) est **de la même famille
qu'Auto-Tune** (brevet Hildebrand, expiré : rééchantillonnage + périodes répétées / supprimées) et il est déjà le meilleur du
dépôt sur la justesse, l'escalier, les formants vers le haut. Les écarts mesurés ne sont pas « l'algorithme est mauvais » mais :

1. **le réglage d'usine** cache le hard-tune (Satin par défaut, presets qui ne changent pas le rendu) ;
2. **les bords** : attaque non corrigée en Direct (~24 ms), ~10 ms de signal irrégulier à chaque changement de note, peigne
   quand la correction est faible, pas de stabilisation de note ;
3. **le timbre en descente** (résidu rééchantillonné → voix assombrie), voix soufflée non suivie ;
4. **l'entourage** : doublage statique, harmonie granulaire / sans formants, aucune couleur, pas de « personnalité » réglable.

Cible : un moteur unique, deux latences, **un comportement de hard-tune réglable comme un instrument**, et une chaîne de couleur
intégrée optionnelle.

## 2. Moteur cible

```
entrée ─┬─► ANALYSE ───────────────────────────────────────────────────────────────┐
        │   YIN décimé (actuel) + fenêtre courte d'attaque (≤ 6 ms, f0 > 150 Hz)    │
        │   + détecteur d'attaque (flux d'énergie) + Viterbi (actuel)               │
        │   + réglage « Suivi » (seuils de voisement : serré ↔ souple/soufflé)      │
        │   + marques de période (pics du résidu LPC = instants de fermeture)       │
        │                                                                           ▼
        │   COMPORTEMENT (courbe de correction, actuelle + 5 ajouts)          note visée, s(t)
        │   Vitesse (−3 … 400 ms) · Transition (vitesse au changement de note)
        │   Stabilisateur (0 / 40 / 80 / 200 ms, Qualité seulement au-delà de 40)
        │   Warble (0 = jamais de bascule sur vibrato … 100 = bascule libre type Classic)
        │   Humanize / Sustain (vitesse fonction de l'âge de la note) · Flex (n'agit qu'à l'approche)
        │   Expression : centre / vibrato / dérive séparés (−100 … +200 %, façon Melodyne)
        │                                                                           │
        ├─► RESYNTHÈSE VOIX PRINCIPALE                                              ▼
        │   blanchiment LPC (actuel) → résidu
        │   → TD-PSOLA du résidu sur marques de période (pas de rééchantillonnage) : la forme de
        │     l'impulsion ne s'étire plus en descente ; rapport changé seulement à une marque
        │   → part apériodique (souffle) : non raccordée, gardée telle quelle (séparation simple
        │     résidu = impulsions + bruit par moyenne synchrone)
        │   → recoloration LPC (actuelle), « Gorge » = facteur d'enveloppe sur la recoloration
        │   → toujours 100 % traité quand actif (fin du mélange partiel) ; sec exact sinon
        │
        ├─► VOIX AJOUTÉES (même résidu, même enveloppe, lecteurs indépendants)
        │   Doubleur ADT 2 à 4 voix : désaccord en bruit filtré (±3-12 ct, 0,3-3 Hz), retard qui dérive
        │   (8-30 ms), « Gorge » ±0,3 dt, filtre HF différent, largeur L/C/R (comme MetaTune Doubler)
        │   Harmonies 1 à 3 voix : intervalles de gamme ou d'accord (lien Orbe, V6/V7 d'audit 3),
        │   formants préservés (même recoloration), décalage de temps humain, panoramique
        │
        └─► COULEUR (optionnelle, après mix) : Saturation douce (×2 suréch.) · Air (étagère dynamique)
            · De-esseur léger · Delay synchro 1/4, 1/8. · Reverb sombre · Width — briques de la suite
```

### Choix argumentés

- **Pas de passage au phase vocoder pour la voix principale.** Spectral = latence (≥ 1 trame) et transitoires étalés ; le
  hard-tune rap se joue dans les 10 premières ms et en Direct. Le domaine temporel synchrone à la période reste le bon choix
  (c'est celui d'Auto-Tune). Signalsmith Stretch (MIT) peut servir pour les **grandes** transpositions (octave basse, poupée
  hyperpop) en Qualité, où ses formants « moins nets » sont acceptables.
- **TD-PSOLA sur le résidu plutôt que rééchantillonnage du résidu** : corrige la pente mesurée en descente (−2,7 dB/oct à
  −3 dt, −9,6 à −7). Repli moins cher si le PSOLA du résidu déçoit : égaliseur de pente fonction du rapport (≈ +0,9 dB/oct par
  demi-ton descendu, à calibrer sur le banc [E]).
- **Changement de rapport synchronisé aux marques de période** : hypothèse principale pour les ~10 ms irréguliers par bascule
  ([C'] 12 %). À vérifier d'abord par instrumentation (journal des raccords autour d'une bascule) avant de réécrire.
- **Attaque** : à l'apparition d'une attaque voisée, prendre la note de la première estimation fiable sur fenêtre courte et
  imposer la correction sans rampe (vitesse 0 effective) pendant les 30 premières ms si Vitesse ≤ 10 ms. C'est le « coup de
  glotte » accordé du hard-tune. En Qualité, l'anticipation existe déjà (accroche 10,5 ms).
- **Vitesse négative** (signature MetaTune, mécanisme non publié) : interprétation proposée et testable : au changement de note,
  **dépasser** la cible de quelques cents pendant 5-15 ms (pré-accentuation du saut), ce qui rend l'escalier plus « claqué ».
  À valider à l'écoute A/B ; ne pas l'annoncer comme équivalent MetaTune.
- **Warble comme réglage, pas comme bug** : la sortie bascule aujourd'hui ~10 fois/s sur un vibrato à cheval ([C]). Certains
  le veulent (Future, « warble »), d'autres non (MetaTune Stabilizer). Un potard le rend volontaire.

## 3. Modes et contrôles (UI)

Remplacer « Rendu Satin / Précis / Robot » par **un style + une macro** :

| Contrôle | Rôle | Remplace |
|---|---|---|
| **Style** : Naturel · Pop · Hard · Classic | réglages internes cohérents (vitesse, transition, stabilisateur, warble, expression) | Rendu |
| **Dureté** (macro 0-100) | de Naturel à Hard : vitesse 200 → −3 ms, transition, stabilisateur, expression | Vitesse + Expression + Glissement |
| Avancé (repliable) | Vitesse, Transition, Humanize, Flex, Stabilizer, Warble, Suivi, Vibrato (centre/modul./dérive) | — |
| **Gorge** (±) et **Formants** (±) | timbre, dans les deux latences | Formants (Qualité seulement) |
| **Doubleur** (point XY façon MetaTune : quantité × largeur) | ADT | Doublage |
| **Harmonie** (1-3 voix, gamme / accord) | | Harmonie |
| **Couleur** : Drive · Air · Espace (delay+reverb) | 3 potards, OFF par défaut | — |

Retours visuels : HeatMap entrée / sortie (MetaTune), marque de bascule de note (pour voir le warble), voyant « non suivi »
quand le voisement décroche (voix soufflée), voyant « tonalité en recherche » très visible (aujourd'hui : aucune correction tant
que l'Auto n'est pas verrouillé).

Compatibilité : nouveaux paramètres **en fin**, `rendu` gardé et traduit (Satin → Style Naturel, Précis → Pop, Robot → Hard),
projets anciens au son identique (règle du dépôt, `stateVersion`).

## 4. Presets (après refonte)

Chaque preset fixe **tout** le comportement (fin du `render = −1`). Exemples de cibles (à régler à l'oreille, recettes de 05) :
*Hard 0* (T-Pain/Travis : Dureté 100, warble 30 %, attaque accordée, doubleur 20 %, Espace 1/4) · *Warble Future* (vitesse
15-20 ms, warble 80 %, Drive 30 %) · *Mélodique trap* (12 ms, stabilisateur 40, doubleur 30 %) · *Ad-libs* (Dureté 100,
Gorge +1, Drive, delay 1/8.) · *Hyperpop* (Hard + Formants +3, octave haute, Drive 50 %, Air) · *Chant naturel* (Satin actuel) ·
*Rap à peine* (vitesse 60, Flex fort, Humanize).

## 5. Fonctions « qu'on ne trouve pas ailleurs » (réalistes)

1. **Warble dosable** (0-100 %) : personne ne le propose explicitement (Auto-Tune : Classic oui/non ; MetaTune : supprimé).
2. **Escalier dosable** : nombre de paliers par demi-ton et durée de palier dans les glissés (mélismes numériques à la demande).
3. **Gamme qui suit l'accord** via le lien Orbe (V6/V7 d'audit 3) : harmonies justes à l'accord, notes de passage justes.
4. **Formant rythmé** synchronisé au tempo (V5 d'audit 3) sur la voix ou les doubles.
5. **Doubleur « vraie prise »** : chaque double a sa propre courbe de correction (vitesse décalée de 3-5 ms, recette Antares)
   au lieu d'une copie désaccordée.
6. **Mode ad-lib automatique** : phrases courtes (< 600 ms) traitées plus dur que les longues (Humanize inversé).

Exclus volontairement : clonage de voix, IA lourde (licences, CPU, latence), ARA (FL Studio : support à vérifier, coût L+).

## 6. Feuille de route

| Phase | Contenu | Effort | Risque | Gain attendu | Jalons mesurables (banc `/tmp/b_at4`, à intégrer aux tests du dépôt) |
|---|---|---|---|---|---|
| **P0 « ça sonne hard » ** | Presets qui fixent le rendu ; preset Robot → rendu Robot ; nouvelle instance en Précis (ou Style Pop) ; supprimer le mélange partiel sec/traité en Précis (comme Satin) ; **écoute A11** sur 3 vraies prises | S (1 j) | faible (compatibilité : seulement les nouvelles instances) | très fort, perçu immédiatement | [A] preset Robot sur instance neuve : ≥ 85 % trames ≤ 10 ct (aujourd'hui 28 %) ; [D] +2 ct : ≤ 0,3 dB RMS (aujourd'hui 2,5) |
| **P1 signature hard-tune** | Attaque accordée en Direct ; transitions propres (instrumentation puis rapport changé aux marques de période) ; Stabilisateur ; Warble ; Transition ; Humanize ; vitesse < 0 (dépassement) | M+M (5-7 j) | moyen (régressions sur vibrato naturel : garder les tests du dépôt) | fort : attaque et transitions = 80 % du caractère | [A'] accroche Direct vitesse 0 ≤ 15 ms (33) ; [C'] irrégulier ≤ 3 % (12) ; [C] bascules/s réglables 0 à ≥ 8 ; tests `tune_core_test` intacts |
| **P2 timbre** | TD-PSOLA du résidu ou égaliseur de pente ; part apériodique non raccordée ; Suivi (voix soufflée) ; Gorge en Direct | M-L (4-8 j) | moyen-haut (cœur du moteur) | fort sur transpositions, harmonies basses, voix soufflées | [E] −3 dt : pente ≥ −0,7 dB/oct (−2,7), −7 dt erreur ≤ 4 dB (13,6) ; [G] Suivi souple : ≥ 60 % (5-11) ; THD+N existants ≤ +3 dB |
| **P3 voix ajoutées + couleur** | Doubleur ADT 2-4 voix sur résidu ; harmonies 1-3 voix à formants ; Couleur (Drive, Air, Espace) depuis les briques de la suite ; nouveaux presets | L (6-10 j) | moyen (CPU ×2-3 : marge OK, [H]) | fort sur « complet » | [F] AM doubleur : aléatoire, non périodique (pic spectral de l'enveloppe < 3 dB au-dessus du fond) ; harmonie Direct à formants (erreur ≤ 2 dB à +4 dt) ; CPU total ≤ 12 % |
| **P4 identité + UI** | Style + Dureté, HeatMap, Warble/Escalier, gamme d'accord (lien Orbe), formant rythmé, A/B | M-L | faible-moyen | différenciation | test d'écoute § 7 |

Ordre : P0 tout de suite (un jour, gros effet), puis **écouter** avant P1. Ne lancer P2 que si l'écoute confirme que le timbre
(et pas seulement le comportement) est en cause.

## 7. Validation

**Objectif** : intégrer les bancs [A]-[H] (voix source-filtre avec jitter, shimmer, souffle, scoops) dans `tests/tune_core_test.cpp`
avec les seuils du tableau § 6 ; garder les mesures existantes (THD+N, latence = impulsion, zéro allocation, NaN).

**Écoute A/B (décisive)** :
- 4 prises réelles de l'artiste (rap chanté grave, chant pop aigu, ad-libs criés, voix soufflée), sèches, 16-24 bits.
- Comparer Orbe (avant / après chaque phase) à MetaTune et Auto-Tune (version d'essai) **aux réglages équivalents** : vitesse 0
  chromatique, vitesse 20 en gamme, naturel 120 ms. Niveaux égalisés (LUFS court terme), latence compensée, même chaîne après.
- Aveugle (ABX ou plugin de test aveugle dans FL), 10 essais par paire, 2 auditeurs (l'artiste et son associé). Noter :
  attaque, transitions, warble, timbre (étouffé / tubulaire / brillant), artefacts, « ça sonne fini ».
- Critère de sortie : Orbe non distingué de la référence (≤ 6/10 en ABX) **ou** préféré sur au moins 3 des 4 prises en Hard.

## 8. Intégration des rapports 04 et 05

**Hypothèses de `04` confrontées aux mesures de `02`** :

| Suspect (04 § synthèse 2) | Mesure | Verdict |
|---|---|---|
| (a) la recoloration LPC colore même les petites corrections | [D] à +30 ct : 1,0 dB RMS (Orbe) contre 1,3 (Classique, signal brut) ; [C'] et [I] identiques formants préservés ou libres | **non confirmé** sur voix de synthèse ; à réécouter sur vraie prise. Le chemin brut reste utile comme **couleur « Classic »** (05) |
| (b) le souffle répété devient un peigne | [I] : part hors harmoniques inchangée (±1,7 dB) | **non confirmé** sur ce signal |
| (c) raccords ponctuels concentrés | [C'] ≈ 10 ms irréguliers par bascule, identique formants on/off, même ordre pour l'ancien Splicer | **confirmé comme piste principale** |

Conséquences sur le plan :
- **L'architecture A de `04` (TD-PSOLA sur le résidu, marques glottiques, séparation harmonique / bruit) devient la cible de la
  resynthèse** : elle traite à la fois (c) les transitions (grains de 2T recouvrants au lieu d'un raccord ponctuel) et la pente
  en descente mesurée en [E] (pas d'étirement de l'impulsion). Si l'instrumentation de P1 ne trouve pas de correctif local au
  raccord, **avancer le PSOLA-résidu en P1** plutôt qu'en P2. La séparation H/N reste utile mais n'est plus prioritaire ([I]).
- Latence de A (≈ T, ≤ 16 ms à 62 Hz) : garder la latence Direct actuelle (221 éch.) et **retomber sur le Splicer pour f0 basse**
  ou si les marques sont irrégulières, comme `04` le propose. Les latences déclarées ne changent pas (compatibilité FL).
- Suivi : pYIN (seuils en loi bêta + non-voisement probabiliste) **et** sélecteur de type de voix (plage f0) de `04` vont dans le
  réglage « Suivi » de P2 ; les réseaux (SwiftF0, MIT) seulement pour l'analyse hors ligne de l'éditeur de notes.
- Mode Qualité : architecture B de `04` (vocodeur verrouillé + true envelope, ou d'abord Signalsmith Stretch MIT) pour
  **Formants/Gorge volontaires, harmonies et octaves**, pas pour la voix principale.
- Éditeur de notes hors ligne : architecture C de `04` (type WORLD, BSD) = piste P4+, hors périmètre immédiat.

**Apports de `05` intégrés** :
- Le constat central de `05` (« un autotune qui sonne complet en solo = correcteur + épaisseur + couleur + espace ») justifie P3 ;
  mais nos mesures montrent que **P0-P1 (comportement, attaque, transitions, peigne) passent avant** : sans eux, la couleur ne
  ferait que maquiller.
- Contrôles ajoutés à § 3 : **Transition séparée de Vitesse** (Waves Tune RT : 15 / 120 ms par défaut), **Natural Vibrato
  0-200 %**, **gate de souffle avant détection** (Graillon), **Keep My Scale** et **groupes d'instances** (MetaTune), mode
  **référence** (fichier rendu par l'artiste avec MetaTune, aligné pour A/B aveugle).
- Chaîne intégrée dans l'ordre proposé par `05` : `AUTOTUNE → formant/gorge → drive doux → de-ess → air → [doubleur / harmonies /
  delay / réverb] → mix` (de-ess après drive et air).
- Profils cibles de `05` § 3 (« MetaTune hard-tune rap », « Melodic trap », « Satin naturel », « Classic hit », « Hyperpop métal »,
  « Chœur Messina », « Robot / talk-box ») = base des presets de § 4, à caler à l'oreille.
- Validation : MUSHRA ou ABX à loudness égale (BS.1770 ± 0,5 LU), corpus de 6-10 phrases de l'artiste, ancre basse = Orbe nu ;
  `05` rappelle (Gupta 2015) que les mesures objectives détectent des régressions mais **ne décident pas** du « son pro ».
