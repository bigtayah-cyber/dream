# Audit n°3 : les 4 plugins à un potard, vus par un concepteur de plugins à un potard

Périmètre : `Source/OneKnob/OneKnob.h`, `OneKnobProcessor.*` et le moteur partagé `Source/Engine.h`. Aucun fichier source modifié.
Bancs jetables dans `/tmp/au3` : `onek::Chain` compilée seule, à 44,1 kHz. Signaux : la prise rap (`prise.wav`, −26 dBFS RMS), la même entrecoupée de pauses réelles, des sinus à 220 et 3 150 Hz, et une mélodie chantée de synthèse (la mineur, ±35 cents d'erreur, glissando).

## Verdict

Le reproche « bâclé » est **fondé sur l'algorithme**, pas seulement sur l'interface. Les quatre plugins sont **un seul moteur (Voile) habillé quatre fois**. Chaque voicing est une fonction linéaire qui pousse les mêmes macros (tension, mordant, brume, halo), un compresseur et deux plateaux d'égalisation. Le potard n'est pas non plus « bon à toute position » :
- à 0, rien n'est transparent ;
- la compression ne suit presque pas le potard ;
- la course a des marches audibles ;
- l'auto-entrée remonte le bruit dans les pauses ;
- les « doubles » sont des harmonies granulaires qui grésillent ;
- 17 ms de latence partout ;
- la correction de hauteur ne s'enclenche jamais sur du rap.

À garder : niveau stable sur une prise continue (±0,4 dB sur toute la course), mode « sûr » de la hauteur, saturation suréchantillonnée ×8, limiteur tanh ADAA, zéro allocation, environ 4,5 % d'un cœur processeur.

## 1. Mesures transverses

| # | Constat | Mesure |
|---|---|---|
| T1 | **k = 0 n'est pas neutre** | Cohérence entrée/sortie de 0,89 à 0,90 sur toute la bande (environ 10 % d'énergie non corrélée). À 0 il reste de la reverb, du delay, de la saturation, une compression de 0,25 à 0,40 et des harmonies. |
| T2 | **La compression ne suit pas le potard** | De 0 à 10, l'écart-type du niveau sur 100 ms passe seulement de 2,2 à 1,7 dB, et la crête de −2 dB. Compresseur parallèle plafonné à 60 % de signal traité, donc au plus environ 8 dB de réduction, déjà engagée à 0. |
| T3 | **L'auto-entrée pompe dans les pauses** | Le bruit de pièce (−53 dBFS) passe le seuil fixe de −55 dBFS. Le gain d'entrée monte de +10 à +17 dB pendant une pause. Bruit entre les phrases : **+9,5 dB** (−53 → −43,6). Dans la séquence avec pauses, les phrases sortent jusqu'à **+12 dB** au-dessus de l'entrée : l'auto-gain, borné à −9 dB, ne rattrape pas. Sans auto-entrée : ±2 dB. |
| T4 | **« Doubles » = harmonies +12 et −5 granulaires** | Sinus de 220 Hz : la quarte basse sort en raies à 157 et 173 Hz, l'octave à 408 et 470 Hz. C'est la modulation du décaleur à deux têtes (fenêtre de 32 ms) : **roulis à 8 Hz, grésillement à 31 Hz**. Présent aussi dans Gorge et Patine, décrits comme « sans autotune » (−31 à −27 dB). |
| T5 | **Latence de 751 échantillons (17 ms)** | Inutile pour Gorge et Patine. Gêne le retour casque pendant l'enregistrement. |
| T6 | **Hauteur : rien sur le rap** | En Auto, la tonalité n'est jamais verrouillée sur la prise, même bouclée sur 34 s. Mélodie chantée : verrouillage en 2,5 s ; erreur moyenne de 16,3 c en entrée → Grain Nord à 5 : 13,8 c (presque rien), Grain Nord à 10 : 7,3 c, Poupée à 10 : 2,1 c. |
| T7 | **Pas d'anticipation** | Glissando A3→B3 de 400 ms, Poupée à 10 : marches puis **dépassement de +0,8 demi-ton** à l'arrivée. L'analyse a 20 à 40 ms de retard, alors que les 16 ms de latence payées pourraient servir d'anticipation. |
| T8 | **Marches dans la course** | Énergie au-dessus de 10 kHz sur un balayage : Grain Nord **+12 dB à 7,3 et +37 dB à 8,6** (`hn = 1 + int(3·lofi)`), Poupée **+17 dB à 8,9**, Gorge +5 dB à 9,1 (maintien d'échantillon du moteur dès mordant ≥ 0,5). |
| T9 | **Repliement en fin de course** | Sinus de 3 150 Hz, énergie inharmonique sous 1,9 kHz : de −87 à **−53 dB** (Grain Nord), de −89 à −63 dB (Poupée). |
| T10 | **Ordre de la chaîne** | Compresseur et de-esser placés *après* reverb, delay et doubles : les queues pompent, la reverb reçoit les sifflantes. Le « limiteur » est un tanh permanent (×0,9, ×0,95) qui colore tout. Le `mordant` couple saturation, quantification et maintien d'échantillon : pas de grain chaud sans lo-fi. |
| T11 | **Aigus sans de-esser** | Poupée (+5 dB, +5 de plus via BRILLANCE) et Patine (+4,5 dB) ont un de-esser à 0. Aigus relatifs de Poupée : de −1,5 à +0,1 dB entre 0 et 10, sur une prise pourtant peu sifflante. |

Enfin, chaque paramètre suit une **droite indépendante**. Il n'y a ni inflexion pensée, ni étalonnage de sonie, ni adaptation au signal au-delà d'un RMS global. Les bons plugins à un potard (OneKnob, CLA, Fresh Air) font entrer chaque paramètre à son tour, sur une plage étalonnée à l'écoute.

## 2. Plugin par plugin

### Grain Nord (« retune, doubles, brume sombre »)
**(a) Le potard.**
- Tension de 0 à 0,70 ; retune de 140 à 30 ms ; zone morte de 14 à 5 cents.
- Mordant de 0,05 à 0,35.
- Brume de 0,08 à 0,70 : reverb de 0,9 à 2,0 s + delay en croches.
- Halo de 0,1 à 0,5.
- Compression de 0,25 à 0,60.
- Plateau aigu de −1 à −5 dB.
- Au-delà de 6 : passe-bas de 19 à 7,5 kHz et lo-fi.

**(b) Pourquoi ça ne tient pas.** L'identité annoncée est le retune. Il est absent sur le rap et faible au réglage par défaut sur du chant (T6). On entend surtout une ambiance qui monte, des harmonies granulaires et du lo-fi en marches.

**(c) Défauts audibles.**
- Grésillement des harmonies (T4).
- Marches à 7,3 et 8,6 (T8).
- Repliement à 10.
- Reverb pompée par le compresseur.
- Bruit remonté dans les pauses.

**(d) Meilleure version.**
- **Algorithme** : PSOLA (décalage synchrone à la période) avec anticipation. Les doubles deviennent de vrais doubles : même note, ±7 à 12 cents, 12 à 25 ms, formants légèrement variés, gauche et droite décorrélées. Harmonies en option.
- **Macro** : 0 à 3 doubles et largeur ; 3 à 7 retune de 90 à 25 ms, seulement si la tonalité est connue ; 7 à 10 « bande usée » (réduction de débit continue et filtrée, pleurage).
- **Tonalité** : celle de l'hôte, ou verrouillage en un clic par analyse de l'instrumental en sidechain.
- **Unique** : une **« brume tonale »**, reverb dont seules les composantes accordées à la gamme sont entretenues (résonateurs accordés en sortie du réseau de retards), atténuée sous la voix. Une nappe qui suit la tonalité, inédite en un bouton.

### Gorge (« grain rauque, compression, slapback »)
**(a) Le potard.**
- Mordant de 0,12 à 0,54 : tanh avec drive de 2 à 5,9, plus du lo-fi au-delà de 9.
- Compression de 0,35 à 0,90.
- Grave de +0,5 à +4 dB, aigu de −0,5 à −3 dB.
- De-esser de 0,3 à 0,8.
- Slap à 85 ms, de 0 à 22 %.
- Brume et harmonies discrètes.

**(b) Pourquoi ça ne tient pas.** Le « rauque » est une distorsion statique de toute la voix (H3 à −24 dB, H2 à −29 dB à 10). Une voix gutturale est rauque *sur les attaques et les fins de phrase*. La compression varie à peine (T2). Le slap n'est pas calé sur le tempo.

**(c) Défauts audibles.**
- Harmonies fantômes.
- 17 ms de latence pour rien.
- Lo-fi qui entre en marche à 9,1.
- Delay ping-pong caché dans la brume.

**(d) Meilleure version.**
- **Zéro latence.**
- **Saturation dynamique** sur la bande 150–900 Hz, suréchantillonnée, avec un drive qui suit l'enveloppe des syllabes.
- **Compression optique** à relâche dépendante du programme.
- **Slap** calé sur le tempo, filtré.
- **Macro** : compression et grave (0 à 4), puis grain (3 à 8), puis slap (6 à 10).
- **Unique** : un **générateur de « râle »**. Sous-harmonique f0/2 synchrone au YIN, plus une modulation de bruit à la période (vocal fry), réservées aux syllabes voisées graves. C'est la vraie texture de gorge, pas une distorsion de guitare.

### Poupée (« hauteur nette, doubles, brillance »)
**(a) Le potard.**
- Tension de 0,35 à 1 ; retune de 60 à 4 ms ; zone morte de 8 à 1 cents.
- Mordant de 0,10 à 0,55.
- Halo de 0,10 à 0,70.
- Brume au-delà de 5.
- Compression de 0,40 à 0,90.
- Aigu de +1 à +6 dB.
- Slap au-delà de 5.

**(b) Pourquoi ça ne tient pas.** L'hyperpop, c'est un hard-tune qui **plaque aussi les transitions**, avec des formants déplacés et des doubles propres. Le mode « sûr » fait l'inverse : il corrige seulement une hauteur stable et ne corrige rien tant que la tonalité n'est pas trouvée. Les +12 sans formants donnent l'effet « chipmunk », avec en plus le grésillement à 31 Hz.

**(c) Défauts audibles.**
- Dépassement de +0,8 demi-ton à la fin des glissandos.
- Marche à 8,9.
- Repliement à −63 dB.
- Aigus sans de-esser.

**(d) Meilleure version.**
- **Hard-tune chromatique par défaut**, ou tonalité de l'hôte ; anticipation ; quantification pendant les transitions ; retune de 0 à 5 ms.
- **PSOLA à formants préservés** (enveloppe par cepstre ou LPC).
- **Macro** : 0 à 3 hard-tune ; 3 à 7 **formants de +0 à +4 demi-tons indépendants de la hauteur**, et octave en double ; 7 à 10 écrêtage suréchantillonné et brillance contrôlée par de-esser.
- **Unique** : **hauteur et formants liés sur une seule courbe** pensée pour l'hyperpop, avec des formants resserrés pour garder l'intelligibilité. Little AlterBoy et Auto-Tune séparent ces commandes.

### Patine (« saturation, compression parallèle, air »)
**(a) Le potard.** Mordant de 0,05 à 0,40, brume jusqu'à 0,27, halo jusqu'à 0,25, compression de 0,30 à 0,80, aigu de 0 à +4,5 dB à 9 kHz.

**(b) Pourquoi ça ne tient pas.** C'est le finisseur qu'on devrait pouvoir laisser sur toutes les voix. Or il ajoute de l'espace et des harmonies qu'on ne lui demande pas, paie 17 ms de latence et booste l'air sans contrôle des sifflantes.

**(c) Défauts audibles.**
- Harmonies fantômes (environ −27 dB à 10).
- Ambiance dès 0.
- Plage de compression faible.

**(d) Meilleure version.**
- **Saturation de bande** (hystérésis simplifiée), puis **compression en série et en parallèle**, puis **air dynamique** couplé à un de-esser.
- Latence de quelques échantillons seulement.
- **Macro** : dynamique (0 à 5), densité (3 à 8), air (5 à 10).
- **Unique** : une **courbe tonale cible « voix pop finie »**. Un égaliseur lent de 8 à 12 bandes rapproche le spectre moyen d'une cible apprise sur des voix de référence, et le potard dose la distance à la cible et la densité. Un Gullfoss dédié à la voix, en un geste.

## 3. Structure de macro commune

1. **Points d'arrêt par paramètre** (5 à 7 points, interpolation Hermite monotone). Plus aucun `int(…)` sur la course.
2. **Détecteur de voix** (énergie + voisement) : auto-entrée et auto-gain figés hors voix, seuils relatifs au niveau voisé, plus jamais absolus.
3. **Sonie étalonnée hors ligne** par courbe de compensation en fonction de k ; l'auto-gain ne fait qu'une correction fine de ±3 dB.
4. **Ordre** : entrée → hauteur → saturation → compression → de-esser → égalisation → **bus d'espace parallèle**, atténué sous la voix → vrai limiteur de crête, inactif sous −1 dBFS.
5. **Latence par modèle** : 0 sans hauteur ; anticipation de l'analyse quand il y a de la hauteur.

## 4. Nouveaux plugins à un potard

| Prio | Nom | Concept et macro | Algorithme | Différence avec l'existant | Effort |
|---|---|---|---|---|---|
| 1 | **Souffle** | Air sans sifflement : de-esser → air → excitation | Plateau > 10 kHz dynamique + excitateur à harmoniques paires (5–12 kHz), mis en sourdine par le détecteur de sifflantes | Impossible de faire siffler | Faible |
| 2 | **Velours** | Voix douce et proche : optique → creux à 3 kHz → bande | Compression optique, coupe dynamique 2,5–4 kHz sur les syllabes dures, saturation de bande | Douceur sans éteindre la voix | Faible |
| 3 | **Bande** | Radio AM → téléphone → mégaphone | Passe-bande glissant, écrêtage suréchantillonné, résonance de pavillon | Une course de textures plutôt qu'un preset figé | Faible |
| 4 | **Respire** | Nettoyer une prise rap : respirations −3 → −15 dB, puis bruit | Classement par trame (voix, respiration, bruit) avec YIN, énergie et platitude spectrale ; gain par classe | Classe au lieu de couper au seuil : les fins de mots restent | Moyen |
| 5 | **Ad-lib** | Téléphone → *throw* delay → reverb sombre | Delay calé sur le tempo **envoyé seulement sur le dernier mot de chaque phrase** | Automatise les throws dessinés à la main | Moyen |
| 6 | **Distance** | De « collée au micro » à « au fond de la pièce » | Effet de proximité, rapport direct/réfléchi, réflexions précoces, perte d'aigus dans l'air | Un axe perceptif au lieu de 6 réglages de reverb | Moyen |
| 7 | **Râle** | Grain guttural, vocal fry | Sous-harmonique f0/2 + bruit modulé à la période | Inexistant en un bouton ; code partagé avec Gorge | Moyen |
| 8 | **Sous-sol** | Couche « voix pitchée vers le bas » de la trap sombre, de 0 à −12 demi-tons | PSOLA à formants (moteur de Poupée), en parallèle | Un −12 sans voix de monstre | Moyen |
| 9 | **Bégaie** | Coupes et répétitions : rares → glitch | Détection des syllabes, répétitions calées sur le tempo en fin de phrase | Suit le texte, pas une grille aveugle | Moyen |
| 10 | **Foule** | Gang vocals à partir d'une seule prise : de 2 à 8 voix | PSOLA à formants, ±15 cents, 10 à 40 ms, formants ±2 demi-tons, panoramique | Chaque voix est une « personne », pas un chorus | Élevé |

Les trois premiers réutilisent les briques existantes. Respire et Ad-lib reposent sur le détecteur de voix de la phase 0. Sous-sol et Foule découlent du moteur de Poupée.

## 5. Plan de reprise des 4 existants

**Phase 0 : briques communes (environ 3 jours)**
1. Détecteur de voix, avec auto-entrée et auto-gain figés hors voix (corrige T3).
2. Séparer saturation et lo-fi ; lo-fi par rééchantillonneur filtré et continu (T8, T9).
3. Espace en bus parallèle, de-esser avant les aigus, vrai limiteur de crête (T10, T11).
4. Gorge et Patine sans moteur de hauteur : latence 0, pas d'harmonies (T4, T5).
5. Tables de points d'arrêt et sonie étalonnée (T1, T2).

**Phase 1 : moteur de hauteur (environ 1 semaine)**
6. Anticipation, PSOLA, formants ; doubles réels d'un côté, harmonies en option (T4, T7).
7. Tonalité de l'hôte ou verrouillage en un clic ; Poupée en chromatique dur (T6).

**Phase 2 : revoicing (environ 1 semaine)**
8. Brume tonale (Grain Nord), râle (Gorge), hauteur + formants (Poupée), courbe cible (Patine).
9. Écoute en aveugle sur 5 prises (rap parlé, rap mélodique, chant aigu, voix sifflante, prise bruitée) à 0, 3, 5, 7 et 10.

**Phase 3 : verrouillage**
10. Ces bancs deviennent des tests dans `tests/onek_test.cpp` :
    - cohérence ≥ 0,98 à 0 ;
    - aucun saut supérieur à 3 dB sur un balayage ;
    - bruit de pause : +2 dB au plus ;
    - repliement < −80 dB ;
    - harmonies < −90 dB dans Gorge et Patine ;
    - dépassement de hauteur < 20 cents.
