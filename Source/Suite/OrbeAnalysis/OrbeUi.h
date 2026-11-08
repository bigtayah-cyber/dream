// Orbe · éléments d'interface communs aux plugins de la suite (JUCE). Face avant « matériel de studio ».
#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "../../Kit/Kit.h"
#include "OrbeAnalysis.h"

namespace ou {

using juce::Colour, juce::Graphics, juce::Rectangle, juce::String;
inline const Colour kLilac(0xFFCFC6FF), kIce(0xFFA9ECFF), kPeach(0xFFFFC4AB), kOk(0xFF9BE6B4), kWarn(0xFFFFD27A), kBad(0xFFFF7A7A);

inline kit::Palette palette() {
    kit::Palette p;
    p.plateTop = Colour(0xFF2B2B31); p.plateBot = Colour(0xFF16161A);
    p.ink = Colour(0xFFE9E7F0); p.mute = Colour(0xFF8F8D9B); p.line = Colour(0xFF3B3B44);
    p.accent = kLilac; p.accent2 = kIce; p.screen = Colour(0xFF0B0C11); p.screenInk = Colour(0xFFEDEBF6); p.brush = 1.f;
    return p;
}
inline String T(const std::string& s) { return String::fromUTF8(s.c_str()); }
inline String note(int pc) { return T(oa::kNoteNames[((pc % 12) + 12) % 12]); }
inline String noteOct(int midi) { return note(midi % 12) + String(midi / 12 - 1); }
inline const String kDot = kit::u8("  ·  ");
inline String fr(double v, int d = 1) { return String(v, d).replace(".", ","); }          // décimale à la française
inline Colour camColour(int number, float a = 1.f, float sat = .7f, float li = .78f) { return Colour::fromHSL((float)oa::camelotHue(number) / 360.f, sat, li, a); }
inline Colour sevColour(int sev) { return sev == 0 ? kBad : sev == 1 ? kWarn : kOk; }
inline Colour confColour(double c) { return c > .6 ? kOk : c > .3 ? kWarn : kBad; }
inline String confLabel(double c) { return kit::u8(c > .6 ? "sûr" : c > .3 ? "probable" : "à vérifier"); }

// Panneau encastré avec titre gravé.
inline void panel(Graphics& g, const kit::Palette& p, Rectangle<float> r, const String& title, float u) {
    g.setColour(juce::Colours::black.withAlpha(0.20f)); g.fillRoundedRectangle(r, 7.f * u);
    g.setColour(juce::Colours::black.withAlpha(0.55f)); g.drawRoundedRectangle(r, 7.f * u, 1.f);
    g.setColour(juce::Colours::white.withAlpha(0.05f)); g.drawRoundedRectangle(r.translated(0, 1.f).reduced(0.5f), 7.f * u, 1.f);
    kit::engraved(g, p, title, Rectangle<float>(r.getX() + 14 * u, r.getY() + 8 * u, r.getWidth() - 28 * u, 14 * u), juce::Justification::centredLeft, 9.5f * u, 2.f * u);
    g.setColour(p.line.withAlpha(0.7f)); g.fillRect(r.getX() + 14 * u, r.getY() + 25 * u, r.getWidth() - 28 * u, 1.f);
}
// Afficheur à cristaux : chiffres fantômes en fond, valeur allumée devant.
inline void lcd(Graphics& g, Rectangle<float> r, const String& s, float h, Colour c, juce::Justification j = juce::Justification::centredRight) {
    String ghost; for (auto ch : s) ghost += juce::CharacterFunctions::isDigit(ch) ? juce::juce_wchar('8') : ch;
    g.setFont(kit::mono(h));
    g.setColour(c.withAlpha(0.07f)); g.drawText(ghost, r, j, false);
    g.setColour(c.withAlpha(0.22f)); g.drawText(s, r.expanded(0.f, 0.f), j, false);
    g.setColour(c); g.drawText(s, r, j, false);
}
inline void pill(Graphics& g, const kit::Palette& p, float x, float y, float u, Colour c, const String& label, float w = 110.f) {
    kit::led(g, x + 6 * u, y + 7 * u, 4.f * u, true, c);
    g.setColour(p.ink.withAlpha(0.85f)); kit::tracked(g, label, Rectangle<float>(x + 18 * u, y, w * u, 14 * u), juce::Justification::centredLeft, 1.4f * u, kit::fnt(9.5f * u, true));
}
inline void hairline(Graphics& g, Rectangle<float> r) { g.setColour(juce::Colours::black.withAlpha(0.5f)); g.fillRect(r.withHeight(1.5f)); g.setColour(juce::Colours::white.withAlpha(0.06f)); g.fillRect(r.translated(0, 1.5f).withHeight(1.f)); }

// Sélecteur segmenté relié à un paramètre « choix ».
class Seg : public juce::Component {
public:
    Seg(juce::RangedAudioParameter& p, juce::StringArray labels, const kit::Palette& pal)
        : labels_(std::move(labels)), P(pal), att(p, [this](float v) { idx = juce::roundToInt(v); repaint(); }) { att.sendInitialUpdate(); }
    void paint(Graphics& g) override {
        auto r = getLocalBounds().toFloat().reduced(0.5f);
        g.setColour(juce::Colours::black.withAlpha(0.6f)); g.fillRoundedRectangle(r, 4.f);
        g.setColour(juce::Colours::white.withAlpha(0.10f)); g.drawRoundedRectangle(r, 4.f, 1.f);
        const int n = labels_.size(); const float w = r.getWidth() / (float)n;
        for (int i = 0; i < n; ++i) {
            auto c = Rectangle<float>(r.getX() + w * (float)i, r.getY(), w, r.getHeight()).reduced(2.f);
            if (i == idx) {
                g.setGradientFill(juce::ColourGradient(P.accent.withAlpha(0.55f), c.getX(), c.getY(), P.accent.withAlpha(0.22f), c.getX(), c.getBottom(), false));
                g.fillRoundedRectangle(c, 3.f); g.setColour(P.accent.withAlpha(0.9f)); g.drawRoundedRectangle(c, 3.f, 1.f);
            }
            g.setColour(i == idx ? juce::Colours::white : P.ink.withAlpha(0.6f));
            kit::tracked(g, labels_[i], c, juce::Justification::centred, 1.f, kit::fnt(juce::jmin(10.5f, c.getHeight() * 0.42f), true));
        }
    }
    void mouseDown(const juce::MouseEvent& e) override { const int n = labels_.size(); const int i = juce::jlimit(0, n - 1, (int)(e.position.x / (float)getWidth() * (float)n)); att.setValueAsCompleteGesture((float)i); }
private:
    juce::StringArray labels_; kit::Palette P; int idx = 0; juce::ParameterAttachment att;
};

// Base des éditeurs : face brossée, vis, en-tête de la marque, taille proportionnelle.
class Base : public juce::AudioProcessorEditor, public juce::DragAndDropContainer, protected juce::Timer {
public:
    Base(juce::AudioProcessor& p, float w, float h, String product, String tagline)
        : AudioProcessorEditor(&p), W(w), H(h), pal(palette()), look(pal), product_(std::move(product)), tag_(std::move(tagline)) { setLookAndFeel(&look); }
    ~Base() override { stopTimer(); setLookAndFeel(nullptr); }
    void paint(Graphics& g) final {
        const float u = u_;
        g.drawImageAt(plate, 0, 0);
        hairline(g, R(0, 70, W, 1.f));
        for (auto p : {juce::Point<float>(12, 12), {W - 12, 12}, {12, H - 12}, {W - 12, H - 12}}) kit::screw(g, p.x * u, p.y * u, 5.f * u, (p.x + p.y) * 0.9f);
        const float cx = 48 * u, cy = 36 * u, r = 17 * u;   // l'orbe : sphère éclairée entourée d'un anneau
        g.setColour(juce::Colours::black.withAlpha(0.5f)); g.fillEllipse(cx - r - 3 * u, cy - r - 1 * u, (r + 3 * u) * 2, (r + 3 * u) * 2);
        g.setGradientFill(juce::ColourGradient(kIce, cx - r * 0.5f, cy - r * 0.6f, Colour(0xFF5A46C8), cx + r, cy + r, true)); g.fillEllipse(cx - r, cy - r, r * 2, r * 2);
        g.setColour(juce::Colours::white.withAlpha(0.5f)); g.fillEllipse(cx - r * 0.62f, cy - r * 0.78f, r * 0.7f, r * 0.42f);
        g.setColour(kLilac.withAlpha(0.8f)); g.drawEllipse(cx - r * 1.28f, cy - r * 1.28f, r * 2.56f, r * 2.56f, 1.f * u);
        kit::engraved(g, pal, "Orbe", R(84, 18, 150, 34), juce::Justification::centredLeft, 28.f * u, 9.f * u, true);
        g.setColour(kLilac); kit::tracked(g, product_, R(246, 18, 200, 34), juce::Justification::centredLeft, 5.f * u, kit::fnt(17.f * u, true));
        g.setColour(pal.mute); g.setFont(kit::fnt(10.f * u)); g.drawText(tag_, R(84, 48, 330, 16).toNearestInt(), juce::Justification::centredLeft);
        body(g);
    }
    void resized() final { u_ = (float)getWidth() / W; plate = kit::brushedPlate(getWidth(), getHeight(), pal); layout(); repaint(); }
protected:
    virtual void layout() = 0;
    virtual void body(Graphics&) = 0;
    void start(int hz = 24) {
        setResizable(true, true); setResizeLimits((int)(W * .8f), (int)(H * .8f), (int)(W * 1.7f), (int)(H * 1.7f));
        getConstrainer()->setFixedAspectRatio((double)W / (double)H); setSize((int)W, (int)H); startTimerHz(hz);
    }
    Rectangle<float> R(float x, float y, float w, float h) const { return {x * u_, y * u_, w * u_, h * u_}; }
    Rectangle<int> Ri(float x, float y, float w, float h) const { return R(x, y, w, h).toNearestInt(); }
    float W, H, u_ = 1.f;
    kit::Palette pal; kit::Look look;
    juce::TooltipWindow tooltip{this, 600};
    juce::Image plate;
    String product_, tag_;
};

}  // namespace ou
