#include "DetectEditor.h"

using namespace ou;
using kit::u8;

namespace {
String mmss(double s) { const int t = (int)s; return String(t / 60) + ":" + String(t % 60).paddedLeft('0', 2); }
void txt(juce::Graphics& g, const String& s, juce::Rectangle<float> r, float h, juce::Colour c, juce::Justification j = juce::Justification::centredLeft, bool bold = false, bool mono = false) {
    g.setColour(c); g.setFont(mono ? kit::mono(h) : kit::fnt(h, bold)); g.drawText(s, r, j, true);
}
}  // namespace

DetectEditor::DetectEditor(DetectProcessor& p)
    : Base(p, 1000.f, 640.f, u8("DÉTECT"), u8("Tempo · tonalité · roue de Camelot · accordage du 808")), proc(p), cur(p.result()), halfB(u8("×½")) {
    captureB.setButtonText("Capturer"); addAndMakeVisible(captureB);
    capAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, "capture", captureB);
    for (auto* b : {&clearB, &analyzeB, &halfB, &dblB, &tapB, &applyB}) addAndMakeVisible(*b);
    applyB.setButtonText(u8("Appliquer le tap")); applyB.setVisible(false);
    captureB.setTooltip(u8("Enregistre l'audio entrant (jusqu'à 2 minutes, silence ignoré)"));
    clearB.setTooltip(u8("Efface la capture")); analyzeB.setTooltip(u8("Analyse tempo et tonalité de la capture (5 s minimum)"));
    halfB.setTooltip(u8("Divise le tempo par deux")); dblB.setTooltip(u8("Double le tempo"));
    tapB.setTooltip(u8("Tape le tempo en rythme : 3 frappes ou plus"));
    clearB.onClick = [this] { proc.clearCapture(); };
    analyzeB.onClick = [this] { proc.analyze(); };
    halfB.onClick = [this] { proc.scaleBpm(0.5); };
    dblB.onClick = [this] { proc.scaleBpm(2.0); };
    tapB.onClick = [this] { tap(); };
    applyB.onClick = [this] { if (tapBpm > 0) proc.setBpm(tapBpm); applyB.setVisible(false); };
    start(24);
}

void DetectEditor::layout() {
    captureB.setBounds(Ri(200, 114, 114, 22)); clearB.setBounds(Ri(200, 140, 114, 24));
    analyzeB.setBounds(Ri(36, 224, 278, 28));
    halfB.setBounds(Ri(36, 424, 134, 28)); dblB.setBounds(Ri(180, 424, 134, 28));
    tapB.setBounds(Ri(36, 462, 134, 44)); applyB.setBounds(Ri(36, 514, 278, 24));
}

void DetectEditor::tap() {
    const double now = juce::Time::getMillisecondCounterHiRes();
    if (!taps.empty() && now - taps.back() > 2000) taps.clear();
    taps.push_back(now); if (taps.size() > 8) taps.erase(taps.begin());
    if (taps.size() >= 3) { const double iv = (taps.back() - taps.front()) / (double)(taps.size() - 1); tapBpm = std::round(60000.0 / iv * 10) / 10; }
    else tapBpm = 0;
}

void DetectEditor::timerCallback() {
    cur = proc.result(); capSec = proc.captureSeconds(); busy = proc.busy(); full = proc.captureFull(); hostBpm = proc.hostBpm.load();
    if (!taps.empty() && juce::Time::getMillisecondCounterHiRes() - taps.back() > 2000 && tapBpm == 0) taps.clear();
    analyzeB.setButtonText(busy ? u8("Analyse…") : u8("Analyser")); analyzeB.setToggleState(busy, juce::dontSendNotification);
    analyzeB.setEnabled(!busy && capSec >= DetectProcessor::kMinSec); clearB.setEnabled(!busy && capSec > 0);
    halfB.setEnabled(cur->valid); dblB.setEnabled(cur->valid);
    applyB.setVisible(cur->valid && tapBpm > 0);
    repaint();
}

std::string DetectEditor::activeCode() const {
    if (!focus.empty()) return focus;
    return cur->valid ? oa::camelot(cur->pc, cur->minor) : std::string();
}

std::string DetectEditor::wheelHit(juce::Point<float> p) const {
    const auto w = wheelRect(); const float cx = w.getCentreX(), cy = w.getCentreY(), R2 = w.getWidth() / 2 - 6 * u_, R1 = R2 * .7f, R0 = R2 * .4f;
    const float x = p.x - cx, y = p.y - cy, d = std::hypot(x, y);
    if (d < R0 || d > R2) return {};
    float a = std::atan2(y, x) + juce::MathConstants<float>::halfPi; a = std::fmod(a + juce::MathConstants<float>::twoPi * 2, juce::MathConstants<float>::twoPi);
    int n = juce::roundToInt(a / (juce::MathConstants<float>::pi / 6)) % 12; if (n == 0) n = 12;
    return std::to_string(n) + (d < R1 ? "A" : "B");
}
void DetectEditor::mouseMove(const juce::MouseEvent& e) { const auto h = wheelHit(e.position); if (h != hover) { hover = h; repaint(); } }
void DetectEditor::mouseDown(const juce::MouseEvent& e) { const auto h = wheelHit(e.position); if (h.empty()) return; focus = (focus == h) ? std::string() : h; repaint(); }

void DetectEditor::body(juce::Graphics& g) {
    const float u = u_; const auto& r = *cur; const bool ok = r.valid;
    // ---- en-tête : afficheur d'état
    kit::screenWell(g, pal, R(640, 16, 336, 38));
    String st = busy ? u8("ANALYSE EN COURS…") : ok ? fr(r.bpm) + u8(" BPM") + kDot + T(oa::keyShort(r.pc, r.minor)) + kDot + T(oa::camelot(r.pc, r.minor)) : u8("EN ATTENTE") + kDot + mmss(capSec) + " / 2:00";
    txt(g, st, R(654, 16, 310, 38), 13.f * u, kIce, juce::Justification::centredLeft, false, true);

    // ---- capture
    panel(g, pal, R(20, 82, 310, 180), "Capture", u);
    kit::screenWell(g, pal, R(36, 112, 150, 50));
    lcd(g, R(40, 114, 142, 46), mmss(capSec), 34.f * u, full ? kWarn : kIce);
    txt(g, "/ 2:00", R(138, 146, 44, 12), 8.5f * u, pal.mute, juce::Justification::centredRight, false, true);
    kit::screenWell(g, pal, R(36, 172, 278, 44));
    {
        const int n = proc.peakCount(); const auto w = R(36, 172, 278, 44).reduced(4.f * u, 3.f * u); const int span = juce::jmax(n, 200);
        for (int x = 0; x < (int)w.getWidth(); ++x) {
            const int i = (int)((float)x / w.getWidth() * (float)span); if (i >= n) break;
            const float a = juce::jlimit(0.f, 1.f, std::sqrt(proc.peakAt(i)) * 1.1f) * w.getHeight() * .5f;
            g.setColour(kLilac.withAlpha(.85f)); g.fillRect(w.getX() + (float)x, w.getCentreY() - a, 1.f, juce::jmax(1.f, a * 2));
        }
        if (n == 0) txt(g, u8("Lance la lecture dans l'hôte"), w, 10.f * u, pal.mute.withAlpha(.8f), juce::Justification::centred);
    }

    // ---- tempo
    panel(g, pal, R(20, 276, 310, 344), "Tempo", u);
    kit::screenWell(g, pal, R(36, 306, 278, 84));
    lcd(g, R(44, 310, 196, 76), ok ? fr(r.bpm) : "---,-", 56.f * u, ok ? kIce : kIce.withAlpha(.5f));
    txt(g, "BPM", R(244, 358, 60, 20), 12.f * u, pal.mute, juce::Justification::centredLeft, true);
    if (ok) {
        pill(g, pal, 36 * u, 398 * u, u, confColour(r.bconf), confLabel(r.bconf), 80.f);
        if (r.nAlt) { String a = u8("Aussi : "); for (int i = 0; i < r.nAlt; ++i) a += (i ? u8(" · ") : String()) + fr(r.alt[i]); txt(g, a, R(130, 396, 184, 18), 10.f * u, pal.mute, juce::Justification::centredRight, false, true); }
    } else txt(g, u8("Capture au moins 5 secondes, puis Analyser"), R(36, 396, 278, 18), 10.f * u, pal.mute);
    kit::screenWell(g, pal, R(180, 462, 134, 44));
    lcd(g, R(184, 464, 126, 40), tapBpm > 0 ? fr(tapBpm) : (taps.empty() ? "---,-" : "..."), 24.f * u, kPeach);
    if (tapBpm > 0 || !taps.empty()) txt(g, "TAP", R(186, 464, 40, 12), 8.f * u, pal.mute, juce::Justification::centredLeft, true);
    // comparaison avec le tempo de l'hôte
    auto hc = R(36, 548, 278, 58);
    g.setColour(juce::Colours::black.withAlpha(.3f)); g.fillRoundedRectangle(hc, 5.f * u);
    kit::engraved(g, pal, u8("Tempo de l'hôte"), R(46, 553, 140, 12), juce::Justification::centredLeft, 8.5f * u, 1.6f * u);
    txt(g, hostBpm > 0 ? fr(hostBpm) + " BPM" : "--", R(190, 551, 114, 16), 12.5f * u, kIce, juce::Justification::centredRight, false, true);
    {
        String v; juce::Colour c = pal.mute;
        if (hostBpm <= 0) v = u8("Inconnu : lance la lecture dans l'hôte.");
        else if (!ok) v = u8("Analyse d'abord un extrait.");
        else {
            const double d = (r.bpm - hostBpm) / hostBpm;
            double best = 1e9; int mult = 0; const double m[3] = {1.0, 0.5, 2.0};
            for (int i = 0; i < 3; ++i) { const double dd = std::abs(r.bpm * m[i] - hostBpm) / hostBpm; if (dd < best) { best = dd; mult = i; } }
            if (std::abs(d) < .005) { v = u8("Identique au projet."); c = kOk; }
            else if (mult == 0 && best <= .06) { v = u8("Proche du projet : ") + fr(r.bpm - hostBpm) + " BPM (" + fr(std::abs(d) * 100) + " %)."; c = kOk; }
            else if (mult != 0 && best <= .06) { v = u8(mult == 1 ? "Même tempo en moitié : essaie ×2." : "Même tempo en double : essaie ×½."); c = kWarn; }
            else { v = u8("Écart de ") + fr(r.bpm - hostBpm) + u8(" BPM avec le projet."); c = kBad; }
        }
        if (c != pal.mute) kit::led(g, 52 * u, 592 * u, 3.5f * u, true, c);
        txt(g, v, R(c != pal.mute ? 62.f : 46.f, 582, 250, 20), 10.5f * u, c != pal.mute ? pal.ink.withAlpha(.9f) : pal.mute);
    }

    // ---- tonalité
    panel(g, pal, R(346, 82, 306, 278), u8("Tonalité"), u);
    kit::screenWell(g, pal, R(362, 112, 274, 64));
    lcd(g, R(368, 116, 262, 56), ok ? T(oa::keyLong(r.pc, r.minor)) : "---", 30.f * u, ok ? kLilac : kLilac.withAlpha(.5f), juce::Justification::centred);
    const std::string cam = ok ? oa::camelot(r.pc, r.minor) : std::string();
    {
        auto chip = R(362, 188, 64, 30);
        g.setColour(ok ? camColour(std::atoi(cam.c_str())) : pal.line); g.fillRoundedRectangle(chip, 5.f * u);
        g.setColour(juce::Colours::black.withAlpha(.55f)); g.drawRoundedRectangle(chip, 5.f * u, 1.f);
        txt(g, ok ? T(cam) : "--", chip, 16.f * u, Colour(0xFF101014), juce::Justification::centred, true, true);
        if (ok) { pill(g, pal, 440 * u, 188 * u, u, confColour(r.kconf), confLabel(r.kconf), 90.f);
                  txt(g, u8("Camelot"), R(440, 206, 80, 12), 8.5f * u, pal.mute); }
        if (ok) { const int p1 = r.rankPc[1]; const bool m1 = r.rankMinor[1];
                  txt(g, u8("Sinon : ") + T(oa::keyLong(p1, m1)) + kDot + T(oa::camelot(p1, m1)), R(362, 226, 274, 16), 11.f * u, pal.ink.withAlpha(.8f)); }
        else txt(g, u8("Aucune analyse"), R(362, 226, 274, 16), 11.f * u, pal.mute);
    }
    {   // chromagramme
        const auto a = R(362, 252, 274, 96); const float bw = a.getWidth() / 12.f;
        const int* iv = ok && r.minor ? oa::kMinor : oa::kMajor;
        for (int i = 0; i < 12; ++i) {
            const int rel = ok ? ((i - r.pc) % 12 + 12) % 12 : -1; bool inScale = false; for (int k = 0; k < 7; ++k) if (iv[k] == rel) inScale = true;
            const float v = ok ? (float)r.chroma[i] : 0.f; const float hh = (a.getHeight() - 18 * u) * juce::jmax(.02f, v);
            auto b = juce::Rectangle<float>(a.getX() + bw * (float)i + 3 * u, a.getBottom() - 16 * u - hh, bw - 6 * u, hh);
            g.setColour(juce::Colours::black.withAlpha(.35f)); g.fillRoundedRectangle(b.withTop(a.getY()).withHeight(a.getHeight() - 16 * u), 2.f * u);
            g.setColour(!ok ? pal.line : rel == 0 ? kIce : inScale ? kLilac.withAlpha(.8f) : pal.mute.withAlpha(.5f)); g.fillRoundedRectangle(b, 2.f * u);
            txt(g, note(i), R(a.getX() / u + bw / u * (float)i - 2, 334, bw / u + 4, 14), 8.5f * u, rel == 0 ? kIce : pal.mute, juce::Justification::centred, rel == 0);
        }
    }

    // ---- 808
    const std::string code = activeCode(); int kpc = 0; bool kmin = false; const bool haveKey = !code.empty() && oa::fromCamelot(code, kpc, kmin);
    panel(g, pal, R(346, 372, 306, 248), u8("808 · Accordage"), u);
    if (haveKey) {
        g.setColour(pal.ink.withAlpha(.92f)); g.setFont(kit::fnt(12.f * u, true));
        g.drawText(u8("Accorde la fondamentale de ton 808 sur ") + note(kpc) + ".", R(362, 402, 274, 18).toNearestInt(), juce::Justification::centredLeft, true);
        g.setColour(pal.mute); g.setFont(kit::fnt(10.f * u));
        g.drawFittedText(u8("Les autres notes de la gamme servent pour les glissés et les changements d'accord."), R(362, 422, 274, 28).toNearestInt(), juce::Justification::topLeft, 2);
        const int* iv = kmin ? oa::kMinor : oa::kMajor; const int r0 = 24 + kpc;
        for (int i = 0; i < 7; ++i) {
            const int midi = r0 + iv[i]; auto t = R(362.f + (float)(i % 4) * 70.f, 462.f + (float)(i / 4) * 74.f, 64, 62);
            const bool root = i == 0;
            g.setColour(juce::Colours::black.withAlpha(.35f)); g.fillRoundedRectangle(t, 5.f * u);
            if (root) { g.setColour(kPeach.withAlpha(.18f)); g.fillRoundedRectangle(t, 5.f * u); }
            g.setColour(root ? kPeach.withAlpha(.9f) : juce::Colours::white.withAlpha(.1f)); g.drawRoundedRectangle(t.reduced(.5f), 5.f * u, 1.f);
            txt(g, noteOct(midi), t.withTrimmedTop(6 * u).withHeight(26 * u), 19.f * u, root ? kPeach : pal.ink, juce::Justification::centred, false, true);
            txt(g, fr(oa::mtof(midi)) + " Hz", t.withTrimmedTop(34 * u).withHeight(12 * u), 8.5f * u, pal.mute, juce::Justification::centred, false, true);
            txt(g, root ? u8("FONDAMENTALE") : "MIDI " + String(midi), t.withTrimmedTop(48 * u).withHeight(11 * u), 7.f * u, root ? kPeach.withAlpha(.9f) : pal.mute.withAlpha(.7f), juce::Justification::centred, true);
        }
        g.setColour(pal.mute.withAlpha(.9f)); g.setFont(kit::fnt(9.f * u)); g.drawFittedText(u8("Fondamentale : note MIDI 24 + hauteur de la tonique."), R(362 + 3 * 70 + 2, 540, 62, 54).toNearestInt(), juce::Justification::topLeft, 5);
    } else txt(g, u8("Analyse un extrait, ou choisis une case sur la roue."), R(362, 410, 274, 40), 11.f * u, pal.mute);

    // ---- roue de Camelot
    panel(g, pal, R(668, 82, 312, 338), "Roue de Camelot", u);
    {
        const auto w = wheelRect(); const float cx = w.getCentreX(), cy = w.getCentreY(), R2 = w.getWidth() / 2 - 6 * u, R1 = R2 * .7f, R0 = R2 * .4f;
        const auto comp = code.empty() ? std::array<std::string, 4>{} : oa::compatCodes(code);
        for (int n = 1; n <= 12; ++n) for (int L = 0; L < 2; ++L) {
            const std::string c = std::to_string(n) + (L ? "B" : "A");
            const float a = -juce::MathConstants<float>::halfPi + (float)(n % 12) * juce::MathConstants<float>::pi / 6.f, half = juce::MathConstants<float>::pi / 12.f - .012f;
            const float ri = L ? R1 + 2 * u : R0, ro = L ? R2 : R1 - 2 * u;
            const bool isSel = c == code, isComp = !isSel && std::find(comp.begin(), comp.end(), c) != comp.end(), hov = c == hover;
            juce::Path p; p.addPieSegment(cx - ro, cy - ro, ro * 2, ro * 2, a - half + juce::MathConstants<float>::halfPi, a + half + juce::MathConstants<float>::halfPi, ri / ro);
            const float hue = (float)oa::camelotHue(n) / 360.f;
            g.setColour(isSel ? Colour::fromHSL(hue, .7f, .78f, .75f) : isComp ? Colour::fromHSL(hue, .7f, .78f, .28f) : Colour::fromHSL(hue, .4f, .7f, .07f)); g.fillPath(p);
            g.setColour(hov ? juce::Colours::white.withAlpha(.9f) : isComp || isSel ? Colour::fromHSL(hue, .7f, .78f, .8f) : juce::Colours::white.withAlpha(.1f)); g.strokePath(p, juce::PathStrokeType(hov || isComp ? 1.2f : 1.f));
            const float rm = (ri + ro) / 2, x = cx + std::cos(a) * rm, y = cy + std::sin(a) * rm; int kp = 0; bool km = false; oa::fromCamelot(c, kp, km);
            txt(g, T(c), juce::Rectangle<float>(x - 20 * u, y - 11 * u, 40 * u, 12 * u), juce::jmax(9.f, w.getWidth() / 28.f), isSel ? Colour(0xFF08090B) : pal.ink.withAlpha(isComp ? .95f : .6f), juce::Justification::centred, false, true);
            txt(g, T(oa::keyShort(kp, km)), juce::Rectangle<float>(x - 20 * u, y + 1 * u, 40 * u, 11 * u), 8.5f * u, isSel ? Colour(0xFF08090B).withAlpha(.75f) : pal.mute, juce::Justification::centred);
            if (ok && c == oa::camelot(r.pc, r.minor)) { g.setColour(isSel ? Colour(0xFF08090B) : kIce); g.fillEllipse(x - 2.5f * u, y + 12 * u, 5 * u, 5 * u); }
        }
        txt(g, code.empty() ? "--" : T(code), juce::Rectangle<float>(cx - R0, cy - 18 * u, R0 * 2, 32 * u), 28.f * u, pal.ink.withAlpha(.9f), juce::Justification::centred, true, true);
        txt(g, code.empty() ? "CAMELOT" : focus.empty() ? u8("DÉTECTÉ") : u8("CHOISIE"), juce::Rectangle<float>(cx - R0, cy + 14 * u, R0 * 2, 12 * u), 8.f * u, pal.mute, juce::Justification::centred, true);
        int hp = 0; bool hm = false;
        txt(g, !hover.empty() && oa::fromCamelot(hover, hp, hm) ? T(hover) + kDot + T(oa::keyLong(hp, hm)) : u8("Touche une case pour voir ses voisines"), R(680, 402, 288, 14), 10.f * u, pal.mute, juce::Justification::centred);
    }

    // ---- compatibles
    panel(g, pal, R(668, 432, 312, 188), code.empty() ? "Compatibles" : "Compatibles avec " + T(code), u);
    if (!code.empty()) {
        const auto comp = oa::compatCodes(code); const char* why[4] = {"même tonalité", "−1 · plus calme", "+1 · plus d'énergie", "relative · autre couleur"};
        for (int i = 0; i < 4; ++i) {
            int p = 0; bool m = false; oa::fromCamelot(comp[(size_t)i], p, m); const float y = 464.f + (float)i * 36.f;
            auto chip = R(684, y, 44, 26); g.setColour(camColour(std::atoi(comp[(size_t)i].c_str()))); g.fillRoundedRectangle(chip, 4.f * u);
            txt(g, T(comp[(size_t)i]), chip, 12.f * u, Colour(0xFF101014), juce::Justification::centred, true, true);
            txt(g, T(oa::keyLong(p, m)), R(738, y, 230, 14), 12.f * u, pal.ink.withAlpha(.92f), juce::Justification::centredLeft, true);
            txt(g, u8(why[i]), R(738, y + 15, 230, 12), 9.f * u, pal.mute);
        }
    } else txt(g, u8("Aucune tonalité choisie."), R(684, 470, 280, 20), 11.f * u, pal.mute);
}
