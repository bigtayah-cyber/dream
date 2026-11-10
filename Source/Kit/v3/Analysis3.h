// Kit v3 : base des éditeurs de mesure (Détect, Mix) en « hybride v2 ». Même interface que ou::Base (OrbeUi.h, inchangé) :
// Classique = rendu d'origine à l'identique (boîtier d'appareil), Clair / Sombre = page v3 (fond, marque, cartes en cache).
#pragma once
#include "Rack3.h"
#include "../../Suite/OrbeAnalysis/OrbeUi.h"

namespace ou3 {
namespace v3 = kit::v3;
using juce::Colour, juce::Graphics, juce::Rectangle, juce::String;

class Base3 : public juce::AudioProcessorEditor, public juce::DragAndDropContainer, protected juce::Timer {
public:
    Base3(juce::AudioProcessor& p, float w, float h, float w3, float h3, String product, const char* serialNo, kit::Display display, Colour accent,
          const String& key, const String& first, const String& second, Colour accent3)
        : AudioProcessorEditor(&p), W(w), H(h), W3(w3), H3(h3), pal(ou::palette(accent)), look(pal), disp(display), scr(kit::displayColours(display, pal).ink),
          skin(key, accent3), product_(std::move(product)), serial_(serialNo), key_(key), first_(first), second_(second) {
        setLookAndFeel(&look);
        addAndMakeVisible(skin.btn);
        skin.btn.onTheme = [this](v3::Mode m) { setThemeMode(m, true); };
        skin.btn.onScale = [this](float s) { setUiScale(s); };
    }
    ~Base3() override { stopTimer(); v3::Prefs::setScale(key_, scale_); setLookAndFeel(nullptr); }

    void setThemeMode(v3::Mode m, bool persist = true) {
        skin.set(m);
        const bool n3 = isV3();
        setLookAndFeel(n3 ? static_cast<juce::LookAndFeel*>(&skin.look3) : static_cast<juce::LookAndFeel*>(&look));
        help.T = skin.tp();
        themeChanged(skin.tp());
        if (persist) v3::Prefs::setTheme(key_, m);
        if (v3::rk::fit(*this, n3, {W3, H3}, {W, H}, 0.8f, 1.7f, scale_)) resized();
        sendLookAndFeelChange();
        repaint();
    }
    void setUiScale(float s) {
        const bool n3 = isV3();
        scale_ = juce::jlimit(n3 ? 0.75f : 0.8f, n3 ? 1.5f : 1.7f, s);
        setSize(juce::roundToInt((n3 ? W3 : W) * scale_), juce::roundToInt((n3 ? H3 : H) * scale_));
        v3::Prefs::setScale(key_, scale_);
    }
    void paint(Graphics& g) final {
        if (!isV3()) {
            content = kit::w::drawFace(g, pal, getLocalBounds(), u_, {kit::w::Family::Meter, product_, serial_, kHeader, kSide});
            body(g); return;
        }
        skin.art.draw(g, String("bd|") + v3::modeKey(skin.mode), getLocalBounds().toFloat(), [this](Graphics& ig, Rectangle<float> b) {
            const float k = b.getWidth() / W3;
            v3::background(ig, skin.T, b);
            v3::rk::brand(ig, skin.T, k, first_, second_);
            backdrop3(ig, k);
        });
        body3(g);
    }
    void resized() final {
        if (isV3()) {
            u_ = (float)getWidth() / W3; scale_ = u_;
            help.layout(Rectangle<float>(30.f * u_, 30.f * u_).withPosition((W3 - 126.f) * u_, 34.f * u_).toNearestInt(), getLocalBounds());
            skin.btn.setBounds(Rectangle<float>(28.f * u_, 28.f * u_).withPosition((W3 - 160.f) * u_, 35.f * u_).toNearestInt());
            layout3();
        } else {
            u_ = (float)getWidth() / W; look.k = u_; scale_ = u_;
            content = getLocalBounds().toFloat().reduced(kSide * u_, 0);
            help.layout(kit::w::helpSlot(content, u_, false, kHeader).toNearestInt(), getLocalBounds());
            layout();
        }
        skin.btn.scale = scale_;
        repaint();
    }
protected:
    static constexpr float kHeader = 50.f, kSide = 16.f;
    bool isV3() const { return skin.v3(); }
    virtual void layout() = 0;      // Classique
    virtual void layout3() = 0;     // v3 (coordonnées W3 x H3)
    virtual void body(Graphics&) = 0;
    virtual void body3(Graphics&) = 0;
    virtual void backdrop3(Graphics&, float k) = 0;   // fonds et cartes (mis en cache)
    virtual void themeChanged(const v3::Theme* tp) = 0;   // tp nul : Classique
    void start(int hz, const String& key) {
        setResizable(true, true);
        scale_ = v3::Prefs::scale(key, 1.f);
        setThemeMode(v3::Prefs::theme(key, v3::Mode::Light), false);
        startTimerHz(hz);
    }
    void setHelp(const juce::StringArray& lines) { help.install(*this, product_, lines); }
    void well(Graphics& g, Rectangle<float> r) const { kit::screenWell(g, pal, r, disp); }
    Rectangle<float> R(float x, float y, float w, float h) const { return {x * u_, y * u_, w * u_, h * u_}; }
    Rectangle<int> Ri(float x, float y, float w, float h) const { return R(x, y, w, h).toNearestInt(); }
    float W, H, W3, H3, u_ = 1.f, scale_ = 1.f;
    kit::Palette pal; kit::w::ScaledLook look;
    kit::Display disp; Colour scr;
    v3::rk::Skin skin;
    juce::TooltipWindow tooltip{this, 600};
    Rectangle<float> content;
    v3::bridge::HelpKey help;
    String product_; const char* serial_;
    String key_, first_, second_;
};

}  // namespace ou3
