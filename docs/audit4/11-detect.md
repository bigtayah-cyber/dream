# Audit 4 : Orbe Detect (tempo, tonalité, accordage du 808), vu depuis la voix

*10 nov. 2026. Lecture de `DetectProcessor` et `DetectEditor.cpp`. L'interface affiche déjà les confiances, les tempos alternatifs, la deuxième tonalité, le tap, la roue de Camelot et l'accordage du 808. Aucune mesure de justesse faite. Rien n'a été écouté.*

## Références de la catégorie

- **Mixed In Key** : la référence des DJ. Test DJ TechTools (66 morceaux Beatport, vérité terrain par vote de 6 auditeurs) : **86 %** pour Mixed In Key, 70 % pour Serato et Rekordbox, **65 %** pour KeyFinder, 47 % pour Traktor. L'erreur typique est la confusion majeur/mineur sur la bonne tonique.
Pour la voix, ce qui compte est la **fiabilité** de ce qu'on envoie à la correction de hauteur : une tonalité fausse rend toute la chaîne fausse.

## Constats

1. **Aucune mesure de justesse sur un corpus.** Sans chiffre, on ne sait pas si Detect est à 60 % ou à 85 %. Deux méthodes standard :
   - le score pondéré du MIREX : 1 pour la bonne tonalité, 0,5 pour la quinte, 0,3 pour le relatif, 0,2 pour le parallèle ;
   - un jeu public annoté, par exemple GiantSteps Key, de la musique électronique (je ne l'ai pas vérifié aujourd'hui).
2. **L'accordage du beat n'est pas mesuré.** Les beats rap à base de samples sont souvent désaccordés de 10 à 40 cents (sample ralenti ou accéléré). Une tonalité « juste » mais décalée de 30 cents fait sonner faux la voix accordée par Autotune, qui vise le La 440. Fredonne calcule déjà un écart en cents pour la voix (`SegResult::cents`) ; Detect ne le fait pas pour le beat.
3. **Le résultat ne peut aller nulle part.** L'artiste recopie à la main « F mineur » dans 3 plugins. Le lien entre instances (« Fil ») est prévu pour plus tard dans l'audit 3.
4. Pas de mode « voix » : Detect écoute un mix ou un beat. Il ne dit pas si la **voix** chante dans la tonalité.

## Améliorations

| # | Amélioration | Impact | Effort | Risque | Change le son |
|---|---|---|---|---|---|
| DE-1 | Banc de justesse : 50 à 100 extraits annotés (beats rap et pop électro de l'artiste et jeu public), score MIREX dans la CI | 4 | M | Nul | Non |
| DE-2 | Accordage du beat en cents (pic fin du chroma sur 36 bins par octave), affiché à côté de la tonalité : « F mineur, +23 cents » | 4 | S | Faible | Non |
| DE-3 | Bouton « Copier » qui met « Fm +23 ct » dans le presse-papiers ; menu « Coller la tonalité » dans les menus Tonalité de Voile, Looks, Atelier, Autotune et des un-potard | 4 | S | Nul | Non |
| DE-4 | Si majeur et relatif mineur sont proches, afficher les deux avec « choisis à l'oreille » et un bouton qui joue l'accord de chaque candidat | 3 | S | Nul | Non |
| DE-5 | Accordage de référence transmis : Voile et Autotune acceptent une référence en cents (paramètre en fin de liste) | 3 | M | Faible | Seulement si réglé |

## Idée signature

**Carte des fausses notes** : placé sur la piste voix, Detect suit la hauteur chantée avec le YIN de Fredonne. Il la compare à la tonalité du beat (mémorisée lors d'une analyse précédente ou collée) et affiche :

- le pourcentage du temps chanté hors gamme ;
- les notes en cause, par exemple « 70 % de tes Mi sont à −35 cents » ;
- les instants concernés sur la ligne de temps.

L'artiste sait alors s'il doit réenregistrer, corriger, ou changer la gamme d'Autotune. Les détecteurs donnent une tonalité ; aucun ne juge le chant par rapport à elle. Briques : YIN, la segmentation de Fredonne, la tonalité de Detect. Effort M.

## Sources

[Comparaison des détecteurs de tonalité, DJ TechTools](https://djtechtools.com/2015/11/16/key-detection-software-comparison-2015-edition)
