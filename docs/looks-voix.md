# Looks de voix — synthèse de recherche (dreamaws, Orbe Looks / Orbe Atelier)

Méthode : recherche web uniquement (aucun audio téléchargé, aucun texte long recopié ; tout est paraphrasé).
Les artistes ne sont cités qu'ici, comme **familles d'écoute** ; ils n'apparaissent ni dans l'interface, ni dans le code, ni dans les presets.

## 1. Sources et fiabilité

| Réf | Source | Type | Ce qu'elle documente réellement |
|---|---|---|---|
| S1 | Sound On Sound, *Inside Track* sur un album pop-électro intimiste (« bad guy ») | Interview d'ingénieur | HP d'entrée, compresseur doux (couleur 670), de-esser réglé vers 6,5 kHz, coupe des médiums 200 Hz–2 kHz sur les doubles, Vocal Rider ≈ 1,5 dB, voix non accordées, album très sec, léger élargisseur |
| S2 | Sound On Sound, *Inside Track* sur un album de rap mélodique (« Draco ») | Interview d'ingénieur | Auto-Tune toujours actif sur l'aux (imprimé après), delay en noires + room 12–15 % / pré-delay 12 ms au tracking ; bus voix : coupe graves + boost > 5,2 kHz, de-ess ≈ 5,5 kHz, compresseur, room 15 ms / 751 ms / coupe HF 9,4 kHz / 15 % wet, léger flanger, limiteur |
| S3 | Sound On Sound, *Inside Track* sur un album rap (« Intro ») | Interview d'ingénieur | Auto-Tune EFX réglage 25 seulement sur refrains/ad-libs, voix principale non accordée ; CLA76 ≈ 3 dB ; bus : coupe 12 dB à 80 Hz, +2 dB 130 Hz, +2,5 dB 5 kHz ; plusieurs de-essers en série ; lo-fi 15 bits ; ad-libs en mono, slap stéréo |
| S4 | Sound On Sound, *Inside Track* sur un album de rap/trap (« Rehab ») | Interview d'ingénieur | Auto-Tune en premier, retune 20 puis 12→5 ; de-ess après ; HP ≈ 96 Hz, coupes 200 et 500 Hz, +6,5 kHz ; delay 1/2 à 43 % de feedback ; reverb « hall » sombre ; limiteur ; voix 1–2 dB au-dessus du beat |
| S5 | Sound On Sound, *Inside Track* sur un album de drill UK | Interview d'ingénieur | Voix rarement accordées (2 titres sur l'album) ; reverb inversée et coupures en effets de genre ; préférence pour la dynamique plutôt que la distorsion. **Aucun chiffre vocal** |
| S6 | Sound On Sound, fiche d'enregistrement d'un album de R&B-rap (Noah « 40 » Shebib) | Interview d'ingénieur | LA-2A léger au tracking, Auto-Tune sur 9 pistes sur 12 « pour centrer » la hauteur, chaîne unique, 4 envois (reverb, delay, exciter, 2e delay). **Aucun chiffre** |
| S7 | Red Bull Music Academy, entretien avec l'ingénieur voix de Future | Interview | Auto-Tune présent par défaut dans le modèle de session, traité comme partie de l'interprétation. **Aucun réglage** |
| S8 | Antares, blog « Auto-Tune for rap/hip-hop » | Éditeur (source primaire de l'outil) | Retune 0–5 = effet dur, 10–25 = mélodique, ≥ 50 = transparent ; ordre micro → compression (3–6 dB) → Auto-Tune → EQ → delay → reverb ; delays copiés de la voix corrigée |
| S9 | Mix with the Masters (fiches vidéo : Tom Elmhirst, Illangelo, Young Guru) | Masterclass | Liste d'outils (saturation agressive, pitch shifter, delays/reverbs variés, ping-pong pour largeur). **Pas de réglages publiés** sur les pages publiques |
| S10 | Sage Audio (« How to mix rap vocals », « lo-fi vocals ») | Blog de studio | HP ≈ 150 Hz, creux 300 Hz, bosse 500 Hz, 2,5–3 kHz, étagère d'air ; envoi aigus saturé en parallèle ; lo-fi = filtrer + saturer les médiums + bitcrush + bruit (valeurs non chiffrées) |
| S11 | Blogs de vendeurs de presets (Rys Up, BCHill, Cedar Sound) pour drill/rage/hyperpop | Commercial, **faible fiabilité** | Valeurs de départ plausibles (HP 80–100 Hz, 2:1–3:1 attaque 10–30 ms, slap 80–120 ms, reverb 0,6–1 s pré-delay 20–50 ms, saturation 5–10 %) ; rage = saturation + détune faible + delay 1/8–1/4 + reverb large ; hyperpop = pitch/formants poussés |
| S12 | MusicRadar, tutoriel hyperpop | Presse | Formants +3 ou (+2 demi-tons & +1,5 formant), enregistrer plus lent puis accélérer. **Pas de chiffres** sur saturation/doubles |
| S13 | r/audioengineering | Forum | Recherche tentée : seuls des instantanés agrégés illisibles → **non utilisé** |

Limites : aucune source publique sérieuse sur la chaîne **réelle** d'un artiste FR précis (recherche faite : seuls des profils de prestataires). Les looks « FR » sont donc des **hypothèses d'écoute** appuyées sur les sources rap générales (S3, S4, S10, S11). Les pages de masterclass ne publient pas de valeurs.

## 2. Les 10 looks (nom original → famille sonore)

Légende : **D** = documenté (source), **H** = hypothèse / valeur plausible. Toutes les valeurs ci-dessous sont des plages de départ ; l'implémentation en tire les réglages de ses 4 macros (Intensité, Espace, Mordant, Couleur).

| # | Nom (produit) | Famille d'écoute (réf. non affichée) | Ordre de la chaîne |
|---|---|---|---|
| 1 | Turbine | Trap/mélodique autotune dur (Future, Travis Scott ; côté FR : trap mélodique) | Comp léger → Tune → EQ → De-ess → Doubles → Delay → Reverb → Limiteur |
| 2 | Béton | Rap sec, proche, compressé (Drake, rap US/FR « punchline ») | EQ → Comp → Sat → De-ess → Slap → pièce courte |
| 3 | Nacre | Chant R&B aérien et large (The Weeknd, SZA ; R&B FR) | Tune doux → EQ → Comp → Sat → De-ess → Doubles larges → Delay → Reverb |
| 4 | Verre Pilé | Hyperpop (100 gecs, Charli XCX) | Tune dur → EQ clair → Comp dur → Sat/crush → Doubles ±interval → Delay court |
| 5 | Cave | Drill sombre (UK/FR) | EQ sombre → Comp → Sat → De-ess → Slap sourd → Reverb courte sombre |
| 6 | Combiné | Voix lo-fi / téléphone / télé | EQ bande étroite → Comp → Sat + bitcrush → Reverb minuscule |
| 7 | Alcôve | Rap FR « murmure » proche (rap chuchoté) | EQ → Comp fort → De-ess fort → micro-doubles → Slap → pièce |
| 8 | Braise | Voix mélodique saturée « rage » (Playboi Carti, Ken Carson) | EQ médiums → Comp → Sat + crush → Tune → Détune ±0,3 → Delay 1/8 → Reverb large |
| 9 | Sépia | Boom-bap chaleureux (US/FR 90s) | EQ chaud → Comp doux → Sat « bande » → De-ess → Slap → Room |
| 10 | Satin | Pop électro alternative intime, doublée (S1) | EQ → Comp doux → Sat discrète → De-ess 6,5 kHz → Doubles → Reverb sèche |

## 2 bis. Les 17 looks ajoutés (vague 2, index 11 à 27)

Ajoutés à la suite des 10 d'origine, qui gardent leur index (compatibilité des projets). La liste de l'éditeur les range par catégorie : **Classiques** (les 10 d'origine), **Rap** (8), **Pop électro** (9). Toutes les valeurs sont des **hypothèses d'écoute (H)**, construites à partir des sources du § 1 et des familles du § 2, sans mesure de chaîne réelle ; à juger à l'oreille dans FL Studio. Colonne « Modules » : ceux que le look déclare (champ `mods` de `Source/Suite/OrbeChain/Looks.h`, lettres gravées sur la face).

| # | Nom | Catégorie | Description (affichée) | Modules |
|---|---|---|---|---|
| 11 | Obsidienne | Rap | Trap sombre, graves pleins, écho de noire | EQ, comp, sat, tune, de-ess, doubles, delay, reverb |
| 12 | Poussière | Rap | Boom-bap voilé, médiums gras, poussière | EQ, comp, sat, de-ess, delay, reverb |
| 13 | Givre | Rap | Voix froide et fine, attaque sèche | EQ, comp, sat, de-ess, delay, reverb |
| 14 | Brume | Rap | Voix flottante dans la brume, large et floue | EQ, comp, sat, tune, de-ess, doubles, delay, reverb |
| 15 | Mirage | Rap | Autotune mélodique lisse et brillant | EQ, comp, sat, tune, de-ess, doubles, delay, reverb |
| 16 | Cassette | Rap | Grain de bande usée, bas débit | EQ, comp, sat, delay, reverb |
| 17 | Ruelle | Rap | Ad-libs : slap et échos de noire en avant | EQ, comp, sat, de-ess, delay, reverb |
| 18 | Gueulante | Rap | Rap frontal, compression serrée, mordant | EQ, comp, sat, de-ess, delay, reverb |
| 19 | Paillettes | Pop électro | Hyperpop étincelant, octave haute, ultra net | EQ, comp, sat, tune, de-ess, doubles, delay, reverb |
| 20 | Rêverie | Pop électro | Pop onirique, halo de réverbération long | EQ, comp, sat, tune, de-ess, doubles, delay, reverb |
| 21 | Opale | Pop électro | Synth-pop nacré, voix lustrée et aérienne | EQ, comp, sat, tune, de-ess, doubles, delay, reverb |
| 22 | Ondes | Pop électro | Voix de radio, bande médium compressée | EQ, comp, sat, delay, reverb |
| 23 | Allô | Pop électro | Voix de téléphone, bande très étroite | EQ, comp, sat, delay, reverb |
| 24 | Soupir | Pop électro | Souffle proche, très présent, très sec | EQ, comp, sat, de-ess, doubles, reverb |
| 25 | Chorale | Pop électro | Chœur large, voix multipliées en éventail | EQ, comp, sat, tune, de-ess, doubles, delay, reverb |
| 26 | Automate | Pop électro | Voix robotique douce, hauteur verrouillée | EQ, comp, sat, tune, de-ess, doubles, delay, reverb |
| 27 | Nocturne | Pop électro | Pop alternatif nocturne, velouté et sombre | EQ, comp, sat, tune, de-ess, doubles, delay, reverb |

## 3. Plages de réglage par look (EQ, dynamique, saturation, hauteur) : les 10 d'origine

| Look | HP (Hz) | Creux / bosses EQ | Étagère aigus | Comp : ratio / attaque / release / GR | Saturation | Tune (retune · gamme) | Statut |
|---|---|---|---|---|---|---|---|
| Turbine | 80–100 **D**(S3,S4,S11) | −2 dB 300 Hz **H** ; +2 dB 3,5 kHz **H** | +2..+5 dB > 5 kHz **D**(S2) | 3–5:1 / 8 ms / 90 ms / 3–6 dB **D**(S8) | douce 5–15 % **H** | retune 0–5 ms (dur) **D**(S8), gamme du morceau | Tune, comp : D ; reste : H |
| Béton | 90 **H** | −1,5 dB 300 Hz ; +1,5 dB 3 kHz | +0..+3 dB 10 kHz | 3–5:1 / 15 ms / 100 ms / 4–6 dB | 5–20 % | aucune | H (S10, S11) |
| Nacre | 110 | −2 dB 250 Hz ; +1 dB 3 kHz | +2..+6 dB 11 kHz (air) **D**(S10) | 2–3:1 / 25 ms / 160 ms / 2–4 dB | tape 8–20 % | retune 25–80 ms (transparent→léger) **D**(S8) | H |
| Verre Pilé | 120 | −2 dB 200 Hz ; +2 dB 5 kHz | +3..+9 dB 10 kHz | 6:1 / 3 ms / 60 ms / 6–10 dB | 30–80 % + crush 14→10 bits | retune 1–4 ms, formants non disponibles (voir §6) | H (S12 pour l'idée) |
| Cave | 80 **D**(S3) | +2 dB 150 Hz ; −2 dB 400 Hz ; +2 dB 3 kHz | −2..−6 dB 9 kHz (sombre) | 4–6:1 / 10 ms / 80 ms | 15–45 % | aucune (**D** S5 : rare en drill) | HP D ; reste H |
| Combiné | 150–400 | +3..+4 dB 1 kHz ; +3 dB 2,5 kHz | −6..−20 dB > 3,4 kHz (équiv. passe-bas) | 5:1 / 5 ms / 70 ms | 30–70 % + crush 12→8 bits **D**(S3: 15 bits, S10) | aucune | S10 qualitatif ; chiffres H |
| Alcôve | 100 | −2 dB 220 Hz ; +1,5 dB 4 kHz | +1..+5 dB 12 kHz | 4:1 / 12 ms / 140 ms / 5–8 dB | 10–30 % | aucune | H |
| Braise | 110 | −2 dB 500 Hz ; +3 dB 2,5 kHz (nasal) **D**(S11) | −1..+3 dB 8 kHz | 4:1 / 20 ms / 70 ms (« attaque moyenne, release rapide ») **D**(S11) | 45–95 %, biais, crush léger | retune 5–25 ms (S4: 5–12) **D** | S11 faible fiabilité |
| Sépia | 70 | +2,5 dB 150 Hz ; −1 dB 350 Hz ; +1,5 dB 2,5 kHz | −5..+2 dB 9 kHz | 3:1 / 20 ms / 120 ms / 3 dB (LA-2A léger **D** S6) | tape 20–60 % | aucune | H |
| Satin | 100 **D**(S1) | −2 dB 400 Hz (médiums 200 Hz–2 kHz **D** S1) ; +1 dB 3,5 kHz | +1..+5 dB 10 kHz | 2,2:1 / 20 ms / 130 ms, doux **D**(S1) | 8–25 % (colorateur léger) **D** | retune 20–60 ms léger (S1 : non accordé → très léger) | HP, comp doux, de-ess : D |

## 4. Plages d'effets de temps et d'espace

| Look | De-ess | Doubles (demi-tons, largeur) | Delay (temps · feedback · niveau) | Reverb (durée · pré-delay · niveau) | Statut |
|---|---|---|---|---|---|
| Turbine | 6,5 kHz, léger **D**(S2 5,5 kHz) | ±0,14 (épaississement) · large | 1/4 noire **D**(S2) · 25–45 % · 12–45 % | 0,8–2,2 s · 12–15 ms **D**(S2) · 10–40 % (S2 : 15 %, 751 ms) | D pour valeurs de S2 |
| Béton | 7 kHz | 0–0,1 niveau | slap 90 ms **D**(S11 : 80–120) · 0–5 % · 5–20 % | 0,5 s · 20 ms · 3–13 % | H |
| Nacre | 7,5 kHz | ±0,2 · très large | 1/8 pointée · 30–50 % · 10–40 % | 1,4–3,8 s · 30 ms · 15–50 % | H |
| Verre Pilé | 8 kHz | +12 et +7 (couches hautes) · 20–60 % | 1/16 · 25 % · 8–28 % | 0,7–2 s · 10 ms · 6–26 % | H |
| Cave | 7 kHz | léger, étroit | slap 110 ms · 10 % · 5–25 % | 0,6–2 s sombre (damp 0,7) · 25 ms · 6–34 % | H ; reverb inversée non reproduite |
| Combiné | aucun | aucun | 1/8 · 0–15 % | 0,5–1,5 s · 5 ms · 5–25 % | H |
| Alcôve | 7 kHz fort | ±0,08 · 15–45 % | slap 70 ms · 0 % · 6–20 % | 0,4–1,3 s · 20 ms · 4–20 % | H |
| Braise | 6,5 kHz | ±0,3 (détune instable) **D**(S11) · 30–70 % | 1/8 · 35–55 % · 10–40 % **D**(S11) | 1,5–4 s large · 20 ms · 10–45 % | S11 |
| Sépia | 6,5 kHz | aucun | slap 100 ms · 15 % · 4–16 % | pièce 0,5–1,5 s · 12 ms · 6–26 % | H |
| Satin | 6,5 kHz **D**(S1 6557 Hz) | ±0,07 « MicroShift » · 35–70 % **D**(S1 doubles, léger élargisseur) | 0–12 % | très sec 0,6–1,8 s · 25 ms · 2–22 % **D**(S1 sec) | D partiel |

## 5. Ordre de chaîne retenu (fixe, Orbe Atelier)

`Tune/Shifter → EQ 4 bandes (+ passe-haut) → Compresseur → Saturation/crush → De-ess → Doubleur → Delay → Reverb → Mix sec/humide → Sortie → Limiteur`

Justification : S8 (compression/Tune avant EQ et effets, delays copiés de la voix corrigée, reverb après delay), S4 (Tune en premier, de-ess après), S2/S4 (limiteur en fin de bus). Écart assumé : la compression pré-Tune de S8 est remplacée par une analyse de hauteur directe sur le signal d'entrée (le détecteur YIN supporte la dynamique brute).

## 6. Hypothèses et limites assumées

- Les valeurs chiffrées des looks sont des **points de départ d'écoute**, pas des mesures de chaînes réelles. Seules les sources S1–S4, S6, S8 donnent des chiffres vérifiables, et elles concernent des **mixes**, pas des artistes « look » entiers.
- **Formants** : non implémentés (le moteur n'a qu'un décaleur de hauteur à fenêtres croisées). Pour l'hyperpop, l'effet est approché par doubles +12/+7, EQ clair et saturation ; à citer comme limite.
- **Reverb inversée / coupures** (drill, S5) et **ad-libs séparés** : hors périmètre d'un effet de piste.
- Entrée stéréo → somme mono pour l'analyse de hauteur (voix) ; sortie stéréo.
