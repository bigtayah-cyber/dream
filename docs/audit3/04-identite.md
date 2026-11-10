# Audit 3, n° 4 : identité, des fonctions « qui n'existent pas ailleurs »

*Ce qui suit, ce sont des **fonctions dans les plugins existants**. Les nouveaux plugins sont déjà dans `docs/audit2/05-nouveaux-plugins.md` (Fil, Rémanence, Syllabe, Cadence, Pli, Chimère, Montée, Givre, Empreinte, Mue) et ne sont pas repris. Chaque idée cite les briques du dépôt qui la rendent faisable. Effort : S ≤ 2 jours, M ≤ 1 semaine, L plus.*

Fil rouge proposé : **la suite écoute la voix et la tonalité, et tout le reste s'y accroche.** Aucun concurrent ne relie de cette façon le détecteur de phrases (Relance), la tonalité (Detect, Orbe) et la hauteur (TuneCore). C'est le terrain de dreamaws.

## Voix

| # | Idée | Ce que ça fait | Briques existantes | Effort |
|---|---|---|---|---|
| V1 | **Double fantôme** (Autotune, Looks) | Le double ADT suit la hauteur **non corrigée** pendant que la voix lead est accordée. Le double garde donc les petites imprécisions d'une vraie seconde prise. Les autres doubleurs décalent soit le signal corrigé, soit le signal brut, jamais l'un contre l'autre | `TuneCore` (hauteur brute et cible), `AdtVoice` | S |
| V2 | **De-ess vers l'espace** (DeEsser, Looks) | La sibilance retirée du son sec part, adoucie, dans la reverb : l'air reste, le sifflement de premier plan disparaît | `DeEssDyn` (quantité réduite par échantillon), FDN | S |
| V3 | **Lancer de fin de phrase** (Looks, Voile : macro « Lancer ») | Le delay ne s'ouvre que sur le dernier mot de chaque phrase, synchronisé au tempo : le « throw » qu'un ingé automatise à la main sur chaque refrain | Segmenteur de phrases de Relance, `PingPong` | S |
| V4 | **Respiration** (Atelier, Cabine : un potard de −12 à +6 dB) | Les respirations sont détectées (non voisé, faible, entre deux phrases) et baissées, ou montées pour un effet intime | YIN (voisement), enveloppe de Relance | S |
| V5 | **Formant rythmé** (Poupée, Autotune) | Le formant oscille en mesure (1/8, 1/16) : la voix « yodle » façon hyperpop sans que la hauteur bouge | `VoiceShifter` (formants), horloge hôte de Galet | S |
| V6 | **Gamme qui suit l'accord** (Autotune, Voile) | La gamme cible devient l'accord en cours (accord envoyé par Orbe, ou détecté par Detect) : les notes de passage restent justes même hors gamme. C'est le premier usage concret de « Fil » | Orbe (accords), lien entre instances à créer | M |
| V7 | **Harmonie à la tierce juste de l'accord** (Autotune) | La tierce suit l'accord en cours au lieu d'un intervalle fixe : majeure ou mineure selon l'accord | V6 + `Splicer` | M |
| V8 | **Ad-libs auto** (Relance, mode « Ad-lib ») | Relance choisit des mots déjà chantés, les passe au formant et au filtre, et les place dans les trous de la phrase suivante | Relance existant | S |

## Studio

| # | Idée | Ce que ça fait | Briques | Effort |
|---|---|---|---|---|
| S1 | **Pompe sans sidechain** (Galet) | Galet détecte lui-même les coups de grosse caisse dans le signal qui le traverse (onsets de la bande grave) et relance sa forme à chaque coup : pompe calée sur le vrai kick, même sans entrée sidechain. Ça règle aussi le problème du preset « Pompe rythmique » | `osamp::onsets`, `retrig` de Galet | S |
| S2 | **Aimant tonal** (Orbe EQ) | Les cloches s'aimantent aux fréquences des notes de la tonalité détectée, ou s'en écartent pour les coupes de résonance : couper « le Ré qui sonne » plutôt que « 293,7 Hz » | Detect (tonalité), EQ appairé | S |
| S3 | **Devant / Derrière** (Cabine, une macro) | Un seul geste règle à la fois présence, compression, pré-délai et envois. C'est la décision principale d'un mix rap | Moteur de Cabine | S |
| S4 | **Rapport voix/instru en LU** (Master, Mix) | Master affiche l'écart de loudness entre la voix (lu dans les instances Cabine ou Looks) et le reste, avec une cible par style (trap, pop électro) | Lien entre instances, LUFS | M |
| S5 | **Coupes sur la voix** (Accroc, mode « Voix ») | Les bégaiements et les tape-stops se déclenchent en fin de phrase, et non plus sur la grille : vocal chops automatiques | Segmenteur de Relance, moteur d'Accroc | S |
| S6 | **Écoute du conflit** (Orbe EQ) | Le spectre de la voix (instance Cabine/Looks) est superposé à celui du beat. Les zones de masquage sont surlignées et une cloche de coupe est proposée | Lien entre instances, FFT d'OrbeDsp | M |
| S7 | **Master « référence dreamaws »** | Courbe tonale moyenne des sorties du label : l'EQ propose l'écart (« Empreinte » dans l'audit 2) ; ici, réduit à un bouton dans Master | `OrbeAnalysis` | M |

## Instruments

| # | Idée | Ce que ça fait | Briques | Effort |
|---|---|---|---|---|
| I1 | **Beatbox vers Beat** | On fait la batterie à la bouche, Beat reconnaît kick, caisse et charleston (centroïde spectral et énergie de bande à chaque onset) et en tire un motif | Onsets, FFT, motifs de Beat | M |
| I2 | **808 qui suit les accords** | La 808 glisse automatiquement vers la fondamentale de l'accord suivant d'Orbe (glide sur le dernier 1/16) | Sortie MIDI d'Orbe, glide de la 808 | S |
| I3 | **Chop sur les syllabes** (Pads) | Une prise de voix est découpée aux syllabes, et chaque tranche est accordée sur la gamme du projet | Onsets, YIN, `TuneCore` mode Direct | M |
| I4 | **Flip dans la tonalité** (Sample) | La tonalité de l'échantillon est détectée, puis il est transposé dans celle du projet (Detect ou Orbe) en un clic | Analyse de tonalité de `SamplerDsp`, rééchantillonnage | S |
| I5 | **Harmoniser ce que je fredonne** (Fredonne → Orbe) | La ligne fredonnée devient une mélodie, et Orbe lui propose trois grilles d'accords | Fredonne (notes), générateur Orbe | M |
| I6 | **Kit qui suit le tempo et la tonalité** (Beat) | Toms, rim et percussions accordés sur la tonalité ; l'ouverture des charlestons suit le swing | Moteur de Beat, Detect | S |

## À écarter (ou plus tard)

- **IA générative** (voix, stems) : hors de portée du moteur, et ce n'est pas l'identité du label.
- **Talkbox ou vocodeur avec sidechain** : il faudrait ajouter un bus sidechain, ce qui change la configuration des bus des projets existants. À faire seulement dans un nouveau plugin.
- **Un mode « Classique » pour chaque nouveau moteur** : le coût de maintenance double. Le figer une fois (comme les un-potard), puis ne plus en ajouter.
