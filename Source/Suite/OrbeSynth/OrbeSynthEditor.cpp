#include "OrbeSynthEditor.h"

using namespace kit;
using juce::Colour, juce::Rectangle, juce::String;

namespace {
const Colour kLilac(0xFFCFC6FF), kIce(0xFFA9ECFF), kPeach(0xFFFFC4AB);
constexpr float W = 1100.f, H = 660.f;
// Sections (unités de conception) et cellules de potard.
const Rectangle<float> kS1{20, 70, 560, 204}, kS2{596, 70, 484, 204}, kS3{20, 286, 560, 204}, kS4{596, 286, 484, 204};
String comma(String s) { return s.replace(".", ","); }
float hzOf(float cut) { return 60.f * std::pow(300.f, cut); }
String hzText(float f) { return f >= 1000.f ? comma(String(f / 1000.f, 1)) + " kHz" : String(juce::roundToInt(f)) + " Hz"; }
}  // namespace

namespace osui {

// ---------------- Knob ----------------
Knob::Knob(APVTS& s, const char* id, String lab, String tip, const Palette& pal) : prm(s.getParameter(id)), label(std::move(lab)), P(pal) {
    sl.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    sl.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    sl.setMouseDragSensitivity(180);
    sl.setTooltip(tip);
    sl.onValueChange = [this] { repaint(); };
    addAndMakeVisible(sl);
    att = std::make_unique<APVTS::SliderAttachment>(s, id, sl);
    sl.setDoubleClickReturnValue(true, sl.proportionOfLengthToValue(prm->getDefaultValue()));
}
void Knob::resized() {
    const float u = (float)getWidth() / 86.f, s = 62.f * u;
    sl.setBounds(juce::Rectangle<float>(((float)getWidth() - s) * .5f, 0.f, s, s).toNearestInt());
}
void Knob::paint(juce::Graphics& g) {
    const float u = (float)getWidth() / 86.f;
    engraved(g, P, label, {0, 63 * u, (float)getWidth(), 12 * u}, juce::Justification::centred, 9.f * u, 1.5f * u);
    g.setColour(kIce.withAlpha(.85f));
    g.setFont(mono(9.5f * u));
    g.drawText(comma(prm->getCurrentValueAsText()), Rectangle<float>(0, 76 * u, (float)getWidth(), 12 * u).toNearestInt(), juce::Justification::centred);
}

// ---------------- Seg ----------------
Seg::Seg(juce::RangedAudioParameter& p, juce::StringArray l, bool wv, const Palette& pal)
    : labels_(std::move(l)), icons(wv), P(pal), att(p, [this](float v) { idx = juce::roundToInt(v); repaint(); }) { att.sendInitialUpdate(); }

static juce::Path wavePath(int w, Rectangle<float> r) {
    juce::Path p;
    const float A = r.getHeight() * .5f, cy = r.getCentreY();
    auto pt = [&](float t, float v) { return juce::Point<float>(r.getX() + r.getWidth() * t, cy - A * v); };
    auto line = [&](std::initializer_list<std::pair<float, float>> pts) { bool f = true; for (auto& q : pts) { if (f) p.startNewSubPath(pt(q.first, q.second)); else p.lineTo(pt(q.first, q.second)); f = false; } };
    switch (w) {
        case 0: for (int i = 0; i <= 48; ++i) { const auto q = pt((float)i / 48.f, std::sin(juce::MathConstants<float>::twoPi * 2.f * (float)i / 48.f)); if (i) p.lineTo(q); else p.startNewSubPath(q); } break;
        case 1: line({{0, 0}, {.125f, 1}, {.375f, -1}, {.625f, 1}, {.875f, -1}, {1, 0}}); break;
        case 2: line({{0, -1}, {.5f, 1}, {.5f, -1}, {1, 1}}); break;
        default: line({{0, 1}, {.25f, 1}, {.25f, -1}, {.5f, -1}, {.5f, 1}, {.75f, 1}, {.75f, -1}, {1, -1}}); break;
    }
    return p;
}
void Seg::paint(juce::Graphics& g) {
    auto r = getLocalBounds().toFloat().reduced(.5f);
    const float u = r.getHeight() / 28.f;
    g.setColour(juce::Colours::black.withAlpha(.6f));
    g.fillRoundedRectangle(r, 4.f * u);
    g.setColour(juce::Colours::white.withAlpha(.10f));
    g.drawRoundedRectangle(r, 4.f * u, 1.f);
    const int n = labels_.size();
    const float w = r.getWidth() / (float)n;
    for (int i = 0; i < n; ++i) {
        auto c = Rectangle<float>(r.getX() + w * (float)i, r.getY(), w, r.getHeight()).reduced(2.f);
        const bool on = i == idx;
        if (on) {
            g.setGradientFill(juce::ColourGradient(P.accent.withAlpha(.55f), c.getX(), c.getY(), P.accent.withAlpha(.22f), c.getX(), c.getBottom(), false));
            g.fillRoundedRectangle(c, 3.f * u);
            g.setColour(P.accent.withAlpha(.9f));
            g.drawRoundedRectangle(c, 3.f * u, 1.f);
        }
        g.setColour(on ? juce::Colours::white : P.ink.withAlpha(.6f));
        if (icons) g.strokePath(wavePath(i, Rectangle<float>(0, 0, 26.f * u, 11.f * u).withCentre(c.getCentre())), juce::PathStrokeType(1.5f * u, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        else tracked(g, labels_[i], c, juce::Justification::centred, 1.f * u, fnt(juce::jmin(10.5f * u, r.getHeight() * .38f), true));
    }
}
void Seg::mouseDown(const juce::MouseEvent& e) {
    const int n = labels_.size();
    att.setValueAsCompleteGesture((float)juce::jlimit(0, n - 1, (int)(e.position.x / ((float)getWidth() / (float)n))));
}

// ---------------- PresetBar ----------------
PresetBar::PresetBar(OrbeSynthProcessor& p, const Palette& pal) : proc(p), P(pal) { setMouseCursor(juce::MouseCursor::PointingHandCursor); setTooltip(kit::u8("Presets d'usine : clic pour la liste, flèches pour parcourir")); }
void PresetBar::paint(juce::Graphics& g) {
    const float u = (float)getHeight() / 32.f;
    auto r = getLocalBounds().toFloat().reduced(3.f * u);
    screenWell(g, P, r);
    const int cur = proc.getCurrentProgram(), n = proc.getNumPrograms();
    g.setColour(P.accent);
    for (int s : {-1, 1}) {
        juce::Path c;
        const float cx = s < 0 ? r.getX() + 16 * u : r.getRight() - 16 * u;
        c.startNewSubPath(cx - s * 3.f * u, r.getCentreY() - 5.f * u); c.lineTo(cx + s * 3.f * u, r.getCentreY()); c.lineTo(cx - s * 3.f * u, r.getCentreY() + 5.f * u);
        g.strokePath(c, juce::PathStrokeType(1.8f * u, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    g.setColour(P.accent.withAlpha(.9f));
    g.setFont(mono(11.f * u));
    g.drawText(String(cur + 1).paddedLeft('0', 2) + " / " + String(n), Rectangle<float>(r.getX() + 34 * u, r.getY(), 70 * u, r.getHeight()).toNearestInt(), juce::Justification::centredLeft);
    g.setColour(P.screenInk);
    g.setFont(fnt(15.f * u, true));
    g.drawText(proc.getProgramName(cur), r.toNearestInt(), juce::Justification::centred);
    if (proc.presetModified()) {
        g.setColour(kPeach.withAlpha(.9f));
        g.setFont(mono(9.f * u));
        g.drawText(u8("MODIFIÉ"), Rectangle<float>(r.getRight() - 90 * u, r.getY(), 62 * u, r.getHeight()).toNearestInt(), juce::Justification::centredRight);
    }
}
void PresetBar::mouseDown(const juce::MouseEvent& e) {
    const int n = proc.getNumPrograms(), cur = proc.getCurrentProgram();
    if (e.position.x < 34.f * (float)getHeight() / 32.f) { pick((cur + n - 1) % n); return; }
    if (e.position.x > (float)getWidth() - 34.f * (float)getHeight() / 32.f) { pick((cur + 1) % n); return; }
    juce::PopupMenu m;
    for (int i = 0; i < n; ++i) m.addItem(i + 1, String(i + 1).paddedLeft('0', 2) + "   " + proc.getProgramName(i), true, i == cur);
    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withMinimumWidth(getWidth()), [this](int id) { if (id > 0) pick(id - 1); });
}
void PresetBar::pick(int i) { proc.setCurrentProgram(i); repaint(); }

// ---------------- Affichages ----------------
CurveView::CurveView(OrbeSynthProcessor& p, const Palette& pal, std::initializer_list<const char*> ids) : proc(p), P(pal) {
    for (auto* id : ids) watch.push_back(std::make_unique<juce::ParameterAttachment>(*p.apvts.getParameter(id), [this](float) { repaint(); }));
}

void EnvView::paint(juce::Graphics& g) {
    const float u = (float)getHeight() / 64.f;
    auto r = getLocalBounds().toFloat().reduced(3.f * u);
    screenWell(g, P, r);
    g.saveState();
    g.reduceClipRegion(r.toNearestInt());
    const auto p = proc.readParams();
    const float hold = .6f, T = p.a + p.d + hold + p.r;
    auto X = [&](float t) { return r.getX() + 8 * u + t / T * (r.getWidth() - 16 * u); };
    auto Y = [&](float v) { return r.getBottom() - 15 * u - v * (r.getHeight() - 28 * u); };
    for (float v : {0.f, .5f, 1.f}) { g.setColour(juce::Colours::white.withAlpha(v == 0.f ? .12f : .05f)); g.fillRect(r.getX(), Y(v), r.getWidth(), 1.f); }
    const float cut[3] = {p.a, p.a + p.d, p.a + p.d + hold};
    g.setColour(juce::Colours::white.withAlpha(.06f));
    for (float t : cut) g.fillRect(X(t), r.getY(), 1.f, r.getHeight());
    juce::Path ln;
    ln.startNewSubPath(X(0), Y(0)); ln.lineTo(X(p.a), Y(1));
    for (int k = 1; k <= 24; ++k) ln.lineTo(X(p.a + p.d * (float)k / 24.f), Y(p.s + (1.f - p.s) * std::exp(-3.f * (float)k / 24.f)));
    ln.lineTo(X(p.a + p.d + hold), Y(p.s));
    for (int k = 1; k <= 24; ++k) ln.lineTo(X(p.a + p.d + hold + p.r * (float)k / 24.f), Y(p.s * std::exp(-3.f * (float)k / 24.f)));
    juce::Path fill(ln);
    fill.lineTo(X(T), Y(0)); fill.closeSubPath();
    g.setGradientFill(juce::ColourGradient(kLilac.withAlpha(.28f), 0, Y(1), kLilac.withAlpha(0.f), 0, Y(0), false));
    g.fillPath(fill);
    g.setColour(kLilac);
    g.strokePath(ln, juce::PathStrokeType(1.6f * u, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(P.mute);
    g.setFont(mono(8.5f * u));
    const float mid[4] = {p.a / 2, p.a + p.d / 2, p.a + p.d + hold / 2, p.a + p.d + hold + p.r / 2};
    const char* nm[4] = {"A", "D", "S", "R"};
    for (int i = 0; i < 4; ++i) g.drawText(nm[i], Rectangle<float>(X(mid[i]) - 8 * u, r.getBottom() - 13 * u, 16 * u, 11 * u), juce::Justification::centred);
    g.restoreState();
}

void FilterView::paint(juce::Graphics& g) {
    const float u = (float)getHeight() / 64.f;
    auto r = getLocalBounds().toFloat().reduced(3.f * u);
    screenWell(g, P, r);
    g.saveState();
    g.reduceClipRegion(r.toNearestInt());
    const auto p = proc.readParams();
    const float fc = hzOf(p.cut), Q = std::pow(10.f, (.5f + p.res * 14.f) / 20.f);  // Q en dB, comme Web Audio
    const float x0 = r.getX() + 6 * u, ww = r.getWidth() - 12 * u;
    auto X = [&](float f) { return x0 + std::log(f / 20.f) / std::log(1000.f) * ww; };
    auto Y = [&](float v) { return r.getBottom() - 14 * u - juce::jlimit(0.f, 78.f, 20.f * std::log10(v) + 60.f) / 78.f * (r.getHeight() - 22 * u); };
    for (float f : {100.f, 1000.f, 10000.f}) {
        g.setColour(juce::Colours::white.withAlpha(.06f)); g.fillRect(X(f), r.getY(), 1.f, r.getHeight());
        g.setColour(P.mute.withAlpha(.8f)); g.setFont(mono(8.f * u));
        g.drawText(f >= 1000.f ? String((int)(f / 1000)) + "k" : String((int)f), Rectangle<float>(X(f) + 2 * u, r.getBottom() - 12 * u, 26 * u, 10 * u), juce::Justification::centredLeft);
    }
    g.setColour(juce::Colours::white.withAlpha(.10f)); g.fillRect(r.getX(), Y(1.f), r.getWidth(), 1.f);  // 0 dB
    const float fp = std::min(19000.f, fc * (1.f + p.famt * 18.f));
    if (p.famt > 0.f) {  // zone balayée par l'enveloppe de filtre
        g.setColour(kIce.withAlpha(.08f)); g.fillRect(Rectangle<float>(X(fc), r.getY(), X(fp) - X(fc), r.getHeight()));
        g.setColour(kIce.withAlpha(.35f)); g.fillRect(X(fp), r.getY(), 1.f, r.getHeight());
    }
    auto resp = [&](float f) { const float x = f / fc; return 1.f / std::sqrt(std::pow(1.f - x * x, 2.f) + std::pow(x / Q, 2.f)); };
    juce::Path ln;
    const int steps = (int)ww;
    for (int i = 0; i <= steps; ++i) { const float f = 20.f * std::pow(1000.f, (float)i / (float)steps); const float y = Y(resp(f)); if (i) ln.lineTo(x0 + (float)i, y); else ln.startNewSubPath(x0, y); }
    juce::Path fill(ln);
    fill.lineTo(x0 + ww, r.getBottom()); fill.lineTo(x0, r.getBottom()); fill.closeSubPath();
    g.setGradientFill(juce::ColourGradient(kIce.withAlpha(.26f), 0, r.getY(), kIce.withAlpha(0.f), 0, r.getBottom(), false));
    g.fillPath(fill);
    g.setColour(kIce);
    g.strokePath(ln, juce::PathStrokeType(1.6f * u, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(P.screenInk.withAlpha(.9f)); g.setFont(mono(9.f * u));
    g.drawText(hzText(fc), Rectangle<float>(r.getRight() - 62 * u, r.getY() + 4 * u, 56 * u, 11 * u), juce::Justification::centredRight);
    g.restoreState();
}

// ---------------- Clavier ----------------
static bool isBlack(int n) { const int m = n % 12; return m == 1 || m == 3 || m == 6 || m == 8 || m == 10; }
Keys::Keys(juce::MidiKeyboardState& s, const Palette& pal) : st(s), P(pal) {}
Keys::~Keys() { release(); }

Rectangle<float> Keys::keyRect(int n) const {
    int whites = 0, idx = 0;
    for (int i = lo; i <= hi; ++i) { if (!isBlack(i)) { ++whites; if (i < n) ++idx; } }
    auto in = getLocalBounds().toFloat().reduced(3.f);
    const float ww = in.getWidth() / (float)whites;
    if (!isBlack(n)) return {in.getX() + (float)idx * ww, in.getY(), ww, in.getHeight()};
    const float bw = ww * .62f;
    return {in.getX() + (float)idx * ww - bw * .5f, in.getY(), bw, in.getHeight() * .62f};
}
int Keys::noteAt(juce::Point<float> p, float& vel) const {
    int hit = -1;
    for (int n = lo; n <= hi && hit < 0; ++n) if (isBlack(n) && keyRect(n).contains(p)) hit = n;
    for (int n = lo; n <= hi && hit < 0; ++n) if (!isBlack(n) && keyRect(n).contains(p)) hit = n;
    if (hit >= 0) vel = juce::jlimit(.3f, 1.f, .3f + .7f * (p.y - keyRect(hit).getY()) / keyRect(hit).getHeight());
    return hit;
}
void Keys::press(const juce::MouseEvent& e) {
    float vel = .8f;
    const int n = noteAt(e.position, vel);
    if (n == cur) return;
    release();
    if (n >= 0) { cur = n; st.noteOn(1, n, vel); }
}
void Keys::release() { if (cur >= 0) { st.noteOff(1, cur, 0.f); cur = -1; } }

void Keys::paint(juce::Graphics& g) {
    const float u = (float)getHeight() / 126.f;
    auto b = getLocalBounds().toFloat();
    g.setColour(juce::Colours::black.withAlpha(.65f));
    g.fillRoundedRectangle(b, 5.f * u);
    g.setColour(juce::Colours::white.withAlpha(.08f));
    g.drawRoundedRectangle(b.reduced(.5f), 5.f * u, 1.f);
    for (int n = lo; n <= hi; ++n) {
        if (isBlack(n)) continue;
        const bool dn = st.isNoteOnForChannels(0xffff, n);
        auto k = keyRect(n).reduced(.7f, 0.f).withTrimmedBottom(1.f);
        g.setGradientFill(dn ? juce::ColourGradient(kLilac, k.getX(), k.getY(), Colour(0xFF9D92E0), k.getX(), k.getBottom(), false)
                             : juce::ColourGradient(Colour(0xFFF3F1F8), k.getX(), k.getY(), Colour(0xFFC4C1D0), k.getX(), k.getBottom(), false));
        g.fillRoundedRectangle(k, 3.f * u);
        g.setColour(juce::Colours::black.withAlpha(.25f));
        g.drawRoundedRectangle(k, 3.f * u, 1.f);
        if (n % 12 == 0) {
            g.setColour(Colour(0xFF5E5B6C));
            g.setFont(mono(8.5f * u));
            g.drawText("C" + String(n / 12 - 1), k.withTop(k.getBottom() - 16 * u), juce::Justification::centred);
        }
    }
    for (int n = lo; n <= hi; ++n) {
        if (!isBlack(n)) continue;
        const bool dn = st.isNoteOnForChannels(0xffff, n);
        auto k = keyRect(n);
        g.setColour(juce::Colours::black.withAlpha(.5f));
        g.fillRoundedRectangle(k.translated(0, 2.f * u), 2.5f * u);
        g.setGradientFill(dn ? juce::ColourGradient(kLilac.darker(.4f), k.getX(), k.getY(), kLilac.darker(1.4f), k.getX(), k.getBottom(), false)
                             : juce::ColourGradient(Colour(0xFF34343B), k.getX(), k.getY(), Colour(0xFF0B0B0E), k.getX(), k.getBottom(), false));
        g.fillRoundedRectangle(k, 2.5f * u);
        g.setColour(juce::Colours::white.withAlpha(dn ? .3f : .10f));
        g.fillRect(k.getX() + 2 * u, k.getY() + 1.f, k.getWidth() - 4 * u, 1.2f);
    }
}

// ---------------- Molettes ----------------
Wheel::Wheel(std::atomic<float>& v, float lo, float hi, bool sp, const Palette& pal) : val(v), lo_(lo), hi_(hi), spring(sp), P(pal) {
    setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
}
void Wheel::drag(const juce::MouseEvent& e) {
    const float t = 1.f - juce::jlimit(0.f, 1.f, (e.position.y - 8.f) / ((float)getHeight() - 16.f));
    val = lo_ + t * (hi_ - lo_);
    repaint();
}
void Wheel::paint(juce::Graphics& g) {
    const float u = (float)getWidth() / 36.f;
    auto b = getLocalBounds().toFloat();
    g.setColour(juce::Colours::black.withAlpha(.7f));
    g.fillRoundedRectangle(b, 5.f * u);
    g.setColour(juce::Colours::white.withAlpha(.08f));
    g.drawRoundedRectangle(b.reduced(.5f), 5.f * u, 1.f);
    auto body = b.reduced(5.f * u, 6.f * u);
    g.setGradientFill(juce::ColourGradient(Colour(0xFF121215), body.getX(), 0, Colour(0xFF121215), body.getRight(), 0, false));
    { juce::ColourGradient cg(Colour(0xFF121215), body.getX(), 0, Colour(0xFF121215), body.getRight(), 0, false); cg.addColour(.5, Colour(0xFF5C5C63)); g.setGradientFill(cg); }
    g.fillRoundedRectangle(body, 4.f * u);
    const float v = (val.load() - lo_) / (hi_ - lo_);  // 0..1
    g.saveState();
    g.reduceClipRegion(body.toNearestInt());
    const float step = 7.f * u, off = std::fmod(v * 150.f * u, step);
    for (float y = body.getY() - step + off; y < body.getBottom(); y += step) {
        g.setColour(juce::Colours::black.withAlpha(.5f)); g.fillRect(body.getX(), y, body.getWidth(), 1.5f * u);
        g.setColour(juce::Colours::white.withAlpha(.10f)); g.fillRect(body.getX(), y + 1.5f * u, body.getWidth(), 1.f);
    }
    g.setGradientFill(juce::ColourGradient(juce::Colours::black.withAlpha(.55f), 0, body.getY(), juce::Colours::transparentBlack, 0, body.getY() + 16 * u, false));
    g.fillRect(body.withHeight(16 * u));
    g.setGradientFill(juce::ColourGradient(juce::Colours::black.withAlpha(.55f), 0, body.getBottom(), juce::Colours::transparentBlack, 0, body.getBottom() - 16 * u, false));
    g.fillRect(body.withTop(body.getBottom() - 16 * u));
    g.restoreState();
    const float ny = body.getBottom() - 6.f * u - v * (body.getHeight() - 12.f * u);  // repère lumineux
    g.setColour(P.accent);
    g.fillRoundedRectangle(body.getX() + 3 * u, ny - 1.2f * u, body.getWidth() - 6 * u, 2.4f * u, 1.2f * u);
    if (spring) { g.setColour(P.ink.withAlpha(.5f)); g.fillRect(b.getX() + 1.5f * u, body.getCentreY() - .6f, 3 * u, 1.2f); g.fillRect(b.getRight() - 4.5f * u, body.getCentreY() - .6f, 3 * u, 1.2f); }
}
}  // namespace osui

// ---------------- Éditeur ----------------
Palette OrbeSynthEditor::palette() {
    Palette p;
    p.plateTop = Colour(0xFF2B2B31); p.plateBot = Colour(0xFF16161A);
    p.ink = Colour(0xFFE9E7F0); p.mute = Colour(0xFF8F8D9B); p.line = Colour(0xFF3B3B44);
    p.accent = kLilac; p.accent2 = kIce;
    p.screen = Colour(0xFF0B0C11); p.screenInk = Colour(0xFFEDEBF6);
    p.brush = 1.f;
    return p;
}

OrbeSynthEditor::OrbeSynthEditor(OrbeSynthProcessor& p)
    : AudioProcessorEditor(&p), proc(p), pal(palette()), look(pal), presetBar(p, pal), envView(p, pal), fltView(p, pal),
      keyboard(p.keys, pal), bendWheel(p.bend, -1.f, 1.f, true, pal), modWheel(p.mod, 0.f, 1.f, false, pal) {
    setLookAndFeel(&look);
    auto& ap = proc.apvts;
    addAndMakeVisible(presetBar);
    const juce::StringArray waves{"Sinus", "Triangle", "Scie", u8("Carré")};
    wave1 = std::make_unique<osui::Seg>(*ap.getParameter("w1"), waves, true, pal);
    wave2 = std::make_unique<osui::Seg>(*ap.getParameter("w2"), waves, true, pal);
    wave1->setTooltip(u8("Forme d'onde de l'oscillateur 1 (bande limitée PolyBLEP)"));
    wave2->setTooltip(u8("Forme d'onde de l'oscillateur 2 ; sert aussi de modulateur pour la FM"));
    divSeg = std::make_unique<osui::Seg>(*ap.getParameter("dlydiv"), juce::StringArray{"1/16", "1/8", u8("1/8 p."), "1/4", u8("1/4 p."), "1/2"}, false, pal);
    divSeg->setTooltip(u8("Durée du delay, synchronisée sur le tempo de l'hôte"));
    for (auto* c : {(juce::Component*)wave1.get(), (juce::Component*)wave2.get(), (juce::Component*)divSeg.get(), (juce::Component*)&envView, (juce::Component*)&fltView,
                    (juce::Component*)&keyboard, (juce::Component*)&bendWheel, (juce::Component*)&modWheel})
        addAndMakeVisible(c);
    bendWheel.setTooltip(u8("Pitch bend (±2 demi-tons), revient au centre")); modWheel.setTooltip(u8("Molette de modulation : ouvre le filtre"));
    keyboard.setTooltip(u8("Clavier : clic et glissé pour jouer"));
    struct K { const char* id; const char* label; const char* tip; };
    const K ks[] = {
        {"semi", "Transpo.", "Transposition de l'oscillateur 2, en demi-tons"}, {"mix", "Niveau 2", "Niveau de l'oscillateur 2 dans le mélange"},
        {"fm", "FM", "Modulation de fréquence de l'oscillateur 1 par l'oscillateur 2 (attaque brillante qui se calme)"},
        {"uni", "Unisson", "Nombre de voix superposées par note (1 à 7), réparties dans le champ stéréo"}, {"det", "Désaccord", "Écart entre les voix d'unisson, en centièmes de demi-ton"},
        {"sub", "Sub", "Sinus une octave plus bas, qui contourne le filtre"},
        {"cut", "Coupure", "Fréquence de coupure du filtre passe-bas"}, {"res", "Résonance", "Résonance du filtre"},
        {"famt", "Enveloppe", "Quantité d'enveloppe de filtre : ouverture au début de la note"}, {"fdec", "Déclin", "Durée de retombée de l'enveloppe de filtre"},
        {"a", "Attaque", "Temps de montée du volume"}, {"d", "Déclin", "Temps de descente vers le sustain"},
        {"s", "Sustain", "Niveau tenu tant que la touche est enfoncée"}, {"r", "Relâche", "Durée d'extinction après le relâchement"},
        {"drive", "Saturation", "Saturation douce (tanh) sur l'ensemble des voix"}, {"cho", "Chorus", "Chorus stéréo à deux voix"},
        {"dly", "Delay", "Niveau du delay synchronisé sur le tempo"}, {"rev", "Reverb", "Niveau de la réverbération"}, {"vol", "Volume", "Volume de sortie"},
    };
    for (auto& k : ks) { knobs.push_back(std::make_unique<osui::Knob>(ap, k.id, u8(k.label), u8(k.tip), pal)); addAndMakeVisible(*knobs.back()); }

    setResizable(true, true);
    setResizeLimits(825, 495, 2200, 1320);
    getConstrainer()->setFixedAspectRatio((double)W / (double)H);
    setSize((int)W, (int)H);
    startTimerHz(30);
}

OrbeSynthEditor::~OrbeSynthEditor() { stopTimer(); setLookAndFeel(nullptr); }

void OrbeSynthEditor::resized() {
    u_ = (float)getWidth() / W;
    presetBar.setBounds(Si(392, 12, 372, 36));
    wave1->setBounds(Si(kS1.getX() + 16, kS1.getY() + 50, 250, 28)); wave2->setBounds(Si(kS1.getX() + 294, kS1.getY() + 50, 250, 28));
    auto cell = [&](int i, float x, float y) { knobs[(size_t)i]->setBounds(Si(x, y, 86, 88)); };
    for (int i = 0; i < 6; ++i) cell(i, kS1.getX() + 16 + (float)i * 88.4f, kS1.getY() + 98);
    for (int i = 0; i < 4; ++i) cell(6 + i, kS2.getX() + 16 + (float)i * 122.f, kS2.getY() + 106);
    for (int i = 0; i < 4; ++i) cell(10 + i, kS3.getX() + 16 + (float)i * 147.3f, kS3.getY() + 106);
    for (int i = 0; i < 5; ++i) cell(14 + i, kS4.getX() + 16 + (float)i * 91.5f, kS4.getY() + 36);
    fltView.setBounds(Si(kS2.getX() + 16, kS2.getY() + 36, 452, 64));
    envView.setBounds(Si(kS3.getX() + 16, kS3.getY() + 36, 528, 64));
    divSeg->setBounds(Si(kS4.getX() + 16, kS4.getY() + 152, 300, 28));
    bendWheel.setBounds(Si(26, 518, 36, 116)); modWheel.setBounds(Si(70, 518, 36, 116));
    keyboard.setBounds(Si(122, 514, 958, 126));
    plate = juce::Image();
    repaint();
}

void OrbeSynthEditor::timerCallback() {
    const int v = proc.voices.load();
    if (v != lastVoices) { lastVoices = v; repaint(S(800, 12, 280, 36).toNearestInt()); }
    uint64_t m[2] = {0, 0};
    for (int n = 0; n < 128; ++n) if (proc.keys.isNoteOnForChannels(0xffff, n)) m[n >> 6] |= (uint64_t)1 << (n & 63);
    if (m[0] != lastMask[0] || m[1] != lastMask[1]) { lastMask[0] = m[0]; lastMask[1] = m[1]; keyboard.repaint(); }
    const float b = proc.bend.load(), md = proc.mod.load();
    if (b != lastBend) { lastBend = b; bendWheel.repaint(); }
    if (md != lastMod) { lastMod = md; modWheel.repaint(); }
    const bool mod = proc.presetModified();
    if (mod != lastModified) { lastModified = mod; }
    presetBar.repaint();
}

void OrbeSynthEditor::section(juce::Graphics& g, float u, Rectangle<float> d, const char* num, const String& title, const String& sub) {
    auto r = S(d.getX(), d.getY(), d.getWidth(), d.getHeight());
    g.setColour(juce::Colours::black.withAlpha(.22f));
    g.fillRoundedRectangle(r, 6.f * u);
    g.setColour(juce::Colours::black.withAlpha(.45f));
    g.drawRoundedRectangle(r.expanded(.5f), 6.f * u, 1.f);
    g.setColour(juce::Colours::white.withAlpha(.06f));
    g.drawRoundedRectangle(r.translated(0, 1.f).reduced(.5f), 6.f * u, 1.f);
    g.setColour(kLilac);
    g.setFont(mono(11.f * u));
    g.drawText(num, S(d.getX() + 16, d.getY() + 8, 24, 16).toNearestInt(), juce::Justification::centredLeft);
    engraved(g, pal, title, S(d.getX() + 42, d.getY() + 8, 260, 16), juce::Justification::centredLeft, 11.f * u, 2.2f * u, true);
    g.setColour(pal.mute);
    g.setFont(fnt(9.5f * u));
    g.drawText(sub, S(d.getRight() - 300, d.getY() + 8, 284, 16).toNearestInt(), juce::Justification::centredRight);
    g.setColour(juce::Colours::black.withAlpha(.5f)); g.fillRect(S(d.getX() + 16, d.getY() + 29, d.getWidth() - 32, 1.f));
    g.setColour(juce::Colours::white.withAlpha(.06f)); g.fillRect(S(d.getX() + 16, d.getY() + 30, d.getWidth() - 32, 1.f));
}

void OrbeSynthEditor::paint(juce::Graphics& g) {
    const float u = u_;
    if (plate.isNull() || plate.getWidth() != getWidth() || plate.getHeight() != getHeight()) plate = brushedPlate(getWidth(), getHeight(), pal);
    g.drawImageAt(plate, 0, 0);
    for (float y : {60.f, 502.f}) {
        g.setColour(Colour(0xFF000000).withAlpha(.5f)); g.fillRect(S(0, y, W, 1.5f));
        g.setColour(juce::Colours::white.withAlpha(.06f)); g.fillRect(S(0, y + 1.5f, W, 1.f));
    }
    for (auto p : {juce::Point<float>(12, 12), {1088, 12}, {12, 648}, {1088, 648}}) screw(g, p.x * u, p.y * u, 5.f * u, (p.x + p.y) * .9f);
    paintHeader(g, u);

    section(g, u, kS1, "01", "Oscillateurs", u8("Deux oscillateurs · unisson · FM · sub"));
    engraved(g, pal, "Oscillateur 1", S(kS1.getX() + 16, kS1.getY() + 36, 250, 12), juce::Justification::centredLeft, 9.f * u, 1.8f * u);
    engraved(g, pal, "Oscillateur 2", S(kS1.getX() + 294, kS1.getY() + 36, 250, 12), juce::Justification::centredLeft, 9.f * u, 1.8f * u);
    section(g, u, kS2, "02", "Filtre", u8("Passe-bas résonant · 12 dB/oct"));
    section(g, u, kS3, "03", "Enveloppe", u8("ADSR exponentiel · volume"));
    section(g, u, kS4, "04", "Effets", u8("Saturation · chorus · delay · reverb"));
    engraved(g, pal, u8("Valeur du delay"), S(kS4.getX() + 16, kS4.getY() + 136, 200, 12), juce::Justification::centredLeft, 9.f * u, 1.8f * u);
    const float bpm = proc.hostBpm.load() > 0.f ? proc.hostBpm.load() : 120.f;
    const float ms = 60000.f / bpm * osyn::divBeats()[proc.readParams().dlyDiv];
    g.setColour(kIce.withAlpha(.85f)); g.setFont(mono(10.f * u));
    g.drawText(String(juce::roundToInt(ms)) + " ms @ " + String(juce::roundToInt(bpm)) + " BPM", S(kS4.getX() + 336, kS4.getY() + 152, 132, 28).toNearestInt(), juce::Justification::centredRight);
    engraved(g, pal, "Bend", S(18, 636, 52, 12), juce::Justification::centred, 8.5f * u, 1.6f * u);
    engraved(g, pal, "Mod", S(62, 636, 52, 12), juce::Justification::centred, 8.5f * u, 1.6f * u);
}

void OrbeSynthEditor::paintHeader(juce::Graphics& g, float u) {
    const float cx = 44 * u, cy = 31 * u, r = 16 * u;  // l'orbe : sphère éclairée entourée d'un anneau, signature de la marque
    g.setColour(juce::Colours::black.withAlpha(.5f)); g.fillEllipse(cx - r - 3 * u, cy - r - 1 * u, (r + 3 * u) * 2, (r + 3 * u) * 2);
    g.setGradientFill(juce::ColourGradient(kIce, cx - r * .5f, cy - r * .6f, Colour(0xFF5A46C8), cx + r, cy + r, true));
    g.fillEllipse(cx - r, cy - r, r * 2, r * 2);
    g.setColour(juce::Colours::white.withAlpha(.5f)); g.fillEllipse(cx - r * .62f, cy - r * .78f, r * .7f, r * .42f);
    g.setColour(kLilac.withAlpha(.8f)); g.drawEllipse(cx - r * 1.28f, cy - r * 1.28f, r * 2.56f, r * 2.56f, 1.f * u);
    engraved(g, pal, "Orbe Synth", S(80, 12, 300, 30), juce::Justification::centredLeft, 24.f * u, 6.5f * u, true);
    g.setColour(pal.mute); g.setFont(fnt(9.f * u));
    g.drawText(u8("dreamaws · suite Orbe · synthé polyphonique"), S(82, 42, 300, 12).toNearestInt(), juce::Justification::centredLeft);

    screenWell(g, pal, S(804, 14, 272, 32));
    engraved(g, pal, "Voix", S(818, 22, 40, 16), juce::Justification::centredLeft, 9.f * u, 1.8f * u);
    const int v = proc.voices.load();
    for (int i = 0; i < osyn::kMaxVoices; ++i) {
        const float x = (866.f + (float)i * 10.5f) * u, y = 30.f * u, rr = 2.6f * u;
        if (i < v) {
            g.setGradientFill(juce::ColourGradient(kLilac.withAlpha(.4f), x, y, kLilac.withAlpha(0.f), x + rr * 3, y, true));
            g.fillEllipse(x - rr * 3, y - rr * 3, rr * 6, rr * 6);
        }
        g.setColour(i < v ? kLilac : Colour(0xFF23232B));
        g.fillEllipse(x - rr, y - rr, rr * 2, rr * 2);
    }
    g.setColour(kIce); g.setFont(mono(11.f * u));
    g.drawText(String(v).paddedLeft('0', 2) + "/" + String(osyn::kMaxVoices), S(1030, 22, 42, 16).toNearestInt(), juce::Justification::centredRight);
}
