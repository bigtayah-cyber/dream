# Orbe Autotune — moteur « Orbe »

*9 nov. 2026. Code : `Source/Suite/OrbeAutotune/TuneCore.h` (moteur, sans JUCE), `NoteEdit.h` (édition note par note),
intégration dans `AutotuneDsp.h` (`Engine::processOrbe`), `AutotuneProcessor.*`, `AutotuneEditor.*`.
Mesures : `tests/tune_core_test.cpp` (cible `tune_core_test`). Rien n'a encore été écouté dans FL Studio.*

## 1. Schéma des blocs

```
 entrée L/R ──┬───────────────────────────────────────────────────────────────► sec retardé de L (exact)
              │                                                                         │
              │  mid = (L+R)/2                                                          │
              ├──► [passe-bas 2×biquad] ─► ↓4 (↓8 à 88/96 k) ─► YIN (CMNDF) ─► candidats (≤ 5, retards croissants)
              │                                                    │                    │
              │                     énergie HF / énergie (consonnes, souffle)           │
              │                                                    ▼                    │
              │                            Viterbi en ligne (candidats + état « non voisé »)
              │                            décision retardée de LF trames (Qualité : 1 à 3)
              │                                                    │
              │                            affinage NSDF plein débit (interpolation parabolique)
              │                                                    ▼
              │                            courbe de correction (§3) ─► décalage s(t) en demi-tons
              │                                                    │        (+ édition de note, §5)
              │                                                    ▼
              ├──► préaccentuation ─► treillis LPC (blanchiment) ─► résidu ─► lecteur à raccords synchrones
              │        ▲ ordre fs/700+4, fenêtre 23 ms, chaque trame          (retard d ∈ [L−T0/2, L+T0/2])
              │        └────────── coefficients K(t) mémorisés ────────────────────────┐
              │                                                                         ▼
              │                                             treillis LPC inverse avec K(instant lu) ─► désaccentuation
              │                                                                         │
              └──────────────────────────── voisé ? ── fondu 4 ms ── sortie = sec + a·(traité − sec)
                                             (non voisé, ou décalage < 0,5 ct : sec exact)
```

Après TuneCore, dans `atn::Engine` : étage de formants STFT (mode Qualité seulement, pour le potard « Formants »),
harmonie, doublage, mix. Les latences déclarées sont **exactement celles de l'ancien moteur** (Direct : min(256, 5 ms) ;
Qualité : anticipation win/2 + STFT 1023), donc la bascule Classique ↔ Orbe ne change ni la latence ni la compensation.

## 2. Détection de hauteur — pourquoi ces choix

- **YIN décimé (×4, ≈ 11 à 12 kHz)** : la CMNDF est robuste sur la voix et peu coûteuse une fois décimée. Plancher 62 Hz
  (rap grave), plafond ≈ 1100 Hz.
- **Candidats pris dans l'ordre des retards croissants**, pas les « meilleurs » : sur un signal périodique, la CMNDF est
  souvent *plus basse* à 2T, 3T… qu'à T. Garder les meilleurs valeurs faisait perdre la bonne octave (bug trouvé en test).
- **Pénalité de sous-multiple** : un candidat dont la période est un multiple entier (≥ 2) d'un autre candidat presque aussi
  bon est pénalisé (erreur d'octave basse classique).
- **Viterbi léger** (esprit pYIN) : états = candidats + « non voisé » ; coût local = CMNDF + (1 − voisement) ; transition =
  0,035 par demi-ton (+0,3 au-delà de 9 dt), bascule voisé/non voisé 0,22. En Qualité, la décision est **retardée** de 1 à 3
  trames (on connaît la suite avant de trancher) ; en Direct, décision immédiate (pas d'anticipation possible).
- **Voisement** = périodicité × (énergie HF / énergie faible) × porte d'énergie (−72 dBFS et −42 dB sous la crête récente).
  Le souffle (apériodique) et les consonnes (HF) sont classés non voisés. En Direct, un détecteur rapide (3 ms) de
  consonnes coupe le traitement sans attendre l'analyse.
- **Affinage plein débit** (NSDF de McLeod autour du retard décimé) : précision < 0,5 ct sur une voix tenue.
- **Alignement** : l'estimation décrit l'instant « fin de trame − fenêtre/2 − période/2 ». En Qualité, la trame utilisée est
  choisie pour tomber sur l'échantillon qui sort ; l'écart restant (et tout l'écart en Direct, ≈ 8 ms) est compensé par
  **extrapolation linéaire** de la hauteur. Sans cela, le mode robot laissait passer 30 % du vibrato en Direct.

## 3. Courbe de correction (l'algo « à nous »)

La hauteur p est séparée en **centre** et **expression** :
- `c1` : passe-bas 30 ms (décision de note, glissandos) ; `c` : deux pôles de 90 ms (centre sans vibrato : un vibrato de
  5,5 Hz y est atténué à 8 %). Quand `c1` s'écarte de plus de 0,7 dt de `c`, ou quand la note visée change, `c` repart de
  `c1` (changement de note, pas de retard de 200 ms).
- **Note visée N** : gamme/tonalité (menus existants, notes inversées comprises) avec hystérésis de 0,2 dt ; note forcée
  (`Params::forcedNote`) ; ou note éditée (§5, prioritaire).
- **Cible** : `s* = Quantité × [ zone_morte(N − c) − (1 − h)·(p − c) ]`
  - `h` = Expression × saturation((Vitesse − 5 ms) / 45 ms). Vitesse 0 → h = 0 → `s* = N − p` : **robot**, tout est aplati.
    Vitesse ≥ 50 ms → h = Expression : seul le **centre** est ramené sur la note, le vibrato et les inflexions passent.
  - la zone morte (Tolérance) agit sur le centre, jamais sur le vibrato.
- **Glissando volontaire** : pente de `c1` de même signe (> 2,5 dt/s) pendant plus de 80 ms (un vibrato change de sens
  avant) → la correction s'efface (× (1 − h)) et se retire en 25 ms. Jamais en robot ni sur une note éditée.
- **Vitesse** : `s` suit `s*` par un filtre d'ordre 1 de constante Vitesse ; début de phrase : `s` part de 0 (l'attaque
  naturelle passe, puis la voix se pose). Non voisé : `s` est tenu 60 ms (consonne au milieu d'un mot), puis revient à 0.

## 4. Resynthèse

- **Excitation-PSOLA** (choix original) : le signal est blanchi par un **treillis LPC** (préaccentuation 0,9, ordre fs/700+4
  = 67 à 44,1 kHz, fenêtre de Hann 23 ms, fenêtre de retard gaussienne de largeur 0,35·f0 pour ne pas modéliser les
  harmoniques, autocorrélation lissée ≈ 4 ms). Le **résidu** (impulsions glottiques, spectre plat) est décalé par un lecteur à
  retard variable qui saute d'un nombre entier de périodes (raccords synchrones, alignés par corrélation sur les échantillons
  que le fondu va lire). Puis le treillis inverse **recolore** avec l'enveloppe d'origine : les formants ne bougent pas.
- **Détail décisif** : la recoloration utilise l'enveloppe de l'instant où le résidu lu a été blanchi (et non l'instant de
  sortie). Sans cela, la petite fluctuation de l'enveloppe d'une trame à l'autre devenait un bruit de modulation
  (THD+N voix 110 Hz : −47 dB → −84 dB, mesuré).
- Treillis : stables tant que |k| < 1 (k bornés à 0,995), coefficients interpolés échantillon par échantillon.
- **Non voisé** : sortie = sec retardé de L **exactement** (aucun traitement, aucun pré-écho : tout est dans le domaine
  temporel, sans bloc FFT dans le chemin principal). Fondu de 4 ms à l'entrée/sortie du traitement.
- Formants « libres » (case décochée) : le lecteur lit le signal brut au lieu du résidu (le timbre suit la hauteur).
- Potard « Formants » (déplacement volontaire) : étage STFT existant, mode Qualité seulement (comme avant).

## 5. Édition note par note (type Melodyne)

**Approche retenue : correction pilotée par la timeline, en temps réel, sans latence supplémentaire.**
1. ÉDITION → ARMER, puis lancer la lecture : le plugin capture la hauteur détectée (une trame toutes les 2,9 ms, jusqu'à
   ≈ 90 s) avec la **position de timeline de l'hôte**. La capture s'arrête au retour de boucle ou par un second clic.
2. La courbe est nettoyée (médiane de 5) et découpée en notes (saut ≥ 0,75 dt tenu 4 trames, notes < 40 ms fusionnées).
3. Piano-roll : glisser une note (demi-ton ; Maj = au cent), flèches ±1 dt (Maj : ±10 ct), double-clic = caler sur la note,
   Alt-clic = couper, M = fusionner avec la suivante, bords = redimensionner, Suppr = note à zéro, Ctrl+Z / Ctrl+Maj+Z,
   réglages par note : Force, Vibrato (expression gardée), Vitesse.
4. À la relecture, à chaque trame, le moteur cherche la note éditée qui couvre la position de timeline de l'audio analysé et
   impose sa cible (centre détecté + décalage) à la place de la gamme. La hauteur reste mesurée en direct : la note éditée
   se pose sur la voix réellement jouée, vibrato compris selon le réglage.
5. Les notes éditées et la courbe sont **sauvegardées dans l'état du projet** (nœud `NOTEEDIT`, binaire compact en base 64 :
   ≈ 40 octets par note, 2 octets par trame de courbe).

**Pourquoi pas un rendu hors-ligne ni une latence « Édition » de plusieurs secondes** : FL Studio compense mal les très grandes
latences et un rendu impose de figer la piste. La voie retenue ne change pas la latence, marche en lecture normale et en
export. **Contraintes honnêtes** : il faut rejouer **le même audio à la même position** (si la prise est déplacée sur la
timeline, l'édition ne suit pas : refaire la capture ; une nouvelle capture du même passage reprend les éditions qui se
recouvrent) ; une note éditée ne commence à être corrigée qu'au début de la trame qui la couvre (≤ 3 ms) puis à la vitesse
choisie ; si l'interface publie une modification pendant un bloc audio, ce bloc joue sans éditions (verrou non bloquant).
L'édition n'agit que sur le moteur Orbe.

## 6. Écriture des notes (# / b)

Paramètre « Altérations » (dièses / bémols). Quand la tonalité est connue (manuelle ou Auto verrouillée), l'armure décide :
tonique majeure relative F, Bb, Eb, Ab, Db → bémols ; G, D, A, E, B → dièses ; C, F#/Gb et la gamme chromatique →
préférence de l'utilisateur. Appliqué au menu Tonalité, au clavier, à l'écran, à l'afficheur et au piano-roll. Notes toujours
par lettres. Aucun effet sur le son.

## 7. Paramètres et compatibilité

Nouveaux paramètres **ajoutés en fin**, IDs existants intacts, pas de booléen : `algo` (Classique / Orbe), `expr`
(Expression), `accid` (# / b). Un instance neuve démarre en Orbe ; **un projet sauvegardé avant (sans `algo`) est rechargé
en Classique** : son identique (vérifié par `orbeautotune_snapshot etat`).

Corrigé au passage : **B1** (fantôme d'harmonie/doublage à la reprise : −30 dB → inaudible, les mémoires sont vidées au
réveil, sans allocation) et **G4** (paramètres lus par pointeurs préchargés, tampon de travail de 8192).

## 8. Mesures (44,1 kHz ; « ancien » = moteur Classique, après correction de B1)

| Mesure | ancien Qualité | ancien Direct | Orbe Qualité | Orbe Direct |
|---|---|---|---|---|
| Latence déclarée = impulsion | 1729 = 1729 | 221 = 221 | 1729 = 1729 | 221 = 221 |
| Retard moyen de la voix corrigée (enveloppe) | 1650 | 152 | 1747 | 223 |
| Erreur après correction +30 ct (4 notes, robot et 20 ms) | ≤ 0,3 ct | ≤ 0,4 ct | ≤ 0,2 ct | ≤ 0,2 ct |
| Vibrato 5,5 Hz ±40 ct, naturel 120 ms : centre / profondeur | +27 ct / ×0,98 | +27 ct / ×0,94 | **−0,3 ct / ×1,02** | **−0,3 ct / ×1,02** |
| Même vibrato, robot 0 ms : profondeur restante | ×1,60 (sauts de note) | ×1,38 | **×0,13** | **×0,16** |
| Glissando 3 dt / 400 ms, naturel : écart à l'entrée | 4,9 ct | 5,6 ct | **0,6 ct** | **0,5 ct** |
| Enveloppe (formants) à +4 dt, préservés | 14,4 dB | 7,1 dB (aucune préservation) | **2,3 dB** | **2,3 dB** |
| Enveloppe à −4 dt, préservés | 7,0 dB | 7,2 dB | **1,8 dB** | **2,1 dB** |
| THD+N sinus 440 / voix 110 / 196 / 330 Hz (dB) | −76 / −64 / −63 / −62 | −89 / **−98** / **−89** / −73 | −97 / −84 / −75 / −72 | **−108** / −85 / −83 / **−79** |
| « s » et souffle : écart à l'entrée retardée | 0 dB (traités) | 0 dB | **−143 dB (intacts)** | **−300 dB** |
| Prise réelle, détection : voisées manquées / erreurs d'octave | 8 / 0 | | 0 / 0 | |
| Prise réelle, robot : trames à ≤ 10 ct de la grille | 75 % | 28 % | **86 %** | 51 % |
| Allocations pendant le traitement / NaN sur entrée hostile | | | 0 / aucun | 0 / aucun |

44,1 / 48 / 88,2 / 96 kHz : erreur ≤ 0,1 ct, Direct ≤ 256 échantillons. `reset()` rend un moteur identique à un neuf.
pluginval niveau 10 (Linux) : SUCCESS. Formants : enveloppe test à bosses de 300 à 450 Hz de large (une enveloppe plus fine
que l'écart entre harmoniques n'est observable par aucune méthode).

## 9. Limites connues

- **THD+N en Direct** : l'ancien mode Direct (raccords sur le signal brut) reste plus propre sur les voix graves (−98 contre
  −85 dB à 110 Hz). C'est le prix de la préservation des formants (le résidu blanchi est plus sensible à l'interpolation).
  Les deux restent sous −70 dB sauf la voix aiguë.
- **Direct** : sans anticipation, l'analyse a ≈ 8 ms de retard (compensé par extrapolation, imparfait sur les attaques et
  les sauts rapides) ; 1 à 2 trames d'erreur d'octave possibles exactement au changement de note (sans effet audible sur la
  correction, la cible est calculée dans la même octave ; visibles sur la courbe brute, filtrées à la capture).
- Le lecteur de raccords oscille par construction entre L − T0/2 et L + T0/2 (moyenne L) : sur une voix grave en Direct, la
  fenêtre est calée contre le minimum (24 échantillons), le retard moyen dépasse alors un peu L.
- LPC : moins juste sur les voix très aiguës (f0 > 600 Hz) et sur les grandes transpositions (> 7 dt, l'impulsion glottique
  rééchantillonnée change un peu la pente du spectre).
- La détection plafonne à ≈ 1100 Hz (sifflements, voix de tête très aiguës non corrigés).
- Note forcée par MIDI : prévue dans le moteur (`Params::forcedNote`), **pas branchée** : le plugin ne déclare pas d'entrée
  MIDI (l'ajouter changerait la configuration de bus des projets existants). L'édition note par note couvre le besoin.
- Pas d'export MIDI des notes, pas de « sélection multiple » ni de glisser du temps des notes dans le piano-roll.

## 10. À écouter à l'oreille (FL Studio)

1. Robot (vitesse 0) sur une prise rap chantée : grain des raccords, surtout en Direct, et sur les « r » roulés.
2. Naturel (80 à 150 ms, Expression 100 %) : vibrato gardé, attaques (le « scoop » doit rester), fins de phrase.
3. Consonnes et souffle : aucune coloration, aucun « tchk » à l'entrée et la sortie du traitement (fondu 4 ms).
4. Formants : transposer ±4 et ±7 dt, comparer Orbe / Classique (effet « chipmunk » attendu seulement en Classique Direct).
5. Voix très grave (< 90 Hz) et très aiguë (> 500 Hz) en Direct : stabilité.
6. Édition : capturer 30 s en boucle, déplacer 3 notes, relancer, exporter ; recharger le projet et vérifier que les notes
   reviennent.
7. Harmonie et doublage à la reprise d'une phrase après un souffle (B1) : plus de « flam ».
8. Charge CPU de plusieurs instances à 96 kHz (LPC d'ordre 80 en stéréo).
