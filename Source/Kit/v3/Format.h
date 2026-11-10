// Kit v3 : formatage unique des valeurs affichées (virgule française, vrai signe moins, espace avant l'unité).
// Additif : n'inclut que JUCE ; utilisable par tous les éditeurs, un à un.
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>

namespace kit::v3::fmt {

inline const juce::String& minus() { static const juce::String m = juce::String::charToString((juce::juce_wchar)0x2212); return m; }

// Nombre avec `decimals` décimales, virgule, signe moins typographique, jamais « −0,0 ».
inline juce::String num(double v, int decimals = 1) {
    if (!std::isfinite(v)) return "--";
    const double q = std::pow(10.0, decimals);
    double r = std::round(v * q) / q;
    if (r == 0.0) r = 0.0;
    juce::String s = juce::String(std::fabs(r), decimals).replace(".", ",");
    return r < 0.0 ? minus() + s : s;
}
// Toujours signé : « +1,5 », « −0,09 », « 0 ».
inline juce::String signedNum(double v, int decimals = 1) {
    const auto s = num(v, decimals);
    const double q = std::pow(10.0, decimals);
    return (std::round(v * q) > 0.0) ? "+" + s : s;
}
inline juce::String withUnit(const juce::String& n, const juce::String& unit) { return unit.isEmpty() ? n : n + juce::String::charToString((juce::juce_wchar)0x00A0) + unit; }
inline juce::String db(double v, int d = 1) { return withUnit(num(v, d), "dB"); }
inline juce::String pct(double v01) { return withUnit(num(v01 * 100.0, 0), "%"); }
inline juce::String ms(double v) { return withUnit(num(v, v < 10.0 ? 1 : 0), "ms"); }
inline juce::String hz(double v) { return std::fabs(v) >= 1000.0 ? withUnit(num(v / 1000.0, v >= 10000.0 ? 1 : 2), "kHz") : withUnit(num(v, v < 100.0 ? 1 : 0), "Hz"); }
inline juce::String semis(double v, int d = 1) { return withUnit(signedNum(v, d), "dt"); }
inline juce::String cents(double v) { return withUnit(signedNum(v, 0), "ct"); }

// Texte d'un paramètre JUCE remis aux normes : « 2.5 » -> « 2,5 », « -3 » -> « −3 », « 50% » -> « 50 % », « 0dB » -> « 0 dB ».
inline juce::String tidy(juce::String s) {
    s = s.trim().replace(".", ",");
    if (s.startsWithChar('-')) s = minus() + s.substring(1);
    for (int i = 1; i < s.length(); ++i) {   // espace insécable entre le nombre et l'unité collée
        const auto c = s[i], p = s[i - 1];
        if (juce::CharacterFunctions::isDigit(p) && !juce::CharacterFunctions::isDigit(c) && c != ',' && c != ' ' && c != 0x00A0) {
            s = s.substring(0, i) + juce::String::charToString((juce::juce_wchar)0x00A0) + s.substring(i);
            break;
        }
    }
    return s.replace(" ", juce::String::charToString((juce::juce_wchar)0x00A0));
}
inline juce::String param(const juce::RangedAudioParameter* p) { return p == nullptr ? juce::String() : tidy(p->getCurrentValueAsText()); }

}  // namespace kit::v3::fmt
