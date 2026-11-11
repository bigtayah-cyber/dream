# Audit 4 : Gorge (un-potard, lampes, grain, râle, slapback)

*10 nov. 2026. Lecture de `OneKnobModels.h` (section GORGE). Latence 0. Moteur déjà mesuré dans les audits 2 et 3. Rien n'a été écouté.*

## Références de la catégorie

- **Soundtoys Decapitator** : 5 styles (A : préampli de bande Ampex 350 ; E : Chandler/EMI TG ; N : Neve 1057 ; T et P : triode et pentode de la Culture Vulture), Drive, Punish (+20 dB), **Thump** (bosse au point de coupure grave avant saturation), Tone (bascule), coupe-haut et **Steep** (30 ou 6 dB/oct), **Auto** (niveau compensé), Mix.
- **Baby Audio Comeback Kid** : le Ducker fait suivre l'écho à l'attaque du sec, et Destiny ajoute un peu de hasard.
- Compresseurs à lampes vari-mu (Fairchild, Manley) : la référence classique du « glue » doux. Je ne les ai pas vérifiés en ligne aujourd'hui.

## Ce que Gorge fait bien

- Le grain n'ajoute que la **différence** sur la bande de 150 à 900 Hz : il vaut exactement 0 quand le potard est à 0.
- Le drive suit les syllabes.
- Le slap est calé sur le tempo et borné de 60 à 140 ms.

C'est plus fin que Decapitator sur une voix, qui sature toute la bande.

## Ce qui manque pour le niveau pro

1. **Pas d'écoute « delta »** alors que c'est gratuit : le grain est déjà calculé comme une différence. Decapitator n'en a pas. soothe2 l'a, et c'est l'outil qui apprend à doser.
2. **Le slap est toujours au centre et toujours filtré pareil.** Les ingés rap le placent souvent du côté opposé à la double, ou en mono très sombre.
3. **On ne voit pas la réduction du vari-mu** dans les mêmes unités que le reste. La réduction s'affiche, mais pas le « grain ajouté » en dB.
4. **Le râle n'agit que sur les syllabes graves voisées.** Sur un rap aigu ou chuchoté, il ne s'entend jamais, et rien ne le signale.

## Améliorations

| # | Amélioration | Impact | Effort | Risque | Change le son |
|---|---|---|---|---|---|
| GO-1 | Bouton « Écouter le grain » (delta = la différence ajoutée), gardé d'un preset à l'autre comme dans soothe2 | 4 | S | Nul | Non |
| GO-2 | Deux témoins sous le potard : « grain +x dB » et « lampes −y dB » | 3 | S | Nul | Non |
| GO-3 | Slap : position (centre, gauche, droite, opposé à l'autre bord) et couleur (sombre ou téléphone), dans le volet | 3 | S | Faible | Non (défaut identique) |
| GO-4 | Témoin « râle actif » allumé seulement quand il agit | 2 | S | Nul | Non |
| GO-5 | Option « Thump » dans le volet : bosse de +2 à +4 dB à 150 Hz avant le grain, comme Decapitator, pour une voix rap plus « poitrine » | 3 | S | Faible | Oui si activée |

## Idée signature

**Slap qui suit le flow** : le niveau du slap baisse quand le débit syllabique monte. Le grain dynamique compte déjà les syllabes. Sur un couplet rapide, le slap s'efface et ne brouille plus les mots ; sur les fins de phrase et les mots tenus, il revient. Le Ducker de Comeback Kid suit le niveau. Celui-ci suivrait le **débit**, ce qu'aucune référence citée ne fait. Effort S.

## Sources

[Decapitator, Splice](https://splice.com/blog/get-creative-distortion-using-soundtoys-decapitator) · [Comeback Kid, Tape Op](https://tapeop.com/reviews/gear/137/comeback-kid-delay-echo-plug-in) · [Manuel de soothe2 (delta)](https://oeksound.com/manuals/soothe2/)
