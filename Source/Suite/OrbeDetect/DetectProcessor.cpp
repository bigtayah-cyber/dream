#include "DetectProcessor.h"
#include "DetectEditor.h"

using juce::String;

juce::AudioProcessorValueTreeState::ParameterLayout DetectProcessor::makeLayout() {
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    l.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"bypass", 1}, "Bypass", false));
    l.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"capture", 1}, "Capture", true));
    return l;
}

DetectProcessor::DetectProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true).withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "ORBEDETECT", makeLayout()) {
    bypass = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter("bypass"));
    capParam = apvts.getRawParameterValue("capture");
    peaks.reset(new std::atomic<float>[kPeaks]);
    for (int i = 0; i < kPeaks; ++i) peaks[(size_t)i].store(0.f);
    res = std::make_shared<const DetectResult>();
}

DetectProcessor::~DetectProcessor() { worker.stopThread(20000); }

bool DetectProcessor::isBusesLayoutSupported(const BusesLayout& l) const {
    const auto o = l.getMainOutputChannelSet();
    return o == l.getMainInputChannelSet() && (o == juce::AudioChannelSet::mono() || o == juce::AudioChannelSet::stereo());
}

void DetectProcessor::prepareToPlay(double rate, int) {
    const int want = (int)(rate * kMaxSec);
    if (rate != sr || (int)cap.size() != want) {
        worker.stopThread(20000);
        sr = rate; capMax = want; cap.assign((size_t)want, 0.f); capLen = 0; peakIdx = 0; peakCnt = 0; peakAcc = 0.f; spp = juce::jmax(1, (int)(rate / 20.0));
    }
    clearReq = false;
}

void DetectProcessor::getHostBpm() {
    if (auto* ph = getPlayHead()) if (auto pos = ph->getPosition()) if (auto b = pos->getBpm()) hostBpm.store((float)*b);
}

void DetectProcessor::processBlock(juce::AudioBuffer<float>& buf, juce::MidiBuffer&) {
    juce::ScopedNoDenormals nd;
    getHostBpm();
    const int n = buf.getNumSamples(), nch = buf.getNumChannels();
    if (nch < 1 || n < 1) return;
    if (bypass != nullptr && bypass->get()) return;   // contourné : l'audio passe tel quel (latence nulle), plus de capture
    if (clearReq.exchange(false)) { capLen.store(0); peakIdx.store(0); peakCnt = 0; peakAcc = 0.f; }
    const float* L = buf.getReadPointer(0);
    const float* R = buf.getReadPointer(nch > 1 ? 1 : 0);
    float pk = 0.f;
    for (int i = 0; i < n; ++i) pk = juce::jmax(pk, std::abs(L[i]), std::abs(R[i]));
    level.store(juce::jmax(pk, level.load() * 0.92f));
    int len = capLen.load(std::memory_order_relaxed);
    if (capParam->load() < 0.5f || pk < 1e-4f || len >= capMax) return;
    const int k = juce::jmin(n, capMax - len);
    int pi = peakIdx.load(std::memory_order_relaxed);
    for (int i = 0; i < k; ++i) {
        const float m = (L[i] + R[i]) * 0.5f;
        cap[(size_t)(len + i)] = m;
        peakAcc = juce::jmax(peakAcc, std::abs(m));
        if (++peakCnt >= spp) { if (pi < kPeaks) peaks[(size_t)pi++].store(peakAcc, std::memory_order_relaxed); peakCnt = 0; peakAcc = 0.f; }
    }
    peakIdx.store(pi, std::memory_order_release);
    capLen.store(len + k, std::memory_order_release);
}

void DetectProcessor::clearCapture() { if (!busy()) clearReq = true; }

void DetectProcessor::analyze() {
    if (busy() || clearReq.load()) return;
    const int len = capLen.load(std::memory_order_acquire);
    if (len < (int)(sr * kMinSec)) return;
    work = std::make_shared<std::vector<float>>(cap.begin(), cap.begin() + len);
    workSr = sr; working = true;
    worker.startThread();
}

void DetectProcessor::Worker::run() {
    auto data = owner.work; const double rate = owner.workSr; const int n = (int)data->size();
    DetectResult r; r.dur = n / rate;
    const auto t = oa::detectTempo(data->data(), n, rate);
    if (!threadShouldExit()) {
        const auto k = oa::detectKey(data->data(), n, rate);
        r.valid = t.bpm > 0; r.bpm = r.bpm0 = t.bpm; r.bconf = t.conf; r.nAlt = t.nAlt; r.alt[0] = t.alt[0]; r.alt[1] = t.alt[1];
        r.pc = k.pc; r.minor = k.minor; r.kconf = k.conf;
        for (int i = 0; i < 12; ++i) r.chroma[i] = k.chroma[i];
        for (int i = 0; i < 4; ++i) { r.rankPc[i] = k.rank[i].pc; r.rankMinor[i] = k.rank[i].minor; }
        owner.setResult(r);
    }
    owner.working = false;
}

void DetectProcessor::setBpm(double v) {
    auto r = *result(); if (!r.valid) return;
    r.bpm = juce::jlimit(20.0, 400.0, std::round(v * 10.0) / 10.0); r.nAlt = 0;
    for (double f : {0.5, 2.0}) if (r.bpm * f >= 50 && r.bpm * f <= 220) r.alt[r.nAlt++] = std::round(r.bpm * f * 10.0) / 10.0;
    setResult(r);
}
void DetectProcessor::scaleBpm(double f) { const auto r = result(); if (r->valid) setBpm(r->bpm * f); }

void DetectProcessor::getStateInformation(juce::MemoryBlock& dest) {
    auto s = apvts.copyState();
    const auto r = result();
    if (r->valid) s.setProperty("res", juce::Base64::toBase64(r.get(), sizeof(DetectResult)), nullptr);
    if (auto xml = s.createXml()) copyXmlToBinary(*xml, dest);
}
void DetectProcessor::setStateInformation(const void* data, int size) {
    if (auto xml = getXmlFromBinary(data, size)) {
        auto s = juce::ValueTree::fromXml(*xml);
        if (!s.isValid() || !s.hasType(apvts.state.getType())) return;
        const String b64 = s.getProperty("res").toString();
        if (b64.isNotEmpty()) {
            juce::MemoryOutputStream mo; juce::MemoryBlock mb;
            if (juce::Base64::convertFromBase64(mo, b64) && mo.getDataSize() == sizeof(DetectResult)) {
                DetectResult r; std::memcpy(&r, mo.getData(), sizeof r); setResult(r);
            }
        }
        s.removeProperty("res", nullptr);
        apvts.replaceState(s);
    }
}

juce::AudioProcessorEditor* DetectProcessor::createEditor() { return new DetectEditor(*this); }
#ifndef ORBE_SUITE_SNAPSHOT
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new DetectProcessor(); }
#endif
