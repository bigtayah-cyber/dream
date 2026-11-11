# Audit 4 : Orbe DeEsser

*10 nov. 2026. L'audit 3 (`deesser-denoiser.md`) a déjà mesuré et proposé le seuil relatif (`DeEssDyn`), le Split en plateau aigu, le mode Delta, la course log et l'ouverture vers 2,5 kHz, ainsi que le lookahead lié à l'attaque. Je ne les répète pas, je les complète. Rien n'a été écouté.*

## Références de la catégorie

- **FabFilter Pro-DS** :
  - traitement **pleine bande** (conseillé pour une voix seule) ou **bande séparée** ; le mode **Allround** sert au mix et au mastering ;
  - **lookahead jusqu'à 15 ms** (10 ms conseillées sur une voix), désactivable : latence nulle sans lookahead ;
  - **sidechain externe** (une voix propre pour déclencher) ;
  - **liaison stéréo** de 0 à 100 %, puis **mid seul ou side seul** au-delà ;
  - deux écoutes : « Audition Sidechain » et « Audition Triggering ».
- **oeksound soothe2** : mode Soft (moins dépendant du niveau), Delta gardé entre presets, Trim du traité, annuler séparé pour A et B.

## Ce qui manque encore après les correctifs de l'audit 3

1. **Aucune entrée sidechain.** Avec Pro-DS, l'ingé déclenche le de-ess d'un bus de choeurs à partir de la voix lead propre.
2. **Pas de stéréo réfléchie** : le détecteur prend max(|G|, |D|), avec une liaison fixe à 100 %. Il n'y a ni M/S, ni liaison réglable. Sur une double stéréo, la sifflante d'un seul côté baisse les deux.
3. **Le lookahead est fixe à 1 ms.** Pro-DS va jusqu'à 15 ms et en fait un réglage visible, avec la latence annoncée.
4. **Rien n'est fait pour le « ch » et le « j » français** : une seule bande sert à la fois au « s » (6 à 10 kHz) et au « ch » (2,5 à 5 kHz). Aucun de-esser cité ne traite les deux séparément.
5. **Le graphe de réduction n'est pas daté.** Pro-DS montre un historique défilant où chaque sifflante traitée est marquée. Ici, l'éditeur v3 doit au minimum défiler sur 5 s, avec les événements visibles.
6. **Pas de défaut « voix rap »** : le seuil absolu de −30 dBFS ne fait rien sur une voix à −24 dBFS crête (mesure de l'audit 3).

## Améliorations

| # | Amélioration | Impact | Effort | Risque | Change le son |
|---|---|---|---|---|---|
| DS-1 | Lookahead réglable de 0 à 10 ms (défaut 5 ms), latence déclarée et affichée | 3 | S | Faible | Oui (version d'état) |
| DS-2 | Liaison stéréo de 0 à 100 % + mode M/S (mid seul / side seul) | 3 | M | Faible | Non (défaut 100 %) |
| DS-3 | Entrée sidechain externe (bus optionnel, désactivé par défaut) | 3 | M | Moyen (bus et état AU) | Non |
| DS-4 | Historique défilant de la réduction, avec une marque par sifflante traitée et le compteur « −x dB max sur 5 s » | 3 | S | Nul | Non |
| DS-5 | Bouton « Auto-seuil » : écoute 5 s et place le seuil relatif 4 à 6 dB au-dessus du médian des sifflantes | 4 | S | Faible | Non |
| DS-6 | Trim du traité (soothe2), pour un A/B honnête en mode Large bande | 2 | S | Nul | Non |

## Idée signature

**« S » et « CH » séparés** : deux détecteurs relatifs `DeEssDyn`, l'un sur 5,5 à 11 kHz (« s », « z ») et l'autre sur 2,5 à 5 kHz (« ch », « j »), avec chacun sa quantité. Une règle de centroïde empêche le détecteur « ch » de réagir aux voyelles brillantes : il ne s'active que si le segment est non voisé, d'après le YIN déjà présent dans la suite.

- Pour un rap français, « ch », « j » et « ge » sont des sifflantes à part entière, que les de-essers anglo-saxons, réglés sur le « s », laissent passer.
- Effort M. Interface : deux potards « S » et « CH » à côté du seuil.

## Sources

[Pro-DS, contrôles avancés (FabFilter)](https://www.fabfilter.com/help/pro-ds/using/advancedcontrols) · [Manuel de soothe2](https://oeksound.com/manuals/soothe2/)
