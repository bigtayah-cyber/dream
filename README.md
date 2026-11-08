# dreamaws plugins

Deux suites de plugins (VST3 / AU, macOS), interface « matériel de studio » dessinée procéduralement.

## Orbe : générateur musical
Accords, mélodie, basse et batterie en 28 styles, 7 gammes. Verrous par accord, jeu des accords (plaqués, arpèges),
humanisation, aperçu sonore, sortie MIDI, glisser-déposer du fichier MIDI vers le DAW. Génération reproductible par graine.

## Voile : chaîne vocale
Correction de hauteur dure, saturation, doubles, delay et reverb (4 macros, 8 presets).

## Structure
- `Source/Kit/Kit.h` : kit d'interface partagé (métal brossé, potards, faders, voyants, afficheur)
- `Source/Orbe/` : moteur (`OrbeCore.h`, `OrbeStyles.h` généré), synthé d'aperçu, plugin, éditeur
- `Source/Engine.h`, `PluginProcessor.*`, `PluginEditor.*` : Voile
- `tests/` : moteurs (`offline.cpp`, `orbe_test.cpp`), plugin (`orbe_audio.cpp`), capture d'interface (`snapshot.cpp`)
- `tools/gen_styles.js` : régénère `OrbeStyles.h` depuis orbe.html
- `installer/` : installeur macOS pour tous les plugins

## Tests
```
g++ -O2 -std=c++17 tests/orbe_test.cpp -o orbe_test && ./orbe_test
g++ -O2 -std=c++17 tests/offline.cpp -o offline_test && ./offline_test
cmake -B build && cmake --build build --target orbe_audiotest && xvfb-run -a ./build/orbe_audiotest_artefacts/*/orbe_audiotest
```

## Licence
JUCE 8 : vérifier les conditions avant toute vente (https://juce.com/juce-8-licence).
