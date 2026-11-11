#include "Orbe808Editor.h"

using namespace kit;
using juce::Colour, juce::Justification, juce::Rectangle, juce::String;
namespace Colours = juce::Colours;
using suiteui::kPeach, suiteui::kLilac, suiteui::kIce;

namespace v3 = kit::v3;
namespace {
struct KnobDef { const char* id; const char* label; const char* tip; };
const KnobDef kDefs[9] = {
    {"len", "Longueur", "Durée de résonance de la note"},
    {"punch", "Punch", "Hauteur de départ au-dessus de la note, en demi-tons"},
    {"pt", "Vitesse", "Vitesse de retombée du punch"},
    {"drive", "Saturation", "Quantité de saturation"},
    {"tone", "Harmoniques", "Part d'harmoniques ajoutées à l'octave"},
    {"cut", "Filtre", "Fréquence de coupure du filtre passe-bas"},
    {"click", "Attaque", "Clic d'attaque"},
    {"glide", "Glissé", "Durée du glissé entre deux notes liées (legato)"},
    {"level", "Niveau", "Volume de sortie"}};
}  // namespace

// ---------------- clavier ----------------
int Orbe808Keys::keyAt(juce::Point<float> p) const {
    if (!getLocalBounds().toFloat().contains(p)) return -1;
    static const int ws[7] = {0, 2, 4, 5, 7, 9, 11};
    const float kw = (float)getWidth() / (float)kWhite, bh = (float)getHeight() * 0.62f, bw = kw * 0.58f;
    if (p.y < bh)
        for (int w = 0; w < kWhite - 1; ++w) {
            const int d = w % 7;
            if (d == 2 || d == 6) continue;  // pas de touche noire entre E–F et B–C
            const float cx = kw * (float)(w + 1);
            if (std::fabs(p.x - cx) < bw * 0.5f) return kBase + ws[d] + 12 * (w / 7) + 1;
        }
    const int w = juce::jlimit(0, kWhite - 1, (int)(p.x / kw));
    return kBase + ws[w % 7] + 12 * (w / 7);
}

void Orbe808Keys::press(int note) {
    if (note == cur) return;
    if (note >= 0) proc.keys.noteOn(1, note, 0.8f);   // la nouvelle touche d'abord : l'ancienne se relâche ensuite, d'où le glissé
    if (cur >= 0) proc.keys.noteOff(1, cur, 0.f);
    cur = note;
    repaint();
}

void Orbe808Keys::paint(juce::Graphics& g) {
    if (v3::bridge::v3(T)) { paint3(g); return; }
    static const int ws[7] = {0, 2, 4, 5, 7, 9, 11};
    static const char* nm[7] = {"C", "D", "E", "F", "G", "A", "B"};
    const float u = (float)getWidth() / 952.f, kw = (float)getWidth() / (float)kWhite, H = (float)getHeight(), bh = H * 0.62f, bw = kw * 0.58f;
    const int snd = proc.sounding.load();
    auto lit = [&](int n) { return n == cur || n == snd || proc.keys.isNoteOn(1, n); };
    g.setColour(Colours::black.withAlpha(0.6f)); g.fillRoundedRectangle(getLocalBounds().toFloat(), 4.f * u);
    for (int w = 0; w < kWhite; ++w) {
        const int n = kBase + ws[w % 7] + 12 * (w / 7);
        auto r = Rectangle<float>(kw * (float)w + 1.f, 1.f, kw - 2.f, H - 3.f);
        const bool on = lit(n);
        g.setGradientFill(on ? juce::ColourGradient(kPeach.brighter(0.2f), r.getX(), r.getY(), kPeach.darker(0.35f), r.getX(), r.getBottom(), false)
                             : juce::ColourGradient(Colour(0xFFDAD7E3), r.getX(), r.getY(), Colour(0xFF9B98A9), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle(r, 3.f * u);
        g.setColour(Colours::white.withAlpha(0.35f)); g.fillRect(r.getX() + 2, r.getY() + 1, r.getWidth() - 4, 1.2f);
        g.setColour(Colours::black.withAlpha(0.55f)); g.drawRoundedRectangle(r, 3.f * u, 1.f);
        g.setColour(Colour(0xFF2A2A31).withAlpha(0.85f));
        g.setFont(mono(10.5f * u));
        g.drawText(String(nm[w % 7]) + (w % 7 == 0 ? String(n / 12 - 1) : String()), r.withTop(r.getBottom() - 20 * u), Justification::centred);
    }
    for (int w = 0; w < kWhite - 1; ++w) {
        const int d = w % 7;
        if (d == 2 || d == 6) continue;
        const int n = kBase + ws[d] + 12 * (w / 7) + 1;
        auto r = Rectangle<float>(kw * (float)(w + 1) - bw * 0.5f, 0.f, bw, bh);
        const bool on = lit(n);
        g.setColour(Colours::black.withAlpha(0.5f)); g.fillRoundedRectangle(r.translated(0, 2.f * u), 3.f * u);
        g.setGradientFill(on ? juce::ColourGradient(kPeach, r.getX(), r.getY(), kPeach.darker(0.6f), r.getX(), r.getBottom(), false)
                             : juce::ColourGradient(Colour(0xFF4A4A52), r.getX(), r.getY(), Colour(0xFF121215), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle(r, 3.f * u);
        g.setColour(Colours::white.withAlpha(0.14f)); g.fillRect(r.getX() + 2, r.getY() + 1, r.getWidth() - 4, 1.2f);
    }
}

// ---------------- éditeur ----------------
Palette Orbe808Editor::palette() { return kit::w::instrument(kPeach); }

Orbe808Editor::Orbe808Editor(Orbe808Processor& p)
    : AudioProcessorEditor(&p), proc(p), pal(palette()), look(pal), keysUi(p, pal) {
    setLookAndFeel(&look);
    auto& ap = proc.apvts;
    for (int i = 0; i < kKnobs; ++i) {
        auto& s = knobs[i];
        s.setSliderStyle(juce::Slider::RotaryVerticalDrag); s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        s.setMouseDragSensitivity(180); s.setTooltip(u8(kDefs[i].tip));
        if (i == 0 || i == 3) s.getProperties().set("kitKnob", (int)KnobStyle::Chicken);   // gros Longueur et Saturation à bec
        addAndMakeVisible(s);
        att[i] = std::make_unique<SliderAtt>(ap, kDefs[i].id, s);
        if (auto* prm = ap.getParameter(kDefs[i].id)) s.setDoubleClickReturnValue(true, prm->convertFrom0to1(prm->getDefaultValue()));
    }
    juce::StringArray pl;
    for (int i = 0; i <= o808::kNumPresets; ++i) pl.add(Orbe808Processor::presetLabel(i));
    presetSeg = std::make_unique<v3::GridSeg>(*ap.getParameter("preset"), pl, pal, 2);
    satSeg = std::make_unique<v3::GridSeg>(*ap.getParameter("sat"), juce::StringArray{u8("Douce"), u8("Écrêtée"), u8("Pliée")}, pal);
    bendSeg = std::make_unique<v3::GridSeg>(*ap.getParameter("bend"), juce::StringArray{u8("±1"), u8("±2"), u8("±5"), u8("±12")}, pal);
    for (auto* c : {(juce::Component*)presetSeg.get(), (juce::Component*)satSeg.get(), (juce::Component*)bendSeg.get(), (juce::Component*)&keysUi}) addAndMakeVisible(c);
    presetSeg->setTooltip(u8("Six sons de départ ; toucher un réglage passe en « Perso »"));
    satSeg->setTooltip(u8("Courbe de saturation : douce (tanh), écrêtée ou repliée")); bendSeg->setTooltip(u8("Amplitude de la molette de pitch bend, en demi-tons"));

    help.install(*this, "Orbe 808", {u8("Basse 808 monophonique avec glissé legato : joue-la en MIDI depuis l'hôte ou avec le clavier."),
        u8("- Écran : forme d'onde de la dernière note jouée et niveau de sortie."),
        u8("- Presets : six sons de départ ; toucher un réglage passe en « Perso »."),
        u8("- Longueur : résonance ; Punch et Vitesse : chute de hauteur à l'attaque ; Saturation et Courbe : grain ; Glissé : durée du legato entre notes liées."),
        u8("- Pitch bend : amplitude de la molette, en demi-tons. Double-clic sur un potard : valeur d'origine.")});
    pkMin.assign(kCols, 0.f); pkMax.assign(kCols, 0.f);
    for (int i = 0; i < kKnobs; ++i) {   // v3 : potards haut de gamme sur les mêmes paramètres
        auto* k = k3.add(new v3::Knob3(ap, kDefs[i].id, u8(kDefs[i].label), u8(kDefs[i].tip), v3::KnobSize::S));
        k->setLabelPx(10.f);
        if (i == 5) k->setFormatter([](double v) { const double f = 120.0 * std::pow(100.0, v); return v > 0.98 ? String(u8("ouvert")) : v3::fmt::hz(f); });
        addChildComponent(k);
    }
    accSeg = std::make_unique<kit::v3::UiSeg>(acc, kit::v3::notes::accidentalLabels(), kit::v3::notes::accidentalTip(), pal);
    addAndMakeVisible(*accSeg); acc.onChange = [this] { repaint(); };
    addAndMakeVisible(skin.themeBtn);
    skin.onMode = [this] {
        const bool n3 = skin.on();
        for (auto& s : knobs) s.setVisible(!n3);
        for (auto* k : k3) { k->setVisible(n3); k->setup(&skin.T, &skin.art); }
        for (auto* s : {presetSeg.get(), satSeg.get(), bendSeg.get()}) { s->T = skin.tp(); s->px = 11.f; s->repaint(); }
        help.T = skin.tp(); keysUi.T = skin.tp(); accSeg->T = skin.tp(); accSeg->px = 12.f;
    };
    setResizable(true, true);
    setResizeLimits(800, 506, 1700, 1074);
    getConstrainer()->setFixedAspectRatio(1000.0 / 632.0);
    setSize(1000, 632);
    skin.init(*this, look, 1000.f, 632.f);
    refreshShot();
    startTimerHz(30);
}

Orbe808Editor::~Orbe808Editor() { stopTimer(); skin.shutdown(); }

void Orbe808Editor::resized() {
    u_ = (float)getWidth() / 1000.f;
    skin.scale = u_;
    if (skin.on()) { resized3(); return; }
    help.layout(kit::w::helpSlot(getLocalBounds().toFloat().reduced(18.f * u_, 0), u_, false).toNearestInt(), getLocalBounds());
    skin.themeBtn.setBounds(help.getBounds().translated(-help.getWidth() - juce::roundToInt(6.f * u_), 0));
    const float pitch = 952.f / (float)kKnobs;
    for (int i = 0; i < kKnobs; ++i) knobs[i].setBounds(Si(24.f + pitch * ((float)i + 0.5f) - 48.f, 364, 96, 96));
    presetSeg->setBounds(Si(700, 108, 276, 120));
    accSeg->setBounds(Si(926, 78, 50, 22));
    satSeg->setBounds(Si(700, 254, 276, 28));
    bendSeg->setBounds(Si(700, 310, 276, 28));
    keysUi.setBounds(Si(24, 520, 952, 88));
    repaint();
}

String Orbe808Editor::valueText(int i) const {
    auto* prm = proc.apvts.getParameter(kDefs[i].id);
    if (!prm) return {};
    const float v = proc.apvts.getRawParameterValue(kDefs[i].id)->load();
    if (i == 5) { const float f = 120.f * std::pow(100.f, v); return v > 0.98f ? u8("ouvert") : f >= 1000.f ? suiteui::fr1(f / 1000.f) + " k" : String(juce::roundToInt(f)) + " Hz"; }
    return prm->getText(prm->convertTo0to1(v), 0);
}

void Orbe808Editor::refreshShot() {
    auto P = Orbe808Processor::paramsFrom(proc.apvts);
    shotNote = proc.lastNote.load();
    P.level = 1.f; P.bendRange = 0.f;
    uint64_t k = 1469598103934665603ull;
    auto mix = [&](float f) { uint32_t b; std::memcpy(&b, &f, 4); k = (k ^ b) * 1099511628211ull; };
    for (float f : {P.len, P.punch, P.pt, P.drive, (float)P.sat, P.tone, P.cut, P.click, (float)shotNote}) mix(f);
    if (k == shotKey) return;
    shotKey = k;
    const double sr = 16000.0;
    shotSeconds = juce::jmax(0.3f, P.len * 1.2f + 0.1f);
    const int total = (int)(shotSeconds * (float)sr);
    std::vector<float> buf((size_t)total);
    o808::Engine e;
    e.prepare(sr); e.setParams(P); e.noteOn(shotNote, 1.f);
    e.render(buf.data(), total);
    for (int c = 0; c < kCols; ++c) {
        const int a = (int)((int64_t)c * total / kCols), b = juce::jmax(a + 1, (int)((int64_t)(c + 1) * total / kCols));
        float lo = 0.f, hi = 0.f;
        for (int i = a; i < b && i < total; ++i) { lo = juce::jmin(lo, buf[(size_t)i]); hi = juce::jmax(hi, buf[(size_t)i]); }
        pkMin[(size_t)c] = lo; pkMax[(size_t)c] = hi;
    }
}

void Orbe808Editor::timerCallback() {
    refreshShot();
    const float m = proc.meter.load();
    meterShown = juce::jmax(m, meterShown * 0.88f);
    if (skin.on()) for (auto* k : k3) k->tickMod(1.f / 30.f);
    repaint();
}

void Orbe808Editor::paintHeader(juce::Graphics& g, float u) {
    const int n = proc.sounding.load() >= 0 ? proc.sounding.load() : proc.lastNote.load();
    const float hz = 440.f * std::pow(2.f, ((float)n - 69.f) / 12.f);
    screenWell(g, pal, S(560, 8, 360, 34), Display::Amber);
    const bool on = proc.sounding.load() >= 0;
    led(g, 578 * u, 25 * u, 3.6f * u, on ? 1.f : 0.f, pal.ledRed, LedBezel::None);
    g.setColour(kPeach); g.setFont(mono(13.f * u));
    const int idx = (int)proc.apvts.getRawParameterValue("preset")->load();
    g.drawText(kit::v3::notes::name(n, acc.flat()) + "  " + suiteui::fr1(hz) + " Hz", S(592, 8, 170, 34).toNearestInt(), juce::Justification::centredLeft);
    g.setColour(displayColours(Display::Amber, pal).ink.withAlpha(0.85f));
    g.drawText(upper(Orbe808Processor::presetLabel(idx)), S(760, 8, 152, 34).toNearestInt(), juce::Justification::centredRight);
}

void Orbe808Editor::paintScope(juce::Graphics& g, float u) {
    auto box = S(24, 92, 652, 246);
    screenWell(g, pal, box, Display::Amber);
    auto a = box.reduced(14 * u, 30 * u).withTrimmedBottom(10 * u);
    const float cy = a.getCentreY(), amp = a.getHeight() * 0.5f * 0.94f;
    // repères de temps
    g.setFont(mono(9.f * u));
    for (float t = 0.f; t < shotSeconds; t += 0.5f) {
        const float x = a.getX() + a.getWidth() * t / shotSeconds;
        g.setColour(Colours::white.withAlpha(t == 0.f ? 0.18f : 0.06f)); g.fillRect(x, a.getY(), 1.f, a.getHeight());
        g.setColour(pal.mute.withAlpha(0.8f)); g.drawText(t == 0.f ? "0" : suiteui::fr1(t) + " s", Rectangle<float>(x + 3 * u, a.getBottom() - 1 * u, 44 * u, 11 * u), juce::Justification::centredLeft);
    }
    g.setColour(Colours::white.withAlpha(0.12f)); g.fillRect(a.getX(), cy, a.getWidth(), 1.f);
    // forme d'onde de la note
    juce::Path top, bot, all;
    for (int c = 0; c < kCols; ++c) {
        const float x = a.getX() + a.getWidth() * (float)c / (float)(kCols - 1);
        const float y = cy - pkMax[(size_t)c] * amp;
        if (c == 0) top.startNewSubPath(x, y); else top.lineTo(x, y);
    }
    for (int c = kCols - 1; c >= 0; --c) {
        const float x = a.getX() + a.getWidth() * (float)c / (float)(kCols - 1);
        if (c == kCols - 1) bot.startNewSubPath(x, cy - pkMin[(size_t)c] * amp); else bot.lineTo(x, cy - pkMin[(size_t)c] * amp);
    }
    all = top; all.addPath(bot); all.closeSubPath();
    g.setGradientFill(juce::ColourGradient(kPeach.withAlpha(0.55f), 0, cy - amp, kPeach.withAlpha(0.10f), 0, cy, false));
    g.fillPath(all);
    g.setColour(kPeach.withAlpha(0.95f)); g.strokePath(all, juce::PathStrokeType(1.1f * u));
    // textes
    g.setFont(mono(10.f * u)); g.setColour(pal.mute);
    g.drawText(upper(u8("Forme d'onde de la note")), S(38, 100, 280, 16).toNearestInt(), juce::Justification::centredLeft);
    g.setColour(kPeach); g.drawText(kit::v3::notes::name(shotNote, acc.flat()), S(400, 100, 60, 16).toNearestInt(), juce::Justification::centredRight);
    g.setColour(pal.mute); g.drawText("SORTIE", S(480, 100, 60, 16).toNearestInt(), juce::Justification::centredRight);
    const float db = 20.f * std::log10(juce::jmax(1e-4f, meterShown));
    suiteui::ledBar(g, S(548, 104, 114, 8), juce::jlimit(0.f, 1.f, (db + 42.f) / 42.f), 14, kPeach);
}

void Orbe808Editor::paint(juce::Graphics& g) {
    if (skin.on()) { paint3(g); return; }
    const float u = u_;
    kit::w::drawFace(g, pal, getLocalBounds(), u, {kit::w::Family::Instrument, "Orbe 808", kit::w::serial::b808});
    suiteui::rule(g, u, 352, 964, 18); suiteui::rule(g, u, 506, 964, 18);
    paintHeader(g, u);
    paintScope(g, u);
    engraved(g, pal, "Presets", S(700, 90, 200, 14), juce::Justification::centredLeft, 10.f * u, 2.f * u);
    engraved(g, pal, u8("Courbe de saturation"), S(700, 236, 276, 14), juce::Justification::centredLeft, 10.f * u, 2.f * u);
    engraved(g, pal, u8("Pitch bend · demi-tons"), S(700, 292, 276, 14), juce::Justification::centredLeft, 10.f * u, 2.f * u);
    const float pitch = 952.f / (float)kKnobs;
    for (int i = 0; i < kKnobs; ++i) {
        const float cx = 24.f + pitch * ((float)i + 0.5f);
        engraved(g, pal, u8(kDefs[i].label), S(cx - pitch * 0.5f, 466, pitch, 14), juce::Justification::centred, 9.5f * u, 1.5f * u);
        g.setColour(pal.mute); g.setFont(mono(10.f * u));
        g.drawText(valueText(i), S(cx - pitch * 0.5f, 482, pitch, 14).toNearestInt(), juce::Justification::centred);
    }
    g.setColour(pal.line); g.fillRect(S(24.f + pitch * 8.f, 372, 1.f, 100));  // le niveau est séparé des réglages de son
}

// ============================================================ v3 « hybride v2 » (mêmes coordonnées de conception 1000 x 632)
void Orbe808Keys::paint3(juce::Graphics& g) {
    static const int ws[7] = {0, 2, 4, 5, 7, 9, 11};
    static const char* nm[7] = {"C", "D", "E", "F", "G", "A", "B"};
    const float u = (float)getWidth() / 952.f, kw = (float)getWidth() / (float)kWhite, H = (float)getHeight(), bh = H * 0.62f, bw = kw * 0.58f;
    const int snd = proc.sounding.load();
    auto lit = [&](int n) { return n == cur || n == snd || proc.keys.isNoteOn(1, n); };
    const auto& t = *T;
    for (int w = 0; w < kWhite; ++w) {
        const int n = kBase + ws[w % 7] + 12 * (w / 7);
        auto r = Rectangle<float>(kw * (float)w + 1.5f * u, 1.f, kw - 3.f * u, H - 4.f);
        const bool on = lit(n);
        g.setColour(t.shadow.withMultipliedAlpha(0.5f)); g.fillRoundedRectangle(r.translated(0, 1.5f), 7.f * u);
        g.setColour(on ? t.accent : (t.dark ? Colour(0xFFD9DCE4) : Colour(0xFFFFFFFF))); g.fillRoundedRectangle(r, 7.f * u);
        g.setColour(t.dark ? Colour(0x33FFFFFF) : t.fieldEdge); g.drawRoundedRectangle(r.reduced(0.5f), 7.f * u, 1.f);
        g.setColour(on ? Colour(0xFF1B1E26) : Colour(0xFF5A5F6B)); g.setFont(v3::font(11.f * u, true));
        g.drawText(String(nm[w % 7]) + (w % 7 == 0 ? String(n / 12 - 1) : String()), r.withTop(r.getBottom() - 22 * u), Justification::centred, false);
    }
    for (int w = 0; w < kWhite - 1; ++w) {
        const int d = w % 7;
        if (d == 2 || d == 6) continue;
        const int n = kBase + ws[d] + 12 * (w / 7) + 1;
        auto r = Rectangle<float>(kw * (float)(w + 1) - bw * 0.5f, 0.f, bw, bh);
        g.setColour(Colours::black.withAlpha(0.28f)); g.fillRoundedRectangle(r.translated(0, 2.f * u), 5.f * u);
        g.setColour(lit(n) ? t.accent.darker(0.15f) : Colour(0xFF20232C)); g.fillRoundedRectangle(r, 5.f * u);
        g.setColour(Colours::white.withAlpha(0.10f)); g.fillRoundedRectangle(r.reduced(2.f * u, 1.f).withHeight(3.f * u), 1.5f * u);
    }
}

void Orbe808Editor::resized3() {
    help.layout(Si(954, 24, 28, 28), getLocalBounds());
    skin.themeBtn.setBounds(Si(918, 24, 28, 28));
    const float pitch = 952.f / (float)kKnobs;
    for (int i = 0; i < kKnobs; ++i) { k3[i]->setScale(u_); k3[i]->setBounds(Si(24.f + pitch * ((float)i + 0.5f) - 52.f, 364, 104, 112)); }
    presetSeg->setBounds(Si(706, 118, 264, 104));
    accSeg->setBounds(Si(916, 98, 54, 22));
    satSeg->setBounds(Si(706, 250, 264, 30));
    bendSeg->setBounds(Si(706, 304, 264, 30));
    keysUi.setBounds(Si(24, 524, 952, 88));
    repaint();
}

void Orbe808Editor::paintBackdrop3(juce::Graphics& g, Rectangle<float> b) {
    const float u = b.getWidth() / 1000.f;
    auto R = [u](float x, float y, float w, float h) { return Rectangle<float>(x * u, y * u, w * u, h * u); };
    const auto& T = skin.T;
    v3::background(g, T, b);
    v3::brand(g, T, u, "Orbe", "808");
    v3::hero(g, T, R(24, 92, 652, 246), 20.f * u);
    v3::card(g, T, R(690, 92, 296, 246), 18.f * u);
    v3::card(g, T, R(24, 352, 952, 156), 18.f * u);
    v3::label(g, T, "Presets", R(706, 100, 200, 14), 11.f * u);
    v3::label(g, T, u8("Courbe de saturation"), R(706, 232, 260, 14), 11.f * u);
    v3::label(g, T, u8("Pitch bend · demi-tons"), R(706, 286, 260, 14), 11.f * u);
    const float pitch = 952.f / 9.f;
    g.setColour(T.dark ? Colours::white.withAlpha(0.07f) : Colour(0x14101828)); g.fillRect(R(24.f + pitch * 8.f, 368, 1, 124));
}

void Orbe808Editor::paintScope3(juce::Graphics& g) {
    const float u = u_;
    const auto& T = skin.T;
    auto box = S(24, 92, 652, 246);
    auto a = box.reduced(18 * u, 38 * u).withTrimmedBottom(14 * u);
    const float cy = a.getCentreY(), amp = a.getHeight() * 0.5f * 0.92f;
    g.setFont(v3::font(11.f * u));
    for (float t = 0.f; t < shotSeconds; t += 0.5f) {
        const float x = a.getX() + a.getWidth() * t / shotSeconds;
        g.setColour(Colours::white.withAlpha(t == 0.f ? 0.16f : 0.06f)); g.fillRect(x, a.getY(), 1.f, a.getHeight());
        g.setColour(T.scrMute); g.drawText(t == 0.f ? "0" : v3::fmt::withUnit(v3::fmt::num(t, 1), "s"), Rectangle<float>(x + 3 * u, a.getBottom() + 1 * u, 50 * u, 13 * u), Justification::centredLeft, false);
    }
    g.setColour(Colours::white.withAlpha(0.12f)); g.fillRect(a.getX(), cy, a.getWidth(), 1.f);
    {
        juce::Graphics::ScopedSaveState st(g);
        g.reduceClipRegion(a.expanded(4 * u).toNearestInt());
        auto X = [&](float i) { return a.getX() + a.getWidth() * i / (float)(kCols - 1); };
        const v3::Trace::Opts o{kCols, 1.2f, 1e9f, -2.f, 0};
        trMax.build(pkMax.data(), kCols, X, [&](float v) { return cy - v * amp; }, cy, o);
        trMin.build(pkMin.data(), kCols, X, [&](float v) { return cy - v * amp; }, cy, o);
        trMax.draw(g, T.accent, 1.8f * u, 1.f, 0.30f, cy - amp * 0.0f);
        trMin.draw(g, T.accent, 1.8f * u, 1.f, 0.0f, 0.f);
    }
    // note, fréquence, niveau
    const int n = proc.sounding.load() >= 0 ? proc.sounding.load() : proc.lastNote.load();
    const float hz = 440.f * std::pow(2.f, ((float)n - 69.f) / 12.f);
    const auto big = v3::font(34.f * u, true);
    g.setColour(T.scrInk); g.setFont(big); g.drawText(kit::v3::notes::name(n, acc.flat()), Rectangle<float>(box.getX() + 20 * u, box.getY() + 12 * u, 90 * u, 40 * u), Justification::centredLeft, false);
    g.setColour(T.scrMute); v3::tabular(g, v3::fmt::hz(hz), Rectangle<float>(box.getX() + 100 * u, box.getY() + 22 * u, 110 * u, 22 * u), v3::font(15.f * u), Justification::centredLeft);
    const int idx = (int)proc.apvts.getRawParameterValue("preset")->load();
    g.setColour(T.out); v3::caps(g, Orbe808Processor::presetLabel(idx), Rectangle<float>(box.getX() + 230 * u, box.getY() + 22 * u, 190 * u, 22 * u), 12.f * u, Justification::centredLeft);
    const float db = 20.f * std::log10(juce::jmax(1e-4f, meterShown));
    g.setColour(T.scrMute); v3::caps(g, "Sortie", Rectangle<float>(box.getRight() - 230 * u, box.getY() + 14 * u, 60 * u, 16 * u), 10.5f * u, Justification::centredRight);
    v3::meterBar(g, T, Rectangle<float>(box.getRight() - 160 * u, box.getY() + 19 * u, 130 * u, 6 * u), (db + 42.f) / 42.f, T.out);
    g.setColour(T.scrInk); v3::tabular(g, v3::fmt::db(db, 0), Rectangle<float>(box.getRight() - 160 * u, box.getY() + 28 * u, 130 * u, 16 * u), v3::font(12.f * u), Justification::centredRight);
}

void Orbe808Editor::paint3(juce::Graphics& g) {
    skin.art.draw(g, String("bd|") + v3::modeKey(skin.mode), getLocalBounds().toFloat(), [this](juce::Graphics& ig, Rectangle<float> b) { paintBackdrop3(ig, b); });
    paintScope3(g);
}
