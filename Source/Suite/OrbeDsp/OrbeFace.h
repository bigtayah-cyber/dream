// Socle commun des effets Orbe (Denoiser, EQ, DeEsser) : processeur de base, composants de face avant « matériel de studio »
// et aides de tracé (grille fréquentielle log, courbes de spectre). Header-only ; dépend de JUCE et de Kit.h.
#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "../../Kit/Widgets.h"
#include "OrbeDsp.h"
#include "../../Kit/BypassFade.h"
#include "../../Kit/Sanitize.h"

namespace of {
using juce::Colour, juce::Rectangle, juce::String;
using kit::w::kLilac; using kit::w::kIce; using kit::w::kPeach;
// Effets Orbe : façade de rack graphite (kit commun).
inline kit::Palette palette(Colour accent = kLilac) { return kit::w::rack(accent); }

// ---------------------------------------------------------------- processeur de base
inline std::unique_ptr<juce::AudioParameterFloat> fparam(const char* id, const String& name, float lo, float hi, float step, float def,
                                                         std::function<String(float)> text, float centre = 0.f, std::function<float(const String&)> parse = nullptr) {
    juce::NormalisableRange<float> r(lo, hi, step);
    if (centre > 0.f) r.setSkewForCentre(centre);
    auto a = juce::AudioParameterFloatAttributes().withStringFromValueFunction([text](float v, int) { return text(v); });
    if (parse) a = a.withValueFromStringFunction([parse](const String& s) { return parse(s); });
    return std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{id, 1}, name, r, def, a);
}
inline String fmtHz(float v) { return v >= 1000.f ? String(v / 1000.f, v >= 10000.f ? 1 : 2) + " kHz" : String(juce::roundToInt(v)) + " Hz"; }
inline float parseHz(const String& s) {
    const double v = s.getDoubleValue(); const auto t = s.toLowerCase();
    return (float)((t.contains("k") ? v * 1000.0 : v));
}
inline String fmtDb(float v) { return (v > 0.04f ? "+" : "") + String(v, 1) + " dB"; }
inline String fmtPct(float v) { return String(juce::roundToInt(v * 100.f)) + " %"; }

class FxProcessor : public juce::AudioProcessor {
public:
    FxProcessor(const String& nm, const String& stateName, juce::AudioProcessorValueTreeState::ParameterLayout&& l)
        : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true).withOutput("Output", juce::AudioChannelSet::stereo(), true)),
          apvts(*this, nullptr, stateName, std::move(l)), pname(nm) {
        bypass = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter("bypass"));
    }
    static void addBypass(juce::AudioProcessorValueTreeState::ParameterLayout& l) { l.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"bypass", 1}, "Bypass", false)); }

    bool isBusesLayoutSupported(const BusesLayout& l) const override {
        return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() &&
               (l.getMainInputChannelSet() == juce::AudioChannelSet::mono() || l.getMainInputChannelSet() == juce::AudioChannelSet::stereo());
    }
    void releaseResources() override {}
    const String getName() const override { return pname; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    bool hasEditor() const override { return true; }
    juce::AudioProcessorParameter* getBypassParameter() const override { return bypass; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const String getProgramName(int) override { return {}; }
    void changeProgramName(int, const String&) override {}

    void processBlockBypassed(juce::AudioBuffer<float>& b, juce::MidiBuffer&) override {
        if (b.getNumChannels() < 2) return;
        byp.process(b.getWritePointer(0), b.getWritePointer(1), b.getNumSamples(), getTotalNumInputChannels() >= 2);
    }
    void getStateInformation(juce::MemoryBlock& dest) override {
        beforeSave();
        if (auto xml = apvts.copyState().createXml()) copyXmlToBinary(*xml, dest);
    }
    void setStateInformation(const void* data, int size) override {
        if (auto xml = getXmlFromBinary(data, size))
            if (xml->hasTagName(apvts.state.getType())) { apvts.replaceState(juce::ValueTree::fromXml(*xml)); afterLoad(); }
    }
    float raw(const char* id) const { return apvts.getRawParameterValue(id)->load(); }

    juce::AudioProcessorValueTreeState apvts;
protected:
    virtual void beforeSave() {}
    virtual void afterLoad() {}
    // Découpe un bloc en tranches de `maxBlock` et appelle f(inL, inR|nullptr, outL, outR, n) ; renvoie faux si la configuration est inutilisable.
    // Le bypass est honoré ici (le paramètre « bypass » est déclaré : JUCE n'appelle alors jamais processBlockBypassed) :
    // sortie = entrée retardée de la latence, fondu croisé de 10 ms ; le traitement continue en interne (queues et mémoires intactes).
    template <class F> bool stereo(juce::AudioBuffer<float>& b, int maxBlock, F f) {
        if (b.getNumChannels() < 2) return false;
        const bool st = getTotalNumInputChannels() >= 2;
        float* l = b.getWritePointer(0); float* r = b.getWritePointer(1);
        kit::sanitize(l, b.getNumSamples()); if (st) kit::sanitize(r, b.getNumSamples());   // NaN/inf d'un plugin précédent : remplacés par 0 avant tout état interne
        fader.run(l, r, b.getNumSamples(), st, bypass != nullptr && bypass->get(),
                  [&](float* cl, float* cr, int c) { for (int p = 0; p < c; p += maxBlock) { const int k = juce::jmin(maxBlock, c - p); f(cl + p, st ? cr + p : nullptr, cl + p, cr + p, k); } });
        return true;
    }
    // À appeler dans prepareToPlay à la place de byp.init : latence déclarée + tranche maximale.
    void prepareBypass(double sampleRate, int latency, int maxBlock) { byp.init(latency); fader.prepare(sampleRate, latency, maxBlock); }
    odsp::BypassDelay byp;
    bypassfx::Fader fader;
    juce::AudioParameterBool* bypass = nullptr;
private:
    String pname;
};

// ---------------------------------------------------------------- composants de face avant (kit commun)
using kit::w::Pot; using kit::w::Power; using kit::w::Key; using kit::w::Seg; using kit::w::section;

// ---------------------------------------------------------------- aides de tracé
namespace gfx {
inline float xOf(float f, Rectangle<float> r, float f0 = 20.f, float f1 = 20000.f) { return r.getX() + r.getWidth() * std::log(f / f0) / std::log(f1 / f0); }
inline float fOf(float x, Rectangle<float> r, float f0 = 20.f, float f1 = 20000.f) { return f0 * std::exp((x - r.getX()) / r.getWidth() * std::log(f1 / f0)); }
inline float yOfDb(float d, Rectangle<float> r, float top, float bot) { return r.getBottom() - r.getHeight() * (d - bot) / (top - bot); }

inline String hzLabel(float f) { return f >= 1000.f ? String(f / 1000.f, f == 1500.f ? 1 : 0) + "k" : String((int)f); }
// Repères fréquentiels log : lignes fines + repères gravés
inline void freqGrid(juce::Graphics& g, Rectangle<float> r, float u, float f0 = 20.f, float f1 = 20000.f, std::vector<float> marks = {20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000}, bool labels = true) {
    for (float dec = 10.f; dec <= 10000.f; dec *= 10.f)
        for (int m = 1; m < 10; ++m) {
            const float f = dec * (float)m; if (f < f0 || f > f1) continue;
            const bool major = std::find(marks.begin(), marks.end(), f) != marks.end();
            g.setColour(juce::Colours::white.withAlpha(major ? 0.10f : 0.035f));
            g.drawVerticalLine(juce::roundToInt(xOf(f, r, f0, f1)), r.getY(), r.getBottom());
        }
    if (!labels) return;
    g.setFont(kit::mono(7.6f * u));
    for (float f : marks) {
        if (f < f0 || f > f1) continue;
        const float x = xOf(f, r, f0, f1);
        g.setColour(Colour(0xFFE9E6F5).withAlpha(0.5f));
        g.drawText(hzLabel(f), Rectangle<float>(x - 14 * u, r.getBottom() + 2 * u, 28 * u, 10 * u), juce::Justification::centred);
    }
}
// Courbe de spectre (dB par bin) sur un axe log ; maximum des bins par colonne, interpolation quand un bin couvre plusieurs colonnes.
inline juce::Path specPath(const float* db, int bins, float sr, Rectangle<float> r, float topDb, float botDb, bool closed, float f0 = 20.f, float f1 = 20000.f) {
    juce::Path p; const float binHz = sr / (2.f * (float)(bins - 1));
    bool first = true;
    for (float x = r.getX(); x <= r.getRight(); x += 1.f) {
        const float fa = fOf(x - 0.5f, r, f0, f1), fb = fOf(x + 0.5f, r, f0, f1);
        float v;
        const int ia = (int)std::floor(fa / binHz), ib = (int)std::floor(fb / binHz);
        if (ib <= ia) { const float t = fa / binHz - (float)ia; const int i0 = juce::jlimit(0, bins - 1, ia), i1 = juce::jlimit(0, bins - 1, ia + 1); v = db[i0] + (db[i1] - db[i0]) * t; }
        else { v = -200.f; for (int i = juce::jmax(0, ia + 1); i <= juce::jmin(bins - 1, ib); ++i) v = juce::jmax(v, db[i]); }
        const float y = juce::jlimit(r.getY(), r.getBottom(), yOfDb(v, r, topDb, botDb));
        if (first) { if (closed) { p.startNewSubPath(x, r.getBottom()); p.lineTo(x, y); } else p.startNewSubPath(x, y); first = false; }
        else p.lineTo(x, y);
    }
    if (closed) { p.lineTo(r.getRight(), r.getBottom()); p.closeSubPath(); }
    return p;
}
}  // namespace gfx
}  // namespace of
