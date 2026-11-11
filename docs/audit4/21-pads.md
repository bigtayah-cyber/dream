# Audit 4 · Orbe Pads (découpe en 16 pads)

*Lecture de `PadEngine.h`, `SamplerDsp.h`, `SamplerCommon.h` et `PadsProcessor.cpp`. Bancs : `/tmp/b_aud4s/wsola.cpp` et `wsola2.cpp` (moteur partagé avec Sample). Rien n'a été écouté.*

## Références
- **Serato Sample 2** : *Find Samples* (32 repères utiles), repères aléatoires ou à intervalles égaux, quantisés ; tonalité et accord fin ; synchro hôte. Son **Pitch 'n Time** est jugé au niveau de Kontakt 7 et du Warp d'Ableton (passage de 105 à 65 BPM). Points faibles : peu d'effets, pas de boucle.
- Slicex, FPC, Battery (connaissance générale) : découpe sur les transitoires, couches de vélocité, sorties séparées, ADSR par pad. Arcade : modificateurs par touche, jeu dans la tonalité.

## Mesures (nouveau : time-stretch)
WSOLA à fenêtre fixe (N = 2 048, Hs = 512, ±256), sur des clics de 2 ms (crête 0,8) et sur un son tenu à 220 Hz :

| Durée | Transitoires | Son tenu |
|---|---|---|
| ×0,7 à ×0,9 (accélérer) | Crêtes à 0,28–0,44, soit **−5 à −9 dB** : attaques ramollies | ±0,85 dB |
| ×1,25 | Doublon à −29,6 dB | ±0,85 dB |
| ×1,6 (ralentir) | **Doublon à −13 dB, pré-écho à −11,5 dB** : coups doublés (« flam ») | ±0,86 dB |

Les sons tenus passent bien. Les attaques, non : c'est le défaut classique d'un WSOLA sans détection de transitoires.

## Constats (code)
1. **Risque de crash toujours ouvert** (A10 de l'audit 3). Les voix gardent un `const Bank*` brut. `Swap::publish` libère une banque remplacée depuis plus de 30 s, au publish suivant. Or une voix peut durer plus longtemps : un échantillon de 60 s joué à −24 dt dure 240 s. Si la banque change deux fois pendant ce temps, la voix lit de la mémoire libérée.
2. Pas de décroissance, de maintien, de début ou fin fins, ni de vélocité vers le filtre ou la hauteur.
3. Une seule sortie ; le gain `master` est appliqué sans lissage.
4. La tonalité du sample est détectée (`osamp::Key`) mais pas utilisée par pad.

## Améliorations
| # | Amélioration | Impact | Effort | Risque |
|---|---|---|---|---|
| P1 | **Banques par générations** : chaque voix note sa génération ; l'audio publie la plus ancienne encore jouée ; le thread de message libère ce qui est plus ancien | 5 (crash) | S | Faible |
| P2 | Stretch **qui préserve les transitoires** (repartir au début de l'attaque sans recouvrement) ; fenêtre de 1 024 pour la batterie, 2 048 pour les sons tenus | 5 | M | Moyen |
| P3 | ADSR, début et fin, vélocité vers volume, filtre et hauteur | 4 | S | Faible |
| P4 | 4 sorties stéréo assignables | 4 | M | Moyen (bus) |
| P5 | Gain master lissé | 2 | S | Faible |
| P6 | « Choisir 16 » : les transitoires les plus marquées et les plus différentes | 4 | S | Faible |

Micro-UX : repères aimantés aux passages par zéro ; Alt + survol pour écouter un pad ; glisser un chop vers FL ; note MIDI affichée sur chaque pad ; couleur par groupe de coupure.

## Signature
**« Pads accordés »** : on mesure la hauteur dominante de chaque chop (chroma), puis l'option « dans la tonalité » transpose chaque pad vers le degré le plus proche. Les chops se jouent alors en mélodie sans fausse note. Serato transpose l'échantillon entier ; ici, chaque chop est traité à part. Effort S/M.

## Sources
- Serato Sample 2 : https://www.musicradar.com/reviews/serato-sample-2-review
- Slicex, FPC, Battery, Arcade : connaissance générale.
