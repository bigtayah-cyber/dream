# Audit 4 : Orbe Fredonne (voix fredonnée vers MIDI)

*10 nov. 2026. Lecture de `FredonneProcessor` et `FredonneEditor`.*

*Ce qui existe : prise de 120 s qui se déclenche au premier son (pré-roll de 4096 échantillons), YIN, grille (libre, 1/4, 1/8, 1/16), octave, calage sur la gamme, justesse, tonalité et mode Auto, suivi de l'hôte, sortie MIDI en direct, et **poignée « .MID » à glisser** dans l'hôte. L'état garde la prise.*

*Rien n'a été écouté.*

## Références de la catégorie

- **Vochlea Dubler 2** :
  - quantificateur de tonalité sur **15 gammes** ou une gamme perso ;
  - **générateur d'accords** très configurable ;
  - **pitch bend** dédié ;
  - jusqu'à **8 déclencheurs** vocaux appris (un « boum » devient un kick : note MIDI, 808 intégré, ou les deux) ;
  - **vélocité tirée du volume**, et une enveloppe qui peut moduler d'autres paramètres.

  Critique d'Attack Magazine : il faut un chanteur précis, sinon on corrige à la main.

## Constats

1. **La vélocité n'est pas la même selon la sortie.** Le fichier `.MID` tire la vélocité du niveau de la note (`OrbeAnalysis.cpp:518`, de 8 à 123). La sortie MIDI en direct, elle, envoie 106 pour toutes les notes (`FredonneProcessor.cpp:112`). Jouer dans FL et glisser le fichier donnent donc deux résultats différents.
2. **Les glissés sont perdus.** Un fredonnement de 808 (« ouuuu » qui descend) devient une suite de notes en escalier, ou une seule note. Pas de pitch bend, ni de note « glide ».
3. **On ne peut pas corriger les notes dans l'éditeur** : supprimer une note parasite, en fusionner deux. Le seul moyen est de changer la sensibilité ou la durée minimale et de tout recalculer.
4. **La quantification est un choix de grille, sans force réglable.** Une quantification à 100 % sur un fredonnement rap tue le swing ; une force de 50 à 70 % est l'usage courant dans les DAW.
5. **La sortie MIDI en direct dans FL** demande de régler le port MIDI de sortie du wrapper et un canal « MIDI Out ». La poignée `.MID` est donc le chemin principal, et l'aide doit le dire en premier.

## Améliorations

| # | Amélioration | Impact | Effort | Risque | Change le son |
|---|---|---|---|---|---|
| FR-1 | Sortie directe avec la même vélocité que le fichier, plus un réglage « dynamique » (de plate à pleine) | 3 | S | Faible | Non (MIDI seulement) |
| FR-2 | Force de quantification (0 à 100 %) et swing (de 50 à 66 %) | 4 | S | Faible | Non |
| FR-3 | Édition simple sur la vue des notes : clic pour supprimer, Alt-glisser pour fusionner, glisser verticalement pour transposer | 4 | M | Faible | Non |
| FR-4 | Pitch bend en option (plage ±2 ou ±12 dt) dans le fichier `.MID` et la sortie directe | 3 | M | Faible | Non |
| FR-5 | Aide FL : « Glisse la poignée .MID sur le piano roll d'un canal », placée en tête de l'aide | 3 | S | Nul | Non |
| FR-6 | Gammes en plus (penta mineure, dorien, phrygien, harmonique) pour le calage, comme Dubler | 2 | S | Nul | Non |

## Idée signature

**Fredonner la 808** : un mode « 808 » qui :

- force l'octave grave ;
- fusionne les notes reliées par un glissé continu en **une note longue plus un glide** : note suivante liée (« slide ») ou pitch bend calibré pour Orbe 808 ;
- cale la tonique sur l'accordage du 808 donné par Detect.

On fredonne la basse avec ses glissés, et on obtient une ligne de 808 trap prête à jouer. Dubler fait du pitch bend générique, mais ne transforme pas un glissé fredonné en slides de 808. Les briques existent : la segmentation YIN, la tonique de Detect et le moteur d'Orbe 808. Effort M.


## Sources

[Dubler 2, Attack Magazine](https://www.attackmagazine.com/reviews/gear-software/dubler2-your-voice-to-midi/)
