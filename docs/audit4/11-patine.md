# Audit 4 : Patine (un-potard, préampli, opto, air, courbe cible)

*10 nov. 2026. Lecture de `OneKnobModels.h` (section PATINE). Latence 0. Niveau stable d'après les audits 2 et 3. Rien n'a été écouté.*

## Références de la catégorie

- **Soundtoys Decapitator** : Tone (bascule du sombre au brillant), coupe-haut avec Steep, **Auto** (niveau compensé quand on monte le drive), Mix.
- **oeksound soothe2** :
  - **Delta** (on écoute ce qui est retiré, et ce réglage peut rester actif d'un preset à l'autre) ;
  - **Trim** sur le seul signal traité, pour comparer à niveau égal ;
  - bypass doux ;
  - qualité de rendu hors ligne séparée ;
  - annuler/rétablir séparés pour A et B.
- **iZotope Nectar 4** : le **Vocal Assistant** propose un point de départ avec des intentions (forme, intensité, effets, largeur), puis un EQ qui « suit » la voix.
- **Baby Audio Smooth Operator** (équilibreur spectral) : référence connue, que je n'ai pas vérifiée en ligne aujourd'hui.

## Constats

- Patine est le plus « finisseur » des quatre :
  - une triode ADAA sous 2,5 kHz, qui n'ajoute que la différence ;
  - un opto en série ;
  - une densité FET en parallèle ;
  - un de-esser avec air ;
  - un égaliseur lent à 8 bandes vers une cible « voix pop finie ».
- **La cible est fixe et hypothétique** (« HYPOTHÈSE : pente douce au-dessus de 1 kHz », `OneKnobModels.h:535`). Une seule cible pour le rap sec, la pop aérienne et la voix grave, c'est le point faible.
- **L'égaliseur lent n'est pas visible.** On ne sait pas ce qu'il corrige ni de combien. Le Vocal Assistant et Smooth Operator affichent leur courbe.
- **Pas de delta**, alors que la triode et la densité sont additives.
- Le Drive du volet va jusqu'à +18 dB, mais l'affichage est en % et non en dB.

## Améliorations

| # | Amélioration | Impact | Effort | Risque | Change le son |
|---|---|---|---|---|---|
| PA-1 | Petite courbe de l'égaliseur cible dans le volet : la cible, la moyenne mesurée et la correction appliquée, sur 8 points | 4 | S | Nul | Non |
| PA-2 | Choix de la cible (« Pop finie », « Rap sec », « Aérien », « Sombre ») dans l'option, en fin de liste | 4 | S | Faible | Non pour les états existants |
| PA-3 | Bouton « Écouter la patine » (delta : couleur ajoutée + correction de l'égaliseur) | 3 | S | Nul | Non |
| PA-4 | Drive affiché en dB (0 à +18) | 2 | S | Nul | Non |
| PA-5 | Qualité « rendu » : à l'export (`isNonRealtime()` de JUCE), triode suréchantillonnée ×2 | 2 | S | Faible | Seulement à l'export |

## Idée signature

**Cible apprise d'une référence** : un bouton « Apprendre » fait écouter à Patine 10 à 20 s d'une voix de référence (l'acapella d'un titre que l'artiste aime, jouée sur la même piste FL). Patine enregistre sa moyenne sur 8 bandes comme cible. Ensuite, le potard rapproche la voix de **cette** référence, avec les mêmes garde-fous qu'aujourd'hui (±6 dB, 60 % de l'écart, 1 dB/s).

- Les briques existent : l'égaliseur à 8 bandes, la moyenne lente et l'état sauvegardé.
- En un seul bouton, c'est un « match EQ » de voix. Les correspondances d'EQ existent ailleurs dans des outils de mastering, pas dans un finisseur vocal à un potard.
- Effort S à M (la capture et l'état sont simples ; ce qui coûte, c'est l'interface).

## Sources

[Manuel de soothe2](https://oeksound.com/manuals/soothe2/) · [Nectar 4, fonctions](https://www.izotope.com/en/products/nectar/features) · [Decapitator, Splice](https://splice.com/blog/get-creative-distortion-using-soundtoys-decapitator)
