// Orbe · éléments d'interface communs aux plugins de la suite (JUCE). Face avant « matériel de studio ».
#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "../../Kit/Widgets.h"
#include "OrbeAnalysis.h"

namespace ou {

using juce::Colour, juce::Graphics, juce::Rectangle, juce::String;
using kit::w::kLilac; using kit::w::kIce; using kit::w::kPeach; using kit::w::kOk; using kit::w::kWarn; using kit::w::kBad;
using kit::w::lcd; using kit::w::hairline; using kit::w::fr;

// Outils de mesure : boîtier d'appareil gris martelé (kit commun).
inline kit::Palette palette(Colour accent = kOk) { return kit::w::meter(accent); }
inline String T(const std::string& s) { return String::fromUTF8(s.c_str()); }
// flat = false : écriture d'origine (C♯ E♭ F♯ A♭ B♭) ; flat = true : tous les noirs en bémol. Nom seulement, jamais le son.
inline String note(int pc, bool flat = false) {
    static const char* const fl[12] = {"C", "D\xE2\x99\xAD", "D", "E\xE2\x99\xAD", "E", "F", "G\xE2\x99\xAD", "G", "A\xE2\x99\xAD", "A", "B\xE2\x99\xAD", "B"};
    pc = ((pc % 12) + 12) % 12;
    return flat ? String::fromUTF8(fl[pc]) : T(oa::kNoteNames[pc]);
}
inline String noteOct(int midi, bool flat = false) { return note(midi % 12, flat) + String(midi / 12 - 1); }
inline String keyShort(int pc, bool minor, bool flat) { return flat ? note(pc, true) + (minor ? "m" : "") : T(oa::keyShort(pc, minor)); }
inline String keyLong(int pc, bool minor, bool flat) { return flat ? note(pc, true) + (minor ? kit::u8(" mineur") : kit::u8(" majeur")) : T(oa::keyLong(pc, minor)); }
inline const String kDot = kit::u8("  ·  ");
inline Colour camColour(int number, float a = 1.f, float sat = .7f, float li = .78f) { return Colour::fromHSL((float)oa::camelotHue(number) / 360.f, sat, li, a); }
inline Colour sevColour(int sev) { return sev == 0 ? kBad : sev == 1 ? kWarn : kOk; }
inline Colour confColour(double c) { return c > .6 ? kOk : c > .3 ? kWarn : kBad; }
inline String confLabel(double c) { return kit::u8(c > .6 ? "sûr" : c > .3 ? "probable" : "à vérifier"); }

// Section sérigraphiée : cadre gravé fin, titre posé dans une coupure du filet (pas de carte).
inline void panel(Graphics& g, const kit::Palette& p, Rectangle<float> r, const String& title, float u) {
    const auto f = kit::fnt(9.f * u, true);
    float tw = 0; for (auto c : kit::upper(title)) tw += kit::glyphWidth(f, c) + 2.f * u;
    const float ty = r.getY() + 6.f * u, x0 = r.getX(), x1 = r.getRight(), y1 = r.getBottom();
    juce::Path frame;
    frame.startNewSubPath(x0 + 12.f * u, ty); frame.lineTo(x0, ty); frame.lineTo(x0, y1); frame.lineTo(x1, y1); frame.lineTo(x1, ty); frame.lineTo(x0 + 22.f * u + tw, ty);
    g.setColour(juce::Colours::black.withAlpha(0.45f)); g.strokePath(frame, juce::PathStrokeType(1.4f), juce::AffineTransform::translation(0, 1.f));
    g.setColour(p.silkInk().withAlpha(0.42f)); g.strokePath(frame, juce::PathStrokeType(1.f));
    kit::engraved(g, p, title, Rectangle<float>(x0 + 16.f * u, ty - 6.f * u, tw + 8.f * u, 12.f * u), juce::Justification::centredLeft, 9.f * u, 2.f * u, true);
}
using kit::w::dial; using kit::w::DialZone;
inline void pill(Graphics& g, const kit::Palette& p, float x, float y, float u, Colour c, const String& label, float w = 110.f) {
    kit::led(g, x + 6 * u, y + 7 * u, 4.f * u, true, c);
    g.setColour(p.ink.withAlpha(0.85f)); kit::tracked(g, label, Rectangle<float>(x + 18 * u, y, w * u, 14 * u), juce::Justification::centredLeft, 1.4f * u, kit::fnt(9.5f * u, true));
}

using kit::w::Seg;

// Base des éditeurs de mesure : boîtier d'appareil (flancs, vis), bandeau d'en-tête commun, aide « ? », taille proportionnelle.
class Base : public juce::AudioProcessorEditor, public juce::DragAndDropContainer, protected juce::Timer {
public:
    Base(juce::AudioProcessor& p, float w, float h, String product, const char* serialNo, kit::Display display, Colour accent)
        : AudioProcessorEditor(&p), W(w), H(h), pal(palette(accent)), look(pal), disp(display), scr(kit::displayColours(display, pal).ink),
          product_(std::move(product)), serial_(serialNo) { setLookAndFeel(&look); }
    ~Base() override { stopTimer(); setLookAndFeel(nullptr); }
    void paint(Graphics& g) final {
        content = kit::w::drawFace(g, pal, getLocalBounds(), u_, {kit::w::Family::Meter, product_, serial_, kHeader, kSide});
        body(g);
    }
    void resized() final {
        u_ = (float)getWidth() / W; look.k = u_;
        content = getLocalBounds().toFloat().reduced(kSide * u_, 0);
        help.layout(kit::w::helpSlot(content, u_, false, kHeader).toNearestInt(), getLocalBounds());
        layout(); repaint();
    }
protected:
    static constexpr float kHeader = 50.f, kSide = 16.f;
    virtual void layout() = 0;
    virtual void body(Graphics&) = 0;
    void start(int hz = 24) {
        setResizable(true, true); setResizeLimits((int)(W * .8f), (int)(H * .8f), (int)(W * 1.7f), (int)(H * 1.7f));
        getConstrainer()->setFixedAspectRatio((double)W / (double)H); setSize((int)W, (int)H); startTimerHz(hz);
    }
    void setHelp(const juce::StringArray& lines) { help.install(*this, product_, lines); }
    // Afficheur du type de l'appareil.
    void well(Graphics& g, Rectangle<float> r) const { kit::screenWell(g, pal, r, disp); }
    Rectangle<float> R(float x, float y, float w, float h) const { return {x * u_, y * u_, w * u_, h * u_}; }
    Rectangle<int> Ri(float x, float y, float w, float h) const { return R(x, y, w, h).toNearestInt(); }
    float W, H, u_ = 1.f;
    kit::Palette pal; kit::w::ScaledLook look;
    kit::Display disp; Colour scr;
    juce::TooltipWindow tooltip{this, 600};
    Rectangle<float> content;
    kit::w::HelpKey help;
    String product_; const char* serial_;
};

}  // namespace ou
