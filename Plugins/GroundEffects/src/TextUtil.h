// Text helpers that need no SDK (STL only), so the tests build them: a slider's display
// format from a translated unit text, and a byte cut that never splits a UTF-8 character.
#pragma once

#include <string>
#include <string_view>

namespace GroundEffects {

// The unit text ("{value} ms") with its FIRST {value} replaced by the number format ("%d").
// The number format is code; a '%' of the translation is doubled so it is never a conversion,
// and a later {value} stays literal text: ImGui passes one number, so a second conversion
// would read an argument that is not there.
inline std::string UnitFormat(const char* unitText, const char* numberFormat) {
    const std::string_view text = unitText ? unitText : "";
    const std::string_view mark = "{value}";
    std::string out;
    bool placed = false;
    for (size_t i = 0; i < text.size();) {
        if (!placed && text.compare(i, mark.size(), mark) == 0) {
            out += numberFormat ? numberFormat : "";
            placed = true;
            i += mark.size();
            continue;
        }
        if (text[i] == '%') out += '%';
        out += text[i++];
    }
    return out;
}

// The length of the longest prefix of `s` of at most maxBytes bytes that ends on a character
// boundary: a UTF-8 continuation byte is never the first byte cut off.
inline size_t Utf8CutLength(std::string_view s, size_t maxBytes) {
    if (s.size() <= maxBytes) return s.size();
    size_t cut = maxBytes;
    while (cut > 0 && (static_cast<unsigned char>(s[cut]) & 0xC0) == 0x80) --cut;
    return cut;
}

// `s` cut to at most maxBytes bytes on a character boundary.
inline void Utf8Truncate(std::string& s, size_t maxBytes) { s.resize(Utf8CutLength(s, maxBytes)); }

} // namespace GroundEffects
