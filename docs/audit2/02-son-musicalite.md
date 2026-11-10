# Audit n° 2 — Son et musicalité (point de vue producteur / ingé son rap + pop électro alternative)

*9 nov. 2026. Lecture seule du code DSP, plus des bancs de mesure jetables (`/tmp/au2b/bench.cpp`, `/tmp/au2b/shelf.cpp`, compilés à part : les moteurs sont du C++ sans JUCE, donc le build du plugin n'était pas nécessaire). Rien n'a été écouté dans FL Studio. Les points déjà notés dans `docs/audit.md` ne sont pas repris, sauf quand la mesure les précise.*

## 0. Verdict

La suite est **saine techniquement** : suréchantillonnage à phase linéaire, ADAA, EQ « apparié » (Vicanek), limiteur true-peak, Wiener « dirigé par la décision ». Elle n'a pas encore de **signature sonore**. Trois briques communes, partagées par 10 plugins, sonnent « plugin gratuit » :
1. le **décaleur de hauteur à deux têtes** (`voile::Shifter`), utilisé par Voile, les 4 un-potard, Looks, Atelier, les doubles et le mode Qualité d'Autotune ;
2. la **reverb FDN à 4 lignes** (`voile::Reverb`), utilisée par Voile, les un-potard, Looks, Atelier, Cabine, Synth et Sampler ;
3. le **compresseur générique** (`onek::Comp`, crête, sans modèle), utilisé par les un-potard, Looks, Atelier, Cabine et Master.

Les remplacer, c'est relever **toute la suite d'un coup**. Le meilleur algorithme du dépôt est même déjà écrit et presque inutilisé : le `Splicer` du mode Direct d'Autotune.

## 1. Mesures

| Mesure (banc `/tmp/au2b`) | Résultat | Lecture |
|---|---|---|
| Voix synthétique 196 Hz (40 harmoniques, 2 formants) décalée : **modulation d'amplitude** (écart-type/moyenne de l'enveloppe sur 5 ms) | Source 0,016. **2 têtes : 0,141 (+0,14 dt), 0,181 (+1 dt), 0,196 (+3 dt).** Splicer : 0,010 / 0,027 / 0,088 | Le 2 têtes ajoute un **trémolo/phasing de ~18 %** : la voix accordée « flange » et paraît lointaine. Le Splicer est 5 à 14 fois plus propre |
| Même test : énergie hors harmoniques | +3 dt : **−33 dB (2 têtes)** contre −52 dB (Splicer) ; source −63 dB | À +3 dt (harmonies, hyperpop), le 2 têtes produit un voile granuleux nettement audible |
| Reverb FDN 4 lignes, réponse impulsionnelle | 1er écho à **22,6 ms** ; seulement **90 échantillons sur 2 400** au-dessus de −40 dB dans les 50 premières ms | Attaque creuse, puis écho clairsemé et « grenaillé » : sur voix et caisse claire, ça sonne métallique (flutter) |
| Compression parallèle des un-potard | Réduction **plafonnée à −8 dB**, quels que soient le seuil et le ratio | `l*(1-0,6·m) + l*g*0,6·m` : 40 % de signal non comprimé en permanence. Pas de voix « collée » façon rap moderne |
| 808, drive 0,55, C2 | Harmoniques/fondamentale = **−7,5 dB à 50 ms ET à 1,2 s** (alors que le niveau a chuté de 17,7 dB) | La saturation est appliquée **avant** l'enveloppe : la queue reste aussi « carrée » que l'attaque. Pas de « bloom » |
| Plateau aigu RBJ (un-potard, Looks, Cabine), +7 dB à 11 kHz, 44,1 kHz | 8 kHz : +1,1 dB (analogique +1,6) ; 16 kHz : **+6,7** (analogique +5,65) | « Air » trop pointu, concentré près de Nyquist (sifflant), pas assez de présence douce. L'EQ apparié d'Orbe EQ existe mais n'est pas réutilisé |

## 2. Par famille

### 2.1 Hauteur (Voile, un-potard, Looks/Atelier, Autotune)

**Algorithme.** YIN → note cible → lissage exponentiel (retune) → décaleur. La logique de correction est soignée (mode sûr, zone morte, tonalité Krumhansl avec hystérésis). Le **rendu** est le maillon faible : deux têtes de 32 ms, non synchrones à la période, font se croiser deux copies décalées de 16 ms à une phase quelconque. Résultat : un peigne qui balaie (mesuré) et des formants qui suivent la hauteur (« chipmunk »).

**Référence.** Pour la voix, le standard est le **TD-PSOLA** : grains de deux périodes fenêtrés en Hann, ré-espacés à la nouvelle période. Les formants sont préservés par construction, pour ~2 périodes de latence. Le son « T-Pain/Future » vient d'un décaleur propre et de transitions instantanées, pas du phasing.

**Écarts audibles et améliorations :**
- **Étape 1 (M)** : brancher le `Splicer` (déjà écrit, `AutotuneDsp.h`) à la place de `voile::Shifter` dans `Engine.h` et `OrbeChain.h`, avec la période donnée par YIN. AM ÷5, et 5 ms de latence au lieu de 16.
- **Étape 2 (L)** : vrai PSOLA. Repères de période = pic de chaque cycle (recherche à ±T0/2 de la prédiction) ; grains `x·hann(2T0)` posés tous les `T0/ratio` ; formant indépendant = rééchantillonnage du grain d'un facteur `F`. Le même moteur sert aux doubles, aux harmonies et à l'hyperpop (formants +2..+4 dt, `looks-voix.md` §6).
- **Détection** : Voile et la chaîne décimant par une moyenne de 2 échantillons sans passe-bas, il y a des erreurs d'octave sur voix soufflée ou en friture. Reprendre partout le `Detector` d'Autotune (passe-bas 3 kHz, médiane, rejet des sauts) (S).
- **Retune selon la durée de note** : rapide sur les notes courtes, ×3 plus lent sur les tenues, puis vibrato rendu (équivalent du « Humanize » d'Antares). Pas d'effet robot sur les tenues, attaque nette gardée (S).
- **Doubles de Voile à +12 / −5 dt fixes** : la quarte inférieure sort de la gamme sur le IV en majeur, et l'octave sans formants donne une voix de poupée. Reprendre l'harmonie diatonique d'Autotune (`harm`).

### 2.2 Dynamique (compresseurs, de-essers, Master)

**`onek::Comp`** : détection crête, lissage linéaire, ratio fixe, genou de 6 dB. Un VCA numérique sans caractère. En rap, on chaîne un **FET** (1176 : attaque 20–800 µs, rétroaction, distorsion qui croît avec la réduction) **puis un opto** (LA-2A : attaque ≈ 10 ms, relâche en deux temps, 60 ms puis 1 à 5 s).

Modèles proposés (M) :
- **FET** : détection sur la **sortie** ; lissage en dB `gr += (c−gr)·(c>gr ? α_att : α_rel)` ; distorsion `y += k·(gr_dB/20)·y²`, suréchantillonnée ×2 ; mode « all-buttons » (ratio 20, relâche courte).
- **Opto** : mémoire de cellule `dm/dt = (gr − m)/τ_charge` et relâche `τ_rel = 60 ms + (3 s − 60 ms)·m_norm`, qui dépend du programme et ne pompe pas.
- **Un-potard** (S, fort impact) : (1) comp et de-ess sont appliqués **après** reverb, delay et doubles (`OneKnob.h`, `Chain::process`) : les queues pompent et les doubles sont compressés avec la voix. Les placer avant l'espace, comme dans OrbeChain. (2) Supprimer le plafond de −8 dB.

**De-essers** (`onek::DeEss`, Orbe DeEsser) : seuil absolu. Les références (Pro-DS, Weiss) détectent en **relatif** : on déclenche si `E_sibilance(dB) − E_large-bande(dB) > seuil`. Le réglage tient à −30 comme à −8 dBFS (M).

**Master** : comp crête à relâche fixe de 200 ms. Il faut un « glue » à relâche auto (0,1/1,2 s), détection RMS et sidechain HP 90 Hz. Il manque surtout un **clipper doux ×4 avant le limiteur**, standard des masters rap forts : il prend les crêtes de caisse claire et d'808, et le limiteur respire (M).

### 2.3 EQ et saturation

- **EQ des chaînes** (un-potard, Looks/Atelier, Cabine, Master) : RBJ en float. Réutiliser `eq::` (apparié) partout (S). Pour Cabine (module « 550 »), ajouter le **Q proportionnel** de l'API : `Q = Q0·(1 + |G_dB|/6)` (S).
- **Saturations** (`tanh(kx+b) − tanh(b)`, suréchantillonnées) : propres mais **sans mémoire**, donc du fuzz numérique. Pour un caractère bande/lampe : (1) **pré-/dé-emphase** (+6 dB à 3 kHz avant, inverse après : les « s » s'arrondissent) ; (2) **biais dynamique** `b(t) = b0 + β·env(t)` (2e harmonique sur les attaques) ; (3) compensation de gain en RMS au lieu de `1/(1+1,2m)`.
- **Bitcrush** : à 6–8 bits, les fins de phrase disparaissent par paliers. Normaliser le niveau avant le crush (S).

### 2.4 Espace : reverb, delay, doubles

**Reverb** : 4 lignes de 23 à 35 ms, 2 passe-tout, Hadamard, sans modulation, amortissement à un pôle. Cible (M) : FDN **8 lignes** (29–97 ms, longueurs premières, Householder) ; **modulation** de ±8–16 échantillons par ligne (LFO 0,1–1 Hz décorrélés) ; **4 passe-tout d'entrée** (g = 0,75/0,75/0,625/0,625, Dattorro) ; **absorption à 2 bandes** dans la boucle, `g_i = 10^(−3·L_i/(RT60(f)·fs))` ; **premières réflexions** (6–12 prises de 3 à 25 ms, la « room 12–15 ms » de S2/S3) ; HP 250 Hz / LP 7 kHz sur l'envoi (Cabine le fait, pas Looks ni Voile) ; **ducking** par la voix. Trois couleurs : **plaque**, **chambre**, **pièce**.

**Delay** : ajouter dans la boucle une saturation douce et un léger pleurage (0,3 Hz, ±0,5 ms). Chaque répétition s'use comme sur bande et passe derrière la voix (S).

**Doubles** (Looks/Atelier, Cabine, Autotune) : ±0,14 dt à deux têtes, ou lignes modulées par sinus. Le balayage est **périodique** : on entend un flanger lent, pas une seconde prise. Modèle **ADT** (M), par voix : retard 12–35 ms + bruit filtré (0,5–2 Hz, ±3 ms) ; hauteur ±6–10 cents en bruit filtré indépendant ; EQ ou formant légèrement différents ; attaques décalées de 0 à 20 ms au hasard à chaque phrase.

### 2.5 Instruments

- **808** : moteur propre (ADAA + ×8). Défaut : **saturation avant l'enveloppe** (mesuré). Appliquer `shape(osc·env·k)/shape(peak·k)` : attaque grasse, queue qui redevient sinus. C'est le « bloom » trap. Ajouter un passe-bas qui suit l'enveloppe (S).
- **Synth** : biquad 12 dB linéaire. Pour la pop électro alternative, passer à un SVF/échelle **ZDF (TPT)** avec `tanh` dans la rétroaction, en 12 et 24 dB (M).
- **Beat** : la saturation du kit agit sur la **somme** des pistes : le kick module les charlestons (intermodulation). Saturer par piste (S).

## 3. Chaîne vocale rap/pop : ce qui manque pour un rendu pro 2026

1. **Accordage propre** (PSOLA) et **formants** hors d'Autotune.
2. **FET → opto en série**, précédés d'un « rider » rapide (50–200 ms).
3. **Air dynamique / exciter** : plateau 10–20 kHz réduit par le détecteur de sibilance, plus une branche parallèle HP 4 kHz → saturation → 5–15 % (S10).
4. **EQ dynamique** sur 200–500 Hz (boue) et 2,5–5 kHz (dureté).
5. **Doubles ADT** et **harmonies diatoniques** à formants préservés.
6. **Échos de fin de phrase automatiques** : à chaque fin de phrase (voix → silence > 150 ms), les 250 dernières ms partent dans le delay. C'est le « delay throw » que les ingénieurs automatisent à la main ; **aucun plugin grand public ne le fait seul**.
7. **Pré-verb / reverb inversée** (drill), en option avec 500 ms de latence.
8. **Textures** : stutter et gate synchronisés, vocal chop, « formant flip » sur les ad-libs.

## 4. Générateur Orbe

**Acquis depuis l'audit 1** : deux accords par mesure, voicings serré / drop 2 / large / shell, glissés 808, batterie A/A'/A/fill, annuler/rétablir, verrous de piste.

- **Progressions** : banques diatoniques correctes (trap i–VI–III–VII, drill en mineur harmonique). Il manque les couleurs du genre : **♭II phrygien** (trap sombre, drill), **iv en majeur** et **♭VI–♭VII–I** (pop alternative), **IV lydien**, **pédale de tonique à l'808** et **boucles de 2 accords** (trap, rage). La relance d'accord reste uniforme sur 7 degrés : il faut une pondération par style.
- **Voicings** : bonne logique de proximité. Ajouter la doublure à l'octave de la note de tête, la quinte à vide sous les pads et les clusters add9 dans l'aigu pour les cloches. Ajouter aussi un étalement de 5 à 20 ms (grave → aigu) et des vélocités par note sur les « keys ».
- **808** : registre juste (G1–F#2). Il manque **le saut d'octave glissé** en fin de mesure (la figure de la trap), l'**accroche au kick** et des notes coupées avant la caisse claire.
- **Batterie** : les rolls de charleston sont **toujours aux mêmes positions** (12 / 12,667 / 13,333). Il faut un générateur de rolls : ratchets 1/32, 1/24 et 1/48, **rampes de vélocité et de hauteur** (le pitch roll est une signature trap), position et densité tirées. Ajouter un **micro-timing par instrument** (caisse claire en retard de 8–15 ms en boom-bap/R&B, charleston swingué à 54–58 %) au lieu d'un swing global.
- **Mélodie** : marche aléatoire AA'AB'. Bien pour une cloche, pas encore une **topline**.

**Angles différenciants** :
1. **Accordage guidé par l'accord** : Orbe transmet l'accord courant à Autotune, Looks et Voile. Sur les temps forts, la correction vise les notes de l'accord, pas toute la gamme. Seul le mode MIDI d'Auto-Tune le permet, et à la main.
2. **Mode flow** : grille de cadence rap (syllabes en doubles-croches et triolets, « triplet flow », « drill bounce ») exportée en MIDI guide. C'est une aide à l'écriture inédite.
3. **808 et kick générés ensemble**, avec le bloom (2.5).
4. **Graine partagée** : une même idée pilote accords, Beat, 808 et look de voix dans la même tonalité.

## 5. Les 10 améliorations d'algorithme à plus fort impact sonore

Effort : S ≤ 1 jour, M = 2 à 4 jours, L = 1 à 2 semaines.

| # | Amélioration | Plugins touchés | Gain audible attendu | Effort |
|---|---|---|---|---|
| 1 | Remplacer `voile::Shifter` par le `Splicer` existant, piloté par la période YIN, et utiliser le `Detector` d'Autotune partout | Voile, 4 un-potard, Looks, Atelier, Autotune Qualité | Voix accordée sans phasing (AM 0,18 → ~0,03), plus présente ; latence ÷3 | M |
| 2 | Nouvelle reverb : FDN 8 lignes modulées, diffusion Dattorro, absorption à 2 bandes, premières réflexions, filtres et ducking sur l'envoi ; 3 algorithmes (plaque, chambre, pièce) | 7 plugins | Fin du son métallique et grenaillé ; voix « chère » | M |
| 3 | Un-potard : EQ/comp/de-ess **avant** l'espace, plafond de −8 dB supprimé | 4 un-potard | Voix rap collée et dense, reverb qui ne pompe plus | S |
| 4 | TD-PSOLA avec formants indépendants (puis doubles et harmonies dessus) | Toute la famille hauteur | Fin du chipmunk, hyperpop crédible, harmonies naturelles | L |
| 5 | Compresseurs modélisés **FET** (rétroaction, distorsion selon la réduction) et **opto** (relâche programme-dépendante), chaînables | Looks, Atelier, Cabine, un-potard | « Grain » de console et voix qui tient devant le beat sans pomper | M |
| 6 | Doubles **ADT** (retard et hauteur en bruit filtré, attaques désynchronisées, formant ou EQ différents) | Looks, Atelier, Cabine, Autotune, Voile | Vraie double prise au lieu d'un flanger lent | M |
| 7 | 808 : saturation **après** l'enveloppe, drive et filtre suiveurs | 808 (et le son de basse d'aperçu d'Orbe) | « Bloom » trap : attaque grasse, queue propre | S |
| 8 | De-essers à détection **relative**, air dynamique et exciter parallèle | DeEsser, chaînes, un-potard, Cabine | Aigus brillants sans sifflement, réglage indépendant du niveau | M |
| 9 | Orbe : rolls de charleston générés (ratchets, rampes de vélocité et de hauteur), micro-timing par instrument, sauts d'octave et pédale à l'808, ♭II / iv / ♭VI–♭VII et boucles de 2 accords | Orbe, Beat | Grooves qui sonnent 2026 et non « boîte à rythmes de démo » | M |
| 10 | Saturation à pré-/dé-emphase et biais dynamique, plus clipper doux ×4 avant le limiteur du Master | Chaînes, Voile, Master | Caractère bande/lampe ; masters forts qui respirent | M |

Ordre conseillé : **3 → 7 → 1** (petits efforts, gains immédiats), puis **2 et 5** (les briques partagées), puis **4, 6, 8**. Les angles différenciants (accordage guidé par l'accord, échos de fin de phrase, mode flow) viennent ensuite. Ce sont eux qui donneront la sensation que « ça n'existe pas ailleurs ».
