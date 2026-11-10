# Audit 5 — Nouveaux plugins : ce qui n'existe pas ailleurs

POV directeur produit / R&D. Base : `docs/audit.md` §3 et §5, `docs/looks-voix.md`, README, CMake, en-têtes des moteurs. Aucun fichier source modifié.

## 1. Ce que la suite sait déjà faire (briques réutilisables)

- **Voix** : YIN, décaleur (`Engine.h`), formants cepstraux (`AutotuneDsp.h`), chaîne `OrbeChain.h`.
- **Analyse** : tempo, tonalité, LUFS, bandes contre cible (`OrbeAnalysis`, `OrbeMix`), voix vers notes (`OrbeFredonne`).
- **Spectral** : STFT 2048 WOLA (`DenoiserDsp.h`), réutilisable.
- **Génération** : Orbe, Beat, 808, Synth.
- **Manque structurel** : les 24 plugins **ne se parlent pas**. Orbe connaît tonalité, accords et sections ; aucun effet n'en profite. C'est le levier de différenciation le plus fort et le moins coûteux.

## 2. Veille : ce que fait le marché fin 2026

| Tendance | Exemples | Lecture R&D |
|---|---|---|
| Résynthèse vocale | Minimal Audio **Evoke** (analyse la voix et génère un nouveau signal, rack de 8 effets) | Créneau occupé. Ne pas faire un « Evoke de plus » |
| Formants seuls | zplane **TIMBRE** (formants sur deux bandes, sans toucher la hauteur), ub dsp Vowel Blender | Commodité. Nos formants existent déjà |
| Multi-effet voix | UJAM **Voxcraft** | Occupé ; déjà couvert par Looks/Atelier |
| Démêlage spectral | Trackspacer, Hornet **SpectraDuck**, sonible **smart/pure:unmask** | Occupé. Tous **aveugles à la musique** : ils ne savent pas quelle note joue la 808 |
| Séparation temps réel | zplane **PEEL STEMS 2**, LALAL.AI (latence jusqu'à 6 s) | Gros modèles ML : **à éviter** |
| Assistants mix/master | smart:chain, Ozone 12, **REFERENCE 3**, **Ninja AB** (par section) | Très occupé ; seule la **section** est neuve |
| MIDI génératif lié aux accords | Denormal **Guardrail** (joue une piste d'accords au clavier), Soft Loop **Harmonic Echo** (écho MIDI génératif) | Le contexte harmonique reste **côté MIDI**. Aucun effet **audio** n'écoute les accords du morceau |
| Synthèses originales | **Phaseburn One** (boucle à verrouillage de phase), u-he Zebra 3, Sound Radix Radical1 (additif), reFX Rippler (modal) | L'originalité passe par **un principe physique et une identité**, pas par le nombre d'oscillateurs |
| Découpe vocale | WA Production ChopBeast 2 | Outils **manuels** : rien ne génère d'ad-libs à partir de la voix elle-même |

**Trous de marché retenus**
1. Des effets audio qui **connaissent l'harmonie et la structure** du morceau, par un bus de contexte propre à la suite.
2. Des traitements **conscients de la parole rap** : syllabes, flow, fins de phrase.
3. Un mix **dynamique par section**, qui façonne l'énergie du morceau au lieu d'appliquer un réglage fixe.
4. De la génération **à partir de la voix de l'artiste** (ad-libs, rythmes, textures), pas à partir de banques.

## 3. Dix plugins nouveaux

Noms provisoires, distincts de ceux du §3.4 de l'audit ; à vérifier sur KVR, l'INPI et l'EUIPO avant sortie.

### 3.1 Fil : le système nerveux de la suite
- **Concept** : Orbe (ou Détect) publie la tonalité, l'accord courant, la section et le tempo, et tous les plugins dreamaws s'y abonnent.
- **Utilisateur** : rien. Une pastille « Fil » s'allume ; choix « suivre Orbe / détection / manuel ».
- **Algorithme** : fichier mappé en mémoire (`juce::MemoryMappedFile` dans le dossier temporaire), protégé par un verrou de séquence (seqlock) et écrit sans allocation. Chaque `.vst3` est un binaire séparé, donc un singleton statique ne suffit pas. Horodatage en ppq. Secours : champ accord du `ProcessContext` VST3 si l'hôte le remplit (FL Studio : à vérifier).
- **Différenciant** : aucun éditeur tiers ne diffuse le contexte harmonique vers ses effets audio en dehors des pistes d'accords de certains DAW.
- **Risques** : plusieurs projets ouverts (identifiant de session), dérive de synchronisation ; les plugins « bridgés » restent couverts par le fichier mappé.
- **Effort** : M. **Lien** : il rend possibles les plugins 2, 5, 6, 7, 8 et 9.

### 3.2 Rémanence : les échos qui connaissent les accords
- **Concept** : un delay et une reverb dont les traînes se **réaccordent** sur l'accord en cours et « résolvent » à chaque changement.
- **Utilisateur** : un réglage AFFINITÉ (de 0 = écho normal à 1 = écho entièrement harmonisé), un réglage BRILLANCE et un sélecteur de motif (notes de l'accord, quinte seule, octaves).
- **Algorithme** : réinjection qui passe par le décaleur existant, quantifié à la note de l'accord la plus proche (YIN sur la traîne), et banc de 8 résonateurs en peigne accordés sur les notes de l'accord, avec un fondu de 50 ms au changement d'accord. Reverb de `OrbeChain`.
- **Différenciant** : Harmonic Echo reste en MIDI. Ici, c'est la voix elle-même qui chante l'harmonie dans ses échos, une signature immédiatement audible.
- **Risques** : artefacts du décaleur (dépend du lot H), boue harmonique (filtre dans la boucle).
- **Effort** : M. **Lien** : Fil, `Engine.h`.

### 3.3 Syllabe : traiter séparément consonnes et voyelles
- **Concept** : la voix est scindée en **consonnes** (attaques, souffle, « t », « k », « s ») et en **voyelles** (partie tonale), et chaque flux reçoit sa propre chaîne.
- **Utilisateur** : deux colonnes, CONSONNES et VOYELLES (niveau, saturation, envoi delay, envoi reverb, largeur), avec des presets comme « reverb sur voyelles seulement » ou « consonnes écrasées ».
- **Algorithme** : séparation harmonique/percussive par filtrage médian sur la STFT (Fitzgerald, 2010) avec le cadre STFT du Denoiser, affinée par la platitude spectrale et le voisement YIN. Un mode « direct » sans STFT (détecteur de transitoires et passe-haut dynamique) pour l'enregistrement.
- **Différenciant** : de-essers et shapers agissent sur un seul paramètre ; ici la phonétique sert de routage (reverb sur les seules voyelles : rap net et ample à la fois).
- **Risques** : environ 46 ms de latence en mode STFT, « phasiness » à la recombinaison (sortie somme exacte à réglages neutres, à tester).
- **Effort** : M. **Lien** : `DenoiserDsp.h`, briques de `OrbeChain`.

### 3.4 Relance : le backeur génératif
- **Concept** : le plugin écoute la voix principale et **fabrique lui-même des ad-libs** à partir de ses fins de phrases.
- **Utilisateur** : DENSITÉ, AUDACE, CARACTÈRE (écho de mot, octave basse, téléphone, inversé, bégaiement, harmonie) et verrou de graine.
- **Algorithme** : segmentation par énergie et voisement (YIN), détection de fin de phrase (chute d'énergie suivie d'un silence de plus de 150 ms), capture des 200 à 600 dernières ms dans un tampon circulaire, puis rejeu décalé d'une ou deux croches synchronisées, avec transformation par les briques existantes (décaleur, formants, bitcrush, panoramique). Choix probabilistes reproductibles par graine.
- **Différenciant** : ChopBeast reste manuel et Voxcraft applique des effets. Personne ne **répond** à la voix avec la voix.
- **Risques** : effet gadget (densité faible par défaut), fausses détections sur un débit très dense.
- **Effort** : M. **Lien** : Fil (harmonies dans la tonalité), Autotune (formants).

### 3.5 Cadence : du flow vers le rythme
- **Concept** : la voix rap (ou un fredonnement) devient une **grille rythmique**, et Orbe Beat et Orbe 808 construisent le motif autour du flow.
- **Utilisateur** : il enregistre 4 à 8 mesures, choisit RENFORCER (kicks sur les syllabes accentuées) ou RÉPONDRE (kicks et 808 dans les silences), puis glisse le MIDI ou l'envoie à Beat et 808 par Fil.
- **Algorithme** : détection d'attaques (flux spectral), estimation d'accent (énergie et durée de la voyelle), quantification sur la grille de 16 ou 32 pas, puis passage dans `generate()` et `vary()` d'Orbe Beat avec des poids issus de la grille de flow.
- **Différenciant** : les générateurs partent du genre ; ici le beat part de **ce que dit l'artiste**.
- **Risques** : flow très syncopé (proposer deux ou trois variantes), valeur perçue à démontrer en vidéo.
- **Effort** : M. **Lien** : Fredonne, Beat, 808.

### 3.6 Pli : démêlage qui connaît les notes
- **Concept** : un démêlage spectral entre la voix et la 808 qui sait **quelle note joue la 808** et ne creuse que les harmoniques en conflit avec les formants de la voix.
- **Utilisateur** : source prioritaire, PROFONDEUR, spectre annoté des harmoniques.
- **Algorithme** : side-chain sur la voix (enveloppe cepstrale reprise de l'Autotune), note de la 808 lue par Fil ou par YIN basse fréquence. Cloches dynamiques sur les harmoniques n·f0 de la 808, modulées par la puissance de la voix à ces fréquences : sans STFT, sans latence.
- **Différenciant** : Trackspacer et SpectraDuck travaillent sur 32 bandes fixes. Ici, l'atténuation est chirurgicale et musicale, et la fondamentale de la 808 n'est jamais touchée.
- **Risques** : créneau occupé (gain à prouver en écoute aveugle) ; glissades rapides à suivre.
- **Effort** : M. **Lien** : 808, EQ (filtres appariés), Fil.

### 3.7 Chimère : une voix, un chœur qui suit l'harmonie
- **Concept** : une seule prise devient un chœur de 3 à 8 « personnages » qui suivent les accords d'Orbe avec une vraie conduite des voix.
- **Utilisateur** : nombre de voix, ÉCART, et par personnage âge (formants), souffle, retard, position stéréo.
- **Algorithme** : moteur de voicing d'Orbe (conduite à mouvement minimal) qui calcule les notes cibles, puis décaleur et formants par voix, et micro-variations de timing (5 à 25 ms) et de vibrato par personnage.
- **Différenciant** : les harmoniseurs suivent une gamme ou un clavier. Ici, c'est la **grille d'accords du morceau** (Fil) qui pilote, avec des timbres distincts.
- **Risques** : processeur (rendu hors ligne possible), qualité du décaleur (lot H).
- **Effort** : L. **Lien** : Orbe (voicings), Autotune, Fil.

### 3.8 Montée : sculpter l'énergie section par section
- **Concept** : sur le bus master ou un bus de groupe, le plugin façonne la **courbe d'énergie** du morceau : le refrain s'ouvre (largeur, air, densité), le couplet se resserre.
- **Utilisateur** : passe d'ÉCOUTE, frise des sections (détectées ou via Fil), curseur d'énergie par section, DOSAGE global.
- **Algorithme** : sections tirées de la nouveauté spectrale (matrice d'autosimilarité de chroma et de MFCC, calculée hors du thread audio) ou de Fil. Sur chaque section, une inclinaison spectrale, une largeur M/S (mono garanti sous 120 Hz), une saturation parallèle et une accentuation des transitoires, avec des fondus d'une mesure. Mesures de `OrbeMix`.
- **Différenciant** : les assistants de mix posent un réglage **statique**. Ninja AB compare par section, mais rien n'agit par section.
- **Risques** : erreurs de segmentation (correction manuelle), pompage si surdosé.
- **Effort** : M. **Lien** : Mix, Master, Fil.

### 3.9 Givre : figer une phrase en nappe accordée
- **Concept** : on fige une phrase vocale ou un sample en une texture tenue dont les partiels **se recalent sur l'accord courant**.
- **Utilisateur** : FIGER (ou note MIDI), puis DÉRIVE, GRAIN, ACCORD ; la texture suit Orbe.
- **Algorithme** : capture d'une ou plusieurs trames STFT, randomisation de phase (gel spectral classique), puis réaffectation de chaque pic spectral vers la note de l'accord la plus proche, sur toutes les octaves, avec interpolation douce. Couche granulaire légère pour le mouvement.
- **Différenciant** : le gel spectral existe ; le **gel harmonisé piloté par le morceau** est rare.
- **Risques** : son métallique si la réaffectation est brutale.
- **Effort** : M. **Lien** : STFT du Denoiser, Fil, Pads (glisser la nappe vers un pad).

### 3.10 Empreinte : la signature sonore du label
- **Concept** : un fichier `.empreinte` apprend la signature des sorties dreamaws (courbe tonale, dynamique par bande, stéréo, densité des transitoires), puis montre et corrige l'écart d'un nouveau mix.
- **Utilisateur** : il dépose des références du label (empreinte partagée), lit l'écart par bande et par section, applique une correction partielle.
- **Algorithme** : moyenne et dispersion des mesures de `OrbeMix` sur les références, puis correction de l'EQ (bandes appariées de l'EQ) et de la dynamique douce (`MasterDsp`), sans aller au-delà de la dispersion apprise.
- **Différenciant** : faible seul (Ozone, REFERENCE 3) ; l'intérêt est une **identité de label portable** et la cohérence d'un EP, stratégique pour dreamaws.
- **Risques** : homogénéisation des sons, fonction proche de produits existants.
- **Effort** : S à M. **Lien** : Mix, Master, EQ.

## 4. Plugin phare pour plus tard : « Mue », le synthé qui pousse sur la voix

Aucune imitation : trois principes, **la matière vient de l'artiste**, **le synthé connaît le morceau**, **il respire comme un corps**.

**Architecture (trois couches, chacune en morphing)**
1. **Peau (source spectrale)** : oscillateur additif de 64 à 128 partiels dont les amplitudes viennent de « cartes de voyelles ». Ces cartes sont **capturées sur la voix des artistes dreamaws** par l'enveloppe cepstrale existante (une voyelle chantée devient une table). Un pad XY passe d'une voyelle à l'autre : un timbre que personne d'autre ne possède.
2. **Os (corps résonant)** : banc de 16 à 32 résonateurs modaux (amortissement, inharmonicité, matière) excité par la Peau ou par du bruit. Option « sympathique » : les modes s'accordent sur l'accord courant via Fil, si bien que le son vibre avec l'harmonie du morceau.
3. **Haleine (excitation)** : bruit filtré et micro-grains issus d'un échantillon de souffle de l'artiste, avec une enveloppe par note pour des attaques organiques.

**Mouvement** : quatre **Marées** (modulations lentes à phases liées) et un enregistreur de **Geste** (trajet dessiné sur le pad XY, en boucle). Unisson par **écart de formants** plutôt que par désaccord : largeur sans flou de hauteur.

**Sortie** : filtres et saturation d'`OrbeSynthEngine` (lot H), effets d'`OrbeChain`, Gestes exportés en MIDI.

**Identité visuelle** : planche de spécimens sur la palette Orbe, chaque carte de voyelle dessinée comme une empreinte.

**Feuille de route** : v0 Peau + filtre + ADSR ; v1 Os ; v2 Haleine et Marées ; v3 Fil et presets faits des voix du label. **Effort : XL** (3–4 lots L). Prérequis : Fil, lot H.

## 5. Classement intérêt / effort

| Rang | Plugin | Originalité (/5) | Intérêt pour l'artiste (/5) | Effort | Réutilisation de l'existant |
|---|---|---|---|---|---|
| 1 | **Fil** | 4 (invisible mais unique) | 5 (débloque tout) | M | Forte |
| 2 | **Relance** | 5 | 5 | M | Forte (YIN, décaleur) |
| 3 | **Rémanence** | 5 | 4 | M | Forte (delay, reverb, décaleur) |
| 4 | Syllabe | 4 | 4 | M | Moyenne (STFT) |
| 5 | Givre | 4 | 4 | M | Moyenne |
| 6 | Montée | 4 | 4 | M | Moyenne (Mix) |
| 7 | Cadence | 5 | 3 | M | Forte (Beat, Fredonne) |
| 8 | Chimère | 4 | 4 | L | Moyenne (dépend du lot H) |
| 9 | Pli | 3 | 4 | M | Forte |
| 10 | Empreinte | 2 | 3 | S–M | Très forte |
| — | Mue (phare) | 5 | 5 | XL | Partielle |

**Recommandation : faire d'abord ces trois-là**
1. **Fil** : silencieux seul, il transforme 24 plugins isolés en **écosystème**, impossible à copier sans toute la suite. Commencer par Orbe (émetteur) et Rémanence (récepteur).
2. **Relance** : le plus démontrable en 15 s de vidéo, cœur de l'usage rap, briques déjà testées.
3. **Rémanence** : première preuve audible de Fil (« mes échos chantent les accords d'Orbe ») et son signature du label.

**Par rapport au §5 de l'audit** : finir les lots A et H avant Rémanence et Chimère ; Fil peut avancer en parallèle de G (coordonner `OrbeProcessor.cpp`).

## Sources de la veille
[Evoke](https://manual.minimal.audio/evoke-manual/evoke-manual/what-is-evoke) · [TIMBRE](https://www.production-expert.com/production-expert-1/zplane-announces-timbre-real-time-formant-shifting-plugin) · [Voxcraft](https://www.soundonsound.com/news/ujam-introduce-voxcraft) · [SpectraDuck](https://blog.dubspot.com/hornet-spectraduck-2026) · [pure:unmask](https://www.mixonline.com/technology/news-products/sonible-pureunmask-plug-in-a-mix-product-of-the-week) · [PEEL STEMS 2](https://www.attackmagazine.com/news/peel-stems-2-is-here-with-lower-latency-for-real-time-stem-separation/) · [comparatif séparation 2026](https://www.production-expert.com/production-expert-1/6-stem-separation-tools-compared-for-audio-pros-in-2026) · [smart:chain](https://blog.dubspot.com/sonible-smart-chain-2026) · [REFERENCE 3](https://bedroomproducersblog.com/2026/04/07/mastering-the-mix-reference-3/) · [Ninja AB](https://www.production-expert.com/production-expert-1/the-him-dsp-launches-ninja-ab-a-section-by-section-ab-reference-plugin) · [Guardrail](https://www.production-expert.com/production-expert-1/denormal-audio-launches-guardrail-a-midi-plugin-that-turns-chord-tracks-into-playable-performances) · [Harmonic Echo](https://soundshockaudio.com/harmonic-echo/) · [Phaseburn One](https://synthanatomy.com/2026/08/phaseburn-one-a-multi-engine-synth-plugin-built-around-a-phase-locked-loop.html) · [Radical1](https://synthanatomy.com/2026/01/sound-radix-radical1-a-modular-additive-synthesizer-with-near-infinite-sound-design.html) · [Rippler](https://synthanatomy.com/2026/08/refx-rippler-review-a-modal-synthesizer-with-chromaphone-like-dna.html) · [ChopBeast 2](https://www.bestservice.com/en/chopbeast_2.html)
