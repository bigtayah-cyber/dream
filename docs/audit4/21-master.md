# Audit 4 · Orbe Master

*Bancs : `/tmp/b_aud4s/master.cpp` et `pump.cpp` (48 kHz, `MasterDsp.h`). Rien n'a été écouté.*

## Références
- **FabFilter Pro-L 2** : 8 styles de limitation (*Safe*, *Modern*, *Aggressive*...) ; suréchantillonnage jusqu'à ×32 ; mode true peak ; mesure EBU R128 / BS.1770-4 ; **Unity Gain** (écoute à niveau égal) et **Audition Limiting** (on n'entend que ce qui est retiré) ; dither.
- **iZotope Ozone 11** : assistant ; *Threshold Learn* vers une cible LUFS ; Maximizer IRC adaptatif avec *Soft Clip* ; *Low End Focus* ; Imager par bande ; Match EQ ; prévisualisation MP3/AAC ; Delta.
- FG-X, LANDR (connaissance générale) : un seul geste « plus fort », un écrêteur avant le limiteur.

## Mesures (nouveau)
| Mesure | Résultat | Lecture |
|---|---|---|
| LUFS momentané, sinus 997 Hz à −20 dBFS | **−20,00** | Juste |
| Limiteur, sinus de 40 Hz, 8 dB de GR | **DHT 1,16 %** (0,05 % à 100 Hz) | La fenêtre de 3 ms et la relâche fixe suivent l'onde des graves : distorsion sur la 808 |
| Plafond à −1 dBTP, aigus avec crêtes entre échantillons | Sortie à **−0,68 dBTP** (réf. ×16) | Dépassement de 0,32 dB. Estimation ×4 parabolique, puis écrêtage sur les échantillons |
| Compresseur de bus (50 %), kick + nappe | −1,1 dB à 210 ms, 0 à 400 ms | Pompage **modéré** (nuance l'audit 3). Le vrai défaut : 30 ms d'attaque sur un détecteur crête |

D'après le code :
- `Lufs` ne donne que le momentané, alors qu'`OrbeAnalysis` a déjà l'intégré, le court terme et la LRA.
- L'EQ est en RBJ non apparié (le shelf à 10 kHz se comprime à 44,1 kHz).
- La mono des basses est un passe-haut du 2e ordre sur le côté seul (déphasage).
- Pas de dither.

## Améliorations
| # | Amélioration | Impact | Effort | Risque |
|---|---|---|---|---|
| M1 | Limiteur à relâche **adaptative** (2 constantes), fenêtre de 5 ms ou détection en passe-haut ; cible DHT < 0,3 % à 40 Hz | 5 | M | Moyen |
| M2 | True peak strict : polyphase ×4 conforme ou ×8, écrêtage final suréchantillonné, test en CI | 4 | S | Faible |
| M3 | Brancher `oa::MixMeter` (intégré, court terme, LRA, PLR) ; bouton **Cible** (−14 / −11 / −9 LUFS) qui règle le gain | 5 | S | Nul |
| M4 | **Écoute à niveau égal** et **Delta** | 5 | S | Nul |
| M5 | Écrêteur doux ×4 avant le limiteur (audit 3) | 4 | S | Moyen |
| M6 | EQ appariée (`eq::designMatched`) | 3 | S | Faible |
| M7 | Dither TPDF pour l'export en 16 bits | 2 | S | Nul |
| M8 | Mono des basses en Linkwitz-Riley | 2 | S | Faible |

Micro-UX : GR en dB continue et historique défilant ; clic sur un compteur pour le remettre à zéro ; saisie directe du plafond ; infobulle « plateforme » sur la cible.

## Signature
**« Le grave suit la 808 »** : le suivi de hauteur (YIN) estime la fondamentale du grave. La coupure de la mono des basses et le filtre de détection du limiteur se placent à 1,5 × f0. La 808 garde son corps et n'est plus distordue, et le stéréo au-dessus reste large. Effort M.

## Stabilité
- `lim.resetMeter()` est appelé à chaque bloc : le GR affiché dépend de la taille de bloc. Le publier en crête tenue sur 50 ms.
- La latence (16 + environ 149 échantillons) est à vérifier à 44,1 et 96 kHz.

## Sources
- Pro-L 2 : https://www.fabfilter.com/news/1512462600/fabfilter-releases-fabfilter-pro-l-2-true-peak-limiter-plug-in
- Ozone 11 : https://www.izotope.com/en/products/ozone/features
- BS.1770-4 et EBU Tech 3341/3342 : cités de mémoire.
