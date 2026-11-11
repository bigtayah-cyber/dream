# Lot A · Version d'état et durée de vie des banques

*11 novembre 2026, branche `vague2-wip`. Périmètre : les 13 plugins studio et instruments, Orbe Detect et Orbe Mix. Hors périmètre, volontairement non touchés : Orbe Autotune, Voile, Looks, Relance, DeEsser, Denoiser, Atelier, Fredonne.*

## 1. Le patron `stateVersion`

Fichier : `Source/Kit/StateVersion.h`. Une seule propriété, `stateVersion`, sur l'arbre de l'APVTS. **Aucun identifiant de paramètre n'est touché** : l'automation et les anciens projets se relisent à l'identique.

| Valeur | Sens |
|---|---|
| absente (lue 0) | projet enregistré avant la vague 2 |
| 2 | vague 2 (nouvelle reverb, saturation compensée, limiteur de Master corrigé) |
| 3 et plus | réservé au prochain changement audible |

**Écriture.** Copier l'arbre, puis `kit::stampState(copie)` :

```cpp
void XProcessor::getStateInformation(juce::MemoryBlock& dest) {
    auto st = apvts.copyState();
    kit::stampState(st);                       // propriété « stateVersion »
    if (auto xml = st.createXml()) copyXmlToBinary(*xml, dest);
}
```
Quand le processeur garde déjà un hook (`beforeSave` d'`OrbeFace.h`), le tampon va dans le hook, sur `apvts.state`.

**Lecture.** Une absence n'est jamais une erreur : `setStateInformation` n'a rien à changer tant qu'aucune branche DSP ne dépend de la version. Le jour où un changement audible arrive :
1. un membre `int loadedVersion = kit::kStateVersion;` (nouvelle instance : version courante) ;
2. dans `setStateInformation`, **avant** `replaceState`, `loadedVersion = kit::stateVersionOf(arbre)` ;
3. le DSP teste `loadedVersion < 3` pour garder l'ancien chemin (modèle : `onek::classicForStateVersion`, `dspv` de Voile) ;
4. à l'écriture, `kit::stampState(st, loadedVersion)` : un projet ancien garde son ancien son tant que l'utilisateur ne choisit pas le nouveau.

Tant que rien ne dépend de la version, écrire `kStateVersion` est exact : le projet rouvert sonne déjà comme la version 2.

**Où c'est écrit** (tous les plugins sans version, hors Autotune et voix) : Orbe et Orbe FX (`OrbeProcessor`), Orbe Master, Orbe Mix, Orbe Cabine, Orbe Detect, Orbe EQ, Orbe Galet, Orbe Synth, Orbe 808, Orbe Beat, Orbe Pads et Orbe Sample (par `writeState` de `SamplerCommon.cpp`, qui garde l'en-tête binaire « OSMP » et l'échantillon). Déjà versionnés : les 4 un-potard (2) et Accroc (1, propre).

**Test.** `tests/state_version_test.cpp` (cible `state_version_test`, sans plugin) : un état ancien sans version se lit 0 ; le tampon écrit 2 sans modifier un seul paramètre ; l'aller-retour XML relit 2 ; un état plus récent se relit sans erreur.

## 2. Durée de vie des banques (Pads, Sample)

Fichier : `Source/Suite/OrbeSampler/Swap.h` (C++17 pur). L'ancienne `Swap` libérait un objet 30 s après son remplacement ; une voix peut durer 240 s : le thread audio lisait alors de la mémoire libérée.

Nouveau schéma, par **générations** : chaque objet publié reçoit un numéro croissant ; en fin de bloc, le thread audio annonce (`reportInUse`) la plus ancienne génération encore lue par une voix ; `publish` ne libère que plus ancien que cela, et jamais l'objet courant. Pas de verrou ni d'allocation côté audio. `prepareToPlay` appelle `quiesce()` (voix coupées, rien n'est plus tenu).

## 3. Mesures avant et après

| Sujet | Avant | Après |
|---|---|---|
| Banque de Pads : voix de 12 s, banque remplacée à chaque bloc | libérée à 30 s ; une voix de 240 s lit de la mémoire libérée | vivante tant que la voix joue (1 201 blocs, 1 201 publications), libérée dès qu'elle finit |
| ASan + UBSan, TSan (`sampler_test`) | non testé | 0 erreur ; 2 threads, 3 000 publications |
| Master, limiteur 40 Hz, 8 dB de réduction | DHT 1,16 % | 0,000 % (25, 30, 40, 60, 100 Hz à 44,1, 48 et 96 kHz) |
| Master, plafond −1 dBTP (banc de l'audit : sinus 12 kHz et salves de bruit) | −0,68 dBTP : dépasse de 0,32 dB | −1,01 dBTP (référence ×16) ; en test : bruit blanc pleine bande −1,035, signal tonal −1,15, musique simulée −1,13, jamais au-dessus du plafond à 44,1, 48 et 96 kHz |
| Master, LUFS | momentané seulement | momentané, court terme (3 s), intégré (portes −70 LUFS et −10 LU) ; identiques à Orbe Mix au centième sur 40 s de signal dynamique |
| Mix, pic vrai, sinus 12 kHz à −3,10 dBTP | −3,76 (sous-estimé de 0,66 dB) | −3,10 ; écart maximal à la référence ×16 : 0,07 dB (sinus de 1 à 21 kHz) |

Le code de pic vrai est **unique** : `Source/Dsp/TruePeak.h` (×4 polyphasé, 48 coefficients, affinage parabolique), utilisé par le limiteur de Master et par le compteur de Mix.

## 4. Reste à faire

- Écouter le limiteur de Master dans FL Studio (le maintien de 30 ms change la relâche perçue : de la 808 et des graves, pas du reste).
- Branche DSP future : premier usage réel de `loadedVersion` (voir le patron).
- Cible LUFS, écoute à niveau égal et Delta de Master (M3 et M4 de l'audit) : seuls l'intégré et le court terme sont faits.
