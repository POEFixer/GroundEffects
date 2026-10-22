// Pure text helpers (no SDK, no ImGui): ASCII case folding and the substring
// tests the rule matcher and the path pre-filter use. Header-only so the
// standalone test in tests/ compiles the same code.
#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace GroundText {

inline char LowerAscii(char c) {
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

inline wchar_t LowerAscii(wchar_t c) {
    return (c >= L'A' && c <= L'Z') ? static_cast<wchar_t>(c - L'A' + L'a') : c;
}

// ASCII lower case; other bytes (UTF-8 continuation bytes included) unchanged.
inline std::string ToLower(std::string_view s) {
    std::string out(s);
    for (char& c : out) c = LowerAscii(c);
    return out;
}

// Lower case with leading and trailing ASCII white space removed.
inline std::string NormalizeNeedle(std::string_view s) {
    size_t b = 0, e = s.size();
    while (b < e && (s[b] == ' ' || s[b] == '\t' || s[b] == '\r' || s[b] == '\n')) ++b;
    while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t' || s[e - 1] == '\r' || s[e - 1] == '\n')) --e;
    return ToLower(s.substr(b, e - b));
}

// Both arguments already lower case; an empty needle never matches.
inline bool ContainsLower(std::string_view lowerHaystack, std::string_view lowerNeedle) {
    return !lowerNeedle.empty() && lowerHaystack.find(lowerNeedle) != std::string_view::npos;
}

// Case-insensitive ASCII search in a wide string without allocating.
inline bool ContainsWideNoCase(std::wstring_view haystack, std::wstring_view needle) {
    if (needle.empty() || haystack.size() < needle.size()) return false;
    for (size_t i = 0; i + needle.size() <= haystack.size(); ++i) {
        size_t j = 0;
        while (j < needle.size() && LowerAscii(haystack[i + j]) == LowerAscii(needle[j])) ++j;
        if (j == needle.size()) return true;
    }
    return false;
}

// The metadata paths ground effects use (VisibleServerGroundEffect and friends).
inline bool LooksLikeGroundEffectPath(std::wstring_view path) {
    return ContainsWideNoCase(path, L"ground_effect") ||
           ContainsWideNoCase(path, L"groundeffect");
}

} // namespace GroundText
