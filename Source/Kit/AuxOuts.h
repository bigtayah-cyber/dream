// Sorties auxiliaires optionnelles (Beat : une par piste ; Pads : une par pad). Le bus principal reste seul activé par défaut : un projet
// d'avant se rouvre en stéréo simple, et l'hôte active les sorties qu'il veut. Aucun identifiant de paramètre n'est touché ; le paramètre
// « outs » (ajouté en fin de liste) règle ce que contient le bus principal : 0 mix seul (aucune sortie auxiliaire écrite), 1 mix + sorties, 2 sorties seules.
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace kit::aux {

// Ajoute n bus stéréo désactivés par défaut, nommés « prefix 1 »…
template <class Props>
inline Props withAux(Props p, int n, const juce::String& prefix) {
    for (int i = 0; i < n; ++i) p = p.withOutput(prefix + " " + juce::String(i + 1), juce::AudioChannelSet::stereo(), false);
    return p;
}
// Bus principal stéréo ; chaque bus auxiliaire est soit désactivé, soit stéréo.
inline bool layoutOk(const juce::AudioProcessor::BusesLayout& l) {
    if (l.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    for (int i = 1; i < l.outputBuses.size(); ++i) if (!l.outputBuses[i].isDisabled() && l.outputBuses[i] != juce::AudioChannelSet::stereo()) return false;
    return true;
}
// Pour chaque bus auxiliaire i (0..n-1) : pointeurs gauche/droite dans `buffer`, ou nullptr si le bus n'est pas activé.
struct Taps {
    float* l[32] = {}; float* r[32] = {};
    int n = 0; bool any = false;
};
inline Taps taps(juce::AudioProcessor& proc, juce::AudioBuffer<float>& buffer, int nAux) {
    Taps t; t.n = std::min(nAux, 32);
    for (int i = 0; i < t.n; ++i) {
        auto* bus = proc.getBus(false, 1 + i);
        if (bus == nullptr || !bus->isEnabled() || bus->getNumberOfChannels() < 2) continue;
        const int c0 = bus->getChannelIndexInProcessBlockBuffer(0);
        if (c0 < 0 || c0 + 1 >= buffer.getNumChannels()) continue;
        buffer.clear(c0, 0, buffer.getNumSamples()); buffer.clear(c0 + 1, 0, buffer.getNumSamples());
        t.l[i] = buffer.getWritePointer(c0); t.r[i] = buffer.getWritePointer(c0 + 1); t.any = true;
    }
    return t;
}

}  // namespace kit::aux
