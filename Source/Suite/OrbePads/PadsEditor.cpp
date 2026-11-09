#include "PadsEditor.h"

using namespace kit;
using namespace osamp;
using juce::Colour, juce::Rectangle, juce::String;

// ---------------- un pad : caoutchouc, tranche d'onde, voyant ----------------
class PadsEditor::PadButton : public juce::Component {
public:
    PadButton(PadsEditor& e, int i) : ed(e), idx(i) {}
    void paint(juce::Graphics& g) override {
        const float u = ed.u_;
        auto r = getLocalBounds().toFloat().reduced(2.f * u);
        const Colour c = bankColour(idx / 4);
        const bool has = ed.snap && idx < ed.snap->nsl;
        const float fl = ed.flash[idx];
        g.setColour(juce::Colours::black.withAlpha(0.6f));
        g.fillRoundedRectangle(r.translated(0, 2.5f * u), 8.f * u);
        if (fl > 0.02f) {   // lueur du pad selon la vélocité
            juce::ColourGradient glow(c.withAlpha(0.45f * fl), r.getCentreX(), r.getCentreY(), c.withAlpha(0.f), r.getRight() + 6.f * u, r.getCentreY(), true);
            g.setGradientFill(glow); g.fillRoundedRectangle(r.expanded(5.f * u), 12.f * u);
        }
        auto top = Colour(0xFF3B3C44).interpolatedWith(c, 0.30f * fl + (has ? 0.f : -0.f)), bot = Colour(0xFF1B1C21).interpolatedWith(c, 0.16f * fl);
        if (down) std::swap(top, bot);
        g.setGradientFill(juce::ColourGradient(top, r.getX(), r.getY(), bot, r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle(r, 8.f * u);
        // grain du caoutchouc
        g.setColour(juce::Colours::white.withAlpha(0.025f));
        for (float y = r.getY() + 3 * u; y < r.getBottom() - 2 * u; y += 3.f * u) g.fillRect(r.getX() + 4 * u, y, r.getWidth() - 8 * u, 1.f);
        g.setColour(juce::Colours::white.withAlpha(0.16f));
        g.drawRoundedRectangle(r.reduced(0.5f), 8.f * u, 1.f);
        // tranche d'onde
        if (has && idx < 16 && !ed.padPk[idx].empty()) {
            auto w = Rectangle<float>(r.getX() + 10 * u, r.getY() + r.getHeight() * 0.40f, r.getWidth() - 20 * u, r.getHeight() * 0.44f);
            const auto& pk = ed.padPk[idx];
            const float bw = w.getWidth() / (float)pk.size();
            for (size_t i = 0; i < pk.size(); ++i) {
                const float h = juce::jmax(1.5f * u, pk[i] * w.getHeight());
                g.setColour(c.withAlpha(0.35f + 0.5f * fl));
                g.fillRoundedRectangle(w.getX() + (float)i * bw + bw * 0.15f, w.getCentreY() - h / 2, bw * 0.7f, h, bw * 0.3f);
            }
            const float pos = ed.proc.padPos[idx].load();
            if (pos >= 0.f) { g.setColour(juce::Colours::white.withAlpha(0.9f)); g.fillRect(w.getX() + pos * w.getWidth(), w.getY() - 2 * u, 1.5f * u, w.getHeight() + 4 * u); }
        } else {
            g.setColour(ed.pal.mute);
            tracked(g, "Vide", r.withTrimmedTop(r.getHeight() * 0.35f), juce::Justification::centred, 1.6f, fnt(9.5f * u, true));
        }
        g.setColour(has ? juce::Colours::white.withAlpha(0.92f) : ed.pal.mute);
        g.setFont(mono(13.f * u));
        g.drawText(String(idx + 1).paddedLeft('0', 2), Rectangle<float>(r.getX() + 8 * u, r.getY() + 5 * u, 40 * u, 16 * u), juce::Justification::centredLeft);
        g.setColour(ed.pal.mute);
        g.setFont(mono(9.5f * u));
        g.drawText(noteName(PadsProcessor::kFirstNote + idx), Rectangle<float>(r.getX() + 8 * u, r.getBottom() - 16 * u, 50 * u, 12 * u), juce::Justification::centredLeft);
        led(g, r.getRight() - 12 * u, r.getY() + 12 * u, 3.4f * u, fl > 0.25f, c);
        if (ed.proc.sel.load() == idx) {
            g.setColour(c.withAlpha(0.95f));
            g.drawRoundedRectangle(r.reduced(1.f * u), 8.f * u, 1.8f * u);
        }
    }
    void mouseDown(const juce::MouseEvent& e) override {
        down = true;
        ed.selectPad(idx);
        ed.proc.padDown(idx, e.mods.isShiftDown() ? 0.5f : 0.9f);
        repaint();
    }
    void mouseUp(const juce::MouseEvent&) override { down = false; ed.proc.padUp(idx); repaint(); }
private:
    PadsEditor& ed;
    int idx;
    bool down = false;
};

Colour PadsEditor::bankColour(int b) {
    static const Colour c[4] = {Colour(0xFFA9ECFF), Colour(0xFFCFC6FF), Colour(0xFFFFC4AB), Colour(0xFFECEBF2)};  // glace, lilas, pêche, blanc
    return c[juce::jlimit(0, 3, b)];
}

Palette PadsEditor::palette() { return kit::w::instrument(Colour(0xFFA9ECFF)); }

// ---------------- éditeur ----------------
PadsEditor::PadsEditor(PadsProcessor& p)
    : AudioProcessorEditor(&p), proc(p), pal(palette()), look(pal),
      wave(pal, bankColour(0)),
      modeSeg(*p.apvts.getParameter("mode"), {"Transitoires", "Grille", "Manuel"}, pal),
      divSeg(*p.apvts.getParameter("div"), {"4", "8", "16"}, pal),
      playSeg(*p.apvts.getParameter("play"), {u8("Un coup"), "Tenue"}, pal),
      dltSeg(*p.apvts.getParameter("dlt"), {"1/16", "1/8", "1/8 p.", "1/4"}, pal) {
    setLookAndFeel(&look);
    auto& ap = proc.apvts;
    wave.emptyText = u8("Glisse un sample ici ou clique sur Importer");
    wave.paintOver = [this](juce::Graphics& g, WaveView& w) { paintWaveOver(g, w); };
    wave.onDown = [this](const juce::MouseEvent& e) { waveDown(e); };
    wave.onDrag = [this](const juce::MouseEvent& e) { waveDrag(e); };
    wave.onUp = [this](const juce::MouseEvent&) { dragMark = -1; };
    wave.onDouble = [this](const juce::MouseEvent& e) {
        const auto m = proc.marksCopy();
        for (int i = 1; i < (int)m.size(); ++i) if (std::abs(wave.xOf(m[(size_t)i]) - e.position.x) < 8.f * u_) { proc.removeMark(i); return; }
    };
    addAndMakeVisible(wave);
    for (auto* c : {(juce::Component*)&modeSeg, (juce::Component*)&divSeg, (juce::Component*)&playSeg, (juce::Component*)&dltSeg}) addAndMakeVisible(c);
    sens.setSliderStyle(juce::Slider::LinearHorizontal); sens.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(sens);
    sensAtt = std::make_unique<SliderAtt>(ap, "sens", sens);
    sens.setTooltip(u8("Sensibilité de la détection des attaques"));
    for (auto* b : {&importB, &demoB, &clearB}) addAndMakeVisible(b);
    importB.setTooltip(u8("Charger un sample (WAV, AIFF, FLAC, OGG) : il est enregistré dans le projet"));
    demoB.setTooltip(u8("Recharger le sample de démonstration généré par le plugin"));
    clearB.setTooltip(u8("Supprimer tous les repères sauf le premier"));
    importB.onClick = [this] {
        chooser = std::make_unique<juce::FileChooser>(u8("Choisir un sample"), juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3;*.m4a;*.caf");
        juce::Component::SafePointer<PadsEditor> self(this);
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                             [safe = self](const juce::FileChooser& fc) { if (safe && fc.getResult().existsAsFile()) safe->loadFile(fc.getResult()); });
    };
    demoB.onClick = [this] { proc.loadDemo(); };
    clearB.onClick = [this] { proc.clearMarks(); };

    const char* names[9] = {"Hauteur", "Volume", "Pan", "Filtre", "Attaque", nullptr, "Bits", "Reverb", "Delay"};
    for (int i = 0; i < 9; ++i) {
        k[i] = std::make_unique<Knob>(ap, "p0_" + String(PadsProcessor::padParamNames()[i]), i == 5 ? u8("Relâche") : String(names[i]), pal);
        addAndMakeVisible(*k[i]);
    }
    k[0]->setTooltip(u8("Hauteur par rééchantillonnage, en demi-tons (la vitesse change avec)"));
    k[3]->setTooltip(u8("Filtre passe-bas"));
    k[6]->setTooltip(u8("Bitcrush : quantification de l'amplitude (13 à 3 bits)"));
    k[7]->setTooltip(u8("Envoi vers la réverbération")); k[8]->setTooltip(u8("Envoi vers le delay synchronisé au tempo de l'hôte"));
    masterK = std::make_unique<Knob>(ap, "master", "Master", pal);
    addAndMakeVisible(*masterK);
    revB.setButtonText("Reverse"); addAndMakeVisible(revB);
    revB.setTooltip(u8("Joue la tranche à l'envers"));
    playSeg.setTooltip(u8("Un coup : la tranche joue en entier. Tenue : elle s'arrête au relâchement de la note."));
    for (int i = 0; i < 16; ++i) { pads[i] = std::make_unique<PadButton>(*this, i); addAndMakeVisible(*pads[i]); }
    selectPad(proc.sel.load());

    help.install(*this, "Orbe Pads", {u8("Sampler à 16 pads : un sample découpé en tranches, jouées par les pads ou en MIDI (à partir de C1)."),
        u8("- Importer charge un sample (WAV, AIFF, FLAC, OGG) enregistré dans le projet ; Démo recharge le sample d'exemple."),
        u8("- Découpe : Attaques (réglage de sensibilité), Grille (divisions égales) ou Manuel."),
        u8("- En manuel : clic sur la forme d'onde pour ajouter un repère, glisser pour le déplacer, double-clic ou clic droit pour le supprimer."),
        u8("- Clic sur un pad : le sélectionne et le joue (Maj : plus doux). Les réglages de droite s'appliquent au pad sélectionné.")});
    setResizable(true, true);
    setResizeLimits(760, 502, 1600, 1056);
    getConstrainer()->setFixedAspectRatio(1000.0 / 660.0);
    setSize(1000, 660);
    refreshBank();
    startTimerHz(30);
}

PadsEditor::~PadsEditor() { stopTimer(); setLookAndFeel(nullptr); }

void PadsEditor::selectPad(int i) {
    i = juce::jlimit(0, 15, i);
    const bool first = !grpSeg;
    proc.sel = i;
    const String pre = "p" + String(i) + "_";
    for (int j = 0; j < 9; ++j) k[j]->bind(pre + PadsProcessor::padParamNames()[j]);
    revAtt.reset();
    revAtt = std::make_unique<ButtonAtt>(proc.apvts, pre + "rev", revB);
    grpSeg = std::make_unique<Seg>(*proc.apvts.getParameter(pre + "grp"), juce::StringArray{"Non", "A", "B", "C"}, pal);
    grpSeg->setTooltip(u8("Les pads d'un même groupe se coupent entre eux (charleston ouvert / fermé)"));
    addAndMakeVisible(*grpSeg);
    if (!first) resized();
    repaint();
}

bool PadsEditor::isInterestedInFileDrag(const juce::StringArray& f) {
    for (auto& s : f) if (juce::File(s).hasFileExtension("wav;aif;aiff;flac;ogg;mp3;m4a;caf")) return true;
    return false;
}

void PadsEditor::loadFile(const juce::File& f) {
    if (!proc.loadFile(f)) { status = u8("Fichier illisible"); statusTicks = 90; }
    else if (proc.truncated) { status = u8("Sample coupé à 60 s"); statusTicks = 120; }
    repaint();
}

void PadsEditor::refreshBank() {
    snap = proc.bank.share();
    snapRaw = snap.get();
    if (!snap || !snap->pcm) { wave.setSample(nullptr); for (auto& p : padPk) p.clear(); }
    else {
        if (wave.sample() != snap->pcm) wave.setSample(snap->pcm);
        for (int i = 0; i < 16; ++i) padPk[i] = i < snap->nsl ? miniPeaks(*snap->pcm, snap->a[i], snap->b[i], 28) : std::vector<float>();
    }
    repaint();
}

void PadsEditor::resized() {
    u_ = (float)getWidth() / 1000.f;
    help.layout(kit::w::helpSlot(getLocalBounds().toFloat().reduced(18.f * u_, 0), u_, false).toNearestInt(), getLocalBounds());
    importB.setBounds(Si(722, 8, 110, 34)); demoB.setBounds(Si(838, 8, 90, 34));
    wave.setBounds(Si(24, 80, 952, 152));
    modeSeg.setBounds(Si(24, 242, 300, 30));
    sens.setBounds(Si(440, 244, 220, 26)); divSeg.setBounds(Si(440, 242, 150, 30));
    clearB.setBounds(Si(800, 242, 176, 30));
    for (int i = 0; i < 16; ++i) pads[i]->setBounds(Si(24.f + (float)(i % 4) * 112.f, 290.f + (float)(3 - i / 4) * 88.f, 112, 88));
    for (int i = 0; i < 6; ++i) k[i]->setBounds(Si(496.f + 78.6f * (float)i, 322, 76, 98));
    for (int i = 6; i < 9; ++i) k[i]->setBounds(Si(496.f + 78.6f * (float)(i - 6), 428, 76, 98));
    revB.setBounds(Si(744, 440, 110, 22));
    if (grpSeg) grpSeg->setBounds(Si(744, 488, 216, 28));
    playSeg.setBounds(Si(496, 566, 150, 28)); dltSeg.setBounds(Si(664, 566, 190, 28));
    masterK->setBounds(Si(882, 534, 84, 104));
    repaint();
}

void PadsEditor::timerCallback() {
    ++tick;
    if (proc.bank.get() != snapRaw) refreshBank();
    bool act = false;
    for (int i = 0; i < 16; ++i) {
        const bool on = proc.padPos[i].load() >= 0.f;
        const float old = flash[i];
        flash[i] = on ? 1.f : flash[i] * 0.82f;
        if (flash[i] < 0.02f) flash[i] = 0.f;
        act |= on;
        if (old != flash[i] || on) pads[i]->repaint();
    }
    if (act || anyActive) wave.repaint();
    anyActive = act;
    const int mode = (int)proc.p("mode");
    if (mode != lastMode) { lastMode = mode; sens.setVisible(mode == 0); divSeg.setVisible(mode == 1); clearB.setVisible(mode == 2); repaint(Si(24, 236, 952, 46)); }
    if (lastRev != proc.holder.revision.load()) { lastRev = proc.holder.revision.load(); repaint(Si(300, 8, 420, 62)); }
    if (statusTicks > 0 && --statusTicks == 0) { status.clear(); repaint(Si(300, 8, 420, 62)); }
    if (tick % 6 == 0) { for (auto& x : k) x->repaint(); masterK->repaint(); }
    if (proc.holder.state.load() == 1 && tick % 8 == 0) repaint(Si(300, 8, 420, 62));
}

// ---------------- dessin ----------------
void PadsEditor::panel(juce::Graphics& g, Rectangle<float> r, const String& title) {
    g.setColour(juce::Colours::black.withAlpha(0.22f));
    g.fillRoundedRectangle(r, 8.f * u_);
    g.setColour(juce::Colours::white.withAlpha(0.07f));
    g.drawRoundedRectangle(r, 8.f * u_, 1.f);
    engraved(g, pal, title, Rectangle<float>(r.getX() + 12 * u_, r.getY() + 6 * u_, r.getWidth() - 24 * u_, 18 * u_), juce::Justification::centredLeft, 9.5f * u_, 1.8f, true);
}

void PadsEditor::paint(juce::Graphics& g) {
    const float u = u_;
    kit::w::drawFace(g, pal, getLocalBounds(), u, {kit::w::Family::Instrument, "Orbe Pads", kit::w::serial::pads});

    // afficheur
    auto lcd = S(300, 4, 410, 44);
    screenWell(g, pal, lcd, Display::VFD);
    const auto s = proc.holder.get();
    const int st = proc.holder.state.load();
    g.setColour(pal.screenInk);
    g.setFont(fnt(14.f * u, true));
    g.drawFittedText(s ? proc.holder.name() : u8("Aucun sample"), Rectangle<float>(lcd.getX() + 12 * u, lcd.getY() + 6 * u, 190 * u, 20 * u).toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);
    g.setColour(status.isNotEmpty() ? pal.accent2 : pal.mute);
    g.setFont(mono(10.f * u));
    const String l2 = status.isNotEmpty() ? status : (s ? durText(s->dur()) + "  " + fr(s->sr / 1000.0, 1) + " kHz  " + String(snap ? snap->nsl : 0) + " / 16" : String());
    g.drawText(l2, Rectangle<float>(lcd.getX() + 12 * u, lcd.getY() + 26 * u, 200 * u, 14 * u), juce::Justification::centredLeft);
    if (s && st == 2) {
        auto cell = [&](float x, const String& cap, const String& val, Colour c, float w) {
            g.setColour(pal.mute); g.setFont(fnt(8.f * u, true));
            tracked(g, cap, Rectangle<float>(lcd.getX() + x * u, lcd.getY() + 6 * u, w * u, 11 * u), juce::Justification::centredLeft, 1.4f, fnt(8.f * u, true));
            g.setColour(c); g.setFont(mono(17.f * u));
            g.drawText(val, Rectangle<float>(lcd.getX() + x * u, lcd.getY() + 17 * u, w * u, 24 * u), juce::Justification::centredLeft);
        };
        const float bpm = proc.holder.bpm.load();
        cell(212, "Tempo", bpm > 0 ? fr(bpm, bpm == std::round(bpm) ? 0 : 1) + " bpm" : String(u8("\xE2\x80\x94")), pal.accent, 78);
        const int pc = proc.holder.pc.load();
        const bool mn = proc.holder.minor.load() != 0;
        cell(296, u8("Tonalité"), keyName(pc, mn), pal.accent2, 64);
        cell(358, "Camelot", pc >= 0 ? String(camelot(pc, mn)) : String(u8("\xE2\x80\x94")), pal.accent2, 40);
    } else if (st == 1) {
        g.setColour(pal.accent.withAlpha(0.6f + 0.4f * std::sin((float)tick * 0.35f)));
        tracked(g, "Analyse...", Rectangle<float>(lcd.getX() + 212 * u, lcd.getY() + 6 * u, 180 * u, 38 * u), juce::Justification::centredLeft, 2.f, fnt(11.f * u, true));
    }

    // barre de découpe
    const int mode = (int)proc.p("mode");
    if (mode == 0) engraved(g, pal, u8("Sensibilité"), S(340, 248, 96, 18), juce::Justification::centredRight, 9.f * u, 1.4f, true);
    if (mode == 1) engraved(g, pal, "Divisions", S(340, 248, 96, 18), juce::Justification::centredRight, 9.f * u, 1.4f, true);

    // panneau du pad sélectionné
    const int sel = proc.sel.load();
    String t = "Pad " + String(sel + 1).paddedLeft('0', 2) + "   " + noteName(PadsProcessor::kFirstNote + sel);
    if (snap && sel < snap->nsl && snap->pcm) t += "   " + durText((snap->b[sel] - snap->a[sel]) / snap->pcm->sr);
    panel(g, S(488, 290, 488, 352), t);
    engraved(g, pal, "Groupe de coupure", S(744, 468, 200, 14), juce::Justification::centredLeft, 8.5f * u, 1.4f, true);
    engraved(g, pal, "Lecture", S(496, 548, 140, 14), juce::Justification::centredLeft, 8.5f * u, 1.4f, true);
    engraved(g, pal, "Delay (temps)", S(664, 548, 180, 14), juce::Justification::centredLeft, 8.5f * u, 1.4f, true);
    g.setColour(juce::Colours::white.withAlpha(0.07f));
    g.fillRect(S(496, 538, 360, 1));
}

void PadsEditor::paintWaveOver(juce::Graphics& g, WaveView& w) {
    if (!snap || !snap->pcm) return;
    const auto r = w.inner();
    const int sel = proc.sel.load();
    float labelRight[2] = {-1e9f, -1e9f};
    for (int i = 0; i < snap->nsl; ++i) {
        const Colour c = bankColour(i / 4);
        const float x0 = w.xOf(snap->a[i]), x1 = w.xOf(snap->b[i]);
        g.setColour(c.withAlpha(i == sel ? 0.16f : (i % 2 ? 0.045f : 0.0f)));
        g.fillRect(x0, r.getY(), x1 - x0, r.getHeight());
        g.setColour(c.withAlpha(0.9f));
        g.fillRect(x0, r.getY(), i == 0 ? 1.5f : 1.2f, r.getHeight());
        const String n = String(i + 1);
        const float tw = juce::jmax(14.f * u_, 7.f * u_ * (float)n.length() + 6 * u_);
        int row = -1;   // repères en quinconce sur deux rangées ; sans place, pas d'étiquette (le trait reste)
        for (int k = 0; k < 2 && row < 0; ++k) if (x0 >= labelRight[k] + 1.f) row = k;
        if (i == sel && row < 0) row = 1;
        if (row >= 0) {
            auto tab = Rectangle<float>(x0, r.getY() + (float)row * 14.f * u_, tw, 13.f * u_);
            labelRight[row] = tab.getRight();
            g.setColour(c.withAlpha(i == sel ? 1.f : 0.8f));
            g.fillRect(tab);
            g.setColour(juce::Colours::black.withAlpha(0.85f)); g.setFont(mono(9.5f * u_)); g.drawText(n, tab, juce::Justification::centred);
        }
        const float pos = proc.padPos[i].load();
        if (pos >= 0.f) {
            const float xp = w.xOf(snap->a[i] + pos * (snap->b[i] - snap->a[i]));
            g.setColour(juce::Colours::white);
            g.fillRect(xp - 0.75f, r.getY(), 1.5f, r.getHeight());
        }
    }
}

void PadsEditor::waveDown(const juce::MouseEvent& e) {
    if (!snap || !snap->pcm) return;
    const auto m = proc.marksCopy();
    dragMark = -1;
    for (int i = 1; i < (int)m.size(); ++i) if (std::abs(wave.xOf(m[(size_t)i]) - e.position.x) < 7.f * u_) dragMark = i;
    if (e.mods.isRightButtonDown()) { if (dragMark > 0) proc.removeMark(dragMark); dragMark = -1; return; }
    if (dragMark > 0) return;
    const double f = wave.frameAt(e.position.x);
    if ((int)proc.p("mode") == 2 && !e.mods.isShiftDown()) {
        proc.addMark((int)f);
        const auto m2 = proc.marksCopy();
        for (int i = 1; i < (int)m2.size(); ++i) if (std::abs(m2[(size_t)i] - f) < snap->pcm->sr * 0.03) dragMark = i;
        return;
    }
    for (int i = 0; i < snap->nsl; ++i) if (f >= snap->a[i] && f < snap->b[i]) { selectPad(i); proc.padDown(i, 0.9f); { juce::Component::SafePointer<PadsEditor> self(this); juce::Timer::callAfterDelay(160, [safe = self, i] { if (safe) safe->proc.padUp(i); }); } break; }
}

void PadsEditor::waveDrag(const juce::MouseEvent& e) {
    if (dragMark > 0) proc.moveMark(dragMark, (int)wave.frameAt(e.position.x));
}
