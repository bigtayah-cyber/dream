# Audit 6 : mise à jour visuelle « moderne avec identité »

*10 nov. 2026. Point de vue : directeur artistique et designer produit audio. Aucun fichier source modifié, aucun commit.*

**Captures lues une par une (×2)**, dans `/tmp/claude-0/-home-claude/12ae9892-af71-5ca8-a97b-93f9fd958b0b/scratchpad/` :
- les 17 faces de `cdj/` (toujours à jour : `Kit.h` n'a pas bougé depuis le 8 nov. 18 h 18) ;
- `galet_a.png` et `at_new.png` ;
- `vis6/orbe.png` et `vis6/onek0–3.png`, régénérés pour cet audit (`onek_snapshot` reconstruit dans `/tmp/b_vis`).

Orbe FX partage la face d'Orbe : 25 plugins au total. Les maquettes sont dans `docs/audit2/visuel/`.

**Contrainte de l'artiste : évoluer sans dénaturer.**
- On garde : la disposition, l'emplacement des commandes, la hiérarchie, les couleurs de famille, et les vis, VU et écrans typés.
- On modernise : la finition, les contrastes, la typographie, le mouvement, les retours visuels, les thèmes et l'échelle.
- Chaque modification indique son **risque de dépaysement** : faible, moyen ou fort.

## 0. Verdict

Le kit v2 a réglé le « fait maison » de l'audit 1 : polices embarquées, plaque nette sur Retina, vis à fraisure, cinq potards, VU à balistique, châssis commun. La suite est **sérieuse et reconnaissable**. Il ne faut surtout pas casser cela.

Ce qui la date tient à la **finition**, pas à la disposition :
- cinq polices différentes, avec les valeurs en police mono ;
- la virgule décimale change d'un plugin à l'autre ;
- un puits sombre sous chaque valeur ;
- des capitales espacées partout ;
- les écrans se rafraîchissent entre 15 et 30 Hz, sans aucun lissage ;
- aucun retour visuel au survol, au focus, ni pour la modulation ;
- pas de thème clair, et une échelle limitée.

Tout cela se corrige **dans le kit** (`Kit.h`, `Widgets.h`) sans déplacer un seul bouton. Estimation : **70 à 80 % du gain pour un risque de dépaysement faible**.

**Recommandation : direction A « Évolution douce »**, avec son thème clair. B et C restent des pistes pour de futurs plugins (§ 6).

## 1. Repères 2025–2026

- **Simplicité et vitesse.** Les plugins nés dans le logiciel « s'éloignent consciemment » du skeuomorphisme. Le moteur principal : l'envie d'aller vite (MusicRadar, juillet 2025). Le matériel survit parce qu'il **distingue** des outils proches et signale la qualité (UX Collective). Pour dreamaws, c'est un argument pour garder vis et VU comme signature.
- **Objets logiciels originaux plutôt qu'émulations.** Formes simples, peu de couleurs, typographie forte (Baby Audio, Klevgrand, l'OP-1 cité comme référence « moderne », MusicRadar).
- **Visualisation au centre et palette sobre.** C'est le cas de FabFilter Pro-Q 4 (Sound On Sound, février 2025).
- **Échelle et taille.** Pro-Q 4 sépare l'échelle de l'interface et la taille de la fenêtre. Soundtoys est redimensionnable partout depuis la v5.5 (2025).
- **Accessibilité.** Native dans JUCE depuis la 6.1 ; atelier de design inclusif à l'ADC24 ; demande exprimée sur le forum Waves.

Aucune forme, aucun nom ni aucune couleur de marque n'est repris.

## 2. Critique transverse

**Hiérarchie.**
- Les faces d'analyse (EQ, Denoiser, DeEsser, Autotune) mettent déjà leur écran en héros : c'est le modèle.
- Ailleurs, la hiérarchie existe mais elle est **mal soulignée** :
  - Master : la courbe, 4 barres-graphes, les LUFS et le VU ont le même poids ;
  - Mix : 6 chiffres de même taille, colorés sans logique ;
  - Atelier : 40 potards de même poids.
- La solution n'est pas de redessiner. Il suffit de jouer sur l'intensité : l'élément principal à 100 %, le secondaire à 70 % de contraste, les unités en `inkFaint`.

**Densité.** Les puits sombres sous chaque valeur (Master, Galet, Cabine, Autotune) doublent le nombre de rectangles. On les remplace par un chiffre tabulaire posé sur la plaque, à la même place. Les capitales espacées servent aujourd'hui à tout : on les réserve aux titres.

**Contraste** (WCAG, mesuré sur les captures) :

| Élément | Rapport | Verdict |
|---|---|---|
| Sérigraphie sur plaque rack | 11,7 – 15,2 | Excellent |
| `mute` #9D9AA8 sur plaque rack | 5,0 – 6,6 | AA |
| `mute` sur plaque meter #3A3D42 (Mix, Détect, Fredonne) | **3,96** | Sous AA |
| Potard désactivé (Galet) | ≈ 3,0 | Limite : l'état n'est porté que par le gris |
| Aide imprimée d'Orbe (9 px) | ≈ 3,4 | Illisible |

**Typographie.**
- Cinq voix coexistent : Plex Sans Condensed, Plex Mono, Share Tech Mono, DSEG7 et la matrice 5×7. Autotune en montre quatre à la fois.
- La virgule décimale est incohérente : « 2.0 dB » sur Master, Galet, EQ et Voile, mais « 1,4 kHz » sur Synth, 808 et Mix. On compte environ 38 appels `fr()` et au moins 15 `String(v, n)` bruts.
- Le texte descend à 9–10 px logiques, et à 7–8 px sur Atelier.

**Cohérence.** Il existe trois familles de boutons (barre LED, segment, carte) et trois façons d'afficher un preset (VFD, menu, texte). On les unifie **en gardant la place et la forme générale** de chacun.

**Mouvement.**
- `startTimerHz(15/20/30)`, aucun lissage : les spectres grésillent, en particulier sur le DeEsser.
- Aucun retour au survol ni au focus.
- **Modulation invisible** : une automation FL fait bouger l'index, mais on ne distingue pas la valeur de base. Galet module sans le montrer sur ses potards.

## 3. Par famille : on garde / on modernise / on change seulement si indispensable

| Famille | On garde | On modernise | On change seulement si indispensable |
|---|---|---|---|
| **Effets rack** (Voile, Cabine, Master, Looks, Atelier) | Disposition, plaque graphite, potards à jupe de Voile et Looks, VU crème, VFD de Looks, couleur de nom par plugin | Valeurs tabulaires sans puits, arc de valeur fin, virgule française, VU et cadran de correction de Voile plus nets avec lecture chiffrée intégrée (maquette A) | Atelier : tranches repliables (texte à 7–8 px). Master : atténuer les barres-graphes de 30 % pour laisser la courbe dominer |
| **Analyse** (EQ, DeEsser, Denoiser, Autotune) | Écran central, poignées numérotées, note à 7 segments et cadran de correction de l'Autotune | Spectres lissés (150–300 ms) avec crête en trait fin, remplissage dégradé, sélection de bande plus franche, info-bulles en Plex Sans | DeEsser : historique de réduction défilant sur 3 s dans l'écran de droite, aujourd'hui presque vide |
| **Instruments** (Synth, 808, Beat, Pads, Sample, Orbe, Orbe FX) | Joues bois, sections numérotées et soulignées du Synth, grille couleur de Beat, pads à vignette, cartes d'accords d'Orbe, forme d'onde du 808 | Segments unifiés, presets en pilule à la même place, survol et état « joué » sur les pads, les pas et le clavier | Synth : clavier repliable (un tiers de la hauteur). Orbe : retirer la phrase d'aide imprimée |
| **Mesure** (Mix, Détect, Fredonne) | Disposition, roue de Camelot, grands chiffres, cadrans à aiguille | Couleur des chiffres **sémantique** (dans la cible / hors cible), conseils en Plex Sans et non en mono | Plaque martelée gris clair remplacée par un graphite légèrement martelé : c'est elle qui fait passer le texte sous AA |
| **Galet** | Courbe éditable en héros, écran Transfert | Arcs de modulation sur les potards modulés, interrupteurs ON plus visibles dans l'en-tête de section | — |
| **Un potard** (Grain Nord, Gorge, Poupée, Patine) | **Tout l'objet** : couleurs franches, potard héros propre à chaque boîte, VU, levier IN, plaque série | Valeur affichée pendant le geste (« 6,0 », fondu 150 ms), menus « Auto » en sélecteur cranté, sérigraphie plus nette | Titre de Poupée (doublé rose/cyan, seul effet « 2019 » de la suite) ; LCD vide « ... » de Poupée ; « OFF » orphelin de Grain Nord et Gorge ; bouton orange sans étiquette de Gorge |

## 4. Direction v2 : système de finition

### 4.1 Tokens (ajoutés à `kit::Palette`, rétrocompatibles)

| Rôle | Sombre | Clair | Usage |
|---|---|---|---|
| `bg` / `surface` | #18181B / #26262B (proche de l'actuel) | #E3DED4 / #F1EEE7 | Plaque |
| `well` | #0E0E11 | #1A1A1C | Écrans, **toujours sombres** (identité) |
| `ink` | #F2EFE8 (16 : 1) | #18181A (15 : 1) | Texte |
| `inkMute` | #A4A1AE (7,3 : 1) | #5F5C66 | Étiquettes |
| `inkFaint` | #8C8A96 (5,4 : 1) | #6B6873 (4,8 : 1) | Unités ; rien sous 4,5 : 1 |
| `accent` | **couleur de famille actuelle, inchangée** | même teinte, assombrie pour garder 4,5 : 1 | Nom, arc de valeur, sélection |
| `mod` | #7FD8FF | #2D6CDF | Modulation et automation, même couleur dans toute la suite |
| `ok` / `warn` / `hot` | #5BD69A / #FFC24A / #FF4D4D | versions plus foncées | Mesures uniquement |

### 4.2 Typographie

- **Une seule famille** : IBM Plex Sans Condensed (déjà embarquée, licence OFL), en Regular et SemiBold, plus le Bold à ajouter (environ 90 Ko).
- **Chiffres tabulaires** : JUCE n'expose pas `tnum`. Deux solutions : générer une fois une variante renommée avec fontTools (« Plex Sans Cond Tab », conforme à l'OFL), ou utiliser des cases de chasse fixe dans `tracked()`.
- **Échelle** : 11 / 12 / 14 / 18 / 24 / 32–64 px.
- **DSEG7, VFD et matrice 5×7** : conservés là où ils font l'identité (LUFS de Master, note d'Autotune, Looks, Cabine), et uniquement là.

### 4.3 Composants (même forme, finition neuve)

- **Potards** : on garde les cinq styles et leur place. On ajoute un arc de valeur fin juste au-delà de l'échelle, et un arc `mod` avec un point pour la valeur instantanée (maquette A, MORDANT). On ajoute aussi un survol (piste éclaircie) et un focus (anneau de 2 px). Le moletage est adouci : moins de traits, plus de netteté.
- **Segmented**, **PresetPill**, **Switch** uniques dans `Widgets.h`, chacun rendu à l'emplacement actuel.
- **Écrans** : grille à 6 %, trace de 2 px, remplissage dégradé, crête tenue.

### 4.4 Mouvement de base (sans nouvelles animations)
- Écrans et compteurs à 30–60 Hz via `juce::VBlankAttachment` (JUCE 8), arrêt quand la fenêtre est cachée ; repeinte limitée à la zone qui bouge.
- Lissage d'affichage : spectres 150–300 ms avec crête tenue, index de potard automatisé τ ≈ 60 ms, chiffres LUFS interpolés au lieu de sauter.
- Les VU gardent leur balistique `VuBallistics.h` : c'est déjà le bon modèle.
- Dépaysement : nul (ce qui bougeait bouge mieux, rien de nouveau ne bouge).

### 4.5 Thèmes, échelle, grille

- **Thèmes** sombre (par défaut), clair et « système », réglés une fois pour toute la suite. Les écrans et les VU crème ne changent pas.
- **Échelle 50–200 %** et menu « Taille ». Les limites sont aujourd'hui disparates : 0,8–1,7× dans `OrbeFace`, 300–800 px pour les un potard, 2240 px pour Atelier.
- **Grille de 8 px** pour les **nouveaux** plugins. Les faces existantes gardent leurs coordonnées.

## 5. Plan de migration par le kit

| # | Modification | Portée | Effort | Risque technique | **Dépaysement** |
|---|---|---|---|---|---|
| K1 | Tokens dans `Palette` (couleurs de famille inchangées) ; `meter()` sur un graphite martelé plus sombre | 25 | S | Faible | Faible (Meter : moyen) |
| K2 | `fmt::value()` centralisé : virgule, unités, espace fine | 25 | S | Une quinzaine d'éditeurs à reprendre | Faible |
| K3 | Valeurs en Plex Sans tabulaire, puits supprimés ou très atténués | 25 | S | Largeurs à vérifier | Faible |
| K10 | Plex Bold ; capitales espacées réservées à `section()` | 25 | S | +90 Ko par binaire | Faible |
| K9 | Survol, focus, accessibilité (`setTitle`/`setDescription`) | 25 | S | Aucun | Nul |
| K8 | `VBlankAttachment` (30–60 Hz, arrêt si fenêtre cachée) + `SmoothedDisplay` (spectres, index automatisés) | Écrans | M | CPU à mesurer | Faible |
| K4 | Arc de valeur fin + arc de modulation sur les 5 styles de potards existants | ~22 | M | À valider sur 3 plugins | Faible à moyen |
| K5 | `Segmented`, `PresetPill`, `Switch` uniques, posés aux emplacements actuels | ~18 | M | Clavier et souris à retester | Moyen |
| K7 | Échelle 50–200 % + menu « Taille » | 25 | M | Mises en page en `u`, déjà prêtes pour la plupart | Nul |
| K6 | Thème clair, sombre ou système (écrans et VU inchangés) | 25 | M | Couleurs en dur et mini-kits `of::`, `suiteui::`, `ou::`, `oc::ui` à fusionner d'abord | Nul (option) |

**Ordre conseillé.**
1. K1, K2, K3, K10 et K9 : environ une journée, gain sur toute la suite, aucun déplacement.
2. Puis K8, puis K4 et K5.
3. Enfin K7 et K6.
4. Par famille (effort M) : les points « si indispensable » du § 3.

**Garde-fou.** Captures des 25 faces avant et après chaque étape. Il faut une cible « snapshots » unique qui partage une seule bibliothèque JUCE : aujourd'hui, chaque outil recompile JUCE, soit 367 étapes en `-j1`.

## 6. Trois directions

**A — « Évolution douce » (recommandée)**
- Fichiers : `visuel/A-evolution-douce.png` et `A-evolution-douce-clair.png`.
- **Voile exactement à sa disposition actuelle** : 4 potards à jupe et échelle 0–10, MIX/SORTIE à capuchon pêche, cadran de correction et VU crème, tonalité à droite, plaque DW verticale, rails et vis.
- Ce qui change :
  - un arc de valeur orange fin, et un arc de modulation bleu sur MORDANT ;
  - les valeurs « hauteur · 6,2 » en Plex tabulaire, sans puits ;
  - la lecture « −0,09 dt » intégrée au cadran au lieu de flotter ;
  - un preset en pilule avec un point « modifié » ;
  - les sélecteurs en Plex Sans au lieu de la police mono.
- Le thème clair n'éclaircit que la plaque : les écrans et les VU restent identiques.
- *Dépaysement* : faible.

**B — « Graphite vivant »** (`visuel/B-graphite-vivant.*`)
- Même plugin réorganisé autour d'un écran de hauteur héros, avec des potards plats en disque.
- Plus actuel, mais il **déplace les commandes et supprime les VU**.
- *Dépaysement* : fort. À réserver à un **nouveau** plugin, ou à une future v3.

**C — « Halo nocturne »** (`visuel/C-halo-nocturne.*`)
- Héros plein cadre lumineux, sans plaque ni vis.
- Très fort en vidéo et sur Instagram, mais il perd l'identité matérielle.
- *Dépaysement* : très fort. Son traitement lumineux peut servir **uniquement à l'intérieur des écrans existants** (trace de hauteur d'Autotune, roue de Détect) sans changer la face.

**Recommandation : A, sans hésiter.** C'est la seule direction qui respecte la contrainte de l'artiste, et elle apporte déjà l'essentiel du « moderne » attendu en 2026 :
- netteté, contraste AA, une seule typographie, chiffres propres ;
- retours de survol et de modulation visibles ;
- mouvement fluide des écrans et compteurs existants ;
- deux thèmes et une échelle libre.

## Sources

- [MusicRadar, « The biggest driver right now is people wanting everything fast » (juillet 2025)](https://www.musicradar.com/music-tech/plugins/the-biggest-driver-right-now-is-people-wanting-everything-fast-predicting-the-future-evolution-of-plugin-design)
- [UX Collective, « Why is skeuomorphism not dead yet in music production UI? »](https://uxdesign.cc/why-is-skeuomorphism-hard-to-die-in-music-production-interface-design-a634efa3e089)
- [Sound On Sound, FabFilter Pro-Q 4 (février 2025)](https://www.soundonsound.com/reviews/fabfilter-pro-q-4?amp)
- [CDM, Soundtoys 5.5, fenêtres redimensionnables](https://cdm.link/soundtoys-5-5-update-free-window-resizing-everything-explained/)
- [KVR, JUCE 6.1 et l'accessibilité](https://www.kvraudio.com/news/juce-6-1-released---includes-accessibility-support-52875)
- [ADC24, atelier de design inclusif (PDF)](https://midi.org/wp-content/uploads/2025/03/ADC24-inclusive-design-workshop.pdf)
- [Forum Waves, accessibilité pour les lecteurs d'écran](https://forum.waves.com/t/accessibility-of-waves-plugin-ui-for-screen-reader-users/8071)
