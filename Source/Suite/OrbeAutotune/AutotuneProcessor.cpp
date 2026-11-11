#include "AutotuneProcessor.h"
#include "AutotuneEditor.h"
#include "../../Kit/Kit.h"

using kit::u8;
using juce::String;

juce::AudioProcessorValueTreeState::ParameterLayout AutotuneProcessor::makeLayout() {
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    const atn::Params d;
    auto flt = [&](const char* id, const char* name, float lo, float hi, float step, float def, std::function<String(float)> fmt, float centre = 0.f) {
        NormalisableRange<float> r(lo, hi, step); if (centre > 0.f) r.setSkewForCentre(centre);
        l.add(std::make_unique<AudioParameterFloat>(ParameterID{id, 1}, u8(name), r, def, AudioParameterFloatAttributes().withStringFromValueFunction([fmt](float v, int) { return fmt(v); })));
    };
    auto pct = [](float v) { return String(roundToInt(v * 100.f)) + " %"; };
    auto sgn = [](float v, int dec) { return (v > 0.f ? "+" : "") + String(v, dec); };
    flt("speed", "Vitesse", 0.f, 400.f, 1.f, d.speed, [](float v) { return v < 0.5f ? u8("0 \xC2\xB7 robot") : String(roundToInt(v)) + " ms"; }, 60.f);
    flt("tol", "Tolérance", 0.f, 50.f, 1.f, d.tol, [](float v) { return String(roundToInt(v)) + " ct"; });
    flt("amt", "Quantité", 0.f, 1.f, 0.01f, d.amt, pct);
    flt("trans", "Transposer", -12.f, 12.f, 1.f, d.trans, [sgn](float v) { return sgn(v, 0) + " dt"; });
    flt("form", "Formants", -6.f, 6.f, 0.5f, d.form, [sgn](float v) { return sgn(v, 1) + " dt"; });
    l.add(std::make_unique<AudioParameterBool>(ParameterID{"formPres", 1}, u8("Formants préservés"), d.formPres));
    flt("vd", "Vibrato", 0.f, 60.f, 1.f, d.vd, [](float v) { return v < 0.5f ? String("off") : String(roundToInt(v)) + " ct"; });
    flt("vr", "Vitesse du vibrato", 3.f, 8.f, 0.1f, d.vr, [](float v) { return String(v, 1) + " Hz"; });
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{"harm", 1}, "Harmonie", StringArray{u8("Aucune"), u8("Tierce au-dessus"), "Quinte", u8("Tierce en dessous"), u8("Octave au-dessus"), u8("Octave en dessous")}, d.harm));
    flt("hv", "Volume d'harmonie", 0.f, 1.f, 0.01f, d.hv, pct);
    flt("dbl", "Doublage", 0.f, 1.f, 0.01f, 0.f, [](float v) { return v < 0.005f ? String("off") : String(roundToInt(v * 100.f)) + " %"; });
    flt("mix", "Mix", 0.f, 1.f, 0.01f, d.mix, pct);
    StringArray keys{"Auto"}; for (int i = 0; i < 12; ++i) keys.add(atn::noteNames()[i]);
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{"key", 1}, u8("Tonalité"), keys, 0));
    StringArray sc; for (int i = 0; i < atn::kScales; ++i) sc.add(atn::scales()[i].name);
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{"scale", 1}, "Gamme", sc, 1));
    for (int i = 0; i < 12; ++i) l.add(std::make_unique<AudioParameterChoice>(ParameterID{"x" + String(i), 1}, u8("Note ") + atn::noteNames()[i] + u8(" inversée"), StringArray{"Non", "Oui"}, 0));   // choix à 2 valeurs : valeur toujours calée sur 0/1 (restauration d'état fiable)
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{"latmode", 1}, u8("Latence"), StringArray{"Direct", u8("Qualité")}, 0));   // Direct : <= 256 éch. ; Qualité : formants, latence d'origine
    l.add(std::make_unique<AudioParameterBool>(ParameterID{"bypass", 1}, "Bypass", false));
    // ---- ajoutés en fin (les IDs précédents ne bougent pas) ; un ancien projet sans « algo » est rechargé en Classique
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{"algo", 1}, u8("Moteur"), StringArray{"Classique", "Orbe"}, 1));
    flt("expr", "Expression", 0.f, 1.f, 0.01f, d.expr, pct);
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{"accid", 1}, u8("Altérations"), StringArray{u8("Dièses (#)"), u8("Bémols (b)")}, 0));
    // ---- rendu (ajouté en fin) : Précis par défaut pour une nouvelle instance (le hard-tune s'entend d'emblée) ; un état sans « rendu » est rechargé en Précis (son d'origine)
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{"rendu", 1}, u8("Rendu"), StringArray{"Satin", u8("Précis"), "Robot"}, 1));
    flt("glide", "Glissement", 20.f, 60.f, 1.f, d.glide, [](float v) { return String(roundToInt(v)) + " ms"; });
    // ---- comportement (P1/P2), ajouté en fin ; un état sans « atV » est rechargé avec les valeurs neutres (son d'origine)
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{"stab", 1}, u8("Stabilisateur"), StringArray{"Off", "40 ms", "80 ms", "200 ms"}, 0));
    flt("warb", "Warble", 0.f, 1.f, 0.01f, d.warb, pct);
    flt("human", "Humanize", 0.f, 1.f, 0.01f, d.human, [](float v) { return v < 0.005f ? String("off") : String(roundToInt(v * 100.f)) + " %"; });
    flt("tspd", "Transition", 0.f, 400.f, 1.f, d.tspd, [](float v) { return v < 0.5f ? String("= vitesse") : String(roundToInt(v)) + " ms"; }, 60.f);
    flt("ovs", "Vitesse < 0", 0.f, 1.f, 0.01f, d.ovs, [](float v) { return v < 0.005f ? String("off") : String(roundToInt(v * 100.f)) + " %"; });
    flt("track", "Suivi", 0.f, 1.f, 0.01f, d.track, [](float v) { return v < 0.34f ? u8("serré") : v < 0.67f ? String("moyen") : u8("souple"); });
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{"attack", 1}, u8("Attaque accordée"), StringArray{"Non", "Oui"}, d.attack ? 1 : 0));
    return l;
}

AutotuneProcessor::AutotuneProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true).withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "ORBEAUTOTUNE", makeLayout()) {
    bypass = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter("bypass"));
    initRaw();
    hist.push_back({}); histPos = 0;
}

// G4 : pointeurs de paramètres préchargés (aucune chaîne construite dans le fil audio).
void AutotuneProcessor::initRaw() {
    static const char* ids[rNum] = {"speed", "tol", "amt", "trans", "form", "formPres", "vd", "vr", "harm", "hv", "dbl", "mix", "latmode", "key", "scale", "algo", "expr", "rendu", "glide", "stab", "warb", "human", "tspd", "ovs", "track", "attack"};
    for (int i = 0; i < rNum; ++i) rp[i] = apvts.getRawParameterValue(ids[i]);
    for (int i = 0; i < 12; ++i) rx[i] = apvts.getRawParameterValue("x" + String(i));
}

bool AutotuneProcessor::isBusesLayoutSupported(const BusesLayout& l) const {
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    const bool inOk = in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
    return inOk && (out == in || (in == juce::AudioChannelSet::mono() && out == juce::AudioChannelSet::stereo()));
}

atn::Params AutotuneProcessor::readParams() const {
    atn::Params p;
    auto v = [this](RawId i) { return rp[i]->load(std::memory_order_relaxed); };
    p.speed = v(rSpeed); p.tol = v(rTol); p.amt = v(rAmt); p.trans = v(rTrans); p.form = v(rForm); p.formPres = v(rFormPres) > 0.5f;
    p.vd = v(rVd); p.vr = v(rVr); p.harm = juce::roundToInt(v(rHarm)); p.hv = v(rHv); p.dbl = v(rDbl); p.mix = v(rMix);
    p.lowLat = juce::roundToInt(v(rLat)) == 0;
    p.key = juce::roundToInt(v(rKey)) - 1; p.scale = juce::roundToInt(v(rScale));
    p.orbe = v(rAlgo) > 0.5f; p.expr = v(rExpr);
    p.render = juce::jlimit(0, 2, juce::roundToInt(v(rRender))); p.glide = v(rGlide);
    static const int stabMs[4] = {0, 40, 80, 200};
    p.stab = stabMs[juce::jlimit(0, 3, juce::roundToInt(v(rStab)))]; p.warb = v(rWarb); p.human = v(rHuman); p.tspd = v(rTspd); p.ovs = v(rOvs); p.track = v(rTrack);
    p.attack = v(rAttack) > 0.5f;
    for (int i = 0; i < 12; ++i) if (rx[i]->load(std::memory_order_relaxed) > 0.5f) p.flips |= 1 << i;
    return p;
}

void AutotuneProcessor::setPlain(const char* id, float v) {
    if (auto* p = apvts.getParameter(id)) { p->beginChangeGesture(); p->setValueNotifyingHost(p->convertTo0to1(v)); p->endChangeGesture(); }
}
void AutotuneProcessor::setCurrentProgram(int i) {
    prog = juce::jlimit(0, atn::kPresets - 1, i);
    const auto& q = atn::presets()[prog];
    setPlain("speed", q.speed); setPlain("tol", q.tol); setPlain("amt", q.amt); setPlain("trans", q.trans); setPlain("form", q.form);
    setPlain("vd", q.vd); setPlain("vr", q.vr); setPlain("harm", (float)q.harm); setPlain("hv", q.hv); setPlain("dbl", q.dbl);
    setPlain("rendu", (float)q.render); setPlain("glide", q.glide);   // chaque preset fixe son rendu et son comportement
    setPlain("stab", (float)stabIndex(q.stab)); setPlain("warb", q.warb); setPlain("human", q.human); setPlain("tspd", q.tspd); setPlain("ovs", q.ovs);
    setPlain("track", q.track); setPlain("attack", q.render == 0 ? 0.f : 1.f);
}
bool AutotuneProcessor::presetMatches(int i) const {
    const auto& q = atn::presets()[juce::jlimit(0, atn::kPresets - 1, i)];
    auto eq = [](float a, float b) { return std::fabs(a - b) < 0.011f * juce::jmax(1.f, std::fabs(b)); };
    return eq(raw("speed"), q.speed) && eq(raw("tol"), q.tol) && eq(raw("amt"), q.amt) && eq(raw("trans"), q.trans) && eq(raw("form"), q.form) && eq(raw("vd"), q.vd) &&
           eq(raw("vr"), q.vr) && juce::roundToInt(raw("harm")) == q.harm && eq(raw("hv"), q.hv) && eq(raw("dbl"), q.dbl) &&
           juce::roundToInt(raw("rendu")) == q.render && eq(raw("glide"), q.glide) &&
           juce::roundToInt(raw("stab")) == stabIndex(q.stab) && eq(raw("warb"), q.warb) && eq(raw("human"), q.human) && eq(raw("tspd"), q.tspd) && eq(raw("ovs"), q.ovs);
}
void AutotuneProcessor::toggleNote(int pc) {
    if (auto* p = apvts.getParameter("x" + String(pc))) { p->beginChangeGesture(); p->setValueNotifyingHost(p->getValue() > 0.5f ? 0.f : 1.f); p->endChangeGesture(); }
}
void AutotuneProcessor::clearNotes() { for (int i = 0; i < 12; ++i) if (auto* p = apvts.getParameter("x" + String(i))) if (p->getValue() > 0.5f) { p->beginChangeGesture(); p->setValueNotifyingHost(0.f); p->endChangeGesture(); } }

void AutotuneProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    engine.prepare(sampleRate);
    engine.set(readParams());   // mode de latence choisi : la latence déclarée en dépend
    latLow = engine.latencyDirect(); latQual = engine.latencyQuality();
    byp.init(juce::jmax(latLow, latQual));
    byp.lat = engine.latency();
    scratch.setSize(1, juce::jmax(8192, samplesPerBlock));   // G4 : FL envoie parfois des blocs plus grands qu'annoncé
    capture.hop = engine.orbeCore().hopSize();
    faderIsLow = engine.latency() == latLow && juce::roundToInt(raw("latmode")) == 0;
    fader.prepare(sampleRate, engine.latency(), juce::jmax(4096, samplesPerBlock));
    faderIdle.prepare(sampleRate, faderIsLow ? latQual : latLow, juce::jmax(4096, samplesPerBlock));
    setLatencySamples(engine.latency());
}

void AutotuneProcessor::processBlockBypassed(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    const int n = buffer.getNumSamples(), nc = buffer.getNumChannels();
    if (nc == 0) return;
    if (scratch.getNumSamples() < n) scratch.setSize(1, n);
    float* l = buffer.getWritePointer(0);
    float* r = nc > 1 ? buffer.getWritePointer(1) : scratch.getWritePointer(0);
    byp.process(l, r, n, getTotalNumInputChannels() >= 2);
}

void AutotuneProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    juce::ScopedNoDenormals nd;
    const int n = buffer.getNumSamples(), nc = buffer.getNumChannels();
    if (nc == 0) return;
    if (scratch.getNumSamples() < n) scratch.setSize(1, n);
    const atn::Params prm = readParams();
    engine.set(prm);
    // timeline de l'hôte : éditions de notes et capture (mode Orbe)
    double pos = 0.0; bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto info = ph->getPosition()) { if (auto ts = info->getTimeInSamples()) { pos = (double)*ts; playing = info->getIsPlaying(); } }
    lastPos.store(pos, std::memory_order_relaxed); lastPlaying.store(playing, std::memory_order_relaxed);
    const juce::SpinLock::ScopedTryLockType tl(editLock);   // jamais bloquant : si l'interface publie, ce bloc joue sans éditions
    engine.setTimeline(pos, playing, (tl.isLocked() && editLive->n > 0) ? editLive.get() : nullptr, &capture);
    if (prm.lowLat != faderIsLow) {   // changement de mode : nouvelle latence déclarée, les deux fondus de contournement échangent leur rôle
        std::swap(fader, faderIdle);
        faderIsLow = prm.lowLat; byp.lat = engine.latency();
        setLatencySamples(engine.latency());
    }
    float* l = buffer.getWritePointer(0);
    float* r = nc > 1 ? buffer.getWritePointer(1) : scratch.getWritePointer(0);
    const bool stereoIn = getTotalNumInputChannels() >= 2 && nc >= 2;
    // le fondu inactif suit l'entrée sans la modifier (mémoire de l'entrée retardée prête pour la bascule)
    faderIdle.run(l, nc > 1 ? r : nullptr, n, stereoIn, false, [](float*, float*, int) {});
    // bypass honoré : entrée retardée + fondu de 10 ms ; le moteur continue de tourner
    fader.run(l, nc > 1 ? r : nullptr, n, stereoIn, bypass != nullptr && bypass->get(), [&](float* cl, float* cr, int c) {
        float* rr = cr ? cr : r + (cl - l);   // une seule voie : tampon de travail
        if (!stereoIn) juce::FloatVectorOperations::copy(rr, cl, c);   // entrée mono : même signal des deux côtés
        engine.process(cl, rr, c);
    });
}

void AutotuneProcessor::getStateInformation(juce::MemoryBlock& dest) {
    auto st = apvts.copyState(); st.setProperty("prog", prog, nullptr); st.setProperty("atV", 1, nullptr);
    if (!notesUi.empty() || !curveUi.empty()) {   // notes éditées et courbe capturée, sauvegardées avec le projet
        juce::ValueTree ed("NOTEEDIT");
        const auto n = ned::packNotes(notesUi), c = ned::packCurve(curveUi, capture.hop);
        ed.setProperty("notes", juce::MemoryBlock(n.data(), n.size()).toBase64Encoding(), nullptr);
        ed.setProperty("curve", juce::MemoryBlock(c.data(), c.size()).toBase64Encoding(), nullptr);
        st.appendChild(ed, nullptr);
    }
    if (auto xml = st.createXml()) copyXmlToBinary(*xml, dest);
}
void AutotuneProcessor::setStateInformation(const void* data, int size) {
    if (auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(apvts.state.getType())) {
            auto st = juce::ValueTree::fromXml(*xml); prog = juce::jlimit(0, atn::kPresets - 1, (int)st.getProperty("prog", prog));
            std::vector<ned::Note> nv; curveUi.clear();
            const auto ed = st.getChildWithName("NOTEEDIT");
            if (ed.isValid()) {
                juce::MemoryBlock mb;
                if (mb.fromBase64Encoding(ed.getProperty("notes").toString())) ned::unpackNotes((const uint8_t*)mb.getData(), mb.getSize(), nv);
                juce::MemoryBlock mc;
                if (mc.fromBase64Encoding(ed.getProperty("curve").toString())) ned::unpackCurve((const uint8_t*)mc.getData(), mc.getSize(), curveUi);
                st.removeChild(ed, nullptr);
            }
            const bool legacy = !st.hasProperty("atV");   // projet d'avant le comportement P1/P2 : valeurs neutres
            bool hasAlgo = false, hasRender = false;
            for (const auto& c : st) { const auto id = c.getProperty("id").toString(); if (id == "algo") hasAlgo = true; if (id == "rendu") hasRender = true; }
            apvts.replaceState(st);
            if (!hasAlgo) setPlain("algo", 0.f);   // projet d'avant le moteur Orbe : son identique (Classique)
            if (!hasRender) setPlain("rendu", 1.f);   // projet d'avant le rendu Satin : Précis (son identique)
            if (legacy) {   // son d'origine : aucun des nouveaux réglages ne joue
                setPlain("stab", 0.f); setPlain("warb", 1.f); setPlain("human", 0.f); setPlain("tspd", 0.f); setPlain("ovs", 0.f); setPlain("track", 0.f); setPlain("attack", 0.f);
            }
            hist.clear(); hist.push_back({}); histPos = 0;
            setNotes(std::move(nv), false);
        }
}

// ---------------------------------------------------------------- édition note par note
void AutotuneProcessor::publishEdits() {
    const juce::SpinLock::ScopedLockType sl(editLock);
    editLive->assign(notesUi);
}
void AutotuneProcessor::setNotes(std::vector<ned::Note> v, bool undoable) {
    std::sort(v.begin(), v.end(), [](const ned::Note& a, const ned::Note& b) { return a.start < b.start; });
    if (v.size() > (size_t)ned::kMaxNotes) v.resize((size_t)ned::kMaxNotes);
    notesUi = std::move(v);
    if (undoable) {
        hist.resize((size_t)histPos + 1); hist.push_back(notesUi);
        if (hist.size() > 64) hist.erase(hist.begin());
        histPos = (int)hist.size() - 1;
    } else if (histPos >= 0) hist[(size_t)histPos] = notesUi;
    publishEdits();
}
bool AutotuneProcessor::undo() { if (!canUndo()) return false; notesUi = hist[(size_t)--histPos]; publishEdits(); return true; }
bool AutotuneProcessor::redo() { if (!canRedo()) return false; notesUi = hist[(size_t)++histPos]; publishEdits(); return true; }
void AutotuneProcessor::armCapture() { capture.armed.store(0); capture.n.store(0); capture.armed.store(1); }
void AutotuneProcessor::stopCapture() { if (capture.armed.load() == 2) capture.armed.store(3); else if (capture.armed.load() == 1) capture.armed.store(0); }
bool AutotuneProcessor::pollCapture() {
    if (capture.armed.load(std::memory_order_acquire) != 3) return false;
    const int n = capture.n.load(std::memory_order_acquire);
    curveUi.assign(capture.f.begin(), capture.f.begin() + n);
    ned::despike(curveUi);
    capture.armed.store(0);
    auto nv = ned::segment(curveUi.data(), (int)curveUi.size(), capture.hop, sampleRateNow());
    // les notes déjà éditées qui recouvrent une nouvelle note lui transmettent leur réglage (nouvelle prise du même passage)
    for (auto& q : nv) for (const auto& o : notesUi) if (o.edited && o.start < q.end && q.start < o.end) { q.shift = std::round((o.target() - q.center) * 100.f); q.amount = o.amount; q.expr = o.expr; q.speedMs = o.speedMs; q.edited = 1; break; }
    setNotes(std::move(nv), true);
    return true;
}

juce::AudioProcessorEditor* AutotuneProcessor::createEditor() { return new AutotuneEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AutotuneProcessor(); }
