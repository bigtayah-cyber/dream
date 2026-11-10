# Audit 3, n° 6 : feuille de route priorisée

Impact : effet sur « ça sonne pro / ça se règle » (5 = majeur). Effort : S ≤ 2 j, M ≤ 1 semaine, L > 1 semaine. Risque : risque de casser des projets existants ou de régresser.

## 2e livraison : « ça ne fait plus cheap » (fondations et rendus)

| # | Chantier | Plugins | Impact | Effort | Risque | Preuve / détail |
|---|---|---|---|---|---|---|
| A1 | `stateVersion = 2` écrit dans les 23 plugins sans version | Tous | 3 (protège tout le reste) | S | Faible | 05, R1 |
| A2 | **Tranche voix commune** : `OptoComp`/`FetComp` + `DeEssDyn` relatif + `Saturator` + EQ appairé et lissé, à la place de `onek::Comp`/`DeEss`/RBJ. Ancien chemin conservé pour les états v1 | Looks, Atelier, Cabine (Master : compresseur seulement) | 5 | M | Moyen (son des presets) | 01 § 1.1 ; DHT ÷10 mesurée |
| A3 | Attaques accentuées : anticipation de 1 à 2 ms sur le compresseur, de-ess avant le shelf, attaque ≥ 10 ms sur Gueulante, Verre Pilé et Paillettes, shelf aigu appairé | Looks, Atelier | 5 (plainte directe de l'artiste) | S | Faible | 01 § 0 : +3,5 à +4,3 dB d'attaque. *À coordonner avec l'agent « claques »* |
| A4 | Niveau de Looks : gain statique tabulé par look et par macro ; AGC limité à ±3 dB | Looks | 4 | S | Moyen | 01 § 1.3 |
| A5 | Doubles `AdtVoice` à la place du chorus et des décaleurs à désaccord fixe | Cabine, Looks, Atelier | 4 | S | Moyen | AM 0,17 → 0,10 |
| A6 | Saturation de Cabine ×4 + ADAA ; compensation statique (Cabine, Master) ; compresseur de bus RMS ou vari-mu avec relâche auto (Master) | Cabine, Master | 4 | S | Moyen | −48 dBc à sat 10 ; `MasterDsp.h:258` |
| A7 | Plages et courbes (ratio, Q, 808 longueur et punch, graine en entier, formants au pas de 0,05, de-ess relatif, unités « x10 ») | Atelier, Cabine, 808, Accroc, Relance, Autotune, DeEsser | 4 | S | Faible si les ID restent identiques. **Changer une plage change l'automation existante** : ne modifier que les skews, pas les bornes, sauf avec `stateVersion` | 02 § 2 |
| A8 | Kit v3 : **presets utilisateur, annuler/rétablir, A/B, niveau égal** dans la barre commune | Tous | 5 (réglabilité) | M | Faible | 02 § 1 |
| A9 | CI : tous les tests console sur Mac et Windows, pluginval figé, tests d'interface sous Windows | — | 3 | S | Nul | 05, R3-R4 |
| A10 | Pads/Sample : durée de vie des banques sûre | Pads, Sample | 3 (crash possible) | S | Faible | 05, R2 |
| A11 | Écouter enfin : les 8 points de `tune-algo.md` § 10, et Looks avant/après A2-A5 sur 3 vraies prises (rap sec, chant aérien, ad-libs) | Autotune, Looks | 5 | S | Nul | Jamais fait |

## 3e livraison : « ça a une identité » (design et signatures)

| # | Chantier | Impact | Effort | Risque |
|---|---|---|---|---|
| B1 | Migration v3 terminée sur les 26 ; « Classique » retiré du menu | 5 | M | Faible |
| B2 | Hiérarchie : tiroir « avancé » pour Galet et Atelier, Cabine avec courbe de transfert et GR continue, LUFS dans Master | 4 | M | Faible |
| B3 | Beat : charlestons métalliques, clap stéréo, kick ADAA | 4 | M | Moyen (son des projets) |
| B4 | Signatures « petites » : Double fantôme (V1), De-ess vers l'espace (V2), Lancer de fin de phrase (V3), Pompe sans sidechain (S1), Coupes sur la voix (S5), 808 qui suit les accords (I2), Flip dans la tonalité (I4) | 5 | S chacune | Faible |
| B5 | Macros Atelier (4), Cabine « Devant/Derrière », Master « Énergie » | 4 | S | Faible |
| B6 | Aperçu d'Orbe branché sur le moteur d'Orbe Synth | 3 | S | Faible |
| B7 | Version et commit affichés dans l'aide ; signature Developer ID et Authenticode si diffusion hors label | 3 | S/M | Nul |

## Plus tard (4e livraison et au-delà)

Le lien entre instances (« Fil »), qui ouvre V6, V7, S4 et S6 ; Beatbox vers Beat (I1) ; Chop sur les syllabes (I3) ; trois algorithmes de reverb ; mode stéréo lié d'OrbeChain.

## À écarter

- Ajouter encore des looks ou des presets avant A2-A4 : chaque preset ajouté sera à recalibrer.
- Un mode « ancien son » pour chaque nouveau moteur : figer une fois (A1 + A2), puis n'en plus ajouter.
- L'IA générative et les talkbox avec sidechain (changement de bus).
- Retoucher la reverb FDN maintenant : la mesure la dit saine (queue diffuse, RT juste). Seule la densité du début (NED 0,64 à 30 ms) est perfectible ; c'est un détail à traiter après A2-A6.
