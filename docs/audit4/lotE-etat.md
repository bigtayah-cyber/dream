# Lot E · Studio et instruments : état

*11 novembre 2026, branche `vague2-wip`. Hors périmètre, non touchés : Orbe Autotune, les plugins voix (Voile, Looks, Relance, Denoiser, DeEsser, Atelier, Fredonne, Détect), Orbe et Orbe FX. Lot A (crash Pads/Sample, limiteur de Master, pic vrai de Mix) non refait.*

## Version d'état 3

`kit::kStateVersionDsp = 3` (`Source/Kit/StateVersion.h`) ; `kStateVersion` reste 2 (le test `state_version_test` l'exige). Synth, 808, Sample et Cabine écrivent 3 ; un état d'avant (absent ou 2) garde l'ancien son. Pour Synth, 808 et Sample, `loadedVersion` est lu **avant** `replaceState`. Pour Cabine, les deux nouveaux réglages sont remis à 0 au chargement d'un état d'avant. Master, EQ, Galet et Accroc : nouveaux paramètres **neutres par défaut**, donc aucune version nécessaire. Tous les identifiants existants sont inchangés ; les nouveaux sont ajoutés en fin.

## Mesures avant / après

| Plugin | Sujet | Avant | Après |
|---|---|---|---|
| Synth | repliement, scie/carré/triangle de A4 à E7, pire cas | −44,1 dB | −85 dB (BLEP/BLAMP à table, 16 points) |
| Synth | unisson 7 voix, facteur de crête | 3,74 | 2,05 (phases aléatoires, voix 0 à phase 0) |
| Synth | CPU, 16 voix × 7 unisson × 2 oscillateurs | 8,5 % d'un cœur | 11,4 % |
| Synth | ajouts | — | filtre 2 (ZDF, Off/LP/HP/BP), 2 LFO, glide, mono/legato, bend, détune étendu ; défauts identiques à l'ancien moteur (dumps égaux au bit) |
| 808 | relance : trou entre deux notes (retrig) | 3,3 ms | 0,0 ms (fondu croisé de 2,5 ms, deux chaînes) |
| 808 | mode « Pliée » (ex « Repliée »), erreur contre rendu 192 kHz | −24,9 dB | −44,7 dB (k réduit de moitié, état v3 seulement) |
| Sample | étirement : crête d'attaque | −12 … −1,7 dB | 0,04 dB (WSOLA à transitoires préservées) |
| Sample | pré-écho d'une salve | −11,5 dB | −180 dB |
| Beat, Pads | sorties séparées | aucune | 8 pistes / 16 pads (bus auxiliaires désactivés par défaut, paramètre « outs » : mix seul / mix + sorties / sorties seules) ; le mix reste identique au bit |
| Cabine | de-esser relatif en tête (réglage « Sifflantes ») | aucun | sifflante −7,3 dB, voyelle −0,0 dB (réglage 0,8) ; à 0 : sortie identique au bit |
| Cabine | gain d'entrée | aucun | ±24 dB, exact (+6,00 dB mesuré) |
| Cabine | presets | « Beat » = « Brut » | tous distincts (test sur les 11) ; « Beat » devient « Voix sur beat chargé » |
| Galet | formes | une pour 4 modules | une par module, ou liées (défaut) ; séparées égales = liées (écart 0) |
| Accroc | traîne d'écho après l'accident | aucune | écho synchro 1/8, 3/16, 1/4 en ping-pong, alimenté par l'accident seul ; −33 dB à 0,5 s, −75 dB à 3,5 s |
| Master | cible LUFS (−14, −11, −9) | aucune | −13,32 LUFS pour −14 demandés après 45 s (servo lent, 1 dB/s au plus) |
| Master | écoute à niveau égal | aucune | sortie entendue −17,30 LUFS pour une entrée à −17,30 (sans amplification) |
| Master | Delta | aucun | chaîne neutre : résidu nul |
| EQ | pentes coupe-bas / coupe-haut | 12 dB/oct | 12, 24, 48 dB/oct ; −49,2 dB à une octave (48), mesure = courbe affichée |

Tests : `orbesynth_test` (11 tests), `test_orbe808`, `test_orbebeat`, `sampler_test`, `cabine_test`, `galet_test`, `accroc_test`, `master_test`, `eq_test`, `state_version_test` passent.

## Correction de diagnostic (808)

La mesure d'« aliasing » de l'audit (énergie totale hors harmoniques) comptait les jupes de l'enveloppe, pas du repliement. Remplacée par l'erreur contre un rendu à 192 kHz, décimé et aligné au retard fractionnaire. C'est elle qui donne −24,9 puis −44,7 dB.

## Reste à faire

- **Écoute** : rien de ce lot n'a été écouté dans FL Studio. Le comportement des sorties auxiliaires (Beat, Pads) dans FL Studio n'est pas vérifié ; AU non testé.
- **Synth** : filtre non linéaire, LFO calé sur le tempo, Y5.
- **Pads** : pas d'étirement (l'artefact concerne Sample seulement).
- **Cabine** : compresseur Opto/FET, ordre couleur/compresseur, « Écouter 5 s ». Les deux nouveaux potards n'existent que dans la face v3.
- **Galet** : déclenchement audio, emplacements de formes, module Temps ; le sélecteur « Séparé » n'existe que dans la face v3.
- **Accroc** : rampes par accident, probabilité par pas, gestes ; potards de traîne dans la face v3 seulement.
- **EQ** : solo de bande, auto-gain, bandes dynamiques, M/S.
- **Master** : relâche adaptative du limiteur, écrêteur doux ; interface de la cible et de l'écoute non vérifiée visuellement (compilée seulement).
- **Mix** : non traité (le pic vrai est dans le lot A) ; référence déposée, historique LUFS, facteur de crête des graves restent à faire.
