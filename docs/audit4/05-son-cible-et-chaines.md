# Audit 4 / 05 — Son cible et chaînes de traitement (Orbe Autotune)

> Rédigé le 2026-11-10 à partir de recherches web (textes uniquement : manuels, revues, tutoriels, interviews). **Aucune écoute n'a été possible** : tout ce qui suit décrit le son à partir de ce que disent des tiers (fabricants, journalistes, ingénieurs). Les valeurs marquées *(pratique courante)* ne viennent pas d'une source précise et doivent être validées à l'oreille. Aucune modification du code n'a été faite.

---

## Synthèse (à lire en premier)

**Pourquoi Orbe Autotune peut sonner « mid » à côté de MetaTune.** Un correcteur « nu » (détection, puis aimantation vers la note, puis resynthèse) ne fait qu'une partie de ce que l'oreille associe à un « son autotune pro ». Les produits de référence ajoutent trois couches :

1. **Une correction qui dépend du contexte, et pas seulement une vitesse.**
   - MetaTune a une *Speed* qui va de 200 ms à **−3 ms** (en dessous de zéro, c'est un mode « plus dur que dur »). Il a aussi un *Amount* de 0 à 100 % (à 80 %, il reste une tolérance), un réglage *Sustain* qui rend la vitesse dépendante de la durée de la note (valeur positive : les tenues se stabilisent ; valeur négative : le vibrato des tenues est gardé tout en restant dur sur les attaques) et un *Note Stabilizer* qui ignore les notes de moins de 40, 80 ou 200 ms pour éviter le « flutter ».
   - Auto-Tune Pro a des réglages équivalents : *Humanize* (laisse respirer les tenues), *Flex-Tune* (ne corrige que près de la cible), *Natural Vibrato* et *Formant correction*. Il a surtout un **mode Classic**, qui reprend l'algorithme d'Auto-Tune 5, « celui des tubes ». Sound On Sound le juge moins naturel, mais c'est lui qui donne le caractère nostalgique quand la vitesse est rapide. L'Inside Track de *Draco* (Future) confirme qu'Auto-Tune 5 tournait en permanence pendant les prises.
   - Une bonne partie du « son de disque » vient donc du **timbre des artefacts d'un algorithme donné**, et pas de la seule justesse.
2. **Une chaîne intégrée.**
   - MetaTune intègre un *Doubler* : deux voix accordées, avec réglages Width et Wet/Dry.
   - Graillon 3 intègre un compresseur FET/Opto, un gate, un préampli « sibilance », un chorus et un doubleur.
   - Little AlterBoy intègre un *Drive* (le moteur de Decapitator) et un mode Robot pilotable en MIDI.
   - Auto-Tune EFX+ intègre Duet, Vocode, Tube Amp, Mutate et Auto-Motion.
   - Nectar 4 propose une chaîne complète avec Voices (harmonies tonales), Backer (doublures « autre chanteur ») et un assistant qui règle le tout.
   - Un autotune qui sonne « complet » à l'écoute en solo est donc presque toujours un autotune **plus de l'épaisseur** (doubles), **plus de la couleur** (saturation légère) et **plus de l'espace**.
3. **Ergonomie et presets.**
   - Un *heatmap* d'entrée et de sortie, la détection de tonalité (Auto-Key ou Nectar), des presets classés (subtil, créatif, doublage) et un A/B intégré.
   - Des groupes d'instances liées : MetaTune en gère jusqu'à 4.

**Ce que les sources disent « pro » ou « cheap ».**
- Les mêmes causes du son robotique reviennent partout, sauf quand c'est voulu :
  - une vitesse trop rapide appliquée partout ;
  - le mode chromatique, qui tire une dérive vers une mauvaise note ;
  - un formant qui suit la hauteur (effet « chipmunk ») ;
  - une correction toujours active sur des notes qui n'en avaient pas besoin ;
  - un vibrato écrasé.
- Le « warble » ou le côté « phasey » apparaissent quand on pousse un correcteur temps réel vers la transparence (cas d'Auto-Tune Pro, selon Nail The Mix).
- Le *comb filtering* apparaît quand on mélange une voix accordée avec une copie non accordée qui n'est pas parfaitement alignée.
- Les vibratos larges posent problème à tous les correcteurs temps réel (Sound On Sound sur MetaTune).

**Plages de réglage qui reviennent.**

| Usage | Vitesse (sources Slate et Antares) |
|---|---|
| Effet dur | 0 à 10 ms (0 à 5 pour le lead « à la Travis ») |
| Melodic trap | 10 à 25 (Future : 15 à 20 selon Antares) |
| Zone « accidentelle », à éviter | 20 à 40 |
| Pop naturelle | 40 à 60 ms |
| R&B | 60 à 90 ms |
| Indie | 80 à 120 ms |
| Stacks d'harmonies | 30 à 50 ms |

- Doubles et harmonies : décaler leur vitesse de 3 à 5 points par rapport au lead.
- Ad-libs : plus durs que le lead.
- Trap : Flex-Tune coupé.
- Chaîne type d'Antares : compresseur, puis autotune (sur la voix sèche), EQ, delay, réverb. L'harmoniseur vient **après** l'autotune, pour que les harmonies soient construites sur le lead corrigé.

**Recommandations prioritaires pour Orbe Autotune** (détails en section 4) :
1. **Sustain et stabilisateur de notes.** Une vitesse dépendante de la durée de note et un seuil minimal de note (40, 80 ou 200 ms). C'est le différenciateur le plus cité de MetaTune.
2. **Amount (tolérance) et Flex**, séparés de la vitesse, ainsi qu'une **vitesse intra-note distincte de la transition entre notes** : Waves Tune Real-Time sépare *Speed* (défaut 15 ms) et *Note Transition* (défaut 120 ms).
3. **Un caractère « Classic » ou « hit »** : un mode de resynthèse volontairement moins transparent, avec des transitions en marches nettes et sans lissage de formant, à concevoir comme une couleur et non comme un défaut.
4. **Un doubleur intégré** (2 voix, Width, Wet/Dry, décalage ±quelques cents et 10 à 30 ms, vitesse décalée) et des **harmonies tonales après correction** (tierce ou quinte diatonique, octave basse).
5. **Une couleur** : un drive léger (tube ou bande) après la correction, de l'« air » (shelf au-delà de 5 kHz environ) et un de-ess interne, pour ne pas faire siffler le haut des voix accordées.
6. **De l'espace synchronisé au tempo** : un delay à la noire (2 à 3 répétitions, 15 à 20 % de feedback) et une réverb sombre amortie, sur un bus interne avec un mix global.
7. **Des presets par style** (tableau en section 3), un heatmap, une détection de tonalité et un **A/B à niveau égal** contre une « référence » (un rendu MetaTune fait par l'artiste sur sa propre voix).
8. **Une validation à l'aveugle** (MUSHRA ou ABX à loudness égale) plus quelques métriques objectives : temps de transition, dispersion en cents des tenues, distance d'enveloppe spectrale, artefacts sur les attaques et les sibilantes. Une étude sur le chant (Gupta et al., 2015) montre que les mesures objectives classiques corrèlent mal avec la qualité globale perçue : **l'écoute reste l'arbitre**.

**Limites.**
- Pas d'écoute : impossible de dire si Orbe est « métallique » ou seulement « sec ».
- Les valeurs d'artistes viennent de tutoriels de fabricants (Antares, Slate) et sont en partie marketing.
- Les « retune speed » ne sont pas toujours dans les mêmes unités d'un plugin à l'autre.
- Pour Playboi Carti, les sources décrivent surtout une **performance vocale**, et non un réglage connu.

---

## 1. Ce que les plugins de référence ajoutent à un correcteur « nu »

### 1.1 Tableau des fonctions

| Plugin | Correction (au-delà de la vitesse) | Timbre et formant | Effets intégrés | Ergonomie | Sources |
|---|---|---|---|---|---|
| **Slate MetaTune** | Speed de 200 à −3 ms (0 à −3 = « plus dur », sans latence ajoutée) ; Amount de 0 à 100 % ; **Sustain** (± : tenues stabilisées, ou vibrato préservé) ; **Note Stabilizer** (40/80/200 ms, +280 ms de latence) ; Low Latency ; chromatique ou gamme, notes activables | (pas de formant documenté) | **Doubler** : 2 voix accordées, Width, Wet/Dry (50 % = 3 voix égales) | Orb (vitesse et quantité sur un seul contrôle), **heatmaps** entrée/sortie, générateur de tonalité, A/B, Undo, presets (« creative, subtle, doubling »), **Groups** (4), Keep My Scale | [Manuel Slate], [SOS MetaTune] |
| **Antares Auto-Tune Pro / Pro X** | Retune Speed, **Humanize**, **Flex-Tune**, **Natural Vibrato** (réduit ou amplifie), vibrato artificiel à 7 paramètres, Tracking, Transpose, Detune ; **Classic mode** (algorithme Auto-Tune 5) ; Graph mode | Formant correction, **Throat** (longueur de conduit) | (dans Pro) aucun ; EFX+ : Duet, Vocode (10 vocodeurs), Tube Amp, Mutate, HPF, Auto-Motion | Auto-Key (envoie la tonalité à toutes les instances), presets, ARA | [SOS Auto-Tune Pro], [SOS EFX+], [Antares trap] |
| **Auto-Tune Access / Artist / Hybrid** | Gammes simplifiées (Access) ; Artist pour la prise et le live ; Hybrid ajoute des modes « hybrides » | Throat et formant selon la version | — | — | [Manuels Artist et Hybrid] (non lus en détail) |
| **Waves Tune Real-Time** | **Speed** de 0,1 à 800 ms (défaut 15) **séparée de Note Transition** de 0,1 à 800 ms (défaut 120) ; Correction 1 à 100 % (ce n'est pas un dry/wet) | Formant Corrected / Non-Corrected | Vibrato On/Off et Depth de 0 à 200 % | Link vitesse/transition | [Nilo, WTRT] |
| **Auburn Graillon 3** | 3 moteurs (G2, G3, I1 « naturel », 7 fois plus lent) ; Snap Min (préserve le vibrato) ; réglage fin par note ±100 cents | Formant avec contour réglable de 0 à 100 % | **Compresseur FET/Opto, Gate, Preamp « sibilance », Chorus, Doubler** (version Full) | Gratuit en partie | [Auburn Graillon 3], [SOS round-up] |
| **Soundtoys Little AlterBoy** | Modes Transpose, **Quantize** (hard-tune), **Robot** (une note, pilotable en MIDI) | Pitch et **Formant** séparés | **Drive** (Decapitator), Mix | Très simple, 4 curseurs | [PLAP LAB], [SOS round-up] |
| **iZotope Nectar 4** | Module Pitch | — | Chaîne complète (2 comp, 2 EQ, gate, de-esser, saturation, delay, réverb, Dimension), **Voices** (harmonies tonales et détection de tonalité), **Backer** (doublures « personas »), Auto Level | **Vocal Assistant** (chaîne automatique), Audiolens (profil d'une référence) | [SOS Nectar 4] |
| **Melodyne / Revoice Pro** | Hors ligne, note par note ; Revoice : alignement et doubles | Formant et dérive fins | — | — | [Nail The Mix], [Revoice] |
| **Créatifs « hyperpop »** (Humanoid, Pitch Monster, Vocal Bender, OVox, The Mouth) | Robotify, Chord mode, unison jusqu'à 8 voix, LFO et step-sequencer de hauteur | Formant, resynthèse, wavetable | Vocodeur, synthé, bitcrush | Pilotage MIDI | [SOS round-up] |

### 1.2 Ce que les utilisateurs et revues associent à « pro » ou à « cheap »

- **Pro / naturel** :
  - une vitesse lente qui garde les scoops, glissés et vibratos ;
  - le formant maintenu ;
  - une gamme réglée sur le morceau (pas en chromatique) ;
  - une correction uniquement là où c'est utile ([Slate, robot voice]).
  - Greg Wells monte jusqu'à ~109 (unités Auto-Tune) en phrase par phrase et utilise le tracking « Choosy » pour corriger moins de notes ([Puremix]).
  - La gestion du vibrato est la grande force de MetaTune selon Sound On Sound.
- **Pro / effet assumé** :
  - transitions nettes ;
  - tenues « clampées » ;
  - gamme restreinte, voire quelques notes seulement (« T-Pain » : [MusicRadar T-Pain]).
- **Cheap** :
  - zone 20 à 40, le « on dirait un accident » ([Antares trap]) ;
  - « chipmunk » dû au formant non corrigé ([Slate]) ;
  - warble et phasing en mode transparent ;
  - comb filtering entre voix accordée et voix sèche décalée ([Nail The Mix]) ;
  - flutter sur les notes courtes (raison d'être du Note Stabilizer) ;
  - vibrato large qui « saute » ([SOS MetaTune]).
- Le **comportement de l'artiste** compte : Charli XCX explique qu'elle « se laisse aller » dans la note parce qu'elle chante toujours à travers l'Auto-Tune, et que son producteur ajoute volontairement de l'imprécision ailleurs (modulation un peu fausse, voix pitchées) pour rester humain ([MusicRadar Brat]). Pour Kanye (*808s*), Jeff Bhasker décrit le son « heartbreak » comme distorsion, octave et Auto-Tune ([Rolling Stone 808s]).

---

## 2. Chaînes vocales réelles et recettes par style

### 2.1 Ordre de chaîne de référence

D'après Antares (trap) : préampli, **compresseur, autotune (sur la voix sèche), EQ, delay, réverb**. L'harmoniseur se place **après** l'autotune.

Variante documentée (Inside Track, Future, *Draco*, [SOS Draco]) :

- **À la prise** : Auto-Tune 5 toujours actif (imprimé), puis Extra Long Delay et D-Verb (delay à la noire, 12 à 15 % wet, pré-délai de 12 ms), uniquement dans le casque.
- **Par piste** : Mod Delay, AC202 (le canal analogique sert de « rider » et donne de la couleur), compresseur opto.
- **Bus voix** : EQ (coupe des basses, boost au-delà de 5,21 kHz), puis **de-esser à 5 506 Hz**, compresseur Renaissance, D-Verb (Medium Room, 15 ms de pré-délai, 751 ms, coupe-haut à 9,36 kHz, 15 % wet), **flanger léger**, puis limiteur L1 « pour sortir le haut ».

Ordre proposé pour la chaîne intégrée d'Orbe (synthèse ; à valider à l'oreille) :
`gate de souffle → comp léger → AUTOTUNE → formant/throat → drive doux → de-ess → EQ (air) → [envois : doubleur / harmonies / delay / réverb] → mix`.
Le de-ess vient **après** le drive et l'air, car ces deux étages accentuent les sibilantes (*pratique courante*).

### 2.2 Recettes par style (descriptions publiques uniquement)

- **T-Pain (hard tune historique)** :
  - vitesse 0 ;
  - majeur ou mineur réglé (sinon la gamme n'agit pas) ;
  - version extrême : seulement quelques notes actives ([MusicRadar T-Pain]).
  - T-Pain cite le talk-box de Roger Troutman comme influence ([Berklee]). Ce son a donc une logique d'**instrument**, pas de correction.
- **Travis Scott (selon Antares, marketing)** :
  - vitesse de 0 à 5 ;
  - throws de delay à la noire ;
  - réverb sombre en nappe.
  - Pas de réglage confirmé par Mike Dean dans les sources lues.
- **Future** :
  - Auto-Tune 5 toujours actif à la prise ([SOS Draco], [RBMA Firkins]) ;
  - Antares suggère une vitesse de 15 à 20 avec une compression lourde à attaque lente et une saturation bande ou tube (pas de clip dur) ;
  - delay à la noire, réverbs courtes, flanger.
- **Kanye, *808s & Heartbreak*** : Auto-Tune utilisé comme instrument (même sur scène), plus distorsion et octave ([Rolling Stone 808s]). Aucun réglage public.
- **Playboi Carti (« baby voice »)** : décrite comme une **interprétation** aiguë et mâchée, et non comme un effet identifié ([Complex]). On peut l'approcher par une montée de formant de 1 à 3 demi-tons et un pitch de 0 à 2 demi-tons (*hypothèse*), mais rien de sourcé.
- **Melodic trap (empilement)** ([Antares trap]) :
  - 3 à 5 couches ;
  - lead dur (0 à 5) ou souple (10 à 25) ;
  - doubles plus souples, ad-libs plus durs ;
  - stack d'harmonies serré ;
  - compression avec 3 à 6 dB de réduction ;
  - présence entre 2 et 5 kHz ;
  - delay principal à la noire (2 à 3 répétitions, 15 à 20 % de feedback), plus un delay pointé ou à la double croche plus discret.
- **Hyperpop / PC Music** :
  - vitesse 0 et formant à +7,2 demi-tons pour l'effet « chipmunk » ([NI hyperpop]) ;
  - variante avec moins d'artefacts : pitch à +2 et formant à +1,5, ou enregistrer plus lent et plus grave puis varispeed ([MusicRadar hyperpop]) ;
  - saturation pour avancer la voix ;
  - contraste entre voix très proche et ambiance écrasée ([Mixdown]).
- **Bon Iver / Messina, Prismizer** : un harmoniseur temps réel (logiciel plus matériel) pour des chœurs « clones » joués en direct ; le Prismizer de Francis Starlite produit un effet voisin, mais avec de la latence ([W Magazine]). Il faut le traduire par des **harmonies multi-voix pilotées par MIDI ou par la tonalité, avec formant fixe**.
- **Frank Ocean (*Nikes*)** : voix pitchée vers le haut (des fans l'ont abaissée de 5 demi-tons pour retrouver le timbre naturel) ([Hypebeast]). Cela confirme l'intérêt d'un pitch global avec un formant indépendant.
- **Kali Uchis, Lil Uzi Vert** : pas de source technique fiable trouvée. Ne pas inventer.

### 2.3 Doubles, harmonies, largeur

- **Doubles** :
  - décalage de quelques cents et de quelques ms ; sous 30 ms environ, on entend un seul événement, au-delà un écho ([iZotope doubling], exemple à ±25 ms) ;
  - les doubles identiques restent au centre : il faut un décalage de hauteur **et** de temps pour élargir ;
  - les doubles d'autotune gagnent à avoir une vitesse décalée de 3 à 5 points ([Antares]).
  - Le Doubler de MetaTune fait 2 voix, en L et en R.
- **Harmonies** : après correction ; tierce ou quinte diatonique, octave basse pour « heartbreak » ou dark trap, octave haute pour l'hyperpop (*pratique courante*). Stack à 30 ou 50 ms ([Slate]).
- **Delay et réverb** : synchronisés au tempo ; réverb filtrée, plutôt sombre (coupe-haut entre 9 et 10 kHz chez Future).

---

## 3. Vocabulaire et tableau de profils cibles

**Vocabulaire d'écoute** (pour les retours de l'artiste) :

| Mot | Description |
|---|---|
| *snap* / marches | Transitions instantanées |
| *glide* | Portamento conservé |
| *clampé* | Tenue figée, sans vibrato |
| *warble* | Oscillation parasite sur les tenues |
| *flutter* | Sautillement sur les notes courtes |
| *chipmunk* / *gros* | Formant haut ou bas |
| *métallique* / *phasey* | Resynthèse ou comb filtering |
| *granuleux* | Grain de la resynthèse, souvent sur le souffle et les sibilantes |
| *sec* / *nu* | Ni doubles, ni couleur, ni espace |
| *air* | Brillance au-delà de 10 kHz |
| *présence* | Zone de 2 à 5 kHz |
| *body* | Zone de 150 à 300 Hz |

Les valeurs ci-dessous sont **indicatives** et à recaler à l'écoute. Les unités de vitesse sont des ms, comme sur MetaTune.

| Profil | Vitesse | Amount / Flex | Sustain / Stab. | Formant | Doubles | Harmonies | Couleur | Espace | Référence textuelle |
|---|---|---|---|---|---|---|---|---|---|
| **« MetaTune hard-tune rap »** | 0 à −3 | 100 % / off | Sustain + ; Stab. Short | corrigé | Doubler 30 à 40 % wet, width 70 à 100 % | — | drive léger | delay à la noire, 15 à 20 % de feedback ; réverb sombre | [Manuel Slate], [Antares trap] |
| **« Melodic trap »** | 10 à 25 | 100 % / off | Sustain − (garde le vibrato) | corrigé | 2 doubles à vitesse décalée | tierce diatonique, discrète | saturation bande ou tube | delay à la noire et au pointé | [Antares trap], [SOS Draco] |
| **« Satin naturel » (pop, R&B)** | 40 à 90 | 70 à 85 % / Flex on | Stab. Mid | corrigé | faible ou nul | stack 30 à 50 ms | air, de-ess | réverb plate courte | [Slate robot voice], [Puremix] |
| **« Classic hit »** (années 2008 à 2016) | 0 à 5 | 100 % | — | **non corrigé** | 1 double | octave basse (option) | distorsion douce | delay à la noire | [SOS Auto-Tune Pro, Classic], [Rolling Stone 808s] |
| **« Hyperpop métal »** | 0 | 100 %, peu de notes | — | +3 à +7 demi-tons | unison large | octave haute | drive franc, bitcrush léger | courts et brillants | [NI hyperpop], [MusicRadar hyperpop] |
| **« Chœur Messina »** | 10 à 30 | 100 % | — | fixe | — | 3 ou 4 voix via MIDI ou la tonalité | — | réverb large | [W Magazine] |
| **« Robot / talk-box »** | 0 | Robot (une note ou MIDI) | — | non corrigé | — | vocodeur léger | drive | — | [PLAP LAB], [SOS EFX+] |

---

## 4. Idées concrètes pour qu'Orbe Autotune « sonne complet »

Classées par impact supposé sur l'écoute en solo.

1. **Contrôles de correction façon MetaTune / Auto-Tune**
   - **Sustain** : vitesse modulée par la durée de la note.
   - **Note Stabilizer** (40, 80 ou 200 ms), avec une version sans latence si possible ; MetaTune accepte 280 ms et le désactive en Low Latency.
   - **Amount**, distinct de la vitesse.
   - **Flex** : zone morte autour de la cible.
   - **Humanize** et **Natural Vibrato** (de 0 à 200 %, comme WTRT).
   - **Speed intra-note séparée de Transition**.
   - **Vitesse négative**, en mode « overshoot » ou marche plus franche.
2. **Mode « Classic / Hit »** : une resynthèse volontairement moins transparente, avec des marches nettes et sans correction de formant. L'objectif est d'imiter l'*effet* attribué à Auto-Tune 5, sans prétendre le cloner.
3. **Formant et Throat** : formant indépendant (±12 demi-tons) avec une option « formant corrigé » ; longueur de conduit (Throat) pour le caractère.
4. **Doubleur intégré** :
   - 2 voix en L et R ;
   - Width et Wet/Dry façon MetaTune ;
   - micro-désaccord de ±5 à 15 cents (*pratique courante*) ;
   - délais de 10 à 30 ms, différents entre L et R ;
   - vitesse de correction décalée ;
   - surveillance de la compatibilité mono.
5. **Harmonies tonales après correction** : intervalles diatoniques calculés à partir de la gamme (tierce, quinte, octave ±), 1 à 4 voix, formant fixe, entrée MIDI optionnelle (mode Messina).
6. **Couleur** :
   - drive doux après la correction (tube ou bande, comme le Drive de Little AlterBoy ou le Tube Amp d'EFX+) ;
   - air en shelf ;
   - de-esser intégré **après** le drive, pour masquer le grain sur les sibilantes.
7. **Gate de souffle et de bruit avant détection** (comme le Vocal Gate de Graillon) : il réduit le flutter et les fausses détections sur les respirations. On peut proposer en option une « respiration conservée » non accordée.
8. **Espace lié au tempo et à la tonalité** : delay à la noire, au pointé ou à la double croche (feedback de 15 à 20 %, filtré) et réverb sombre, sur un bus interne ; en option, des queues de delay accordées dans la gamme.
9. **Vocodeur léger / Robot** : une note fixe ou MIDI (Little AlterBoy Robot, EFX+ Vocode).
10. **Ergonomie** :
    - **heatmap** entrée/sortie ;
    - détection de tonalité ;
    - presets par style (section 3) avec option « Keep My Scale » ;
    - **A/B à loudness égale** ;
    - groupes d'instances (lead, doubles, ad-libs) ;
    - un mode « référence », qui charge un fichier rendu par l'artiste avec sa propre chaîne MetaTune et l'aligne pour une comparaison aveugle.

---

## 5. Méthodologie d'écoute et de validation

**Écoute (prioritaire).**
- **Corpus** : 6 à 10 phrases de l'artiste lui-même :
  - rap parlé-chanté ;
  - tenues avec vibrato ;
  - mélismes rapides ;
  - sibilantes et souffles ;
  - registre grave et registre aigu.
- **Pour chaque profil**, rendre trois versions :
  - Orbe ;
  - la référence (MetaTune, rendu par l'artiste) ;
  - une ancre basse (Orbe nu, vitesse 0, sans chaîne).
- **Égaliser la loudness** (ITU-R BS.1770, à ±0,5 LU près), sinon le plus fort gagne.
- **Test** : MUSHRA (ITU-R BS.1534) avec la référence cachée et l'ancre, ou ABX ou préférence à l'aveugle pour un seul auditeur. Au moins 2 écoutes, au casque et sur haut-parleurs de téléphone.
- **Grille de notation** :
  - justesse perçue ;
  - naturel ou caractère voulu ;
  - artefacts (métal, warble, flutter, grain) ;
  - épaisseur et largeur ;
  - place dans le mix (sur l'instru) ;
  - envie d'utiliser.

**Mesures objectives (aides, pas verdict).**

| Mesure | Ce qu'elle vérifie |
|---|---|
| Distribution en cents des tenues après correction | Écart-type selon le profil ; un profil hard doit être presque nul, un profil naturel doit garder le vibrato |
| Temps de transition de 10 à 90 % entre notes | Comparé à la vitesse affichée |
| Taux de micro-changements de note de moins de 80 ms | Mesure du flutter |
| Distance d'enveloppe spectrale entre entrée et sortie (MFCC ou log-spectral distance) | Préservation des formants |
| Énergie des artefacts au-dessus de 5 kHz sur les sibilantes, et étalement des attaques | Grain et attaques baveuses |
| Corrélation L/R et somme mono des doubles | Comb filtering |
| Latence et alignement avec le signal sec | Phasing en parallèle |
| LUFS et crest factor | Contrôle du niveau pour l'A/B |

Gupta et al. (2015) montrent, sur le chant, que les mesures objectives corrèlent au mieux avec l'intelligibilité et **mal avec la qualité globale**. Ces métriques servent à détecter des régressions, **pas à décider** si « ça sonne pro ».

**Critère de sortie proposé.** Sur chaque profil, à l'aveugle, Orbe n'est pas significativement moins bien noté que la référence par l'artiste et au moins une autre oreille, et aucun artefact n'est noté « gênant » sur le corpus.

---

## 6. Limites honnêtes

- Aucune écoute : ni d'Orbe, ni des références. Ce document décrit ce que **disent** les sources.
- Une partie des recettes d'artistes vient de **blogs de fabricants** (Antares, Slate), donc de contenus marketing ; les seules chaînes réellement documentées par un ingénieur sont celles de Future (*Draco*, SOS) et quelques témoignages (*808s*, Messina).
- Les échelles de « retune speed » diffèrent : Auto-Tune utilise des unités proches de la milliseconde, de 0 à 400, MetaTune des ms de 200 à −3, WTRT des ms de 0,1 à 800.
- Je n'ai trouvé aucune source technique fiable pour Carti, Lil Uzi ou Kali Uchis.
- Manuels Auto-Tune Artist et Hybrid, Revoice : non lus en détail.

---

## Bibliographie

- [Manuel Slate] Slate Digital, *MetaTune — Documentation*. https://docs.slatedigital.com/MetaTune/MetaTune.html
- [Slate robot voice] Slate Digital, *Vocal tuning without the robot voice: a producer's guide*. https://slatedigital.com/vocal-tuning-without-the-robot-voice-a-producers-guide/
- [SOS MetaTune] Sound On Sound, *Slate Digital MetaTune* (revue). https://www.soundonsound.com/reviews/slate-digital-metatune
- [SOS Auto-Tune Pro] Sound On Sound, *Antares Auto-Tune Pro* (revue). https://www.soundonsound.com/reviews/antares-auto-tune-pro
- [SOS EFX+] Sound On Sound, *Antares Auto-Tune EFX+* (revue). https://www.soundonsound.com/reviews/antares-auto-tune-efx
- [SOS round-up] Sound On Sound, *Creative Pitch-correction Plug-ins*. https://www.soundonsound.com/reviews/creative-pitch-correction-plug-ins
- [Antares trap] Antares, *Auto-Tune for rap & hip hop: how trap producers use pitch correction*. https://www.antarestech.com/blog/auto-tune-for-rap-hip-hop-how-trap-producers-use-pitch-correction
- [Antares Pro X] Antares, *Auto-Tune Pro X tutorial overview*. https://www.antarestech.com/blog/auto-tune-pro-x-tutorial-overview
- [Manuels Artist et Hybrid] Antares, *Auto-Tune Artist manual* (PDF). https://www.barryrudolph.com/recall/manuals/auto-tune_artist.pdf ; *EFX+ manual* : https://www.barryrudolph.com/recall/manuals/auto-tune_efx_plus.pdf
- [Nilo, WTRT] Joseph Nilo, *Waves Tune Real-Time explained*. https://josephnilo.com/blog/waves-tune-real-time-explained/
- [Auburn Graillon 3] Auburn Sounds, *Introducing Graillon 3*. https://www.auburnsounds.com/blog/Introducing-Graillon-3.html
- [PLAP LAB] Produce Like A Pro, *Little AlterBoy review*. https://producelikeapro.com/blog/little-alterboy-review/
- [SOS Nectar 4] Sound On Sound, *iZotope Nectar 4 Advanced*. https://soundonsound.com/reviews/izotope-nectar-4-advanced
- [Nail The Mix] Nail The Mix, *The Best Autotune Plugin: It's Not What You Think*. https://www.nailthemix.com/best-autotune-plugin
- [Revoice] AudioTechnology, *Review: Revoice Pro 4*. https://www.audiotechnology.com/reviews/revoice-pro-4
- [Puremix] Puremix, *Auto-Tune vocals with Greg Wells*. https://puremix.com/blog/auto-tune-vocals-greg-wells
- [SOS Draco] Sound On Sound, *Inside Track: Future « Draco »*. https://www.soundonsound.com/techniques/inside-track-future-draco
- [RBMA Firkins] Red Bull Music Academy Daily, *Seth Firkins on being Future's vocal producer and the power of Auto-Tune*. https://daily.redbullmusicacademy.com/2017/08/seth-firkins-interview
- [Rolling Stone 808s] Rolling Stone, *The Raw, Rushed Making of '808s & Heartbreak'*. https://www.rollingstone.com/music/music-features/making-of-808s-heartbreak-759011/
- [FADER Mike Dean] The FADER, *Mike Dean on his approach to producing for Travis Scott and Kanye West* (citée par Antares, sans réglages). https://www.thefader.com/2018/10/08/mike-dean-interview-travis-scott-and-kanye-west
- [MusicRadar T-Pain] MusicRadar, *The T-Pain Auto-Tune effect explained once and for all*. https://musicradar.com/computermusic/the-t-pain-auto-tune-effect-explained-once-and-for-all-246132
- [Berklee] Berklee Now, *A Sonic History of Auto-Tune According to T-Pain*. https://www.berklee.edu/news/berklee-now/sonic-history-auto-tune-according-t-pain
- [Complex] Complex, *The History of Playboi Carti's Baby Voice*. https://www.complex.com/music/a/greg-gaffney/playboi-carti-baby-voice-history
- [MusicRadar Brat] MusicRadar, *Charli XCX, Auto-Tune and Brat*. https://musicradar.com/news/charli-xcx-auto-tune-brat
- [NI hyperpop] Native Instruments Blog, *Hyperpop*. https://www.native-instruments.com/blogs/blog/hyperpop
- [MusicRadar hyperpop] MusicRadar, *How to make supersized hyperpop vocals*. https://www.musicradar.com/tutorials/the-main-lead-is-often-pushed-and-pulled-in-as-many-directions-as-possible-to-create-the-characteristic-machine-like-sound-how-to-make-supersized-hyperpop-vocals
- [Mixdown] Mixdown, *A beginner's guide to hyperpop production*. https://mixdownmag.com.au/features/a-beginners-guide-to-hyperpop-production/
- [W Magazine] W Magazine, *The engineer behind Bon Iver's 22, A Million…*. https://www.wmagazine.com/story/the-engineer-behind-bon-ivers-22-a-million-clears-up-any-confusion-about-its-high-tech-sound/amp
- [Hypebeast] Hypebeast, *Hear Frank Ocean's "Nikes" pitched down five semitones*. https://hypebeast.com/2016/10/frank-ocean-nikes-pitched-down
- [iZotope doubling] iZotope, *Basic vocal doubling techniques*. https://izotope.com/en/learn/basic-vocal-doubling-techniques
- [Gupta 2015] Gupta et al., *On the Perceptual Relevance of Objective Source Separation Measures for Singing Voice Separation* (2015). https://musicinformatics.gatech.edu/wp-content_nondefault/uploads/2015/10/Gupta-et-al_2015_On-the-Perceptual-Relevance-of-Objective-Source-Separation-Measures-for-Singing.pdf
- [ITU BS.1534] ITU-R BS.1534-3 (MUSHRA). https://www.itu.int/dms_pubrec/itu-r/rec/bs/R-REC-BS.1534-3-201510-I!!PDF-E.pdf
- [ITU BS.1770] ITU-R BS.1770 (mesure de loudness), référence standard citée sans lecture détaillée. https://www.itu.int/rec/R-REC-BS.1770
