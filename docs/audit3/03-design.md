# Audit 3, n° 3 : design des interfaces

*Captures vues : `scratchpad/v3/voile_sombre.png`, `v3/at_edit_sombre.png`, `rel_sombre.png` (kit v3) ; `looks_a.png`, `galet_a.png`, `cab.png`, `onek_all.png` (rendu « Classique »). Les 23 éditeurs incluent déjà les en-têtes v3 (migration en cours) : certaines captures « classiques » datent peut-être d'avant leur migration. Rien n'a été redimensionné ni manipulé en direct.*

## 0. Verdict

Le kit v3 (Voile, Autotune, Relance) **tient la comparaison** avec un FabFilter ou un Output de 2025 : grande zone de visualisation qui raconte ce que fait le plugin, typographie unique à chiffres tabulaires, contraste WCAG calculé (`Theme.h:24-36`), accent de couleur par famille. Le rendu « Classique » (vis, potards chromés, afficheurs à matrice de points, interrupteur à levier) date de 2015 et fait « kit maison » à côté. Aujourd'hui, **trois langages visuels coexistent** : v3 plat, rack classique et panneaux un-potard teintés. L'impression « cheap » vient d'abord de cette incohérence, pas d'un écran en particulier.

## 1. Ce qui fait amateur (par ordre d'impact)

1. **Trois langages, et un bouton qui les échange.** Le thème « Classique » reste proposé par le menu de thème (`Theme.h:12`). Un produit haut de gamme n'a qu'une identité ; le mode clair et le mode sombre suffisent. Proposition : garder « Classique » seulement pour les projets déjà ouverts avec, et ne plus l'afficher dans le menu.
2. **Pas de hiérarchie dans les écrans denses.** Galet a 23 potards de même taille et de même poids. Cabine en a 15, en 3 rangées identiques. Atelier en a 38. Ce qui distingue un Pro-Q ou un Saturn, c'est **un geste principal** (la courbe), des réglages secondaires plus petits, et un tiroir « avancé ». Galet : la forme d'onde et 4 potards (Division, Profondeur volume, Filtre, Drive), le reste dans un tiroir. Atelier : les modules en bandes repliables, une seule ouverte à la fois.
3. **Des retours visuels qui ne disent rien.** Cabine : la « Réduction dB » est une échelle de 7 LED (1, 2, 3, 4, 6, 8, 10, 14), trop grossière pour régler un compresseur voix ; il faut une aiguille ou une barre continue, plus la courbe de transfert. Looks : « Correction demi-tons » reste vide tant que Tune n'agit pas. Master : pas de LUFS.
4. **Doublons et libellés incertains dans Autotune (édition).** « NOTE À ZÉRO » en haut et « NOTES À ZÉRO » à droite. Les curseurs Force, Vibrato et Vitesse sous la grille sont des curseurs plats à côté des potards du bas : deux familles de contrôles pour des grandeurs voisines. Les dièses du masque de notes sont des onglets qui chevauchent les naturelles, ce qui se lit mal. Proposition : un vrai clavier d'une octave (dièses noirs, plus courts), une seule commande « Notes à zéro ».
5. **Voile v3 : un vumètre « collé » dans la carte Tonalité.** La carte mélange réglage (tonalité et gamme) et mesure (VU). Mix et Sortie ont des capuchons bruns, seule couleur différente de l'écran, ce qui leur donne un poids qu'ils n'ont pas. Proposition : VU en bande verticale à droite, sur toute la hauteur, et capuchons identiques partout ; la couleur sert seulement à l'état.
6. **Relance v3 : une zone vide et des sigles cryptiques.** « Voix actives » montre 4 points gris. Les cercles E 8 T R B H demandent de lire l'aide. Proposition : afficher les noms au survol et colorer la timeline par type de réponse (la timeline le fait déjà pour les bulles).
7. **Typographie mixte dans les écrans classiques** : matrice de points, monospace et sans-serif condensée sur le même écran (Looks, Galet). En v3 il n'y a plus qu'une famille : c'est bien, à finir partout.
8. **Unités et valeurs** : « x10 », « 0.0 » sans unité (Cabine Saturation), points décimaux anglais dans les écrans classiques (`6.0`, `0.0 dB`) mais virgules en v3 (`7,0`, `0,0 dB`). Choisir la virgule partout : la suite est en français.

## 2. Ce qui manque pour un rendu « produit »

| Manque | Pourquoi | Effort |
|---|---|---|
| **Barre supérieure unique** sur les 26 : preset (‹ › liste, enregistrer, A/B), annuler, niveau égal, bypass, thème, aide, taille | C'est la signature d'un éditeur (FabFilter, Kilohearts). Aujourd'hui, chaque plugin a sa barre | M (dans le kit) |
| **Taille de fenêtre** : poignée et choix 75/100/125/150 % mémorisé | `setResizable` existe dans les éditeurs. [?] Non vérifié : mémorisation de l'échelle par plugin, et rendu net à 150 % sur Retina et Windows en 125 % | S |
| **États de survol et de focus** : halo d'accent, valeur affichée au survol | Indispensable pour la découverte. [?] Le focus clavier n'est pas visible sur les captures | S |
| **Animations d'état** : fondu des modules qu'on allume, aiguilles à balistique (le VU en a une, `kit_vu_test`) | Donne la sensation « vivante » d'Output et de Baby Audio | S |
| **Écran d'accueil d'un preset** : une phrase, et une indication de ce qu'il faut tourner en premier | Looks le fait (sous-titre) ; à généraliser aux 26 | S |
| **Icônes de plugin et page d'installation cohérentes** (`DW-5xx/6xx/7xx` déjà présents) | Identité de gamme | S |

## 3. Accessibilité

- Les contrastes sont calculés en v3 (cible 4,6:1) : très bien. Les écrans classiques ont des libellés gris sur gris sous 3:1 (Galet « FRÉQUENCE » désactivé, Looks « DLY / REV » éteints) : ils disparaissent si on ne passe pas en v3.
- La couleur seule porte l'état ON/OFF (LED) dans les écrans classiques. En v3, la pastille et le texte le disent tous les deux : à garder.
- [?] Navigation au clavier et lecteurs d'écran (`setTitle`/`setDescription` sur les potards) : non vérifiés.

## 4. Un-potard (direction à garder)

Les panneaux teintés (bleu, rouille, prune, olive) avec VU crème sont la partie la plus « objet » de la suite : c'est cohérent et ça se reconnaît. Seuls changements : retirer les vis et la matière brossée, qui les rattachent au rendu Classique ; afficher en grand, sous le potard, **ce que fait la position** (« doubles + brume », « slapback ») : c'est le conseil de l'audit 2-03, pas encore fait d'après les captures.
