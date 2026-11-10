// Kit v3 : noms de notes et de tonalités, dièses ou bémols. Additif : n'inclut que JUCE.
// L'enharmonie ne change JAMAIS le son (même numéro MIDI, même classe de hauteur) : seulement le nom affiché.
// Les noms de notes sont en ASCII (« Db », « Eb », « Gb », « Ab », « Bb ») : les polices du kit n'ont pas ♯ ni ♭.
// Seul le sélecteur # / ♭ (voir accidentalLabels) emploie le glyphe ♭, comme Orbe Autotune.
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace kit::v3::notes {

inline const char* pcName(int pc, bool flat) {
    static const char* const sh[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    static const char* const fl[12] = {"C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"};
    pc = ((pc % 12) + 12) % 12;
    return flat ? fl[pc] : sh[pc];
}
// MIDI -> « Db4 » ; octave : 60 = C4 par défaut (octaveOfC0 = -1) ; passer -2 pour la convention 60 = C3.
inline juce::String name(int midi, bool flat, int octaveOfC0 = -1) { return juce::String(pcName(midi, flat)) + juce::String(midi / 12 + octaveOfC0); }
inline juce::String letter(int midi, bool flat) { return pcName(midi, flat); }
inline bool isBlack(int pc) { pc = ((pc % 12) + 12) % 12; return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10; }
inline juce::StringArray pcNames(bool flat) { juce::StringArray a; for (int i = 0; i < 12; ++i) a.add(pcName(i, flat)); return a; }
// Tonalité : « Db majeur », « Eb mineur ».
inline juce::String keyName(int pc, bool minor, bool flat) { return juce::String(pcName(pc, flat)) + (minor ? " mineur" : " majeur"); }
// Sélecteur d'écriture : mêmes libellés que celui d'Orbe Autotune (« # » / ♭, UTF-8 pour MSVC).
inline juce::StringArray accidentalLabels() { return juce::StringArray{"#", juce::String::fromUTF8("\xE2\x99\xAD")}; }
inline juce::String accidentalTip() { return juce::String::fromUTF8("\xC3\x89" "criture des notes : di\xC3\xA8ses (C#) ou b\xC3\xA9mols (Db). Seul le nom change, jamais le son."); }
// Paramètre « accid » (ajouté EN FIN de liste, défaut 0 = dièses : l'état d'avant se rouvre à l'identique). Choix, jamais booléen.
inline std::unique_ptr<juce::AudioParameterChoice> makeAccidParam() {
    return std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"accid", 1}, juce::String::fromUTF8("Alt\xC3\xA9rations"),
        juce::StringArray{juce::String::fromUTF8("Di\xC3\xA8ses (#)"), juce::String::fromUTF8("B\xC3\xA9mols (b)")}, 0);
}

// Lecture du paramètre « accid » d'un apvts (true = bémols).
inline bool flatPref(juce::AudioProcessorValueTreeState& a) { auto* p = a.getRawParameterValue("accid"); return p != nullptr && p->load() > 0.5f; }
// Renomme les 12 entrées d'un menu de tonalité (ids firstId .. firstId + 11 = C .. B) sans toucher à leur numéro : le paramètre reste le même.
inline void relabelKeyItems(juce::ComboBox& box, int firstId, bool flat) { for (int i = 0; i < 12; ++i) box.changeItemText(firstId + i, pcName(i, flat)); }

}  // namespace kit::v3::notes
