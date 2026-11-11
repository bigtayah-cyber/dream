#include "DenoiserEditor.h"

using namespace of;
using kit::u8;
namespace v3 = kit::v3;

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
        {   // réduction moyenne réellement appliquée (bins où il y a du signal)
            float s = 0.f; int c = 0;
            for (int k = 2; k < dn::kBins; ++k) if (inS[(size_t)k] > -90.f) { s += juce::jmax(0.f, inS[(size_t)k] - outS[(size_t)k]); ++c; }
            red = c > 0 ? s / (float)c : 0.f;
        }
        repaint();
    }
    const v3::Theme* T = nullptr;   // nul ou Classique : rendu d'origine
    float red = 0.f;
    bool isLearning() const { return learning; }
    bool isKnown() const { return known; }
    float progress() const { return learnP; }
    void paint3(juce::Graphics& g) {
        const float u = (float)getWidth() / 700.f; const auto& t = *T;
        auto r = getLocalBounds().toFloat().withTrimmedLeft(30 * u).withTrimmedBottom(20 * u).withTrimmedTop(26 * u).withTrimmedRight(6 * u);
        constexpr float top = -10.f, bot = -110.f;
        g.setColour(t.scrMute); v3::caps(g, u8("Spectre · bruit"), {4 * u, 0, 200 * u, 18 * u}, 11.f * u, juce::Justification::centredLeft);
        const Colour inC = t.scrMute.withAlpha(0.9f);
        const struct { const char* t; Colour c; } leg[] = {{"entrée", inC}, {"sortie", t.sung}, {"bruit appris / estimé", t.out}};
        float lx = 250.f * u;
        for (auto& l : leg) {
            g.setColour(l.c); g.fillRoundedRectangle(lx, 7.5f * u, 12 * u, 3 * u, 1.5f * u);
            g.setColour(t.scrInk.withAlpha(0.85f)); g.setFont(v3::font(12.f * u)); g.drawText(u8(l.t), Rectangle<float>(lx + 17 * u, 0, 150 * u, 18 * u), juce::Justification::centredLeft, false);
            lx += (l.t[0] == 'b' ? 170.f : 82.f) * u;
        }
        for (float d = bot + 20.f; d <= top + 0.1f; d += 20.f) {
            const float y = gfx::yOfDb(d, r, top, bot);
            g.setColour(juce::Colours::white.withAlpha(0.06f)); for (float x = r.getX(); x < r.getRight(); x += 6.f * u) g.fillRect(x, y - 0.5f, 2.f * u, 1.f);
            g.setColour(t.scrMute); g.setFont(v3::font(10.5f * u)); g.drawText(v3::fmt::num(d, 0), Rectangle<float>(0, y - 7 * u, 26 * u, 14 * u), juce::Justification::centredRight, false);
        }
        for (float f : {20.f, 50.f, 100.f, 200.f, 500.f, 1000.f, 2000.f, 5000.f, 10000.f, 20000.f}) {
            g.setColour(t.scrMute); g.setFont(v3::font(10.5f * u));
            g.drawText(f >= 1000.f ? v3::fmt::num(f / 1000.f, 0) + "k" : v3::fmt::num(f, 0), Rectangle<float>(gfx::xOf(f, r) - 16 * u, r.getBottom() + 3 * u, 32 * u, 14 * u), juce::Justification::centred, false);
        }
        juce::Graphics::ScopedSaveState st(g);
        g.reduceClipRegion(r.toNearestInt());
        auto pin = gfx::specPath(inS.data(), dn::kBins, sr, r, top, bot, true);
        g.setGradientFill(juce::ColourGradient(inC.withAlpha(0.30f), 0, r.getY(), inC.withAlpha(0.03f), 0, r.getBottom(), false)); g.fillPath(pin);
        if (known || proc.engine.latency() > 0) {
            juce::Path pn = gfx::specPath(noiseS.data(), dn::kBins, sr, r, top, bot, false);
            juce::Path dashed; const float dl[] = {6.f * u, 4.f * u};
            juce::PathStrokeType(1.8f * u).createDashedStroke(dashed, pn, dl, 2);
            g.setColour(t.out.withAlpha(0.18f)); g.strokePath(pn, juce::PathStrokeType(6.f * u, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour(t.out); g.fillPath(dashed);
        }
        const auto pout = gfx::specPath(outS.data(), dn::kBins, sr, r, top, bot, false);
        g.setColour(t.sung.withAlpha(0.08f)); g.strokePath(pout, juce::PathStrokeType(10.f * u, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(t.sung.withAlpha(0.18f)); g.strokePath(pout, juce::PathStrokeType(5.f * u, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(t.sung); g.strokePath(pout, juce::PathStrokeType(1.8f * u, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    void paint(juce::Graphics& g) override {
        if (v3::bridge::v3(T)) { paint3(g); return; }
        const float u = (float)getWidth() / 744.f;
        auto well = getLocalBounds().toFloat();
        kit::screenWell(g, pal, well, kit::Display::VFD);
        auto r = well.reduced(10 * u).withTrimmedLeft(24 * u).withTrimmedBottom(12 * u).withTrimmedTop(6 * u);
        constexpr float top = -10.f, bot = -110.f;
        g.saveState(); g.reduceClipRegion(r.toNearestInt());
        for (float d = bot; d <= top + 0.1f; d += 20.f) { g.setColour(juce::Colours::white.withAlpha(0.07f)); g.drawHorizontalLine(juce::roundToInt(gfx::yOfDb(d, r, top, bot)), r.getX(), r.getRight()); }
        gfx::freqGrid(g, r, u, 20.f, 20000.f, {20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000}, false);
        auto pin = gfx::specPath(inS.data(), dn::kBins, sr, r, top, bot, true);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xFF8C8F99).withAlpha(0.42f), 0, r.getY(), juce::Colour(0xFF8C8F99).withAlpha(0.10f), 0, r.getBottom(), false)); g.fillPath(pin);
        g.setColour(juce::Colour(0xFF8C8F99).withAlpha(0.55f)); g.strokePath(gfx::specPath(inS.data(), dn::kBins, sr, r, top, bot, false), juce::PathStrokeType(1.1f * u));
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
        const struct { const char* t; Colour c; } leg[] = {{"Bruit appris / estim\xC3\xA9", kPeach}, {"Sortie", kIce}, {"Entr\xC3\xA9" "e", juce::Colour(0xFF8C8F99)}};
        for (auto& l : leg) {
            const auto t = u8(l.t); const float w = juce::GlyphArrangement::getStringWidth(kit::mono(8.f * u), upper(t)) + 4;
            lx -= w; g.setColour(pal.mute); g.setFont(kit::mono(8.f * u)); g.drawText(upper(t), Rectangle<float>(lx, r.getY() + 3 * u, w, 11 * u), juce::Justification::centredRight);
            lx -= 10 * u; g.setColour(l.c); g.fillRect(Rectangle<float>(lx, r.getY() + 7.5f * u, 8 * u, 2.4f * u)); lx -= 12 * u;
        }
        g.setFont(kit::mono(9.f * u));
        g.setColour(learning ? kPeach : pal.screenInk.withAlpha(0.8f));
        g.drawText(learning ? u8("APPRENTISSAGE DU BRUIT\xE2\x80\xA6 ") + String(juce::roundToInt(learnP * 100.f)) + " %" : known ? u8("PROFIL DE BRUIT APPRIS") : u8("AUCUN PROFIL \xC2\xB7 MODE ADAPTATIF"),
                   Rectangle<float>(r.getX() + 6 * u, r.getY() + 3 * u, 300 * u, 11 * u), juce::Justification::centredLeft);
    }
    kit::Palette pal = of::palette(kIce);
private:
    static String upper(const String& s) { return kit::upper(s); }
    DenoiserProcessor& proc;
    float sr = 48000.f, learnP = 0.f; bool learning = false, known = false;
    std::array<float, dn::kBins> inS, outS, noiseS;
};

DenoiserEditor::DenoiserEditor(DenoiserProcessor& p)
    : AudioProcessorEditor(&p), proc(p), pal(of::palette(kIce)), look(pal), power(pal),
      learnKey(u8("Learn"), pal, juce::Colour(0xFFFF3B30)), adaptKey("Adapt", pal, juce::Colour(0xFFFFB02E)), listenKey(u8("Listen"), pal, kIce) {
    setLookAndFeel(&look);
    scope = std::make_unique<DnScope>(proc); addAndMakeVisible(*scope);
    struct D { const char* id; const char* lab; const char* tip; };
    const D defs[] = {
        {"red", "R\xC3\xA9" "duction", "Att\xC3\xA9nuation maximale appliqu\xC3\xA9" "e au bruit (0 \xC3\xA0 24 dB)."},
        {"sens", "Seuil", "Sensibilit\xC3\xA9 : plus haut, plus de bruit est consid\xC3\xA9r\xC3\xA9 comme tel (risque d'entamer la voix)."},
        {"smooth", "Lissage", "Lissage temps/fr\xC3\xA9quence du gain : plus haut, moins de \xC2\xAB bruit musical \xC2\xBB mais attaques plus douces."},
        {"air", "Aigus", "Pr\xC3\xA9servation des aigus et du souffle d'air : limite la r\xC3\xA9" "duction au-dessus de 4 kHz."}};
    for (auto& d : defs) { auto* pt = pots.add(new Pot(proc.apvts, d.id, u8(d.lab), u8(d.tip), pal)); pt->setStyle(kit::KnobStyle::Fluted); addAndMakeVisible(pt); }
    humSeg = std::make_unique<v3::bridge::Seg>(proc.apvts, "hum", juce::StringArray{u8("Coup\xC3\xA9"), "50 Hz", "60 Hz"}, u8("Filtre anti-ronflement du secteur (fondamentale et 3 harmoniques)."), pal);
    addAndMakeVisible(*humSeg);
    for (auto* k : {&learnKey, &adaptKey, &listenKey}) addAndMakeVisible(*k);
    learnKey.setClickingTogglesState(false);
    learnKey.setTooltip(u8("Joue 1 \xC3\xA0 3 s de silence (bruit seul) : le profil du bruit est m\xC3\xA9moris\xC3\xA9 (environ 2 s) et enregistr\xC3\xA9 avec la session."));
    learnKey.onClick = [this] { proc.learn(); };
    adaptKey.setTooltip(u8("Suit en continu le plancher de bruit (minima statistiques). Un son tenu et parfaitement stable peut \xC3\xAAtre pris pour du bruit."));
    listenKey.setTooltip(u8("N'\xC3\xA9" "coute que ce qui est retir\xC3\xA9 : r\xC3\xA8gle le seuil jusqu'\xC3\xA0 n'entendre aucune voix."));
    addAndMakeVisible(power);
    power.setTooltip(u8("Effet en service. Bascule pour le couper (la latence reste compens\xC3\xA9" "e)."));
    // --- v3 : potards haut de gamme et touches (mêmes paramètres, attachements propres), thème
    const char* ids3[4] = {"red", "sens", "smooth", "air"};
    for (int i = 0; i < 4; ++i) addChildComponent(k3.add(new v3::Knob3(proc.apvts, ids3[i], u8(defs[i].lab), u8(defs[i].tip), v3::KnobSize::M)));
    learn3.setButtonText("Learn"); adapt3.setButtonText("Adapt"); listen3.setButtonText("Listen");
    learn3.setTooltip(learnKey.getTooltip()); adapt3.setTooltip(adaptKey.getTooltip()); listen3.setTooltip(listenKey.getTooltip());
    adapt3.setClickingTogglesState(true); listen3.setClickingTogglesState(true);
    learn3.onClick = [this] { proc.learn(); };
    for (auto* k : {&learn3, &adapt3, &listen3}) addChildComponent(*k);
    adapt3Att = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, "adapt", adapt3);
    listen3Att = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, "listen", listen3);
    {   // v3 seulement : trois curseurs et le mode Prise
        const char* ids[3] = {"gap", "blo", "bhi"};
        const char* labs[3] = {"Entre les phrases", "R\xC3\xA9" "duction graves", "R\xC3\xA9" "duction aigus"};
        const char* tips[3] = {"R\xC3\xA9" "duction suppl\xC3\xA9mentaire quand il n'y a pas de voix (silences entre les phrases). 0 : coup\xC3\xA9.",
                               "Ajoute ou retire de la r\xC3\xA9" "duction sous 200 Hz (le potard R\xC3\xA9" "duction reste le ma\xC3\xAEtre).",
                               "Ajoute ou retire de la r\xC3\xA9" "duction au-dessus de 5 kHz."};
        for (int i = 0; i < 3; ++i) {
            extraS[i].setSliderStyle(juce::Slider::LinearHorizontal); extraS[i].setTextBoxStyle(juce::Slider::TextBoxRight, false, 78, 20);
            extraS[i].setTooltip(u8(tips[i])); extraL[i].setText(u8(labs[i]), juce::dontSendNotification); extraL[i].setJustificationType(juce::Justification::centredLeft);
            addChildComponent(extraS[i]); addChildComponent(extraL[i]);
            extraAtt[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, ids[i], extraS[i]);
        }
        liveBtn.setButtonText(u8("Mode Prise (latence 11 ms)")); addChildComponent(liveBtn);
        liveBtn.setTooltip(u8("Faible latence pour enregistrer : FFT 512 (au lieu de 2048), latence d'environ 11 ms, r\xC3\xA9" "duction plafonn\xC3\xA9" "e \xC3\xA0 12 dB. L'h\xC3\xB4te est pr\xC3\xA9venu du changement de latence."));
        liveAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, "live", liveBtn);
    }
    addAndMakeVisible(themeBtn);
    themeBtn.onTheme = [this](v3::Mode m) { setThemeMode(m, true); };
    themeBtn.onScale = [this](float s) { setUiScale(s); };
    help.install(*this, "Orbe Denoiser", {u8("R\xC3\xA9" "duit le bruit de fond et le souffle d'une voix (soustraction spectrale de Wiener liss\xC3\xA9" "e)."),
        u8("- LEARN : joue 1 \xC3\xA0 3 s de bruit seul (sans voix) ; le profil est m\xC3\xA9moris\xC3\xA9 en 2 s environ et enregistr\xC3\xA9 avec la session."),
        u8("- ADAPT : suit le plancher de bruit en continu, sans apprentissage. Un son tenu et tr\xC3\xA8s stable peut \xC3\xAAtre pris pour du bruit."),
        u8("- LISTEN : n'\xC3\xA9" "coute que ce qui est retir\xC3\xA9 ; monte le Seuil jusqu'\xC3\xA0 ce qu'aucune voix n'y passe."),
        u8("- R\xC3\xA9" "duction : att\xC3\xA9nuation maximale ; Lissage : moins de \xC2\xAB bruit musical \xC2\xBB ; Aigus : pr\xC3\xA9serve l'air au-dessus de 4 kHz."),
        u8("- Anti-ronflement : filtre le secteur \xC3\xA0 50 ou 60 Hz (et 3 harmoniques)."),
        u8("- Latence ") + String(dn::kLatency) + u8(" \xC3\xA9" "chantillons (FFT 2048, saut 512), d\xC3\xA9" "clar\xC3\xA9" "e \xC3\xA0 l'h\xC3\xB4te ; Mode Prise : 512 \xC3\xA9" "chantillons."),
        u8("- Entre les phrases : r\xC3\xA9" "duction en plus hors voix. R\xC3\xA9" "duction graves / aigus : r\xC3\xA9glage par bande (v3)."),
        u8("- Bouton demi-disque : thème Clair, Sombre ou Classique et taille de la fenêtre (mémorisés).")});
    powerAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, "bypass", power);
    adaptAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, "adapt", adaptKey);
    listenAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, "listen", listenKey);
    setResizable(true, true);
    scale_ = v3::Prefs::scale("denoiser", 1.f);
    setThemeMode(v3::Prefs::theme("denoiser", v3::Mode::Light), false);
    startTimerHz(30);
}
DenoiserEditor::~DenoiserEditor() { stopTimer(); v3::Prefs::setScale("denoiser", scale_); setLookAndFeel(nullptr); }

void DenoiserEditor::setThemeMode(v3::Mode m, bool persist) {
    mode = m;
    T = v3::make(m == v3::Mode::Classic ? v3::Mode::Light : m, v3::accents::ice);
    T.mode = m;
    look3.setTheme(T);
    art.clear();
    const bool n3 = isV3();
    setLookAndFeel(n3 ? static_cast<juce::LookAndFeel*>(&look3) : static_cast<juce::LookAndFeel*>(&look));
    for (auto* p : pots) p->setVisible(!n3);
    for (auto* k : k3) { k->setVisible(n3); k->setup(&T, &art); }
    for (auto* k : {&learnKey, &adaptKey, &listenKey}) k->setVisible(!n3);
    for (auto* k : {&learn3, &adapt3, &listen3}) { k->setVisible(n3); k->T = &T; }
    for (auto& c : extraS) c.setVisible(n3);
    for (auto& c : extraL) c.setVisible(n3);
    liveBtn.setVisible(n3);
    scope->T = &T; scope->setOpaque(!n3);
    humSeg->T = &T; humSeg->px = 12.f;
    power.T = &T; help.T = &T; themeBtn.T = &T; themeBtn.current = m;
    if (persist) v3::Prefs::setTheme("denoiser", m);
    const float bw = n3 ? kW3 : kW, bh = n3 ? kH3 : kH, lo = n3 ? 0.75f : 0.5f, hi = n3 ? 1.5f : 2.f;
    setResizeLimits(juce::roundToInt(bw * lo), juce::roundToInt(bh * lo), juce::roundToInt(bw * hi), juce::roundToInt(bh * hi));
    getConstrainer()->setFixedAspectRatio((double)bw / bh);
    scale_ = juce::jlimit(lo, hi, scale_);
    const int w = juce::roundToInt(bw * scale_), h = juce::roundToInt(bh * scale_);
    if (getWidth() == w && getHeight() == h) resized(); else setSize(w, h);
    sendLookAndFeelChange();
    repaint();
}

void DenoiserEditor::setUiScale(float s) {
    const bool n3 = isV3();
    scale_ = juce::jlimit(n3 ? 0.75f : 0.5f, n3 ? 1.5f : 2.f, s);
    setSize(juce::roundToInt((n3 ? kW3 : kW) * scale_), juce::roundToInt((n3 ? kH3 : kH) * scale_));
    v3::Prefs::setScale("denoiser", scale_);
}

void DenoiserEditor::resized() {
    if (isV3()) {
        u_ = (float)getWidth() / kW3; scale_ = u_; themeBtn.scale = scale_;
        themeBtn.setBounds(Si(800, 21, 28, 28));
        help.layout(Si(838, 20, 30, 30), getLocalBounds());
        power.setBounds(Si(884, 22, 96, 26));
        scope->setBounds(Si(34, 90, 700, 232));
        learn3.setBounds(Si(38, 392, 98, 44)); adapt3.setBounds(Si(146, 392, 98, 44)); listen3.setBounds(Si(254, 392, 98, 44));
        humSeg->setBounds(Si(38, 500, 314, 40));
        liveBtn.setBounds(Si(38, 450, 314, 30));
        for (int i = 0; i < 3; ++i) { extraL[i].setBounds(Si(396.f + 202.f * (float)i, 520, 190, 18)); extraS[i].setBounds(Si(396.f + 202.f * (float)i, 540, 190, 24)); }
        const float kh = v3::Knob3::boxHeight(v3::KnobSize::M, false);
        for (int i = 0; i < 4; ++i) { k3[i]->setScale(u_); k3[i]->setBounds(Si(448.f + 152.f * (float)i - 70.f, 410, 140, kh)); }
        return;
    }
    u_ = (float)getWidth() / kW;
    scale_ = u_; themeBtn.scale = u_;
    const auto content = getLocalBounds().toFloat().reduced(18.f * u_, 0);
    power.setBounds(kit::w::inSlot(content, u_).toNearestInt());
    help.layout(kit::w::helpSlot(content, u_, true).toNearestInt(), getLocalBounds());
    themeBtn.setBounds(help.getBounds().translated(-help.getWidth() - juce::roundToInt(6.f * u_), 0));
    scope->setBounds(Si(32, 64, 744, 206));
    for (int i = 0; i < pots.size(); ++i) pots[i]->setBounds(Si(331.f + 111.f * (float)i, 312, 111, 78));
    learnKey.setBounds(Si(32, 314, 90, 40)); adaptKey.setBounds(Si(127, 314, 90, 40)); listenKey.setBounds(Si(222, 314, 90, 40));
    humSeg->setBounds(Si(152, 368, 160, 26));
}

void DenoiserEditor::timerCallback() {
    scope->update((float)(proc.getSampleRate() > 0 ? proc.getSampleRate() : 48000.0));
    const bool l = proc.engine.learning();
    learnKey.setToggleState(l, juce::dontSendNotification);
    learnKey.fill = l ? proc.engine.learnProgress() : 0.f;
    learnKey.setButtonText(l ? String(juce::roundToInt(proc.engine.learnProgress() * 100.f)) + " %" : u8("Learn"));
    learnKey.repaint();
    if (isV3()) {
        learn3.lit = l;
        learn3.setButtonText(l ? v3::fmt::pct(proc.engine.learnProgress()) : String("Learn"));
        learn3.repaint();
        redSm += (scope->red - redSm) * 0.25f;
        for (auto* k : k3) k->tickMod(1.f / 30.f);
        repaint(Si(740, 86, 236, 240));
    }
}

void DenoiserEditor::paintBackdrop3(juce::Graphics& g, juce::Rectangle<float> b) {
    const float u = b.getWidth() / kW3;
    auto R = [u](float x, float y, float w, float h) { return juce::Rectangle<float>(x * u, y * u, w * u, h * u); };
    v3::background(g, T, b);
    g.setColour(T.inkMute); v3::caps(g, u8("dreamaws · orbe"), R(27, 14, 200, 16), 12.f * u, juce::Justification::centredLeft);
    g.setColour(T.accentText); kit::tracked(g, "Denoiser", R(26, 28, 260, 32), juce::Justification::centredLeft, 2.f * u, v3::font(34.f * u, true));
    g.setColour(T.accent); g.fillRoundedRectangle(R(28, 61, 30, 3), 1.5f * u);
    v3::hero(g, T, R(20, 74, 960, 262), 20.f * u);
    g.setColour(juce::Colours::white.withAlpha(0.10f)); g.fillRect(R(746, 90, 1, 230));
    v3::card(g, T, R(20, 350, 350, 230), 16.f * u);
    v3::card(g, T, R(382, 350, 598, 230), 16.f * u);
    g.setColour(T.ink);
    v3::caps(g, u8("Analyse du bruit"), R(38, 364, 300, 18), 12.f * u, juce::Justification::centredLeft);
    v3::caps(g, u8("Réduction · spectre Wiener lissé"), R(400, 364, 500, 18), 12.f * u, juce::Justification::centredLeft);
    g.setColour(T.inkMute); v3::caps(g, "Anti-ronflement", R(38, 474, 300, 18), 11.f * u, juce::Justification::centredLeft);
}

void DenoiserEditor::paint(juce::Graphics& g) {
    if (isV3()) {
        const float u = u_;
        auto S = [u](float x, float y, float w, float h) { return juce::Rectangle<float>(x * u, y * u, w * u, h * u); };
        art.draw(g, juce::String("bd|") + v3::modeKey(mode), getLocalBounds().toFloat(), [this](juce::Graphics& ig, juce::Rectangle<float> b) { paintBackdrop3(ig, b); });
        // colonne héros : réduction moyenne appliquée, état du profil
        g.setColour(T.scrMute); v3::caps(g, u8("Réduction moyenne"), S(762, 90, 210, 18), 11.f * u, juce::Justification::centredLeft);
        const auto big = v3::font(60.f * u, true);
        const String n = v3::fmt::num(redSm > 0.05f ? -(double)redSm : 0.0, 1);
        g.setColour(T.scrInk); v3::tabular(g, n, S(762, 112, 210, 72), big, juce::Justification::centredLeft);
        g.setColour(T.scrMute); g.setFont(v3::font(17.f * u)); g.drawText("dB", S(766, 0, 0, 0).withX(S(762, 0, 0, 0).getX() + v3::tabularWidth(n, big) + 6.f * u).withY(154.f * u).withSize(40.f * u, 22.f * u), juce::Justification::centredLeft, false);
        g.setColour(T.scrMute); v3::caps(g, "Profil", S(762, 214, 210, 18), 11.f * u, juce::Justification::centredLeft);
        const bool l = scope->isLearning(), k = scope->isKnown();
        g.setColour(l ? T.out : T.scrInk); g.setFont(v3::font(19.f * u, true));
        g.drawText(l ? u8("Apprentissage…") : k ? u8("Bruit appris") : u8("Adaptatif"), S(762, 234, 210, 26), juce::Justification::centredLeft, false);
        if (l) {
            auto bar = S(762, 270, 196, 6);
            g.setColour(juce::Colours::white.withAlpha(0.12f)); g.fillRoundedRectangle(bar, 3.f * u);
            g.setColour(T.out); g.fillRoundedRectangle(bar.withWidth(bar.getWidth() * juce::jlimit(0.f, 1.f, scope->progress())), 3.f * u);
        }
        return;
    }
    const float u = u_;
    kit::w::drawFace(g, pal, getLocalBounds(), u, {kit::w::Family::Rack, "Orbe Denoiser", kit::w::serial::denoiser});
    section(g, pal, u, u8("Analyse du bruit"), 32, 290, 280);
    section(g, pal, u, u8("R\xC3\xA9" "duction \xC2\xB7 spectre Wiener liss\xC3\xA9"), 331, 290, 445);
    kit::engraved(g, pal, "Anti-ronflement", Rectangle<float>(36 * u, 372 * u, 112 * u, 18 * u), juce::Justification::centredLeft, 8.4f * u, 1.6f * u, true);
}
