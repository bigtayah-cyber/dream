# Audit 4, volet 04 : algorithmes de hauteur et de formants (Orbe Autotune)

*10 nov. 2026. Recherche web et analyse, sans aucune modification du code. Les coûts CPU sont **estimés par comptage
d'opérations, pas mesurés** (aucun banc compilé). Les références [n] renvoient à la bibliographie en fin de document.*

---

## Synthèse (≤ 1200 mots)

**1. Notre moteur est déjà, sur le fond, celui d'Auto-Tune « Classic ».** Le brevet Antares (US 5 973 252, déposé le
14/10/1998, donc tombé vers octobre 2018 [1]) décrit trois choses : une autocorrélation décimée ×8 calculée par récurrence ;
un contrôle à L et 2L à plein débit contre les erreurs d'octave ; un « convertisseur de débit » qui **répète ou supprime des
cycles entiers** quand le pointeur de lecture sort de sa zone. En 2016, Hildebrand l'a confirmé : il n'utilise pas les
grains de Lent (« trop imprécis »), seulement un suivi continu très précis et un rééchantillonneur qui répète ou saute des
cycles [2]. C'est notre `Splicer`. Le « mid » ne vient donc probablement **pas du principe** de décalage. Il vient de ce
qu'on a ajouté autour, ou de la manière dont c'est fait.

**2. Trois suspects algorithmiques pour le son « tubulaire / métallique ».** Tous trois sont à confirmer à l'écoute et
à la mesure ; l'agent « son » tranchera.
- **(a) Blanchiment et recoloration LPC même pour les petites corrections.** Auto-Tune 5 et Classic n'ont *aucune*
  correction de formants : le signal brut est rééchantillonné, et sur ±1 demi-ton le décalage de formants (≈ 6 %) ne
  s'entend pas. La correction de formants n'est arrivée qu'avec Evo, et le mode Classic la désactive [3][4]. Notre chaîne
  préaccentuation → treillis d'ordre 67 → résidu → treillis inverse ajoute à chaque instant une erreur de modélisation
  (enveloppe lissée par la fenêtre gaussienne, interpolation des coefficients, plancher de −40 dB). Elle colore le son
  même quand la correction est de 30 cents. **Piste :** formants « libres » (lecture du signal brut, comme Classic) sous
  ~2 à 3 demi-tons de décalage, avec un fondu vers le chemin LPC au-delà.
- **(b) Répétition du bruit.** Répéter un cycle répète aussi le souffle et le bruit de friction qu'il contient. Un bruit
  répété périodiquement devient un peigne, ce qui donne un timbre « métallique », bourdonnant. C'est le défaut connu de
  PSOLA sur les voix soufflées. Le modèle harmonique + bruit (HNM) de Stylianou existe précisément pour le corriger [5].
  Les voix rap ou soufflées sont les plus touchées. **Piste :** séparer le résidu en partie périodique (décalée) et partie
  apériodique (non répétée).
- **(c) Raccords ponctuels au lieu d'un recouvrement période par période.** Un lecteur unique fait un raccord fondu toutes
  les T0/|1−r| périodes. TD-PSOLA [6] refait chaque période avec deux grains de Hann qui se recouvrent, ce qui étale
  l'erreur au lieu de la concentrer. Quand la hauteur bouge vite (vibrato, rap), un raccord « au mauvais moment » donne un
  petit saut d'enveloppe audible.

**3. Détection de hauteur : garder notre base et la renforcer, sans réseau de neurones dans le temps réel.**
- YIN + affinage NSDF (MPM) + Viterbi en ligne correspond déjà à l'état de l'art classique. Le gain le moins cher vient de
  **pYIN** [7] : 100 seuils sous un a priori bêta (moyennes 0,10 / 0,15 / 0,20) donnent des probabilités de candidats et
  une probabilité de non-voisement. Le surcoût est faible, et les erreurs d'octave tombent à 0,5 à 1,7 %. Le Viterbi
  hors ligne doit être remplacé par un Viterbi à **décalage fixe** (1 à 3 trames, ce que fait déjà le mode Qualité).
- On peut aussi ajouter un **sélecteur « type de voix »** (plage f0 : grave, ténor, soprano), comme l'« Input Type »
  d'Auto-Tune [4]. C'est le plus efficace contre les octaves sur le rap grave.
- **Réseaux de neurones :** SPICE (180 k paramètres, trame de 32 ms) [8], CREPE (MIT pour le code, 487 k paramètres en
  tiny, 22 M en full) [8][9], PESTO (≈ 130 k paramètres, < 10 ms annoncés, mais code **LGPL-3**, donc exclu par notre
  règle) [10][11], FCPE (10,6 M paramètres, MIT) [12][13], SwiftF0 (95 k paramètres, MIT, **176 ms d'anticipation** en
  flux) [14][15], RMVPE (licence non indiquée) [16]. Aucun ne tient un budget de 15 ms avec une licence propre et un coût
  CPU négligeable. **À réserver** à l'analyse hors ligne de l'éditeur de notes (SwiftF0 en ONNX, sous MIT, est le candidat
  le plus simple).

**4. Décalage de hauteur avec une latence ≤ 15 ms : le temporel gagne.** Pour séparer les harmoniques d'une voix à 80 Hz,
une TFCT a besoin de N ≥ 2048 à 48 kHz, soit ≈ 43 ms de fenêtre. À N = 512 (≈ 11 ms), un bin fait 94 Hz : les harmoniques
graves se confondent et le vocodeur de phase « bave » (phasiness). Le vocodeur à verrouillage de phase identité de
Laroche et Dolson [17] (brevet Creative US 6 549 884, expiré en 2019 [18]), la variante PGHI [19], Signalsmith Stretch
(MIT, préréglages autour de 100 à 120 ms de bloc) [20][21] et l'enveloppe « true envelope » de Röbel et Rodet [22] sont
donc **excellents, mais pour le mode Qualité (≥ 40 ms) ou le hors ligne**, pas pour Direct. WORLD (BSD modifiée, sans
brevet [23]) est l'outil idéal pour l'édition hors ligne et le changement de genre. Il n'est pas temps réel avec Harvest.

**5. Ce que font les produits commerciaux** (vérifié / déduit / inconnu) :
- **Auto-Tune Classic** : autocorrélation et répétition de cycles, vérifié [1][2].
- **Evo / Pro** : correction de formants et « Throat Length » (longueur du conduit vocal modélisé) [3][4] ; méthode non
  publiée.
- **Waves Tune RT** : latence de **0 à 4 ms selon la note** [24], typique d'un lecteur à cycles (déduction).
- **MetaTune** (≈ 3 ms) [25][26], **Graillon 3** (moteurs G2 / G3 / I1) [27], **Little AlterBoy** [28] : algorithmes non
  publiés.
- **Melodyne** : détection temporelle et « instantanés » périodiques de type table d'onde [29], soit une resynthèse
  synchrone à la période.

**6. Recommandation (trois architectures, pseudo-code au §6).**
- **A — « PSOLA-résidu HN » (priorité, mode Direct, ≤ 15 ms).** TD-PSOLA à marques glottiques sur le résidu LPC, avec
  séparation harmonique / bruit du résidu (le bruit n'est jamais répété). Les formants passent en libre sous 2 à 3 demi-tons
  et sont préservés au-delà. Le décalage de formants et la « gorge » se font par rééchantillonnage des grains ou par
  déformation de l'enveloppe. Coût estimé ≈ 1,5 à 2,5 k opérations par échantillon (≈ 2 à 4 % d'un cœur à 48 kHz).
  Latence : max(5 ms, ≈ 1 période), donc ≤ 16 ms jusqu'à 62 Hz.
- **B — Vocodeur à peaks verrouillés + true envelope (mode Qualité, 40 à 50 ms).** Pour les grands décalages, le
  changement de genre et les harmonies. On peut d'abord tester Signalsmith Stretch (MIT) configuré en petit bloc avant
  d'écrire le nôtre.
- **C — Analyse / synthèse de type WORLD (éditeur de notes hors ligne).** f0 par pYIN ou SwiftF0, enveloppe CheapTrick,
  apériodicité D4C. Hauteur, formants et souffle deviennent indépendants.

**Tests de validation chiffrés** (détail au §7) :
- erreur de hauteur RMS < 1 ct sur voyelle synthétique tenue ;
- erreurs grossières (> 50 ct) < 1 % sur MIR-1K ;
- octaves < 0,5 % ;
- THD+N inter-harmonique ≤ −60 dB à ±3 demi-tons ;
- distance d'enveloppe (LSD 0 à 5 kHz) < 1,5 dB à ±7 demi-tons ;
- erreur F1 à F3 < 3 % ;
- jitter de sortie en mode robot < 0,2 % ;
- « répétition de bruit » (autocorrélation résiduelle à la période, sur bruit pur) < 0,15.

**Licences :** exclus Rubber Band (GPL) [30], SoundTouch (LGPL), PESTO (LGPL-3). Utilisables : Signalsmith (MIT), WORLD
(BSD), code CREPE / FCPE / SwiftF0 (MIT ; licence des **poids** à confirmer). Brevets Antares et Laroche-Dolson expirés.

---

## 1. Suivi de hauteur temps réel

| Méthode | Principe | Latence minimale (f_min = 62 Hz) | Robustesse voix | Licence / statut |
|---|---|---|---|---|
| YIN [31] | CMNDF + seuil absolu + parabole | fenêtre ≥ 2·τmax ≈ 32 ms, estimation centrée à −16 ms | bonne ; erreurs d'octave basses | idée publiée |
| MPM / NSDF [32] | maxima clés de la NSDF, premier ≥ k·max | même ordre ; NSDF bien normalisée sur les fenêtres courtes | moins d'octaves que YIN sur la voix | idée publiée |
| pYIN [7] | YIN sur une distribution de seuils + HMM (480 bins de 10 ct × voisé/non voisé) | YIN + retard du Viterbi (fixed-lag) | rappel 0,98 ; octaves 0,5 à 1,7 % ; voisement ≈ 92 à 95 % | idée publiée (Vamp : GPL, non vérifié) |
| SWIPE' [33] | correspondance avec un spectre « en dents de scie » sur plusieurs fenêtres | plusieurs fenêtres, la plus longue pour le grave : lent | très robuste aux erreurs d'octave | idée publiée ; coût élevé |
| SPICE [8] | CQT + encodeur auto-supervisé | trame CQT de 32 ms | RPA MIR-1K ≈ 90 % | TF Hub, licence non vérifiée |
| CREPE tiny / full [9][8] | CNN sur 1024 échantillons à 16 kHz (64 ms) | ≥ 32 ms | RPA ≈ 90 à 98 % ; mauvais au bruit fort | code MIT ; poids non précisés |
| PESTO [10] | VQT en flux + CNN équivariant, ≈ 130 k paramètres | < 10 ms annoncés | proche de CREPE | **LGPL-3, exclu** [11] |
| FCPE [12] | mel + transformeur, 10,6 M paramètres | non publiée | RPA MIR-1K 96,8 % | MIT [13] |
| SwiftF0 [14] | STFT à 16 kHz, 95 k paramètres (14 k dans le paquet) | **176 ms** d'anticipation en flux | 94 % propre, 91,8 % à 10 dB | MIT (paquet) [15] |

**Points clés pour notre cas.**
- **Doublements d'octave.** L'erreur dominante de YIN, c'est le sous-multiple : la période 2T passe sous le seuil. Notre
  pénalité de sous-multiple et le parcours des retards croissants vont dans le bon sens. Le brevet Antares ajoute un test à
  L et 2L sur les données à plein débit [1] ; c'est bon marché, à garder. Une **plage f0 par type de voix** [4] retire
  d'emblée la moitié des confusions.
- **Voisé / non voisé.** pYIN obtient le non-voisement comme la masse de probabilité restante [7]. C'est plus propre que
  notre produit périodicité × HF × énergie, car c'est calibré par construction. On peut combiner les deux : la probabilité
  pYIN devient l'observation, et notre ratio HF reste un indice de consonne.
- **Rap, chuchotement, souffle.** Il n'y a pas de f0 fiable. Le bon comportement est de **ne pas corriger** : sec exact,
  ce que fait déjà TuneCore. Le paramètre « Tracking » d'Auto-Tune règle justement la tolérance d'apériodicité :
  l'augmenter pour une voix soufflée, le baisser en cas de clics [4]. Exposer un équivalent (le seuil de voisement) est
  utile.
- **Latence contre précision.** La précision < 0,5 ct vient de l'affinage parabolique ; la latence vient de la fenêtre.
  Notre extrapolation linéaire de la hauteur sur ≈ 8 ms en Direct est la bonne réponse. Waves Tune RT a une latence de
  0 à 4 ms « selon la note » [24], ce qui suggère un retard qui dépend de la période en cours. Un **retard au repos
  adaptatif** est une piste, mais il impose une latence variable que l'hôte ne compense pas.

## 2. Transposition de haute qualité

**TD-PSOLA** [6] : marques synchrones (instants de fermeture glottique, GCI), grains de Hann de 2T, replacés tous les T/r.
Les formants sont conservés naturellement pour des décalages modérés.
- Le placement des marques est critique. La revue de Drugman et al. [34] compare SEDREAMS, DYPSA, YAGA et d'autres ;
  SEDREAMS (signal « mean-based » + pics du résidu LPC) est simple et robuste.
- Sur le **résidu LPC**, les pics sont nets, ce qui facilite les marques. C'est notre cas : on a déjà le résidu
  gratuitement.

**Lecteur à cycles (Lent 1989, Antares)** [2][1] : un seul pointeur, des sauts d'un nombre entier de périodes. Latence
minimale (≤ ½ période). C'est le « son » Auto-Tune. Défauts : raccords concentrés, et bruit répété.

**Excitation-PSOLA / LPC « synchronous splicing » (notre approche)** : formants préservés grâce au filtre. Défauts :
l'erreur de modèle LPC devient la couleur du son, et l'ordre élevé (67) sur une fenêtre courte peut capter des harmoniques
aiguës. Notre fenêtre de retard gaussienne limite ce risque, sans l'annuler.

**Vocodeur de phase avec verrouillage identité (Laroche-Dolson)** [17][35] :
- pics = bins supérieurs à leurs deux voisins de chaque côté ; une région d'influence par pic ;
- chaque région est translatée en fréquence, sans rééchantillonnage ;
- même rotation de phase pour tous les bins d'une région (pas d'arctangente) ;
- 75 % de recouvrement obligatoire pour les décalages fractionnaires (bandes latérales −21 dB à 50 %, −51 dB à 75 %) ;
- coût indépendant du décalage, harmonisation gratuite (copie des pics) ;
- brevet expiré [18].

**PGHI** (« Phase vocoder done right ») [19] : la phase est reconstruite par intégration du gradient (en tas). Moins de
phasiness, mais l'anticipation d'une trame ajoute de la latence.

**Enveloppe découplée** :
- **Cepstre lissé** : ordre ≈ fs/(2·f0).
- **True envelope** (itérations du lissage cepstral avec max) [22] : bien meilleure sur les voix aiguës. Le temps de calcul
  est réduit d'un facteur 2,5 à 11 par sous-échantillonnage du log-spectre, pour un coût « légèrement supérieur » à la LPC.
- **LPC** : la moins chère, biaisée vers les harmoniques sur les voix aiguës.
- Cadre général source-filtre en TFCT : Verfaille et Depalle [36].

**WORLD** (Harvest / DIO, CheapTrick, D4C) [23][37] :
- f0 robuste, enveloppe adaptée à f0, apériodicité par bande ;
- BSD modifiée, sans brevet, usage commercial autorisé [23] ;
- permet de modifier f0, l'enveloppe (gorge, genre) et le souffle de façon indépendante ;
- inconvénients : analyse lourde, pensée pour le hors ligne, et rendu « vocodé » possible sur le chant (jugement, non
  mesuré).

**Modèle harmonique + bruit** [5] : il découpe le spectre à une fréquence maximale de voisement Fm. Sous Fm, le signal est
harmonique ; au-dessus, c'est du bruit modulé. La modification de hauteur ne touche que la partie harmonique, ce qui
supprime le bourdonnement des voix soufflées. C'est l'idée clé de l'architecture A.

**Formants indépendants et « throat length ».**
- (i) PSOLA sur signal brut : **rééchantillonner chaque grain** d'un facteur α déplace les formants de α, alors que
  l'espacement des grains fixe la hauteur. Le coût est quasi nul, et c'est la méthode classique.
- (ii) Source-filtre : synthèse avec E(ω/α) au lieu de E(ω). En TFCT avec true envelope, c'est exact. En LPC, on passe par
  une déformation en fréquence par passe-tout (« warped LPC », Strube, *non revérifié*) ou par un décalage des LSF.
- La « longueur de gorge » d'Auto-Tune règle un conduit vocal modélisé : plus court donne une voix plus féminine ou
  enfantine, plus long une voix plus masculine [3][4]. Une gorge de longueur ℓ a des résonances ∝ 1/ℓ, donc **gorge
  ×1,1 ≈ formants ×0,91** : c'est un α uniforme (modèle de tube simple, approximation de notre part).

**Transitoires et consonnes.**
- On les détecte (flux spectral, ratio HF, ou pYIN non voisé) et on les laisse passer **sans traitement, au retard exact**.
  TuneCore le fait déjà, avec un fondu de 4 ms.
- En TFCT, il faut une réinitialisation de phase à l'attaque (travaux de Röbel sur les transitoires, *cité de mémoire*).

**Signalsmith Stretch** (MIT) [20][21] :
- approche spectrale multi-pics (dernière méthode de l'exposé ADC22 « Four ways to write a pitch-shifter ») ;
- `setFormantFactor` et `compensatePitch`, `setFormantBase` (f0 approximative) ; latence en deux moitiés,
  `inputLatency()` + `outputLatency()` ;
- le README admet que les formants sont « moins nets que les méthodes monophoniques comme PSOLA » ;
- préréglages de l'ordre de 100 à 120 ms de bloc (exemple : 120 ms ; *tailles exactes de `presetDefault` et
  `presetCheaper` non revérifiées*) ;
- usage chez nous : le mode Qualité, l'harmonie, ou le décalage hors ligne ; jamais le Direct.

## 3. Comparaison objective (latence ≤ 15 ms)

| Approche | Latence voix grave (80 Hz) | CPU / échantillon (estimé) | Qualité petite correction | Qualité grand décalage | Défauts typiques |
|---|---|---|---|---|---|
| Lecteur à cycles, signal brut (Classic) | ≤ ½ T ≈ 6 ms | ≈ 20 à 40 (lecture sinc) | **excellente** | chipmunk | raccords, bruit répété |
| Lecteur à cycles + LPC (actuel) | ≈ 5 à 8 ms | ≈ 1,2 à 1,6 k | bonne, mais colorée | bonne | coloration LPC, bruit répété |
| **A : PSOLA-résidu + séparation H/N** | ≈ T ≈ 12 ms | ≈ 1,5 à 2,5 k | excellente | bonne | marques GCI ratées → bourdonnement |
| TFCT N = 512 + verrouillage + enveloppe | ≈ 11 ms | ≈ 0,6 à 4 k | moyenne (graves flous) | moyenne | phasiness, pré-écho |
| B : TFCT N = 2048, mode Qualité | ≈ 43 ms | ≈ 1 à 5 k | très bonne | **très bonne** | latence |
| WORLD | hors ligne | élevé | très bonne | très bonne | rendu vocodé possible |

**Conclusion :** sous 15 ms, seul le temporel synchrone à la période tient. Le meilleur compromis est A, qui garde
la latence actuelle et s'attaque aux trois suspects de la synthèse (point 2).

## 4. Produits commerciaux : ce qui est établi

- **Antares, brevet US 5 973 252** [1] : énergie E(L) − 2H(L) mise à jour récursivement ; décimation 8:1 ; parabole sur
  trois points ; test L/2L ; décroissance (Decay) pour lisser la correction ; vibrato ajouté par modulation ; sortie par
  interpolation, avec des cycles répétés quand le pointeur prend de l'avance (son aigu) et sautés quand il prend du retard
  (son grave).
- **Retune Speed, Humanize, Flex-Tune** [4] :
  - Retune Speed 0 = robot ; 10 à 50 ms = naturel ;
  - Humanize = retune plus lent sur la partie tenue des notes longues ;
  - Flex-Tune = zone de correction réduite autour de chaque note ;
  - Classic = son d'Auto-Tune 5, « un peu plus brillant, attaque plus marquée », sans formants ni gorge.
  Notre courbe (centre / expression, zone morte) couvre déjà ces concepts.
- **Evo** [3] : formants, throat, meilleur suivi d'octave, Input Type.
- **Waves Tune RT** [24], **MetaTune** [25][26], **Graillon 3** [27], **Little AlterBoy** [28] : voir la synthèse.
  **Aucun ne publie son algorithme**, et toute affirmation plus précise trouvée en ligne serait de la rumeur.
- **Melodyne** [29] : détection temporelle, « instantanés » périodiques de type table d'onde ; DNA = Fourier +
  « évaluation de pertinence » des partiels.

## 5. Licences et pièges

**Exclus :**
- Rubber Band : GPL, ou licence commerciale de Breakfast Quay ; GPL interdite sur l'App Store [30] ;
- SoundTouch : LGPL ;
- PESTO : LGPL-3 [11] ;
- Vamp pYIN : GPL probable, non vérifié ;
- FFTW (GPL), s'il est un jour tenté.

**Autorisés :**
- Signalsmith Stretch : MIT [20] ;
- WORLD : BSD modifiée [23] ;
- code torchcrepe : MIT [9] ;
- torchfcpe : MIT [13] ;
- swift-f0 : MIT [15].

**Poids des réseaux :** la licence n'est pas énoncée séparément pour CREPE, FCPE et SwiftF0. Pour RMVPE, la licence du code
n'est pas indiquée dans l'article [16]. Pour SPICE, les conditions TF Hub sont à vérifier.

**Brevets :** Antares tombé vers 2018 (calcul sur 20 ans depuis le dépôt) ; Laroche-Dolson indiqué « Expired - Lifetime »
(2019) [18]. Le brevet PSOLA de France Télécom (vers 1988) est *supposé expiré, non vérifié*. Rien dans ce document n'est un
avis juridique.

## 6. Architectures candidates : pseudo-code

### A. PSOLA sur résidu, séparation harmonique / bruit (mode Direct)

```
Paramètres : fs ; Lz = latence déclarée = max(5 ms, Tmax_voix) ; ordre LPC p = fs/1000 + 4 (≈ 52 à 48 kHz)
             Fm (fréquence max de voisement) ∈ [2,5 ; 8] kHz ; seuil formants libres S_f = 2,5 dt
Structures :
  x[4096], e[4096]   anneaux : entrée préaccentuée, résidu LPC
  Kring[64]          coefficients du treillis par trame (déjà présent)
  marks              file circulaire de 64 marques d'analyse {t (fractionnaire), T, voisé}
  olaH[8192]         anneau d'addition-recouvrement de la partie harmonique
  t_s                instant de la prochaine marque de synthèse
  lpH, hpN           crossover de Linkwitz-Riley d'ordre 4 à Fm (résidu → harmonique / bruit)

Par échantillon n :
  pe = x[n] − 0.9·x[n−1] ;  e[n] = treillisAnalyse(pe, K(n))           // existant
  eH = lpH(e[n]) ;  eN = hpN(e[n])                                     // séparation H/N (HNM-lite)
  // détection de marque (SEDREAMS simplifié) : dans [t_last + 0,7T ; t_last + 1,3T],
  // argmax de −eH lissé (passe-bas 1 kHz) ; accepté si voisé(détecteur) ; sinon marque « virtuelle » à t_last + T
  si n == t_last + 1,3·T : pousser marque(argmax), t_last = argmax
  // synthèse : placer les grains dont le centre ≤ n − Lz + T
  tant que t_s ≤ n − Lz + T(t_s) :
      k  = marque d'analyse la plus proche de t_s (en temps d'analyse = t_s)        // pas d'étirement : même axe temps
      T  = min(T_k, T_{k±1}) ;  w = Hann(2T)
      g  = eH[t_k − T .. t_k + T] · w                         // lecture sinc 16 points (existant) aux positions fractionnaires
      si |décalage| ≥ S_f ou formant ≠ 1 : g reste sur le résidu (formants préservés / déformés par l'enveloppe)
      ajouter g dans olaH centré sur t_s
      t_s += T_k / r(t_s)                                      // r = rapport de hauteur issu de la courbe de correction
  h  = olaH[n − Lz] ;  olaH[n − Lz] = 0
  nN = eN[n − Lz]                                              // bruit : JAMAIS répété, seulement retardé de Lz
  y_res = h + g_N(n)·nN                                        // g_N : gain de bruit (≈ 1 ; « souffle » réglable)
  y = treillisSynthese(y_res, K(instant d'analyse de la marque k))  // recoloration à l'instant lu (règle existante)
  // formants libres (|décalage| < S_f) : même PSOLA, mais sur x brut au lieu de eH, sans treillis (son « Classic »)
  // fondu de 20 ms entre les deux chemins selon |décalage|
  // déformation des formants / gorge : chemin brut → rééchantillonner g d'un facteur α avant l'OLA ;
  //                                    chemin LPC  → K(n) déformés (passe-tout λ) ou LSF × α
  y = deaccentuation(y) ; mélange voisé / sec (existant)
```

**Coût estimé par échantillon** (48 kHz, comptage d'opérations) :

| Poste | Opérations par échantillon |
|---|---|
| Treillis d'analyse + synthèse (p = 52) | ≈ 4p ≈ 210 |
| Autocorrélation de trame (fenêtre 1024, 53 retards, saut de 128) | ≈ 430 |
| Crossover | ≈ 20 |
| Grains : 2 recouvrements × lecture sinc 16 points | ≈ 70 |
| Détection de marques | ≈ 10 |
| Détecteur (YIN décimé + NSDF) | ≈ 500 à 800 |
| **Total** | **≈ 1,3 à 1,6 k** (≈ 2,5 k avec le chemin brut en parallèle pendant les fondus) |

Soit ≈ 70 à 120 MFLOP/s, quelques % d'un cœur.

**Latence :** il faut connaître la marque suivante, donc une anticipation ≈ T. C'est ≤ 12,5 ms à 80 Hz et 16 ms à 62 Hz
(au-delà de 15 ms, rester en lecteur à cycles pour f0 < 67 Hz). On reste sous 15 ms pour toutes les voix au-dessus de
67 Hz.

**Risques :**
- **Marques fausses** sur les attaques et le vocal fry : bourdonnement ou « grésillement ». Parade : repli automatique sur
  le Splicer actuel quand la régularité des marques chute.
- **Fm mal choisi** : HF non décalées, donc un léger « double » sur les grands décalages. Fm adaptatif selon le ratio
  harmonique des bandes (idée D4C).
- **Changement de latence** par rapport à l'existant : à décider. On peut garder Lz actuel en Direct et réserver A au
  mode Qualité si nécessaire.

### B. Vocodeur à peaks verrouillés + true envelope (mode Qualité, ≈ 45 ms)

```
N = 2048 (48 kHz), hop H = N/8 = 256, fenêtre de Hann ; latence ≈ N + H
Par trame :
  X = FFT(w·x)
  E = trueEnvelope(log|X|, ordre = fs/(2·f0), ≤ 6 itérations, sous-échantillonné ×4)   [22]
  S = X / E                                                    // source blanchie
  pics = bins k : |S_k| > |S_{k±1}|, |S_{k±2}| ; régions = mi-chemin ou minimum entre pics   [17]
  pour chaque pic p (fréquence instantanée ω_p par différence de phase) :
      k' = r·k_p ; translater la région (interpolation linéaire, 75 % de recouvrement)
      φ_p ← φ_p + (r − 1)·ω_p·H  (rotation accumulée)          // verrouillage identité : même rotation pour la région
  transitoire (flux > seuil) : phase réinitialisée à la phase d'analyse, pas d'accumulation
  Y = S' · E(ω/α)                                              // α = facteur formants / gorge, r = rapport de hauteur
  y += w·IFFT(Y)   (OLA, normalisation Σw² )
```

**Coût :** FFT + IFFT ≈ 10·N·log2N / H ≈ 900 opérations par échantillon, plus true envelope (2 FFT de N/4 par itération ×
6) ≈ 600, plus les pics ≈ 50 : **≈ 1,5 à 2 k par échantillon**.

**Raccourci :** Signalsmith Stretch avec `setTransposeSemitones`, `setFormantFactor(α, compensatePitch = true)` et
`setFormantBase(f0 / fs)`, à mesurer avec un bloc de 2048 et un intervalle de 256.

**Risques :** pré-écho sur les plosives (passer en sec sur les transitoires), phasiness résiduelle sous 100 Hz.

### C. Analyse / synthèse de type WORLD (éditeur de notes, hors ligne)

```
Capture → f0 (pYIN hors ligne ou SwiftF0 ONNX, Viterbi complet) → CheapTrick (enveloppe adaptée à f0, 5 ms)
        → D4C (apériodicité par bande) → édition (centre, vibrato, transitions, formants α, souffle)
        → synthèse (impulsions mixtes + bruit filtré) ; bypass exact des segments non voisés
```

Le coût n'a pas d'importance ici, car le calcul se fait en arrière-plan. Risques : rendu « vocodé » sur certaines voix (à
écouter), et dépendance à l'exactitude de la segmentation.

## 7. Tests de validation chiffrés (proposés)

| Test | Signal | Mesure | Seuil |
|---|---|---|---|
| Précision de hauteur | voyelle synthétique (source glottique LF + 5 formants), f0 de 70 à 800 Hz | erreur RMS en cents sur une note tenue | < 1 ct (Qualité), < 2 ct (Direct) |
| Erreurs grossières / octaves | MIR-1K, PTDB-TUG (si la licence le permet) + rap enregistré | % de trames à plus de 50 ct ; % d'erreurs d'octave | < 1 % ; < 0,5 % |
| Voisement | les mêmes, annotés | F1 voisé / non voisé | ≥ 0,93 |
| THD+N inter-harmonique | voyelle synthétique décalée de ±1, ±3, ±7 dt | énergie entre harmoniques de la f0 cible / énergie harmonique | ≤ −60 dB (±3 dt), ≤ −50 dB (±7 dt) |
| Préservation de l'enveloppe | idem | LSD entre true envelopes entrée / sortie, de 0 à 5 kHz | < 1,5 dB (formants préservés) |
| Formants | idem | erreur relative sur F1 à F3 (racines LPC) | < 3 % ; avec α = 1,1 : F_i × 1,1 ± 2 % |
| Jitter (robot) | voix réelle avec vibrato, vitesse 0 | jitter période à période en sortie (marques GCI) | < 0,2 % ; résidu de hauteur < 2 ct crête à crête |
| Répétition de bruit | bruit rose « voisé » artificiellement (f0 annoncée 150 Hz), puis souffle réel | max de l'autocorrélation normalisée de la sortie autour de la période imposée | < 0,15 (le lecteur à cycles actuel devrait donner bien plus) |
| Transitoires | impulsions et plosives /t/ /k/ | énergie avant l'attaque (pré-écho) / énergie de l'attaque | < −40 dB |
| Identité | décalage 0 | différence bit à bit avec le sec retardé | exactement 0 (déjà le cas) |
| Latence | impulsion et sinus | retard mesuré égal à la latence déclarée | ± 0 échantillon |
| CPU | 48 kHz, mono, buffer 64 | ns par échantillon (bench -O2) | < 2 % d'un cœur (Direct) |

## 8. Ce que je n'ai pas pu vérifier

- GitHub n'est pas accessible depuis cette session (le dépôt Signalsmith a été lu via un miroir, ecosyste.ms). Les tailles
  exactes de bloc de `presetDefault` et `presetCheaper` ne sont pas revérifiées.
- Les algorithmes internes de MetaTune, Graillon, Little AlterBoy, Waves Tune RT et d'Auto-Tune Evo / Pro ne sont pas
  publiés. Seuls les comportements observables sont cités.
- Les licences des **poids** de CREPE, FCPE, SwiftF0 et SPICE, et la licence du code RMVPE, ne sont pas vérifiées. La
  licence GPL du Vamp pYIN n'est pas vérifiée non plus.
- L'expiration exacte du brevet Antares (paiement des annuités) n'est pas vérifiée, ni le brevet PSOLA de France Télécom.
- Sont cités de mémoire : Strube (warped LPC), Röbel (transitoires en TFCT), Lent 1989 (connu ici via [2]).
- Tous les coûts CPU sont des comptages d'opérations, **sans aucun benchmark exécuté**. Les trois « suspects » du son
  métallique sont des hypothèses issues de la littérature, **non mesurées sur notre code**.

## Bibliographie

1. Hildebrand, *Pitch detection and intonation correction apparatus and method*, US 5 973 252 (déposé le 14/10/1998, délivré le 26/10/1999). https://uspto.report/patent/grant/5973252
2. S. Costello (Valhalla DSP), « Auto-Tune, autocorrelation, and seismic analysis », avec la réponse de Hildebrand (2016). https://valhalladsp.com/?p=1559
3. MusicRadar, test d'Antares Auto-Tune Evo. https://www.musicradar.com/reviews/tech/antares-auto-tune-evo-183704
4. Antares, manuel d'Auto-Tune Artist (Retune Speed, Humanize, Flex-Tune, Classic, Formant / Throat, Input Type, Tracking). https://www.barryrudolph.com/recall/manuals/auto-tune_artist.pdf
5. Y. Stylianou, « Applying the harmonic plus noise model in concatenative speech synthesis », IEEE TSAP 2001. https://ee.columbia.edu/~dpwe/classes/e6820-2007-05/papers/Styl01-hnm.pdf ; aussi https://www.isca-archive.org/eurospeech_1995/stylianou95b_eurospeech.html
6. Cours sur TD-PSOLA et les modifications prosodiques (Gutierrez-Osuna, TAMU). https://people.engr.tamu.edu/rgutier/lectures/sp/l19.pdf (article d'origine : Moulines et Charpentier, *Speech Communication*, 1990, non consulté)
7. M. Mauch, S. Dixon, « pYIN: a fundamental frequency estimator using probabilistic threshold distributions », ICASSP 2014. https://core.ac.uk/download/30696897.pdf
8. B. Gfeller et al., « SPICE: Self-supervised pitch estimation », 2019. https://arxiv.org/pdf/1910.11664
9. torchcrepe (MIT). https://pypi.org/project/torchcrepe/ ; CREPE : Kim et al., ICASSP 2018
10. A. Riou et al., « PESTO: Real-time pitch estimation… » (TISMIR, 2025). https://arxiv.org/html/2508.01488v2 ; dépôt https://github.com/SonyCSLParis/pesto
11. pesto-pitch sur PyPI (LGPLv3). https://pypi.org/project/pesto-pitch/
12. « FCPE: A Fast Context-based Pitch Estimation Model », 2025. https://arxiv.org/html/2509.15140v1
13. torchfcpe sur PyPI (MIT). https://pypi.org/project/torchfcpe
14. « SwiftF0: Fast and Accurate Monophonic Pitch Detection », 2025. https://arxiv.org/html/2508.18440v1
15. swift-f0 sur PyPI (MIT, ONNX, 176 ms d'anticipation). https://pypi.org/project/swift-f0/
16. « RMVPE: A Robust Model for Vocal Pitch Estimation in Polyphonic Music », 2023. https://arxiv.org/html/2306.15412v1
17. J. Laroche, M. Dolson, « New phase-vocoder techniques for real-time pitch shifting, chorusing, harmonizing… », JAES 1999. https://www.ee.columbia.edu/~dpwe/papers/LaroD99-pvoc.pdf
18. US 6 549 884 B1, *Phase-vocoder pitch-shifting* (Creative, « Expired - Lifetime »). https://patents.google.com/patent/US6549884
19. Z. Průša, N. Holighaus, « Phase Vocoder Done Right », 2017. https://arxiv.org/pdf/2202.07382
20. Signalsmith Stretch (MIT) : README via miroir. https://awesome.ecosyste.ms/projects/github.com%2FSignalsmith-Audio%2Fsignalsmith-stretch
21. Signalsmith Stretch Web (README, `formantCompensation`, `formantBaseHz`, blocs de 120 ms). https://unpkg.com/signalsmith-stretch@1.3.2/README.md ; exposé ADC22 : https://classcentral.com/course/youtube-four-ways-to-write-a-pitch-shifter-geraint-luff-adc22-320094
22. A. Röbel, X. Rodet, « Efficient spectral envelope estimation and its application to pitch shifting and envelope preservation », DAFx 2005. https://dafx.de/paper-archive/details/EDSTSaO17Gd_0LV03iwDfw (PDF : https://dafx.de/paper-archive/2005/P_030.pdf)
23. M. Morise, FAQ WORLD (BSD modifiée, sans brevet, usage commercial autorisé). https://www.isc.meiji.ac.jp/~mmorise/world/english/faq.html
24. Sound On Sound, test de Waves Tune Real-Time (latence de 0 à 4 ms selon la note). https://www.soundonsound.com/reviews/waves-tune-real-time
25. Sound On Sound, test de Slate Digital MetaTune. https://www.soundonsound.com/reviews/slate-digital-metatune
26. Documentation de MetaTune (Note Stabilizer : 280 ms). https://docs.slatedigital.com/MetaTune/MetaTune.html
27. Auburn Sounds, « Introducing Graillon 3 ». https://www.auburnsounds.com/blog/Introducing-Graillon-3.html
28. Produce Like A Pro, test de Little AlterBoy. https://producelikeapro.com/blog/little-alterboy-review/ ; et SOS, « Creative Pitch-correction Plug-ins » : https://www.soundonsound.com/reviews/creative-pitch-correction-plug-ins
29. Sound On Sound, entretien avec Peter Neubäcker (Melodyne). https://www.soundonsound.com/people/peter-neubacker-melodyne-celemony-next-decade
30. README de Rubber Band (GPL ou licence commerciale). https://donkey.abanbytes.eu/david/librubberband/raw/commit/9afba0678769bd8c72f370bac20bc5cee6cce649/README.md
31. A. de Cheveigné, H. Kawahara, « YIN, a fundamental frequency estimator for speech and music », JASA 2002 (non refetché ; voir docs/recherche-algos.md)
32. P. McLeod, G. Wyvill, « A smarter way to find pitch », ICMC 2005. https://www.cs.otago.ac.nz/graphics/Geoff/tartini/papers.html
33. A. Camacho, J. Harris, « A sawtooth waveform inspired pitch estimator for speech and music », JASA 2008. https://audiolabs-erlangen.de/content/05_fau/professor/00_mueller/02_teaching/2024s_sarntal/01_group_F0/2008_CamachoH_SWIPE_JASA.pdf
34. T. Drugman et al., « Detection of Glottal Closure Instants from Speech Signals: a Quantitative Review », IEEE TASLP 2012. https://arxiv.org/pdf/2001.00473
35. J. Laroche, M. Dolson, « About this phasiness business », 1997. https://www.ee.columbia.edu/~dpwe/papers/LaroD97-phasiness.pdf
36. V. Verfaille, P. Depalle, « Adaptive effects based on STFT, using a source-filter model », DAFx 2004. https://dafx.de/paper-archive/2004/P_296.PDF
37. Publications de M. Morise (DIO, Harvest, CheapTrick, D4C). https://www.isc.meiji.ac.jp/~mmorise/world/english/publications.html
