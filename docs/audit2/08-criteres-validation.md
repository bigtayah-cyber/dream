# Critères de validation des lots (vague 2)

*Validateur indépendant, 10 nov. 2026. Sources : checklist de sortie (`07-controle.md` § 9c), checklist FL (`04-usage-daw.md`), règles du projet et demandes de l'artiste.
Méthode allégée (consigne de l'artiste) : les rapports et mesures des agents font foi sauf incohérence flagrante ; le validateur fait des **contrôles par sondage** sur ce que les agents ne voient pas eux-mêmes.*

## A. Critères universels (chaque plugin touché par le lot)

| # | Critère | Preuve attendue |
|---|---|---|
| U1 | Tests console du plugin verts | sortie « OK / TOUT EST BON », code retour 0 |
| U2 | pluginval niveau 10 SUCCESS | `xvfb-run -a /tmp/pv/pluginval --strictness-level 10 --validate-in-process --skip-gui-tests "<.vst3>"` |
| U3 | Aucun ID APVTS existant modifié ou supprimé ; ajouts **en fin de liste** | `git diff <base> -- Source` filtré sur `ParameterID` / `AudioParameter*` |
| U4 | Aucun nouveau booléen APVTS (utiliser un choix à 2 entrées) | même diff |
| U5 | Neutralité à 0 et bypass exact | erreur < −120 dB, saut < −60 dBFS |
| U6 | Latence déclarée = mesurée | écart 0 échantillon |
| U7 | Zéro allocation et zéro verrou dans le thread audio | lecture ciblée de `process` |
| U8 | Zéro NaN : une entrée NaN/inf est nettoyée (`kit::sanitize` ou équivalent) et la sortie est finie 100 ms après | banc NaN |
| U9 | Paramètres lissés, pas de clic à l'automation brutale | test du lot ou banc |
| U10 | Blocs 1 à 8192, 44,1 / 48 / 96 kHz | test du lot ou pluginval |
| U11 | Accents via `kit::u8`, aucun `\x..` suivi d'un caractère hexadécimal | grep |
| U12 | Anciens projets : état sans version rechargé à l'identique, **ou** différence écrite dans le CHANGELOG | code de `setStateInformation` |
| U13 | Niveau : pas de régression > ±3 dB sur les réglages par défaut | test du lot |
| U14 | CPU raisonnable (ordre de grandeur de la version précédente) | mesure de l'agent |
| U15 | Compilable sous MSVC : pas de `M_PI` sans `_USE_MATH_DEFINES`, VLA, `__attribute__`, `__builtin_*`, `ssize_t`, `alloca`, lambda à init-capture de `this` imbriquée | grep |
| U16 | Aucun code GPL copié (ChowTapeModel, compresseurs et filtres de chowdsp_utils interdits) | grep des en-têtes, provenance dans les commentaires |
| U17 | Ce qui est annoncé est **branché** dans un plugin, ou explicitement marqué « brique non branchée » | grep des `#include` |

## B. Critères propres aux lots

- **LOT-1 Autotune Orbe** : ancien projet sans `algo` → moteur Classique (son identique) ; édition note par note sauvegardée ; bémols ; fantôme B1 < −120 dB ; latences Direct/Qualité exactes ; les 8 points d'écoute de `tune-algo.md` § 10 restent à l'artiste.
- **LOT-1 Galet** : forme sauvegardée et nettoyée ; ADAA sans repliement audible ; latence exacte ; NaN en entrée récupéré (G2).
- **LOT-1 correctifs P0** : chaque correctif a un test de non-régression, ou il est marqué « non testé ».
- **LOT-2B Dsp + 808** : briques sans JUCE, sans allocation, testées ; aucun GPL ; 808 sans NaN et sans clic au retrig.
- **LOT-2A Splicer / Engine / OrbeChain** : décaleur, reverb FDN, ADT sans régression de niveau ; latence inchangée ou redéclarée ; **décision écrite** sur le changement de son des anciens projets (9 plugins).
- **Lots suivants** : un-potard (phase 0 de `03-un-potard.md`, NaN, latence 814/815) ; presets Looks (anciens index de preset inchangés) ; Accroc (critères U1–U17 complets, réutilisation de `Dsp/Compressors.h`) ; kit v3 / Voile / Autotune design (pas de changement d'état, pluginval **avec** tests d'interface) ; Relance (U1–U17).

## C. Verdicts

- **GO** : U1–U17 et critères du lot tenus ; seules restent des écoutes.
- **GO SOUS RÉSERVES** : aucun risque de perte de projet ni de plantage ; défauts listés avec correction et test à ajouter avant la sortie.
- **NO-GO** : plantage, échec pluginval, ID APVTS cassé, NaN persistant, latence fausse, code GPL, ou ancien projet qui change de son sans décision écrite.
