#include "DeEsserEditor.h"

using namespace of;
using kit::u8;

// Spectre de la sibilance (2 à 20 kHz) : entrée, sortie, et zone de détection réglée par Fréquence et Largeur.
class DeScope : public juce::Component {
public:
    explicit DeScope(DeEsserProcessor& p) : proc(p) { setOpaque(true); }
    void paint(juce::Graphics& g) override {
        const float u = (float)getWidth() / 528.f; const auto pal = of::palette();
        auto well = getLocalBounds().toFloat(); kit::screenWell(g, pal, well);
        auto r = well.reduced(10 * u).withTrimmedLeft(24 * u).withTrimmedBottom(12 * u).withTrimmedTop(6 * u);
        constexpr float f0 = 2000.f, f1 = 20000.f, top = -10.f, bot = -100.f;
        const float sr = (float)(proc.getSampleRate() > 0 ? proc.getSampleRate() : 48000.0);
        const auto P = proc.readParams();
        g.saveState(); g.reduceClipRegion(r.toNearestInt());
        for (float d = bot; d <= top + 0.1f; d += 15.f) { g.setColour(juce::Colours::white.withAlpha(0.07f)); g.drawHorizontalLine(juce::roundToInt(gfx::yOfDb(d, r, top, bot)), r.getX(), r.getRight()); }
        const std::vector<float> marks{2000, 3000, 4000, 5000, 6000, 8000, 10000, 12000, 16000, 20000};
        gfx::freqGrid(g, r, u, f0, f1, marks, false);
        // zone de détection (−3 dB du passe-bande)
        const float half = std::pow(2.f, P.width / 2.f), xa = gfx::xOf(juce::jlimit(f0, f1, P.freq / half), r, f0, f1), xb = gfx::xOf(juce::jlimit(f0, f1, P.freq * half), r, f0, f1);
        g.setGradientFill(juce::ColourGradient(kPeach.withAlpha(P.listen ? 0.30f : 0.16f), 0, r.getY(), kPeach.withAlpha(0.04f), 0, r.getBottom(), false));
        g.fillRect(Rectangle<float>(xa, r.getY(), xb - xa, r.getHeight()));
        g.setColour(kPeach.withAlpha(0.45f)); g.drawVerticalLine(juce::roundToInt(xa), r.getY(), r.getBottom()); g.drawVerticalLine(juce::roundToInt(xb), r.getY(), r.getBottom());
        const float xc = gfx::xOf(P.freq, r, f0, f1);
        g.setColour(kPeach); g.drawVerticalLine(juce::roundToInt(xc), r.getY(), r.getBottom());
        const auto pin = gfx::specPath(proc.inAn.db.data(), odsp::Analyzer::kBins, sr, r, top, bot, true, f0, f1);
        g.setGradientFill(juce::ColourGradient(kLilac.withAlpha(0.30f), 0, r.getY(), kLilac.withAlpha(0.03f), 0, r.getBottom(), false)); g.fillPath(pin);
        g.setColour(kLilac.withAlpha(0.8f)); g.strokePath(gfx::specPath(proc.inAn.db.data(), odsp::Analyzer::kBins, sr, r, top, bot, false, f0, f1), juce::PathStrokeType(1.2f * u));
        g.setColour(kIce); g.strokePath(gfx::specPath(proc.outAn.db.data(), odsp::Analyzer::kBins, sr, r, top, bot, false, f0, f1), juce::PathStrokeType(1.6f * u));
        // seuil : repère horizontal indicatif (niveau de bande, pas de bin)
        g.restoreState();
        g.setFont(kit::mono(7.6f * u));
        for (float d = bot + 15.f; d <= top; d += 15.f) { g.setColour(pal.mute); g.drawText(String((int)d), Rectangle<float>(well.getX() + 2 * u, gfx::yOfDb(d, r, top, bot) - 5 * u, 22 * u, 10 * u), juce::Justification::centredRight); }
        for (float f : marks) { g.setColour(pal.mute); g.drawText(gfx::hzLabel(f), Rectangle<float>(gfx::xOf(f, r, f0, f1) - 14 * u, r.getBottom() + 1 * u, 28 * u, 10 * u), juce::Justification::centred); }
        g.setFont(kit::mono(8.f * u));
        g.setColour(kPeach); g.drawText(fmtHz(P.freq) + "  /  " + String(P.width, 1) + " oct", Rectangle<float>(juce::jlimit(r.getX(), r.getRight() - 120 * u, xc - 60 * u), r.getY() + 2 * u, 120 * u, 11 * u), juce::Justification::centred);
        g.setColour(P.listen ? kPeach : pal.mute); g.drawText(P.listen ? u8("\xE2\x97\x8F \xC3\x89" "COUTE DE LA SIBILANCE") : u8("SPECTRE \xC2\xB7 ENTR\xC3\x89" "E / SORTIE"), Rectangle<float>(r.getX() + 6 * u, r.getY() + 2 * u, 180 * u, 11 * u), juce::Justification::centredLeft);
        float lx = r.getRight() - 6 * u;
        const struct { const char* t; Colour c; } leg[] = {{"Sortie", kIce}, {"Entr\xC3\xA9" "e", kLilac}};
        for (auto& l : leg) { const auto t = kit::upper(u8(l.t)); const float w = kit::mono(8.f * u).getStringWidthFloat(t) + 4; lx -= w; g.setColour(pal.mute); g.drawText(t, Rectangle<float>(lx, r.getBottom() - 14 * u, w, 11 * u), juce::Justification::centredRight); lx -= 10 * u; g.setColour(l.c); g.fillRect(Rectangle<float>(lx, r.getBottom() - 9.5f * u, 8 * u, 2.4f * u)); lx -= 10 * u; }
    }
private:
    DeEsserProcessor& proc;
};

// Réduction de gain : valeur crête et historique défilant (environ 6 s).
class GrPanel : public juce::Component {
public:
    explicit GrPanel(DeEsserProcessor& p) : proc(p) { setOpaque(true); hist.fill(0.f); }
    void push(float v) { hist[(size_t)pos] = v; pos = (pos + 1) % (int)hist.size(); peak = juce::jmax(v, peak - 0.25f); repaint(); }
    void paint(juce::Graphics& g) override {
        const float u = (float)getWidth() / 156.f; const auto pal = of::palette();
        auto well = getLocalBounds().toFloat(); kit::screenWell(g, pal, well);
        g.setColour(pal.mute); kit::tracked(g, u8("R\xC3\xA9" "duction de gain"), Rectangle<float>(10 * u, 8 * u, 136 * u, 10 * u), juce::Justification::centredLeft, 1.2f * u, kit::fnt(7.2f * u, true));
        g.setColour(peak > 0.05f ? kPeach : pal.screenInk); g.setFont(kit::mono(22.f * u));
        g.drawText(peak > 0.05f ? juce::String::fromUTF8("\xE2\x88\x92") + String(peak, 1) : String("0.0"), Rectangle<float>(6 * u, 20 * u, 144 * u, 28 * u), juce::Justification::centred);
        g.setColour(pal.mute); g.setFont(kit::mono(8.f * u)); g.drawText("dB", Rectangle<float>(100 * u, 48 * u, 48 * u, 10 * u), juce::Justification::centredRight);
        auto r = Rectangle<float>(12 * u, 64 * u, 132 * u, well.getHeight() - 76 * u);
        const float scale = juce::jmax(6.f, std::ceil(proc.apvts.getRawParameterValue("range")->load() / 3.f) * 3.f);
        g.setColour(juce::Colours::white.withAlpha(0.07f));
        for (int i = 0; i <= 4; ++i) g.drawHorizontalLine(juce::roundToInt(r.getY() + r.getHeight() * (float)i / 4.f), r.getX(), r.getRight());
        juce::Path p; p.startNewSubPath(r.getX(), r.getY());
        const int n = (int)hist.size();
        for (int i = 0; i < n; ++i) { const float v = juce::jlimit(0.f, scale, hist[(size_t)((pos + i) % n)]); p.lineTo(r.getX() + r.getWidth() * (float)i / (float)(n - 1), r.getY() + r.getHeight() * v / scale); }
        p.lineTo(r.getRight(), r.getY()); p.closeSubPath();
        g.setGradientFill(juce::ColourGradient(kPeach.withAlpha(0.15f), 0, r.getY(), kPeach.withAlpha(0.6f), 0, r.getBottom(), false)); g.fillPath(p);
        g.setColour(kPeach); g.strokePath(p, juce::PathStrokeType(1.1f * u));
        g.setColour(pal.mute); g.setFont(kit::mono(7.f * u));
        g.drawText("0", Rectangle<float>(r.getX() + 1, r.getY() - 9 * u, 20 * u, 8 * u), juce::Justification::centredLeft);
        g.drawText(juce::String::fromUTF8("\xE2\x88\x92") + String((int)scale), Rectangle<float>(r.getX() + 1, r.getBottom() - 9 * u, 24 * u, 8 * u), juce::Justification::centredLeft);
    }
private:
    DeEsserProcessor& proc; std::array<float, 180> hist; int pos = 0; float peak = 0.f;
};

DeEsserEditor::DeEsserEditor(DeEsserProcessor& p)
    : AudioProcessorEditor(&p), proc(p), pal(of::palette()), look(pal), power(pal), listenKey(u8("\xC3\x89" "couter la sibilance"), pal, kPeach) {
    setLookAndFeel(&look);
    scope = std::make_unique<DeScope>(proc); addAndMakeVisible(*scope);
    grPanel = std::make_unique<GrPanel>(proc); addAndMakeVisible(*grPanel);
    struct D { const char* id; const char* lab; const char* tip; };
    const D defs[] = {
        {"freq", "Fr\xC3\xA9quence", "Centre de la bande de sibilance d\xC3\xA9tect\xC3\xA9" "e (4 \xC3\xA0 10 kHz). Voix d'homme plut\xC3\xB4t 5-7 kHz, voix de femme 7-9 kHz."},
        {"width", "Largeur", "Largeur de la bande de d\xC3\xA9tection, en octaves."},
        {"thr", "Seuil", "Niveau de la bande au-dessus duquel la sibilance est att\xC3\xA9nu\xC3\xA9" "e."},
        {"range", "Plage", "R\xC3\xA9" "duction maximale appliqu\xC3\xA9" "e (0 \xC3\xA0 24 dB)."},
        {"atk", "Attaque", "Rapidit\xC3\xA9 de la r\xC3\xA9" "duction quand une sibilance arrive."},
        {"rel", "Rel\xC3\xA2" "che", "Temps de retour au gain normal apr\xC3\xA8s la sibilance."}};
    for (auto& d : defs) addAndMakeVisible(pots.add(new Pot(proc.apvts, d.id, u8(d.lab), u8(d.tip))));
    modeSeg = std::make_unique<Seg>(proc.apvts, "mode", juce::StringArray{"Split", "Large bande"}, u8("Split : seule la bande de sibilance est att\xC3\xA9nu\xC3\xA9" "e. Large bande : tout le signal baisse (plus fort, moins naturel)."), pal);
    styleSeg = std::make_unique<Seg>(proc.apvts, "style", juce::StringArray{"Doux", "Net"}, u8("Doux : genou large, r\xC3\xA9" "duction progressive. Net : r\xC3\xA9" "duction franche d\xC3\xA8s le seuil."), pal);
    addAndMakeVisible(*modeSeg); addAndMakeVisible(*styleSeg); addAndMakeVisible(listenKey);
    listenKey.setTooltip(u8("N'\xC3\xA9" "coute que la bande de d\xC3\xA9tection pour r\xC3\xA9gler Fr\xC3\xA9quence et Largeur sur les \xC2\xAB s \xC2\xBB."));
    listenAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, "listen", listenKey);
    addAndMakeVisible(power);
    power.setTooltip(u8("Effet en service. Appuie pour le couper (la latence reste compens\xC3\xA9" "e)."));
    powerAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, "bypass", power);
    setResizable(true, true);
    setResizeLimits(380, 246, 1520, 984);
    getConstrainer()->setFixedAspectRatio((double)kW / kH);
    setSize((int)kW, (int)kH);
    startTimerHz(30);
}
DeEsserEditor::~DeEsserEditor() { stopTimer(); setLookAndFeel(nullptr); }

void DeEsserEditor::resized() {
    u_ = (float)getWidth() / kW;
    plate = kit::brushedPlate(getWidth(), getHeight(), pal);
    scope->setBounds(Si(32, 104, 528, 206)); grPanel->setBounds(Si(572, 104, 156, 206));
    power.setBounds(Si(670, 20, 58, 52));
    const float px[6] = {32, 148, 264, 380, 496, 612};
    for (int i = 0; i < 6; ++i) pots[i]->setBounds(Si(px[i], 348, 116, 76));
    modeSeg->setBounds(Si(32, 460, 190, 26)); styleSeg->setBounds(Si(238, 460, 150, 26)); listenKey.setBounds(Si(404, 460, 324, 26));
}

void DeEsserEditor::timerCallback() {
    proc.inAn.compute(); proc.outAn.compute();
    grPanel->push(proc.gr.load());
    scope->repaint();
}

void DeEsserEditor::paint(juce::Graphics& g) {
    const float u = u_;
    drawFace(g, pal, plate, u, kW, kH, "Orbe DeEsser", u8("Att\xC3\xA9nuateur de sibilance \xC2\xB7 voix"), "DW-703");
    section(g, pal, u, u8("D\xC3\xA9tection \xC2\xB7 bande de sibilance"), 32, 328, 464);
    section(g, pal, u, u8("Dynamique"), 496, 328, 232);
    section(g, pal, u, u8("Mode \xC2\xB7 \xC3\xA9" "coute"), 32, 438, 696);
    g.setColour(pal.mute); g.setFont(kit::mono(8.f * u));
    g.drawText(u8("Lookahead 1 ms \xC2\xB7 latence ") + String(proc.getLatencySamples()) + u8(" \xC3\xA9" "chantillons"), Rectangle<float>(404 * u, 489 * u, 324 * u, 10 * u), juce::Justification::centredRight);
}
