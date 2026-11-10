# Audit 3 : Orbe DeEsser et Orbe Denoiser

10 novembre 2026, branche `vague2-wip`. Points de comparaison : FabFilter Pro-DS et Waves Sibilance pour le de-esser ; iZotope RX Voice De-noise / Spectral De-noise, Accusonus ERA et Cedar pour le débruiteur.
Bancs hors ligne sans JUCE (`g++ -O2`, un seul cœur) dans `/tmp/b_dsaudit` : `sig.h` (signaux), `des_bench.cpp`, `dn_bench.cpp`, `dn_tw.cpp` (variantes), `extra.cpp`.
Ce rapport ne reprend pas ce qui est déjà ouvert dans l'audit 2 et l'audit 3 (seuil absolu, course linéaire de `freq`, zipper M2, M4 sur la fréquence d'échantillonnage). Il donne seulement les mesures qui les confirment.

## Signaux et mesures

- **Voix synthétique** : train d'impulsions glottiques à f0 ≈ 130 Hz, avec vibrato de 5,5 Hz (±1,2 %) et jitter. Pente de −12 dB/oct, 3 formants qui changent à chaque voyelle (a, è, i, o, ou), souffle aspiré à −32 dB.
- **Consonnes** : « s » (bruit de 4,5 à 11 kHz, pic à 7 kHz), « ch » (pic à 3,6 kHz), « f » (1,5 à 12 kHz, faible), plosive grave de 20 ms, silences de 150 à 200 ms.
- **Niveaux et fréquences** : crête à −40, −24, −12 et −6 dBFS, à 44,1, 48 et 96 kHz.
- **Bruits** : blanc, rose, ventilateur (rose filtré + raies à 118 et 236 Hz), ronflement 50 Hz (7 harmoniques), pièce (brownien filtré à 400 Hz + souffle). Rapport signal/bruit (RSB) de 10 et 25 dB sur les voyelles.
- **Mesures du de-esser** :
  - réduction de bande sur les « s » (4,5 à 12 kHz), les « ch » (2,5 à 6 kHz) et les « f » ;
  - perte de naturel : bande 4 à 10 kHz sur les voyelles, hors 80 ms autour des consonnes ;
  - réduction de gain maximale pendant les voyelles (fausse détection).
- **Mesures du débruiteur** :
  - réduction du bruit seul, mesurée de 80 Hz à 16 kHz ;
  - bruit musical : log du rapport de kurtosis sortie/entrée sur les pauses (mesure d'Uemura ; 0 = résidu aussi stationnaire que le bruit d'entrée) ;
  - distance log-spectrale (LSD) entre la voix propre et la sortie ;
  - RSB de forme d'onde sur les voyelles ;
  - pertes de bande sur les voyelles.

## Correctifs DSP faits (testés, prêts pour la livraison de 15 h)

Chaque correctif a un test de non-régression dans `deesser_test.cpp` et `denoiser_test.cpp`. Ces tests **échouent sur l'ancien code et passent sur le nouveau** (vérifié en remettant temporairement les anciens en-têtes). Les deux tests console passent en entier : « TOUT EST BON », 0 allocation dans le temps réel, bypass exact, latence inchangée.

| # | Plugin | Défaut | Fichier | Avant → après |
|---|---|---|---|---|
| F1 | DeEsser | Un seul NaN ou infini en entrée → sortie **NaN pour toujours** (biquads, enveloppe, lookahead empoisonnés). Le G2 de l'audit 2 était toujours ouvert ici : le processor n'appelle pas `kit::sanitize`. | `DeEsserDsp.h:29` (entrée remplacée par 0 si non finie, test sur les bits) | 46 000 échantillons NaN sur 46 000 → 0 |
| F2 | Denoiser | Même défaut. L'audit 2 disait que le Denoiser « résiste » : c'est faux. Un NaN rend `P`, `sm`, `aPrev` et `gPrev` NaN, et la sortie reste NaN. | `DenoiserDsp.h`, `tick()` | 96 000 sur 96 000 NaN après 1 s → 0 |
| F3 | Denoiser | **La garde de tonalité (B2) sous-estime le bruit grave en pente.** Le premier quart d'une fenêtre unique de ±16 bins, sur une pente de 15 dB (bruit de pièce, grondement, clim), prend le niveau du côté bas. Le plancher est donc trop bas et presque rien n'est retiré. Nouvelle garde : premier quart **de chaque côté** (k−16..k−2 et k+2..k+16), et on retient le côté le plus fort. Une raie a ses deux côtés bas, elle reste protégée ; une pente a toujours un côté haut. | `DenoiserDsp.h`, `sideQuartile()` et boucle des minima | Bruit de pièce, 18 dB demandés : **6,1 dB → 16,9 dB** (test) ; banc : 3,2 → 17,3 dB à 48 kHz, 17,5 dB à 44,1 kHz. Note tenue de 7 s avec vibrato : −0,08 dB (inchangé). Blanc, rose, ventilateur : inchangés à ±0,2 dB. |
| F4 | Denoiser | L'apprentissage du profil ne lisait que la voie gauche (M4, 2e moitié). | `DenoiserDsp.h:136` | Gauche muette et bruit à droite : profil nul → profil correct |
| F5 | Denoiser | `int count` débordait après 12,4 h, ce qui est un comportement indéfini (M5). | `Chan::hopPos` modulo `kHop`, `frames` saturé | Pas de test de 12 h ; logique de la première trame inchangée |

**Effet sur le son** :
- F1, F2, F4 et F5 ne changent rien en usage normal.
- F3 change le son seulement quand le bruit est grave et en pente : le débruiteur retire alors enfin ce qu'on lui demande.
- La garde B2 est arrivée dans `vague2-wip` et n'a jamais été livrée. Ce correctif n'appelle donc **pas** de version d'état.
- CHANGELOG mis à jour (sections Orbe DeEsser et Orbe Denoiser).

**Le processor n'est pas compilé ici** (pas de build JUCE). Aucun fichier processor ni éditeur n'a été modifié : les correctifs sont tous dans les en-têtes DSP. Aucun identifiant de paramètre n'a changé.

---

## Orbe DeEsser (`DeEsserDsp.h`, moteur `des::Engine`)

**Algorithme réel** :
- La détection passe par un passe-bande RBJ (`freq`, `width` en octaves), puis un détecteur crête (attaque 0,2 ms, retour 4 ms) sur max(|G|, |D|).
- Le seuil est **absolu** (dBFS), avec un coude doux de 8 dB à 4:1 (style Doux) ou un coude de 0,5 dB à 20:1 (style Net).
- La réduction est lissée en dB (attaque et relâche réglables), avec un lookahead fixe de 1 ms.
- Le mode Split atténue la bande de détection : `d + (g−1)·BP(d)`, une cloche dynamique transparente à g = 1.
- L'écoute envoie la bande de détection.

**La brique `Dsp/DeEssDyn.h` n'est pas utilisée par ce plugin** : elle ne sert qu'aux un-potard. Elle est mesurée plus bas.

### Mesures (réglages d'usine : 6,5 kHz, 1,5 oct, −30 dBFS, plage 10 dB, Doux, Split)

| Crête voix | « s » | « ch » | « f » | Voyelles 4 à 10 kHz | Réduction max sur voyelle | Réduction max sur « s » |
|---|---|---|---|---|---|---|
| −40 dBFS | 0,0 dB | 0,0 | 0,0 | 0,00 | 0,0 | 0,0 |
| −24 dBFS | 0,0 dB | 0,0 | 0,0 | 0,00 | 0,0 | 0,0 |
| −12 dBFS | 3,2 dB | 0,7 | 0,0 | −0,02 | 0,4 | 5,3 |
| −6 dBFS | 5,3 dB | 2,0 | 0,0 | −0,05 | 0,9 | 9,7 |
| −6 dBFS, voix sans « s » (« i » brillant) | n/a | n/a | n/a | **−1,28** | **3,4** | n/a |

- Les résultats sont identiques à ±1 dB à 44,1 et 96 kHz.
- Seuil nécessaire pour retirer 4 dB sur les « s » : −51 dB pour une voix à −30 dBFS, −39 dB à −18 dBFS, −27 dB à −6 dBFS. Le seuil suit le niveau, dB pour dB.

### Défauts, par gravité

1. **[P0, corrigé : F1] NaN permanent.** Voir le tableau des correctifs.
2. **[P1] Seuil absolu (dépend du niveau).**
   - Confirmé, et chiffré ci-dessus : aux réglages d'usine, une voix à −24 dBFS crête ne subit **aucun** de-essing, et une voix à −6 dBFS subit des faux déclenchements sur les voyelles brillantes (3,4 dB, −1,3 dB d'air perdu).
   - Pro-DS et Sibilance détectent par rapport au reste du signal.
   - Cause : `DeEsserDsp.h:33` (`over = lin2db(env) − thr`).
   - Correctif proposé (M, 1 j) :
     - détection relative reprise de `DeEssDyn.h` (rapport bande sibilante / corps), avec `thr` en « dB au-dessus du corps » ;
     - nouveau paramètre en **fin de liste** : `detect` (Absolu/Relatif) ;
     - version d'état : un état sans version rouvre en Absolu, une nouvelle instance démarre en Relatif ;
     - il faut un contrôle dans l'éditeur, donc l'agent qui migre les éditeurs.
   - Je ne l'ai pas fait : il touche le processor et l'éditeur, et change le son.
3. **[P1] La cloche ne couvre pas toute la sibilance.**
   - Avec 1,5 oct centrée à 6,5 kHz (4,6 à 9,2 kHz), un « s » qui monte à 11 kHz n'est réduit que de **5,3 dB pour 9,7 dB de réduction affichée**. Le compteur ment de 4 dB.
   - Pro-DS, en mode « Split », coupe tout ce qui est au-dessus de la fréquence.
   - Cause : `DeEsserDsp.h:50` et `:59` (la même cloche sert à détecter et à atténuer).
   - Correctif (S, ½ j) : en Split, atténuer avec un plateau aigu à `freq·0,85`. Le plateau SVF modulable de `DeEssDyn.h` est transparent à 0 dB. La bande de détection reste réglable.
   - Il change le son : nouvelle option de mode en fin de liste (« Split plateau »), ou version d'état.
4. **[P2] Le « ch » est à peine traité** (0,7 à 2,8 dB) avec la fréquence par défaut. C'est normal pour un de-esser réglé sur le « s ». La course `freq` commence à 4 kHz : il faudrait l'ouvrir à 2,5 kHz pour les « ch » et « j » français (S, avec la course log déjà demandée dans `02-reglabilite`).
5. **[P2] Le lookahead de 1 ms ne sert à rien dès que l'attaque dépasse 1 ms** (0,05 à 20 ms). Lier le lookahead à l'attaque (≤ 5 ms) changerait la latence. À garder en tête, pas urgent.
6. **[P2] Dépassement en sortie.** Un carré à 0 dBFS ressort à **+1,05 dBFS** (rotation de phase de la cloche). Rien à faire en dehors d'une note dans le manuel ; un plateau aigu (point 3) dépasse moins.
7. **[P3] L'écoute envoie la bande de détection, pas ce qui est retiré.** Pro-DS propose les deux. Proposition : un mode « Delta » qui envoie `(1−g)·BP(d)` (S).
8. **[P3] Ouvert, déjà signalé** : coefficients recalculés sans lissage quand on bouge `freq` ou `width` (zipper, M2) ; course de `freq` linéaire.

### La brique `DeEssDyn.h` (un-potard), pour comparaison

| | « s » | « ch » | Voyelles 4 à 10 kHz | Réduction max sur voyelle |
|---|---|---|---|---|
| −40 à −6 dBFS, 44,1, 48 et 96 kHz | **6,3 à 6,7 dB** | 0,3 | −0,12 à −0,17 dB | 1,8 à 2,3 dB |
| Voix sans « s », −6 dBFS | n/a | n/a | −0,10 dB | 1,1 dB |

- C'est bien une détection relative : **elle ne dépend pas du niveau** (à 0,1 dB près entre −40 et −6 dBFS). Elle laisse les voyelles intactes et résiste aux NaN (`sane()`).
- Le début du « s » passe sans être réduit : 0 dB sur les 2 premières ms, 3,9 dB à 5 ms, 6,4 dB à 10 ms. Il n'y a pas de lookahead, par choix (latence nulle), et le lissage d'énergie dure 3 ms (`DeEssDyn.h:77-78`).
  - Option à latence nulle : passer `eC` à 1 ms (S, à mesurer contre le trémolo sur les voyelles).
  - Ou ajouter un lookahead de 2 ms quand l'hôte compense déjà la latence.
- Le « ch » n'est pas touché à 5,5 kHz (même remarque qu'au point 4).
- **C'est la base recommandée pour l'Orbe DeEsser** (point 2).

---

## Orbe Denoiser (`DenoiserDsp.h`, moteur `dn::Engine`)

**Algorithme réel** :
- **Analyse** : STFT de 2048 points avec saut de 512 (fenêtre racine de Hann, recomposition WOLA). La latence est de 2048 échantillons à toutes les fréquences d'échantillonnage : 46 ms à 44,1 kHz, 21 ms à 96 kHz.
- **Estimation du bruit** : minima statistiques sur la puissance lissée (8 sous-fenêtres de 0,25 s, `N = 2·min`), avec garde de tonalité. Avec un profil appris, l'adaptatif est borné entre le profil et le profil +6 dB.
- **Gain** : Wiener à RSB a priori « dirigé par la décision » (β = 0,90 + 0,09·Lissage), sur-soustraction α = 1 + 2·Seuil, plancher −Réduction (moins profond dans les aigus avec « Aigus »).
- **Lissage du gain** : en fréquence d'abord (noyau [¼ ½ ¼], une ou deux passes), puis dans le temps (attaque 0,85 par trame, retour 0,6 à 0,15).
- **Ronflement** : 4 encoches RBJ à Q = 20, avant la STFT.
- **Écoute** : entrée retardée moins la sortie. C'est exact : sortie + retiré = entrée retardée, à −165 dB.

### Mesures (réglages d'usine : 18 dB, Seuil 50 %, Lissage 50 %, Aigus 25 %, adaptatif, après correctifs)

| Bruit (RSB 10 dB) | Bruit seul retiré, 80 Hz à 16 kHz | Bruit musical (log kurtosis) | LSD voyelles, entrée → sortie | RSB voyelles, entrée → sortie |
|---|---|---|---|---|
| Blanc | 15,9 dB | **1,47** | 8,3 → 11,1 dB | 10,0 → 14,4 |
| Rose | 17,0 dB | **1,56** | 7,1 → 12,3 | 9,9 → 11,3 |
| Ventilateur | 17,6 dB | **1,79** | 3,4 → 12,4 | 9,9 → 11,1 |
| Pièce | 17,3 dB (avant F3 : 3,2 dB) | **1,87** | 1,9 → 10,2 | 9,8 → 11,7 |
| Ronflement, sans anti-ronflement | n/a | 1,77 | 4,5 → 13,1 | 10,0 → 12,3 |
| Ronflement, anti-ronflement 50 Hz | n/a | 1,77 | 4,5 → 13,1 | 10,0 → 9,6 |

**Voix presque propre** (bruit blanc, 40 dB de RSB), réglages d'usine :
- RSB de forme d'onde sur les voyelles : **40 → 22,7 dB** ;
- bande 2 à 5 kHz sur les voyelles : **−1,2 dB** ;
- avec 10 dB de RSB, la perte de 2 à 5 kHz monte à **−2,8 dB** ;
- avec 25 dB de RSB, le RSB **baisse** : 25 → 20 dB, le traitement abîme plus qu'il n'enlève.

À 96 kHz :
- le bruit de pièce ne perd que 12,1 dB ;
- le bruit musical sur bruit rose tombe à 0,89 : les bins sont deux fois plus larges en Hz, le comportement change.

Entrées limites (après correctifs) :
- un NaN est absorbé ;
- un silence numérique ressort en silence exact ;
- un carré à 0 dBFS ressort à +0,54 dBFS crête ;
- 0 allocation dans le temps réel.

### Défauts, par gravité

1. **[P0, corrigés : F2, F3]** NaN permanent ; bruit grave non retiré (garde de tonalité).
2. **[P1] Bruit musical marqué aux réglages d'usine.**
   - Le log du rapport de kurtosis vaut 1,5 à 2,2 sur tous les bruits. Le résidu « gazouille ».
   - Les références (RX, ERA, Cedar) donnent un résidu presque stationnaire, de l'ordre de 0,3 à 0,6 avec cette mesure. C'est une estimation de ma part : je n'ai pas mesuré ces produits.
   - Causes : β « dirigé par la décision » bas par défaut (0,945 au lieu de 0,98, `DenoiserDsp.h:142`) ; attaque du gain presque instantanée (0,85 par trame, `:174`) ; plancher dur `max(g, floorG)` au lieu d'un plancher sur ξ.
   - Variantes essayées (`dn_tw.cpp`) :

     | Variante | Log kurtosis, blanc RSB 10 | Coût |
     |---|---|---|
     | Réglage actuel | 1,47 | n/a |
     | β 0,98 et attaque 0,5 | 0,82 | Rose : 1,56 → 1,08 ; le RSB des voyelles perd 0,5 à 1,5 dB |
     | Avec ξ_min en plus | Aucun gain mesurable | n/a |

   - Ces réglages ne suffisent pas : il faut changer d'estimateur.
   - Correctif proposé (L, 2 à 3 j) :
     - MMSE-LSA (Ephraim-Malah 1985) avec probabilité de présence de parole (IMCRA, Cohen 2003) : un algorithme publié, sans code GPL ;
     - ou, plus simple, un lissage cepstral du gain (Breithaupt et Martin 2008).
   - Ces correctifs changent le son : il faut une version d'état (`algo` en fin de liste, un état sans version reste en v1).
3. **[P1] La voix perd de l'éclat et de la présence** (−1,2 à −2,8 dB de 2 à 5 kHz sur les voyelles, RSB de forme d'onde plafonné à environ 23 dB même sans bruit).
   - Cause probable : les harmoniques aigus vibrés (vibrato ±1,2 %, soit 1 à 2,5 bins entre 2 et 5 kHz) traversent les bins plus vite que le RSB a priori ne suit. Le noyau de lissage [¼ ½ ¼] (`:167-171`) étale ensuite le plancher sur les pics.
   - **Correctif mesuré** (S, ½ j) : dilater le gain sur ±1 bin (prendre le max des voisins) avant le lissage fréquentiel, comme le font les débruiteurs musicaux pour protéger les partiels.

     | Signal | Mesure | Actuel | Avec dilatation |
     |---|---|---|---|
     | Blanc, RSB 10 | RSB voyelles en sortie | 14,4 dB | **16,2 dB** |
     | Blanc, RSB 10 | Perte 2 à 5 kHz | −2,8 dB | **−1,15 dB** |
     | Blanc, RSB 40 | RSB voyelles en sortie | 22,7 dB | **25,9 dB** |
     | Ventilateur, RSB 25 | LSD | 8,9 dB | **7,1 dB** |
     | Pièce, RSB 25 | RSB voyelles | 24,8 → 21,7 dB | 24,8 → **24,2** dB |

     Coût sur la réduction du bruit seul : 0,3 à 1,4 dB de moins. Le bruit musical ne change pas.
   - Il change le son : je ne l'ai pas activé. À livrer avec le point 2, derrière `algo` = 2.
4. **[P1] Anti-ronflement.**
   - Encoches à Q = 20 sur les seuls harmoniques 1 à 4 (`:76`). Un ronflement secteur réel a des harmoniques impairs jusqu'à 7 ou 9 (250, 350, 450 Hz) : ils restent.
   - Les encoches tournent la phase autour de f0 : le RSB de forme d'onde sur les voyelles passe de 25 à **10,6 dB** à 25 dB de RSB. C'est peu audible, mais une voix grave prend une légère coloration vers 100 à 200 Hz.
   - Correctif (M, 1 j) : retirer les raies dans la STFT (mettre à zéro les bins de 50·k Hz, ±1 bin, k ≤ 16, et suivre la fréquence par interpolation du pic), ou un peigne de 8 à 10 encoches à Q ≥ 40.
5. **[P2] Le profil est lié à la fréquence d'échantillonnage** (M4, 1re moitié, toujours ouvert).
   - Le processor (`DenoiserProcessor.cpp:45-58`) enregistre 1025 bins sans la fréquence.
   - Correctif (S) : propriété `profileSr`, profil ignoré s'il ne correspond pas.
6. **[P2] Le comportement dépend de la fréquence d'échantillonnage** : `kN` est fixe à 2048 (`:7`). À 96 kHz : résolution de 47 Hz, réduction de 12 dB sur le bruit de pièce au lieu de 17.
   - Correctif (M) : `kN` = 4096 à 88,2 et 96 kHz. La latence et les tableaux changent, donc l'allocation se fait dans `prepare`.
7. **[P2] Pendant l'apprentissage (2 s), le gain vaut 1** (`:157`) : le débruitage s'arrête net puis repart. RX garde le traitement adaptatif pendant la capture. Correctif (S) : pendant l'apprentissage, garder le chemin adaptatif.
8. **[P3] Réglages et unités.**
   - « Seuil » (`sens`) est en % alors qu'il règle une sur-soustraction de 0 à +4,8 dB. Afficher « +x dB », ou le renommer « Sensibilité ».
   - « Lissage » règle à la fois β et le retour du gain, et passe la vitesse du noyau de 1 à 2 passes à 66 % (saut discret).
   - Aucun auto-seuil : l'adaptatif en tient lieu.
   - L'affichage est correct.

## Récapitulatif des correctifs proposés (non faits)

| Priorité | Plugin | Correctif | Effort | Impact | Change le son ? |
|---|---|---|---|---|---|
| 1 | DeEsser | Détection relative (`DeEssDyn`), `detect` en fin de liste, version d'état | M | Fort : de-essing à tout niveau, sans faux déclenchement | Oui |
| 2 | DeEsser | Split en plateau aigu (couvre jusqu'à 20 kHz, compteur honnête) | S | Moyen | Oui |
| 3 | Denoiser | Dilatation du gain sur ±1 bin (mesurée ci-dessus) | S | Moyen : voix plus claire, +1,5 à 3 dB de RSB | Oui |
| 4 | Denoiser | MMSE-LSA + IMCRA, ou lissage cepstral (bruit musical) | L | Fort | Oui |
| 5 | Denoiser | Anti-ronflement dans la STFT, 16 harmoniques | M | Moyen | Oui |
| 6 | Denoiser | Fréquence enregistrée avec le profil | S | Bug | Non |
| 7 | Denoiser | `kN` adapté à la fréquence d'échantillonnage | M | Faible à moyen | Oui à 88,2 et 96 kHz |
| 8 | DeEsser | Mode Delta, `freq` dès 2,5 kHz en course log | S | Ergonomie | Non |
| 9 | Denoiser | Adaptatif conservé pendant l'apprentissage | S | Faible | Non |

Les points 1 à 5 doivent passer par une version d'état. Il n'en existe pas encore pour ces deux plugins : `FxProcessor` (`OrbeFace.h`) n'en a pas. Il suffit d'une propriété `dspVer` écrite dans `beforeSave()` et lue dans `afterLoad()` (absente = 1), comme pour les un-potard.
