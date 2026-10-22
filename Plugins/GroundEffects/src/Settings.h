// Ground Effects settings: defaults, ranges, JSON persistence and the one-time
// migration of the upstream INI file. No SDK or ImGui dependency.
#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace GroundEffects {

struct Color4 {
    float r = 1.0f, g = 1.0f, b = 1.0f, a = 1.0f;
};

// One colour rule: matched case-insensitively against the effect's type id,
// visuals, end behaviour and metadata paths. The first enabled match wins.
struct Rule {
    bool        enabled = true;
    std::string label;
    std::string match;
    Color4      color{ 1.0f, 0.15f, 0.10f, 0.45f };
};

namespace Limits {
constexpr int   kScanIntervalMin = 100,  kScanIntervalMax = 1000;
constexpr int   kScanBudgetMin = 1,      kScanBudgetMax = 10;
constexpr int   kMaxMarkersMin = 8,      kMaxMarkersMax = 256;
constexpr int   kSegmentsMin = 8,        kSegmentsMax = 48;
constexpr float kOutlineMin = 1.0f,      kOutlineMax = 8.0f;
constexpr float kHeightMin = -150.0f,    kHeightMax = 150.0f;
constexpr float kOpacityMin = 0.1f,      kOpacityMax = 1.0f;
constexpr size_t kMaxRules = 64;
constexpr size_t kMaxLabelLen = 95;
constexpr size_t kMaxMatchLen = 191;
}

struct Settings {
    // When to show
    bool onlyHostile = true;          // Positioned says the effect is not friendly
    bool skipTownHideout = true;
    bool onlyWhenForeground = true;   // only while the game window has focus
    bool hideWhenMenuVisible = false; // hide while the POEFixer menu is open

    // Display
    bool  fillCircles = true;
    bool  showLabels = false;
    float outlineThickness = 2.0f;
    float opacity = 1.0f;             // multiplies every rule colour's alpha
    int   circleSegments = 24;
    float heightOffset = 0.0f;        // world units added to the effect height

    // Performance
    int  scanIntervalMs = 150;
    int  scanBudgetMs = 2;            // scan work allowed per frame
    int  maxMarkers = 80;
    bool usePathPrefilter = true;     // skip entities whose path is not a ground effect

    std::vector<Rule> rules;

    void Clamp();
};

std::vector<Rule> DefaultRules();
Settings DefaultSettings();

// Parsers return false when the text is not usable; `out` then keeps what it had.
bool ParseJson(const std::string& text, Settings& out);
bool ParseLegacyIni(const std::string& text, Settings& out);
std::string ToJson(const Settings& s);

// Unreadable: a settings file exists but could not be read (held open by another
// program, access denied, oversized). `out` holds the defaults; the caller must
// not write them over the file (see KeepUnreadableCopy).
enum class LoadSource { Json, LegacyIni, Defaults, CorruptJson, Unreadable };

// `pluginDir` is the folder the DLL lives in; files go to <pluginDir>/config/.
LoadSource Load(const std::filesystem::path& pluginDir, Settings& out);
bool Save(const std::filesystem::path& pluginDir, const std::string& json);
// Best effort: copies an unreadable GroundEffects.json to GroundEffects.json.unreadable
// before the first save replaces it.
void KeepUnreadableCopy(const std::filesystem::path& pluginDir);

std::filesystem::path ConfigDir(const std::filesystem::path& pluginDir);

} // namespace GroundEffects
