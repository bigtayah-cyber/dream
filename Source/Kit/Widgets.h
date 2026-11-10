// Composants de face avant communs à la suite Orbe et à Voile (au-dessus de Kit.h).
// Remplace les quatre mini-kits (of::, suiteui::, ou::, oc::ui::) : une seule définition de Seg, Pot, Power,
// des couleurs, du châssis d'en-tête (marque, plaque série unique, IN en haut à droite) et de l'aide « ? ».
#pragma once
#include <functional>
#include <juce_audio_processors/juce_audio_processors.h>
#include "Kit.h"

namespace kit::w {
using juce::Colour, juce::Graphics, juce::Rectangle, juce::String;

// ------------------------------------------------------------------ couleurs
inline const Colour kLilac(0xFFCFC6FF), kIce(0xFFA9ECFF), kPeach(0xFFFFC4AB);
inline const Colour kOk(0xFF9BE6B4), kWarn(0xFFFFD27A), kBad(0xFFFF7A7A);
inline const Colour kSilk(0xFFEDEAF2);  // sérigraphie blanche des racks

inline const char* const kNotes[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
// Notes en lettres (C D E F G A B), octave MIDI standard (60 = C4).
inline String noteName(int n) { return String(kNotes[((n % 12) + 12) % 12]) + String(n / 12 - 1); }
inline String fr(double v, int d = 1) { return String(v, d).replace(".", ","); }  // décimale à la française

// ------------------------------------------------------------------ familles et palettes
// Rack : effets (graphite anodisé, oreilles 19") ; Instrument : panneau noir mat, joues en bois ;
// Meter : appareil de mesure (gris martelé, flancs de boîtier).
enum class Family { Rack, Instrument, Meter };

inline Palette rack(Colour accent = kLilac) {
    Palette p;
    p.plateTop = Colour(0xFF2C2C31); p.plateBot = Colour(0xFF161618);
    p.ink = kSilk; p.silk = kSilk; p.mute = Colour(0xFF9D9AA8); p.line = Colour(0x24EDEAF2);
    p.accent = accent; p.accent2 = kIce;
    p.screen = Colour(0xFF07080B); p.screenInk = kIce; p.brush = 0.85f; p.finish = Finish::Anodized;
    return p;
}
inline Palette instrument(Colour accent = kPeach) {
    Palette p = rack(accent);
    p.plateTop = Colour(0xFF1F1F21); p.plateBot = Colour(0xFF101011);
    p.finish = Finish::Lacquer; p.brush = 0.7f;
    return p;
}
inline Palette meter(Colour accent = kOk) {
    Palette p = rack(accent);
    p.plateTop = Colour(0xFF3A3D42); p.plateBot = Colour(0xFF25272B);
    p.finish = Finish::Hammertone; p.brush = 0.9f; p.mute = Colour(0xFFA9ADB5);
    return p;
}
inline Palette familyPalette(Family f, Colour accent) { return f == Family::Rack ? rack(accent) : f == Family::Instrument ? instrument(accent) : meter(accent); }

// ------------------------------------------------------------------ numéros de série (uniques, un seul endroit)
namespace serial {
inline constexpr const char* voile = "DW-100";
inline constexpr const char* cabine = "DW-601", *master = "DW-602", *looks = "DW-603", *atelier = "DW-604";
inline constexpr const char* denoiser = "DW-701", *eq = "DW-702", *deesser = "DW-703", *autotune = "DW-704", *galet = "DW-705", *accroc = "DW-706";
inline constexpr const char* synth = "DW-801", *b808 = "DW-802", *beat = "DW-803", *pads = "DW-804", *sample = "DW-805";
inline constexpr const char* detect = "DW-901", *mix = "DW-902", *fredonne = "DW-903";
}  // namespace serial

// ------------------------------------------------------------------ châssis commun
struct Face {
    Family family = Family::Rack;
    String product;              // ex. « Orbe Cabine » : « Orbe » en sérigraphie, la suite en couleur d'accent
    const char* serialNo = "";
    float header = 50.f;         // hauteur du bandeau d'en-tête (unités de conception)
    float side = 18.f;           // oreilles / joues / flancs
};

namespace detail {
inline void bevel(Graphics& g, Rectangle<float> b, float w) {
    g.setColour(juce::Colours::white.withAlpha(0.18f));
    g.fillRect(b.getX(), b.getY(), b.getWidth(), w); g.fillRect(b.getX(), b.getY(), w, b.getHeight());
    g.setColour(juce::Colours::black.withAlpha(0.45f));
    g.fillRect(b.getX(), b.getBottom() - w, b.getWidth(), w); g.fillRect(b.getRight() - w, b.getY(), w, b.getHeight());
}
inline void wood(Graphics& g, Rectangle<float> side, juce::uint32 sd) {
    juce::ColourGradient gr(Colour(0xFF5E3C22), side.getX(), side.getY(), Colour(0xFF2E1C10), side.getRight(), side.getBottom(), false);
    gr.addColour(0.5, Colour(0xFF48301B));
    g.setGradientFill(gr); g.fillRect(side);
    const int n = juce::jmax(6, (int)(side.getWidth() / 1.6f));
    for (int i = 0; i < n; ++i) {
        const float x0 = side.getX() + side.getWidth() * ((float)i + 0.5f) / (float)n;
        juce::Path v; v.startNewSubPath(x0, side.getY());
        for (float y = side.getY(); y <= side.getBottom(); y += 6.f) v.lineTo(x0 + side.getWidth() * 0.07f * kit::detail::noise1(sd + (juce::uint32)i, y / 70.f, i), y);
        g.setColour(i % 3 == 0 ? juce::Colours::black.withAlpha(0.24f) : Colour(0xFF8A5C34).withAlpha(0.20f));
        g.strokePath(v, juce::PathStrokeType(i % 3 == 0 ? 1.1f : 0.7f));
    }
    g.setGradientFill(juce::ColourGradient(juce::Colours::white.withAlpha(0.12f), side.getX(), side.getY(), juce::Colours::transparentWhite, side.getRight(), side.getY(), false));
    g.fillRect(side);
    bevel(g, side, 1.2f);
}
}  // namespace detail

// Plaque nette (Retina), flancs de la famille, vis, bandeau d'en-tête et plaque série en bas à droite.
// Renvoie la zone utile (sans flancs), en pixels.
inline Rectangle<float> drawFace(Graphics& g, const Palette& p, Rectangle<int> bounds, float u, const Face& f) {
    drawPlate(g, bounds, p, seedFor(f.product));
    auto r = bounds.toFloat();
    const float sw = f.side * u;
    auto left = r.withWidth(sw), right = r.withLeft(r.getRight() - sw);
    const auto sd = seedFor(f.product);
    for (auto s : {left, right}) {
        if (f.family == Family::Instrument) {
            detail::wood(g, s, sd + (s.getX() > r.getX() ? 99U : 7U));
        } else if (f.family == Family::Rack) {  // oreilles : léger retrait de la façade, lumières oblongues vissées
            g.setColour(juce::Colours::black.withAlpha(0.26f)); g.fillRect(s);
            detail::bevel(g, s, 1.f);
            for (float fy : {0.0f, 1.0f}) {
                const float cy = fy == 0.f ? r.getY() + 16.f * u : r.getBottom() - 16.f * u;
                auto slot = Rectangle<float>(sw * 0.46f, 15.f * u).withCentre({s.getCentreX(), cy});
                g.setColour(juce::Colours::black.withAlpha(0.82f)); g.fillRoundedRectangle(slot, slot.getWidth() * 0.5f);
                g.setColour(juce::Colours::white.withAlpha(0.14f)); g.drawRoundedRectangle(slot.translated(0.4f, 0.6f), slot.getWidth() * 0.5f, 0.8f);
            }
        } else {  // flancs de boîtier d'appareil de mesure : caoutchouc gris foncé, rainures
            g.setGradientFill(juce::ColourGradient(Colour(0xFF2A2C30), s.getX(), s.getY(), Colour(0xFF151618), s.getRight(), s.getY(), false));
            g.fillRect(s);
            for (float y = s.getY() + 30.f * u; y < s.getBottom() - 30.f * u; y += 5.f * u) {
                g.setColour(juce::Colours::black.withAlpha(0.45f)); g.fillRect(s.getX() + 3.f * u, y, s.getWidth() - 6.f * u, 1.f);
                g.setColour(juce::Colours::white.withAlpha(0.06f)); g.fillRect(s.getX() + 3.f * u, y + 1.f, s.getWidth() - 6.f * u, 1.f);
            }
            detail::bevel(g, s, 1.f);
        }
        // vis : en haut et en bas de chaque flanc (celle du bas à droite reste hors de la poignée)
        for (float cy : {r.getY() + 16.f * u, r.getBottom() - 16.f * u})
            screw(g, s.getCentreX(), cy, juce::jmin(4.6f * u, sw * 0.28f), s.getX() * 0.03f + cy * 0.02f, f.family == Family::Rack ? ScrewHead::Phillips : ScrewHead::Hex);
    }
    auto inner = r.withTrimmedLeft(sw).withTrimmedRight(sw);
    // ombre des flancs sur la façade
    g.setColour(juce::Colours::black.withAlpha(0.30f));
    g.fillRect(inner.getX(), inner.getY(), 2.f, inner.getHeight());
    g.fillRect(inner.getRight() - 2.f, inner.getY(), 2.f, inner.getHeight());

    // bandeau d'en-tête
    if (f.header > 0.f) {
        auto hb = inner.withHeight(f.header * u);
        g.setGradientFill(juce::ColourGradient(juce::Colours::black.withAlpha(0.06f), hb.getX(), hb.getY(), juce::Colours::black.withAlpha(0.24f), hb.getX(), hb.getBottom(), false));
        g.fillRect(hb);
        g.setColour(juce::Colours::black.withAlpha(0.55f)); g.fillRect(hb.getX(), hb.getBottom() - 1.f, hb.getWidth(), 1.f);
        g.setColour(juce::Colours::white.withAlpha(0.09f)); g.fillRect(hb.getX(), hb.getBottom(), hb.getWidth(), 1.f);
        g.setColour(p.accent.withAlpha(0.85f)); g.fillRect(hb.getX(), hb.getBottom() - 3.f * u, 64.f * u, 2.f * u);  // bande d'accent de l'appareil
        const float x = hb.getX() + 14.f * u;
        engraved(g, p, "dreamaws", Rectangle<float>(x, hb.getY() + 7.f * u, 160.f * u, 11.f * u), juce::Justification::centredLeft, 8.4f * u, 2.6f * u);
        const auto words = juce::StringArray::fromTokens(f.product, " ", "");
        const String first = words.size() > 1 ? words[0] : String(), rest = words.size() > 1 ? f.product.fromFirstOccurrenceOf(" ", false, false) : f.product;
        const auto big = fnt(18.f * u, true);
        float tx = x;
        if (first.isNotEmpty()) {
            engraved(g, p, first, Rectangle<float>(tx, hb.getY() + 19.f * u, 200.f * u, 24.f * u), juce::Justification::centredLeft, 18.f * u, 3.2f * u, true);
            for (auto c : upper(first)) tx += glyphWidth(big, c) + 3.2f * u;
            tx += 6.f * u;
        }
        g.setColour(juce::Colours::black.withAlpha(0.3f));
        tracked(g, rest, Rectangle<float>(tx, hb.getY() + 19.8f * u, 400.f * u, 24.f * u), juce::Justification::centredLeft, 3.2f * u, big);
        g.setColour(p.accent);
        tracked(g, rest, Rectangle<float>(tx, hb.getY() + 19.f * u, 400.f * u, 24.f * u), juce::Justification::centredLeft, 3.2f * u, big);
    }
    // plaque série rivetée, verticale, en bas du flanc droit
    if (f.serialNo != nullptr && *f.serialNo != 0) {
        auto pl = Rectangle<float>(sw * 0.62f, 62.f * u).withCentre({right.getCentreX(), r.getBottom() - 72.f * u});
        g.setColour(juce::Colours::black.withAlpha(0.5f)); g.fillRoundedRectangle(pl.translated(0.6f, 1.2f), 2.f * u);
        g.setGradientFill(juce::ColourGradient(Colour(0xFFCFCBC2), pl.getX(), pl.getY(), Colour(0xFF8E8A82), pl.getRight(), pl.getBottom(), false));
        g.fillRoundedRectangle(pl, 2.f * u);
        g.setColour(juce::Colours::black.withAlpha(0.45f)); g.drawRoundedRectangle(pl, 2.f * u, 0.7f);
        for (float fy : {0.f, 1.f}) {
            const float cy = pl.getY() + 4.f * u + fy * (pl.getHeight() - 8.f * u);
            g.setColour(Colour(0xFF5E5A54)); g.fillEllipse(pl.getCentreX() - 1.5f * u, cy - 1.5f * u, 3.f * u, 3.f * u);
        }
        juce::Graphics::ScopedSaveState st(g);
        g.addTransform(juce::AffineTransform::rotation(-juce::MathConstants<float>::halfPi, pl.getCentreX(), pl.getCentreY()));
        auto tr = Rectangle<float>(pl.getHeight() - 14.f * u, pl.getWidth()).withCentre(pl.getCentre());
        g.setColour(Colour(0xFF23211E)); g.setFont(mono(juce::jmin(8.f * u, pl.getWidth() * 0.7f)));
        g.drawText(f.serialNo, tr, juce::Justification::centred, false);
    }
    return inner;
}

// Emplacements standard dans le bandeau : IN tout à droite, « ? » à sa gauche (ou tout à droite sans IN).
inline Rectangle<float> inSlot(Rectangle<float> content, float u, float header = 50.f) {
    return Rectangle<float>(62.f * u, (header - 8.f) * u).withPosition(content.getRight() - 72.f * u, content.getY() + 4.f * u);
}
inline Rectangle<float> helpSlot(Rectangle<float> content, float u, bool hasIn, float header = 50.f) {
    const float d = 22.f * u, right = content.getRight() - (hasIn ? 80.f * u : 12.f * u);
    return Rectangle<float>(d, d).withPosition(right - d, content.getY() + (header * 0.5f - 12.f) * u);
}

// ------------------------------------------------------------------ interrupteur IN
// Levier métal : vers le haut = effet en service. Le paramètre lié est « bypass » (état basculé = court-circuité).
class Power : public juce::Button {
public:
    explicit Power(const Palette& p = rack()) : juce::Button("in"), P(p) { setClickingTogglesState(true); setTooltip(u8("En service / court-circuité (bypass).")); }
    void setPalette(const Palette& p) { P = p; repaint(); }
    void paintButton(Graphics& g, bool hover, bool) override {
        auto r = getLocalBounds().toFloat();
        const bool on = !getToggleState();
        const float s = juce::jmin(r.getWidth() / 62.f, r.getHeight() / 42.f);
        auto txt = r.withWidth(28.f * s);
        led(g, txt.getCentreX(), r.getCentreY() - 8.f * s, 3.4f * s, on ? 1.f : 0.f, P.ledRed, LedBezel::Chrome);
        engraved(g, P, "IN", txt.withY(r.getCentreY() + 1.f * s).withHeight(13.f * s), juce::Justification::centred, 10.f * s, 1.4f * s, true);
        auto lev = r.withTrimmedLeft(28.f * s).reduced(0, 1.f);
        leverSwitch(g, P, lev, on, {}, {});
        if (hover) { g.setColour(juce::Colours::white.withAlpha(0.05f)); g.fillRoundedRectangle(r, 4.f); }
    }
private:
    Palette P;
};

// ------------------------------------------------------------------ aide en surimpression
class HelpPanel : public juce::Component {
public:
    HelpPanel() { setInterceptsMouseClicks(true, false); setWantsKeyboardFocus(true); }
    void set(const String& title, const juce::StringArray& lines) { t = title; L = lines; repaint(); }
    void paint(Graphics& g) override {
        g.fillAll(juce::Colours::black.withAlpha(0.80f));
        const float u = juce::jmax(0.7f, juce::jmin((float)getWidth() / 900.f, (float)getHeight() / 520.f) * 1.15f);
        const float fh = juce::jlimit(11.f, 17.f, 13.5f * u);
        const float lineH = fh * 1.55f;
        float textH = 0.f;
        const float cw = juce::jmin((float)getWidth() - 40.f, 720.f * u);
        for (auto& l : L) textH += lineH * (float)juce::jmax(1, (int)std::ceil(juce::GlyphArrangement::getStringWidth(fnt(fh), l) / (cw - 40.f)));
        auto card = Rectangle<float>(cw, juce::jmin((float)getHeight() - 30.f, textH + 70.f * u)).withCentre(getLocalBounds().toFloat().getCentre());
        g.setColour(Colour(0xF0101114)); g.fillRoundedRectangle(card, 8.f);
        g.setColour(juce::Colours::white.withAlpha(0.14f)); g.drawRoundedRectangle(card, 8.f, 1.f);
        g.setColour(kSilk);
        tracked(g, t, card.removeFromTop(40.f * u).reduced(20.f, 0).withTrimmedTop(8.f * u), juce::Justification::centredLeft, 2.f, fnt(14.f * u, true));
        auto body = card.reduced(20.f, 6.f * u);
        g.setFont(fnt(fh));
        for (auto& l : L) {
            const int n = juce::jmax(1, (int)std::ceil(juce::GlyphArrangement::getStringWidth(fnt(fh), l) / (body.getWidth())));
            auto row = body.removeFromTop(lineH * (float)n);
            const bool bullet = l.startsWith("- ");
            g.setColour(bullet ? kSilk.withAlpha(0.88f) : kSilk.withAlpha(0.96f));
            g.drawFittedText(bullet ? String(juce::CharPointer_UTF8("\xe2\x80\xa2 ")) + l.substring(2) : l, row.toNearestInt(), juce::Justification::topLeft, n + 1, 1.f);
        }
        g.setColour(kSilk.withAlpha(0.45f)); g.setFont(fnt(10.f));
        g.drawText(u8("Clic n'importe où pour fermer"), getLocalBounds().toFloat().removeFromBottom(22.f), juce::Justification::centred);
    }
    void mouseDown(const juce::MouseEvent&) override { setVisible(false); }
    bool keyPressed(const juce::KeyPress&) override { setVisible(false); return true; }
private:
    String t; juce::StringArray L;
};

// Touche « ? » : capuchon rond gravé ; ouvre l'aide en surimpression.
class HelpKey : public juce::Button {
public:
    HelpKey() : juce::Button("?") { setTooltip(u8("Aide")); }
    // À appeler dans le constructeur de l'éditeur, après avoir ajouté les autres composants.
    void install(juce::Component& editor, const String& title, const juce::StringArray& lines) {
        panel.set(title, lines);
        editor.addAndMakeVisible(*this);
        editor.addChildComponent(panel);
        onClick = [this] { panel.setVisible(true); panel.toFront(true); };
    }
    void layout(Rectangle<int> keyBounds, Rectangle<int> editorBounds) { setBounds(keyBounds); panel.setBounds(editorBounds); }
    void paintButton(Graphics& g, bool hover, bool down) override {
        auto r = getLocalBounds().toFloat().reduced(1.f);
        const float d = juce::jmin(r.getWidth(), r.getHeight());
        auto c = Rectangle<float>(d, d).withCentre(r.getCentre());
        g.setColour(juce::Colours::black.withAlpha(0.7f)); g.fillEllipse(c.expanded(1.5f));
        if (!down) { g.setColour(juce::Colours::black.withAlpha(0.45f)); g.fillEllipse(c.translated(0, 1.5f)); }
        auto cap = c.reduced(1.f).translated(0, down ? 1.f : 0.f);
        g.setGradientFill(juce::ColourGradient(Colour(hover ? 0xFF55555C : 0xFF46464C), cap.getX(), cap.getY(), Colour(0xFF151517), cap.getX(), cap.getBottom(), false));
        g.fillEllipse(cap);
        g.setColour(juce::Colours::white.withAlpha(0.15f)); g.drawEllipse(cap.reduced(0.5f), 0.8f);
        g.setColour(kSilk.withAlpha(0.92f)); g.setFont(fnt(d * 0.56f, true));
        g.drawText("?", cap.translated(0, -0.5f), juce::Justification::centred, false);
    }
    HelpPanel panel;
};

// ------------------------------------------------------------------ potard + nom + afficheur de valeur
class ResetSlider : public juce::Slider {
public:
    std::function<double()> resetTo;  // double-clic : valeur de référence (sinon : valeur par défaut du paramètre)
    void mouseDoubleClick(const juce::MouseEvent& e) override {
        if (resetTo) setValue(resetTo(), juce::sendNotificationSync);
        else juce::Slider::mouseDoubleClick(e);
    }
};

class Pot : public juce::Component {
public:
    static constexpr float kW = 117.f, kH = 78.f, kKnobR = 19.f;  // boîte « fixe » (largeur → échelle)
    // Boîte fixe 117 × 78 : l'échelle suit la largeur du composant. Double-clic : valeur par défaut.
    Pot(juce::AudioProcessorValueTreeState& a, const String& id, const String& lab, const String& tip, const Palette& p = rack())
        : look(std::make_unique<Look>(p)), label(lab), R(kKnobR), fixedBox(true) { init(a, id, tip + u8(" Double-clic : valeur d'origine."), nullptr); }
    // Rayon donné (unités de conception), échelle fixée par setScale ; double-clic : valeur de référence.
    Pot(juce::AudioProcessorValueTreeState& a, const String& id, const String& lab, const String& tip, std::function<double()> reset, float radius, Colour accent = kLilac)
        : look(std::make_unique<Look>(rack(accent))), label(lab), R(radius) { const bool ref = (bool)reset; init(a, id, tip + u8(ref ? " Double-clic : valeur de référence." : " Double-clic : valeur d'origine."), std::move(reset)); }
    ~Pot() override { s.setLookAndFeel(nullptr); }

    static float heightFor(float R) { return 3.f * R + 25.f; }
    void setScale(float u) { k = u; resized(); repaint(); }
    void setStyle(KnobStyle st) { s.getProperties().set("kitKnob", (int)st); s.repaint(); }
    void setPalette(const Palette& p) { auto nl = std::make_unique<Look>(p); s.setLookAndFeel(nl.get()); look = std::move(nl); repaint(); }
    void setValueColour(Colour c) { valueInk = c; repaint(); }
    juce::Slider& slider() { return s; }

    void resized() override {
        const float kk = scale();
        const float top = fixedBox ? 1.f * kk : 0.f;
        s.setBounds(Rectangle<float>((float)getWidth() * 0.5f - 1.5f * R * kk, top, 3.f * R * kk, 3.f * R * kk).toNearestInt());
    }
    void paint(Graphics& g) override {
        const float kk = scale(), w = (float)getWidth();
        const float top = fixedBox ? 1.f * kk : 0.f, ly = top + (3.f * R - 1.5f) * kk;
        const auto& P = look->pal();
        engraved(g, P, label, Rectangle<float>(0, ly, w, 11.f * kk), juce::Justification::centred, 8.6f * kk, 1.1f * kk, true);
        const float vw = juce::jmin(w - 4.f * kk, 62.f * kk);
        auto r = Rectangle<float>(w * 0.5f - vw * 0.5f, ly + 11.5f * kk, vw, 11.5f * kk);
        g.setColour(juce::Colours::black.withAlpha(0.62f)); g.fillRoundedRectangle(r, 2.f * kk);
        g.setGradientFill(juce::ColourGradient(juce::Colours::black.withAlpha(0.5f), r.getX(), r.getY(), juce::Colours::white.withAlpha(0.10f), r.getX(), r.getBottom(), false));
        g.drawRoundedRectangle(r, 2.f * kk, 0.8f);
        g.setColour((valueInk.isTransparent() ? P.screenInk : valueInk).withAlpha(0.95f)); g.setFont(lcdFont(9.6f * kk));
        g.drawText(par ? par->getText(par->getValue(), 0) : String(), r, juce::Justification::centred, false);
    }
private:
    float scale() const { return fixedBox ? (float)getWidth() / kW : k; }
    void init(juce::AudioProcessorValueTreeState& a, const String& id, const String& tip, std::function<double()> reset) {
        par = a.getParameter(id);
        s.setLookAndFeel(look.get());
        s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        s.setMouseDragSensitivity(240);
        s.setTooltip(tip);
        s.resetTo = std::move(reset);
        s.onValueChange = [this] { repaint(); };
        addAndMakeVisible(s);
        att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(a, id, s);
        if (!s.resetTo && par != nullptr) s.setDoubleClickReturnValue(true, par->convertFrom0to1(par->getDefaultValue()));
    }
    std::unique_ptr<Look> look; ResetSlider s; String label; float R; bool fixedBox = false; float k = 1.f;
    Colour valueInk;
    juce::RangedAudioParameter* par = nullptr;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};

// ------------------------------------------------------------------ sélecteur à touches (paramètre « choix »)
// Rangée (ou grille) de touches à verrouillage : la touche choisie a sa LED allumée et son texte en blanc.
class Seg : public juce::Component, public juce::SettableTooltipClient {
public:
    Seg(juce::RangedAudioParameter& p, juce::StringArray names, const Palette& pal, int cols = 0)
        : P(pal), labels(std::move(names)), cols_(cols > 0 ? cols : labels.size()),
          att(p, [this](float v) { idx = juce::roundToInt(v); repaint(); }, nullptr) { att.sendInitialUpdate(); }
    Seg(juce::AudioProcessorValueTreeState& a, const String& id, juce::StringArray names, const String& tip, const Palette& pal)
        : Seg(*a.getParameter(id), std::move(names), pal) { setTooltip(tip); }
    void setPalette(const Palette& p) { P = p; repaint(); }
    int selected() const { return idx; }
    void paint(Graphics& g) override {
        auto r = getLocalBounds().toFloat().reduced(0.5f);
        g.setColour(juce::Colours::black.withAlpha(0.72f)); g.fillRoundedRectangle(r, 3.f);
        g.setGradientFill(juce::ColourGradient(juce::Colours::black.withAlpha(0.5f), r.getX(), r.getY(), juce::Colours::white.withAlpha(0.12f), r.getX(), r.getBottom(), false));
        g.drawRoundedRectangle(r, 3.f, 1.f);
        const int n = labels.size(), rows = (n + cols_ - 1) / cols_;
        const float w = r.getWidth() / (float)cols_, h = r.getHeight() / (float)rows;
        for (int i = 0; i < n; ++i) {
            const int span = i == n - 1 ? cols_ - i % cols_ : 1;
            auto c = Rectangle<float>(r.getX() + w * (float)(i % cols_), r.getY() + h * (float)(i / cols_), w * (float)span, h).reduced(1.5f);
            const bool on = i == idx;
            g.setGradientFill(juce::ColourGradient(Colour(on ? 0xFF3E3E44 : 0xFF333338), c.getX(), c.getY(), Colour(on ? 0xFF1B1B1E : 0xFF18181B), c.getX(), c.getBottom(), false));
            g.fillRoundedRectangle(c.translated(0, on ? 0.6f : 0.f), 2.f);
            g.setColour(juce::Colours::white.withAlpha(on ? 0.10f : 0.14f));
            g.drawLine(c.getX() + 2.f, c.getY() + 0.6f, c.getRight() - 2.f, c.getY() + 0.6f, 0.8f);
            // LED-filet en haut de la touche
            auto lit = Rectangle<float>(juce::jmin(c.getWidth() * 0.4f, 18.f), juce::jmax(1.6f, h * 0.07f)).withCentre({c.getCentreX(), c.getY() + juce::jmax(3.f, h * 0.16f)});
            if (on) { g.setColour(P.accent.withAlpha(0.35f)); g.fillRoundedRectangle(lit.expanded(2.f, 1.5f), 2.f); }
            g.setColour(on ? P.accent.brighter(0.3f) : P.accent.withMultipliedBrightness(0.3f).withAlpha(0.8f));
            g.fillRoundedRectangle(lit, lit.getHeight() * 0.5f);
            g.setColour(on ? juce::Colours::white : P.silkInk().withAlpha(0.62f));
            const float fh = juce::jlimit(7.5f, 10.5f, h * 0.36f);
            tracked(g, labels[i], c.withTrimmedTop(h * 0.14f).translated(0, on ? 0.6f : 0.f), juce::Justification::centred, 1.f, fnt(fh, true));
        }
    }
    void mouseDown(const juce::MouseEvent& e) override {
        const int n = labels.size(), rows = (n + cols_ - 1) / cols_;
        const int cx = juce::jlimit(0, cols_ - 1, (int)(e.position.x / ((float)getWidth() / (float)cols_)));
        const int cy = juce::jlimit(0, rows - 1, (int)(e.position.y / ((float)getHeight() / (float)rows)));
        att.setValueAsCompleteGesture((float)juce::jmin(n - 1, cy * cols_ + cx));
    }
private:
    Palette P; juce::StringArray labels; int cols_, idx = 0; juce::ParameterAttachment att;
};

// ------------------------------------------------------------------ touche poussoir à LED (booléen ou action)
class Key : public juce::Button {
public:
    Key(const String& text, const Palette& p, Colour led = kLilac) : juce::Button(text), P(p), ledColour(led) { setClickingTogglesState(true); }
    float fill = 0.f;  // 0..1 : barre de progression au bas de la touche
    void setLed(Colour c) { ledColour = c; repaint(); }
    void paintButton(Graphics& g, bool hover, bool down) override {
        auto r = getLocalBounds().toFloat().reduced(1.5f);
        const bool on = getToggleState();
        pushButton(g, P, r, on, down, ledColour, {});
        auto cap = r.reduced(1.f).translated(0, down ? 1.f : 0.f);
        g.setColour(on ? juce::Colours::white : P.silkInk().withAlpha(hover ? 0.95f : 0.82f));
        tracked(g, getButtonText(), cap.withTrimmedTop(cap.getHeight() * 0.22f), juce::Justification::centred, 1.2f, fnt(juce::jlimit(8.5f, 12.f, cap.getHeight() * 0.36f), true));
        if (fill > 0.f) { g.setColour(ledColour.withAlpha(0.85f)); g.fillRoundedRectangle(cap.getX() + 4.f, cap.getBottom() - 4.f, (cap.getWidth() - 8.f) * juce::jlimit(0.f, 1.f, fill), 2.2f, 1.1f); }
    }
private:
    Palette P; Colour ledColour;
};

// Bouton d'action (TextButton) dessiné en poussoir carré à LED : LED allumée quand l'état est actif.
class PushKey : public juce::TextButton {
public:
    PushKey(const String& text = {}, const Palette& p = rack(), Colour led = kLilac) : juce::TextButton(text), P(p), ledColour(led) {}
    void setLed(Colour c) { ledColour = c; repaint(); }
    void setPalette(const Palette& p) { P = p; repaint(); }
    bool lit = false;  // LED forcée (en plus de l'état basculé)
    void paintButton(Graphics& g, bool hover, bool down) override {
        auto r = getLocalBounds().toFloat().reduced(1.5f);
        const bool on = getToggleState() || lit;
        juce::Graphics::ScopedSaveState st(g);
        if (!isEnabled()) g.setOpacity(0.45f);
        pushButton(g, P, r, on, down, ledColour, {});
        auto cap = r.reduced(1.f).translated(0, down ? 1.f : 0.f);
        g.setColour(on ? juce::Colours::white : P.silkInk().withAlpha(hover ? 0.98f : 0.86f));
        tracked(g, getButtonText(), cap.withTrimmedTop(cap.getHeight() * 0.22f), juce::Justification::centred, 1.2f, fnt(juce::jlimit(8.5f, 12.f, cap.getHeight() * 0.36f), true));
    }
private:
    Palette P; Colour ledColour;
};

// ------------------------------------------------------------------ LookAndFeel à échelle (textes de listes, bascules)
class ScaledLook : public Look {
public:
    explicit ScaledLook(Palette p) : Look(p) {}
    float k = 1.f;
    juce::Font getComboBoxFont(juce::ComboBox&) override { return lcdFont(11.5f * k); }
    void positionComboBoxText(juce::ComboBox& cb, juce::Label& l) override { l.setBounds(6, 1, cb.getWidth() - 22, cb.getHeight() - 2); l.setFont(getComboBoxFont(cb)); }
    void drawToggleButton(Graphics& g, juce::ToggleButton& b, bool, bool down) override {
        if (b.getProperties().contains("kitButton")) { Look::drawToggleButton(g, b, false, down); return; }
        auto r = b.getLocalBounds().toFloat(); const float s = r.getHeight() / 18.f;
        led(g, r.getX() + 9.f * s, r.getCentreY(), 3.6f * s, b.getToggleState() ? 1.f : 0.f, pal().accent, LedBezel::Black);
        g.setColour(pal().silkInk().withAlpha(b.getToggleState() ? 0.98f : 0.66f));
        const float fh = juce::jmin(10.8f * s, 10.f * k);
        tracked(g, b.getButtonText(), r.withTrimmedLeft(21.f * s), juce::Justification::centredLeft, 0.9f * k, fnt(fh, true));
    }
};

// ------------------------------------------------------------------ petits tracés communs
// Filet gravé horizontal (ombre + reflet), sur toute la largeur w (unités) à la hauteur y.
inline void rule(Graphics& g, float u, float y, float w, float x = 0.f) {
    g.setColour(juce::Colours::black.withAlpha(0.5f)); g.fillRect(x * u, y * u, w * u, 1.5f);
    g.setColour(juce::Colours::white.withAlpha(0.06f)); g.fillRect(x * u, y * u + 1.5f, w * u, 1.f);
}
inline void hairline(Graphics& g, Rectangle<float> r) {
    g.setColour(juce::Colours::black.withAlpha(0.5f)); g.fillRect(r.withHeight(1.5f));
    g.setColour(juce::Colours::white.withAlpha(0.06f)); g.fillRect(r.translated(0, 1.5f).withHeight(1.f));
}
// Titre de section sérigraphié + filet.
inline void section(Graphics& g, const Palette& pal, float u, const String& title, float x, float y, float w) {
    engraved(g, pal, title, Rectangle<float>(x * u, y * u, (w - 4) * u, 12 * u), juce::Justification::centredLeft, 8.8f * u, 2.2f * u, true);
    g.setColour(juce::Colours::black.withAlpha(0.5f)); g.fillRect(Rectangle<float>(x * u, (y + 15) * u, w * u, 1.2f * u));
    g.setColour(juce::Colours::white.withAlpha(0.07f)); g.fillRect(Rectangle<float>(x * u, (y + 16.2f) * u, w * u, u));
}
// Barre-graphe à LED (level 0..1), deux dernières LED rouges.
inline void ledBar(Graphics& g, Rectangle<float> r, float level, int n, Colour c) {
    const float w = r.getWidth() / (float)n;
    for (int i = 0; i < n; ++i) {
        const bool on = (float)(i + 1) / (float)n <= level + 0.001f;
        const auto col = i >= n - 2 ? Colour(0xFFFF4A3A) : c;
        auto s = Rectangle<float>(r.getX() + w * (float)i + 1.f, r.getY(), w - 2.f, r.getHeight());
        g.setColour(on ? col.withAlpha(0.95f) : col.withMultipliedBrightness(0.25f).withAlpha(0.5f));
        g.fillRoundedRectangle(s, 1.f);
    }
}
// Barre de niveau segmentée. v dans [lo, hi] ; bipolaire : zéro au centre.
inline void bar(Graphics& g, Rectangle<float> r, float v, float lo, float hi, Colour c, bool bipolar = false) {
    const int n = 28; const float sw = r.getWidth() / (float)n;
    for (int i = 0; i < n; ++i) {
        const float t0 = (float)i / (float)n, t1 = (float)(i + 1) / (float)n, pos = (v - lo) / (hi - lo);
        const bool lit = bipolar ? (pos >= 0.5f ? (t1 > 0.5f && t0 < pos && t0 >= 0.5f - 1e-4f) : (t0 + 1e-4f >= pos && t1 <= 0.5f + 1e-4f)) : t0 < pos;
        g.setColour(lit ? c.withAlpha(0.95f) : c.withAlpha(0.12f));
        g.fillRoundedRectangle(r.getX() + (float)i * sw + 0.6f, r.getY(), sw - 1.4f, r.getHeight(), 1.f);
    }
}
// Afficheur à segments : chiffres fantômes en fond, valeur allumée devant (police d'écran embarquée).
inline void lcd(Graphics& g, Rectangle<float> r, const String& s, float h, Colour c, juce::Justification j = juce::Justification::centredRight) {
    String ghost; for (auto ch : s) ghost += juce::CharacterFunctions::isDigit(ch) ? juce::juce_wchar('8') : ch;
    g.setFont(lcdFont(h));
    g.setColour(c.withAlpha(0.07f)); g.drawText(ghost, r, j, false);
    g.setColour(c.withAlpha(0.22f)); g.drawText(s, r.expanded(0.6f, 0.f), j, false);
    g.setColour(c); g.drawText(s, r, j, false);
}

// Galvanomètre à aiguille (score, corrélation) : cadran crème rétroéclairé, zones colorées, graduations.
struct DialZone { float a, b; Colour c; };
inline void dial(Graphics& g, Rectangle<float> r, float v, float lo, float hi, std::initializer_list<DialZone> zones, std::initializer_list<float> ticks, const String& legend, bool live) {
    const float u = juce::jmin(r.getWidth() / 200.f, r.getHeight() / 110.f);
    g.setColour(juce::Colours::black.withAlpha(0.6f)); g.fillRoundedRectangle(r.expanded(3.f * u), 5.f * u);
    g.setGradientFill(juce::ColourGradient(Colour(0xFF34363B), r.getX(), r.getY(), Colour(0xFF101113), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle(r, 5.f * u);
    auto face = r.reduced(7.f * u);
    g.setColour(Colour(0xFFEDE3C8)); g.fillRoundedRectangle(face, 3.f * u);
    g.setGradientFill(juce::ColourGradient(Colour(0xFFFFB04A).withAlpha(0.35f), face.getCentreX(), face.getBottom(), Colour(0x00FFB04A), face.getCentreX(), face.getY(), true));
    g.fillRoundedRectangle(face, 3.f * u);
    const juce::Point<float> pv(face.getCentreX(), face.getBottom() + face.getHeight() * 0.30f);
    const float R = (pv.y - face.getY()) * 0.80f, span = 0.78f;
    auto ang = [&](float x) { return -span + 2.f * span * juce::jlimit(0.f, 1.f, (x - lo) / (hi - lo)); };
    auto at = [&](float a, float rad) { return juce::Point<float>(pv.x + std::sin(a) * rad, pv.y - std::cos(a) * rad); };
    for (auto& z : zones) {
        juce::Path arc; arc.addCentredArc(pv.x, pv.y, R + 3.f * u, R + 3.f * u, 0, ang(z.a), ang(z.b), true);
        g.setColour(z.c); g.strokePath(arc, juce::PathStrokeType(5.f * u));
    }
    const auto ink = Colour(0xFF1A1714);
    juce::Path arc; arc.addCentredArc(pv.x, pv.y, R, R, 0, ang(lo), ang(hi), true);
    g.setColour(ink); g.strokePath(arc, juce::PathStrokeType(1.1f * u));
    g.setFont(fnt(juce::jmax(8.f, 9.f * u), true));
    for (float t : ticks) {
        const float a = ang(t);
        g.drawLine(juce::Line<float>(at(a, R), at(a, R + 6.f * u)), 1.2f * u);
        const String s = std::abs(t - std::round(t)) < 0.01f ? String(juce::roundToInt(t)) : String(t, 1).replace(".", ",");
        g.drawText((t > 0 && lo < 0 ? "+" : "") + s, Rectangle<float>(28.f * u, 11.f * u).withCentre(at(a, R + 13.f * u)), juce::Justification::centred, false);
    }
    g.setFont(fnt(juce::jmax(7.5f, 9.f * u), true)); g.setColour(ink.withAlpha(0.7f));
    g.drawText(upper(legend), Rectangle<float>(face.getWidth(), 12.f * u).withCentre({face.getCentreX(), face.getY() + face.getHeight() * 0.66f}), juce::Justification::centred, false);
    {
        juce::Graphics::ScopedSaveState st(g); g.reduceClipRegion(face.toNearestInt());
        const float a = live ? ang(v) : ang(lo) - 0.05f;
        g.setColour(juce::Colours::black.withAlpha(0.18f)); g.drawLine(juce::Line<float>(at(a, R * 0.2f).translated(2.f * u, 3.f * u), at(a, R + 6.f * u).translated(2.f * u, 3.f * u)), 1.5f * u);
        g.setColour(Colour(0xFFB0231B)); g.drawLine(juce::Line<float>(at(a, R * 0.2f), at(a, R + 6.f * u)), 1.3f * u);
        auto sh = face.withTop(face.getBottom() - face.getHeight() * 0.14f);
        g.setGradientFill(juce::ColourGradient(Colour(0xFF2A2A2E), sh.getX(), sh.getY(), Colour(0xFF0C0C0E), sh.getX(), sh.getBottom(), false)); g.fillRect(sh);
    }
    g.setGradientFill(juce::ColourGradient(juce::Colours::white.withAlpha(0.2f), face.getX(), face.getY(), juce::Colours::transparentWhite, face.getX(), face.getY() + face.getHeight() * 0.45f, false));
    g.fillRoundedRectangle(face.withHeight(face.getHeight() * 0.45f), 3.f * u);
    g.setColour(juce::Colours::black.withAlpha(0.6f)); g.drawRoundedRectangle(face, 3.f * u, 1.f * u);
}

// ------------------------------------------------------------------ poignée de glisser-déposer (.mid)
class DragHandle : public juce::Component, public juce::SettableTooltipClient {
public:
    DragHandle(std::function<juce::File()> make, const Palette& pal, String title, String sub)
        : mk(std::move(make)), P(pal), t1(std::move(title)), t2(std::move(sub)) { setMouseCursor(juce::MouseCursor::DraggingHandCursor); }
    void paint(Graphics& g) override {
        auto r = getLocalBounds().toFloat().reduced(1.f);
        const float u = juce::jmin((float)getWidth() / 216.f, (float)getHeight() / 64.f);
        g.setColour(juce::Colours::black.withAlpha(0.45f)); g.fillRoundedRectangle(r, 4.f);
        g.setGradientFill(juce::ColourGradient(juce::Colours::black.withAlpha(0.5f), r.getX(), r.getY(), juce::Colours::white.withAlpha(0.12f), r.getX(), r.getBottom(), false));
        g.drawRoundedRectangle(r, 4.f, 1.f);
        if (dragging) { g.setColour(P.accent.withAlpha(0.6f)); g.drawRoundedRectangle(r.reduced(1.f), 4.f, 1.2f); }
        auto ic = Rectangle<float>(r.getX() + 14 * u, r.getCentreY() - 20 * u, 30 * u, 40 * u);
        juce::Path doc;
        doc.startNewSubPath(ic.getX(), ic.getY()); doc.lineTo(ic.getRight() - 9 * u, ic.getY()); doc.lineTo(ic.getRight(), ic.getY() + 9 * u);
        doc.lineTo(ic.getRight(), ic.getBottom()); doc.lineTo(ic.getX(), ic.getBottom()); doc.closeSubPath();
        g.setGradientFill(juce::ColourGradient(Colour(0xFF4A4A52), ic.getX(), ic.getY(), Colour(0xFF202024), ic.getX(), ic.getBottom(), false));
        g.fillPath(doc);
        g.setColour(juce::Colours::white.withAlpha(0.22f)); g.strokePath(doc, juce::PathStrokeType(1.f));
        g.setColour(P.accent); g.setFont(lcdFont(10.f * u));
        g.drawText(".MID", ic.withTop(ic.getCentreY() - 6 * u).withHeight(12 * u), juce::Justification::centred);
        g.setColour(P.silkInk().withAlpha(0.92f));
        tracked(g, t1, Rectangle<float>(r.getX() + 54 * u, r.getCentreY() - 15 * u, r.getWidth() - 58 * u, 15 * u), juce::Justification::centredLeft, 1.1f, fnt(10.5f * u, true));
        g.setColour(P.mute); g.setFont(fnt(9.5f * u));
        g.drawFittedText(t2, Rectangle<float>(r.getX() + 54 * u, r.getCentreY() + 1 * u, r.getWidth() - 58 * u, 24 * u).toNearestInt(), juce::Justification::topLeft, 2);
    }
    void mouseDrag(const juce::MouseEvent& e) override {
        if (dragging || e.getDistanceFromDragStart() < 6) return;
        dragging = true; repaint();
        auto f = mk();
        if (auto* c = juce::DragAndDropContainer::findParentDragContainerFor(this))
            c->performExternalDragDropOfFiles({f.getFullPathName()}, false, this, [this] { dragging = false; repaint(); });
        else { dragging = false; repaint(); }
    }
    void mouseUp(const juce::MouseEvent&) override { dragging = false; repaint(); }
private:
    std::function<juce::File()> mk; Palette P; String t1, t2; bool dragging = false;
};

}  // namespace kit::w
