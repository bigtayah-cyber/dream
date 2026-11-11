# Orbe Autotune : synthèse de l'audit 4

*10 nov. 2026, branche `vague2-wip`. Aucune ligne du dépôt modifiée. Détails : `01-recherche-web.md` (sources),
`02-mesures-existant.md` (bancs, chiffres), `03-architecture-cible-et-plan.md` (cible, plan), `04-algos-pitch-formants.md`
et `05-son-cible-et-chaines.md` (recherches complémentaires). Bancs hors dépôt : `/tmp/b_at4/`.*

**Limites.** Rien n'a été écouté, ni Orbe, ni MetaTune. Les mesures portent sur des voix de synthèse réalistes (source
glottique, jitter, shimmer, souffle, formants, scoops, vibrato), pas sur de vraies prises. La comparaison avec MetaTune porte
sur ses **fonctions documentées**, pas sur son son.

## Verdict

Le jugement « assez mid, incomplet à côté de MetaTune » est **fondé**, mais pas pour la raison qu'on croirait.

Le cœur d'Orbe est de la bonne famille. C'est celle d'Auto-Tune : suivi de la hauteur, puis répétition ou suppression de
périodes entières (brevet Hildebrand, expiré en 2018). Il est déjà le meilleur moteur du dépôt :
- 87 à 90 % des trames sont posées à 10 ct près en hard-tune ;
- l'escalier est net dans les glissés ;
- les formants tiennent vers le haut.

Ce qui manque est ailleurs, et c'est mesurable :
1. **le réglage d'usine** empêche d'entendre le hard-tune ;
2. **les bords des notes** sont mal traités : l'attaque n'est pas accordée en Direct, chaque changement de note produit un
   raccord irrégulier, et les notes presque justes passent dans un filtre en peigne ;
3. il manque **les quatre réglages de comportement** qui font MetaTune et Auto-Tune : Sustain/Humanize, Note Stabilizer,
   Transition et vitesse négative ;
4. **l'entourage** est pauvre : doublage statique, harmonie faible, aucune couleur.

Un jour de travail (P0) devrait déjà changer la première impression. Il faut ensuite 1 à 2 semaines pour la « signature »
(P1), puis le timbre et l'entourage (P2-P3).

## 10 constats clés

1. **Le hard-tune est caché par défaut.**
   - Une instance neuve s'ouvre en rendu **Satin** et en latence **Direct**.
   - Les presets Robot, Trap moderne, Hyperpop, Pop, Naturel, Grosse voix et Harmonie **ne changent pas le rendu**
     (`render = −1`, voir `AutotuneProcessor.cpp:89`).
   - Charger « Robot » sur une instance neuve donne donc du Satin à vitesse 0 : **28 % de trames posées, contre 87 %** en
     rendu Robot (mesure [A]).
   - C'est probablement ce que l'artiste a entendu en premier.

2. **En Direct, l'attaque n'est pas accordée.**
   - À vitesse 0, la note n'est accrochée qu'au bout de 33 ms de médiane en Direct, contre 10,5 ms en Qualité (plancher de
     mesure : 9 ms). Environ 24 ms d'attaque passent donc non corrigées (mesure [A']).
   - Or le hard-tune se joue sur l'attaque.
   - MetaTune annonce environ 3 ms et une vitesse « sous 0 sans anticipation ».

3. **Chaque changement de note produit environ 10 ms de signal irrégulier** en hard-tune (mesure [C'] : 12 à 13 % des trames
   sur une tenue qui oscille entre deux notes ; 0 % en entrée et en Satin).
   - Le résultat est le même avec ou sans préservation des formants. Ce n'est donc pas la LPC, c'est le **raccord du lecteur
     au moment où le rapport de transposition saute**.
   - C'est le meilleur candidat pour le « grain » ou le côté « incomplet ». C'est à confirmer à l'oreille.

4. **Filtre en peigne sur les notes presque justes** (mesure [D]).
   - Quand la correction est comprise entre 0,5 et 3 ct, le rendu Précis mélange le signal sec et le signal traité, alors que
     leurs retards diffèrent (`TuneCore.h:153`).
   - Résultat : ondulation spectrale de 2,5 dB RMS, jusqu'à 7 dB sur certaines harmoniques. En Satin, elle n'est que de
     0,14 dB.
   - On l'entend comme une coloration « phasée » ou « tubulaire » sur une voix bien chantée.
   - La cause est identifiée, et la correction est simple.

5. **Le warble n'est ni maîtrisé, ni réglable** (mesure [C]).
   - Un vibrato à cheval sur deux notes fait basculer la sortie environ 10 fois par seconde. L'hystérésis de 0,2 demi-ton
     n'y peut rien.
   - MetaTune supprime ce warble (Note Stabilizer 40/80/200 ms), Auto-Tune l'évite (« Targeting ignores vibrato »), et
     certains artistes le recherchent (Future).
   - Orbe n'offre aucune de ces options.

6. **Aucun réglage de comportement façon MetaTune ou Auto-Tune.**
   - Il manque Sustain/Humanize (vitesse fonction de l'âge de la note), Note Stabilizer, une Transition séparée de la vitesse
     (Waves Tune RT : 15 / 120 ms), Flex-Tune, la vitesse négative et un mode Classic assumé.
   - Les trois rendus actuels (Satin, Précis, Robot) se chevauchent et parlent mal à un rappeur.

7. **La voix s'assombrit quand on transpose vers le bas** (mesure [E]).
   - Vers le haut, les formants sont excellents : 1,3 à 1,4 dB d'erreur à +3 et +7 dt.
   - Vers le bas, la pente spectrale perd 2,7 dB par octave à −3 dt et 9,6 dB par octave à −7 dt.
   - Cause probable : le résidu est rééchantillonné, ce qui étire l'impulsion glottique.
   - L'effet est faible pour la correction (±1 dt), mais fort pour Transposer vers le bas, la Grosse voix, l'harmonie basse
     et l'octave basse.

8. **Les voix soufflées ou chuchotées décrochent** (mesure [G]).
   - Sur une voix très soufflée, 5 à 11 % seulement des trames sont corrigées : le moteur la juge non voisée et la laisse
     passer sèche.
   - Il n'existe pas de réglage « Suivi », alors qu'Auto-Tune a « Tracking ».
   - En revanche, le souffle n'est **pas** « répété » en bourdonnement (mesure [I]) : cette hypothèse du rapport 04 n'est pas
     confirmée.

9. **L'entourage est pauvre** (mesure [F] et lecture du code).
   - Le doublage se résume à 2 copies avec un désaccord fixe (±5 à 15 ct) et des retards fixes (11 et 17 ms). Ce n'est pas
     un ADT.
   - L'harmonie n'a qu'une voix, placée au centre. Elle est granulaire en Qualité, et sans formants en Direct (une tierce
     au-dessus donne une voix de poupée).
   - Il n'y a aucune couleur (drive, air, de-esseur) ni aucun espace.
   - MetaTune n'a qu'un doubleur, mais réussi. Selon 05, un autotune qui sonne « complet » en solo, c'est un correcteur plus
     de l'épaisseur, de la couleur et de l'espace.

10. **Ce qui est solide et doit être gardé** :
    - justesse ≤ 0,2 ct sur une voix stable ;
    - escalier net en robot (0,8 % de trames entre deux notes, mesure [B]) ;
    - vibrato préservé en réglage naturel ;
    - consonnes et souffle rendus intacts ;
    - zéro allocation, latence Direct de 5 ms ;
    - CPU de 3,1 à 3,3 % d'un cœur (5,6 % avec doublage et harmonie) : marge confortable.

    Les tests du dépôt utilisent une voix parfaitement périodique, trop flatteuse : il faut y ajouter les bancs de 02.

## Plan en phases

| Phase | Contenu | Effort | Jalons (bancs de 02) |
|---|---|---|---|
| **P0 « ça sonne hard » (tout de suite)** | Chaque preset fixe le rendu (Robot → Robot) ; une instance neuve s'ouvre en Précis ; on supprime le mélange partiel sec/traité ; **première écoute** sur 3 vraies prises (point A11 d'audit 3) | 1 jour | Preset Robot sur instance neuve : ≥ 85 % de trames posées (28 % aujourd'hui) ; peigne à +2 ct ≤ 0,3 dB (2,5 aujourd'hui) |
| **P1 signature hard-tune** | Attaque accordée en Direct ; transitions propres (instrumenter le raccord, puis changer le rapport sur les marques de période, ou avancer le PSOLA-résidu) ; Note Stabilizer ; Warble dosable ; Transition ; Humanize/Sustain ; vitesse < 0 (dépassement, à valider à l'oreille) | 5 à 7 jours | Accroche en Direct ≤ 15 ms (33 aujourd'hui) ; trames irrégulières aux bascules ≤ 3 % (12 aujourd'hui) ; bascules par seconde réglables de 0 à ≥ 8 ; tests du dépôt intacts |
| **P2 timbre** | PSOLA sur le résidu à marques glottiques (architecture A de 04 ; à défaut, un égaliseur de pente) ; Suivi (pYIN et type de voix) ; Gorge et Formants dans les deux latences | 4 à 8 jours | Pente à −3 dt ≥ −0,7 dB/oct (−2,7 aujourd'hui) ; erreur à −7 dt ≤ 4 dB (13,6 aujourd'hui) ; voix soufflée en Suivi souple ≥ 60 % |
| **P3 épaisseur et couleur** | Doubleur ADT de 2 à 4 voix (bruit de hauteur et de temps, largeur L/C/R) ; harmonies de 1 à 3 voix à formants préservés, selon la gamme ou l'accord ; Couleur = Drive, Air, de-ess, Espace (delay synchronisé, réverb sombre), à partir des briques de la suite ; presets par profil (05 § 3) | 6 à 10 jours | Modulation d'amplitude du doubleur non périodique ; harmonie en Direct ≤ 2 dB d'erreur de formants à +4 dt ; CPU ≤ 12 % |
| **P4 identité et UI** | Style (Naturel, Pop, Hard, Classic) et macro Dureté ; heatmap entrée/sortie ; fonctions propres (warble et escalier dosables, gamme qui suit l'accord via Orbe, formant rythmé, doubleur « vraie prise » avec sa propre courbe) ; A/B, Keep My Scale | 4 à 8 jours | Test d'écoute ci-dessous |

**Ordre conseillé** : P0, puis **écoute**, puis P1. On ne lance P2 que si l'écoute montre que le timbre est en cause (et pas
seulement le comportement). P3 ensuite : la couleur ne doit pas servir à maquiller des transitions ratées.

**Validation finale** :
- 6 à 10 phrases de l'artiste ;
- rendus Orbe, MetaTune (rendu par l'artiste) et Orbe nu comme ancre basse ;
- loudness égalisée à ±0,5 LU ;
- écoute aveugle en ABX ou MUSHRA, par 2 auditeurs.

Critère de sortie : Orbe n'est pas distingué de la référence en hard-tune, ou il est préféré sur au moins 3 prises sur 4.
Comme le rappelle 05, les mesures servent à détecter les régressions, mais c'est l'oreille qui décide.

**Licences** : pas de GPL/LGPL (Rubber Band, SoundTouch, PESTO exclus) ; Signalsmith Stretch (MIT) et WORLD (BSD) possibles.
