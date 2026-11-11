# Audit 4 · Orbe (générateur d'accords, de leads, de basse et de batterie)

*Lecture de `OrbeCore.h`, `OrbeStyles.h` et `OrbeProcessor.*`. Rien n'a été écouté.*

## Références
- **Scaler 3** : détecte accords, tonalité et gamme **depuis le MIDI et l'audio** ; voice leading automatique ; *Performances* (densité, tessiture, variations) ; **Bind to Keyboard** pour jouer les accords au clavier ; page *Arrange* (pistes et scènes) ; 50 sons, hébergement de VST et AU ; glisser-déposer MIDI.
- Captain Chords, InstaChord, Cthulhu (connaissance générale) : **une touche = un accord** ; mélodie et basse liées aux accords.

## État
Orbe fait déjà beaucoup :
- 8 styles « Rap » (Trap, Drill, Boom bap, Lo-fi, Phonk, Plugg, Rage, Jersey club), plus d'autres groupes ;
- 4 pistes : accords, lead, basse, batterie ;
- 7e, add9, emprunts ; 4 façons de jouer les accords ; humanisation ;
- graines reproductibles et voice leading ;
- glisser vers FL, synchro PPQ, sortie MIDI, dièses ou bémols.

## Ce qui manque
1. **Pas d'entrée MIDI** (`acceptsMidi() = false`). On ne peut ni jouer les accords au clavier, ni lui donner une mélodie à harmoniser. C'est la fonction la plus utilisée de Scaler et d'InstaChord.
2. **On ne peut pas éditer un accord** : il faut tout régénérer. Il manque « cet accord-là : renverser, changer de couleur (sus, 9, 11), substituer ».
3. Une longueur unique (4 ou 8 mesures), **sans sections** couplet / refrain.
4. **Pas de verrous** par piste ni par mesure.
5. Les tirages ne s'annulent pas.
6. L'aperçu sonore utilise un synthé à part (B6 de l'audit 3).

## Améliorations
| # | Amélioration | Impact | Effort | Risque |
|---|---|---|---|---|
| O1 | **Jouer au clavier** : C…B → degrés I…VII, vélocité gardée | 5 | M | **Moyen** : une entrée MIDI change les bus (cas d'Autotune). À faire en nouveau plugin « Orbe Jeu » ou sous `stateVersion` avec test dans FL |
| O2 | Clic sur un accord : renversements, couleurs, substitutions (relatif, emprunt, dominante secondaire), écoute immédiate | 5 | M | Faible |
| O3 | Verrous par piste et par mesure ; « régénérer le reste » | 4 | S | Faible |
| O4 | Sections A et B liées (même degrés, cadence variée) | 4 | M | Faible |
| O5 | Aperçu joué sur les moteurs de Synth et de Beat | 3 | S | Faible |
| O6 | Export MIDI par piste | 3 | S | Nul |
| O7 | Annuler les régénérations (historique des graines) | 4 | S | Nul |

Micro-UX : nom des accords (Cm7) **et** degrés (i, VI) affichés ; Alt + survol pour écouter un accord ; favoris de tirages ; bouton « même humeur » qui garde 50 % des degrés.

## Signature
**« Réponse à la voix »** : on fredonne ou on rappe 4 mesures. Le suivi de hauteur de Fredonne et la segmentation d'`OrbeAnalysis` en tirent le rythme et les notes appuyées. Orbe génère alors des accords qui **soutiennent** les notes chantées, et une lead qui **répond dans les silences** (appel et réponse). Scaler détecte les accords d'un audio ; il ne compose pas *autour* d'une voix. Effort M.

## Stabilité
- Tout passe par des graines : c'est reproductible. À garder pour O3 et O7.
- Ajouter un test : boucle de l'hôte (saut de PPQ en arrière) puis stop au milieu d'une note. Aucune note ne doit rester tenue.

## Sources
- Scaler 3 : https://www.pluginboutique.com/products/14563-Scaler-3
- Captain Chords, InstaChord, Cthulhu : connaissance générale.
