#include "Settings.h"

#include "AtomicFile.h"
#include "TextMatch.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <charconv>
#include <cstdio>
#include <string_view>
#include <unordered_map>

namespace GroundEffects {

using nlohmann::json;

namespace {

constexpr const char* kJsonName = "GroundEffects.json";
constexpr const char* kLegacyIniName = "GroundEffects.ini";

float Clamp01(float v) { return std::clamp(v, 0.0f, 1.0f); }

std::string Truncate(std::string s, size_t maxLen) {
    if (s.size() <= maxLen) return s;
    size_t cut = maxLen;
    // never split a UTF-8 sequence
    while (cut > 0 && (static_cast<unsigned char>(s[cut]) & 0xC0) == 0x80) --cut;
    s.resize(cut);
    return s;
}

Rule MakeRule(const char* label, const char* match, Color4 c) {
    Rule r;
    r.label = label;
    r.match = match;
    r.color = c;
    return r;
}

// ---- JSON field readers that never throw ----------------------------------
bool ReadBool(const json& j, const char* key, bool& v) {
    auto it = j.find(key);
    if (it == j.end() || !it->is_boolean()) return false;
    v = it->get<bool>();
    return true;
}
void ReadInt(const json& j, const char* key, int& v) {
    auto it = j.find(key);
    if (it != j.end() && it->is_number()) v = static_cast<int>(it->get<double>());
}
void ReadFloat(const json& j, const char* key, float& v) {
    auto it = j.find(key);
    if (it != j.end() && it->is_number()) v = static_cast<float>(it->get<double>());
}
void ReadString(const json& j, const char* key, std::string& v) {
    auto it = j.find(key);
    if (it != j.end() && it->is_string()) v = it->get<std::string>();
}
void ReadColor(const json& j, const char* key, Color4& c) {
    auto it = j.find(key);
    if (it == j.end() || !it->is_array() || it->size() != 4) return;
    float v[4];
    for (size_t i = 0; i < 4; ++i) {
        if (!(*it)[i].is_number()) return;
        v[i] = Clamp01(static_cast<float>((*it)[i].get<double>()));
    }
    c = Color4{ v[0], v[1], v[2], v[3] };
}

// ---- legacy INI value parsers (locale-independent) -------------------------
using Kv = std::unordered_map<std::string, std::string>;

const std::string* Find(const Kv& kv, const std::string& key) {
    auto it = kv.find(key);
    return it == kv.end() ? nullptr : &it->second;
}
bool IniBool(const Kv& kv, const std::string& key, bool fallback) {
    const std::string* v = Find(kv, key);
    if (!v) return fallback;
    return *v == "1" || *v == "true" || *v == "True";
}
int IniInt(const Kv& kv, const std::string& key, int fallback) {
    const std::string* v = Find(kv, key);
    if (!v) return fallback;
    int out = fallback;
    auto r = std::from_chars(v->data(), v->data() + v->size(), out);
    return r.ec == std::errc() ? out : fallback;
}
float ParseFloatToken(std::string_view s, float fallback) {
    while (!s.empty() && s.front() == ' ') s.remove_prefix(1);
    float out = fallback;
    auto r = std::from_chars(s.data(), s.data() + s.size(), out);
    return r.ec == std::errc() ? out : fallback;
}
float IniFloat(const Kv& kv, const std::string& key, float fallback) {
    const std::string* v = Find(kv, key);
    return v ? ParseFloatToken(*v, fallback) : fallback;
}
bool IniColor(const std::string& s, Color4& c) {
    float v[4];
    size_t start = 0;
    for (int i = 0; i < 4; ++i) {
        const size_t comma = s.find(',', start);
        if (i < 3 && comma == std::string::npos) return false;
        const std::string_view tok(s.data() + start, (i < 3 ? comma : s.size()) - start);
        float f = -1.0f;
        f = ParseFloatToken(tok, -1.0f);
        if (f < 0.0f) return false;
        v[i] = Clamp01(f);
        start = comma + 1;
    }
    c = Color4{ v[0], v[1], v[2], v[3] };
    return true;
}

// Upstream dropped rules that targeted explosions / server effects, which this
// plugin cannot see; keep doing that so migrated configs stay clean.
bool IsLegacyExplosionRule(const Rule& r) {
    const std::string m = GroundText::ToLower(r.match);
    return m.find("explosion") != std::string::npos || m.find("ondeath") != std::string::npos ||
           m.find("servereffect") != std::string::npos || m.find("entityhazard") != std::string::npos ||
           m.find("volatile") != std::string::npos;
}

} // namespace

void Settings::Clamp() {
    scanIntervalMs = std::clamp(scanIntervalMs, Limits::kScanIntervalMin, Limits::kScanIntervalMax);
    scanBudgetMs = std::clamp(scanBudgetMs, Limits::kScanBudgetMin, Limits::kScanBudgetMax);
    maxMarkers = std::clamp(maxMarkers, Limits::kMaxMarkersMin, Limits::kMaxMarkersMax);
    circleSegments = std::clamp(circleSegments, Limits::kSegmentsMin, Limits::kSegmentsMax);
    if (!(outlineThickness == outlineThickness)) outlineThickness = 2.0f;  // NaN
    if (!(heightOffset == heightOffset)) heightOffset = 0.0f;
    if (!(opacity == opacity)) opacity = 1.0f;
    outlineThickness = std::clamp(outlineThickness, Limits::kOutlineMin, Limits::kOutlineMax);
    heightOffset = std::clamp(heightOffset, Limits::kHeightMin, Limits::kHeightMax);
    opacity = std::clamp(opacity, Limits::kOpacityMin, Limits::kOpacityMax);
    if (rules.size() > Limits::kMaxRules) rules.resize(Limits::kMaxRules);
    for (Rule& r : rules) {
        r.label = Truncate(std::move(r.label), Limits::kMaxLabelLen);
        r.match = Truncate(std::move(r.match), Limits::kMaxMatchLen);
        r.color = Color4{ Clamp01(r.color.r), Clamp01(r.color.g), Clamp01(r.color.b), Clamp01(r.color.a) };
    }
}

std::vector<Rule> DefaultRules() {
    return {
        MakeRule("Shocked Ground", "ShockedGround", Color4{ 0.45f, 0.65f, 1.00f, 0.38f }),
        MakeRule("Chilled Ground", "ChilledGround", Color4{ 0.35f, 0.80f, 1.00f, 0.34f }),
        MakeRule("Ignited Ground", "IgnitedGround", Color4{ 1.00f, 0.22f, 0.08f, 0.40f }),
        MakeRule("Caustic Cloud",  "CausticCloud",  Color4{ 0.35f, 1.00f, 0.20f, 0.34f }),
    };
}

Settings DefaultSettings() {
    Settings s;
    s.rules = DefaultRules();
    return s;
}

bool ParseJson(const std::string& text, Settings& out) {
    json j = json::parse(text, nullptr, /*allow_exceptions=*/false, /*ignore_comments=*/true);
    if (j.is_discarded() || !j.is_object()) return false;

    Settings s = DefaultSettings();
    ReadBool(j, "onlyHostile", s.onlyHostile);
    ReadBool(j, "skipTownHideout", s.skipTownHideout);
    ReadBool(j, "onlyWhenForeground", s.onlyWhenForeground);
    ReadBool(j, "hideWhenMenuVisible", s.hideWhenMenuVisible);
    ReadBool(j, "fillCircles", s.fillCircles);
    ReadBool(j, "showLabels", s.showLabels);
    ReadFloat(j, "outlineThickness", s.outlineThickness);
    ReadFloat(j, "opacity", s.opacity);
    ReadInt(j, "circleSegments", s.circleSegments);
    ReadFloat(j, "heightOffset", s.heightOffset);
    ReadInt(j, "scanIntervalMs", s.scanIntervalMs);
    ReadInt(j, "scanBudgetMs", s.scanBudgetMs);
    ReadInt(j, "maxMarkers", s.maxMarkers);
    ReadBool(j, "usePathPrefilter", s.usePathPrefilter);

    auto rules = j.find("rules");
    if (rules != j.end() && rules->is_array()) {
        s.rules.clear();
        for (const json& jr : *rules) {
            if (!jr.is_object()) continue;
            Rule r;
            ReadBool(jr, "enabled", r.enabled);
            ReadString(jr, "label", r.label);
            ReadString(jr, "match", r.match);
            ReadColor(jr, "color", r.color);
            s.rules.push_back(std::move(r));
            if (s.rules.size() >= Limits::kMaxRules) break;
        }
    }
    s.Clamp();
    out = std::move(s);
    return true;
}

bool ParseLegacyIni(const std::string& text, Settings& out) {
    Kv kv;
    size_t pos = 0;
    while (pos < text.size()) {
        size_t eol = text.find('\n', pos);
        if (eol == std::string::npos) eol = text.size();
        std::string line = text.substr(pos, eol - pos);
        pos = eol + 1;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        kv[line.substr(0, eq)] = line.substr(eq + 1);
    }
    if (kv.empty()) return false;

    Settings s = DefaultSettings();
    s.onlyWhenForeground = IniBool(kv, "OnlyWhenForeground", s.onlyWhenForeground);
    s.skipTownHideout = IniBool(kv, "SkipTownHideout", s.skipTownHideout);
    s.onlyHostile = IniBool(kv, "OnlyHostileGroundEffects", s.onlyHostile);
    s.usePathPrefilter = IniBool(kv, "UsePathPrefilter", IniBool(kv, "UseEntityTypeFilter", s.usePathPrefilter));
    s.hideWhenMenuVisible = IniBool(kv, "HideWhenMenuVisible", s.hideWhenMenuVisible);
    s.fillCircles = IniBool(kv, "FillCircles", s.fillCircles);
    s.showLabels = IniBool(kv, "ShowLabels", s.showLabels);
    s.scanIntervalMs = IniInt(kv, "ScanIntervalMs", s.scanIntervalMs);
    // Upstream's per-scan budget (3-30 ms, default 8) becomes a per-frame budget.
    s.scanBudgetMs = IniInt(kv, "MaxScanTimeMs", s.scanBudgetMs);
    s.maxMarkers = IniInt(kv, "MaxMarkers", s.maxMarkers);
    s.circleSegments = IniInt(kv, "CircleSegments", s.circleSegments);
    s.outlineThickness = IniFloat(kv, "OutlineThickness", s.outlineThickness);
    s.heightOffset = IniFloat(kv, "ZOffset", s.heightOffset);

    const int count = std::clamp(IniInt(kv, "EffectCount", 0), 0, 256);
    std::vector<Rule> rules;
    for (int i = 0; i < count; ++i) {
        const std::string prefix = "Effect" + std::to_string(i) + ".";
        Rule r;
        r.enabled = IniBool(kv, prefix + "Enabled", true);
        const std::string* legacyName = Find(kv, prefix + "Name");
        const std::string* label = Find(kv, prefix + "Label");
        const std::string* match = Find(kv, prefix + "MatchText");
        r.label = label ? *label : (legacyName ? *legacyName : std::string());
        r.match = match ? *match : (legacyName ? *legacyName : r.label);
        if (const std::string* c = Find(kv, prefix + "Color")) IniColor(*c, r.color);
        if (!IsLegacyExplosionRule(r)) rules.push_back(std::move(r));
    }
    if (!rules.empty()) s.rules = std::move(rules);
    s.Clamp();
    out = std::move(s);
    return true;
}

std::string ToJson(const Settings& s) {
    json j;
    j["version"] = 1;
    j["onlyHostile"] = s.onlyHostile;
    j["skipTownHideout"] = s.skipTownHideout;
    j["onlyWhenForeground"] = s.onlyWhenForeground;
    j["hideWhenMenuVisible"] = s.hideWhenMenuVisible;
    j["fillCircles"] = s.fillCircles;
    j["showLabels"] = s.showLabels;
    j["outlineThickness"] = s.outlineThickness;
    j["opacity"] = s.opacity;
    j["circleSegments"] = s.circleSegments;
    j["heightOffset"] = s.heightOffset;
    j["scanIntervalMs"] = s.scanIntervalMs;
    j["scanBudgetMs"] = s.scanBudgetMs;
    j["maxMarkers"] = s.maxMarkers;
    j["usePathPrefilter"] = s.usePathPrefilter;
    json rules = json::array();
    for (const Rule& r : s.rules) {
        rules.push_back({
            { "enabled", r.enabled },
            { "label", r.label },
            { "match", r.match },
            { "color", { r.color.r, r.color.g, r.color.b, r.color.a } },
        });
    }
    j["rules"] = std::move(rules);
    return j.dump(2, ' ', false, json::error_handler_t::replace);
}

std::filesystem::path ConfigDir(const std::filesystem::path& pluginDir) {
    return pluginDir / L"config";
}

LoadSource Load(const std::filesystem::path& pluginDir, Settings& out) {
    out = DefaultSettings();
    if (pluginDir.empty()) return LoadSource::Defaults;
    const std::filesystem::path dir = ConfigDir(pluginDir);
    std::string text;
    const std::filesystem::path jsonPath = dir / kJsonName;
    if (AtomicFile::ReadAll(jsonPath, text)) {
        if (ParseJson(text, out)) return LoadSource::Json;
        // Keep the unreadable file for the user instead of overwriting it.
        std::filesystem::path aside = jsonPath;
        aside += L".corrupt";
        ::MoveFileExW(jsonPath.c_str(), aside.c_str(), MOVEFILE_REPLACE_EXISTING);
        return LoadSource::CorruptJson;
    }
    // Present but not readable (locked, denied, oversized): not a first start.
    if (AtomicFile::Present(jsonPath)) return LoadSource::Unreadable;
    const std::filesystem::path iniPath = dir / kLegacyIniName;
    if (AtomicFile::ReadAll(iniPath, text)) {
        if (ParseLegacyIni(text, out)) return LoadSource::LegacyIni;
    } else if (AtomicFile::Present(iniPath)) {
        return LoadSource::Unreadable;
    }
    return LoadSource::Defaults;
}

void KeepUnreadableCopy(const std::filesystem::path& pluginDir) {
    if (pluginDir.empty()) return;
    const std::filesystem::path jsonPath = ConfigDir(pluginDir) / kJsonName;
    std::filesystem::path aside = jsonPath;
    aside += L".unreadable";
    ::CopyFileW(jsonPath.c_str(), aside.c_str(), FALSE);
}

bool Save(const std::filesystem::path& pluginDir, const std::string& jsonText) {
    if (pluginDir.empty()) return false;
    return AtomicFile::WriteAll(ConfigDir(pluginDir) / kJsonName, jsonText);
}

} // namespace GroundEffects
