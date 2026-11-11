# Orbe Autotune : recherche web, sources clés (audit 4)

*10 nov. 2026. Version courte : la recherche détaillée (algorithmes, licences, pseudo-code ; son cible et chaînes) est dans
`04-algos-pitch-formants.md` et `05-son-cible-et-chaines.md`. Ici : ce que j'ai lu moi-même et ce qui pèse sur le plan.
Les citations sont des résumés de pages lues le 10 nov. ; rien n'a été écouté.*

## 1. MetaTune (Slate Digital) : ce qu'il fait vraiment

Source primaire : [manuel MetaTune](https://docs.slatedigital.com/MetaTune/MetaTune.html) ; revue :
[Sound On Sound, J. Walden, déc. 2021](https://www.soundonsound.com/reviews/slate-digital-metatune) ;
[guide Slate « tuning without the robot voice »](https://slatedigital.com/vocal-tuning-without-the-robot-voice-a-producers-guide/) ;
[MusicRadar](https://www.musicradar.com/news/slate-digital-metatune).

- **Speed** en ms : ~200 à 20 naturel, 20 à 0 robot, **0 à −3 ms « plus dur que les autres tuners »**. SOS : « doesn't involve
  lookahead ». Le mécanisme de la vitesse négative n'est **pas** documenté (hypothèse : dépassement / anticipation de la cible).
- **Amount** 0-100 %. **Sustain** : la vitesse dépend de la durée de la note tenue ; **négatif = favorise le hard-tune et garde
  le vibrato** (SOS). **Note Stabilizer** : ignore les notes plus courtes que 40 / 80 / 200 ms (anti-« flutter »), **+280 ms de
  latence**, désactivé en Low Latency.
- **Low Latency** ≈ 3 ms mesuré par SOS. **Doubler** : 2 voix L/R, Width et Wet/Dry (50 % = 3 voix égales). **Groups** (4),
  HeatMaps entrée / sortie, générateur de ton, A/B, référence A440 ± 1 dt. Formants corrigés par défaut (guide Slate).
- **Ce que MetaTune n'a pas** (d'après le manuel) : pas d'harmoniseur (c'est MetaPitch), pas de détection de tonalité
  automatique, pas d'effets de couleur. **Sa signature tient donc au moteur et à ses 4 réglages de comportement**, pas à une
  chaîne d'effets.

## 2. Auto-Tune (Antares) et autres

- [Manuel Auto-Tune Realtime Advanced (UA)](https://media.uaudio.com/support/manuals/dd/Auto-Tune%20Realtime%20Advanced%20Manual.pdf) :
  Retune Speed 0 = changement immédiat ; **Humanize** = vitesse plus lente sur la partie tenue des notes longues ; **Flex-Tune**
  = ne corrige qu'à l'approche de la note ; **Natural Vibrato** ± ; **Classic Mode** = son Auto-Tune 5, « un peu plus brillant,
  attaque et transitions plus marquées aux vitesses rapides » ; **Tracking** (souplesse de la détection, voix soufflées) ;
  **Input Type** (Soprano, Alto/Tenor, Low Male, Instrument) ; **Targeting ignores vibrato**.
- [SOS, Auto-Tune Pro](https://www.soundonsound.com/reviews/antares-auto-tune-pro) : Classic = algorithme d'Auto-Tune 5
  réintroduit, « celui qui a été détourné sur les tubes » ; Auto-Key envoie la tonalité à toutes les instances.
- [Brevet US 5 973 252 (Hildebrand)](https://patents.google.com/patent/US5973252) : priorité 27/10/1997, **expiré (oct. 2018)**.
  Détection par autocorrélation décimée ×8 + vérification plein débit, suivi par fenêtre de retards autour de la période
  précédente, interpolation quadratique ; **correction par rééchantillonnage, en répétant ou supprimant une période entière**.
  Confirmé par Hildebrand cité par [Valhalla DSP](https://valhalladsp.com/?p=1559) (« pas de PSOLA, trop imprécis »). C'est
  la famille d'Orbe (raccords synchrones) : nous sommes sur la bonne famille d'algorithmes.
- [Waves Tune Real-Time (guide)](https://josephnilo.com/blog/waves-tune-real-time-explained/) : **Speed et Note Transition
  séparés** (15 ms / 120 ms par défaut), Tolerance Cents et **Tolerance Time** (0-300 ms), formants Corrected / Non, cible MIDI.
- [Little AlterBoy (PLAP)](https://producelikeapro.com/blog/little-alterboy-review/) : Transpose / Quantize / Robot (note fixe ou
  MIDI), Formant, **Drive** (saturation type Decapitator), Mix.
- [Graillon 3 (Auburn Sounds)](https://www.auburnsounds.com/blog/Introducing-Graillon-3.html) : 3 moteurs, formants, **Snap Min**
  (garder du vibrato), 7 effets « un bouton » (compresseur, gate, préampli, chorus, doubleur…).
- [Melodyne 5, centre / modulation / dérive](https://helpcenter.celemony.com/M5/doc/melodyneStudio5/en/M5tour_ToolModulationDrift_2) :
  la hauteur d'une note = centre + modulation (vibrato) + dérive (lente), chacune réglable de −100 à +100 %. Orbe a déjà centre
  + expression ; il manque la **dérive** séparée et le réglage **inversé / amplifié**.

## 3. Le « son » trap / rap (ce que disent les sources)

- [Antares, trap & rap](https://www.antarestech.com/blog/auto-tune-for-rap-hip-hop-how-trap-producers-use-pitch-correction) :
  Travis Scott 0-5 ms, Flex-Tune off, delay 1/4 + 1/8 pointée, reverb longue sombre ; Future 15-20 ms (« warble »), compression
  lente, saturation douce ; ad-libs plus durs que le lead ; doubles à vitesse décalée de 3-5 ms. (Source éditeur, à prendre comme
  recettes, pas comme vérité.)
- [Popular Music, « Auto-Tune as instrument »](https://core-prod.cambridgecore.org/core/services/aop-cambridge-core/content/view/C5B160BE7E8AB29576E92CC7CD9D60B7/S0261143024000369a.pdf/auto-tune-as-instrument-trap-musics-embrace-of-a-repurposed-technology.pdf) :
  marqueurs audibles = **paliers** (glissés en escalier), **warble** quand le logiciel hésite entre deux notes, **timbre « droit »**
  sur les notes tenues, **mélismes numériques**, fausses notes assumées, texture « pixelisée » (en partie d'autres traitements).
  Les artistes enregistrent **avec** l'Auto-Tune dans le casque : la latence et l'attaque comptent.

## 4. Algorithmes et licences (résumé ; détail dans 04)

- Enveloppe : [Röbel & Rodet, « true envelope », DAFx-05](https://dafx.de/paper-archive/details/EDSTSaO17Gd_0LV03iwDfw) (plus de
  contrôle que la LPC, coût un peu supérieur). Phase vocoder verrouillé : [Laroche & Dolson 1999](https://www.ee.columbia.edu/~dpwe/papers/LaroD99-pvoc.pdf).
- Suivi : [pYIN, Mauch & Dixon 2014](https://core.ac.uk/download/30696897.pdf) (Orbe en reprend l'esprit : candidats + Viterbi).
- [Signalsmith Stretch](https://gh.r-mj.com/Signalsmith-Audio/signalsmith-stretch) : **MIT**, spectral, formants moins nets
  qu'un PSOLA (dit l'auteur) : utilisable pour harmonies / transpositions larges, pas pour le cœur hard-tune.
- [Rubber Band](https://breakfastquay.com/rubberband/why.html) : **GPL ou licence commerciale payante** → exclu sans achat.
- [PESTO (Sony CSL)](https://github.com/SonyCSLParis/pesto) : suivi neuronal léger (~0,7 ms/inférence ONNX), **LGPL-3.0** →
  piège pour un plugin lié statiquement ; ne reprendre que l'idée (papier ISMIR 2023), réentraîner soi-même si besoin.
