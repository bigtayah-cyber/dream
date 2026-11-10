// Orbe Chain : couche JUCE commune (hôte audio + composants de face avant). Identité Orbe : graphite, lilas, glace, pêche.
#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>
#include "../../Kit/Widgets.h"
#include "Looks.h"
#include "../../Kit/BypassFade.h"
#include "../../Kit/Sanitize.h"

namespace oc {

// Pont entre l'hôte et le moteur : latence, bypass compensé, compteurs pour l'interface.
struct Host {
    Chain chain; BypassDelay byp; bypassfx::Fader fader;
    std::atomic<float> peak{0.f}, level{0.f}, reduction{0.f}, shift{0.f};
    std::atomic<int> keyPc{-1}, keyScale{1};
    int prepare(double sr, int block) { chain.prepare(sr); byp.init(chain.latency()); fader.prepare(sr, chain.latency(), juce::jmax(4096, block)); return chain.latency(); }
    void bypassed(juce::AudioBuffer<float>& b, bool stereoIn) { if (b.getNumChannels() >= 2) byp.process(b.getWritePointer(0), b.getWritePointer(1), b.getNumSamples(), stereoIn); }
    // bypass : sortie = entrée retardée de la latence, fondu de 10 ms ; la chaîne continue de tourner (queues de reverb/delay intactes).
    void run(juce::AudioBuffer<float>& b, juce::AudioPlayHead* ph, const S& s, bool stereoIn, bool autoLevel, bool bypass = false) {
        if (b.getNumChannels() < 2) return;
        if (ph) if (auto pos = ph->getPosition()) if (auto bpm = pos->getBpm()) chain.setBpm(*bpm);
        chain.setAutoLevel(autoLevel); chain.set(s);
        float* l = b.getWritePointer(0); float* r = b.getWritePointer(1);
        kit::sanitize(l, b.getNumSamples()); if (stereoIn) kit::sanitize(r, b.getNumSamples());   // NaN/inf d'un plugin précédent : remplacés par 0
        fader.run(l, r, b.getNumSamples(), stereoIn, bypass, [&](float* cl, float* cr, int c) { chain.process(cl, stereoIn ? cr : nullptr, cl, cr, c); });
        peak.store(chain.outPeak()); level.store(std::sqrt(chain.outMeanSquare())); reduction.store(chain.gainReduction()); shift.store(chain.shiftSemis());
        keyPc.store(chain.keyKnown() ? chain.detectedKey() : -1); keyScale.store(chain.detectedScale());
    }
};

namespace ui {
using juce::Colour;
using kit::w::kLilac; using kit::w::kIce; using kit::w::kPeach; using kit::w::kNotes;
using kit::w::ResetSlider; using kit::w::Pot; using kit::w::Power; using kit::w::ScaledLook; using kit::w::bar;
// Effets Orbe Chain (Looks, Atelier) : façade de rack graphite (kit commun).
inline kit::Palette palette(Colour accent = kLilac) { return kit::w::rack(accent); }
}  // namespace ui
}  // namespace oc
