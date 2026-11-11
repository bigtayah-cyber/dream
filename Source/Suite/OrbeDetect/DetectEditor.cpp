#include "DetectEditor.h"
#include "../../Kit/KeyClipboard.h"

using namespace ou;
using kit::u8;
namespace v3 = kit::v3;

namespace {
String mmss(double s) { const int t = (int)s; return String(t / 60) + ":" + String(t % 60).paddedLeft('0', 2); }
void txt(juce::Graphics& g, const String& s, juce::Rectangle<float> r, float h, juce::Colour c, juce::Justification j = juce::Justification::centredLeft, bool bold = false, bool mono = false) {
    g.setColour(c); g.setFont(mono ? kit::mono(h) : kit::fnt(h, bold)); g.drawText(s, r, j, true);
}
}  // namespace

DetectEditor::DetectEditor(DetectProcessor& p)
    : Base3(p, 1000.f, 640.f, 1180.f, 762.f, u8("Orbe Détect"), kit::w::serial::detect, kit::Display::VFD, kOk, "detect", "Orbe", u8("Détect"), v3::accents::mint), proc(p), cur(p.result()), halfB(u8("×½")) {
    captureB.setButtonText("Capturer"); addAndMakeVisible(captureB);
    capAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, "capture", captureB);
    for (auto* b : {&clearB, &analyzeB, &halfB, &dblB, &tapB, &applyB, &mapB}) addAndMakeVisible(*b);
    mapB.setClickingTogglesState(true);
    mapB.setTooltip(u8("Carte des fausses notes : sur une piste de voix captur\xC3\xA9" "e, juge la hauteur chant\xC3\xA9" "e par rapport \xC3\xA0 la tonalit\xC3\xA9 (clic droit sur la tonalit\xC3\xA9 : coller celle du beat)."));
    mapB.onClick = [this] { showMap = mapB.getToggleState(); if (showMap && !proc.falseMap()) proc.analyzeMap(); repaint(); };
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
    captureB.setClickingTogglesState(true); captureB.setLed(juce::Colour(0xFFFF3B30));
    setHelp({u8("Mesure le tempo et la tonalité d'un extrait, puis donne l'accordage du 808."),
             u8("- Capture : lance la lecture dans l'hôte, appuie sur CAPTURER (jusqu'à 2 minutes, le silence est ignoré)."),
             u8("- Analyser : au moins 5 secondes de capture. ×½ et ×2 corrigent un tempo détecté en moitié ou en double."),
             u8("- Tap : tape au moins 3 fois en rythme ; « Appliquer le tap » remplace le tempo détecté."),
             u8("- Roue de Camelot : clique une case pour la choisir et voir ses voisines compatibles ; reclique pour revenir à la détection."),
             u8("- 808 : accorde la fondamentale du 808 sur la tonique (note MIDI 24 + hauteur de la tonique) ; les autres notes de la gamme servent aux glissés et aux changements d'accord.")});
    accSeg = std::make_unique<kit::v3::UiSeg>(acc, kit::v3::notes::accidentalLabels(), kit::v3::notes::accidentalTip(), pal);
    addAndMakeVisible(*accSeg); acc.onChange = [this] { repaint(); };
    start(24, "detect");
}

void DetectEditor::layout() {
    captureB.setBounds(Ri(200, 110, 114, 30)); clearB.setBounds(Ri(200, 144, 114, 26));
    accSeg->setBounds(Ri(588, 88, 56, 20));
    analyzeB.setBounds(Ri(36, 224, 278, 28));
    halfB.setBounds(Ri(36, 424, 134, 28)); dblB.setBounds(Ri(180, 424, 134, 28));
    mapB.setBounds(Ri(872, 436, 100, 22));
    tapB.setBounds(Ri(36, 462, 134, 44)); applyB.setBounds(Ri(36, 514, 278, 24));
}

void DetectEditor::themeChanged(const v3::Theme* tp) {
    for (auto* b : {&captureB, &clearB, &analyzeB, &halfB, &dblB, &tapB, &applyB, &mapB}) { b->T = tp; b->repaint(); }
    accSeg->T = tp; accSeg->px = 10.5f; accSeg->repaint();
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
    { const auto nm = proc.falseMap(); if (nm != fmap) fmap = nm; mapBusy = busy; mapB.setEnabled(!busy && capSec >= 2.0); }
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
void DetectEditor::mouseDown(const juce::MouseEvent& e) {
    if (e.mods.isPopupMenu() && keyLcdRect().contains(e.position)) { keyMenu(); return; }
    const auto h = wheelHit(e.position); if (h.empty()) return; focus = (focus == h) ? std::string() : h; repaint();
}

void DetectEditor::keyMenu() {
    juce::PopupMenu m; const auto r = cur;
    kit::keyparse::Key pk; const bool canPaste = kit::keyparse::parse(juce::SystemClipboard::getTextFromClipboard().toStdString(), pk);
    const int ref = proc.refKeyCode();
    m.addItem(1, r->valid ? u8("Copier la tonalit\xC3\xA9 : ") + String(kit::keyparse::format(r->pc, r->minor)) : u8("Copier la tonalit\xC3\xA9"), r->valid);
    m.addItem(2, canPaste ? u8("Coller la tonalit\xC3\xA9 de r\xC3\xA9" "f\xC3\xA9rence : ") + String(kit::keyparse::format(pk.pc, pk.minor)) : u8("Coller la tonalit\xC3\xA9 de r\xC3\xA9" "f\xC3\xA9rence"), canPaste);
    m.addItem(3, ref >= 0 ? u8("Revenir \xC3\xA0 la tonalit\xC3\xA9 d\xC3\xA9tect\xC3\xA9" "e (r\xC3\xA9" "f\xC3\xA9rence : ") + String(kit::keyparse::format(ref / 2, (ref & 1) != 0)) + ")" : u8("Revenir \xC3\xA0 la tonalit\xC3\xA9 d\xC3\xA9tect\xC3\xA9" "e"), ref >= 0);
    juce::Component::SafePointer<DetectEditor> self(this);
    m.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(localAreaToGlobal(keyLcdRect().toNearestInt())), [self, r, pk](int id) {
        if (self == nullptr) return;
        if (id == 1 && r->valid) juce::SystemClipboard::copyTextToClipboard(String(kit::keyparse::format(r->pc, r->minor)));
        else if (id == 2) { self->proc.setRefKey(pk.pc, pk.minor); if (self->showMap) self->proc.analyzeMap(); }
        else if (id == 3) { self->proc.clearRefKey(); if (self->showMap) self->proc.analyzeMap(); }
        self->repaint();
    });
}

void DetectEditor::body(juce::Graphics& g) {
    const float u = u_; const auto& r = *cur; const bool ok = r.valid;
    // ---- en-tête : afficheur d'état
    well(g, R(560, 10, 360, 30));
    String st = busy ? u8("ANALYSE EN COURS…") : ok ? fr(r.bpm) + u8(" BPM") + kDot + ou::keyShort(r.pc, r.minor, flat_()) + kDot + T(oa::camelot(r.pc, r.minor)) : u8("EN ATTENTE") + kDot + mmss(capSec) + " / 2:00";
    txt(g, st, R(572, 10, 340, 30), 14.f * u, scr, juce::Justification::centredLeft, false, true);

    // ---- capture
    panel(g, pal, R(20, 82, 310, 180), "Capture", u);
    well(g, R(36, 112, 150, 50));
    lcd(g, R(40, 114, 142, 46), mmss(capSec), 34.f * u, full ? kWarn : scr);
    txt(g, "/ 2:00", R(138, 146, 44, 12), 8.5f * u, pal.mute, juce::Justification::centredRight, false, true);
    well(g, R(36, 172, 278, 44));
    {
        const int n = proc.peakCount(); const auto w = R(36, 172, 278, 44).reduced(4.f * u, 3.f * u); const int span = juce::jmax(n, 200);
        for (int x = 0; x < (int)w.getWidth(); ++x) {
            const int i = (int)((float)x / w.getWidth() * (float)span); if (i >= n) break;
            const float a = juce::jlimit(0.f, 1.f, std::sqrt(proc.peakAt(i)) * 1.1f) * w.getHeight() * .5f;
            g.setColour(kLilac.withAlpha(.85f)); g.fillRect(w.getX() + (float)x, w.getCentreY() - a, 1.f, juce::jmax(1.f, a * 2));
        }
    }

    // ---- tempo
    panel(g, pal, R(20, 276, 310, 344), "Tempo", u);
    well(g, R(36, 306, 278, 84));
    lcd(g, R(44, 310, 196, 76), ok ? fr(r.bpm) : "---,-", 56.f * u, ok ? scr : scr.withAlpha(.5f));
    txt(g, "BPM", R(244, 358, 60, 20), 12.f * u, pal.mute, juce::Justification::centredLeft, true);
    if (ok) {
        pill(g, pal, 36 * u, 398 * u, u, confColour(r.bconf), confLabel(r.bconf), 80.f);
        if (r.nAlt) { String a = u8("Aussi : "); for (int i = 0; i < r.nAlt; ++i) a += (i ? u8(" · ") : String()) + fr(r.alt[i]); txt(g, a, R(130, 396, 184, 18), 10.f * u, pal.mute, juce::Justification::centredRight, false, true); }
    }
    well(g, R(180, 462, 134, 44));
    lcd(g, R(184, 464, 126, 40), tapBpm > 0 ? fr(tapBpm) : (taps.empty() ? "---,-" : "..."), 24.f * u, kPeach);
    if (tapBpm > 0 || !taps.empty()) txt(g, "TAP", R(186, 464, 40, 12), 8.f * u, pal.mute, juce::Justification::centredLeft, true);
    // comparaison avec le tempo de l'hôte
    auto hc = R(36, 548, 278, 58);
    hairline(g, hc.withHeight(2.f));
    kit::engraved(g, pal, u8("Tempo de l'hôte"), R(46, 553, 140, 12), juce::Justification::centredLeft, 8.5f * u, 1.6f * u);
    txt(g, hostBpm > 0 ? fr(hostBpm) + " BPM" : "--", R(190, 551, 114, 16), 12.5f * u, scr, juce::Justification::centredRight, false, true);
    {
        String v; juce::Colour c = pal.mute;
        if (hostBpm <= 0) v = u8("Inconnu");
        else if (!ok) v = "--";
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
    well(g, R(362, 112, 274, 64));
    lcd(g, R(368, 116, 262, 56), ok ? ou::keyLong(r.pc, r.minor, flat_()) : "---", 30.f * u, ok ? kLilac : kLilac.withAlpha(.5f), juce::Justification::centred);
    const std::string cam = ok ? oa::camelot(r.pc, r.minor) : std::string();
    {
        auto chip = R(362, 188, 64, 30);
        g.setColour(ok ? camColour(std::atoi(cam.c_str())) : pal.line); g.fillRoundedRectangle(chip, 5.f * u);
        g.setColour(juce::Colours::black.withAlpha(.55f)); g.drawRoundedRectangle(chip, 5.f * u, 1.f);
        txt(g, ok ? T(cam) : "--", chip, 16.f * u, Colour(0xFF101014), juce::Justification::centred, true, true);
        if (ok) { pill(g, pal, 440 * u, 188 * u, u, confColour(r.kconf), confLabel(r.kconf), 90.f);
                  txt(g, u8("Camelot"), R(440, 206, 80, 12), 8.5f * u, pal.mute); }
        if (ok) { const int p1 = r.rankPc[1]; const bool m1 = r.rankMinor[1];
                  txt(g, u8("Sinon : ") + ou::keyLong(p1, m1, flat_()) + kDot + T(oa::camelot(p1, m1)), R(362, 226, 274, 16), 11.f * u, pal.ink.withAlpha(.8f)); }
    }
    {   // chromagramme
        const auto a = R(362, 252, 274, 96); const float bw = a.getWidth() / 12.f;
        const int* iv = ok && r.minor ? oa::kMinor : oa::kMajor;
        for (int i = 0; i < 12; ++i) {
            const int rel = ok ? ((i - r.pc) % 12 + 12) % 12 : -1; bool inScale = false; for (int k = 0; k < 7; ++k) if (iv[k] == rel) inScale = true;
            const float v = ok ? (float)r.chroma[i] : 0.f; const float hh = (a.getHeight() - 18 * u) * juce::jmax(.02f, v);
            auto b = juce::Rectangle<float>(a.getX() + bw * (float)i + 3 * u, a.getBottom() - 16 * u - hh, bw - 6 * u, hh);
            g.setColour(juce::Colours::black.withAlpha(.35f)); g.fillRoundedRectangle(b.withTop(a.getY()).withHeight(a.getHeight() - 16 * u), 2.f * u);
            g.setColour(!ok ? pal.line : rel == 0 ? scr : inScale ? kLilac.withAlpha(.8f) : pal.mute.withAlpha(.5f)); g.fillRoundedRectangle(b, 2.f * u);
            txt(g, note(i, flat_()), R(a.getX() / u + bw / u * (float)i - 2, 334, bw / u + 4, 14), 8.5f * u, rel == 0 ? scr : pal.mute, juce::Justification::centred, rel == 0);
        }
    }

    // ---- 808
    const std::string code = activeCode(); int kpc = 0; bool kmin = false; const bool haveKey = !code.empty() && oa::fromCamelot(code, kpc, kmin);
    panel(g, pal, R(346, 372, 306, 248), u8("808 · Accordage"), u);
    if (haveKey) {
        kit::engraved(g, pal, u8("Fondamentale du 808"), R(362, 404, 170, 14), juce::Justification::centredLeft, 9.f * u, 1.6f * u, true);
        well(g, R(540, 400, 96, 26));
        kit::screenText(g, pal, disp, note(kpc, flat_()) + (kmin ? " MIN" : " MAJ"), R(546, 400, 84, 26), juce::Justification::centred, 12.f * u);
        kit::engraved(g, pal, u8("Gamme · notes MIDI"), R(362, 438, 274, 14), juce::Justification::centredLeft, 8.f * u, 1.4f * u);
        const int* iv = kmin ? oa::kMinor : oa::kMajor; const int r0 = 24 + kpc;
        for (int i = 0; i < 7; ++i) {
            const int midi = r0 + iv[i]; auto t = R(362.f + (float)(i % 4) * 70.f, 462.f + (float)(i / 4) * 74.f, 64, 62);
            const bool root = i == 0;
            g.setColour(juce::Colours::black.withAlpha(.35f)); g.fillRoundedRectangle(t, 5.f * u);
            if (root) { g.setColour(kPeach.withAlpha(.18f)); g.fillRoundedRectangle(t, 5.f * u); }
            g.setColour(root ? kPeach.withAlpha(.9f) : juce::Colours::white.withAlpha(.1f)); g.drawRoundedRectangle(t.reduced(.5f), 5.f * u, 1.f);
            txt(g, noteOct(midi, flat_()), t.withTrimmedTop(6 * u).withHeight(26 * u), 19.f * u, root ? kPeach : pal.ink, juce::Justification::centred, false, true);
            txt(g, fr(oa::mtof(midi)) + " Hz", t.withTrimmedTop(34 * u).withHeight(12 * u), 8.5f * u, pal.mute, juce::Justification::centred, false, true);
            txt(g, root ? u8("FONDAMENTALE") : "MIDI " + String(midi), t.withTrimmedTop(48 * u).withHeight(11 * u), 7.f * u, root ? kPeach.withAlpha(.9f) : pal.mute.withAlpha(.7f), juce::Justification::centred, true);
        }
    }

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
            txt(g, ou::keyShort(kp, km, flat_()), juce::Rectangle<float>(x - 20 * u, y + 1 * u, 40 * u, 11 * u), 8.5f * u, isSel ? Colour(0xFF08090B).withAlpha(.75f) : pal.mute, juce::Justification::centred);
            if (ok && c == oa::camelot(r.pc, r.minor)) { g.setColour(isSel ? Colour(0xFF08090B) : scr); g.fillEllipse(x - 2.5f * u, y + 12 * u, 5 * u, 5 * u); }
        }
        txt(g, code.empty() ? "--" : T(code), juce::Rectangle<float>(cx - R0, cy - 18 * u, R0 * 2, 32 * u), 28.f * u, pal.ink.withAlpha(.9f), juce::Justification::centred, true, true);
        txt(g, code.empty() ? "CAMELOT" : focus.empty() ? u8("DÉTECTÉ") : u8("CHOISIE"), juce::Rectangle<float>(cx - R0, cy + 14 * u, R0 * 2, 12 * u), 8.f * u, pal.mute, juce::Justification::centred, true);
        int hp = 0; bool hm = false;
        if (!hover.empty() && oa::fromCamelot(hover, hp, hm)) txt(g, T(hover) + kDot + ou::keyLong(hp, hm, flat_()), R(680, 402, 288, 14), 10.f * u, pal.mute, juce::Justification::centred);
    }

    // ---- compatibles
    panel(g, pal, R(668, 432, 312, 188), showMap ? "Carte des fausses notes" : code.empty() ? "Compatibles" : "Compatibles avec " + T(code), u);
    if (showMap) drawMap(g, R(680, 462, 288, 150), nullptr, u);
    else if (!code.empty()) {
        const auto comp = oa::compatCodes(code); const char* why[4] = {"même tonalité", "−1 · plus calme", "+1 · plus d'énergie", "relative · autre couleur"};
        for (int i = 0; i < 4; ++i) {
            int p = 0; bool m = false; oa::fromCamelot(comp[(size_t)i], p, m); const float y = 464.f + (float)i * 36.f;
            auto chip = R(684, y, 44, 26); g.setColour(camColour(std::atoi(comp[(size_t)i].c_str()))); g.fillRoundedRectangle(chip, 4.f * u);
            txt(g, T(comp[(size_t)i]), chip, 12.f * u, Colour(0xFF101014), juce::Justification::centred, true, true);
            txt(g, ou::keyLong(p, m, flat_()), R(738, y, 230, 14), 12.f * u, pal.ink.withAlpha(.92f), juce::Justification::centredLeft, true);
            txt(g, u8(why[i]), R(738, y + 15, 230, 12), 9.f * u, pal.mute);
        }
    }
}

// ============================================================ v3 « hybride v2 » : coordonnées de conception 1180 x 762
void DetectEditor::layout3() {
    captureB.setBounds(Ri(214, 112, 130, 32)); clearB.setBounds(Ri(214, 150, 130, 28));
    accSeg->setBounds(Ri(672, 74, 56, 17));
    analyzeB.setBounds(Ri(36, 254, 308, 32));
    halfB.setBounds(Ri(36, 452, 150, 32)); dblB.setBounds(Ri(194, 452, 150, 32));
    mapB.setBounds(Ri(1040, 500, 112, 28));
    tapB.setBounds(Ri(36, 496, 150, 48)); applyB.setBounds(Ri(36, 552, 308, 32));
}

void DetectEditor::backdrop3(juce::Graphics& g, float k) {
    const auto& t = skin.T;
    auto Rk = [k](float x, float y, float w, float h) { return juce::Rectangle<float>(x * k, y * k, w * k, h * k); };
    for (auto r : {Rk(20, 84, 340, 216), Rk(20, 312, 340, 430), Rk(380, 84, 340, 290), Rk(380, 386, 340, 356), Rk(740, 84, 420, 400), Rk(740, 496, 420, 246)}) v3::card(g, t, r, 18.f * k);
    auto inset = [&](juce::Rectangle<float> r, float rad) {
        g.setGradientFill(juce::ColourGradient(t.scrTop, r.getX(), r.getY(), t.scrBot, r.getX(), r.getBottom(), false)); g.fillRoundedRectangle(r, rad);
        g.setColour(juce::Colours::white.withAlpha(0.07f)); g.drawRoundedRectangle(r.reduced(0.5f), rad, 1.f);
    };
    inset(Rk(36, 190, 308, 52), 12 * k); inset(Rk(36, 344, 308, 96), 16 * k); inset(Rk(194, 496, 150, 48), 14 * k); inset(Rk(396, 112, 308, 78), 16 * k);
    v3::rk::label(g, t.inkMute, k, "Capture", 40, 96, 200); v3::rk::label(g, t.inkMute, k, "Tempo", 40, 324, 200);
    v3::rk::label(g, t.inkMute, k, u8("Tonalit\xC3\xA9"), 400, 96, 200); v3::rk::label(g, t.inkMute, k, u8("808 \xC2\xB7 Accordage"), 400, 398, 250);
    v3::rk::label(g, t.inkMute, k, "Roue de Camelot", 760, 96, 250);
}

void DetectEditor::body3(juce::Graphics& g) {
    const float u = u_; const auto& r = *cur; const bool ok = r.valid; const auto& T = skin.T;
    auto t3 = [&](const String& s, juce::Rectangle<float> rc, float px, juce::Colour c, juce::Justification j = juce::Justification::centredLeft, bool bold = false) {
        g.setColour(c); g.setFont(v3::font(px * u, bold)); g.drawText(s, rc, j, true);
    };
    auto tab = [&](const String& s, juce::Rectangle<float> rc, float px, juce::Colour c, juce::Justification j, bool bold = true) { g.setColour(c); v3::tabular(g, s, rc, v3::font(px * u, bold), j); };
    auto dotc = [&](juce::Colour c) { return T.dark ? c : c.darker(0.35f); };
    // ---- en-tête : état
    { auto pill = R(560, 34, 400, 32); g.setColour(T.field); g.fillRoundedRectangle(pill, 14 * u); g.setColour(T.fieldEdge); g.drawRoundedRectangle(pill.reduced(0.5f), 14 * u, 1.f);
      const String st = busy ? u8("Analyse en cours\xE2\x80\xA6") : ok ? v3::fmt::withUnit(v3::fmt::num(r.bpm, 1), "BPM") + kDot + ou::keyShort(r.pc, r.minor, flat_()) + kDot + ou::T(oa::camelot(r.pc, r.minor)) : u8("En attente") + kDot + mmss(capSec) + " / 2:00";
      tab(st, pill.reduced(16 * u, 0), 15.f, T.ink, juce::Justification::centred); }
    // ---- capture
    tab(mmss(capSec), R(40, 108, 170, 56), 46.f, full ? T.out : T.ink, juce::Justification::centredLeft);
    t3("/ 2:00", R(40, 162, 170, 20), 13.f, T.inkMute);
    { const int n = proc.peakCount(); const auto w = R(36, 190, 308, 52).reduced(10.f * u, 8.f * u); const int span = juce::jmax(n, 200);
      for (int x = 0; x < (int)(w.getWidth() / (3.f * u)); ++x) {
          const int i = (int)((float)x * 3.f * u / w.getWidth() * (float)span); if (i >= n) break;
          const float a = juce::jlimit(0.f, 1.f, std::sqrt(proc.peakAt(i)) * 1.1f) * w.getHeight() * .5f;
          g.setColour(T.sung.withAlpha(0.9f)); g.fillRoundedRectangle(w.getX() + (float)x * 3.f * u, w.getCentreY() - juce::jmax(1.f, a), 2.f * u, juce::jmax(2.f, a * 2), 1.f * u);
      } }
    // ---- tempo
    tab(ok ? v3::fmt::num(r.bpm, 1) : u8("\xE2\x80\x94"), R(50, 348, 220, 74), 62.f, ok ? T.out : T.scrMute, juce::Justification::centredLeft);
    t3("BPM", R(268, 386, 60, 24), 16.f, T.scrMute, juce::Justification::centredLeft);
    if (ok) {
        const auto c = confColour(r.bconf);
        g.setColour(c); g.fillEllipse(juce::Rectangle<float>(8 * u, 8 * u).withCentre(R(56, 424, 1, 1).getPosition()));
        g.setColour(T.scrInk); v3::caps(g, confLabel(r.bconf), R(66, 416, 120, 16), 11.f * u, juce::Justification::centredLeft);
        if (r.nAlt) { String a = u8("Aussi : "); for (int i = 0; i < r.nAlt; ++i) a += (i ? u8(" \xC2\xB7 ") : String()) + fr(r.alt[i]); t3(a, R(170, 414, 166, 20), 12.f, T.scrMute, juce::Justification::centredRight); }
    }
    t3("TAP", R(204, 500, 40, 16), 10.f, T.scrMute, juce::Justification::centredLeft, true);
    tab(tapBpm > 0 ? fr(tapBpm) : (taps.empty() ? String(u8("\xE2\x80\x94")) : "..."), R(200, 512, 138, 32), 24.f, T.out, juce::Justification::centredRight);
    g.setColour(T.fieldEdge); g.fillRect(R(36, 600, 308, 1));
    v3::rk::label(g, T.inkMute, u, u8("Tempo de l'h\xC3\xB4te"), 40, 610, 160, 11.f);
    tab(hostBpm > 0 ? v3::fmt::withUnit(v3::fmt::num(hostBpm, 1), "BPM") : u8("\xE2\x80\x94"), R(190, 606, 154, 24), 17.f, T.ink, juce::Justification::centredRight);
    {
        String v; juce::Colour c = T.inkMute;
        if (hostBpm <= 0) v = u8("Inconnu");
        else if (!ok) v = u8("\xE2\x80\x94");
        else {
            const double d = (r.bpm - hostBpm) / hostBpm; double best = 1e9; int mult = 0; const double m[3] = {1.0, 0.5, 2.0};
            for (int i = 0; i < 3; ++i) { const double dd = std::abs(r.bpm * m[i] - hostBpm) / hostBpm; if (dd < best) { best = dd; mult = i; } }
            if (std::abs(d) < .005) { v = u8("Identique au projet."); c = dotc(kOk); }
            else if (mult == 0 && best <= .06) { v = u8("Proche du projet : ") + v3::fmt::signedNum(r.bpm - hostBpm, 1) + " BPM (" + v3::fmt::num(std::abs(d) * 100, 1) + u8("\xC2\xA0%)."); c = dotc(kOk); }
            else if (mult != 0 && best <= .06) { v = u8(mult == 1 ? "M\xC3\xAAme tempo en moiti\xC3\xA9 : essaie \xC3\x97" "2." : "M\xC3\xAAme tempo en double : essaie \xC3\x97\xC2\xBD."); c = dotc(kWarn); }
            else { v = u8("\xC3\x89" "cart de ") + v3::fmt::signedNum(r.bpm - hostBpm, 1) + u8(" BPM avec le projet."); c = dotc(kBad); }
        }
        if (c != T.inkMute) { g.setColour(c); g.fillEllipse(juce::Rectangle<float>(9 * u, 9 * u).withCentre(R(45, 650, 1, 1).getPosition())); }
        g.setColour(c != T.inkMute ? T.ink : T.inkMute); g.setFont(v3::font(14.f * u)); g.drawFittedText(v, R(c != T.inkMute ? 58.f : 40.f, 636, 290, 56).toNearestInt(), juce::Justification::topLeft, 3, 1.f);
    }
    // ---- tonalité
    t3(ok ? ou::keyLong(r.pc, r.minor, flat_()) : String(u8("\xE2\x80\x94")), R(396, 114, 308, 74), 34.f, T.accent.brighter(0.4f), juce::Justification::centred, true);
    const std::string cam = ok ? oa::camelot(r.pc, r.minor) : std::string();
    {
        auto chip = R(396, 206, 64, 34);
        g.setColour(ok ? camColour(std::atoi(cam.c_str()), 1.f, .7f, T.dark ? .72f : .62f) : T.track); g.fillRoundedRectangle(chip, 17 * u);
        t3(ok ? ou::T(cam) : u8("\xE2\x80\x94"), chip, 17.f, Colour(0xFF101014), juce::Justification::centred, true);
        if (ok) { g.setColour(dotc(confColour(r.kconf))); g.fillEllipse(juce::Rectangle<float>(9 * u, 9 * u).withCentre(R(478, 218, 1, 1).getPosition()));
                  t3(confLabel(r.kconf), R(488, 208, 120, 20), 13.f, T.ink); t3("Camelot", R(488, 224, 120, 16), 11.f, T.inkMute);
                  t3(u8("Sinon : ") + ou::keyLong(r.rankPc[1], r.rankMinor[1], flat_()) + kDot + ou::T(oa::camelot(r.rankPc[1], r.rankMinor[1])), R(396, 246, 308, 20), 13.f, T.inkMute); }
    }
    {
        const auto a = R(396, 276, 308, 76); const float bw = a.getWidth() / 12.f;
        const int* iv = ok && r.minor ? oa::kMinor : oa::kMajor;
        for (int i = 0; i < 12; ++i) {
            const int rel = ok ? ((i - r.pc) % 12 + 12) % 12 : -1; bool inScale = false; for (int kk = 0; kk < 7; ++kk) if (iv[kk] == rel) inScale = true;
            const float v = ok ? (float)r.chroma[i] : 0.f; const float hh = juce::jmax(5.f * u, a.getHeight() * juce::jmax(.02f, v));
            auto well = juce::Rectangle<float>(a.getX() + bw * (float)i + 3 * u, a.getY(), bw - 6 * u, a.getHeight());
            g.setColour(T.track.withAlpha(0.6f)); g.fillRoundedRectangle(well, 4 * u);
            g.setColour(!ok ? T.track : rel == 0 ? T.out : inScale ? T.accent : T.tick); g.fillRoundedRectangle(well.withTop(well.getBottom() - hh), 4 * u);
            t3(note(i, flat_()), juce::Rectangle<float>(well.getX() - 3 * u, a.getBottom() + 4 * u, well.getWidth() + 6 * u, 16 * u), 11.5f, rel == 0 ? T.accentText : T.inkMute, juce::Justification::centred, rel == 0);
        }
    }
    // ---- 808
    const std::string code = activeCode(); int kpc = 0; bool kmin = false; const bool haveKey = !code.empty() && oa::fromCamelot(code, kpc, kmin);
    if (haveKey) {
        t3(u8("Fondamentale du 808"), R(400, 424, 170, 24), 13.f, T.ink);
        { auto pl = R(566, 420, 138, 34); g.setColour(T.field); g.fillRoundedRectangle(pl, 17 * u); g.setColour(T.fieldEdge); g.drawRoundedRectangle(pl.reduced(.5f), 17 * u, 1.f); t3(note(kpc, flat_()) + (kmin ? " min" : " maj"), pl, 15.f, T.accentText, juce::Justification::centred, true); }
        v3::rk::label(g, T.inkMute, u, u8("Gamme \xC2\xB7 notes MIDI"), 400, 466, 250, 11.f);
        const int* iv = kmin ? oa::kMinor : oa::kMajor; const int r0 = 24 + kpc;
        for (int i = 0; i < 7; ++i) {
            const int midi = r0 + iv[i]; auto tl = R(396.f + (float)(i % 4) * 78.f, 490.f + (float)(i / 4) * 92.f, 72, 84); const bool root = i == 0;
            g.setColour(root ? T.accent.withAlpha(0.14f) : T.field); g.fillRoundedRectangle(tl, 14 * u); g.setColour(root ? T.accent : T.fieldEdge); g.drawRoundedRectangle(tl.reduced(.5f), 14 * u, root ? 1.5f : 1.f);
            tab(noteOct(midi, flat_()), tl.withTrimmedTop(8 * u).withHeight(30 * u), 21.f, root ? T.accentText : T.ink, juce::Justification::centred);
            tab(fr(oa::mtof(midi)) + u8("\xC2\xA0Hz"), tl.withTrimmedTop(40 * u).withHeight(16 * u), 12.f, T.inkMute, juce::Justification::centred, false);
            g.setColour(root ? T.accentText : T.inkMute); v3::caps(g, root ? "Tonique" : "MIDI " + String(midi), tl.withTrimmedTop(60 * u).withHeight(16 * u), 10.f * u, juce::Justification::centred);
        }
    }
    // ---- roue de Camelot
    {
        const auto w = wheelRect(); const float cx = w.getCentreX(), cy = w.getCentreY(), R2 = w.getWidth() / 2 - 4 * u, R1 = R2 * .7f, R0 = R2 * .4f;
        const auto comp = code.empty() ? std::array<std::string, 4>{} : oa::compatCodes(code);
        for (int n = 1; n <= 12; ++n) for (int L = 0; L < 2; ++L) {
            const std::string c = std::to_string(n) + (L ? "B" : "A");
            const float a = -juce::MathConstants<float>::halfPi + (float)(n % 12) * juce::MathConstants<float>::pi / 6.f, half = juce::MathConstants<float>::pi / 12.f - .012f;
            const float ri = L ? R1 + 2 * u : R0, ro = L ? R2 : R1 - 2 * u;
            const bool isSel = c == code, isComp = !isSel && std::find(comp.begin(), comp.end(), c) != comp.end(), hov = c == hover;
            juce::Path p; p.addPieSegment(cx - ro, cy - ro, ro * 2, ro * 2, a - half + juce::MathConstants<float>::halfPi, a + half + juce::MathConstants<float>::halfPi, ri / ro);
            const float hue = (float)oa::camelotHue(n) / 360.f, li = T.dark ? .72f : .6f;
            g.setColour(isSel ? Colour::fromHSL(hue, .7f, li, .95f) : isComp ? Colour::fromHSL(hue, .7f, li, .42f) : Colour::fromHSL(hue, .5f, li, T.dark ? .10f : .16f)); g.fillPath(p);
            g.setColour(hov ? T.ink : isComp || isSel ? Colour::fromHSL(hue, .7f, li, 1.f) : T.fieldEdge); g.strokePath(p, juce::PathStrokeType(hov || isComp ? 1.6f : 1.f));
            const float rm = (ri + ro) / 2, x = cx + std::cos(a) * rm, y = cy + std::sin(a) * rm; int kp = 0; bool km = false; oa::fromCamelot(c, kp, km);
            const auto tc = isSel ? Colour(0xFF101014) : T.ink.withAlpha(isComp ? 1.f : .75f);
            t3(ou::T(c), juce::Rectangle<float>(x - 22 * u, y - 14 * u, 44 * u, 16 * u), 13.f, tc, juce::Justification::centred, isSel || isComp);
            t3(ou::keyShort(kp, km, flat_()), juce::Rectangle<float>(x - 22 * u, y + 2 * u, 44 * u, 14 * u), 11.f, isSel ? Colour(0xFF101014).withAlpha(.8f) : T.inkMute, juce::Justification::centred);
            if (ok && c == oa::camelot(r.pc, r.minor)) { g.setColour(isSel ? Colour(0xFF101014) : T.out); g.fillEllipse(juce::Rectangle<float>(5 * u, 5 * u).withCentre({x, y + 20 * u})); }
        }
        tab(code.empty() ? u8("\xE2\x80\x94") : ou::T(code), juce::Rectangle<float>(cx - R0, cy - 24 * u, R0 * 2, 38 * u), 32.f, T.ink, juce::Justification::centred);
        v3::caps(g, code.empty() ? "Camelot" : focus.empty() ? u8("D\xC3\xA9tect\xC3\xA9") : u8("Choisie"), juce::Rectangle<float>(cx - R0, cy + 14 * u, R0 * 2, 16 * u), 11.f * u, juce::Justification::centred);
        int hp = 0; bool hm = false;
        if (!hover.empty() && oa::fromCamelot(hover, hp, hm)) t3(ou::T(hover) + kDot + ou::keyLong(hp, hm, flat_()), R(760, 460, 380, 20), 13.f, T.inkMute, juce::Justification::centred);
    }
    // ---- compatibles
    v3::rk::label(g, T.inkMute, u, showMap ? String(u8("Carte des fausses notes")) : code.empty() ? String("Compatibles") : "Compatibles avec " + ou::T(code), 760, 508, 280, 11.f);
    if (showMap) drawMap(g, R(760, 536, 380, 196), &T, u);
    else if (!code.empty()) {
        const auto comp = oa::compatCodes(code); const char* why[4] = {"m\xC3\xAAme tonalit\xC3\xA9", "\xE2\x88\x92" "1 \xC2\xB7 plus calme", "+1 \xC2\xB7 plus d'\xC3\xA9nergie", "relative \xC2\xB7 autre couleur"};
        for (int i = 0; i < 4; ++i) {
            int p = 0; bool m = false; oa::fromCamelot(comp[(size_t)i], p, m); const float y = 540.f + (float)i * 50.f;
            auto chip = R(760, y, 58, 36); g.setColour(camColour(std::atoi(comp[(size_t)i].c_str()), 1.f, .7f, T.dark ? .72f : .62f)); g.fillRoundedRectangle(chip, 18 * u);
            t3(ou::T(comp[(size_t)i]), chip, 16.f, Colour(0xFF101014), juce::Justification::centred, true);
            t3(ou::keyLong(p, m, flat_()), R(832, y - 1, 310, 20), 15.f, T.ink, juce::Justification::centredLeft, true);
            t3(u8(why[i]), R(832, y + 19, 310, 16), 12.f, T.inkMute);
        }
    }
}

// Carte des fausses notes : résumé chiffré, trois notes en cause, ligne de temps (une case = 100 ms : vert juste, orange pas juste, rouge hors gamme).
void DetectEditor::drawMap(juce::Graphics& g, juce::Rectangle<float> a, const v3::Theme* t, float u) {
    const juce::Colour ink = t ? t->ink : pal.ink, mute = t ? t->inkMute : pal.mute;
    auto line = [&](const String& s, float y, float h, juce::Colour c, bool bold = false) { g.setColour(c); g.setFont(kit::fnt(h * u, bold)); g.drawText(s, juce::Rectangle<float>(a.getX(), a.getY() + y * u, a.getWidth(), (h + 5.f) * u), juce::Justification::centredLeft, true); };
    if (mapBusy) { line(u8("Analyse de la voix\xE2\x80\xA6"), 4, 13, mute); return; }
    if (!fmap || !fmap->valid) { line(u8("Capture une voix (2 s au moins) puis appuie sur Carte voix."), 4, 12, mute); line(u8("Clic droit sur la tonalit\xC3\xA9 : coller celle du beat."), 24, 11, mute); return; }
    const auto& M = *fmap; const bool fl = flat_();
    line(u8("Tonalit\xC3\xA9 jug\xC3\xA9" "e : ") + ou::keyLong(M.key, M.minor, fl) + (proc.refKeyCode() >= 0 ? u8("  (coll\xC3\xA9" "e)") : String()), 0, 12, mute);
    line(String(juce::roundToInt((float)M.offPct())) + u8(" % du temps chant\xC3\xA9 hors gamme \xC2\xB7 ") + String(juce::roundToInt((float)M.badPct())) + u8(" % pas juste"), 18, 13, ink, true);
    float y = 40.f;
    for (int p : fn::worstNotes(M, 3)) {
        const auto& s = M.pc[p]; String d = ou::note(p, fl);
        if (s.offSec >= s.badSec) d += u8(" : ") + String(juce::roundToInt((float)(100.0 * s.offSec / s.sec))) + u8(" % hors gamme");
        else d += u8(" : ") + String(juce::roundToInt((float)(100.0 * s.badSec / s.sec))) + u8(" % \xC3\xA0 ") + String(juce::roundToInt((float)s.badMean)) + u8(" cents");
        line(d, y, 11.5f, ink); y += 16.f;
    }
    const auto tl = juce::Rectangle<float>(a.getX(), a.getBottom() - 30.f * u, a.getWidth(), 24.f * u); g.setColour(juce::Colours::black.withAlpha(0.18f)); g.fillRoundedRectangle(tl, 3.f * u);
    const size_t n = M.cells.size(); const float cw = tl.getWidth() / (float)std::max<size_t>(n, 1);
    for (size_t i = 0; i < n; ++i) {
        const auto& c = M.cells[i]; if (c.pc < 0) continue;
        g.setColour(c.off ? juce::Colour(0xFFE5484D) : c.bad ? juce::Colour(0xFFFFB02E) : juce::Colour(0xFF4CC38A));
        g.fillRect(tl.getX() + (float)i * cw, tl.getY() + 2.f * u, std::max(1.f, cw), tl.getHeight() - 4.f * u);
    }
}
