// Kit d'interface « matériel de studio » partagé par toutes les suites dreamaws.
// Tout est dessiné procéduralement ; seules les polices sont embarquées (OFL, voir assets/fonts/).
// Lumière : une seule source, en haut à gauche. Clair en haut à gauche, ombres vers le bas à droite.
// API rétrocompatible : les anciennes signatures existent toujours ; les nouveautés sont des surcharges.
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
#include <unordered_map>
#include <vector>
#include "FontsData.h"
#include "VuBallistics.h"

namespace kit {

// Matière de la face avant.
enum class Finish { BrushedAlu, Anodized, Hammertone, Lacquer, Bakelite };

struct Palette {
    juce::Colour plateTop, plateBot;  // face avant, haut → bas
    juce::Colour ink, mute, line;     // gravure, texte discret, filets
    juce::Colour accent, accent2;     // couleurs de voyants et d'index
    juce::Colour screen, screenInk;   // afficheur
    float brush = 1.f;                // intensité de la matière (stries, martelé…)
    // --- v2 (valeurs par défaut : rien à changer dans les éditeurs) ---
    Finish finish = Finish::BrushedAlu;
    juce::uint32 seed = 0;            // graine de la plaque (0 : dérivée des couleurs)
    juce::Colour silk;                // sérigraphie (transparent : = ink)
    juce::Colour ledRed{0xFFFF3B30}, ledGreen{0xFF3DDC84}, ledAmber{0xFFFFB02E};
    juce::Colour meterBack{0xFFEFE3C4};
    juce::Colour silkInk() const { return silk.isTransparent() ? ink : silk; }
};

// Les sources sont en UTF-8 ; JUCE lirait un const char* en Latin-1. Toujours passer par u8() pour un texte accentué.
inline juce::String u8(const char* s) { return juce::String::fromUTF8(s); }

// Majuscules y compris accents latins (JUCE ne les convertit pas).
inline juce::String upper(const juce::String& s) {
    juce::String o;
    for (auto c : s.toUpperCase()) o += (c >= 0xE0 && c <= 0xFE && c != 0xF7) ? (juce::juce_wchar)(c - 32) : c;
    return o;
}

// ============================================================ polices embarquées
// Sans-serif : IBM Plex Sans Condensed (sérigraphie) ; mono : IBM Plex Mono ; LCD : Share Tech Mono ; 7 segments : DSEG7.
// Identiques sous Linux, Mac et Windows : plus aucune police système.
class Fonts : private juce::DeletedAtShutdown {
public:
    Fonts() {
        auto mk = [](const unsigned char* d, std::size_t n) { return juce::Typeface::createSystemTypefaceFor(d, n); };
        sans = mk(fontdata::sansRegular, fontdata::sansRegularSize);
        sansBold = mk(fontdata::sansSemiBold, fontdata::sansSemiBoldSize);
        mono = mk(fontdata::monoMedium, fontdata::monoMediumSize);
        lcd = mk(fontdata::lcd, fontdata::lcdSize);
        seg7 = mk(fontdata::seg7, fontdata::seg7Size);
    }
    ~Fonts() override { clearSingletonInstance(); }
    juce::Typeface::Ptr sans, sansBold, mono, lcd, seg7;
    JUCE_DECLARE_SINGLETON_INLINE(Fonts, false)
};

// Plex a un cadratin plus petit que les polices système pour une même hauteur JUCE : on compense
// pour garder la même hauteur de capitales (la chasse condensée reste plus étroite qu'avant).
inline constexpr float kSansScale = 1.10f, kMonoScale = 1.08f;

inline juce::Font typefaceFont(const juce::Typeface::Ptr& tf, float h) {
    if (tf == nullptr) return juce::Font(juce::FontOptions(h));
    return juce::Font(juce::FontOptions(tf).withHeight(juce::jmax(1.f, h)));
}
inline const juce::String& family() {
    static const juce::String f = "IBM Plex Sans Condensed";
    return f;
}
inline juce::Font fnt(float h, bool bold = false) {
    auto* F = Fonts::getInstance();
    return typefaceFont(bold ? F->sansBold : F->sans, h * kSansScale);
}
inline juce::Font mono(float h) { return typefaceFont(Fonts::getInstance()->mono, h * kMonoScale); }
inline juce::Font lcdFont(float h) { return typefaceFont(Fonts::getInstance()->lcd, h * 1.12f); }
inline juce::Font seg7Font(float h) { return typefaceFont(Fonts::getInstance()->seg7, h); }

// Largeur d'un caractère, mise en cache (tracked() est appelé des centaines de fois par repeinte).
inline float glyphWidth(const juce::Font& f, juce::juce_wchar c) {
    static std::unordered_map<juce::uint64, float> cache;
    const auto name = (juce::uint64)(juce::uint32)(f.getTypefaceName() + f.getTypefaceStyle()).hashCode();
    const auto hq = (juce::uint64)juce::jlimit(0, 0xFFF, juce::roundToInt(f.getHeight() * 16.f));
    const juce::uint64 key = (name << 32) | (hq << 20) | ((juce::uint64)c & 0xFFFFF);
    if (auto it = cache.find(key); it != cache.end()) return it->second;
    juce::GlyphArrangement one;
    one.addLineOfText(f, juce::String::charToString(c), 0, 0);
    const float w = one.getNumGlyphs() > 0 ? one.getBoundingBox(0, 1, true).getWidth() : f.getHeight() * 0.3f;
    if (cache.size() > 16384) cache.clear();
    cache.emplace(key, w);
    return w;
}

// Texte espacé, en capitales, comme une sérigraphie.
inline void tracked(juce::Graphics& g, const juce::String& s, juce::Rectangle<float> r, juce::Justification j, float tracking, const juce::Font& f) {
    g.setFont(f);
    const auto up = upper(s);
    float w = 0;
    for (auto c : up) w += glyphWidth(f, c) + tracking;
    w -= tracking;
    float x = j.testFlags(juce::Justification::right) ? r.getRight() - w : j.testFlags(juce::Justification::horizontallyCentred) ? r.getCentreX() - w / 2 : r.getX();
    for (auto c : up) {
        const float cw = glyphWidth(f, c);
        g.drawText(juce::String::charToString(c), juce::Rectangle<float>(x - 2, r.getY(), cw + 4, r.getHeight()), juce::Justification::centredLeft, false);
        x += cw + tracking;
    }
}

// Gravure / sérigraphie : léger relief clair en dessous (lumière en haut à gauche).
inline void engraved(juce::Graphics& g, const Palette& p, const juce::String& s, juce::Rectangle<float> r, juce::Justification j, float size, float tracking = 1.6f, bool bold = false) {
    const auto f = fnt(size, bold);
    g.setColour(juce::Colours::black.withAlpha(0.28f));
    tracked(g, s, r.translated(0, 0.8f), j, tracking, f);
    g.setColour(p.silkInk().withAlpha(0.88f));
    tracked(g, s, r, j, tracking, f);
}

// ============================================================ bruit procédural
namespace detail {
inline juce::uint32 hash32(juce::uint32 x) {
    x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
    return x;
}
inline float hnoise(juce::uint32 seed, int i, int j) {
    const juce::uint32 h = hash32(seed ^ hash32((juce::uint32)i * 0x9E3779B1U ^ hash32((juce::uint32)j + 0x632BE5ABU)));
    return (float)(h & 0xFFFFFFU) / 8388607.5f - 1.f;
}
inline float smooth(float t) { return t * t * (3.f - 2.f * t); }
inline float noise1(juce::uint32 seed, float x, int row) {
    const float fl = std::floor(x);
    const int i = (int)fl;
    return juce::jmap(smooth(x - fl), hnoise(seed, i, row), hnoise(seed, i + 1, row));
}
inline float noise2(juce::uint32 seed, float x, float y) {
    const float fx = std::floor(x), fy = std::floor(y);
    const int i = (int)fx, j = (int)fy;
    const float tx = smooth(x - fx), ty = smooth(y - fy);
    const float a = juce::jmap(tx, hnoise(seed, i, j), hnoise(seed, i + 1, j));
    const float b = juce::jmap(tx, hnoise(seed, i, j + 1), hnoise(seed, i + 1, j + 1));
    return juce::jmap(ty, a, b);
}
inline float sq(float x) { return x * x; }
// Angle de la lumière (haut-gauche) en coordonnées écran (y vers le bas).
inline constexpr float lightAngle = -2.35619449f;
inline float facing(float a) { return std::cos(a - lightAngle); }
}  // namespace detail

inline juce::uint32 seedFor(const juce::String& name) { return detail::hash32((juce::uint32)name.hashCode()); }
inline juce::uint32 paletteSeed(const Palette& p) {
    if (p.seed != 0) return p.seed;
    return detail::hash32(p.plateTop.getARGB() ^ detail::hash32(p.plateBot.getARGB() ^ detail::hash32(p.accent.getARGB() ^ p.accent2.getARGB())));
}

// ============================================================ plaque v2
// Plaque générée à la résolution physique (w × scale, h × scale) : stries orientées (bruit 1D filtré
// le long de x, 2–3 octaves, corrélation 30–400 px), variation lente d'une ligne à l'autre, effet
// multiplicatif (aucun gris ajouté), reflet anisotrope doux, chanfrein de 2 px intégré.
inline juce::Image brushedPlate(int w, int h, const Palette& p, float scale, juce::uint32 seed, bool bevel = true) {
    using namespace detail;
    const float s = juce::jlimit(0.5f, 4.f, scale);
    const int W = juce::jmax(1, juce::roundToInt((float)w * s)), H = juce::jmax(1, juce::roundToInt((float)h * s));
    juce::Image img(juce::Image::ARGB, W, H, false);
    juce::Image::BitmapData d(img, juce::Image::BitmapData::writeOnly);
    const float amp = 0.066f * p.brush;
    const juce::uint32 sd = seed == 0 ? paletteSeed(p) : seed;
    // profils précalculés
    std::vector<float> rowK((size_t)H), colK((size_t)W);
    for (int y = 0; y < H; ++y) {
        const float fy = (float)y / (float)juce::jmax(1, H - 1);
        rowK[(size_t)y] = 1.f + 0.055f * std::exp(-sq((fy - 0.27f) / 0.16f)) - 0.035f * sq(juce::jmax(0.f, fy - 0.55f) / 0.45f);
    }
    for (int x = 0; x < W; ++x) colK[(size_t)x] = 1.f - 0.06f * sq((float)x / (float)juce::jmax(1, W - 1) * 2.f - 1.f);
    const float bev = 2.f * s;
    const auto top = p.plateTop, bot = p.plateBot;
    for (int y = 0; y < H; ++y) {
        const float fy = (float)y / (float)juce::jmax(1, H - 1);
        const auto base = top.interpolatedWith(bot, fy);
        const float br = base.getFloatRed(), bg = base.getFloatGreen(), bb = base.getFloatBlue();
        // termes constants sur la ligne (stries)
        const float rowFine = hnoise(sd + 11, 0, y);
        const float rowMid = noise1(sd + 12, (float)y / (3.f * s), 0);
        const float rowSlow = noise1(sd + 13, (float)y / (40.f * s), 0);
        const float ay = 0.6f + 0.4f * std::abs(hnoise(sd + 14, 1, y));
        auto* line = d.getLinePointer(y);
        for (int x = 0; x < W; ++x) {
            float n = 0.f;
            switch (p.finish) {
                case Finish::BrushedAlu:
                case Finish::Anodized: {
                    const float along = 0.45f * noise1(sd + 1, (float)x / (30.f * s), y) + 0.30f * noise1(sd + 2, (float)x / (120.f * s), y)
                                        + 0.20f * noise1(sd + 3, (float)x / (400.f * s), y / 2);
                    n = 0.42f * rowFine + 0.25f * rowMid + 0.22f * rowSlow + ay * along;
                    if (p.finish == Finish::Anodized) n *= 0.75f;
                    break;
                }
                case Finish::Hammertone: {  // martelé : relief éclairé du haut-gauche (dérivée diagonale du bruit)
                    const float xs = (float)x / s, ys = (float)y / s, e = 1.6f;
                    const float b1 = noise2(sd + 4, (xs - e) / 5.f, (ys - e) / 5.f) - noise2(sd + 4, (xs + e) / 5.f, (ys + e) / 5.f);
                    const float b2 = noise2(sd + 5, (xs - e) / 13.f, (ys - e) / 13.f) - noise2(sd + 5, (xs + e) / 13.f, (ys + e) / 13.f);
                    n = 0.9f * b1 + 0.6f * b2 + 0.25f * hnoise(sd + 6, x, y);
                    break;
                }
                case Finish::Lacquer:
                    n = 0.25f * noise2(sd + 7, (float)x / (70.f * s), (float)y / (70.f * s)) + 0.12f * hnoise(sd + 8, x, y);
                    break;
                case Finish::Bakelite:
                    n = 0.55f * noise2(sd + 9, (float)x / (26.f * s), (float)y / (26.f * s)) + 0.30f * noise2(sd + 10, (float)x / (7.f * s), (float)y / (7.f * s))
                        + 0.15f * hnoise(sd + 8, x, y);
                    break;
            }
            float k = (1.f + amp * n) * rowK[(size_t)y] * colK[(size_t)x];
            if (bevel) {
                const float tl = (float)juce::jmin(x, y), brd = (float)juce::jmin(W - 1 - x, H - 1 - y), edge = juce::jmin(tl, brd);
                if (edge < 1.f) k *= 0.45f;                       // arête : filet sombre
                else if (edge < bev + 1.f) k *= tl <= brd ? 1.24f : 0.66f;  // chanfrein : clair en haut-gauche, sombre en bas-droite
                else if (edge < bev + 1.f + s) k *= 0.93f;        // pied du chanfrein
            }
            auto* px = reinterpret_cast<juce::PixelARGB*>(line + x * d.pixelStride);
            px->setARGB(255, (juce::uint8)juce::jlimit(0, 255, (int)(br * k * 255.f + 0.5f)), (juce::uint8)juce::jlimit(0, 255, (int)(bg * k * 255.f + 0.5f)),
                        (juce::uint8)juce::jlimit(0, 255, (int)(bb * k * 255.f + 0.5f)));
        }
    }
    return img;
}

// Ancienne signature : plaque v2 en pixels logiques (dessinée par g.drawImageAt). Graine dérivée de la palette.
inline juce::Image brushedPlate(int w, int h, const Palette& p) { return brushedPlate(w, h, p, 1.f, paletteSeed(p)); }

// Plaque nette sur écran Retina : générée à l'échelle physique du contexte, mise en cache
// (clé : largeur, hauteur, échelle, graine, palette), puis posée pixel pour pixel.
// À utiliser dans paint() à la place de g.drawImageAt(plate, 0, 0) : kit::drawPlate(g, getLocalBounds(), pal, kit::seedFor("Nom"));
class PlateCache : private juce::DeletedAtShutdown {
public:
    ~PlateCache() override { clearSingletonInstance(); }
    juce::Image get(int w, int h, const Palette& p, float scale, juce::uint32 seed) {
        const juce::uint32 pk = detail::hash32(p.plateTop.getARGB() ^ detail::hash32(p.plateBot.getARGB() + (juce::uint32)p.finish * 977U + (juce::uint32)(p.brush * 1000.f)));
        const juce::String key = juce::String(w) + "x" + juce::String(h) + "@" + juce::String(juce::roundToInt(scale * 100.f)) + ":" + juce::String((juce::int64)seed) + ":" + juce::String((juce::int64)pk);
        for (auto& e : entries) if (e.first == key) return e.second;
        auto img = brushedPlate(w, h, p, scale, seed);
        entries.insert(entries.begin(), {key, img});
        if (entries.size() > 6) entries.pop_back();
        return img;
    }
    JUCE_DECLARE_SINGLETON_SINGLETHREADED_INLINE(PlateCache, false)
private:
    std::vector<std::pair<juce::String, juce::Image>> entries;
};

inline float physicalScale(juce::Graphics& g) { return juce::jlimit(1.f, 4.f, g.getInternalContext().getPhysicalPixelScaleFactor()); }

inline void drawPlate(juce::Graphics& g, juce::Rectangle<int> area, const Palette& p, juce::uint32 seed = 0) {
    const float s = physicalScale(g);
    auto img = PlateCache::getInstance()->get(area.getWidth(), area.getHeight(), p, s, seed == 0 ? paletteSeed(p) : seed);
    juce::Graphics::ScopedSaveState st(g);
    g.setImageResamplingQuality(juce::Graphics::lowResamplingQuality);
    g.drawImageTransformed(img, juce::AffineTransform::scale((float)area.getWidth() / (float)img.getWidth(), (float)area.getHeight() / (float)img.getHeight())
                                    .translated((float)area.getX(), (float)area.getY()));
}

// ============================================================ vis
enum class ScrewHead { Phillips, Slot, Hex };

// Vis à tête bombée dans une fraisure : cône sombre côté lumière, lèvre claire en bas à droite, ombre portée.
inline void screw(juce::Graphics& g, float cx, float cy, float r, float angle, ScrewHead head = ScrewHead::Phillips) {
    const float R = r * 1.30f;
    // fraisure
    g.setGradientFill(juce::ColourGradient(juce::Colours::black.withAlpha(0.62f), cx - R * 0.7f, cy - R * 0.7f, juce::Colours::white.withAlpha(0.22f), cx + R * 0.7f, cy + R * 0.7f, false));
    g.fillEllipse(cx - R, cy - R, R * 2, R * 2);
    g.setColour(juce::Colours::black.withAlpha(0.35f));
    g.drawEllipse(cx - R, cy - R, R * 2, R * 2, juce::jmax(0.5f, r * 0.08f));
    // ombre de la tête dans la fraisure
    g.setColour(juce::Colours::black.withAlpha(0.45f));
    g.fillEllipse(cx - r + r * 0.10f, cy - r + r * 0.16f, r * 2, r * 2);
    // tête bombée
    juce::ColourGradient dome(juce::Colour(0xFFE4E4E8), cx - r * 0.38f, cy - r * 0.45f, juce::Colour(0xFF3A3A3F), cx + r * 0.9f, cy + r * 0.9f, true);
    dome.addColour(0.45, juce::Colour(0xFFA6A6AC));
    g.setGradientFill(dome);
    g.fillEllipse(cx - r, cy - r, r * 2, r * 2);
    g.setColour(juce::Colours::black.withAlpha(0.5f));
    g.drawEllipse(cx - r, cy - r, r * 2, r * 2, juce::jmax(0.5f, r * 0.07f));
    // empreinte
    const auto tf = juce::AffineTransform::rotation(angle).translated(cx, cy);
    const auto dark = juce::Colour(0xFF161618), lip = juce::Colours::white.withAlpha(0.35f);
    juce::Path p;
    if (head == ScrewHead::Hex) {
        p.addPolygon({0, 0}, 6, r * 0.46f, angle);
        p.applyTransform(juce::AffineTransform::translation(cx, cy));
    } else {
        p.addRoundedRectangle(-r * 0.74f, -r * 0.12f, r * 1.48f, r * 0.24f, r * 0.08f);
        if (head == ScrewHead::Phillips) p.addRoundedRectangle(-r * 0.12f, -r * 0.74f, r * 0.24f, r * 1.48f, r * 0.08f);
        p.applyTransform(tf);
    }
    g.setColour(lip);
    g.fillPath(p, juce::AffineTransform::translation(r * 0.05f, r * 0.10f));
    g.setColour(dark);
    g.fillPath(p);
}

// ============================================================ châssis
// Zone de la poignée de redimensionnement JUCE (18 px en bas à droite) : à laisser libre de commandes.
inline juce::Rectangle<int> resizeCorner(juce::Rectangle<int> bounds, int size = 18) { return bounds.removeFromBottom(size).removeFromRight(size); }

// Poignée gravée qui reste dans le tout dernier coin : la vis du coin bas-droit reste visible.
inline void resizeGrip(juce::Graphics& g, juce::Rectangle<float> corner, bool active = false) {
    const float x1 = corner.getRight(), y1 = corner.getBottom();
    for (int i = 0; i < 3; ++i) {
        const float d = 2.5f + (float)i * 2.6f;
        if (d > 8.f) break;
        g.setColour(juce::Colours::black.withAlpha(0.55f));
        g.drawLine(x1 - d, y1 - 1.2f, x1 - 1.2f, y1 - d, 1.f);
        g.setColour(juce::Colours::white.withAlpha(active ? 0.45f : 0.16f));
        g.drawLine(x1 - d + 0.9f, y1 - 1.2f, x1 - 1.2f, y1 - d + 0.9f, 0.8f);
    }
}

struct Chassis {
    bool rackEars = false;    // oreilles de rack 19" avec lumières oblongues
    bool woodCheeks = false;  // joues en bois (instruments)
    float header = 0.f;       // hauteur du bandeau d'en-tête (0 : aucun)
    bool bevel = true;        // chanfrein de 2 px autour
};

// Habillage par-dessus la plaque. Renvoie la zone utile (sans oreilles ni joues).
inline juce::Rectangle<float> chassis(juce::Graphics& g, const Palette& p, juce::Rectangle<float> r, const Chassis& c = {}) {
    auto inner = r;
    auto bevelRect = [&](juce::Rectangle<float> b, float w) {
        g.setColour(juce::Colours::white.withAlpha(0.20f));
        g.fillRect(b.getX(), b.getY(), b.getWidth(), w);
        g.fillRect(b.getX(), b.getY(), w, b.getHeight());
        g.setColour(juce::Colours::black.withAlpha(0.42f));
        g.fillRect(b.getX(), b.getBottom() - w, b.getWidth(), w);
        g.fillRect(b.getRight() - w, b.getY(), w, b.getHeight());
    };
    if (c.woodCheeks) {
        const float cw = juce::jlimit(14.f, 40.f, r.getWidth() * 0.035f);
        for (auto side : {r.withWidth(cw), r.withLeft(r.getRight() - cw)}) {
            juce::ColourGradient wood(juce::Colour(0xFF5A3A22), side.getX(), side.getY(), juce::Colour(0xFF2E1C10), side.getRight(), side.getBottom(), false);
            wood.addColour(0.5, juce::Colour(0xFF46301C));
            g.setGradientFill(wood);
            g.fillRect(side);
            const juce::uint32 sd = paletteSeed(p) + (side.getX() > r.getX() ? 99U : 7U);
            for (int i = 0; i < 26; ++i) {  // veinage
                const float x0 = side.getX() + side.getWidth() * ((float)i + 0.5f) / 26.f;
                juce::Path vein;
                vein.startNewSubPath(x0, side.getY());
                for (float y = side.getY(); y <= side.getBottom(); y += 6.f)
                    vein.lineTo(x0 + side.getWidth() * 0.06f * detail::noise1(sd + (juce::uint32)i, y / 70.f, i), y);
                g.setColour((i % 3 == 0 ? juce::Colours::black.withAlpha(0.22f) : juce::Colour(0xFF8A5C34).withAlpha(0.18f)));
                g.strokePath(vein, juce::PathStrokeType(i % 3 == 0 ? 1.1f : 0.7f));
            }
            g.setGradientFill(juce::ColourGradient(juce::Colours::white.withAlpha(0.10f), side.getX(), side.getY(), juce::Colours::transparentWhite, side.getX(), side.getCentreY(), false));
            g.fillRect(side);
            bevelRect(side, 1.5f);
        }
        inner = r.reduced(cw, 0);
        g.setColour(juce::Colours::black.withAlpha(0.35f));  // ombre des joues sur la face
        g.fillRect(inner.getX(), inner.getY(), 3.f, inner.getHeight());
        g.fillRect(inner.getRight() - 2.f, inner.getY(), 2.f, inner.getHeight());
    }
    if (c.rackEars) {
        const float ew = juce::jlimit(16.f, 34.f, inner.getWidth() * 0.045f);
        for (auto ear : {inner.withWidth(ew), inner.withLeft(inner.getRight() - ew)}) {
            g.setColour(juce::Colours::black.withAlpha(0.22f));
            g.fillRect(ear);
            bevelRect(ear, 1.f);
            for (float fy : {0.12f, 0.88f}) {
                auto slot = juce::Rectangle<float>(ew * 0.42f, juce::jmin(ew * 0.95f, ear.getHeight() * 0.16f)).withCentre({ear.getCentreX(), ear.getY() + ear.getHeight() * fy});
                g.setColour(juce::Colours::black.withAlpha(0.8f));
                g.fillRoundedRectangle(slot, slot.getWidth() * 0.5f);
                g.setColour(juce::Colours::white.withAlpha(0.16f));
                g.drawRoundedRectangle(slot.translated(0.4f, 0.6f), slot.getWidth() * 0.5f, 0.8f);
                screw(g, slot.getCentreX(), slot.getCentreY(), slot.getWidth() * 0.62f, 0.5f + fy);
            }
        }
        inner = inner.reduced(ew, 0);
    }
    if (c.header > 0.f) {
        auto hb = inner.withHeight(c.header);
        g.setGradientFill(juce::ColourGradient(juce::Colours::black.withAlpha(0.10f), hb.getX(), hb.getY(), juce::Colours::black.withAlpha(0.22f), hb.getX(), hb.getBottom(), false));
        g.fillRect(hb);
        g.setColour(juce::Colours::black.withAlpha(0.55f));
        g.fillRect(hb.getX(), hb.getBottom() - 1.f, hb.getWidth(), 1.f);
        g.setColour(juce::Colours::white.withAlpha(0.10f));
        g.fillRect(hb.getX(), hb.getBottom(), hb.getWidth(), 1.f);
    }
    if (c.bevel) bevelRect(r, 1.5f);
    return inner;
}

// ============================================================ voyants
enum class LedBezel { Chrome, Black, None };

// Voyant v2 : lentille teintée visible même éteinte, bague, cœur presque blanc et halo quand il est allumé.
inline void led(juce::Graphics& g, float cx, float cy, float r, float level, juce::Colour c, LedBezel bezel) {
    level = juce::jlimit(0.f, 1.f, level);
    if (bezel != LedBezel::None) {
        const float br = r * 1.42f;
        g.setColour(juce::Colours::black.withAlpha(0.45f));
        g.fillEllipse(cx - br + r * 0.08f, cy - br + r * 0.2f, br * 2, br * 2);
        if (bezel == LedBezel::Chrome)
            g.setGradientFill(juce::ColourGradient(juce::Colour(0xFFE6E6EA), cx - br * 0.6f, cy - br * 0.7f, juce::Colour(0xFF3C3C42), cx + br * 0.7f, cy + br * 0.8f, false));
        else
            g.setGradientFill(juce::ColourGradient(juce::Colour(0xFF4A4A4F), cx - br * 0.6f, cy - br * 0.7f, juce::Colour(0xFF0A0A0B), cx + br * 0.7f, cy + br * 0.8f, false));
        g.fillEllipse(cx - br, cy - br, br * 2, br * 2);
        g.setColour(juce::Colours::black.withAlpha(0.7f));
        g.fillEllipse(cx - r * 1.08f, cy - r * 1.08f, r * 2.16f, r * 2.16f);
    }
    if (level > 0.01f) {
        juce::ColourGradient halo(c.withAlpha(0.42f * level), cx, cy, c.withAlpha(0.f), cx + r * 3.4f, cy, true);
        halo.addColour(0.35, c.withAlpha(0.18f * level));
        g.setGradientFill(halo);
        g.fillEllipse(cx - r * 3.4f, cy - r * 3.4f, r * 6.8f, r * 6.8f);
    }
    const auto off = c.withMultipliedSaturation(0.85f).withMultipliedBrightness(0.30f);
    const auto lens = off.interpolatedWith(c, level);
    g.setGradientFill(juce::ColourGradient(lens.brighter(0.35f + 0.3f * level), cx - r * 0.3f, cy - r * 0.35f, lens.darker(0.6f - 0.35f * level), cx + r, cy + r, true));
    g.fillEllipse(cx - r, cy - r, r * 2, r * 2);
    if (level > 0.01f) {
        g.setGradientFill(juce::ColourGradient(juce::Colours::white.withAlpha(0.88f * level), cx, cy, c.withAlpha(0.f), cx + r * 0.75f, cy, true));
        g.fillEllipse(cx - r * 0.75f, cy - r * 0.75f, r * 1.5f, r * 1.5f);
    }
    g.setColour(juce::Colours::white.withAlpha(0.30f + 0.25f * level));
    g.fillEllipse(cx - r * 0.55f, cy - r * 0.72f, r * 0.62f, r * 0.40f);
}
inline void led(juce::Graphics& g, float cx, float cy, float r, bool on, juce::Colour c) { led(g, cx, cy, r, on ? 1.f : 0.f, c, LedBezel::Black); }

// ============================================================ afficheurs
enum class Display { Classic, LCD, Amber, VFD, Seg7, OLED };

struct DisplayColours { juce::Colour back, ink, ghost; };
inline DisplayColours displayColours(Display d, const Palette& p) {
    switch (d) {
        case Display::LCD:   return {juce::Colour(0xFF9DB46A), juce::Colour(0xFF1C2610), juce::Colour(0x141C2610)};
        case Display::Amber: return {juce::Colour(0xFF140B02), juce::Colour(0xFFFFB02E), juce::Colour(0x12FFB02E)};
        case Display::VFD:   return {juce::Colour(0xFF040A0A), juce::Colour(0xFF6CF2D6), juce::Colour(0x126CF2D6)};
        case Display::Seg7:  return {juce::Colour(0xFF110404), juce::Colour(0xFFFF3B2E), juce::Colour(0x14FF3B2E)};
        case Display::OLED:  return {juce::Colour(0xFF020203), juce::Colour(0xFFE8F1FF), juce::Colour(0x0AE8F1FF)};
        case Display::Classic: break;
    }
    return {p.screen, p.screenInk, p.screenInk.withAlpha(0.06f)};
}

// Afficheur encastré : biseau usiné, verre, reflet, lignes de balayage très discrètes.
inline void screenWell(juce::Graphics& g, const Palette& p, juce::Rectangle<float> r, Display type) {
    const auto dc = displayColours(type, p);
    // découpe dans la plaque : ombre en haut-gauche, lèvre claire en bas-droite
    g.setColour(juce::Colours::black.withAlpha(0.6f));
    g.fillRoundedRectangle(r.expanded(3.f), 6.f);
    g.setGradientFill(juce::ColourGradient(juce::Colours::black.withAlpha(0.55f), r.getX(), r.getY() - 3, juce::Colours::white.withAlpha(0.16f), r.getX(), r.getBottom() + 3, false));
    g.drawRoundedRectangle(r.expanded(3.f), 6.f, 1.2f);
    // fond de l'écran
    g.setGradientFill(juce::ColourGradient(dc.back.brighter(0.06f), r.getX(), r.getY(), dc.back.darker(0.25f), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle(r, 4.f);
    if (type == Display::LCD) {  // rétroéclairage plus fort au centre
        g.setGradientFill(juce::ColourGradient(juce::Colours::white.withAlpha(0.12f), r.getCentreX(), r.getCentreY(), juce::Colours::transparentWhite, r.getX(), r.getY(), true));
        g.fillRoundedRectangle(r, 4.f);
    } else {
        g.setColour(juce::Colours::white.withAlpha(0.016f));
        for (float y = r.getY(); y < r.getBottom(); y += 3.f) g.fillRect(r.getX(), y, r.getWidth(), 1.f);
    }
    // vignettage intérieur et ombre portée du biseau
    g.setGradientFill(juce::ColourGradient(juce::Colours::transparentBlack, r.getCentreX(), r.getCentreY(), juce::Colours::black.withAlpha(0.32f), r.getX(), r.getY(), true));
    g.fillRoundedRectangle(r, 4.f);
    g.setGradientFill(juce::ColourGradient(juce::Colours::black.withAlpha(0.35f), r.getX(), r.getY(), juce::Colours::transparentBlack, r.getX(), r.getY() + juce::jmin(10.f, r.getHeight() * 0.25f), false));
    g.fillRoundedRectangle(r, 4.f);
    // reflet du verre : dégradé diagonal doux depuis le haut-gauche (sans arête dure)
    const float gl = juce::jmin(r.getWidth(), r.getHeight() * 2.f) * 0.55f;
    juce::ColourGradient glare(juce::Colours::white.withAlpha(0.055f), r.getX(), r.getY(), juce::Colours::transparentWhite, r.getX() + gl * 0.7f, r.getY() + gl * 0.7f, false);
    glare.addColour(0.5, juce::Colours::white.withAlpha(0.02f));
    g.setGradientFill(glare);
    g.fillRoundedRectangle(r, 4.f);
}
inline void screenWell(juce::Graphics& g, const Palette& p, juce::Rectangle<float> r) { screenWell(g, p, r, Display::Classic); }

// Matrice de points 5 × 7 (VFD), dessinée en vectoriel.
namespace detail {
inline const unsigned char* glyph5x7(juce::juce_wchar c) {
    static const unsigned char blank[7] = {0, 0, 0, 0, 0, 0, 0};
    struct G { char c; unsigned char rows[7]; };
    static const G table[] = {
        {'0', {0b01110, 0b10001, 0b10011, 0b10101, 0b11001, 0b10001, 0b01110}}, {'1', {0b00100, 0b01100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110}},
        {'2', {0b01110, 0b10001, 0b00001, 0b00010, 0b00100, 0b01000, 0b11111}}, {'3', {0b11111, 0b00010, 0b00100, 0b00010, 0b00001, 0b10001, 0b01110}},
        {'4', {0b00010, 0b00110, 0b01010, 0b10010, 0b11111, 0b00010, 0b00010}}, {'5', {0b11111, 0b10000, 0b11110, 0b00001, 0b00001, 0b10001, 0b01110}},
        {'6', {0b00110, 0b01000, 0b10000, 0b11110, 0b10001, 0b10001, 0b01110}}, {'7', {0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b01000, 0b01000}},
        {'8', {0b01110, 0b10001, 0b10001, 0b01110, 0b10001, 0b10001, 0b01110}}, {'9', {0b01110, 0b10001, 0b10001, 0b01111, 0b00001, 0b00010, 0b01100}},
        {'A', {0b01110, 0b10001, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001}}, {'B', {0b11110, 0b10001, 0b10001, 0b11110, 0b10001, 0b10001, 0b11110}},
        {'C', {0b01110, 0b10001, 0b10000, 0b10000, 0b10000, 0b10001, 0b01110}}, {'D', {0b11100, 0b10010, 0b10001, 0b10001, 0b10001, 0b10010, 0b11100}},
        {'E', {0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b11111}}, {'F', {0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b10000}},
        {'G', {0b01110, 0b10001, 0b10000, 0b10111, 0b10001, 0b10001, 0b01111}}, {'H', {0b10001, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001}},
        {'I', {0b01110, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110}}, {'J', {0b00111, 0b00010, 0b00010, 0b00010, 0b00010, 0b10010, 0b01100}},
        {'K', {0b10001, 0b10010, 0b10100, 0b11000, 0b10100, 0b10010, 0b10001}}, {'L', {0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b11111}},
        {'M', {0b10001, 0b11011, 0b10101, 0b10101, 0b10001, 0b10001, 0b10001}}, {'N', {0b10001, 0b10001, 0b11001, 0b10101, 0b10011, 0b10001, 0b10001}},
        {'O', {0b01110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110}}, {'P', {0b11110, 0b10001, 0b10001, 0b11110, 0b10000, 0b10000, 0b10000}},
        {'Q', {0b01110, 0b10001, 0b10001, 0b10001, 0b10101, 0b10010, 0b01101}}, {'R', {0b11110, 0b10001, 0b10001, 0b11110, 0b10100, 0b10010, 0b10001}},
        {'S', {0b01111, 0b10000, 0b10000, 0b01110, 0b00001, 0b00001, 0b11110}}, {'T', {0b11111, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100}},
        {'U', {0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110}}, {'V', {0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01010, 0b00100}},
        {'W', {0b10001, 0b10001, 0b10001, 0b10101, 0b10101, 0b10101, 0b01010}}, {'X', {0b10001, 0b10001, 0b01010, 0b00100, 0b01010, 0b10001, 0b10001}},
        {'Y', {0b10001, 0b10001, 0b10001, 0b01010, 0b00100, 0b00100, 0b00100}}, {'Z', {0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b10000, 0b11111}},
        {'.', {0, 0, 0, 0, 0, 0b01100, 0b01100}},           {',', {0, 0, 0, 0, 0b01100, 0b00100, 0b01000}},
        {':', {0, 0b01100, 0b01100, 0, 0b01100, 0b01100, 0}}, {'-', {0, 0, 0, 0b11111, 0, 0, 0}},
        {'+', {0, 0b00100, 0b00100, 0b11111, 0b00100, 0b00100, 0}}, {'/', {0, 0b00001, 0b00010, 0b00100, 0b01000, 0b10000, 0}},
        {'%', {0b11000, 0b11001, 0b00010, 0b00100, 0b01000, 0b10011, 0b00011}}, {'#', {0b01010, 0b01010, 0b11111, 0b01010, 0b11111, 0b01010, 0b01010}},
        {'(', {0b00010, 0b00100, 0b01000, 0b01000, 0b01000, 0b00100, 0b00010}}, {')', {0b01000, 0b00100, 0b00010, 0b00010, 0b00010, 0b00100, 0b01000}},
        {'\'', {0b01100, 0b00100, 0b01000, 0, 0, 0, 0}},       {'!', {0b00100, 0b00100, 0b00100, 0b00100, 0, 0, 0b00100}},
        {'?', {0b01110, 0b10001, 0b00001, 0b00010, 0b00100, 0, 0b00100}}, {'=', {0, 0, 0b11111, 0, 0b11111, 0, 0}},
        {'<', {0b00010, 0b00100, 0b01000, 0b10000, 0b01000, 0b00100, 0b00010}}, {'>', {0b01000, 0b00100, 0b00010, 0b00001, 0b00010, 0b00100, 0b01000}},
        {'_', {0, 0, 0, 0, 0, 0, 0b11111}},                 {'*', {0, 0b00100, 0b10101, 0b01110, 0b10101, 0b00100, 0}},
    };
    static const unsigned char degree[7] = {0b01100, 0b10010, 0b10010, 0b01100, 0, 0, 0};
    static const unsigned char middot[7] = {0, 0, 0, 0b01100, 0b01100, 0, 0};
    if (c == 0xB0) return degree;
    if (c == 0xB7) return middot;
    // lettres accentuées latines (déjà en capitales) → lettre de base
    if (c >= 0xC0 && c <= 0xDD) {
        static const char base[] = "AAAAAAACEEEEIIIIDNOOOOOxOUUUUY";
        c = (juce::juce_wchar)base[c - 0xC0];
    }
    for (auto& gl : table) if ((juce::juce_wchar)gl.c == c) return gl.rows;
    return blank;
}
}  // namespace detail

inline void dotMatrixText(juce::Graphics& g, const juce::String& text, juce::Rectangle<float> r, juce::Justification j, float h, juce::Colour c, bool ghost = true) {
    const auto up = upper(text);
    const float pitch = h / 7.f, dot = pitch * 0.78f;
    const float w = (float)up.length() * 6.f * pitch - pitch;
    float x = j.testFlags(juce::Justification::right) ? r.getRight() - w : j.testFlags(juce::Justification::horizontallyCentred) ? r.getCentreX() - w * 0.5f : r.getX();
    const float y = r.getCentreY() - h * 0.5f;
    juce::Path lit, off;
    for (auto ch : up) {
        const auto* rows = detail::glyph5x7(ch);
        for (int row = 0; row < 7; ++row)
            for (int col = 0; col < 5; ++col) {
                const float px = x + (float)col * pitch, py = y + (float)row * pitch;
                ((rows[row] >> (4 - col)) & 1 ? lit : off).addEllipse(px + (pitch - dot) * 0.5f, py + (pitch - dot) * 0.5f, dot, dot);
            }
        x += 6.f * pitch;
    }
    if (ghost) { g.setColour(c.withAlpha(0.07f)); g.fillPath(off); }
    g.setColour(c.withAlpha(0.22f));
    g.strokePath(lit, juce::PathStrokeType(pitch * 0.55f));  // lueur
    g.setColour(c);
    g.fillPath(lit);
}

// Texte sur afficheur, dans la police du type d'écran (segments fantômes pour le 7 segments).
inline void screenText(juce::Graphics& g, const Palette& p, Display type, const juce::String& text, juce::Rectangle<float> r, juce::Justification j, float h) {
    const auto dc = displayColours(type, p);
    switch (type) {
        case Display::VFD: dotMatrixText(g, text, r, j, h, dc.ink); return;
        case Display::Seg7: {
            juce::String ghost;
            for (auto c : text) ghost += (c == '.' || c == ':') ? c : (juce::juce_wchar)'8';
            g.setFont(seg7Font(h));
            g.setColour(dc.ghost);
            g.drawText(ghost, r, j, false);
            g.setColour(dc.ink.withAlpha(0.25f));
            g.drawText(text, r.translated(0, 0.5f).expanded(0.5f, 0), j, false);
            g.setColour(dc.ink);
            g.drawText(text, r, j, false);
            return;
        }
        case Display::LCD:
            g.setFont(lcdFont(h));
            g.setColour(dc.ink.withAlpha(0.18f));
            g.drawText(text, r.translated(0.8f, 0.8f), j, false);  // ombre des cristaux sur le fond
            g.setColour(dc.ink);
            g.drawText(text, r, j, false);
            return;
        case Display::Amber:
            g.setFont(lcdFont(h));
            g.setColour(dc.ink.withAlpha(0.25f));
            g.drawText(text, r.expanded(0.6f), j, false);
            g.setColour(dc.ink);
            g.drawText(text, r, j, false);
            return;
        case Display::OLED:
        case Display::Classic:
            g.setFont(mono(h));
            g.setColour(dc.ink);
            g.drawText(text, r, j, false);
            return;
    }
}

// ============================================================ potards
// Chicken : bec en bakélite ; Skirt : jupe aluminium à graduations ; Cap : petit aluminium moleté à capuchon
// de couleur ; Fluted : noir cannelé ; Stepped : cranté, valeurs sérigraphiées ; Modern : ancien potard à anneau fin.
// Auto : Skirt pour les gros potards (r ≥ 40 px), Fluted sinon.
enum class KnobStyle { Auto, Chicken, Skirt, Cap, Fluted, Stepped, Modern };

namespace detail {
inline constexpr float knobA0 = juce::MathConstants<float>::pi * 0.75f, knobA1 = juce::MathConstants<float>::pi * 2.25f;
inline float knobAngle(float t) { return knobA0 + (knobA1 - knobA0) * t; }

// Ombre portée floue (dégradé radial décalé vers le bas) : rayon ≈ 0,35 r, décalage ≈ 0,25 r.
inline void knobShadow(juce::Graphics& g, float cx, float cy, float r, float strength = 1.f) {
    const float R = r * 1.35f, ox = cx + r * 0.06f, oy = cy + r * 0.25f;
    juce::ColourGradient sh(juce::Colours::black.withAlpha(0.55f * strength), ox, oy, juce::Colours::black.withAlpha(0.f), ox + R, oy, true);
    sh.addColour(0.62, juce::Colours::black.withAlpha(0.42f * strength));
    g.setGradientFill(sh);
    g.fillEllipse(ox - R, oy - R, R * 2, R * 2);
}

// Reflet conique d'un dessus métallique tourné : deux reflets opposés, fixes (ne tournent pas avec le bouton).
inline void conicMetal(juce::Graphics& g, float cx, float cy, float r, juce::Colour dark, juce::Colour light, int sectors = 72) {
    const float tp = juce::MathConstants<float>::twoPi;
    for (int i = 0; i < sectors; ++i) {
        const float a0 = tp * (float)i / (float)sectors, a1 = tp * (float)(i + 1) / (float)sectors;
        const float mid = (a0 + a1) * 0.5f - juce::MathConstants<float>::halfPi;  // angle écran du secteur
        const float k = 0.5f + 0.5f * std::cos(2.f * (mid - lightAngle));
        juce::Path s;
        s.addPieSegment(cx - r, cy - r, r * 2, r * 2, a0, a1 + 0.01f, 0.f);
        g.setColour(dark.interpolatedWith(light, k * k));
        g.fillPath(s);
    }
}

// Moletage : stries claires/sombres selon leur orientation par rapport à la lumière.
inline void knurl(juce::Graphics& g, float cx, float cy, float r0, float r1, int n, float rot, float width) {
    const float tp = juce::MathConstants<float>::twoPi;
    for (int i = 0; i < n; ++i) {
        const float a = rot + tp * (float)i / (float)n, c = std::cos(a), s = std::sin(a);
        const float f = facing(a);
        g.setColour(f > 0 ? juce::Colours::white.withAlpha(0.06f + 0.22f * f) : juce::Colours::black.withAlpha(0.18f - 0.32f * f));
        g.drawLine(cx + c * r0, cy + s * r0, cx + c * r1, cy + s * r1, width);
    }
}

// Échelle sérigraphiée sur la plaque : graduations (et chiffres 0–10 si la place le permet).
inline void silkScale(juce::Graphics& g, const Palette& p, float cx, float cy, float r, bool numbers, bool dots = false, bool minor = true) {
    const auto ink = p.silkInk();
    const float rIn = r * 1.16f, rOut = r * 1.27f;
    for (int i = 0; i <= 20; ++i) {
        const bool major = i % 2 == 0, big = i % 10 == 0;
        if (!major && (!minor || r < 16.f)) continue;
        const float a = knobAngle((float)i / 20.f), c = std::cos(a), s = std::sin(a);
        g.setColour(ink.withAlpha(major ? 0.85f : 0.45f));
        if (dots) {
            if (!major) continue;
            const float dr = juce::jmax(0.9f, r * (big ? 0.055f : 0.04f));
            g.fillEllipse(cx + c * (rIn + r * 0.04f) - dr, cy + s * (rIn + r * 0.04f) - dr, dr * 2, dr * 2);
        } else {
            g.drawLine(cx + c * (major ? rIn : rIn + r * 0.04f), cy + s * (major ? rIn : rIn + r * 0.04f), cx + c * rOut, cy + s * rOut,
                       juce::jmax(0.8f, r * (big ? 0.045f : major ? 0.032f : 0.022f)));
        }
    }
    if (numbers) {
        const float fh = juce::jmax(6.f, r * 0.17f);
        g.setFont(fnt(fh / kSansScale, true));
        g.setColour(ink.withAlpha(0.88f));
        for (int i = 0; i <= 10; ++i) {
            const float a = knobAngle((float)i / 10.f), c = std::cos(a), s = std::sin(a);
            const float rr = r * 1.39f;
            g.drawText(juce::String(i), juce::Rectangle<float>(fh * 1.6f, fh).withCentre({cx + c * rr, cy + s * rr}), juce::Justification::centred, false);
        }
    }
}

// Trait d'index peint (crème), du rayon a au rayon b.
inline void paintedIndex(juce::Graphics& g, float cx, float cy, float ang, float a, float b, float w, juce::Colour c = juce::Colour(0xFFF1EBDD)) {
    const float co = std::cos(ang), si = std::sin(ang);
    juce::Path l;
    l.startNewSubPath(cx + co * a, cy + si * a);
    l.lineTo(cx + co * b, cy + si * b);
    g.setColour(c);
    g.strokePath(l, juce::PathStrokeType(w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}
}  // namespace detail

// Potard d'un style donné. pos 0..1. Tout tient dans un cercle de 1,5 r (mêmes bornes que l'ancien potard).
inline void knob(juce::Graphics& g, const Palette& p, float cx, float cy, float r, float pos, KnobStyle style, bool scale = true) {
    using namespace detail;
    // Style automatique (ancienne signature) : les éditeurs impriment déjà leurs propres chiffres,
    // on ne sérigraphie donc que les 11 graduations principales.
    const bool autoStyle = style == KnobStyle::Auto;
    if (autoStyle) style = r >= 40.f ? KnobStyle::Skirt : KnobStyle::Fluted;
    pos = juce::jlimit(0.f, 1.f, pos);
    const float a = knobAngle(pos), tp = juce::MathConstants<float>::twoPi;
    const bool numbers = scale && r >= 30.f && !autoStyle;
    auto silk = [&](bool nums) { silkScale(g, p, cx, cy, r, nums, false, !autoStyle); };
    switch (style) {
        case KnobStyle::Modern: {
            if (scale) silk(false);
            juce::Path track, lit;
            const float q = juce::MathConstants<float>::halfPi;
            track.addCentredArc(cx, cy, r * 1.08f, r * 1.08f, 0, knobA0 + q, knobA1 + q, true);
            g.setColour(juce::Colours::black.withAlpha(0.45f));
            g.strokePath(track, juce::PathStrokeType(juce::jmax(1.2f, r * 0.05f)));
            lit.addCentredArc(cx, cy, r * 1.08f, r * 1.08f, 0, knobA0 + q, a + q, true);
            g.setColour(p.accent.withAlpha(0.85f));
            g.strokePath(lit, juce::PathStrokeType(juce::jmax(1.2f, r * 0.05f)));
            knobShadow(g, cx, cy, r * 0.95f);
            g.setGradientFill(juce::ColourGradient(juce::Colour(0xFF505055), cx - r, cy - r, juce::Colour(0xFF151517), cx + r, cy + r, false));
            g.fillEllipse(cx - r * 0.95f, cy - r * 0.95f, r * 1.9f, r * 1.9f);
            knurl(g, cx, cy, r * 0.82f, r * 0.94f, 40, a, juce::jmax(0.7f, r * 0.03f));
            g.setGradientFill(juce::ColourGradient(juce::Colour(0xFF46464B), cx - r * 0.5f, cy - r * 0.6f, juce::Colour(0xFF131315), cx + r * 0.5f, cy + r * 0.7f, false));
            g.fillEllipse(cx - r * 0.74f, cy - r * 0.74f, r * 1.48f, r * 1.48f);
            paintedIndex(g, cx, cy, a, r * 0.22f, r * 0.68f, juce::jmax(1.4f, r * 0.08f), p.accent);
            return;
        }
        case KnobStyle::Fluted: {
            if (scale) silk(numbers);
            knobShadow(g, cx, cy, r);
            const int nf = r < 14.f ? 12 : 16;
            juce::Path body;
            for (int i = 0; i <= 160; ++i) {
                const float t = tp * (float)i / 160.f;
                const float rr = r * (0.97f + 0.03f * std::cos((float)nf * (t - a)));
                const juce::Point<float> pt(cx + std::cos(t) * rr, cy + std::sin(t) * rr);
                if (i == 0) body.startNewSubPath(pt); else body.lineTo(pt);
            }
            body.closeSubPath();
            g.setGradientFill(juce::ColourGradient(juce::Colour(0xFF3C3C41), cx - r * 0.7f, cy - r * 0.7f, juce::Colour(0xFF0B0B0C), cx + r * 0.7f, cy + r * 0.8f, false));
            g.fillPath(body);
            {   // cannelures éclairées selon leur orientation
                juce::Graphics::ScopedSaveState st(g);
                g.reduceClipRegion(body);
                for (int i = 0; i < nf; ++i) {
                    const float t = a + tp * ((float)i + 0.5f) / (float)nf;  // creux entre deux côtes
                    const float f = facing(t);
                    juce::Path seg;
                    seg.addPieSegment(cx - r, cy - r, r * 2, r * 2, t + juce::MathConstants<float>::halfPi - tp / (float)nf * 0.32f,
                                      t + juce::MathConstants<float>::halfPi + tp / (float)nf * 0.32f, 0.78f);
                    g.setColour(f > 0 ? juce::Colours::white.withAlpha(0.05f + 0.16f * f) : juce::Colours::black.withAlpha(0.10f - 0.30f * f));
                    g.fillPath(seg);
                }
            }
            // dessus légèrement bombé
            const float tr = r * 0.76f;
            g.setGradientFill(juce::ColourGradient(juce::Colour(0xFF34343A), cx - tr * 0.5f, cy - tr * 0.6f, juce::Colour(0xFF101012), cx + tr * 0.6f, cy + tr * 0.8f, false));
            g.fillEllipse(cx - tr, cy - tr, tr * 2, tr * 2);
            juce::Path hl;
            hl.addCentredArc(cx, cy, tr - 0.6f, tr - 0.6f, 0, -1.9f, -0.2f, true);
            g.setColour(juce::Colours::white.withAlpha(0.20f));
            g.strokePath(hl, juce::PathStrokeType(juce::jmax(0.8f, r * 0.04f)));
            juce::Path lo;
            lo.addCentredArc(cx, cy, tr - 0.6f, tr - 0.6f, 0, 1.2f, 2.9f, true);
            g.setColour(juce::Colours::black.withAlpha(0.5f));
            g.strokePath(lo, juce::PathStrokeType(juce::jmax(0.8f, r * 0.04f)));
            paintedIndex(g, cx, cy, a, r * 0.30f, r * 0.92f, juce::jmax(1.5f, r * 0.085f));
            return;
        }
        case KnobStyle::Skirt: {
            if (scale) silk(numbers);
            knobShadow(g, cx, cy, r);
            // jupe aluminium tournée, graduations gravées qui tournent avec le bouton
            conicMetal(g, cx, cy, r, juce::Colour(0xFF66666C), juce::Colour(0xFFD9D9DD));
            g.setColour(juce::Colours::black.withAlpha(0.55f));
            g.drawEllipse(cx - r, cy - r, r * 2, r * 2, juce::jmax(0.8f, r * 0.025f));
            for (int i = 0; i < 31; ++i) {
                const float t = a + tp * (float)i / 31.f, c = std::cos(t), s = std::sin(t);
                const bool maj = i % 5 == 0;
                g.setColour(juce::Colours::black.withAlpha(maj ? 0.7f : 0.4f));
                g.drawLine(cx + c * r * (maj ? 0.80f : 0.85f), cy + s * r * (maj ? 0.80f : 0.85f), cx + c * r * 0.95f, cy + s * r * 0.95f, juce::jmax(0.6f, r * (maj ? 0.022f : 0.014f)));
            }
            // corps noir moleté
            const float br = r * 0.66f;
            g.setColour(juce::Colours::black.withAlpha(0.45f));
            g.fillEllipse(cx - br + r * 0.03f, cy - br + r * 0.08f, br * 2, br * 2);
            g.setGradientFill(juce::ColourGradient(juce::Colour(0xFF3A3A3F), cx - br * 0.7f, cy - br * 0.7f, juce::Colour(0xFF0A0A0B), cx + br * 0.7f, cy + br * 0.8f, false));
            g.fillEllipse(cx - br, cy - br, br * 2, br * 2);
            knurl(g, cx, cy, br * 0.80f, br * 0.99f, 48, a, juce::jmax(0.6f, r * 0.018f));
            const float tr = br * 0.80f;
            g.setGradientFill(juce::ColourGradient(juce::Colour(0xFF2F2F34), cx - tr * 0.5f, cy - tr * 0.6f, juce::Colour(0xFF0E0E10), cx + tr * 0.6f, cy + tr * 0.8f, false));
            g.fillEllipse(cx - tr, cy - tr, tr * 2, tr * 2);
            g.setGradientFill(juce::ColourGradient(juce::Colours::white.withAlpha(0.14f), cx - tr * 0.4f, cy - tr * 0.5f, juce::Colours::transparentWhite, cx, cy, true));
            g.fillEllipse(cx - tr, cy - tr, tr * 2, tr * 2);
            paintedIndex(g, cx, cy, a, r * 0.14f, br * 0.94f, juce::jmax(1.6f, r * 0.055f));
            paintedIndex(g, cx, cy, a, br * 1.04f, r * 0.97f, juce::jmax(1.6f, r * 0.05f), juce::Colour(0xFF141416));
            return;
        }
        case KnobStyle::Chicken: {
            if (scale) silk(numbers);
            const auto rot = juce::AffineTransform::rotation(a + juce::MathConstants<float>::halfPi).translated(cx, cy);
            juce::Path beak;  // pointe vers le haut en coordonnées locales
            beak.startNewSubPath(-r * 0.09f, -r * 1.0f);
            beak.quadraticTo(0, -r * 1.06f, r * 0.09f, -r * 1.0f);
            beak.lineTo(r * 0.36f, r * 0.12f);
            beak.cubicTo(r * 0.40f, r * 0.62f, -r * 0.40f, r * 0.62f, -r * 0.36f, r * 0.12f);
            beak.closeSubPath();
            // ombre portée floue
            for (int i = 3; i >= 1; --i) {
                g.setColour(juce::Colours::black.withAlpha(0.14f));
                g.fillPath(beak, rot.translated(r * 0.04f * (float)i, r * 0.07f * (float)i));
            }
            // embase ronde
            const float br = r * 0.58f;
            g.setGradientFill(juce::ColourGradient(juce::Colour(0xFF404045), cx - br * 0.7f, cy - br * 0.7f, juce::Colour(0xFF09090A), cx + br * 0.7f, cy + br * 0.8f, false));
            g.fillEllipse(cx - br, cy - br, br * 2, br * 2);
            g.setGradientFill(juce::ColourGradient(juce::Colour(0xFF46464C), cx - r * 0.6f, cy - r * 0.7f, juce::Colour(0xFF0C0C0D), cx + r * 0.6f, cy + r * 0.8f, false));
            g.fillPath(beak, rot);
            {   // arête du bec : reflet côté lumière
                juce::Graphics::ScopedSaveState st(g);
                g.reduceClipRegion(beak, rot);
                g.setGradientFill(juce::ColourGradient(juce::Colours::white.withAlpha(0.22f), cx - r * 0.8f, cy - r * 0.8f, juce::Colours::transparentWhite, cx + r * 0.1f, cy + r * 0.1f, false));
                g.fillRect(juce::Rectangle<float>(cx - r * 1.1f, cy - r * 1.1f, r * 2.2f, r * 2.2f));
            }
            g.setColour(juce::Colours::black.withAlpha(0.6f));
            g.strokePath(beak, juce::PathStrokeType(juce::jmax(0.6f, r * 0.02f)), rot);
            // trait peint le long du bec
            paintedIndex(g, cx, cy, a, r * 0.10f, r * 0.92f, juce::jmax(1.5f, r * 0.07f));
            // dôme central
            const float dr = r * 0.20f;
            g.setGradientFill(juce::ColourGradient(juce::Colour(0xFF505056), cx - dr * 0.5f, cy - dr * 0.5f, juce::Colour(0xFF101012), cx + dr, cy + dr, true));
            g.fillEllipse(cx - dr, cy - dr, dr * 2, dr * 2);
            return;
        }
        case KnobStyle::Cap: {
            if (scale) silk(numbers);
            const float br = r * 0.88f;
            knobShadow(g, cx, cy, br);
            conicMetal(g, cx, cy, br, juce::Colour(0xFF6E6E74), juce::Colour(0xFFE2E2E6), 48);
            knurl(g, cx, cy, br * 0.80f, br, 64, a, juce::jmax(0.6f, r * 0.02f));
            g.setColour(juce::Colours::black.withAlpha(0.5f));
            g.drawEllipse(cx - br, cy - br, br * 2, br * 2, juce::jmax(0.6f, r * 0.02f));
            // capuchon plastique coloré
            const float cr = br * 0.70f;
            const auto cc = p.accent.withMultipliedSaturation(0.85f).withMultipliedBrightness(0.82f);
            g.setColour(juce::Colours::black.withAlpha(0.4f));
            g.fillEllipse(cx - cr + r * 0.02f, cy - cr + r * 0.06f, cr * 2, cr * 2);
            g.setGradientFill(juce::ColourGradient(cc.brighter(0.5f), cx - cr * 0.5f, cy - cr * 0.6f, cc.darker(0.7f), cx + cr * 0.7f, cy + cr * 0.8f, true));
            g.fillEllipse(cx - cr, cy - cr, cr * 2, cr * 2);
            g.setColour(juce::Colours::white.withAlpha(0.30f));
            g.fillEllipse(cx - cr * 0.62f, cy - cr * 0.70f, cr * 0.70f, cr * 0.38f);
            paintedIndex(g, cx, cy, a, cr * 0.18f, cr * 0.92f, juce::jmax(1.4f, r * 0.08f), juce::Colours::white.withAlpha(0.92f));
            paintedIndex(g, cx, cy, a, br * 0.82f, br * 0.98f, juce::jmax(1.2f, r * 0.05f), juce::Colour(0xFF141416));
            return;
        }
        case KnobStyle::Stepped: {
            if (scale) {
                silkScale(g, p, cx, cy, r, false, true);
                if (r >= 22.f) {  // valeurs sérigraphiées à chaque cran pair
                    const float fh = juce::jmax(6.f, r * 0.17f);
                    g.setFont(fnt(fh / kSansScale, true));
                    g.setColour(p.silkInk().withAlpha(0.88f));
                    for (int i = 0; i <= 10; i += 2) {
                        const float t = knobAngle((float)i / 10.f);
                        g.drawText(juce::String(i), juce::Rectangle<float>(fh * 1.6f, fh).withCentre({cx + std::cos(t) * r * 1.39f, cy + std::sin(t) * r * 1.39f}), juce::Justification::centred, false);
                    }
                }
            }
            knobShadow(g, cx, cy, r);
            // corps noir à pans légers, dessus aluminium tourné, trait noir
            juce::Path body;
            body.addPolygon({cx, cy}, 12, r, a);
            g.setGradientFill(juce::ColourGradient(juce::Colour(0xFF3E3E43), cx - r * 0.7f, cy - r * 0.7f, juce::Colour(0xFF09090A), cx + r * 0.7f, cy + r * 0.8f, false));
            g.fillPath(body);
            for (int i = 0; i < 12; ++i) {
                const float t = a + tp * ((float)i + 0.5f) / 12.f;
                const float f = facing(t);
                juce::Path seg;
                seg.addPieSegment(cx - r, cy - r, r * 2, r * 2, t + juce::MathConstants<float>::halfPi - 0.22f, t + juce::MathConstants<float>::halfPi + 0.22f, 0.8f);
                g.setColour(f > 0 ? juce::Colours::white.withAlpha(0.14f * f) : juce::Colours::black.withAlpha(-0.25f * f));
                g.fillPath(seg);
            }
            const float tr = r * 0.74f;
            conicMetal(g, cx, cy, tr, juce::Colour(0xFF7A7A80), juce::Colour(0xFFF0F0F3));
            g.setColour(juce::Colours::black.withAlpha(0.45f));
            g.drawEllipse(cx - tr, cy - tr, tr * 2, tr * 2, juce::jmax(0.6f, r * 0.02f));
            paintedIndex(g, cx, cy, a, r * 0.12f, tr * 0.95f, juce::jmax(1.5f, r * 0.07f), juce::Colour(0xFF151517));
            paintedIndex(g, cx, cy, a, tr * 1.03f, r * 0.97f, juce::jmax(1.4f, r * 0.06f));
            return;
        }
        case KnobStyle::Auto: break;
    }
}

// Ancienne signature : style automatique (jupe pour les gros potards, cannelé sinon), échelle sérigraphiée.
inline void knob(juce::Graphics& g, const Palette& p, float cx, float cy, float r, float pos, bool scale = true) { knob(g, p, cx, cy, r, pos, KnobStyle::Auto, scale); }

// ============================================================ vumètre
// Vumètre à aiguille. vu : lecture en dB VU (sortie de VuBallistics::vu()). Échelle réelle −20..+3,
// position proportionnelle à la tension (0 VU à 71 %), rétroéclairage ambre, vitre bombée, LED de crête.
inline void vuMeter(juce::Graphics& g, const Palette& p, juce::Rectangle<float> r, float vu, bool peak = false, juce::Colour lamp = juce::Colour(0xFFFFB04A)) {
    juce::ignoreUnused(p);
    const float u = juce::jmin(r.getWidth() / 200.f, r.getHeight() / 120.f);
    // cadre vissé
    g.setColour(juce::Colours::black.withAlpha(0.55f));
    g.fillRoundedRectangle(r.translated(0, 2.f * u), 6.f * u);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xFF3A3A3F), r.getX(), r.getY(), juce::Colour(0xFF101012), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle(r, 6.f * u);
    g.setColour(juce::Colours::white.withAlpha(0.14f));
    g.drawRoundedRectangle(r.reduced(0.5f), 6.f * u, 1.f);
    for (auto pt : {r.getTopLeft() + juce::Point<float>(7, 7) * u, r.getTopRight() + juce::Point<float>(-7, 7) * u, r.getBottomLeft() + juce::Point<float>(7, -7) * u,
                    r.getBottomRight() + juce::Point<float>(-7, -7) * u})
        screw(g, pt.x, pt.y, 2.6f * u, pt.x * 0.1f);
    auto face = r.reduced(14.f * u, 12.f * u);
    // cadran crème, rétroéclairé par le bas
    g.setColour(juce::Colour(0xFFEDE1C2));
    g.fillRoundedRectangle(face, 3.f * u);
    juce::ColourGradient glow(lamp.withAlpha(0.55f), face.getCentreX(), face.getBottom() + face.getHeight() * 0.15f, lamp.withAlpha(0.f), face.getCentreX(), face.getY() - face.getHeight() * 0.1f, true);
    g.setGradientFill(glow);
    g.fillRoundedRectangle(face, 3.f * u);
    g.setGradientFill(juce::ColourGradient(juce::Colours::black.withAlpha(0.25f), face.getX(), face.getY(), juce::Colours::transparentBlack, face.getX() + face.getWidth() * 0.18f, face.getY(), false));
    g.fillRoundedRectangle(face, 3.f * u);
    // géométrie de l'arc : pivot sous le cadran
    const juce::Point<float> pv(face.getCentreX(), face.getBottom() + face.getHeight() * 0.42f);
    const float R = (pv.y - face.getY()) * 0.80f;
    const float span = juce::jlimit(0.55f, 0.85f, std::asin(juce::jmin(0.98f, face.getWidth() * 0.37f / R)));
    auto angOf = [&](double vuDb) { return -span + 2.f * span * (float)VuBallistics::needleFraction(vuDb); };
    auto at = [&](float ang, float rad) { return juce::Point<float>(pv.x + std::sin(ang) * rad, pv.y - std::cos(ang) * rad); };
    const auto ink = juce::Colour(0xFF1A1714), red = juce::Colour(0xFFC0281E);
    {   // arc principal et zone rouge
        juce::Path arc, redArc;
        arc.addCentredArc(pv.x, pv.y, R, R, 0, angOf(-20), angOf(0), true);
        g.setColour(ink);
        g.strokePath(arc, juce::PathStrokeType(1.2f * u));
        redArc.addCentredArc(pv.x, pv.y, R + 2.2f * u, R + 2.2f * u, 0, angOf(0), angOf(3), true);
        g.setColour(red);
        g.strokePath(redArc, juce::PathStrokeType(4.4f * u));
    }
    g.setFont(fnt(9.5f * u, true));
    for (double v : {-20.0, -10.0, -7.0, -5.0, -3.0, -2.0, -1.0, 0.0, 1.0, 2.0, 3.0}) {
        const float an = angOf(v);
        const bool hot = v > 0;
        g.setColour(hot || v == 0 ? red : ink);
        g.drawLine(juce::Line<float>(at(an, R), at(an, R + 7.f * u)), 1.3f * u);
        const auto lp = at(an, R + 14.f * u);
        const juce::String t = v > 0 ? "+" + juce::String((int)v) : juce::String((int)std::abs(v));
        g.drawText(t, juce::Rectangle<float>(24.f * u, 11.f * u).withCentre(lp), juce::Justification::centred, false);
    }
    for (double v : {-15.0, -8.5, -6.0, -4.0, -2.5, -1.5, -0.5, 0.5, 1.5, 2.5}) {  // graduations intermédiaires
        const float an = angOf(v);
        g.setColour((v > 0 ? red : ink).withAlpha(0.7f));
        g.drawLine(juce::Line<float>(at(an, R), at(an, R + 4.f * u)), 0.9f * u);
    }
    // échelle en pourcentage sous l'arc
    g.setFont(fnt(6.5f * u));
    g.setColour(ink.withAlpha(0.65f));
    for (int pc : {0, 50, 100}) {
        const float an = -span + 2.f * span * (float)pc / 100.f / std::pow(10.f, 3.f / 20.f);
        g.drawLine(juce::Line<float>(at(an, R - 1.f * u), at(an, R - 4.f * u)), 0.8f * u);
        g.drawText(juce::String(pc), juce::Rectangle<float>(20.f * u, 8.f * u).withCentre(at(an, R - 10.f * u)), juce::Justification::centred, false);
    }
    g.setColour(ink);
    g.setFont(fnt(15.f * u, true));
    g.drawText("VU", juce::Rectangle<float>(40.f * u, 16.f * u).withCentre({face.getCentreX(), face.getY() + face.getHeight() * 0.60f}), juce::Justification::centred, false);
    g.setFont(fnt(6.5f * u));
    g.setColour(ink.withAlpha(0.55f));
    g.drawText("dreamaws", juce::Rectangle<float>(60.f * u, 9.f * u).withPosition(face.getX() + 6.f * u, face.getBottom() - 12.f * u), juce::Justification::centredLeft, false);
    // aiguille noire avec contrepoids, ombre portée sur le cadran
    {
        juce::Graphics::ScopedSaveState st(g);
        g.reduceClipRegion(face.toNearestInt());
        const float an = angOf(juce::jlimit(-40.f, 4.f, vu));
        const auto tip = at(an, R + 9.f * u), tail = at(an, R * 0.18f);
        g.setColour(juce::Colours::black.withAlpha(0.18f));
        g.drawLine(juce::Line<float>(tail.translated(2.f * u, 3.f * u), tip.translated(2.f * u, 3.f * u)), 1.6f * u);
        g.setColour(ink);
        g.drawLine(juce::Line<float>(tail, tip), 1.3f * u);
        g.drawLine(juce::Line<float>(tail, at(an, R * 0.55f)), 2.2f * u);
        // capot du pivot (bas du cadran)
        const auto shroud = face.withTop(face.getBottom() - face.getHeight() * 0.16f);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xFF2A2A2E), shroud.getX(), shroud.getY(), juce::Colour(0xFF0C0C0E), shroud.getX(), shroud.getBottom(), false));
        g.fillRect(shroud);
        g.setColour(juce::Colours::white.withAlpha(0.15f));
        g.fillRect(shroud.getX(), shroud.getY(), shroud.getWidth(), 1.f);

    }
    // vitre bombée
    g.setGradientFill(juce::ColourGradient(juce::Colours::white.withAlpha(0.22f), face.getX(), face.getY(), juce::Colours::transparentWhite, face.getX(), face.getY() + face.getHeight() * 0.45f, false));
    juce::Path glass;
    glass.addRoundedRectangle(face.getX(), face.getY(), face.getWidth(), face.getHeight() * 0.45f, 3.f * u);
    g.fillPath(glass);
    g.setColour(juce::Colours::black.withAlpha(0.6f));
    g.drawRoundedRectangle(face, 3.f * u, 1.2f * u);
    led(g, face.getRight() - 9.f * u, face.getY() + 9.f * u, 2.8f * u, peak ? 1.f : 0.f, juce::Colour(0xFFFF3B30), LedBezel::Black);
}

// ============================================================ interrupteurs
// Interrupteur à levier métal, deux positions ; gravure au-dessus (on) et au-dessous (off).
inline void leverSwitch(juce::Graphics& g, const Palette& p, juce::Rectangle<float> r, bool on, const juce::String& onLabel = "ON", const juce::String& offLabel = "OFF") {
    const float s = juce::jmin(r.getWidth() / 40.f, r.getHeight() / 70.f);
    const juce::Point<float> c = r.getCentre();
    if (onLabel.isNotEmpty()) engraved(g, p, onLabel, juce::Rectangle<float>(r.getWidth(), 10.f * s).withCentre({c.x, c.y - 28.f * s}), juce::Justification::centred, 7.5f * s, 1.f * s, true);
    if (offLabel.isNotEmpty()) engraved(g, p, offLabel, juce::Rectangle<float>(r.getWidth(), 10.f * s).withCentre({c.x, c.y + 28.f * s}), juce::Justification::centred, 7.5f * s, 1.f * s, true);
    // écrou six pans et bague
    juce::Path nut;
    nut.addPolygon(c, 6, 11.f * s, 0.f);
    g.setColour(juce::Colours::black.withAlpha(0.45f));
    g.fillPath(nut, juce::AffineTransform::translation(0.8f * s, 1.8f * s));
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xFFE0E0E4), c.x - 8 * s, c.y - 9 * s, juce::Colour(0xFF505056), c.x + 8 * s, c.y + 10 * s, false));
    g.fillPath(nut);
    g.setColour(juce::Colours::black.withAlpha(0.45f));
    g.strokePath(nut, juce::PathStrokeType(0.7f * s));
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xFF9A9AA0), c.x - 6 * s, c.y - 6 * s, juce::Colour(0xFF2A2A2E), c.x + 6 * s, c.y + 6 * s, true));
    g.fillEllipse(c.x - 6.5f * s, c.y - 6.5f * s, 13.f * s, 13.f * s);
    g.setColour(juce::Colour(0xFF0C0C0D));
    g.fillEllipse(c.x - 3.6f * s, c.y - 3.6f * s, 7.2f * s, 7.2f * s);
    // levier en perspective : vers le haut (on) ou vers le bas (off)
    const float dir = on ? -1.f : 1.f, len = 22.f * s;
    const juce::Point<float> tip(c.x, c.y + dir * len);
    g.setColour(juce::Colours::black.withAlpha(0.35f));  // ombre portée vers le bas-droite
    g.drawLine(c.x + 2.f * s, c.y + 2.5f * s, tip.x + 3.f * s, tip.y + 4.f * s, 5.f * s);
    juce::Path bat;
    bat.startNewSubPath(c.x - 2.2f * s, c.y);
    bat.lineTo(tip.x - 3.4f * s, tip.y);
    bat.lineTo(tip.x + 3.4f * s, tip.y);
    bat.lineTo(c.x + 2.2f * s, c.y);
    bat.closeSubPath();
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xFF5E5E64), c.x - 4 * s, c.y, juce::Colour(0xFF1E1E22), c.x + 4 * s, c.y, false));
    g.fillPath(bat);
    g.setColour(juce::Colours::white.withAlpha(0.65f));
    g.drawLine(c.x - 1.2f * s, c.y, tip.x - 1.8f * s, tip.y, 0.9f * s);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xFFF4F4F6), tip.x - 2 * s, tip.y - 2 * s, juce::Colour(0xFF606066), tip.x + 3 * s, tip.y + 3 * s, true));
    g.fillEllipse(tip.x - 3.6f * s, tip.y - 3.6f * s, 7.2f * s, 7.2f * s);
}

// Bouton poussoir carré en plastique avec LED intégrée (touches de boîte à rythmes, LEARN/ADAPT…).
inline void pushButton(juce::Graphics& g, const Palette& p, juce::Rectangle<float> r, bool on, bool down, juce::Colour ledColour, const juce::String& text = {}) {
    const float rad = juce::jmin(r.getWidth(), r.getHeight()) * 0.14f;
    // découpe dans la plaque
    g.setColour(juce::Colours::black.withAlpha(0.7f));
    g.fillRoundedRectangle(r.expanded(1.5f), rad + 1.5f);
    g.setColour(juce::Colours::white.withAlpha(0.12f));
    g.drawRoundedRectangle(r.expanded(1.5f).translated(0, 0.8f), rad + 1.5f, 0.8f);
    auto cap = r.reduced(1.f).translated(0, down ? 1.f : 0.f);
    if (!down) {
        g.setColour(juce::Colours::black.withAlpha(0.5f));
        g.fillRoundedRectangle(cap.translated(0, 1.8f), rad);
    }
    const auto top = on ? juce::Colour(0xFF4A4A50) : juce::Colour(0xFF404046), bot = juce::Colour(0xFF161619);
    g.setGradientFill(juce::ColourGradient(top, cap.getX(), cap.getY(), bot, cap.getX(), cap.getBottom(), false));
    g.fillRoundedRectangle(cap, rad);
    g.setColour(juce::Colours::white.withAlpha(0.16f));
    g.drawLine(cap.getX() + rad, cap.getY() + 0.7f, cap.getRight() - rad, cap.getY() + 0.7f, 1.f);
    g.setColour(juce::Colours::black.withAlpha(0.6f));
    g.drawRoundedRectangle(cap, rad, 0.8f);
    // fenêtre de LED en haut du capuchon
    auto win = cap.withHeight(juce::jmax(2.f, cap.getHeight() * 0.10f)).reduced(cap.getWidth() * 0.22f, 0).translated(0, cap.getHeight() * 0.12f);
    const auto lens = on ? ledColour : ledColour.withMultipliedBrightness(0.28f).withMultipliedSaturation(0.8f);
    if (on) {
        g.setColour(ledColour.withAlpha(0.30f));
        g.fillRoundedRectangle(win.expanded(3.f), 3.f);
    }
    g.setGradientFill(juce::ColourGradient(lens.brighter(on ? 0.6f : 0.2f), win.getX(), win.getY(), lens.darker(0.3f), win.getX(), win.getBottom(), false));
    g.fillRoundedRectangle(win, win.getHeight() * 0.5f);
    if (text.isNotEmpty()) {
        g.setColour(p.silkInk().withAlpha(on ? 0.98f : 0.8f));
        tracked(g, text, cap.withTrimmedTop(cap.getHeight() * 0.25f), juce::Justification::centred, 1.2f, fnt(juce::jmin(11.f, cap.getHeight() * 0.30f), true));
    }
}

// ============================================================ LookAndFeel commun
// Propriétés de composant reconnues : slider.getProperties().set("kitKnob", (int)KnobStyle::Chicken) ;
// bouton à bascule : getProperties().set("kitButton", "lever") ou "push".
class Look : public juce::LookAndFeel_V4 {
public:
    explicit Look(Palette pal) : P(pal) {
        setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xF0141416));
        setColour(juce::PopupMenu::textColourId, P.ink);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, P.accent.withAlpha(0.28f));
        setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour(juce::TooltipWindow::backgroundColourId, juce::Colour(0xFF0C0C0E));
        setColour(juce::TooltipWindow::textColourId, P.ink);
        setColour(juce::ComboBox::textColourId, P.screenInk);
    }
    const Palette& pal() const { return P; }
    KnobStyle knobStyle = KnobStyle::Auto;  // style par défaut des potards de ce Look

    void drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider& s) override {
        const float r = juce::jmin((float)w, (float)h) * 0.5f / 1.5f;
        auto st = knobStyle;
        if (const auto* v = s.getProperties().getVarPointer("kitKnob")) st = (KnobStyle)(int)*v;
        knob(g, P, (float)x + (float)w * 0.5f, (float)y + (float)h * 0.5f, r, pos, st);
    }

    void drawLinearSlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider::SliderStyle st, juce::Slider&) override {
        const bool vert = st == juce::Slider::LinearVertical;
        auto b = juce::Rectangle<float>((float)x, (float)y, (float)w, (float)h);
        auto slot = [&](juce::Rectangle<float> sr) {  // fente découpée dans la plaque
            g.setColour(juce::Colours::black.withAlpha(0.85f));
            g.fillRoundedRectangle(sr, juce::jmin(sr.getWidth(), sr.getHeight()) * 0.5f);
            g.setColour(juce::Colours::white.withAlpha(0.14f));
            g.drawRoundedRectangle(sr.translated(0.5f, 0.8f), juce::jmin(sr.getWidth(), sr.getHeight()) * 0.5f, 0.7f);
        };
        auto capFace = [&](juce::Rectangle<float> cap, bool v) {  // capuchon de fader de console
            g.setColour(juce::Colours::black.withAlpha(0.45f));
            g.fillRoundedRectangle(cap.translated(1.f, 2.5f).expanded(0.5f), 3.f);
            g.setGradientFill(juce::ColourGradient(juce::Colour(0xFF4E4E54), cap.getX(), cap.getY(), juce::Colour(0xFF121214), v ? cap.getX() : cap.getRight(), v ? cap.getBottom() : cap.getY(), false));
            g.fillRoundedRectangle(cap, 2.5f);
            for (int i = -2; i <= 2; ++i) {  // rainures
                if (i == 0) continue;
                const float o = (float)i * (v ? cap.getHeight() : cap.getWidth()) * 0.14f;
                g.setColour(juce::Colours::black.withAlpha(0.55f));
                if (v) g.fillRect(cap.getX() + 2, cap.getCentreY() + o, cap.getWidth() - 4, 1.f);
                else g.fillRect(cap.getCentreX() + o, cap.getY() + 2, 1.f, cap.getHeight() - 4);
                g.setColour(juce::Colours::white.withAlpha(0.10f));
                if (v) g.fillRect(cap.getX() + 2, cap.getCentreY() + o + 1.f, cap.getWidth() - 4, 0.8f);
                else g.fillRect(cap.getCentreX() + o + 1.f, cap.getY() + 2, 0.8f, cap.getHeight() - 4);
            }
            g.setColour(juce::Colour(0xFFF1EBDD));
            if (v) g.fillRect(cap.getX() + 1.5f, cap.getCentreY() - 0.7f, cap.getWidth() - 3.f, 1.4f);
            else g.fillRect(cap.getCentreX() - 0.7f, cap.getY() + 1.5f, 1.4f, cap.getHeight() - 3.f);
            g.setColour(juce::Colours::white.withAlpha(0.18f));
            g.drawRoundedRectangle(cap.reduced(0.5f), 2.5f, 0.7f);
        };
        if (vert) {
            const float cx = b.getCentreX();
            slot({cx - 2.5f, b.getY() + 4, 5, b.getHeight() - 8});
            g.setColour(P.accent.withAlpha(0.55f));
            g.fillRoundedRectangle(cx - 1.f, pos, 2.f, juce::jmax(0.f, b.getBottom() - 4 - pos), 1.f);
            for (int i = 0; i <= 10; ++i) {
                const float ty = b.getBottom() - 8 - (b.getHeight() - 16) * (float)i / 10.f;
                g.setColour(P.silkInk().withAlpha(i % 5 == 0 ? 0.7f : 0.35f));
                g.drawLine(cx + 9, ty, cx + (i % 5 == 0 ? 15.f : 12.f), ty, 1.f);
            }
            capFace(juce::Rectangle<float>(cx - 11, pos - 7, 22, 14), true);
        } else {
            const float cy = b.getCentreY();
            slot({b.getX() + 4, cy - 2.5f, b.getWidth() - 8, 5});
            g.setColour(P.accent.withAlpha(0.55f));
            g.fillRoundedRectangle(b.getX() + 4, cy - 1.f, juce::jmax(0.f, pos - b.getX() - 4), 2.f, 1.f);
            capFace(juce::Rectangle<float>(pos - 7, cy - 11, 14, 22), false);
        }
    }

    void drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour&, bool hover, bool down) override {
        auto r = b.getLocalBounds().toFloat().reduced(1.f);
        const bool on = b.getToggleState();
        const float rad = juce::jmin(4.f, r.getHeight() * 0.2f);
        // découpe + ombre portée
        g.setColour(juce::Colours::black.withAlpha(0.6f));
        g.fillRoundedRectangle(r.translated(0, down ? 0.5f : 1.5f), rad);
        if (down) r = r.translated(0, 1.f);
        auto top = juce::Colour(0xFF4C4C52), bot = juce::Colour(0xFF1A1A1D);
        if (on) { top = top.interpolatedWith(P.accent, 0.10f); }
        if (hover) top = top.brighter(0.10f);
        if (down) std::swap(top, bot);
        g.setGradientFill(juce::ColourGradient(top, r.getX(), r.getY(), bot, r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle(r, rad);
        g.setColour(juce::Colours::white.withAlpha(0.16f));
        g.drawLine(r.getX() + rad, r.getY() + 0.6f, r.getRight() - rad, r.getY() + 0.6f, 1.f);
        g.setColour(juce::Colours::black.withAlpha(0.55f));
        g.drawRoundedRectangle(r, rad, 0.8f);
        if (on) {  // filet lumineux intégré au capuchon (comme une touche rétroéclairée)
            auto lit = r.reduced(r.getWidth() * 0.12f, 0).withHeight(1.6f).translated(0, 2.f);
            g.setColour(P.accent.withAlpha(0.35f));
            g.fillRoundedRectangle(lit.expanded(1.5f, 1.2f), 1.5f);
            g.setColour(P.accent.brighter(0.4f));
            g.fillRoundedRectangle(lit, 0.8f);
        }
    }
    void drawButtonText(juce::Graphics& g, juce::TextButton& b, bool, bool down) override {
        auto r = b.getLocalBounds().toFloat();
        if (down) r = r.translated(0, 1);
        g.setColour(b.getToggleState() ? juce::Colours::white : P.silkInk().withAlpha(0.9f));
        tracked(g, b.getButtonText(), r, juce::Justification::centred, 1.4f, fnt(juce::jmin(12.f, r.getHeight() * 0.38f), true));
    }
    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& b, bool, bool down) override {
        auto r = b.getLocalBounds().toFloat();
        const auto kind = b.getProperties()["kitButton"].toString();
        if (kind == "lever") { leverSwitch(g, P, r, b.getToggleState(), b.getButtonText(), {}); return; }
        if (kind == "push") { pushButton(g, P, r, b.getToggleState(), down, P.accent, b.getButtonText()); return; }
        led(g, r.getX() + 9.f, r.getCentreY(), 3.6f, b.getToggleState(), P.accent);
        g.setColour(P.silkInk().withAlpha(b.getToggleState() ? 0.95f : 0.65f));
        tracked(g, b.getButtonText(), r.withTrimmedLeft(22.f), juce::Justification::centredLeft, 1.2f, fnt(10.5f, true));
    }

    // Liste déroulante = petit afficheur encastré + triangle gravé blanc.
    void drawComboBox(juce::Graphics& g, int w, int h, bool down, int, int, int, int, juce::ComboBox&) override {
        auto r = juce::Rectangle<float>(0, 0, (float)w, (float)h).reduced(0.5f);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xFF060607), r.getX(), r.getY(), juce::Colour(0xFF141417), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle(r, 3.f);
        g.setGradientFill(juce::ColourGradient(juce::Colours::black.withAlpha(0.6f), r.getX(), r.getY(), juce::Colours::white.withAlpha(0.14f), r.getX(), r.getBottom(), false));
        g.drawRoundedRectangle(r, 3.f, 1.f);
        g.setColour(juce::Colours::white.withAlpha(0.025f));
        g.fillRoundedRectangle(r.withHeight(r.getHeight() * 0.45f).reduced(1.f), 2.f);
        juce::Path tri;
        const float cx = (float)w - 11.f, cy = (float)h * 0.5f + (down ? 0.5f : 0.f), t = juce::jlimit(2.5f, 4.f, (float)h * 0.16f);
        tri.addTriangle(cx - t, cy - t * 0.5f, cx + t, cy - t * 0.5f, cx, cy + t * 0.75f);
        g.setColour(juce::Colours::black.withAlpha(0.6f));
        g.fillPath(tri, juce::AffineTransform::translation(0, -0.8f));
        g.setColour(juce::Colours::white.withAlpha(0.78f));
        g.fillPath(tri);
    }
    juce::Font getComboBoxFont(juce::ComboBox&) override { return lcdFont(12.5f); }  // police d'écran (Share Tech Mono)
    juce::Font getPopupMenuFont() override { return fnt(13.f); }
    void positionComboBoxText(juce::ComboBox& cb, juce::Label& l) override {
        l.setBounds(8, 1, cb.getWidth() - 26, cb.getHeight() - 2);
        l.setFont(getComboBoxFont(cb));
    }

    // Poignée de redimensionnement gravée, cantonnée au tout dernier coin (la vis reste visible).
    void drawCornerResizer(juce::Graphics& g, int w, int h, bool over, bool dragging) override {
        resizeGrip(g, juce::Rectangle<float>(0, 0, (float)w, (float)h), over || dragging);
    }

private:
    Palette P;
};

}  // namespace kit
