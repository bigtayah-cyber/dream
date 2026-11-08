# Audit dreamaws — suite de plugins JUCE (24 plugins)

*Date : 8 nov. 2026. Lecture seule : aucun fichier source n'a été modifié.*
*Méthode : captures PNG de chaque face avant (outils `*_snapshot`), lecture des éditeurs, des moteurs DSP et du kit graphique, comparaison avec les prototypes web.*
*Limite : rien n'a été écouté dans FL Studio. Les notes sur le son viennent du code et des tests hors-ligne, pas d'une écoute.*

Captures utilisées (dossier `/tmp/claude-0/-home-claude/12ae9892-af71-5ca8-a97b-93f9fd958b0b/scratchpad/`) : `audit/voile.png`, `audit/orbe.png` et `audit/synth.png` (régénérées pour l'audit), `k0.png` (un potard, version actuelle), `onek_all.png` (les 4 côte à côte), `cab.png`, `mas.png`, `eq.png`, `deesser.png`, `denoiser.png`, `at/at1.png`, `oc/looks0.png`, `oc/looks3.png`, `oc/at4.png` (Atelier), `an_detect.png`, `an_mix.png`, `an_fredonne.png`, et dans `/tmp/` : `o808.png`, `beat.png`, `pads1.png`, `smp1.png`.
Note : les anciennes captures `voile.png` et `orbe1.png` montraient des accents cassés (« TONALITÃ© ») ; ce défaut est **corrigé** dans le code actuel (`kit::u8`). Il ne reste rien à faire sur ce point.

---

## 1. AUDIT VISUEL (priorité n° 1)

### 1.1 Verdict global

**Ce qui fait déjà « vrai produit »**
- Une vraie grammaire commune : plaque, vis dans les coins, afficheurs encastrés, voyants, texte en capitales espacées. La suite se reconnaît.
- Les faces d'analyse sont crédibles : Orbe EQ (courbe et poignées façon Pro-Q, info-bulle de bande), Master (courbe, barres-graphes, LUFS), Denoiser et DeEsser (spectres entrée/sortie), 808 (forme d'onde et clavier), Beat (grille par pistes colorées).
- Le VU crème des plugins à un potard (`OneKnobEditor.cpp:99`) est l'élément le plus « analogique » de toute la suite.
- Bonnes valeurs affichées sous les potards (unités, virgule française), redimensionnement à ratio fixe partout.

**Ce qui fait « fait maison / vibe code » (par ordre d'impact)**
1. **Un seul potard pour 24 plugins** (`Kit.h:132`) : noir moleté, anneau de valeur coloré lumineux, index coloré. C'est le potard générique de plugin logiciel (Serum, Vital), pas celui d'une machine. Aucune variante : pas de bouton à bec (chicken-head), à jupe, à capuchon aluminium ou cranté.
2. **Police à chasse fixe du système partout** (`Kit.h:38`, `getComboBoxFont` l. 276) : noms de presets, listes (« Trap », « Mineur », « Auto »), valeurs. Sur Mac c'est Menlo, dans les captures DejaVu Sans Mono : ça évoque un éditeur de code. La police d'interface n'est pas embarquée (`family()` l. 26 choisit Helvetica Neue, Inter ou Arial), donc le rendu change d'une machine à l'autre.
3. **Plaque en métal brossé peu crédible** (`Kit.h:70`) : bruit blanc par pixel + marche aléatoire par ligne. On obtient du « grain de sable rayé », pas des stries d'aluminium. Le bruit est ajouté en absolu (`+ n/255`), ce qui grise les plaques sombres. Pas de reflet, pas de chanfrein, pas d'oreilles de rack. Même graine (7) pour tous les plugins. Surtout, l'image est générée en pixels **logiques** (`brushedPlate(getWidth(), getHeight())`) : elle est **floue sur Retina**, alors que l'utilisateur est sur Mac.
4. **Faces « tableau de bord web »** : Mix, Détect et Fredonne sont faits de cartes arrondies, pastilles, jauge de score et phrases d'aide. Ça ressemble à une page web, pas à un appareil de mesure.
5. **Texte d'aide imprimé sur la face** : Autotune (« Hauteur chantée (bleu)… »), EQ (barre d'astuces), Fredonne (« Arme, puis fredonne. »), Détect (« Touche une case… »), Looks (« Auto : aucune correction… »), Voile (« la voix est recalée… »), latences du DeEsser et du Denoiser. Une machine n'imprime jamais de mode d'emploi : ces textes vont dans les info-bulles ou une aide en surimpression (touche ?).
6. **Palette pastel néon unique** (lilas, glace, pêche sur anthracite) pour ~16 plugins. C'est cohérent, mais « app IA » ; et dans le wrapper de FL, ces plugins sont indiscernables d'un coup d'œil.
7. **Détails cassés** :
   - La poignée de redimensionnement (hachures) **recouvre la vis en bas à droite** sur toutes les captures.
   - Le numéro de série **DW-701 sert deux fois** (Denoiser, `DenoiserEditor.cpp:122`, et Autotune, `AutotuneEditor.cpp:333`).
   - Les numéros DW flottent à des places différentes selon le plugin.
   - Voile a des flèches ASCII `<` `>` sur ses boutons de preset.
   - Les voyants éteints sont presque invisibles (lentille noire).
8. **Vis** (`Kit.h:91`) : tête fendue plate, sans fraisure, sans ombre. Elles flottent sur une plaque sans bord.

### 1.2 Corrections dans `Source/Kit/Kit.h` (gain sur les 24 plugins d'un coup)

| Fonction | Problème actuel | Correction précise |
|---|---|---|
| `brushedPlate` (l. 70) | Grain blanc, gris additif, pas de reflet, flou Retina | (1) Stries : par ligne, bruit 1D le long de x, filtré passe-bas sur 2 ou 3 octaves (longueur de corrélation 30–400 px), plus une variation lente d'une ligne à l'autre. (2) Effet **multiplicatif seulement** (supprimer `+ n/255`), amplitude ±3 à 4 %. (3) Reflet anisotrope : bande douce horizontale (gaussienne sur y, ~+6 % de luminance) et légère diagonale. (4) Graine par plugin (hash du nom). (5) Paramètre `scale` = facteur physique de l'écran (`Desktop::getDisplays()` ou `g.getInternalContext().getPhysicalPixelScaleFactor()`), puis dessin avec `drawImage` mis à l'échelle. (6) Finitions au choix : `Finish::{BrushedAlu, Anodized, Hammertone, Lacquer, Bakelite}`. Le Hammertone (martelé fin, sans stries) convient aux compresseurs à lampes, la laque aux pédales. |
| *nouveau* `chassis()` | Plaque sans bord | Chanfrein de 2 px (clair en haut à gauche, sombre en bas à droite), bord arrondi. Option « oreilles de rack 19" » avec lumières oblongues et vis (effets Orbe). Option « joues en bois » (instruments). Bandeau d'en-tête peint ou à plaque vissée. |
| `screw` (l. 91) | Fendue plate | Fraisure (anneau sombre, lèvre claire en bas), tête bombée (dégradé radial), choix fente, cruciforme ou six-pans, ombre portée de 1 px. Exclure le coin bas-droit, réservé à la poignée de redimensionnement, ou dessiner cette poignée en grille gravée hors de la vis. |
| `knob` (l. 132) | Modèle unique, anneau lumineux, ombre dure | Énumération `KnobStyle` : **Chicken** (bec bakélite, type LA-2A/Pultec), **Skirt** (jupe à cadran chiffré, type Neve/Moog), **Cap** (petit aluminium moleté à capuchon coloré, type API 550/1176), **Fluted** (noir cannelé type Davies 1900), **Stepped** (cranté avec valeurs sérigraphiées autour, type Maag/mastering). Échelle **sérigraphiée sur la plaque** (chiffres, dB, Hz) à la place de l'anneau lumineux ; garder l'anneau seulement en option « moderne », fin et discret. Ombre floue (`juce::DropShadow`, rayon ≈ 0,35r, décalage (0 ; 0,25r)). Moletage : stries claires et sombres modulées par le cosinus de l'angle à la source de lumière (fixe en haut à gauche), pas 48 traits noirs uniformes. Dessus métal : reflet conique, simulé par 36 secteurs alternés. Index **blanc ou crème peint**, pas coloré. |
| `led` (l. 103) | Éteint = point noir | Lentille **teintée même éteinte** (rouge ou vert sombre visible), bague (chrome ou plastique noir), cœur presque blanc quand allumé + halo coloré. Deux tailles normalisées (3 mm et 5 mm). |
| *nouveau* `vuMeter()` | VU codé dans OneKnob seulement, échelle linéaire en dB | Passer le VU de `OneKnobEditor::paintVu` dans le Kit. **Échelle VU réelle** : position proportionnelle à la tension, avec −20 vers 0 % et 0 VU vers 71 %. **Balistique VU** : 300 ms, 2ᵉ ordre légèrement amorti (≈ 1,5 % de dépassement), alimentée en **RMS** (aujourd'hui en crête, 0 VU = −18 dBFS crête, d'où l'aiguille collée à droite, l. 92). Rétroéclairage ambre incandescent (dégradé du bas), aiguille noire avec contrepoids et capuchon de pivot, vitre bombée, cadre vissé, LED de crête dans le coin, mini sérigraphie « dreamaws ». |
| `screenWell` (l. 118) | Un seul type d'écran | Types `Display::{LCD (ambre/vert, segments fantômes), VFD (cyan-vert, grille de points, lueur), Seg7 (rouge), OLED}`, chacun avec sa police **embarquée** (BinaryData, licences OFL : DSEG7 pour les 7 segments, VT323 ou Share Tech Mono pour la matrice de points). |
| `fnt` / `family` (l. 26–40) | Police système variable | Embarquer **une** sans-serif industrielle (OFL : Inter Tight, Barlow Condensed ou IBM Plex Sans Condensed) pour la sérigraphie, et la police d'écran. Le `mono()` système ne sert plus qu'au débogage. |
| `drawButtonBackground` (l. 237) | Rectangle dégradé « web » | `PushLit` (capuchon carré en plastique avec LED intégrée, type touches 808 ou pads MPC), `Latch` (bouton poussoir à bague), `Toggle` (**interrupteur à levier** métal, deux positions, gravure ON/OFF), `Rocker`. |
| `drawComboBox` (l. 264) | Menu déroulant de site web | Sélecteur « afficheur + flèches gravées » ou commutateur rotatif cranté ; texte dans la police d'écran ; triangle gravé blanc et non coloré. |
| `drawLinearSlider` (l. 200) | Rail fin, capuchon gris | Fente découpée dans la plaque, capuchon de fader de console (plastique noir, rainures, ligne blanche), échelle en dB sérigraphiée. |
| `tracked` (l. 43) | Une `GlyphArrangement` par caractère à chaque peinture | Mettre en cache la largeur par (texte, police) : c'est appelé des centaines de fois par repeinte (CPU de l'interface). |
| Palette | `accent` sert à tout | Ajouter `silk` (sérigraphie), `ledRed`/`ledGreen`/`ledAmber` normalisés et `meterBack`. L'accent ne sert plus qu'à un **élément héros** par plugin. |

Au passage, fusionner les **quatre mini-kits** qui se dupliquent : `of::` (`OrbeDsp/OrbeFace.h`), `suiteui::` (`Orbe808/SuiteUi.h`), `ou::` (`OrbeAnalysis/OrbeUi.h`) et `oc::ui` (`OrbeChain/ChainUi.h`). On y trouve 3 classes `Seg`, 2 `Pot`, 2 `Power` et `kLilac`/`kIce`/`kPeach` copiés 4 fois. Tant qu'ils restent séparés, toute refonte visuelle est à faire 4 fois.

### 1.3 Direction visuelle par famille

| Famille | Parti pris | Matériaux et éléments |
|---|---|---|
| **Un potard** (Grain Nord, Gorge, Poupée, Patine) | « Boîtes boutique » : chaque plugin est **un objet différent**, tous dans le même format portrait | Une finition par plugin (voir § 3.2), un gros bouton héros, un VU ou un afficheur, un interrupteur à levier IN, une plaque série rivetée. |
| **Effets Orbe** (Cabine, Master, EQ, DeEsser, Denoiser, Autotune, Looks, Atelier) et Voile | « Rack studio graphite » : façades 19" en aluminium anodisé noir, oreilles et vis de rack, sérigraphie blanche, **une bande d'accent** par appareil (façon UAD/Softube) | Écrans d'analyse derrière une vitre biseautée. Potards choisis selon le métier : Cabine = modules 500 (capuchons API) ; Master = crantés de mastering ; EQ = cadre écran + 8 petits capuchons de couleur ; Autotune = rack d'harmoniseur à afficheur rouge. |
| **Instruments Orbe** (Synth, 808, Beat, Pads, Sample, générateur Orbe) | « Instruments de scène » : panneau noir mat, **joues en bois** (synthé, générateur), touches et pads en plastique et caoutchouc | Synth : joues en noyer, sections soulignées de couleur façon Juno, potards à jupe façon Moog. 808 et Beat : touches de pas carrées rétroéclairées de 4 couleurs (rouge, orange, jaune, crème), sans copier de marque. Pads : pads caoutchouc 4×4 qui s'éclairent selon la vélocité. Sample : châssis de sampler à LCD vert (esprit SP). |
| **Outils de mesure** (Détect, Mix, Fredonne) | « Appareils de mesure » au lieu de cartes web | Bezels d'oscilloscope, phosphore vert ou LCD ambre, crêtes-mètres façon Dorrough, cadrans à aiguille pour le score et la corrélation. Supprimer les cartes arrondies et les pastilles. |

**Cohérence et différenciation.**
- *Aujourd'hui* : très cohérent mais **trop homogène**. Les 16 Orbe partagent anthracite, pastels et le même potard ; seuls Voile (orange) et les un-potard (plaques teintées) se distinguent.
- **En-têtes incohérents** : sphère + ORBE + sous-titre lilas (Détect, Mix, Fredonne) ; sphère + ORBE SYNTH ; DREAMAWS + ORBE CABINE + filet dégradé ; ORBE PADS sans sphère ; ORBE LOOKS bicolore sans dreamaws.
- **Interrupteur IN** placé différemment presque partout.
- *Cible* : un **châssis commun** (bandeau d'en-tête à la même hauteur, marque dreamaws et plaque série en bas à droite, IN toujours en haut à droite, même typographie), une **matière par famille** et **un élément héros par plugin** (VU, écran, grille, pads).

### 1.4 Plugin par plugin (capture, ce qui marche, ce qui fait maison, cible)

| Plugin | Ce qui fait produit | Ce qui fait maison | Cible concrète |
|---|---|---|---|
| **Voile** (`audit/voile.png`) | Hiérarchie claire : 4 macros, preset, tonalité | Flèches ASCII `< >`, coin bas-droit vide, MIX et SORTIE serrés, bandeau CORRECTION de type web, gris neutre sans matière, texte d'aide | Rack 2U « processeur vocal » : 4 gros boutons à jupe chiffrée, preset sur VFD, correction sur afficheur à aiguille centre-zéro (±1 dt), sortie sur un vrai VU |
| **Orbe** (`audit/orbe.png`) | Cartes d'accords lisibles (degré, nom, notes), grille de lecture | Segments PLAQ./MONT./A/R/ALÉA collés aux bords ; HARMONIE serré sous MESURES ; batterie en carrés gris illisibles ; minuscules pastilles ACC/LEAD/BAS/BAT ; les faders de mix n'occupent qu'un quart de leur zone | Arrangeur à joues bois, cartes d'accords en « touches » éclairées, piano roll dans un écran OLED, faders longs à capuchon de console, rangée de pads de piste |
| **Orbe FX** | Même face qu'Orbe | Rien n'indique la variante effet | Plaque série « FX » et témoin « audio IN » |
| **Grain Nord, Gorge, Poupée, Patine** (`k0.png`, `onek_all.png`) | Format portrait, VU crème, plaque teintée, gros bouton : **la meilleure base** de la suite | Bouton identique aux autres plugins avec anneau néon, VU sans éclairage ni cadre, afficheur TON sous forme de boîte web, listes Auto/Auto de type site, IN rond générique | Voir § 3.2 : une face matérielle par plugin |
| **Synth** (`audit/synth.png`) | Sections numérotées, écrans filtre et ADSR, clavier propre | Repère « 1,4 kHz » posé à côté du trait de **balayage d'enveloppe** (~10 k) et non de la coupure, ce qui induit en erreur ; formes d'onde en boutons web ; molettes de pitch et modulation crédibles | Joues bois, sélecteur de forme rotatif cranté, écran filtre en VFD, sections soulignées de couleur |
| **808** (`/tmp/o808.png`) | Forme d'onde, clavier, haut lisible | 7 presets en boutons texte, sélecteurs en boutons web, aucune matière | Boîte à rythmes « basse » : touches de pas éclairées, gros DECAY et DRIVE à bec, écran de forme d'onde à phosphore |
| **Beat** (`/tmp/beat.png`) | Grille couleur par piste : convaincant | Pistes nommées « CHARLEY » (charleston), M/S en boutons gris web, beaucoup de petites zones de texte, explication MIDI imprimée en bas | Touches de pas en caoutchouc rétroéclairées, mini-faders de voie, écran LCD pour le pattern et le tempo |
| **Pads** (`/tmp/pads1.png`) | Forme d'onde avec repères, pads avec vignette d'onde | Étiquettes de repères **qui se chevauchent** (11, 12, 13) ; pads en cartes grises au lieu de caoutchouc | Pads caoutchouc gris avec lueur selon la vélocité, LCD vert pour la découpe |
| **Sample** (`/tmp/smp1.png`) | Source et résultat superposés, presets parlants | Les 9 presets sont des boutons de site ; « 77,4 bpm » coupé par un trait de découpe (`7|7,4`) ; texte « Découpes sur C1–G1 » imprimé | Sampler à LCD, presets en touches à LED, molette Data |
| **Cabine** (`cab.png`) | Concept « modèle 500 » ; VU, LED de réduction et de crête | Mur de 15 potards identiques ; le VU pêche détonne ; les sections ne sont que des titres | Trois **vrais modules 500** côte à côte (EQ façon API 550, compresseur FET façon 1176 avec aiguille de réduction, module Espace), chacun avec son châssis et ses vis |
| **Master** (`mas.png`) | Le plus « pro » : courbe, barres-graphes, LUFS | Étiquettes « SORTIE LUFS » collées au chiffre ; 16 potards identiques ; dix boutons ronds ON identiques | Rack de mastering 2U : potards crantés à valeurs sérigraphiées, LUFS sur afficheur à segments, réduction du limiteur sur aiguille |
| **EQ** (`eq.png`) | Crédible (proche de Pro-Q), A/B, info-bulles | Bande inactive 8 peu lisible, rangée de 8 pastilles de bandes un peu « web », barre d'astuces | Garder l'écran et l'encadrer d'un bezel ; potards à capuchon de couleur par bande sous l'écran |
| **DeEsser** (`deesser.png`) | Zone de bande et réduction claires | Le spectre de sortie **masque** l'entrée (couleurs proches) ; l'écran de réduction est presque vide (une ligne) ; la ligne de latence colle au bord bas | Réduction sur VU ou barre-graphe vertical ; entrée en gris sombre rempli, sortie en trait |
| **Denoiser** (`denoiser.png`) | Profil de bruit lisible | Trois gros boutons rectangulaires web à gauche ; texte de latence imprimé | Boutons LEARN, ADAPT et LISTEN à LED intégrée ; écran à phosphore |
| **Autotune** (`at/at1.png`) | Écran de hauteur informatif | Clavier masque en **touches lilas** (état actif et inactif peu lisible) ; numéro de série en double ; légende imprimée ; 12 potards identiques | Rack d'harmoniseur : grosse note sur afficheur rouge à 7 segments, aiguille de correction ±50 ct, clavier masque à touches éclairées |
| **Looks** (`oc/looks0.png`) | Grand écran de look, macros claires | Les **8 LED de modules toujours allumées** (aucune information) ; vide entre MACROS et TONALITÉ ; MIX et SORTIE en pêche hors palette | Les LED reflètent les modules réellement actifs ou leur intensité ; écran VFD ; macros à jupe |
| **Atelier** (`oc/at4.png`) | Console complète lisible en entier | **Texte de 7–8 px à la taille de base 1120×524** (illisible sur un MacBook) ; 40 potards identiques ; flux de chaîne en bas minuscule | Taille de base ≥ 1400×700 ou onglets par module ; tranche de console verticale ; potards plus petits mais de styles distincts par section |
| **Détect** (`an_detect.png`) | Roue de Camelot réussie, informations utiles | Tableau de bord web (cartes, pastilles vertes) ; texte coupé (« Fondamentale : note MIDI 24 + … ») | Accordeur ou appareil de mesure : grand afficheur à segments pour le BPM, roue sérigraphiée sur verre |
| **Mix** (`an_mix.png`) | Informations pertinentes (LUFS, PLR, équilibre tonal, conseils) | Le plus « web » : cartes, jauge de score façon appli, paragraphes | Baie de mesure : 6 compteurs en afficheurs à segments, score sur cadran à aiguille, conseils en LCD défilant |
| **Fredonne** (`an_fredonne.png`) | Piano roll lisible | Gros bouton ARMER gris web, consignes imprimées | Enregistreur de poche : bouton REC rouge éclairé, vumètre LED, afficheur de note |

---

## 2. Synthèse : notes sur 10 et priorités

Les notes de fiabilité tiennent compte des bugs trouvés au § 4.1.

| Plugin | Design | Fonctions | Fiabilité | Commentaire court |
|---|---|---|---|---|
| Voile | 5 | 5 | 6 | Granulaire 2 têtes audible, chaîne simple |
| Orbe | 6 | 6 | 7 | Bon moteur, aucune édition fine |
| Orbe FX | 5 | 5 | 7 | Mêmes limites ; l'aperçu se mélange à l'audio entrant |
| Grain Nord, Gorge, Poupée, Patine | 6 | 5 | **4** | Interrupteur IN inopérant (§ 4.1-1), dépend du niveau d'entrée |
| Synth | 7 | 6 | 7 | Pas de LFO ni de matrice, saturation sans suréchantillonnage |
| 808 | 7 | 5 | 7 | Pas de générateur de ligne 808 (présent dans le prototype) |
| Beat | 7 | 6 | 7 | Sons synthétisés uniquement, pas d'arrangement |
| Pads | 7 | 7 | 7 | Solide ; chargement de fichier dans le thread de l'interface |
| Sample | 7 | 7 | 7 | Solide ; défauts d'affichage mineurs |
| Cabine | 6 | 7 | **5** | Interrupteur IN inopérant |
| Master | 7 | 7 | **5** | Interrupteur IN inopérant |
| EQ | 7 | 7 | 5 | IN inopérant, coefficients changés sans lissage, compression des aigus près de Nyquist |
| DeEsser | 6 | 7 | 5 | IN inopérant |
| Denoiser | 6 | 7 | 5 | IN inopérant ; 2048 échantillons de latence (normal pour ce type) |
| Autotune | 6 | 6 | **4** | IN inopérant ; 37 ms de latence, inutilisable pour s'écouter en direct |
| Looks | 6 | 6 | 5 | IN inopérant ; LED décoratives |
| Atelier | 4 | 7 | 5 | Illisible à la taille de base ; IN inopérant |
| Détect | 6 | 7 | 7 | Utile ; interface web |
| Mix | 6 | 7 | 8 | Utile ; interface web |
| Fredonne | 6 | 6 | 7 | Pas d'édition des notes |

**Priorités** :
1. Bypass non honoré (P0 : touche 14 plugins).
2. Refonte du Kit visuel : plaque, Retina, potards, police embarquée, VU.
3. Édition dans Orbe (piano roll, choix d'accord, annuler).
4. Faces différenciées par famille.
5. Plugins à un potard en version « complète ».
6. Qualité de hauteur (Autotune, Voile, plugins un potard).
7. Presets utilisateur et annuler/rétablir dans toute la suite.

---

## 3. Un potard : refonte et extensions

### 3.1 Constats DSP (`Source/OneKnob/OneKnob.h`)
- **Compresseur et de-esser à seuils absolus** (`comp.thr = -14 - 14·comp`, de-esser `thr = 0.05`) : le même réglage sonne très différemment selon que la voix arrive à −24 ou à −6 dBFS. Il faut un étage d'**entrée automatique ou d'ajustement** (normaliser le RMS vers −18 dBFS avant la chaîne) ou des seuils relatifs au niveau mesuré.
- **Compensation de volume automatique** (`ag`, l. ~205) : RMS sur 0,5 s, coefficient 4e-4 par échantillon. Elle peut « pomper » sur les attaques et les silences ; elle doit pouvoir se couper (mode avancé).
- Saturation, bitcrush et lo-fi **sans suréchantillonnage** : repliement de spectre audible sur une voix poussée à 10.
- Hauteur : décaleur granulaire à deux têtes (`Engine.h:73`) avec une fenêtre de 32 ms, et doublage à +12 dt **sans formants**. Le son « chipmunk » de Poupée vient de là.
- Filtres recalculés sans lissage quand on automatise CARACTÈRE (petits clics possibles).
- Le VU reçoit la **crête de sortie** et non le RMS (§ 1.2).

### 3.2 Parti pris visuel par plugin

| Plugin | Machine d'inspiration (sans marque ni logo) | Face |
|---|---|---|
| **Grain Nord** | Harmoniseur numérique de la fin des années 70 | Aluminium anodisé bleu nuit, gros bouton cannelé alu à jupe, **afficheur rouge à 7 segments** pour la note et la tonalité (remplace la boîte TON), barre-graphe LED vertical à la place du VU, interrupteur à levier IN. |
| **Gorge** | Compresseur à lampes / préampli de console | Peinture **martelée rouille-bordeaux**, bouton **à bec** en bakélite noire, grand VU à aiguille rétroéclairé ambre, voyant « tube » orange, plaque série rivetée. |
| **Poupée** | Pédale d'effet boutique | Boîtier de pédale en **laque prune métallisée**, gros bouton crème, **footswitch** chromé avec LED rouge de 5 mm, sérigraphie colorée façon années 90, petite fenêtre VU ronde. |
| **Patine** | Préampli ou tranche à bande broadcast | **Vert sauge et laiton gravé**, bouton à jupe marron cannelé, VU crème large, interrupteur à levier, plaque laiton avec numéro gravé. |

### 3.3 Mode « complet » (sans perdre la simplicité)
- **Volet dépliable** sous le bouton principal, fermé par défaut, état mémorisé avec le projet :
  - ENTRÉE (trim ±12 dB + voyant de niveau idéal) ;
  - TON (inclinaison grave/aigu ±) ;
  - MIX (sec/traité) ;
  - SORTIE (±12 dB) ;
  - un **réglage spécifique** par modèle : Grain Nord = VITESSE de correction ; Gorge = SLAP (temps du slapback) ; Poupée = OCTAVE (+12 / +7 / off) ; Patine = AIR.
- Bouton **AUTO-GAIN** on/off.
- **A/B**, **annuler/rétablir** (`UndoManager` dans l'APVTS, aujourd'hui `nullptr` partout), **presets utilisateur** (dossier `~/Music/dreamaws/Presets/<plugin>`), 3 ou 4 presets d'usine par modèle (« couplet », « refrain », « ad-libs »).

### 3.4 Nouveaux plugins « un potard » (même format)

| Nom provisoire | Usage | DSP envisagé |
|---|---|---|
| **Souffle** (voix) | Air et brillance sans siffler | Shelf aigu dynamique + excitateur (harmoniques paires sur > 6 kHz) + de-esser couplé au réglage |
| **Velours** (voix) | Voix douce et proche (R&B) | Compression optique (attaque lente, relâche dépendant du programme), saturation de bande légère, coupe de 3 kHz dynamique |
| **Ad-lib** (voix) | Effets d'ad-libs en un geste | Passe-bande téléphone → delay ping-pong synchronisé → reverb sombre, avec ducking sur la voix principale |
| **Plomb** (808 et basse) | 808 qui passe sur téléphone | Saturation suréchantillonnée ×4 + harmoniques entre 100 et 300 Hz, limiteur doux, mono sous 120 Hz |
| **Claque** (batterie) | Rendre le bus de batterie percutant | Shaper de transitoires + compression parallèle façon « New York », clip doux |
| **Bande** (instruments et mix) | Coloration de bande | Hystérésis simplifiée, pleurage et scintillement, roll-off des aigus, bruit |
| **Large** (synthés et pads) | Largeur stéréo | Haas + M/S + chorus doux, mono garanti sous 150 Hz, contrôle de corrélation |
| **Colle** (bus et master) | Compression de bus | VCA de bus (attaque 10–30 ms, ratio 2–4, relâche auto), sidechain HPF 90 Hz |

---

## 4. Audit fonctionnel

### 4.1 Bugs transverses (à corriger d'abord)

1. **P0 — Bypass non honoré dans 14 plugins.** `getBypassParameter()` est surchargé, mais **aucun `processBlock` ne lit le paramètre `bypass`**. C'est le cas de OneKnob (`OneKnobProcessor.cpp:52`), Cabine, Master, Autotune, Looks, Atelier, EQ, DeEsser et Denoiser (via `FxProcessor`), Mix et Détect. Or JUCE n'appelle `processBlockBypassed` **que si aucun paramètre de bypass n'est déclaré** (`juce_AudioProcessor.h:201` et wrapper VST3 l. 3719). Conséquence : en VST3 et AU, l'interrupteur IN et le bypass de FL ne coupent rien. Aucun test ne vérifie le bypass : personne ne s'en est aperçu.
   *Correction* : en tête de chaque `processBlock`, `if (bypass && bypass->get()) { processBlockBypassed(...); return; }` (ligne retard compensé déjà prête), idéalement avec un fondu croisé de 10 ms. Factoriser dans `FxProcessor` et dans `oc::Host`. Voile n'est pas concerné (il n'a pas de paramètre bypass).
2. **Le fond de plaque est flou sur Retina** dans tous les plugins (§ 1.2).
3. **Ni annuler/rétablir ni presets utilisateur** dans la suite (`UndoManager` à `nullptr` ; aucun enregistrement de preset utilisateur). A/B seulement dans l'EQ. La taille de fenêtre n'est mémorisée que dans Master et DeEsser.
4. **Allocations dans le thread audio** : Orbe crée `"vol" + String(i)` 8 fois par bloc (`OrbeProcessor.cpp:275`). Le `std::atomic_load` sur `shared_ptr` n'est pas lock-free avec libstdc++ ou libc++ (verrou interne). Pré-résoudre les pointeurs `std::atomic<float>*` dans le constructeur.
5. **Saturations sans suréchantillonnage** (moteur Voile, OneKnob, chaîne Looks et Atelier, 808, Synth) : repliement de spectre. Seuls Master et Cabine suréchantillonnent.
6. **Numéro de série en double** DW-701 (Autotune et Denoiser).
7. **Interface Atelier trop petite** pour être lisible (`AtelierEditor.cpp:8`, `kW = 1120`).

### 4.2 Générateur d'accords Orbe (`Source/Orbe/OrbeCore.h`, `OrbeStyles.h`, éditeur)

**Ce qui est bon** :
- Graine reproductible.
- Voice leading au plus court avec deuxième passe pour boucler (`voice`, l. 117).
- Nommage correct des accords et des chiffres romains (y compris ø, °, emprunts).
- Mélodie en phrases AA'AB' recalées sur les accords (`generateLead`), résolution finale vers la tonique.
- 28 styles avec banques, rythmes, basse et batterie propres à chacun.
- Aperçu synchronisé sur le transport de l'hôte (ppq), swing et humanisation, MIDI multicanal en sortie, glisser-déposer du MIDI.

**Limites musicales** :

| Domaine | Constat | Amélioration |
|---|---|---|
| Progressions | 3 à 7 grilles de 4 accords par style, **un accord par mesure** toujours ; la relance d'un seul accord tire un degré **uniforme** parmi 7 (y compris ii° en mineur, `rerollChord`) | Rythme harmonique variable (2 accords par mesure, accord tenu sur 2 mesures), pondération des degrés par style, cadences et tensions (sus2/sus4, slash chords, accord de Neapolitan, i–♭VI–♭VII en trap) |
| Voicings | Toujours en position serrée entre les notes MIDI 50 et 62 (triade à l'étroit) | Modes Serré / Ouvert (drop 2) / Large pad, doublure d'octave, basse séparée, choix du renversement par accord, registre réglable |
| Basse | Fondamentale seule, motif fixe par style, aucune glissade 808 | Générateur de **ligne 808** (glissades vers la note suivante, sauts d'octave, notes de passage, accroche au kick), comme le prototype 808.html (« Ligne trap », « Ligne drill (glisses) ») |
| Batterie | Même motif à chaque mesure ; roulements de charleston toujours aux mêmes temps ; pas de fill ni de variation sur 8 mesures | Variation par mesure (A, A', A, fill), roulements de charleston générés (1/3, 1/4, 1/6, 1/8 avec rampe de vélocité), ghost notes, crash au début de la phrase, contretemps du kick par style |
| Mélodie | Tessiture C5 et plus (aiguë), rythmes probabilistes simples, pas de motif rythmique explicite | Tessiture réglable, motif rythmique repris (mêmes rythmes, autres notes), « réponse » sur les mesures 2 et 4, mode **topline** (rythmes de rap, notes répétées) |

**Édition manquante** (le prototype `orbe.html` avait : piano roll, ←/→ progression, export WAV boucle + pistes, Copier) :
1. **Piano roll éditable** (déplacer, allonger, supprimer, vélocité), avec **verrouillage par piste** (accords, lead, basse, batterie).
2. **Choisir l'accord** au clic sur une carte : menu des 7 degrés + emprunts + qualité 7e ou 9e, et ← / → pour faire défiler.
3. **Annuler / rétablir** pour Générer et Relancer (aujourd'hui destructif).
4. **Glisser par piste** : les pastilles ACC/LEAD/BAS/BAT existent mais sont minuscules. Les rendre grandes, ajouter « glisser la piste en WAV » (rendu hors-ligne de l'aperçu).
5. **Variations** : « garder le rythme, changer les notes », « mêmes accords, autre renversement », « +1 mesure ».
6. **Transposition globale** sans régénération, **suivi de la tonalité de l'hôte** et lien avec **Orbe Détect**.
7. **Sortie MIDI** : dans FL Studio, il faut régler un port MIDI de sortie dans le wrapper. Écrire ce mode d'emploi dans l'info-bulle et l'aide.
8. **Favoris** : enregistrer une graine et ses réglages comme « idée » (8 emplacements).

### 4.3 Autres plugins (fonctions et limites)

| Plugin | Bugs probables / limites | Manque par rapport au prototype ou au marché |
|---|---|---|
| Voile | Décaleur 2 têtes (fenêtre 32 ms) avec artefacts de flanger ; reverb FDN à 4 lignes sans modulation (métallique) ; delay toujours à la croche ; aucun formant | Presets utilisateur, delay synchronisé au choix, formants |
| Orbe FX | L'aperçu s'ajoute à l'audio entrant (comportement surprenant) | Mode « MIDI seulement » |
| Synth | Pas de LFO ni de matrice de modulation, filtre 12 dB seulement, saturation sans suréchantillonnage, le repère de fréquence de coupure est trompeur | LFO, 24 dB/oct, glide et mode mono, arpégiateur |
| 808 | Pas de séquence interne | Générateur de ligne 808 du prototype, « Surprends-moi », presets sur 1 ou 2 mesures |
| Beat | Batterie entièrement synthétisée (manque d'impact face à des kits samplés) ; nom « Charley » | Import d'un son par piste, sections et arrangement, export en stems (tout présent dans `beat.html`) |
| Pads | `loadFile` est synchrone dans le thread de l'interface (gros fichiers = gel) | Choke par groupe déjà présent ; manquent l'enveloppe de filtre et le time-stretch par pad |
| Sample | Correct | Export audio (glisser en WAV) |
| Cabine | Interrupteur IN inopérant | Prototype : enregistrement et prises, hors du rôle d'un plugin, c'est normal |
| Master | Interrupteur IN inopérant | LUFS intégré et court terme, cible de loudness, comparaison à un morceau de référence |
| EQ | `set()` change tous les coefficients d'un coup (zipper quand on glisse ou automatise) ; conception RBJ = compression des aigus près de Nyquist à 44,1 kHz (bande AIR à 18 kHz) | EQ dynamique sur 1 ou 2 bandes, M/S, analyseur de sortie |
| DeEsser | Correct (anticipation de 1 ms) | Sidechain externe |
| Denoiser | 2048 échantillons de latence, ce qui est normal pour un traitement STFT | Réglage de l'attaque ou du relâchement de la réduction |
| Autotune | **37 ms de latence** (1791 échantillons) : inutilisable pour s'écouter en enregistrant ; décaleur 2 têtes + formants cepstraux | Mode basse latence (≤ 5 ms, ensemble PSOLA plus court), export des notes en MIDI et A/B (présents dans `autotune.html`) |
| Looks, Atelier | Chaîne pertinente (`docs/looks-voix.md`) ; mêmes limites de hauteur et de saturation | Modules réorganisables (Atelier), LED d'activité réelles (Looks) |
| Détect | Utile | Envoyer la tonalité et le tempo aux autres plugins Orbe |
| Mix | Utile, robuste | Prototype : auto-master, rapport à copier, volume dans le temps |
| Fredonne | Pas d'édition des notes détectées | Correction des notes au clic, quantification humanisée |

**Ce qui est solide** :
- Latence déclarée et ligne de bypass compensée prête (`BypassDelay`).
- Traitement par tranches de `maxBlock` (FL varie la taille des blocs).
- Entrée mono et stéréo acceptée partout.
- Publication sans verrou des échantillons (`osamp::Swap`, anciens objets gardés 30 s).
- Threads d'analyse séparés.
- `ScopedNoDenormals`.
- Préparation complète au changement de fréquence d'échantillonnage.

---

## 5. Plan d'action priorisé

| # | Chantier | Fichiers | Critères d'acceptation vérifiables | Effort | Lot |
|---|---|---|---|---|---|
| 1 | **Bypass honoré** + fondu de 10 ms | `OrbeDsp/OrbeFace.h`, `OrbeChain/ChainUi.h`, `OneKnob/OneKnobProcessor.cpp`, `*Processor.cpp` de Cabine, Master, Autotune, Mix, Détect | Test : bypass=1 → sortie égale à l'entrée retardée de la latence (erreur < −120 dB) dans **processBlock** ; pas de clic (saut < −60 dBFS) au basculement | S | A |
| 2 | Kit : plaque v2 (stries, multiplicatif, reflet, graine, **Retina**) + `chassis()` + vis à fraisure + coin de redimensionnement | `Kit/Kit.h` | Capture ×2 nette ; vis en bas à droite non recouverte ; écart-type de luminance de la plaque entre 2 et 5 % | M | B |
| 3 | Kit : `KnobStyle` (5 styles), ombre floue, index peint, échelles sérigraphiées | `Kit/Kit.h` | Planche de captures des 5 styles ; aucun plugin ne change de géométrie (mêmes bornes) | M | B |
| 4 | Kit : polices embarquées (sans-serif + 7 segments + matrice de points) et types d'écran | `Kit/Kit.h`, `CMakeLists.txt` (BinaryData), `assets/fonts/` | Captures identiques sous Linux et Mac ; plus aucune police système `mono()` dans les faces | M | B |
| 5 | Kit : `vuMeter` (échelle réelle, RMS, balistique 300 ms, éclairage) ; LED v2 ; levier et poussoir | `Kit/Kit.h`, `OneKnob/OneKnobEditor.cpp` | Sinus à −18 dBFS RMS → 0 VU ±0,2 ; temps de montée à 99 % = 300 ms ±10 % (test de la balistique) | M | B |
| 6 | Fusionner les mini-kits (`of`, `suiteui`, `ou`, `oc::ui`) dans le Kit | les 4 en-têtes + leurs éditeurs | Une seule définition de `Seg`, `Pot`, `Power` et des couleurs ; toutes les captures identiques avant et après | M | C (après B) |
| 7 | Châssis commun d'en-tête (marque, série unique, IN en haut à droite) + aide en surimpression à la place du texte imprimé | tous les `*Editor.cpp` (Suite) | Numéros DW uniques ; aucune phrase d'aide sur la face ; touche « ? » qui affiche l'aide | M | D (après C) |
| 8 | Plugins un potard : 4 faces matérielles (§ 3.2) + volet avancé (entrée, ton, mix, sortie, réglage spécifique, auto-gain) | `OneKnob/*` | Volet fermé = aspect actuel ; paramètres automatisables ; ±12 dB d'entrée → même réduction de gain mesurée à ±1 dB près grâce à l'auto-entrée | L | E |
| 9 | Annuler/rétablir + presets utilisateur + A/B + taille mémorisée (socle commun) | nouveau `Source/Kit/Presets.h`, processeurs | Annuler restaure l'état précédent ; un preset enregistré se recharge à l'identique dans un nouveau projet ; taille rouverte à l'identique | M | F |
| 10 | Orbe : piano roll éditable + choix d'accord par carte + annuler + verrous par piste | `Orbe/OrbeEditor.*`, `Orbe/OrbeProcessor.*`, `OrbeCore.h` | Notes modifiées sauvegardées dans l'état ; le MIDI glissé reflète les modifications ; annuler sur 20 niveaux | L | G |
| 11 | Orbe : moteur musical v2 (rythme harmonique, voicings, ligne 808 à glissades, variations de batterie, fills) | `Orbe/OrbeCore.h`, `tools/gen_styles.js`, `OrbeStyles.h` | Tests : glissades 808 présentes en trap et drill ; 8 mesures = au moins 2 variantes de batterie ; aucune note de basse hors de l'accord sur un temps fort | L | G |
| 12 | Suréchantillonnage ×2/×4 des saturations (moteur Voile, OneKnob, chaîne Looks et Atelier, 808, Synth) | `Engine.h`, `OneKnob/OneKnob.h`, `OrbeChain/OrbeChain.h`, `Orbe808Engine.h`, `OrbeSynthEngine.h` | Sinus à 5 kHz poussé à fond : repliement < −70 dB (test FFT hors-ligne) ; latence mise à jour | M | H |
| 13 | Autotune basse latence (≤ 5 ms) + Voile et un potard sur le même décaleur amélioré | `OrbeAutotune/AutotuneDsp.h`, `Engine.h` | Latence annoncée ≤ 256 échantillons en mode direct ; THD+N d'une voix corrigée meilleure qu'aujourd'hui (test hors-ligne) | L | H |
| 14 | EQ : lissage des coefficients + conception corrigée près de Nyquist ; `rt` sans allocation dans Orbe | `OrbeEQ/EqDsp.h`, `OrbeDsp/OrbeDsp.h`, `Orbe/OrbeProcessor.cpp` | Balayage de 100 Hz à 10 kHz en 50 ms sans clic ; à 44,1 kHz, écart de courbe < 0,5 dB à 16 kHz contre la courbe analogique ; zéro allocation dans `processBlock` (test avec un allocateur instrumenté) | M | I |
| 15 | Refonte des faces par famille (effets en rack, instruments à joues bois, mesure en appareils) ; Atelier ≥ 1400 px | éditeurs de la Suite | Revue des captures par famille ; texte ≥ 9 px à la taille de base | L | J (après B, C, D) |

**Lots parallélisables sans conflit de fichiers**
- **A** (bypass) : processeurs seulement.
- **B** (Kit) : `Kit/Kit.h` seulement.
- **F** (presets et annuler) : nouveau fichier + processeurs. À faire **après A**, qui touche les mêmes fichiers.
- **G** (Orbe) : `Source/Orbe/*` seulement.
- **H** (DSP de hauteur et suréchantillonnage) : `Engine.h`, moteurs.
- **I** (EQ et temps réel d'Orbe) : `OrbeEQ`, `OrbeDsp.h`. Coordonner avec G sur `OrbeProcessor.cpp` (une seule ligne).
- **E** (un potard) : `OneKnob/*`. À faire après A et B.
- **C**, puis **D**, puis **J** : séquentiels, sur les éditeurs.

Ordre conseillé :
- semaine 1 : A, B, G, H, I en parallèle ;
- semaine 2 : C, E, F ;
- semaine 3 : D, J.
