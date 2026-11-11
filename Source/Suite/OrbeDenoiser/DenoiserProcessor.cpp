#include "DenoiserProcessor.h"
#include "DenoiserEditor.h"

using kit::u8;
using namespace of;

juce::AudioProcessorValueTreeState::ParameterLayout DenoiserProcessor::makeLayout() {
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    const dn::Params d;
    l.add(fparam("red", u8("R\xC3\xA9" "duction"), 0.f, 24.f, 0.1f, d.reduction, [](float v) { return String(v, 1) + " dB"; }));
    l.add(fparam("sens", "Seuil", 0.f, 1.f, 0.01f, d.sens, fmtPct));
    l.add(fparam("smooth", "Lissage", 0.f, 1.f, 0.01f, d.smooth, fmtPct));
    l.add(fparam("air", "Aigus", 0.f, 1.f, 0.01f, d.air, fmtPct));
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{"hum", 1}, "Anti-ronflement", StringArray{u8("Coup\xC3\xA9"), "50 Hz", "60 Hz"}, 0));
    l.add(std::make_unique<AudioParameterBool>(ParameterID{"adapt", 1}, "Adaptatif", d.adaptive));
    l.add(std::make_unique<AudioParameterBool>(ParameterID{"listen", 1}, u8("\xC3\x89" "couter le bruit retir\xC3\xA9"), false));
    addBypass(l);
    // ajoutés en fin de liste (identifiants existants inchangés)
    l.add(fparam("gap", u8("Entre les phrases"), 0.f, 20.f, 0.1f, d.gapDb, [](float v) { return v < 0.05f ? String(u8("Coup\xC3\xA9")) : String(v, 1) + " dB"; }));
    l.add(fparam("blo", u8("R\xC3\xA9" "duction graves"), -12.f, 12.f, 0.1f, d.bandLo, fmtDb));
    l.add(fparam("bhi", u8("R\xC3\xA9" "duction aigus"), -12.f, 12.f, 0.1f, d.bandHi, fmtDb));
    l.add(std::make_unique<AudioParameterBool>(ParameterID{"live", 1}, u8("Mode Prise"), false, AudioParameterBoolAttributes().withAutomatable(false)));
    return l;
}

DenoiserProcessor::DenoiserProcessor() : FxProcessor("Orbe Denoiser", "ORBEDENOISER", makeLayout()) { apvts.addParameterListener("live", this); }
DenoiserProcessor::~DenoiserProcessor() { apvts.removeParameterListener("live", this); cancelPendingUpdate(); }

// Mode Prise : la latence change (2048 -> 512 échantillons). Le moteur et le contournement sont refaits sous suspendProcessing (le fil audio ne tourne pas),
// puis l'hôte est prévenu. Rien ne se passe avant le premier prepareToPlay.
void DenoiserProcessor::parameterChanged(const juce::String&, float) { triggerAsyncUpdate(); }
void DenoiserProcessor::handleAsyncUpdate() {
    const bool live = raw("live") > 0.5f;
    if (curSr <= 0.0 || live == engine.liveActive()) return;
    suspendProcessing(true);
    engine.prepare(curSr, live);
    prepareBypass(curSr, engine.latency(), maxBlock);
    setLatencySamples(engine.latency());
    suspendProcessing(false);
    updateHostDisplay(ChangeDetails().withLatencyChanged(true));
}

dn::Params DenoiserProcessor::readParams() const {
    dn::Params p;
    p.reduction = raw("red"); p.sens = raw("sens"); p.smooth = raw("smooth"); p.air = raw("air");
    p.hum = juce::roundToInt(raw("hum")); p.adaptive = raw("adapt") > 0.5f; p.listen = raw("listen") > 0.5f;
    p.gapDb = raw("gap"); p.bandLo = raw("blo"); p.bandHi = raw("bhi"); p.live = raw("live") > 0.5f;
    return p;
}

void DenoiserProcessor::prepareToPlay(double sr, int block) {
    maxBlock = juce::jmax(512, block);
    curSr = sr;
    engine.prepare(sr, raw("live") > 0.5f);
    prepareBypass(sr, engine.latency(), maxBlock);
    setLatencySamples(engine.latency());
}

void DenoiserProcessor::processBlock(juce::AudioBuffer<float>& b, juce::MidiBuffer&) {
    juce::ScopedNoDenormals nd;
    engine.set(readParams());
    stereo(b, maxBlock, [this](const float* il, const float* ir, float* ol, float* or_, int n) { engine.process(il, ir, ol, or_, n); });
}

// Le profil de bruit appris est enregistré avec la session (base64 des 1025 bins).
void DenoiserProcessor::beforeSave() {
    std::vector<float> v; engine.getProfile(v);
    if (engine.profileKnown()) {
        apvts.state.setProperty("profile", juce::MemoryBlock(v.data(), v.size() * sizeof(float)).toBase64Encoding(), nullptr);
        apvts.state.setProperty("profileSr", engine.profileRate(), nullptr);   // le profil est en bins : sa fréquence d'échantillonnage permet de le recaler
    } else { apvts.state.removeProperty("profile", nullptr); apvts.state.removeProperty("profileSr", nullptr); }
    apvts.state.setProperty("stateVersion", 2, nullptr);
}
void DenoiserProcessor::afterLoad() {
    juce::MemoryBlock mb;
    if (mb.fromBase64Encoding(apvts.state.getProperty("profile").toString()) && mb.getSize() == dn::kBins * sizeof(float)) {
        std::vector<float> v((size_t)dn::kBins); std::memcpy(v.data(), mb.getData(), mb.getSize());
        for (float& x : v) if (!std::isfinite(x) || x < 0.f) x = 0.f;
        engine.setProfile(v, (float)(double)apvts.state.getProperty("profileSr", 0.0));   // absente (projet ancien) : aucun recalage
    }
}

juce::AudioProcessorEditor* DenoiserProcessor::createEditor() { return new DenoiserEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new DenoiserProcessor(); }
