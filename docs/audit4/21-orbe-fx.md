# Audit 4 · Orbe FX (Orbe chargé comme effet)

*Lecture de `CMakeLists.txt` (cible `OrbeFX`) et des blocs `#ifdef ORBE_FX` d'`OrbeProcessor`. Rien n'a été écouté.*

## Ce que c'est
C'est le moteur d'Orbe, déclaré comme **effet** : entrée et sortie stéréo, `NEEDS_MIDI_OUTPUT TRUE`, pour pouvoir le charger dans le mixeur de FL. L'audio d'entrée **passe tel quel** et l'aperçu du synthé **s'y ajoute** (`OrbeProcessor.cpp:281`).

## Références
- **Scaler 3** : il détecte accords, tonalité et gamme **depuis l'audio**. C'est exactement ce qu'un générateur posé sur une piste audio devrait faire.
- Wrapper de FL (connaissance générale, à vérifier) : un effet qui sort du MIDI doit passer par un port de sortie MIDI vers un canal.

## Constats
1. **L'entrée audio n'est pas écoutée.** Orbe FX est posé sur le sample ou le beat, mais n'en tire rien. C'est pourtant la seule raison d'avoir une version effet.
2. **L'aperçu s'ajoute au signal de la piste** : pas de réglage de mélange, et l'aperçu reste actif même quand la sortie MIDI est utilisée. On risque d'entendre le son en double (aperçu + instrument piloté).
3. **Le routage MIDI dans FL n'est expliqué nulle part.**
4. Le reste manque comme dans Orbe (édition d'accord, verrous, sections) : voir `21-orbe.md`.

## Améliorations
| # | Amélioration | Impact | Effort | Risque |
|---|---|---|---|---|
| F1 | **Écouter l'entrée** : chroma et tonalité (code de Detect), puis « appliquer » tonique et gamme en un clic | 5 | S | Nul |
| F2 | **Accords suivis** : accord courant du sample (chroma par temps, gabarits majeur / mineur / 7e) ; lead et basse générées par-dessus | 5 | M | Faible |
| F3 | Aperçu coupé par défaut quand la sortie MIDI est active ; potard de mélange de l'aperçu | 4 | S | Faible (à noter dans le CHANGELOG) |
| F4 | Panneau « Routage FL » en 3 étapes illustrées | 3 | S | Nul |
| F5 | Affichage du tempo et de la position reçus de l'hôte | 2 | S | Nul |

Micro-UX : tonalité détectée affichée avec sa confiance ; témoin « j'écoute » quand du signal entre ; bouton « figer l'analyse ».

## Signature
**« Orbe écoute le sample »** : posé sur la piste du sample, Orbe FX détecte la tonalité et **les accords mesure par mesure**. Il propose ensuite une 808 qui suit leurs fondamentales (envoyée en MIDI à Orbe 808), une lead dans la gamme et une batterie au tempo. C'est la réponse au geste le plus courant en rap : « j'ai un sample, il me faut la basse et la topline ». Avec I2 (audit 3), on obtient la chaîne sample → Orbe FX → Orbe 808. Effort M.

## Stabilité
- L'analyse ne doit **jamais** tourner dans `processBlock`. Il faut copier le signal dans un tampon circulaire sans verrou (modèle `SeqPub`), puis faire la FFT dans un thread ou un `Timer`.
- Orbe et Orbe FX partagent le même état : vérifier en CI qu'un état enregistré par l'un se recharge dans l'autre.

## Sources
- Scaler 3 : https://www.pluginboutique.com/products/14563-Scaler-3
- Routage MIDI des effets dans FL : connaissance générale, à vérifier dans le manuel.
