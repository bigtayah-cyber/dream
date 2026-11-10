#include "DeEsserEditor.h"

using namespace of;
using kit::u8;
namespace v3 = kit::v3;

// Spectre de la sibilance (2 à 20 kHz) : entrée, sortie, et zone de détection réglée par Fréquence et Largeur.
class DeScope : public juce::Component {
public:
    explicit DeScope(DeEsserProcessor& p) : proc(p) { setOpaque(true); }
    const v3::Theme* T = nullptr;   // nul ou Classique : rendu d'origine
    // v3 : posé dans l'écran héros (fond dessiné par l'éditeur), tracés lumineux.
    void paint3(juce::Graphics& g) {
        const float u = (float)getWidth() / 680.f; const auto& t = *T;
        auto r = getLocalBounds().toFloat().withTrimmedLeft(30 * u).withTrimmedBottom(20 * u).withTrimmedTop(26 * u).withTrimmedRight(6 * u);
        constexpr float f0 = 2000.f, f1 = 20000.f, top = -10.f, bot = -100.f;
        const float sr = (float)(proc.getSampleRate() > 0 ? proc.getSampleRate() : 48000.0);
        const auto P = proc.readParams();
        g.setColour(t.scrMute); v3::caps(g, P.listen ? u8("● Écoute de la sibilance") : u8("Spectre · entrée / sortie"), {4 * u, 0, 300 * u, 18 * u}, 11.f * u, juce::Justification::centredLeft);
        const Colour inC = t.scrMute.withAlpha(0.9f);
        const struct { const char* t; Colour c; } leg[] = {{"entrée", inC}, {"sortie", t.sung}, {"bande", t.accent}};
        float lx = 300.f * u;
        for (auto& l : leg) {
            g.setColour(l.c); g.fillRoundedRectangle(lx, 7.5f * u, 12 * u, 3 * u, 1.5f * u);
            g.setColour(t.scrInk.withAlpha(0.85f)); g.setFont(v3::font(12.f * u)); g.drawText(u8(l.t), Rectangle<float>(lx + 17 * u, 0, 70 * u, 18 * u), juce::Justification::centredLeft, false);
            lx += 82.f * u;
        }
        for (float d = bot + 15.f; d <= top + 0.1f; d += 15.f) {
            const float y = gfx::yOfDb(d, r, top, bot);
            g.setColour(juce::Colours::white.withAlpha(0.06f)); for (float x = r.getX(); x < r.getRight(); x += 6.f * u) g.fillRect(x, y - 0.5f, 2.f * u, 1.f);
            g.setColour(t.scrMute); g.setFont(v3::font(10.5f * u)); g.drawText(v3::fmt::num(d, 0), Rectangle<float>(0, y - 7 * u, 26 * u, 14 * u), juce::Justification::centredRight, false);
        }
        const std::vector<float> marks{2000, 3000, 4000, 5000, 6000, 8000, 10000, 12000, 16000, 20000};
        g.setFont(v3::font(10.5f * u));
        for (float f : marks) { g.setColour(t.scrMute); g.drawText(v3::fmt::num(f / 1000.f, 0) + "k", Rectangle<float>(gfx::xOf(f, r, f0, f1) - 16 * u, r.getBottom() + 3 * u, 32 * u, 14 * u), juce::Justification::centred, false); }
        juce::Graphics::ScopedSaveState st(g);
        g.reduceClipRegion(r.toNearestInt());
        const float half = std::pow(2.f, P.width / 2.f), xa = gfx::xOf(juce::jlimit(f0, f1, P.freq / half), r, f0, f1), xb = gfx::xOf(juce::jlimit(f0, f1, P.freq * half), r, f0, f1);
        g.setGradientFill(juce::ColourGradient(t.accent.withAlpha(P.listen ? 0.34f : 0.20f), 0, r.getY(), t.accent.withAlpha(0.02f), 0, r.getBottom(), false));
        g.fillRect(Rectangle<float>(xa, r.getY(), xb - xa, r.getHeight()));
        const float xc = gfx::xOf(P.freq, r, f0, f1);
        g.setColour(t.accent.withAlpha(0.25f)); g.fillRect(xc - 2.f * u, r.getY(), 4.f * u, r.getHeight());
        g.setColour(t.accent); g.fillRect(xc - 0.75f * u, r.getY(), 1.5f * u, r.getHeight());
        const auto pin = gfx::specPath(proc.inAn.db.data(), odsp::Analyzer::kBins, sr, r, top, bot, true, f0, f1);
        g.setGradientFill(juce::ColourGradient(inC.withAlpha(0.30f), 0, r.getY(), inC.withAlpha(0.03f), 0, r.getBottom(), false)); g.fillPath(pin);
        const auto pout = gfx::specPath(proc.outAn.db.data(), odsp::Analyzer::kBins, sr, r, top, bot, false, f0, f1);
        const juce::PathStrokeType sk(1.8f * u, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
        g.setColour(t.sung.withAlpha(0.08f)); g.strokePath(pout, juce::PathStrokeType(10.f * u, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(t.sung.withAlpha(0.18f)); g.strokePath(pout, juce::PathStrokeType(5.f * u, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(t.sung); g.strokePath(pout, sk);
        const auto ft = v3::fmt::hz(P.freq) + u8(" · ") + v3::fmt::num(P.width, 1) + " oct";
        const auto pill = Rectangle<float>(124 * u, 20 * u).withCentre({juce::jlimit(r.getX() + 64 * u, r.getRight() - 64 * u, xc), r.getY() + 14 * u});
        g.setColour(juce::Colour(0xCC0A0D1C)); g.fillRoundedRectangle(pill, 10 * u);
        g.setColour(t.out); g.setFont(v3::font(12.5f * u, true)); g.drawText(ft, pill, juce::Justification::centred, false);
    }
    void paint(juce::Graphics& g) override {
        if (v3::bridge::v3(T)) { paint3(g); return; }
        const float u = (float)getWidth() / 528.f; const auto pal = of::palette();
        auto well = getLocalBounds().toFloat(); kit::screenWell(g, pal, well, kit::Display::VFD);
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
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xFF8C8F99).withAlpha(0.42f), 0, r.getY(), juce::Colour(0xFF8C8F99).withAlpha(0.10f), 0, r.getBottom(), false)); g.fillPath(pin);
        g.setColour(juce::Colour(0xFF8C8F99).withAlpha(0.55f)); g.strokePath(gfx::specPath(proc.inAn.db.data(), odsp::Analyzer::kBins, sr, r, top, bot, false, f0, f1), juce::PathStrokeType(1.2f * u));
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
        const struct { const char* t; Colour c; } leg[] = {{"Sortie", kIce}, {"Entr\xC3\xA9" "e", juce::Colour(0xFF8C8F99)}};
        for (auto& l : leg) { const auto t = kit::upper(u8(l.t)); const float w = juce::GlyphArrangement::getStringWidth(kit::mono(8.f * u), t) + 4; lx -= w; g.setColour(pal.mute); g.drawText(t, Rectangle<float>(lx, r.getBottom() - 14 * u, w, 11 * u), juce::Justification::centredRight); lx -= 10 * u; g.setColour(l.c); g.fillRect(Rectangle<float>(lx, r.getBottom() - 9.5f * u, 8 * u, 2.4f * u)); lx -= 10 * u; }
    }
private:
    DeEsserProcessor& proc;
};

// Réduction de gain : valeur crête et historique défilant (environ 6 s).
class GrPanel : public juce::Component {
public:
    explicit GrPanel(DeEsserProcessor& p) : proc(p) { setOpaque(true); hist.fill(0.f); }
    void push(float v) { hist[(size_t)pos] = v; pos = (pos + 1) % (int)hist.size(); peak = juce::jmax(v, peak - 0.25f); repaint(); }
    const v3::Theme* T = nullptr;
    void paint3(juce::Graphics& g) {
        const float u = (float)getWidth() / 244.f; const auto& t = *T;
        g.setColour(t.scrMute); v3::caps(g, u8("Réduction de gain"), {4 * u, 0, 236 * u, 18 * u}, 11.f * u, juce::Justification::centredLeft);
        const String n = v3::fmt::num(peak > 0.05f ? -(double)peak : 0.0, 1);
        const auto big = v3::font(60.f * u, true);
        g.setColour(peak > 0.05f ? t.scrInk : t.scrInk.withAlpha(0.8f)); v3::tabular(g, n, {0, 22 * u, 240 * u, 70 * u}, big, juce::Justification::centredLeft);
        g.setColour(t.scrMute); g.setFont(v3::font(17.f * u)); g.drawText("dB", Rectangle<float>(v3::tabularWidth(n, big) + 6 * u, 62 * u, 40 * u, 22 * u), juce::Justification::centredLeft, false);
        const float scale = juce::jmax(6.f, std::ceil(proc.apvts.getRawParameterValue("range")->load() / 3.f) * 3.f);
        auto r = Rectangle<float>(4 * u, 108 * u, 232 * u, (float)getHeight() - 128 * u);
        for (int i = 0; i <= 3; ++i) { const float y = r.getY() + r.getHeight() * (float)i / 3.f; g.setColour(juce::Colours::white.withAlpha(0.06f)); for (float x = r.getX(); x < r.getRight(); x += 6.f * u) g.fillRect(x, y - 0.5f, 2.f * u, 1.f); }
        const int n2 = (int)hist.size();
        for (int i = 0; i < n2; ++i) lin[(size_t)i] = juce::jlimit(0.f, scale, hist[(size_t)((pos + i) % n2)]);
        tr.build(lin.data(), n2, [&](float i) { return r.getX() + r.getWidth() * i / (float)(n2 - 1); }, [&](float v) { return r.getY() + r.getHeight() * v / scale; }, r.getY(), {180, 1.2f, 1e9f, -1.f, 6});
        tr.draw(g, t.out, 1.8f * u, 1.f, 0.30f, r.getBottom(), true);
        g.setColour(t.scrMute); g.setFont(v3::font(10.5f * u));
        g.drawText("0", Rectangle<float>(r.getX(), r.getBottom() + 2 * u, 30 * u, 14 * u), juce::Justification::centredLeft, false);
        g.drawText(v3::fmt::minus() + String((int)scale) + u8(" dB · 6 s"), Rectangle<float>(r.getRight() - 120 * u, r.getBottom() + 2 * u, 120 * u, 14 * u), juce::Justification::centredRight, false);
    }
    void paint(juce::Graphics& g) override {
        if (v3::bridge::v3(T)) { paint3(g); return; }
        const float u = (float)getWidth() / 156.f; const auto pal = of::palette();
        auto well = getLocalBounds().toFloat(); kit::screenWell(g, pal, well);
        g.setColour(pal.mute); kit::tracked(g, u8("R\xC3\xA9" "duction de gain"), Rectangle<float>(10 * u, 8 * u, 136 * u, 10 * u), juce::Justification::centredLeft, 1.2f * u, kit::fnt(7.2f * u, true));
        g.setColour(peak > 0.05f ? kPeach : pal.screenInk); g.setFont(kit::mono(22.f * u));
        g.drawText(peak > 0.05f ? juce::String::fromUTF8("\xE2\x88\x92") + String(peak, 1) : String("0.0"), Rectangle<float>(6 * u, 20 * u, 144 * u, 28 * u), juce::Justification::centred);
        g.setColour(pal.mute); g.setFont(kit::mono(8.f * u)); g.drawText("dB", Rectangle<float>(100 * u, 48 * u, 48 * u, 10 * u), juce::Justification::centredRight);
        auto r = Rectangle<float>(12 * u, 64 * u, 104 * u, well.getHeight() - 76 * u);
        {   // barre-graphe vertical de réduction (LED, 0 en haut)
            const auto bg = Rectangle<float>(126 * u, 64 * u, 18 * u, well.getHeight() - 76 * u);
            const int n = 16; const float sh = bg.getHeight() / (float)n;
            const float sc = juce::jmax(6.f, std::ceil(proc.apvts.getRawParameterValue("range")->load() / 3.f) * 3.f);
            for (int i = 0; i < n; ++i) {
                const bool on = peak / sc > (float)i / (float)n + 0.001f;
                const auto c = i >= n * 3 / 4 ? juce::Colour(0xFFFF4A3A) : i >= n / 2 ? juce::Colour(0xFFFFB02E) : kPeach;
                g.setColour(on ? c : c.withMultipliedBrightness(0.25f).withAlpha(0.6f));
                g.fillRoundedRectangle(bg.getX(), bg.getY() + sh * (float)i + 0.8f, bg.getWidth(), sh - 1.6f, 1.f);
            }
        }
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
    DeEsserProcessor& proc; std::array<float, 180> hist, lin {}; int pos = 0; float peak = 0.f;
    v3::Trace tr;
};

DeEsserEditor::DeEsserEditor(DeEsserProcessor& p)
    : AudioProcessorEditor(&p), proc(p), pal(of::palette(kPeach)), look(pal), power(pal), listenKey(u8("\xC3\x89" "couter la sibilance"), pal, kPeach) {
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
    for (auto& d : defs) { auto* pt = pots.add(new Pot(proc.apvts, d.id, u8(d.lab), u8(d.tip), pal)); pt->setStyle(kit::KnobStyle::Cap); addAndMakeVisible(pt); }
    modeSeg = std::make_unique<v3::bridge::Seg>(proc.apvts, "mode", juce::StringArray{"Split", "Large bande"}, u8("Split : seule la bande de sibilance est att\xC3\xA9nu\xC3\xA9" "e. Large bande : tout le signal baisse (plus fort, moins naturel)."), pal);
    styleSeg = std::make_unique<v3::bridge::Seg>(proc.apvts, "style", juce::StringArray{"Doux", "Net"}, u8("Doux : genou large, r\xC3\xA9" "duction progressive. Net : r\xC3\xA9" "duction franche d\xC3\xA8s le seuil."), pal);
    addAndMakeVisible(*modeSeg); addAndMakeVisible(*styleSeg); addAndMakeVisible(listenKey);
    listenKey.setTooltip(u8("N'\xC3\xA9" "coute que la bande de d\xC3\xA9tection pour r\xC3\xA9gler Fr\xC3\xA9quence et Largeur sur les \xC2\xAB s \xC2\xBB."));
    listenAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, "listen", listenKey);
    addAndMakeVisible(power);
    power.setTooltip(u8("Effet en service. Bascule pour le couper (la latence reste compens\xC3\xA9" "e : lookahead 1 ms)."));
    // --- v3 : potards haut de gamme (mêmes paramètres, attachements propres), touche d'écoute, thème
    const char* ids3[6] = {"freq", "width", "thr", "range", "atk", "rel"};
    for (int i = 0; i < 6; ++i) {
        auto* k = k3.add(new v3::Knob3(proc.apvts, ids3[i], u8(defs[i].lab), u8(defs[i].tip), v3::KnobSize::M));
        addChildComponent(k);
    }
    k3[0]->setFormatter([](double v) { return v3::fmt::hz(v); });
    listen3.setButtonText(u8("Écouter la sibilance")); listen3.setClickingTogglesState(true);
    listen3.setTooltip(listenKey.getTooltip());
    addChildComponent(listen3);
    listen3Att = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, "listen", listen3);
    addAndMakeVisible(themeBtn);
    themeBtn.onTheme = [this](v3::Mode m) { setThemeMode(m, true); };
    themeBtn.onScale = [this](float s) { setUiScale(s); };
    help.install(*this, "Orbe DeEsser", {u8("Att\xC3\xA9nue les sibilances (\xC2\xAB s \xC2\xBB, \xC2\xAB ch \xC2\xBB) d'une voix."),
        u8("- Fr\xC3\xA9quence et Largeur placent la bande de d\xC3\xA9tection (zone p\xC3\xAA" "che sur l'\xC3\xA9" "cran). Voix d'homme : 5-7 kHz ; voix de femme : 7-9 kHz."),
        u8("- \xC3\x89" "couter la sibilance n'envoie que la bande d\xC3\xA9tect\xC3\xA9" "e : r\xC3\xA8gle-la sur les \xC2\xAB s \xC2\xBB, puis coupe l'\xC3\xA9" "coute."),
        u8("- Seuil : niveau \xC3\xA0 partir duquel on att\xC3\xA9nue ; Plage : r\xC3\xA9" "duction maximale."),
        u8("- Split n'att\xC3\xA9nue que la bande ; Large bande baisse tout le signal. Doux / Net : forme du genou."),
        u8("- \xC3\x89" "cran : entr\xC3\xA9" "e en gris, sortie en trait clair. \xC3\x80 droite : r\xC3\xA9" "duction de gain (cr\xC3\xAA" "te et historique)."),
        u8("- Lookahead 1 ms : la latence est d\xC3\xA9" "clar\xC3\xA9" "e \xC3\xA0 l'h\xC3\xB4te et compens\xC3\xA9" "e."),
        u8("- Bouton demi-disque : thème Clair, Sombre ou Classique et taille de la fenêtre (mémorisés).")});
    powerAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, "bypass", power);
    setResizable(true, true);
    scale_ = v3::Prefs::scale("deesser", 1.f);
    setThemeMode(v3::Prefs::theme("deesser", v3::Mode::Light), false);
    startTimerHz(30);
}
DeEsserEditor::~DeEsserEditor() { stopTimer(); v3::Prefs::setScale("deesser", scale_); setLookAndFeel(nullptr); }

void DeEsserEditor::setThemeMode(v3::Mode m, bool persist) {
    mode = m;
    T = v3::make(m == v3::Mode::Classic ? v3::Mode::Light : m, v3::accents::peach);
    T.mode = m;
    look3.setTheme(T);
    art.clear();
    const bool n3 = isV3();
    setLookAndFeel(n3 ? static_cast<juce::LookAndFeel*>(&look3) : static_cast<juce::LookAndFeel*>(&look));
    for (auto* p : pots) p->setVisible(!n3);
    for (auto* k : k3) { k->setVisible(n3); k->setup(&T, &art); }
    listenKey.setVisible(!n3); listen3.setVisible(n3); listen3.T = &T;
    scope->T = &T; grPanel->T = &T; scope->setOpaque(!n3); grPanel->setOpaque(!n3);
    modeSeg->T = styleSeg->T = &T; modeSeg->px = styleSeg->px = 12.f;
    power.T = &T; help.T = &T; themeBtn.T = &T; themeBtn.current = m;
    if (persist) v3::Prefs::setTheme("deesser", m);
    const float bw = n3 ? kW3 : kW, bh = n3 ? kH3 : kH, lo = n3 ? 0.75f : 0.5f, hi = n3 ? 1.5f : 2.f;
    setResizeLimits(juce::roundToInt(bw * lo), juce::roundToInt(bh * lo), juce::roundToInt(bw * hi), juce::roundToInt(bh * hi));
    getConstrainer()->setFixedAspectRatio((double)bw / bh);
    scale_ = juce::jlimit(lo, hi, scale_);
    const int w = juce::roundToInt(bw * scale_), h = juce::roundToInt(bh * scale_);
    if (getWidth() == w && getHeight() == h) resized(); else setSize(w, h);
    sendLookAndFeelChange();
    repaint();
}

void DeEsserEditor::setUiScale(float s) {
    const bool n3 = isV3();
    scale_ = juce::jlimit(n3 ? 0.75f : 0.5f, n3 ? 1.5f : 2.f, s);
    setSize(juce::roundToInt((n3 ? kW3 : kW) * scale_), juce::roundToInt((n3 ? kH3 : kH) * scale_));
    v3::Prefs::setScale("deesser", scale_);
}

void DeEsserEditor::resized() {
    if (isV3()) {
        u_ = (float)getWidth() / kW3; scale_ = u_; themeBtn.scale = scale_;
        themeBtn.setBounds(Si(800, 21, 28, 28));
        help.layout(Si(838, 20, 30, 30), getLocalBounds());
        power.setBounds(Si(884, 22, 96, 26));
        scope->setBounds(Si(34, 90, 680, 232)); grPanel->setBounds(Si(736, 90, 230, 232));
        const float kh = v3::Knob3::boxHeight(v3::KnobSize::M, false);
        const float kx[6] = {98, 246, 394, 542, 742, 890};
        for (int i = 0; i < 6; ++i) { k3[i]->setScale(u_); k3[i]->setBounds(Si(kx[i] - 64, 382, 128, kh)); }
        modeSeg->setBounds(Si(170, 538, 230, 40)); styleSeg->setBounds(Si(416, 538, 170, 40)); listen3.setBounds(Si(640, 538, 324, 40));
        return;
    }
    u_ = (float)getWidth() / kW;
    themeBtn.scale = u_ / 1.f;
    scale_ = u_;
    const auto content = getLocalBounds().toFloat().reduced(18.f * u_, 0);
    power.setBounds(kit::w::inSlot(content, u_).toNearestInt());
    help.layout(kit::w::helpSlot(content, u_, true).toNearestInt(), getLocalBounds());
    themeBtn.setBounds(help.getBounds().translated(-help.getWidth() - juce::roundToInt(6.f * u_), 0));
    scope->setBounds(Si(32, 66, 528, 206)); grPanel->setBounds(Si(572, 66, 156, 206));
    const float px[6] = {32, 148, 264, 380, 496, 612};
    for (int i = 0; i < 6; ++i) pots[i]->setBounds(Si(px[i], 308, 116, 78));
    modeSeg->setBounds(Si(32, 418, 190, 28)); styleSeg->setBounds(Si(238, 418, 150, 28)); listenKey.setBounds(Si(404, 418, 324, 28));
}

void DeEsserEditor::timerCallback() {
    proc.inAn.compute(); proc.outAn.compute();
    grPanel->push(proc.gr.load());
    scope->repaint();
    if (isV3()) for (auto* k : k3) k->tickMod(1.f / 30.f);
}

void DeEsserEditor::paintBackdrop3(juce::Graphics& g, juce::Rectangle<float> b) {
    const float u = b.getWidth() / kW3;
    auto R = [u](float x, float y, float w, float h) { return juce::Rectangle<float>(x * u, y * u, w * u, h * u); };
    v3::background(g, T, b);
    g.setColour(T.inkMute); v3::caps(g, u8("dreamaws · orbe"), R(27, 14, 200, 16), 12.f * u, juce::Justification::centredLeft);
    g.setColour(T.accentText); kit::tracked(g, "DeEsser", R(26, 28, 260, 32), juce::Justification::centredLeft, 2.f * u, v3::font(34.f * u, true));
    g.setColour(T.accent); g.fillRoundedRectangle(R(28, 61, 30, 3), 1.5f * u);
    v3::hero(g, T, R(20, 74, 960, 262), 20.f * u);
    g.setColour(juce::Colours::white.withAlpha(0.10f)); g.fillRect(R(724, 90, 1, 230));
    v3::card(g, T, R(20, 350, 580, 170), 16.f * u);
    v3::card(g, T, R(612, 350, 368, 170), 16.f * u);
    v3::card(g, T, R(20, 530, 960, 56), 16.f * u);
    g.setColour(T.ink);
    v3::caps(g, u8("Détection · bande de sibilance"), R(38, 362, 400, 18), 12.f * u, juce::Justification::centredLeft);
    v3::caps(g, "Dynamique", R(630, 362, 300, 18), 12.f * u, juce::Justification::centredLeft);
    v3::caps(g, u8("Mode · écoute"), R(38, 549, 130, 18), 12.f * u, juce::Justification::centredLeft);
}

void DeEsserEditor::paint(juce::Graphics& g) {
    if (isV3()) { art.draw(g, juce::String("bd|") + v3::modeKey(mode), getLocalBounds().toFloat(), [this](juce::Graphics& ig, juce::Rectangle<float> b) { paintBackdrop3(ig, b); }); return; }
    const float u = u_;
    kit::w::drawFace(g, pal, getLocalBounds(), u, {kit::w::Family::Rack, "Orbe DeEsser", kit::w::serial::deesser});
    section(g, pal, u, u8("D\xC3\xA9tection \xC2\xB7 bande de sibilance"), 32, 286, 464);
    section(g, pal, u, u8("Dynamique"), 496, 286, 232);
    section(g, pal, u, u8("Mode \xC2\xB7 \xC3\xA9" "coute"), 32, 396, 696);
}
