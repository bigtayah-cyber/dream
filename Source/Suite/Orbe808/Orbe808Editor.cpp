#include "Orbe808Editor.h"

using namespace kit;
using juce::Colour, juce::Justification, juce::Rectangle, juce::String;
namespace Colours = juce::Colours;
using suiteui::kPeach, suiteui::kLilac, suiteui::kIce;

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
Palette Orbe808Editor::palette() {
    Palette p;
    p.plateTop = Colour(0xFF2B2B31); p.plateBot = Colour(0xFF16161A);
    p.ink = Colour(0xFFE9E7F0); p.mute = Colour(0xFF8F8D9B); p.line = Colour(0xFF3B3B44);
    p.accent = kPeach; p.accent2 = kLilac;
    p.screen = Colour(0xFF0B0C11); p.screenInk = Colour(0xFFEDEBF6);
    return p;
}

Orbe808Editor::Orbe808Editor(Orbe808Processor& p)
    : AudioProcessorEditor(&p), proc(p), pal(palette()), look(pal), keysUi(p, pal) {
    setLookAndFeel(&look);
    auto& ap = proc.apvts;
    for (int i = 0; i < kKnobs; ++i) {
        auto& s = knobs[i];
        s.setSliderStyle(juce::Slider::RotaryVerticalDrag); s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        s.setMouseDragSensitivity(180); s.setTooltip(u8(kDefs[i].tip));
        addAndMakeVisible(s);
        att[i] = std::make_unique<SliderAtt>(ap, kDefs[i].id, s);
        if (auto* prm = ap.getParameter(kDefs[i].id)) s.setDoubleClickReturnValue(true, prm->convertFrom0to1(prm->getDefaultValue()));
    }
    juce::StringArray pl;
    for (int i = 0; i <= o808::kNumPresets; ++i) pl.add(Orbe808Processor::presetLabel(i));
    presetSeg = std::make_unique<suiteui::Segmented>(*ap.getParameter("preset"), pl, pal, 2);
    satSeg = std::make_unique<suiteui::Segmented>(*ap.getParameter("sat"), juce::StringArray{u8("Douce"), u8("Écrêtée"), u8("Repliée")}, pal);
    bendSeg = std::make_unique<suiteui::Segmented>(*ap.getParameter("bend"), juce::StringArray{u8("±1"), u8("±2"), u8("±5"), u8("±12")}, pal);
    for (auto* c : {(juce::Component*)presetSeg.get(), (juce::Component*)satSeg.get(), (juce::Component*)bendSeg.get(), (juce::Component*)&keysUi}) addAndMakeVisible(c);
    presetSeg->setTooltip(u8("Six sons de départ ; toucher un réglage passe en « Perso »"));
    satSeg->setTooltip(u8("Courbe de saturation : douce (tanh), écrêtée ou repliée")); bendSeg->setTooltip(u8("Amplitude de la molette de pitch bend, en demi-tons"));

    pkMin.assign(kCols, 0.f); pkMax.assign(kCols, 0.f);
    setResizable(true, true);
    setResizeLimits(800, 506, 1700, 1074);
    getConstrainer()->setFixedAspectRatio(1000.0 / 632.0);
    setSize(1000, 632);
    refreshShot();
    startTimerHz(30);
}

Orbe808Editor::~Orbe808Editor() { stopTimer(); setLookAndFeel(nullptr); }

void Orbe808Editor::resized() {
    u_ = (float)getWidth() / 1000.f;
    plate = brushedPlate(getWidth(), getHeight(), pal);
    const float pitch = 952.f / (float)kKnobs;
    for (int i = 0; i < kKnobs; ++i) knobs[i].setBounds(Si(24.f + pitch * ((float)i + 0.5f) - 48.f, 364, 96, 96));
    presetSeg->setBounds(Si(700, 108, 276, 120));
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
    repaint();
}

void Orbe808Editor::paintHeader(juce::Graphics& g, float u) {
    suiteui::logo(g, 48 * u, 36 * u, 17 * u, u, kPeach, Colour(0xFFB8502E));
    engraved(g, pal, "Orbe 808", S(84, 18, 240, 34), juce::Justification::centredLeft, 28.f * u, 6.f * u, true);
    g.setColour(pal.mute); g.setFont(fnt(10.f * u));
    g.drawText(u8("Basse monophonique · glissé legato"), S(338, 26, 280, 20).toNearestInt(), juce::Justification::centredLeft);
    const int n = proc.sounding.load() >= 0 ? proc.sounding.load() : proc.lastNote.load();
    const float hz = 440.f * std::pow(2.f, ((float)n - 69.f) / 12.f);
    screenWell(g, pal, S(640, 18, 336, 36));
    const bool on = proc.sounding.load() >= 0;
    led(g, 660 * u, 36 * u, 3.6f * u, on, kPeach);
    g.setColour(kPeach); g.setFont(mono(13.f * u));
    const int idx = (int)proc.apvts.getRawParameterValue("preset")->load();
    g.drawText(suiteui::noteName(n) + "  " + suiteui::fr1(hz) + " Hz", S(674, 18, 160, 36).toNearestInt(), juce::Justification::centredLeft);
    g.setColour(pal.screenInk.withAlpha(0.8f));
    g.drawText(upper(Orbe808Processor::presetLabel(idx)), S(820, 18, 148, 36).toNearestInt(), juce::Justification::centredRight);
}

void Orbe808Editor::paintScope(juce::Graphics& g, float u) {
    auto box = S(24, 92, 652, 246);
    screenWell(g, pal, box);
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
    g.setColour(kPeach); g.drawText(suiteui::noteName(shotNote), S(400, 100, 60, 16).toNearestInt(), juce::Justification::centredRight);
    g.setColour(pal.mute); g.drawText("SORTIE", S(480, 100, 60, 16).toNearestInt(), juce::Justification::centredRight);
    const float db = 20.f * std::log10(juce::jmax(1e-4f, meterShown));
    suiteui::ledBar(g, S(548, 104, 114, 8), juce::jlimit(0.f, 1.f, (db + 42.f) / 42.f), 14, kPeach);
}

void Orbe808Editor::paint(juce::Graphics& g) {
    const float u = u_;
    g.drawImageAt(plate, 0, 0);
    suiteui::rule(g, u, 70, 1000); suiteui::rule(g, u, 352, 1000); suiteui::rule(g, u, 506, 1000);
    for (auto p : {juce::Point<float>(12, 12), {988, 12}, {12, 620}, {988, 620}}) screw(g, p.x * u, p.y * u, 5.f * u, (p.x + p.y) * 0.9f);
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
