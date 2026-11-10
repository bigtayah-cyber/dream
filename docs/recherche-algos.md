# Recherche algorithmes — suite dreamaws

Date : 2026-11-09. Contexte : JUCE 8, C++17, licence propriétaire. **Règle** : implémenter à partir des publications ; reprendre du code seulement s'il est MIT/BSD/ISC/Apache/domaine public.
Fiabilité des sources : **[R]** publication revue, **[D]** documentation officielle ou manuel, **[B]** blog ou note technique, **[F]** forum. Les numéros [n] renvoient à la liste en fin de document.

> **Alerte licences (vérifiées)** : ChowTapeModel est sous **GPLv3** [27]. Dans chowdsp_utils, **chowdsp_compressor et chowdsp_filters sont GPLv3** ; seuls certains modules (core, buffers…) sont BSD [28]. Ne rien copier de ces modules. Signalsmith Stretch est sous **MIT** [12], donc utilisable.

---

## 1. Autotune / correction de hauteur

**Principe.** La chaîne se fait en quatre étages : détection de f0, choix de la note cible, lissage de la correction (« retune speed »), puis décalage de hauteur avec préservation des formants. Le brevet Antares d'origine (Hildebrand, US 5 973 252, expiré en 2018 [10]) procède ainsi : autocorrélation sur un signal sous-échantillonné ×8, rapport de correction = période mesurée / période voulue, lissage par un coefficient « Decay » (0 = instantané, proche de 1 = graduel), puis rééchantillonnage qui répète ou saute des cycles entiers.

**Détection — YIN [1]** (fenêtre W, décalage τ) :
- d(τ) = Σ_{j=1..W} (x_j − x_{j+τ})², calculable par FFT : d(τ) = r_t(0) + r_{t+τ}(0) − 2·r_t(τ).
- Fonction normalisée (CMNDF) : d′(0) = 1 ; d′(τ) = d(τ) / [(1/τ)·Σ_{j=1..τ} d(j)].
- On prend le premier τ tel que d′(τ) < seuil (0,10 à 0,15), on descend jusqu'au minimum local, puis on affine par interpolation parabolique. f0 = fs/τ.
- W ≥ 2·τ_max. Avec f_min = 70 Hz, τ_max ≈ 14 ms et W ≈ 28 ms.

**MPM [2]** (McLeod) : NSDF n′(τ) = 2·r′(τ)/m′(τ), avec m′(τ) = Σ (x_j² + x_{j+τ}²). On retient les maxima « clés » entre passages à zéro, puis le premier maximum ≥ k·n_max (k = 0,8 à 0,93). MPM fait moins d'erreurs d'octave que YIN sur la voix et fonctionne bien en temps réel.

**pYIN + Viterbi [3]** : on calcule YIN pour une distribution de seuils (bêta), ce qui donne plusieurs candidats f0 pondérés. Un HMM porte ensuite des états {hauteur (bins de 0,1 demi-ton) × voisé/non voisé}, avec une transition limitée à quelques demi-tons par trame et une probabilité de bascule voisé/non voisé faible (≈ 0,01). Le décodage de Viterbi se fait hors ligne, ou en ligne sur une fenêtre glissante de 100 à 200 ms (ce qui ajoute de la latence). Défauts librosa (de mémoire) : 100 seuils, bêta (2, 18), résolution 0,1, switch_prob 0,01. L'implémentation de référence de librosa est sous licence ISC, non revérifiée ici [29]. Le plugin Vamp pYIN serait sous GPL (non vérifié) : partir du papier. CREPE : repère de qualité seulement (trop lourd).

**Note cible et retune.** On calcule p = 69 + 12·log2(f0/440). On quantifie p vers la gamme, avec une hystérésis de ±(0,5 + h) demi-ton, h ≈ 0,1 à 0,2, pour éviter que la note cible oscille. La correction se fait en demi-tons : c = cible − p_centre. Lissage : c_s ← c_s + (c − c_s)·(1 − e^{−T/τ}), avec τ = 0 ms pour l'effet « robot » et 50 à 400 ms pour un rendu naturel.
- **Préserver le vibrato** : p_centre = passe-bas de p (≈ 3 Hz) ou médiane sur 150 à 300 ms. On ne corrige que le centre, donc l'écart p − p_centre (le vibrato) passe intact.
- **« Humanize »** : allonger τ quand la note est tenue, garder τ court aux transitions de note. On peut ajouter un vibrato synthétique en modulant c_s, comme le décrit le brevet.

**Décalage de hauteur.**
- **TD-PSOLA [4]** : on place des marques de pitch synchrones (pics ou instants de fermeture glottique, un par période P). On extrait des grains avec une fenêtre de Hann de longueur 2P, puis on les replace avec une période P′ = P/α. Cela coûte très peu, la latence est d'environ 2 périodes, et les formants sont conservés naturellement jusqu'à environ ±5 demi-tons. **Pièges** : une marque mal placée produit un bourdonnement ; les parties non voisées doivent passer sans traitement ou via WSOLA.
- **WSOLA [5]** : recouvrement-addition avec une tolérance ±Δ (≈ 5 à 10 ms) sur la position du segment, choisie pour maximiser l'intercorrélation. Utile pour l'étirement et les consonnes.
- **Vocodeur de phase avec verrouillage de phase [6]** : on repère les pics du spectre. Pour chaque bin k de la région du pic p : φ_out(k) = φ_out(p) + φ_in(k) − φ_in(p) (verrouillage « identité »). C'est plus propre que le vocodeur classique, avec moins de « phasiness ». La variante PGHI [7] reconstruit la phase par intégration du gradient. Le décalage de hauteur se fait par étirement puis rééchantillonnage, ou par déplacement direct des bins.
- **Signalsmith Stretch (MIT) [12]** : vocodeur multi-pics, décalage de hauteur et de formants intégré, latence de l'ordre de la taille de bloc (environ 100 ms dans le préréglage par défaut, *à mesurer*). C'est la voie la plus rapide pour une qualité proche d'Élastique. Élastique et Rubber Band sont sous licence commerciale ou GPL : à exclure.

**Formants.** Étapes : (1) estimer l'enveloppe E(k) ; (2) blanchir le signal, X/E ; (3) décaler la hauteur ; (4) remultiplier par E(k), ou par E(k/β) pour un décalage de formants.
- **Cepstre** : c = IFFT(log|X|), on ne garde que les coefficients n < N_c, puis E = exp(FFT(c)).
- **« True envelope » [8]** : A₀ = log|X| ; à chaque itération, V_i = lissage cepstral de A_{i−1}, puis A_i = max(A_{i−1}, V_i). On s'arrête quand max(A − V) < 2 dB. Ordre ≈ 0,5·fs/f0, de 3 à 10 itérations.
- **LPC** : ordre ≈ fs/1000 + 2. Moins bon sur les voix aiguës.

**Édition note par note (type Melodyne).** Hors ligne, on calcule pYIN puis une segmentation en notes. Tony [9] utilise un HMM de notes à 3 états (attaque, stable, silence) par demi-ton. Variante simple : nouvelle note dès qu'un saut de plus de 0,6 demi-ton tient plus de 60 ms, ou qu'un onset de flux spectral apparaît, ou que le voisement change. Pour chaque note on obtient : centre = médiane, dérive = passe-bas, vibrato = résidu. L'édition modifie le centre, l'amplitude du vibrato et la pente des transitions. La resynthèse se fait par PSOLA ou par vocodeur hors ligne, donc sans contrainte de latence.

**Latence en temps réel.** Elle vaut environ W/2 à W pour la détection, plus 1 à 2 périodes pour PSOLA, soit 20 à 40 ms au total, à déclarer avec setLatencySamples. Un mode « tracking » rapide (f_min = 100 Hz) la réduit.

**Ce que je ferais dans dreamaws.** Détecteur MPM ou YIN par FFT avec hystérésis de note, plus TD-PSOLA pour le temps réel (formants gratuits) : **M**. Signalsmith Stretch (MIT) comme moteur haute qualité, avec formants « true envelope » : **M**. Éditeur de notes (pYIN, Viterbi, segmentation) hors ligne : **L**.

---

## 2. Compresseurs

**Socle (Giannoulis, Massberg, Reiss [13]).**
- **Calcul de gain (en dB, seuil T, ratio R, genou W)** :
  - si 2(x − T) < −W : y = x ;
  - si |2(x − T)| ≤ W : y = x + (1/R − 1)·(x − T + W/2)² / (2W) ;
  - sinon : y = T + (x − T)/R.
  - Gain g = y − x.
- **Détecteur recommandé** : « smooth branching » dans le domaine log, appliqué à la réduction de gain. y_L[n] = α·y_L[n−1] + (1 − α)·x_L, avec α = α_A en attaque (x_L > y_L) et α_R sinon, et α = e^{−1/(τ·fs)}.
- **RMS** : m ← α·m + (1 − α)·x², sur une fenêtre de 10 à 50 ms.
- **Topologie** : le feed-forward est stable et prévisible. Le feedback (gain calculé sur la sortie y[n−1]) donne la « couleur » des appareils anciens.
- **Side-chain** : passe-haut de 60 à 150 Hz (filtre SVF du §4) pour que la basse ne fasse pas pomper.

**Optique (LA-2A).**
- **Mesures** [15] : la cellule réagit en moins de 5 ms à l'allumage, mais met environ 500 ms à revenir au-delà de 1 MΩ, soit environ 100 fois plus lent. L'attaque du circuit complet est plus lente, car un condensateur la ralentit.
- **Modèle physique de vactrol** [14] (VTL5C3/2, à états de charges q⁺ et q⁻) : R = 1/(μ⁺q⁺ + μ⁻q⁻), en parallèle avec la résistance d'obscurité. Les deux équations de porteurs donnent naturellement la mémoire liée à l'historique du signal. Tous les paramètres sont publiés [14].
- **Grey-box LA-2A** [16] : attaque appris ≤ 15 ms, relâche beaucoup plus long. Une RNN qui module α imite la dépendance au programme.
- **Modèle pratique** : la lumière vient du détecteur. Deux états :
  - s₁ rapide (attaque ≈ 10 ms, relâche ≈ 60 ms) ;
  - s₂ lent (relâche de 1 à 5 s), qui se « charge » proportionnellement au temps passé en compression. C'est la mémoire.
  - On en tire R = R_min·(s₁ + k·s₂)^{−γ} (γ ≈ 0,7 à 1) et g = R/(R + R_série).
  - Le manuel UA décrit un relâche en deux temps : ≈ 60 ms pour 50 %, puis 1 à 15 s *(non revérifié)*.

**FET (1176).** La topologie est en feedback. Un JFET en région ohmique sert de résistance variable dans un diviseur : r_ds ≈ r₀ / (1 − V_gs/V_p). Valeurs du manuel, *non revérifiées en ligne* : attaque de 20 à 800 µs, relâche de 50 ms à 1,1 s, ratios 4, 8, 12 et 20.
- **Modèle** : calcul de gain à genou dur, attaque très courte, relâche dépendant du programme (α_R plus lent quand la réduction de gain dure).
- **Mode « all-buttons »** (heuristique) : ratio ≈ 20, genou plus large, sur-compression au-delà du seuil, et distorsion ajoutée proportionnelle à la réduction de gain.
- Aucune publication revue sur le 1176 n'a été trouvée ici. La thèse de Moore (Huddersfield) étudie la perception des 1176 [17], sans modèle complet vérifié.

**Variable-mu (670, Manley).** Le gain d'une lampe à coupure progressive dépend de la polarisation de grille. Le ratio augmente donc avec le niveau (genou très large, pas de seuil net) et la topologie est en feedback.
- **Modèle** : g = 1 / (1 + (V_c/V₀)^p), avec p ≈ 1,5 à 3, où V_c est la tension de commande lissée.
- Ajouter une légère asymétrie (harmonique 2) proportionnelle à V_c, comme un push-pull déséquilibré.
- Constantes du 670 (de mémoire, *non vérifiées*) : attaque de 0,2 à 0,8 ms, relâche de 0,3 à 25 s, dont deux positions dépendantes du programme.
- Aucune publication revue trouvée : c'est le sujet le moins sourcé de ce rapport.

**VCA (type dbx/SSL).** Giannoulis s'applique tel quel, en feed-forward, avec détecteur RMS. Le VCA est quasi transparent.

**Transformateur.** La saturation dépend du flux, c'est-à-dire de l'intégrale du signal : elle augmente vers le grave.
- **Approche simple** : Φ = intégrateur à fuite de x (f_c ≈ 20 à 40 Hz). Sortie = x − β·(f(Φ) − Φ)·(1/gain d'intégration), avec f = tanh. On l'applique avec ADAA (§3).
- **Approche physique** : Jiles-Atherton dans un circuit (Holters et Zölzer, DAFx 2016 [26], *URL non vérifiée*).

**WDF** : thèse de Kurt Werner [18]. Utile pour les circuits passifs (Pultec, §4). Pour le contrôle de gain, l'approche « détecteur + loi de gain » est moins coûteuse et suffit.

**Pièges.** Rendre g continu (pas de saut de ratio) ; lisser les paramètres ; prévoir un lookahead optionnel (déclarer la latence) ; travailler en dB uniquement dans le détecteur ; protéger les log(0) avec une borne à −120 dB.

**Ce que je ferais dans dreamaws.** Un moteur unique « détecteur + calcul de gain [13] », décliné en 4 caractères : opto (2 états et mémoire), FET (feedback, relâche dépendant du programme), vari-mu (loi continue), VCA (RMS) : **M**. Transformateur par flux intégré + tanh ADAA : **S**. Vactrol physique [14] seulement en mode « premium » : **M**.

---

## 3. Saturation lampe / bande / transformateur

**ADAA [19][20].** Pour une non-linéarité f sans mémoire, de primitive F₁ :
- **Ordre 1** : y[n] = (F₁(x_n) − F₁(x_{n−1})) / (x_n − x_{n−1}). Si |Δx| < ε (≈ 1e-5 en float), utiliser y = f((x_n + x_{n−1})/2).
  - tanh : F₁ = ln cosh x, à calculer de façon stable : |x| + log1p(e^{−2|x|}) − ln 2.
  - Écrêtage dur : F₁ = x²/2 si |x| ≤ 1, sinon |x| − 1/2.
- **Ordre 2** : y = 2/(x_n − x_{n−2}) · [D(x_n, x_{n−1}) − D(x_{n−1}, x_{n−2})], avec D(a, b) = (F₂(a) − F₂(b))/(a − b).
- **Pièges** : l'ADAA ajoute un retard d'un demi-échantillon et un léger passe-bas (à compenser si on mélange sec et traité). L'ordre 2 est sensible au bruit numérique. L'ADAA ne s'applique pas directement aux systèmes à état.
- **Règle pratique** : ADAA d'ordre 1 + suréchantillonnage ×2 donne à peu près la qualité d'un suréchantillonnage ×8 naïf.

**Triode.**
- **Koren [21]** :
  - E₁ = (E_p/k_p)·ln(1 + exp(k_p·(1/μ + E_g/√(k_vb + E_p²)))) ;
  - I_p = (E₁^{E_x}/k_g1)·(1 + sgn E₁).
  - 12AX7 : μ = 100, E_x = 1,4, k_g1 = 1060, k_p = 600, k_vb = 300.
- **Dempwolf et Zölzer [22]** :
  - I_k = G·(ln(1 + e^{C(V_pk/μ + V_gk)})/C)^γ ;
  - courant de grille I_g = G_g·(ln(1 + e^{C_g·V_gk})/C_g)^ξ + I_g0.
  - Paramètres 12AX7 (de mémoire, *à vérifier dans le papier*) : G ≈ 2,24e-3, μ ≈ 103, γ ≈ 1,26, C ≈ 3,4.
- **Étage complet** : résoudre V_p = B⁺ − R_p·I_p(V_g, V_p) par 2 ou 3 itérations de Newton, ou par une table 2D précalculée. Le condensateur de cathode crée la compression dépendant de la polarisation. Revue générale : Pakarinen et Yeh [23].

**Bande (Chowdhury, DAFx 2019 [24]).**
- **Hystérésis de Jiles-Atherton** : dM/dH = (1 − c)·δ_M·(M_an − M) / [(1 − c)·δ_S·k − α·(M_an − M)] + c·dM_an/dH.
  - M_an = M_s·L(Q), avec Q = (H + αM)/a.
  - L(x) = coth x − 1/x (≈ x/3 près de 0), L′(x) = 1/x² − coth²x + 1.
  - δ_S = signe de Ḣ ; δ_M = 1 si δ_S et (M_an − M) sont de même signe, sinon 0.
- **Paramètres** : M_s = 3,5e5, a = 2,2e4, α = 1,6e-3, k = 2,7e4, c = 0,17.
- **Résolution numérique** : Ḣ(n) = 2(H(n) − H(n−1))/T − Ḣ(n−1), puis RK4 avec demi-pas interpolés. Le papier signale l'instabilité de la méthode trapézoïdale. Suréchantillonnage ×16 avec simulation du bias à 55 kHz.
- **Pertes de lecture** (k = 2πf/v) : e^{−kd}·(1 − e^{−kδ})/(kδ)·sin(kg/2)/(kg/2), implémentées en FIR d'ordre 100.
- **Allègements** : supprimer le bias simulé et normaliser H, ce qui permet un suréchantillonnage ×2 à ×4 (perte de fidélité acceptable).
- **Autour du modèle** :
  - égalisation NAB ou CCIR (constantes 50 µs / 3180 µs ou 35 µs) en pré-accentuation avant, désaccentuation après ;
  - « head bump » : cloche +2 dB vers 50 à 100 Hz ;
  - wow et flutter : modulation de retard à 0,5 à 1 Hz et 5 à 10 Hz, quelques échantillons d'amplitude, avec interpolation.
- **Licence** : le code ChowTape est **GPLv3** [27]. Ré-implémenter depuis le papier uniquement.

**Suréchantillonnage.** juce::dsp::Oversampling : demi-bande IIR polyphasé (faible latence, phase non linéaire) ou FIR équiondulation (phase linéaire, plus de latence). Le choix se fait par qualité. Ne suréchantillonner que la partie non linéaire.

**Ce que je ferais dans dreamaws.** Bibliothèque de non-linéarités ADAA1 (tanh, écrêtage dur, asymétrique) + suréchantillonnage ×2 : **S**. Triode Koren en table 2D : **M**. Bande Jiles-Atherton RK4 ×4 sans bias, avec EQ NAB et wow/flutter : **L**.

---

## 4. Égaliseurs analogiques, SVF, TPT/ZDF

**SVF de Simper (Cytomic) [25].**
- **Coefficients** : g = tan(π·f_c/fs), k = 1/Q, a₁ = 1/(1 + g(g + k)), a₂ = g·a₁, a₃ = g·a₂.
- **Par échantillon** :
  - v₃ = v₀ − ic₂ ;
  - v₁ = a₁·ic₁ + a₂·v₃ ;
  - v₂ = ic₂ + a₂·ic₁ + a₃·v₃ ;
  - ic₁ = 2v₁ − ic₁ ; ic₂ = 2v₂ − ic₂.
  - Sortie = m₀·v₀ + m₁·v₁ + m₂·v₂.
- **Mélanges (m₀, m₁, m₂)** :
  - passe-bas (0, 0, 1) ; passe-haut (1, −k, −1) ; passe-bande (0, 1, 0) ;
  - cloche, avec A = √gain et k = 1/(Q·A) : (1, k(A² − 1), 0) ;
  - shelf bas, avec g/√A : (1, k(A − 1), A² − 1) ;
  - shelf haut, avec g·√A : (A², k(1 − A)·A, 1 − A²).
- **Atout** : le filtre reste stable sous modulation rapide, à la différence des biquads en forme directe. C'est essentiel pour l'EQ dynamique.

**TPT/ZDF** : référence Zavalishin, « The Art of VA Filter Design » (livre gratuit, NI) [30]. Un passe-bas d'ordre 1 TPT s'écrit v = (x − s)·G avec G = g/(1 + g), puis y = v + s et s = y + v. Il sert pour les shelves Baxandall d'ordre 1, avec le grave vers 100 Hz et l'aigu vers 10 kHz, en pente douce.

**Filtres « analog-matched »** : la transformation bilinéaire écrase les cloches près de Nyquist. Deux solutions : suréchantillonnage ×2, ou formules « matched » de Vicanek [31] et gain de Nyquist prescrit d'Orfanidis [32].

**Pultec EQP-1A** : modèle WDF de l'étage passif [33] (chowdsp_wdf, bilinéaire, ×2 à ×4 recommandé, ≈ 16 dB de perte d'insertion à compenser). Le « truc » du grave vient du boost et de l'atténuation à la même fréquence, qui agissent sur des bandes légèrement différentes.
- **Approximation légère** (heuristique) :
  - shelf bas résonant SVF (Q ≈ 0,9) à f pour le boost ;
  - shelf bas d'ordre 1 à environ 1,5·f pour la coupe ;
  - cloche d'aigu à bande variable (3 à 16 kHz) ;
  - shelf d'atténuation d'aigu d'ordre 1 (5, 10 ou 20 kHz) ;
  - étage lampe ADAA léger.
- La licence de chowdsp_wdf n'a **pas été vérifiée** : la contrôler avant tout usage.

**Ce que je ferais dans dreamaws.** SVF Simper comme brique unique de tous les filtres et EQ : **S**. Pultec par approximation SVF calée sur les courbes de [33], avec ×2 : **M**. Baxandall TPT d'ordre 1 : **S**.

---

## 5. Réverbes

**FDN de Jot [34][35].**
- **Structure** : N = 8 ou 16 lignes de longueurs m_i premières entre elles, réparties de façon à peu près exponentielle entre 20 et 100 ms. Matrice orthogonale :
  - Hadamard normalisée 1/√N (N puissance de 2, coût O(N log N)) ;
  - ou Householder I − (2/N)·11ᵀ (coût O(N)).
- **Absorption** : g_i = 10^{−3·m_i/(fs·T60)}. Pour un T60 qui dépend de la fréquence, filtre à un pôle par ligne H_i(z) = g_i(1 − b_i)/(1 − b_i·z⁻¹), avec b_i = (ln 10 / 4)·log10(g_i)·(1 − 1/a²) et a = T60(Nyquist)/T60(0), selon PASP [35] *(formule de mémoire, à revérifier)*. Ajouter un filtre de correction de timbre en sortie.
- **Modulation** : 2 à 4 lignes modulées de ±0,3 à 2 ms, à 0,1 à 1 Hz, avec interpolation allpass ou cubique. Cela casse le caractère métallique.
- **Diffusion d'entrée** : 4 allpass de Schroeder en série (g = 0,5 à 0,7, longueurs de 3 à 15 ms).

**Plaque de Dattorro [36]** (fs de référence 29 761 Hz, longueurs à mettre à l'échelle fs/29761) :
- Prédelay, puis passe-bas « bandwidth » (0,9995).
- 4 diffuseurs d'entrée : 142, 107, 379 et 277 échantillons, coefficients 0,75, 0,75, 0,625 et 0,625.
- Réservoir à deux branches croisées. Chaque branche enchaîne :
  - un allpass modulé (672 / 908, coefficient −0,7, excursion d'environ 16 échantillons) ;
  - un délai (4453 / 4217) ;
  - un amortissement passe-bas ;
  - le gain « decay » (≈ 0,5) ;
  - un allpass (1800 / 2656, coefficient 0,5) ;
  - un délai (3720 / 3163).
- Sorties par prises multiples : table du papier.
- *Valeurs citées de mémoire, à confronter au PDF [36].*

**Premières réflexions** : ligne à 6 à 12 prises entre 5 et 80 ms, gains décroissants, alternées gauche/droite (ou issues de la méthode des images sur une pièce rectangulaire), filtrées en passe-bas.

**Pièges.** Dénormaux (activer ScopedNoDenormals) ; gain de boucle ≤ 1 lors des changements de T60 ; lignes trop courtes, qui produisent un son « boîte » ; pitch audible si la modulation est trop forte sur les notes tenues ; DC qui s'accumule dans la boucle.

**Ce que je ferais dans dreamaws.** FDN 8 lignes Householder + absorption à un pôle + 4 diffuseurs + 2 lignes modulées : **M**. Plaque Dattorro en algorithme distinct : **S**. Premières réflexions en prises fixes : **S**.

---

## 6. De-esser, EQ dynamique, doublage, throw delays

**De-esser / EQ dynamique** (bases dans Zölzer, DAFX [37]) :
- **Side-chain** : passe-bande ou passe-haut entre 4 et 10 kHz. Détection relative (heuristique) : rapport énergie de la bande / énergie large bande, pour un seuil indépendant du niveau d'entrée.
- **Action** : une cloche SVF dont le gain est piloté par échantillon (le SVF supporte la modulation). Variante en bande séparée : crossover Linkwitz-Riley, gain appliqué au haut seulement.
- **Temps** : attaque de 0,5 à 2 ms, relâche de 30 à 80 ms, lookahead de 1 à 3 ms (à déclarer en latence).
- **EQ dynamique générale** : même moteur que le compresseur du §2, avec un gain de cloche en sortie.

**Doublage « seconde prise »** (heuristique) : 1 ou 2 voix retardées de 10 à 35 ms.
- Variation de timing par bruit lissé (0,5 à 3 Hz, ±3 à 8 ms) avec interpolation cubique. Cela produit aussi une micro-variation de hauteur, puisque la dérivée du retard modifie la hauteur.
- Désaccord de ±5 à 15 cents via le moteur du §1, ou laissé à la seule modulation.
- EQ légèrement différente, voix panoramisées.
- Pour éviter le flanger : retard ≥ 10 ms, et modulations indépendantes et non périodiques.

**Throw delay de fin de phrase** (heuristique) :
- **Détection** : enveloppe RMS de 20 à 50 ms. Une phrase est « active » si le niveau dépasse seuil + 6 dB pendant plus de 300 ms. La fin est détectée quand le niveau reste sous le seuil plus de 80 à 150 ms, avec un seuil relatif au niveau moyen de la phrase.
- **Action** : ouvrir l'envoi vers le delay sur les derniers N ms (mémoire tampon), ou plus simplement un **ducking** (niveau du delay = 1 − enveloppe de la voix), technique éprouvée et sans risque de fausse détection.

**Ce que je ferais dans dreamaws.** De-esser à cloche SVF dynamique avec détection relative : **S**. Doubleur à retards modulés par bruit lissé : **S**. Throw delay = ducking + mode « fin de phrase » expérimental : **M**.

---

## 7. Basse 808

**Référence circuit** : modèle physique du TR-808 (résonateur en T ponté excité par impulsion) par Werner, Abel et Smith [38]. En trap, l'usage réel est une synthèse simple.

**Structure** :
- **Oscillateur sinus** à accumulateur de phase, phase remise à 0 à la note (attaque nette) sauf en legato.
- **Enveloppe de pitch** : f(t) = f_fin + (f_début − f_fin)·e^{−t/τ_p}, avec τ_p de 5 à 30 ms et un départ de +12 à +24 demi-tons.
- **Glide** en domaine log : s ← s + (cible − s)·(1 − e^{−T/τ_g}), avec τ_g de 30 à 150 ms. Variante : glide à durée fixe linéaire en demi-tons.
- **Enveloppe d'amplitude** : décroissance exponentielle de 0,3 à 2 s, ou ADSR.
- **Saturation APRÈS l'enveloppe** : tanh avec drive de 1 à 10, en ADAA1. Les harmoniques suivent alors l'enveloppe et disparaissent dans la queue, d'où l'audibilité sur petits haut-parleurs.
- **Sortie** : mélange parallèle avec la sinus propre pour garder la fondamentale, filtre anti-DC (passe-haut ≈ 10 Hz), passe-bas optionnel.

**Pièges.** Aliasing du drive sur les notes aiguës (ADAA + ×2) ; DC produit par une saturation asymétrique ; clic si la phase est remise à zéro pendant une note qui sonne encore (faire un fondu de 1 à 2 ms) ; glide calculé en Hz au lieu de demi-tons (rendu non musical).

**Ce que je ferais dans dreamaws.** Oscillateur, enveloppes pitch/amp, glide log, tanh ADAA post-enveloppe, mélange de sub propre : **S**. Mode « circuit 808 » [38] en option : **M**.

---

## Ce qui n'a pas pu être vérifié

- GitHub n'était pas accessible depuis cette session. Les licences viennent de pages miroirs : ChowTape GPLv3 et chowdsp_utils (modules mixtes) via ecosyste.ms, Signalsmith MIT via la documentation Qt.
- Non vérifiés : licence de chowdsp_wdf, licence du plugin Vamp pYIN, licence ISC de librosa.
- Valeurs citées de mémoire et non confrontées aux PDF : constantes du 1176 et du 670, réglages par défaut de librosa.pyin, longueurs de Dattorro, paramètres Dempwolf du 12AX7, formule b_i de Jot.
- Le modèle FET et le modèle vari-mu reposent sur des heuristiques : aucune publication revue n'a été trouvée.
- Les sections doublage et throw delay sont des pratiques d'ingénierie, pas des algorithmes publiés.

---

## Sources

1. [R] de Cheveigné & Kawahara, « YIN, a fundamental frequency estimator », JASA 111(4), 2002.
2. [R] McLeod & Wyvill, « A smarter way to find pitch », ICMC 2005.
3. [R] Mauch & Dixon, « pYIN », ICASSP 2014 — https://code.soundsoftware.ac.uk/publications/107
4. [R] Moulines & Charpentier, « Pitch-synchronous waveform processing techniques… », Speech Communication, 1990.
5. [R] Verhelst & Roelands, « WSOLA », ICASSP 1993.
6. [R] Laroche & Dolson, « Improved phase vocoder time-scale modification of audio », IEEE TSAP 1999 — https://seniordesign.cs.tcu.edu/1516Ribbit/resources/ImprovedPhaseVocoder.pdf
7. [R] Průša & Holighaus, « Phase Vocoder Done Right » — https://arxiv.org/pdf/2202.07382
8. [R] Röbel & Rodet, « Efficient spectral envelope estimation and its application to pitch shifting and envelope preservation », DAFx 2005 — https://dafx.de/paper-archive/details/EDSTSaO17Gd_0LV03iwDfw
9. [R] Mauch et al., « Computer-aided melody note transcription using the Tony software » — https://code.soundsoftware.ac.uk/publications/147
10. [D] Brevet US 5 973 252 (Hildebrand), expiré — https://patents.google.com/patent/US5973252
12. [D] Signalsmith Stretch, licence MIT — https://doc.qt.io/QT-6/qtmultimedia-attribution-signalsmith-stretch.html
13. [R] Giannoulis, Massberg & Reiss, « Digital Dynamic Range Compressor Design — A Tutorial and Analysis », JAES 60(6), 2012 — https://www.aes.org/e-lib/browse.cfm?elib=16354
14. [R] Najnudel, Müller, Hélie & Roze, « Power-Balanced Dynamic Modeling of Vactrols », DAFx 2023 — https://www.dafx.de/paper-archive/2023/DAFx23_paper_50.pdf
15. [R] Eichas & Zölzer, « Modeling of an optocoupler-based audio dynamic range control circuit », Proc. SPIE 9948, 2016 — https://www.hsu-hh.de/ant/wp-content/uploads/sites/699/2017/10/Eichas-Modeling-of-an-optocoupler-based-audio-dynamic-range-control-circuit-99480W.pdf
16. [R] Wright & Välimäki, « Grey-box modelling of dynamic range compression », DAFx20in22 — https://acris.aalto.fi/ws/portalfiles/portal/89253381/Wright_et_alii_GREY_BOX_MODELLING_OF_DYNAMIC_RANGE_COMPRESSION.pdf
17. [R] Moore, thèse (Huddersfield) — https://eprints.hud.ac.uk/id/eprint/34118/1/FINAL%20THESIS%20-%20MOORE.pdf
18. [R] Werner, « Virtual analog modeling of audio circuitry using wave digital filters », thèse, Stanford, 2016 (URL non relevée).
19. [R] Parker, Zavalishin & Le Bivic, « Reducing the aliasing of nonlinear waveshaping using continuous-time convolution », DAFx 2016 — https://dafx.de/paper-archive/details/vem_XXF5qBbfiWOH2RVVAA
20. [R] Bilbao, Esqueda, Parker & Välimäki, « Antiderivative antialiasing for memoryless nonlinearities », IEEE SPL 2017 — https://www.pure.ed.ac.uk/ws/files/26997332/07499828.pdf
21. [B] Koren, « Improved vacuum tube models for SPICE » (normandkoren.com, non revisité).
22. [R] Dempwolf & Zölzer, « A physically-motivated triode model for circuit simulations », DAFx 2011 (non revisité).
23. [R] Pakarinen & Yeh, « A review of digital techniques for modeling vacuum-tube guitar amplifiers », Computer Music Journal, 2009.
24. [R] Chowdhury, « Real-Time Physical Modelling for Analog Tape Machines », DAFx 2019 — https://dafx.de/paper-archive/2019/DAFx2019_paper_3.pdf
25. [B] Simper (Cytomic), « SvfLinearTrapOptimised2 » ; formules via simper-filter (MIT/Apache-2.0) — https://docs.rs/simper-filter/0.1.0/src/simper_filter/lib.rs.html
26. [R] Holters & Zölzer, « Circuit simulation with inductors and transformers based on the Jiles-Atherton model », DAFx 2016 (non revisité).
27. [D] AnalogTapeModel (GPLv3) — https://awesome.ecosyste.ms/projects/github.com%2Fjatinchowdhury18%2Fanalogtapemodel
28. [D] chowdsp_utils (licences par module) — https://awesome.ecosyste.ms/projects/github.com%2FChowdhury-DSP%2Fchowdsp_utils
29. [D] librosa.pyin — https://librosa.org/doc-playground/0.8.1/generated/librosa.pyin.html
30. [D] Zavalishin, « The Art of VA Filter Design », Native Instruments (livre gratuit).
31. [B] Vicanek, « Matched Second Order Digital Filters », 2016 (vicanek.de) ; voir aussi https://www.vicanek.de/articles/AADistortion.pdf
32. [R] Orfanidis, « Digital parametric equalizer design with prescribed Nyquist-frequency gain », JAES 1997.
33. [R] Barrera, Lizarraga-Seijas & Font, « Modeling the Pultec EQP-1A with Wave Digital Filters », SMC 2024 — https://smcnetwork.org/smc2024/papers/SMC2024_paper_id132.pdf
34. [R] Jot & Chaigne, « Digital delay networks for designing artificial reverberators », AES 1991 ; Schlecht & Habets, « On Lossless FDNs » — https://arxiv.org/pdf/1606.07729
35. [D] J. O. Smith, « Physical Audio Signal Processing », CCRMA (section FDN / T60).
36. [R] Dattorro, « Effect Design Part 1 », JAES 1997 — https://ccrma.stanford.edu/%7Edattorro/EffectDesignPart1.pdf
37. [D] Zölzer (éd.), « DAFX — Digital Audio Effects », Wiley, 2e éd. 2011.
38. [R] Werner, Abel & Smith, « A physically-informed, circuit-bendable, digital model of the Roland TR-808 bass drum circuit », DAFx 2014 (non revisité).
