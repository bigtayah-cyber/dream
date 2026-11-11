# Audit 4 · Orbe Beat

*Lecture de `OrbeBeatEngine.h` et `OrbeBeatProcessor.cpp`. Les timbres ont été traités dans l'audit 3 (B3). Rien n'a été écouté.*

## Références
- **XLN XO** : carte des one-shots de tout l'ordinateur, classés par ressemblance ; *Playground* pour générer des variations ; **Accentuator**, **Nudge** et gabarits de groove ; glisser-déposer audio et MIDI.
- FPC, Addictive Drums 2, Beatmaker (connaissance générale) : **sorties séparées**, kits échantillonnés à plusieurs couches de vélocité.

## État
- 8 pistes synthétisées et 7 kits (Trap, Drill, Boom bap, Lo-fi, Jersey, House, Rage).
- 4 motifs enchaînables, 16 ou 32 pas, swing, rolls.
- 808 jouée sur les degrés d'une gamme ; MIDI GM ou canal 10 ; mute et solo ; export MIDI.
- Choke : le charleston fermé coupe l'ouvert, et une 808 coupe la précédente.

## Constats
1. **Une seule sortie stéréo** : impossible de compresser le kick seul ou d'envoyer la snare en reverb dans le mixeur de FL. C'est le premier manque pour un producteur FL.
2. **Moteur mono** : pas de pan par piste, pas de clap stéréo.
3. **Pas d'échantillons** : on ne peut pas utiliser son propre kick, alors que le rap se fait au one-shot.
4. **Réglages par piste minimes** (volume, mute, solo) : hauteur et décroissance sont figées dans le kit.
5. **Ni nudge ni vélocité aléatoire par piste** : pas de snare « paresseuse ».
6. Les groupes de coupure ne se règlent pas.

## Améliorations
| # | Amélioration | Impact | Effort | Risque |
|---|---|---|---|---|
| T1 | **Sorties séparées** : 8 bus stéréo optionnels | 5 | M | Moyen (bus : FL, pluginval) |
| T2 | Par piste : hauteur ±12, décroissance, ton, pan, envoi | 5 | S | Faible |
| T3 | **One-shot glissé** sur une piste (décodeur de Pads) | 5 | M | Faible |
| T4 | Nudge ±30 ms et vélocité aléatoire par piste ; gabarits de groove | 4 | S | Faible |
| T5 | Groupes de coupure réglables (le choke actuel reste par défaut) | 2 | S | Faible |
| T6 | Timbres refaits (B3 de l'audit 3) | 4 | M | Moyen |
| T7 | Fill automatique aux mesures 4 et 8, avec graine | 4 | S | Faible |

Micro-UX : peindre les pas en glissant ; vélocité à la molette ; roll au clic droit (2, 3, 4 ou 6) ; copier et coller un motif ; dé par piste ; écoute du son au clic sur le nom de la piste.

## Signature
**« Groove de la voix »** : on dépose une phrase rappée de 2 mesures. Le détecteur de transitoires de Pads repère les syllabes. Beat place alors les charlestons et les ghost notes **sur le flow** : accents sur les syllabes appuyées, snare qui respire dans les silences. L'audit 3 proposait « Beatbox vers Beat » (I1) ; ici, c'est le **rythme du texte** qui écrit le beat. XO varie au hasard, sans écouter la voix. Effort M.

## Stabilité
- Les cellules sont des atomiques `uint16` : c'est sain. Le nudge (T4) ne tient pas dans ces 16 bits ; il lui faut un second tableau atomique.
- T1 : un projet ancien doit se rouvrir en stéréo simple. À tester dans FL.

## Sources
- XO : https://www.bestservice.com/en/xo.html
- Comparatif des kicks 2026 : https://musictech.com/guides/buyers-guide/best-kick-drum-plugins-2026-tested-reviewed
- FPC, Addictive Drums, Beatmaker : connaissance générale.
