#include "SampleEditor.h"

using namespace kit;
using namespace osamp;
using juce::Colour, juce::Rectangle, juce::String;

static Colour bankColour(int b) {
    static const Colour c[4] = {Colour(0xFFA9ECFF), Colour(0xFFCFC6FF), Colour(0xFFFFC4AB), Colour(0xFFECEBF2)};  // glace, lilas, pêche, blanc
    return c[juce::jlimit(0, 3, b)];
}

// Puce de préréglage : texte sur deux lignes si besoin, voyant à gauche.
class SampleEditor::Chip : public juce::Button {
public:
    Chip(const String& t, const Palette& p) : juce::Button(t), P(p) { setClickingTogglesState(false); }
    void paintButton(juce::Graphics& g, bool hover, bool down) override {   // touche de preset à LED intégrée
        auto r = getLocalBounds().toFloat().reduced(1.5f);
        const bool on = getToggleState();
        pushButton(g, P, r, on, down, P.accent, {});
        auto cap = r.reduced(1.f).translated(0, down ? 1.f : 0.f).withTrimmedTop(r.getHeight() * 0.2f);
        g.setColour(on ? juce::Colours::white : P.ink.withAlpha(hover ? 0.95f : 0.82f));
        g.setFont(fnt(juce::jmax(8.f, r.getHeight() * 0.26f), true));
        g.drawFittedText(upper(getButtonText()), cap.reduced(4.f, 0.f).toNearestInt(), juce::Justification::centred, 2, 0.85f);
    }
private:
    Palette P;
};

Palette SampleEditor::palette() { return [] { auto p = kit::w::instrument(Colour(0xFFCFC6FF)); p.accent2 = Colour(0xFFFFC4AB); return p; }(); }

SampleEditor::SampleEditor(SampleProcessor& p)
    : AudioProcessorEditor(&p), proc(p), pal(palette()), look(pal),
      srcWave(pal, Colour(0xFFECEBF2)), resWave(pal, Colour(0xFFCFC6FF)),
      modeSeg(*p.apvts.getParameter("mode"), {u8("Liés (vinyle)"), u8("Indépendants")}, pal),
      chopSeg(*p.apvts.getParameter("chop"), {"Aucune", "4", "8", "16", "Attaques"}, pal) {
    setLookAndFeel(&look);
    auto& ap = proc.apvts;
    srcWave.emptyText = u8("Glisse un sample ici ou clique sur Importer");
    resWave.emptyText = u8("Le résultat apparaît ici");
    srcWave.paintOver = [this](juce::Graphics& g, WaveView& w) { paintSrcOver(g, w); };
    resWave.paintOver = [this](juce::Graphics& g, WaveView& w) { paintResOver(g, w); };
    srcWave.onDown = [this](const juce::MouseEvent& e) { srcDrag(e, true); };
    srcWave.onDrag = [this](const juce::MouseEvent& e) { srcDrag(e, false); };
    srcWave.onUp = [this](const juce::MouseEvent&) {
        if (drag) { for (auto id : {"zoneA", "zoneB"}) if (auto* q = proc.apvts.getParameter(id)) q->endChangeGesture(); }
        drag = 0;
    };
    srcWave.onDouble = [this](const juce::MouseEvent&) { proc.setZone(0.f, 1.f); srcWave.repaint(); };
    resWave.onDown = [this](const juce::MouseEvent& e) {
        if (!res || !res->pcm) return;
        const double f = resWave.frameAt(e.position.x);
        for (int i = 0; i < res->nsl; ++i) if (f >= res->a[i] && f < res->b[i]) { proc.sliceDown(i); break; }
    };
    addAndMakeVisible(srcWave); addAndMakeVisible(resWave);
    addAndMakeVisible(modeSeg); addAndMakeVisible(chopSeg);
    modeSeg.setTooltip(u8("Liés : la vitesse suit la hauteur, comme un vinyle. Indépendants : étirement WSOLA, la hauteur ne bouge pas."));
    chopSeg.setTooltip(u8("Découpe le résultat en parts jouables au clavier MIDI à partir de C1"));

    struct KD { const char* id; String cap; };
    const KD kd[9] = {{"semi", "Hauteur"}, {"speed", "Vitesse"}, {"cut", "Filtre"}, {"bits", "Bits"}, {"rate", u8("Échantillon")}, {"crackle", "Craquements"},
                      {"wow", "Pleurage"}, {"verb", u8("Réverbe")}, {"master", "Volume"}};
    for (int i = 0; i < 9; ++i) { k[i] = std::make_unique<Knob>(ap, kd[i].id, kd[i].cap, pal); addAndMakeVisible(*k[i]); }
    k[0]->setTooltip(u8("Hauteur en demi-tons"));
    k[1]->setTooltip(u8("Vitesse de lecture en mode indépendants. Liés : calculée depuis la hauteur."));
    k[4]->setTooltip(u8("Fréquence d'échantillonnage simulée (son sale des vieux samplers)"));
    k[5]->setTooltip(u8("Craquements de vinyle (toujours les mêmes d'un rendu à l'autre)"));
    k[6]->setTooltip(u8("Pleurage : variations lentes de la vitesse, comme une cassette usée"));
    k[1]->textOverride = [this] {
        if (proc.p("mode") < 0.5f) return String(juce::roundToInt(std::pow(2.f, proc.p("semi") / 12.f) * 100.f)) + " %";
        if (proc.p("sync") > 0.5f) { const double r = proc.worker.busy ? 0.0 : 1.0; (void)r; return u8("hôte"); }
        return String();
    };
    for (int i = 0; i < 9; ++i) {
        chip[i] = std::make_unique<Chip>(SampleProcessor::presetName(i), pal);
        chip[i]->onClick = [this, i] { proc.applyPreset(i); if (!proc.audition) proc.audition = true; };
        addAndMakeVisible(*chip[i]);
    }
    revB.setButtonText("Reverse"); tapeB.setButtonText(u8("Arrêt de bande")); loopB.setButtonText("Boucle"); syncB.setButtonText(u8("Sync BPM hôte")); followB.setButtonText(u8("Suivre l'hôte"));
    for (auto* b : {&revB, &tapeB, &loopB, &syncB, &followB}) addAndMakeVisible(b);
    aRev = std::make_unique<ButtonAtt>(ap, "rev", revB); aTape = std::make_unique<ButtonAtt>(ap, "tape", tapeB);
    aLoop = std::make_unique<ButtonAtt>(ap, "loop", loopB); aSync = std::make_unique<ButtonAtt>(ap, "sync", syncB); aFollow = std::make_unique<ButtonAtt>(ap, "follow", followB);
    revB.setTooltip(u8("Lit la zone à l'envers")); tapeB.setTooltip(u8("Ralentit la bande jusqu'à l'arrêt sur la fin de la boucle"));
    loopB.setTooltip(u8("Boucle : la queue de réverbe se replie sur le début"));
    syncB.setTooltip(u8("Mode indépendants : cale la boucle sur le tempo de l'hôte (tempo de la source détecté automatiquement)"));
    followB.setTooltip(u8("Quand l'hôte joue, la boucle suit sa position"));
    for (auto* b : {&importB, &demoB, &playB}) addAndMakeVisible(b);
    playB.setClickingTogglesState(true);
    playB.onClick = [this] { proc.audition = playB.getToggleState(); };
    importB.setTooltip(u8("Charger un son (WAV, AIFF, FLAC, OGG, MP3) : il est enregistré dans le projet"));
    importB.onClick = [this] {
        chooser = std::make_unique<juce::FileChooser>(u8("Choisir un son"), juce::File(), osamp::audioWildcards());
        juce::Component::SafePointer<SampleEditor> self(this);
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                             [safe = self](const juce::FileChooser& fc) { if (safe && fc.getResult().existsAsFile()) safe->loadFile(fc.getResult()); });
    };
    demoB.onClick = [this] { proc.loadDemo(); };

    setResizable(true, true);
    help.install(*this, "Orbe Sample", {u8("Transformateur de sample : choisis une zone de la source, applique un préréglage et un traitement, joue le résultat."),
        u8("- Source : glisse les poignées pour régler la zone, ou glisse ailleurs pour en tracer une nouvelle."),
        u8("- Préréglages : neuf transformations de départ ; Hauteur et vitesse ; Découpes : le résultat est découpé et jouable en MIDI à partir de C1."),
        u8("- Importer charge un son (WAV, AIFF, FLAC, OGG, MP3) enregistré dans le projet ; Démo recharge l'exemple ; Lecture écoute le résultat.")});
    setResizeLimits(760, 494, 1600, 1040);
    getConstrainer()->setFixedAspectRatio(1000.0 / 650.0);
    setSize(1000, 650);
    startTimerHz(30);
}

SampleEditor::~SampleEditor() { stopTimer(); setLookAndFeel(nullptr); }

bool SampleEditor::isInterestedInFileDrag(const juce::StringArray& f) {
    for (auto& s : f) if (juce::File(s).hasFileExtension(osamp::audioExtensions())) return true;
    return false;
}

void SampleEditor::loadFile(const juce::File& f) {
    if (!proc.loadFile(f)) { status = u8("Fichier illisible"); statusTicks = 90; }
    else if (proc.truncated) { status = u8("Son coupé à 2 min : choisis ta zone"); statusTicks = 150; }
    repaint();
}

void SampleEditor::resized() {
    u_ = (float)getWidth() / 1000.f;
    help.layout(kit::w::helpSlot(getLocalBounds().toFloat().reduced(18.f * u_, 0), u_, false).toNearestInt(), getLocalBounds());
    importB.setBounds(Si(722, 8, 110, 34)); demoB.setBounds(Si(838, 8, 90, 34));
    srcWave.setBounds(Si(24, 78, 952, 112));
    resWave.setBounds(Si(24, 196, 952, 142));
    for (int i = 0; i < 9; ++i) chip[i]->setBounds(Si(36.f + (float)(i % 3) * 112.f, 382.f + (float)(i / 3) * 42.f, 106, 36));
    modeSeg.setBounds(Si(36, 532, 340 - 12, 28));
    chopSeg.setBounds(Si(36, 586, 340 - 12, 28));
    for (int i = 0; i < 6; ++i) k[i]->setBounds(Si(396.f + 95.f * (float)i, 382, 91, 98));
    for (int i = 6; i < 9; ++i) k[i]->setBounds(Si(396.f + 95.f * (float)(i - 6), 488, 91, 98));
    revB.setBounds(Si(692, 494, 130, 22)); tapeB.setBounds(Si(692, 522, 140, 22)); loopB.setBounds(Si(692, 550, 130, 22));
    syncB.setBounds(Si(836, 494, 136, 22)); followB.setBounds(Si(836, 522, 136, 22));
    playB.setBounds(Si(836, 586, 128, 40));
    repaint();
}

void SampleEditor::timerCallback() {
    ++tick;
    if (proc.out.get() != resRaw) {
        res = proc.out.share(); resRaw = res.get();
        resWave.setSample(res ? res->pcm : nullptr);
    }
    if (lastRev != proc.holder.revision.load()) {
        lastRev = proc.holder.revision.load();
        if (srcWave.sample() != proc.holder.get()) srcWave.setSample(proc.holder.get());
        repaint(Si(300, 8, 420, 62)); resWave.repaint();
    }
    const int pi = proc.presetIdx.load();
    if (pi != lastPreset) { lastPreset = pi; for (int i = 0; i < 9; ++i) chip[i]->setToggleState(i == pi, juce::dontSendNotification); }
    if (playB.getToggleState() != proc.audition.load()) playB.setToggleState(proc.audition.load(), juce::dontSendNotification);
    const bool linked = proc.p("mode") < 0.5f, sync = proc.p("sync") > 0.5f;
    k[1]->setAlpha(linked || sync ? 0.5f : 1.f);
    syncB.setAlpha(linked ? 0.4f : 1.f);
    const int sp = proc.slicePlaying.load();
    const bool playing = proc.playPos.load() >= 0.f;
    if (playing || wasPlaying || sp != lastSlice || proc.worker.busy.load()) resWave.repaint();
    wasPlaying = playing; lastSlice = sp;
    if (statusTicks > 0 && --statusTicks == 0) { status.clear(); repaint(Si(300, 8, 420, 62)); }
    if (tick % 6 == 0) { for (auto& x : k) x->repaint(); srcWave.repaint(); }
    if (proc.holder.state.load() == 1 && tick % 8 == 0) repaint(Si(300, 8, 420, 62));
}

// ---------------- dessin ----------------
void SampleEditor::panel(juce::Graphics& g, Rectangle<float> r, const String& title) {
    g.setColour(juce::Colours::black.withAlpha(0.22f));
    g.fillRoundedRectangle(r, 8.f * u_);
    g.setColour(juce::Colours::white.withAlpha(0.07f));
    g.drawRoundedRectangle(r, 8.f * u_, 1.f);
    engraved(g, pal, title, Rectangle<float>(r.getX() + 12 * u_, r.getY() + 6 * u_, r.getWidth() - 24 * u_, 18 * u_), juce::Justification::centredLeft, 9.5f * u_, 1.8f, true);
}

void SampleEditor::paint(juce::Graphics& g) {
    const float u = u_;
    kit::w::drawFace(g, pal, getLocalBounds(), u, {kit::w::Family::Instrument, "Orbe Sample", kit::w::serial::sample});

    auto lcd = S(300, 4, 410, 44);
    screenWell(g, pal, lcd, Display::VFD);
    const auto s = proc.holder.get();
    const int st = proc.holder.state.load();
    g.setColour(pal.screenInk);
    g.setFont(fnt(14.f * u, true));
    g.drawFittedText(s ? proc.holder.name() : u8("Aucun son"), Rectangle<float>(lcd.getX() + 12 * u, lcd.getY() + 6 * u, 190 * u, 20 * u).toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);
    g.setColour(status.isNotEmpty() ? pal.accent2 : pal.mute);
    g.setFont(mono(10.f * u));
    g.drawText(status.isNotEmpty() ? status : (s ? u8("Source ") + durText(s->dur()) + "  " + fr(s->sr / 1000.0, 1) + " kHz" : String()),
               Rectangle<float>(lcd.getX() + 12 * u, lcd.getY() + 26 * u, 200 * u, 14 * u), juce::Justification::centredLeft);
    if (s && st == 2) {
        auto cell = [&](float x, const String& cap, const String& val, Colour c, float w) {
            g.setColour(pal.mute);
            tracked(g, cap, Rectangle<float>(lcd.getX() + x * u, lcd.getY() + 6 * u, w * u, 11 * u), juce::Justification::centredLeft, 1.4f, fnt(8.f * u, true));
            g.setColour(c); g.setFont(mono(17.f * u));
            g.drawText(val, Rectangle<float>(lcd.getX() + x * u, lcd.getY() + 17 * u, w * u, 24 * u), juce::Justification::centredLeft);
        };
        const float bpm = proc.holder.bpm.load();
        cell(212, "Tempo", bpm > 0 ? fr(bpm, bpm == std::round(bpm) ? 0 : 1) + " bpm" : String(u8("\xE2\x80\x94")), pal.accent, 78);
        const int pc = proc.holder.pc.load(); const bool mn = proc.holder.minor.load() != 0;
        cell(296, u8("Tonalité"), keyName(pc, mn), pal.accent2, 64);
        cell(358, "Camelot", pc >= 0 ? String(camelot(pc, mn)) : String(u8("\xE2\x80\x94")), pal.accent2, 40);
    } else if (st == 1) {
        g.setColour(pal.accent.withAlpha(0.6f + 0.4f * std::sin((float)tick * 0.35f)));
        tracked(g, "Analyse...", Rectangle<float>(lcd.getX() + 212 * u, lcd.getY() + 6 * u, 180 * u, 38 * u), juce::Justification::centredLeft, 2.f, fnt(11.f * u, true));
    }

    panel(g, S(24, 350, 352, 292), u8("Préréglages"));
    engraved(g, pal, u8("Hauteur et vitesse"), S(36, 514, 300, 14), juce::Justification::centredLeft, 8.5f * u, 1.4f, true);
    engraved(g, pal, u8("Découpes"), S(36, 568, 330, 14), juce::Justification::centredLeft, 8.5f * u, 1.4f, true);
    panel(g, S(388, 350, 588, 292), "Traitement");

}

void SampleEditor::paintSrcOver(juce::Graphics& g, WaveView& w) {
    const auto smp = w.sample();
    const auto r = w.inner();
    engraved(g, pal, "Source", Rectangle<float>(r.getX() + 6 * u_, r.getY() + 3 * u_, 120 * u_, 14 * u_), juce::Justification::centredLeft, 8.5f * u_, 1.6f, true);
    if (!smp) return;
    const float za = proc.p("zoneA"), zb = proc.p("zoneB");
    const float x0 = r.getX() + za * r.getWidth(), x1 = r.getX() + zb * r.getWidth();
    g.setColour(juce::Colours::black.withAlpha(0.55f));
    g.fillRect(r.getX(), r.getY(), x0 - r.getX(), r.getHeight());
    g.fillRect(x1, r.getY(), r.getRight() - x1, r.getHeight());
    g.setColour(pal.accent.withAlpha(0.12f));
    g.fillRect(x0, r.getY(), x1 - x0, r.getHeight());
    g.setColour(pal.accent);
    g.fillRect(x0 - 1.f, r.getY(), 2.f, r.getHeight());
    g.fillRect(x1 - 1.f, r.getY(), 2.f, r.getHeight());
    for (float x : {x0, x1}) { juce::Path t; const float d = x == x0 ? 1.f : -1.f; t.addTriangle(x, r.getY(), x + d * 9 * u_, r.getY(), x, r.getY() + 12 * u_); g.fillPath(t); }
    g.setColour(pal.screenInk.withAlpha(0.85f));
    g.setFont(mono(10.5f * u_));
    g.drawText(String(u8("Zone ")) + durText((zb - za) * smp->dur()), Rectangle<float>(r.getRight() - 150 * u_, r.getY() + 3 * u_, 144 * u_, 14 * u_), juce::Justification::centredRight);
}

void SampleEditor::paintResOver(juce::Graphics& g, WaveView& w) {
    const auto r = w.inner();
    engraved(g, pal, u8("Résultat"), Rectangle<float>(r.getX() + 6 * u_, r.getY() + 16 * u_, 120 * u_, 14 * u_), juce::Justification::centredLeft, 8.5f * u_, 1.6f, true);
    if (proc.worker.busy.load()) {
        g.setColour(pal.accent2.withAlpha(0.6f + 0.4f * std::sin((float)tick * 0.4f)));
        tracked(g, "Calcul...", Rectangle<float>(r.getX() + 70 * u_, r.getY() + 16 * u_, 120 * u_, 14 * u_), juce::Justification::centredLeft, 1.6f, fnt(8.5f * u_, true));
    }
    if (!res || !res->pcm) return;
    String info = durText(res->pcm->dur());
    if (res->bpm > 0) info += "  " + fr(res->bpm, 1) + " bpm";
    if (res->pc >= 0) info += "  " + keyName(res->pc, res->minor) + "  " + String(camelot(res->pc, res->minor));
    const int sp = proc.slicePlaying.load();
    for (int i = 0; i < res->nsl; ++i) {
        const Colour c = bankColour(i / 4);
        const float x0 = w.xOf(res->a[i]), x1 = w.xOf(res->b[i]);
        g.setColour(c.withAlpha(i == sp ? 0.20f : (i % 2 ? 0.045f : 0.f)));
        g.fillRect(x0, r.getY(), x1 - x0, r.getHeight());
        g.setColour(c.withAlpha(0.85f));
        g.fillRect(x0, r.getY(), 1.2f, r.getHeight());
        auto tab = Rectangle<float>(x0, r.getBottom() - 13 * u_, juce::jmin(30.f * u_, x1 - x0 - 1.f), 13.f * u_);
        g.fillRect(tab);
        if (tab.getWidth() > 19.f * u_) { g.setColour(juce::Colours::black.withAlpha(0.85f)); g.setFont(mono(9.f * u_)); g.drawText(noteName(SampleProcessor::kFirstNote + i), tab, juce::Justification::centred); }
    }
    const float pp = proc.playPos.load();
    if (pp >= 0.f) { g.setColour(juce::Colours::white); g.fillRect(r.getX() + pp * r.getWidth() - 0.75f, r.getY(), 1.5f, r.getHeight()); }
    {   // lecture du résultat sur fond opaque : les traits de découpe ne la coupent plus
        g.setFont(mono(10.5f * u_));
        const float tw = juce::GlyphArrangement::getStringWidth(g.getCurrentFont(), info) + 12 * u_;
        auto box = Rectangle<float>(r.getRight() - tw - 4 * u_, r.getY() + 2 * u_, tw, 16 * u_);
        g.setColour(juce::Colour(0xE0080A0E)); g.fillRoundedRectangle(box, 2.f * u_);
        g.setColour(pal.screenInk.withAlpha(0.9f));
        g.drawText(info, box.reduced(6 * u_, 0), juce::Justification::centredRight);
    }
}

void SampleEditor::srcDrag(const juce::MouseEvent& e, bool start) {
    if (!srcWave.sample()) return;
    const auto r = srcWave.inner();
    const float frac = juce::jlimit(0.f, 1.f, (e.position.x - r.getX()) / r.getWidth());
    auto set = [&](const char* id, float v) { if (auto* q = proc.apvts.getParameter(id)) q->setValueNotifyingHost(q->convertTo0to1(v)); };
    float a = proc.p("zoneA"), b = proc.p("zoneB");
    if (start) {
        const float px0 = r.getX() + a * r.getWidth(), px1 = r.getX() + b * r.getWidth();
        drag = std::abs(e.position.x - px0) < 10.f * u_ ? 1 : std::abs(e.position.x - px1) < 10.f * u_ ? 2 : 3;
        anchor = frac;
        for (auto id : {"zoneA", "zoneB"}) if (auto* q = proc.apvts.getParameter(id)) q->beginChangeGesture();
        if (drag != 3) return;
    }
    const float mw = 0.004f;
    if (drag == 1) set("zoneA", juce::jmin(frac, b - mw));
    else if (drag == 2) set("zoneB", juce::jmax(frac, a + mw));
    else if (drag == 3) { a = juce::jmin(anchor, frac); b = juce::jmax(anchor, frac); if (b - a < mw) b = juce::jmin(1.f, a + mw); set("zoneA", a); set("zoneB", b); }
    srcWave.repaint();
}
