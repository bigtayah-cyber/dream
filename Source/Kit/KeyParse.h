// Tonalité copiée / collée entre les plugins (texte du presse-papiers). Pur C++ : « Fm », « F mineur », « F# minor », « Bb », « La♭ majeur »… 
// Lettres A à G (jamais Do Ré Mi), dièse (# ♯) ou bémol (b ♭), puis mineur (m, min, minor, mineur) ou majeur (rien, maj, major, majeur).
// Le reste du texte (« +23 ct », « 8A ») est ignoré.
#pragma once
#include <cctype>
#include <string>

namespace kit { namespace keyparse {

struct Key { int pc = 0; bool minor = false; };

inline bool parse(const std::string& s, Key& out) {
    size_t i = 0; while (i < s.size() && std::isspace((unsigned char)s[i])) ++i;
    if (i >= s.size()) return false;
    const char c = (char)std::toupper((unsigned char)s[i]);
    static const int base[7] = {9, 11, 0, 2, 4, 5, 7};   // A B C D E F G
    if (c < 'A' || c > 'G') return false;
    int pc = base[c - 'A']; ++i;
    if (i < s.size()) {
        if (s[i] == '#') { ++pc; ++i; }
        else if (s[i] == 'b' && (i + 1 >= s.size() || !std::isalpha((unsigned char)s[i + 1]) || s[i + 1] == 'm' || s[i + 1] == 'M')) { --pc; ++i; }   // « Bb », « Bbm », mais pas « Bm »
        else if (s.compare(i, 3, "\xE2\x99\xAF") == 0) { ++pc; i += 3; }   // ♯
        else if (s.compare(i, 3, "\xE2\x99\xAD") == 0) { --pc; i += 3; }   // ♭
    }
    std::string rest; for (size_t j = i; j < s.size(); ++j) rest += (char)std::tolower((unsigned char)s[j]);
    size_t k = 0; while (k < rest.size() && rest[k] == ' ') ++k; rest = rest.substr(k);
    bool minor = false;
    if (rest.compare(0, 3, "maj") == 0) minor = false;
    else if (rest.compare(0, 3, "min") == 0 || (!rest.empty() && rest[0] == 'm')) minor = true;
    else if (!rest.empty() && std::isalpha((unsigned char)rest[0])) return false;   // « Fa », « Do » : pas des lettres de note
    out.pc = ((pc % 12) + 12) % 12; out.minor = minor; return true;
}

inline std::string format(int pc, bool minor) {
    static const char* const n[12] = {"C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};
    return std::string(n[((pc % 12) + 12) % 12]) + (minor ? "m" : "");
}

}}  // namespace kit::keyparse
