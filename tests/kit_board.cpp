// Planche de contrôle du Kit graphique : plaques, châssis, 5 styles de potards, voyants, vumètre,
// levier, poussoir, afficheurs. Rendu à l'échelle ×2 (Retina). Usage : kit_board sortie.png [échelle]
// Affiche aussi l'écart-type de luminance relatif de chaque finition de plaque (critère : 2..5 %).
#include "../Source/Kit/Kit.h"

static kit::Palette demoPalette(juce::Colour top, juce::Colour bot, kit::Finish f) {
    kit::Palette p;
    p.plateTop = top; p.plateBot = bot;
    p.ink = juce::Colour(0xFFE9E6F5); p.mute = juce::Colour(0xFF8C8AA0); p.line = juce::Colour(0x33FFFFFF);
    p.accent = juce::Colour(0xFFB9A6FF); p.accent2 = juce::Colour(0xFFFFB98A);
    p.screen = juce::Colour(0xFF0B0C10); p.screenInk = juce::Colour(0xFFBFE3FF);
    p.finish = f;
    return p;
}

// Écart-type relatif de la luminance sur r. Avec ref (même plaque sans matière, brush = 0), on mesure
// la matière seule (rapport img / ref) : le dégradé, le reflet et le vignettage sont exclus.
static double lumaStd(const juce::Image& img, juce::Rectangle<int> r, const juce::Image* ref = nullptr) {
    juce::Image::BitmapData d(img, juce::Image::BitmapData::readOnly);
    std::unique_ptr<juce::Image::BitmapData> dr;
    if (ref) dr = std::make_unique<juce::Image::BitmapData>(*ref, juce::Image::BitmapData::readOnly);
    auto luma = [](juce::Colour c) { return 0.2126 * c.getFloatRed() + 0.7152 * c.getFloatGreen() + 0.0722 * c.getFloatBlue(); };
    double s = 0, s2 = 0; int n = 0;
    for (int y = r.getY(); y < r.getBottom(); ++y)
        for (int x = r.getX(); x < r.getRight(); ++x) {
            double l = luma(d.getPixelColour(x, y));
            if (dr) l /= juce::jmax(1e-4, luma(dr->getPixelColour(x, y)));
            s += l; s2 += l * l; ++n;
        }
    const double m = s / n;
    return std::sqrt(juce::jmax(0.0, s2 / n - m * m)) / m;
}

struct Board : juce::Component {
    kit::Palette pal = demoPalette(juce::Colour(0xFF34343A), juce::Colour(0xFF222226), kit::Finish::BrushedAlu);
    void paint(juce::Graphics& g) override {
        kit::drawPlate(g, getLocalBounds(), pal, kit::seedFor("board"));
        auto inner = kit::chassis(g, pal, getLocalBounds().toFloat(), {true, false, 34.f, false});
        kit::engraved(g, pal, "Kit dreamaws " + kit::u8("\xc2\xb7") + " planche", inner.withHeight(34.f).reduced(14, 0), juce::Justification::centredLeft, 13.f, 2.f, true);
        // potards : une ligne par style, trois positions
        const kit::KnobStyle styles[] = {kit::KnobStyle::Chicken, kit::KnobStyle::Skirt, kit::KnobStyle::Cap, kit::KnobStyle::Fluted, kit::KnobStyle::Stepped, kit::KnobStyle::Modern};
        const char* names[] = {"Chicken", "Skirt", "Cap", "Fluted", "Stepped", "Modern"};
        for (int i = 0; i < 6; ++i) {
            const float x = inner.getX() + 70.f + (float)i * 150.f;
            kit::engraved(g, pal, names[i], {x - 60, 50, 120, 14}, juce::Justification::centred, 10.f, 1.4f, true);
            kit::knob(g, pal, x, 120.f, 34.f, 0.15f + 0.12f * (float)i, styles[i]);
            kit::knob(g, pal, x - 30.f, 215.f, 17.f, 0.5f, styles[i]);
            kit::knob(g, pal, x + 30.f, 215.f, 12.f, 0.9f, styles[i]);
        }
        // vumètre, voyants, levier, poussoirs
        kit::vuMeter(g, pal, {inner.getX() + 20.f, 260.f, 220.f, 132.f}, 0.f, false);
        kit::vuMeter(g, pal, {inner.getX() + 250.f, 260.f, 220.f, 132.f}, 2.6f, true);
        const juce::Colour lc[] = {pal.ledRed, pal.ledGreen, pal.ledAmber, pal.accent};
        for (int i = 0; i < 4; ++i) {
            kit::led(g, inner.getX() + 500.f + (float)i * 26.f, 280.f, 4.f, false, lc[i]);
            kit::led(g, inner.getX() + 500.f + (float)i * 26.f, 310.f, 4.f, true, lc[i]);
            kit::led(g, inner.getX() + 500.f + (float)i * 26.f, 340.f, 2.6f, 1.f, lc[i], kit::LedBezel::Chrome);
        }
        kit::leverSwitch(g, pal, {inner.getX() + 610.f, 262.f, 44.f, 76.f}, true);
        kit::leverSwitch(g, pal, {inner.getX() + 660.f, 262.f, 44.f, 76.f}, false, "IN", "OUT");
        kit::pushButton(g, pal, {inner.getX() + 720.f, 268.f, 46.f, 40.f}, true, false, pal.ledRed, "Rec");
        kit::pushButton(g, pal, {inner.getX() + 776.f, 268.f, 46.f, 40.f}, false, false, pal.ledAmber, "Learn");
        // afficheurs
        const kit::Display ds[] = {kit::Display::Classic, kit::Display::LCD, kit::Display::Amber, kit::Display::VFD, kit::Display::Seg7, kit::Display::OLED};
        const char* txt[] = {"Classic 12,4 dB", "LCD  -14.2 LUFS", "AMBER 440 Hz", "VFD PRESET 07", "128.0", "OLED A min"};
        for (int i = 0; i < 6; ++i) {
            auto r = juce::Rectangle<float>(inner.getX() + 20.f + (float)(i % 3) * 290.f, 412.f + (float)(i / 3) * 48.f, 270.f, 34.f);
            kit::screenWell(g, pal, r, ds[i]);
            kit::screenText(g, pal, ds[i], txt[i], r.reduced(10, 4), juce::Justification::centredLeft, ds[i] == kit::Display::VFD ? 14.f : 18.f);
        }
        g.setColour(pal.ink); g.setFont(kit::fnt(13.f)); g.drawText(kit::u8("Sans : \xc3\x89QUALISEUR, d\xc3\xa9" "cal\xc3\xa9, 0123456789"), juce::Rectangle<float>(inner.getX() + 20.f, 506.f, 420.f, 16.f), juce::Justification::centredLeft);
        g.setFont(kit::mono(12.f)); g.drawText("Mono : -12.0 dB  440,0 Hz", juce::Rectangle<float>(inner.getX() + 460.f, 506.f, 300.f, 16.f), juce::Justification::centredLeft);
        // coin de redimensionnement
        kit::screw(g, (float)getWidth() - 14.f, (float)getHeight() - 14.f, 5.f, 0.6f);
        kit::screw(g, 14.f, (float)getHeight() - 14.f, 5.f, 1.6f, kit::ScrewHead::Slot);
        kit::screw(g, 40.f, (float)getHeight() - 14.f, 5.f, 1.6f, kit::ScrewHead::Hex);
        kit::resizeGrip(g, kit::resizeCorner(getLocalBounds()).toFloat());
    }
};

int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI init;
    const float scale = argc > 2 ? (float)juce::String(argv[2]).getDoubleValue() : 2.f;
    Board b;
    b.setSize(940, 540);
    auto img = b.createComponentSnapshot(b.getLocalBounds(), true, scale);
    juce::File f(argv[1]); f.deleteFile();
    { juce::FileOutputStream os(f); juce::PNGImageFormat().writeImageToStream(img, os); }
    // écart-type de luminance relatif des finitions (matière seule sur 200 × 180 px logiques)
    const kit::Finish fins[] = {kit::Finish::BrushedAlu, kit::Finish::Anodized, kit::Finish::Hammertone, kit::Finish::Lacquer, kit::Finish::Bakelite};
    const char* fn[] = {"BrushedAlu", "Anodized", "Hammertone", "Lacquer", "Bakelite"};
    const juce::Colour tops[] = {juce::Colour(0xFF34343A), juce::Colour(0xFFB9B9BE), juce::Colour(0xFF4A5A52)};
    int fails = 0;
    for (int i = 0; i < 5; ++i)
        for (auto t : tops) {
            auto p = demoPalette(t, t.darker(0.15f), fins[i]);
            auto pl = kit::brushedPlate(400, 300, p, scale, kit::seedFor(fn[i]));
            auto flat = p; flat.brush = 0.f;
            auto pr = kit::brushedPlate(400, 300, flat, scale, kit::seedFor(fn[i]));
            const double sd = lumaStd(pl, juce::Rectangle<int>(100, 60, 200, 180) * scale, &pr);
            const double raw = lumaStd(pl, juce::Rectangle<int>(170, 120, 60, 60) * scale);
            std::printf("plaque %-10s %08x : matiere %.2f %% (zone brute 60x60 : %.2f %%)\n", fn[i], (unsigned)t.getARGB(), sd * 100.0, raw * 100.0);
            if (fins[i] == kit::Finish::BrushedAlu && (sd < 0.02 || sd > 0.05)) ++fails;
        }
    std::printf("%s\n", fails ? "ECHEC plaque brossee hors 2..5 %" : "OK");
    return fails ? 1 : 0;
}
