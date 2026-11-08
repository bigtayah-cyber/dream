#include "DenoiserEditor.h"

using namespace of;
using kit::u8;

// Afficheur : spectre d'entrée (fond), sortie, courbe du bruit appris ou estimé.
class DnScope : public juce::Component {
public:
    explicit DnScope(DenoiserProcessor& p) : proc(p) {
        setOpaque(true); inS.fill(-120.f); outS.fill(-120.f); noiseS.fill(-120.f);
    }
    void update(float sr_) {
        sr = sr_;
        auto& e = proc.engine;
        for (int k = 0; k < dn::kBins; ++k) {
            const float i = e.dispIn[(size_t)k].load(std::memory_order_relaxed), o = e.dispOut[(size_t)k].load(std::memory_order_relaxed), n = e.dispNoise[(size_t)k].load(std::memory_order_relaxed);
            auto sm = [](float& s, float v, float up, float down) { s += (v - s) * (v > s ? up : down); };
            sm(inS[(size_t)k], i, 0.5f, 0.12f); sm(outS[(size_t)k], o, 0.5f, 0.12f); sm(noiseS[(size_t)k], n, 0.2f, 0.2f);
        }
        learnP = e.learnProgress(); learning = e.learning(); known = e.profileKnown();
        repaint();
    }
    void paint(juce::Graphics& g) override {
        const float u = (float)getWidth() / 744.f;
        auto well = getLocalBounds().toFloat();
        kit::screenWell(g, pal, well);
        auto r = well.reduced(10 * u).withTrimmedLeft(24 * u).withTrimmedBottom(12 * u).withTrimmedTop(6 * u);
        constexpr float top = -10.f, bot = -110.f;
        g.saveState(); g.reduceClipRegion(r.toNearestInt());
        for (float d = bot; d <= top + 0.1f; d += 20.f) { g.setColour(juce::Colours::white.withAlpha(0.07f)); g.drawHorizontalLine(juce::roundToInt(gfx::yOfDb(d, r, top, bot)), r.getX(), r.getRight()); }
        gfx::freqGrid(g, r, u, 20.f, 20000.f, {20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000}, false);
        auto pin = gfx::specPath(inS.data(), dn::kBins, sr, r, top, bot, true);
        g.setGradientFill(juce::ColourGradient(kLilac.withAlpha(0.32f), 0, r.getY(), kLilac.withAlpha(0.03f), 0, r.getBottom(), false)); g.fillPath(pin);
        g.setColour(kLilac.withAlpha(0.75f)); g.strokePath(gfx::specPath(inS.data(), dn::kBins, sr, r, top, bot, false), juce::PathStrokeType(1.1f * u));
        if (known || proc.engine.latency() > 0) {
            juce::Path pn = gfx::specPath(noiseS.data(), dn::kBins, sr, r, top, bot, false);
            g.setColour(kPeach); juce::Path dashed; const float dl[] = {5.f * u, 3.f * u};
            juce::PathStrokeType(1.6f * u).createDashedStroke(dashed, pn, dl, 2); g.fillPath(dashed);
        }
        g.setColour(kIce); g.strokePath(gfx::specPath(outS.data(), dn::kBins, sr, r, top, bot, false), juce::PathStrokeType(1.6f * u));
        g.restoreState();
        g.setFont(kit::mono(7.6f * u));
        for (float d = bot + 20.f; d <= top; d += 20.f) { g.setColour(pal.mute); g.drawText(String((int)d), Rectangle<float>(well.getX() + 2 * u, gfx::yOfDb(d, r, top, bot) - 5 * u, 22 * u, 10 * u), juce::Justification::centredRight); }
        for (float f : {20.f, 50.f, 100.f, 200.f, 500.f, 1000.f, 2000.f, 5000.f, 10000.f, 20000.f}) {
            g.setColour(pal.mute); g.drawText(gfx::hzLabel(f), Rectangle<float>(gfx::xOf(f, r) - 14 * u, r.getBottom() + 1 * u, 28 * u, 10 * u), juce::Justification::centred);
        }
        // légende et état
        float lx = r.getRight() - 6 * u;
        const struct { const char* t; Colour c; } leg[] = {{"Bruit appris / estim\xC3\xA9", kPeach}, {"Sortie", kIce}, {"Entr\xC3\xA9" "e", kLilac}};
        for (auto& l : leg) {
            const auto t = u8(l.t); const float w = kit::mono(8.f * u).getStringWidthFloat(upper(t)) + 4;
            lx -= w; g.setColour(pal.mute); g.setFont(kit::mono(8.f * u)); g.drawText(upper(t), Rectangle<float>(lx, r.getY() + 3 * u, w, 11 * u), juce::Justification::centredRight);
            lx -= 10 * u; g.setColour(l.c); g.fillRect(Rectangle<float>(lx, r.getY() + 7.5f * u, 8 * u, 2.4f * u)); lx -= 12 * u;
        }
        g.setFont(kit::mono(9.f * u));
        g.setColour(learning ? kPeach : pal.screenInk.withAlpha(0.8f));
        g.drawText(learning ? u8("APPRENTISSAGE DU BRUIT\xE2\x80\xA6 ") + String(juce::roundToInt(learnP * 100.f)) + " %" : known ? u8("PROFIL DE BRUIT APPRIS") : u8("AUCUN PROFIL \xC2\xB7 MODE ADAPTATIF"),
                   Rectangle<float>(r.getX() + 6 * u, r.getY() + 3 * u, 300 * u, 11 * u), juce::Justification::centredLeft);
    }
    kit::Palette pal = of::palette();
private:
    static String upper(const String& s) { return kit::upper(s); }
    DenoiserProcessor& proc;
    float sr = 48000.f, learnP = 0.f; bool learning = false, known = false;
    std::array<float, dn::kBins> inS, outS, noiseS;
};

DenoiserEditor::DenoiserEditor(DenoiserProcessor& p)
    : AudioProcessorEditor(&p), proc(p), pal(of::palette()), look(pal), power(pal),
      learnKey(u8("Apprendre le bruit"), pal, kPeach), adaptKey("Adaptatif", pal), listenKey(u8("\xC3\x89" "couter le bruit retir\xC3\xA9"), pal, kIce) {
    setLookAndFeel(&look);
    scope = std::make_unique<DnScope>(proc); addAndMakeVisible(*scope);
    struct D { const char* id; const char* lab; const char* tip; };
    const D defs[] = {
        {"red", "R\xC3\xA9" "duction", "Att\xC3\xA9nuation maximale appliqu\xC3\xA9" "e au bruit (0 \xC3\xA0 24 dB)."},
        {"sens", "Seuil", "Sensibilit\xC3\xA9 : plus haut, plus de bruit est consid\xC3\xA9r\xC3\xA9 comme tel (risque d'entamer la voix)."},
        {"smooth", "Lissage", "Lissage temps/fr\xC3\xA9quence du gain : plus haut, moins de \xC2\xAB bruit musical \xC2\xBB mais attaques plus douces."},
        {"air", "Aigus", "Pr\xC3\xA9servation des aigus et du souffle d'air : limite la r\xC3\xA9" "duction au-dessus de 4 kHz."}};
    for (auto& d : defs) { auto* pt = pots.add(new Pot(proc.apvts, d.id, u8(d.lab), u8(d.tip))); addAndMakeVisible(pt); }
    humSeg = std::make_unique<Seg>(proc.apvts, "hum", juce::StringArray{u8("Coup\xC3\xA9"), "50 Hz", "60 Hz"}, u8("Filtre anti-ronflement du secteur (fondamentale et 3 harmoniques)."), pal);
    addAndMakeVisible(*humSeg);
    for (auto* k : {&learnKey, &adaptKey, &listenKey}) addAndMakeVisible(*k);
    learnKey.setClickingTogglesState(false);
    learnKey.setTooltip(u8("Joue 1 \xC3\xA0 3 s de silence (bruit seul) : le profil du bruit est m\xC3\xA9moris\xC3\xA9 (environ 2 s) et enregistr\xC3\xA9 avec la session."));
    learnKey.onClick = [this] { proc.learn(); };
    adaptKey.setTooltip(u8("Suit en continu le plancher de bruit (minima statistiques). Un son tenu et parfaitement stable peut \xC3\xAAtre pris pour du bruit."));
    listenKey.setTooltip(u8("N'\xC3\xA9" "coute que ce qui est retir\xC3\xA9 : r\xC3\xA8gle le seuil jusqu'\xC3\xA0 n'entendre aucune voix."));
    addAndMakeVisible(power);
    power.setTooltip(u8("Effet en service. Appuie pour le couper (la latence reste compens\xC3\xA9" "e)."));
    powerAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, "bypass", power);
    adaptAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, "adapt", adaptKey);
    listenAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, "listen", listenKey);
    setResizable(true, true);
    setResizeLimits(404, 234, 1616, 936);
    getConstrainer()->setFixedAspectRatio((double)kW / kH);
    setSize((int)kW, (int)kH);
    startTimerHz(30);
}
DenoiserEditor::~DenoiserEditor() { stopTimer(); setLookAndFeel(nullptr); }

void DenoiserEditor::resized() {
    u_ = (float)getWidth() / kW;
    plate = kit::brushedPlate(getWidth(), getHeight(), pal);
    scope->setBounds(Si(32, 104, 744, 206));
    power.setBounds(Si(718, 20, 58, 52));
    for (int i = 0; i < pots.size(); ++i) pots[i]->setBounds(Si(331.f + 111.f * (float)i, 352, 111, 74));
    learnKey.setBounds(Si(32, 350, 280, 24)); adaptKey.setBounds(Si(32, 378, 280, 24)); listenKey.setBounds(Si(32, 406, 280, 24));
    humSeg->setBounds(Si(172, 434, 140, 22));
}

void DenoiserEditor::timerCallback() {
    scope->update((float)(proc.getSampleRate() > 0 ? proc.getSampleRate() : 48000.0));
    const bool l = proc.engine.learning();
    learnKey.setToggleState(l, juce::dontSendNotification);
    learnKey.fill = l ? proc.engine.learnProgress() : 0.f;
    learnKey.setButtonText(l ? u8("Apprentissage\xE2\x80\xA6") : u8("Apprendre le bruit"));
    learnKey.repaint();
}

void DenoiserEditor::paint(juce::Graphics& g) {
    const float u = u_;
    drawFace(g, pal, plate, u, kW, kH, "Orbe Denoiser", u8("R\xC3\xA9" "ducteur de bruit et de souffle \xC2\xB7 voix"), "DW-701");
    section(g, pal, u, u8("Analyse du bruit"), 32, 330, 280);
    section(g, pal, u, u8("R\xC3\xA9" "duction \xC2\xB7 spectre Wiener liss\xC3\xA9"), 331, 330, 445);
    kit::engraved(g, pal, "Anti-ronflement", Rectangle<float>(36 * u, 436 * u, 130 * u, 18 * u), juce::Justification::centredLeft, 8.4f * u, 1.6f * u, true);
    g.setColour(pal.mute); g.setFont(kit::mono(8.f * u));
    g.drawText(u8("Latence ") + String(dn::kLatency) + u8(" \xC3\xA9" "chantillons \xC2\xB7 FFT 2048 / saut 512"), Rectangle<float>(331 * u, 438 * u, 445 * u, 12 * u), juce::Justification::centredRight);
}
