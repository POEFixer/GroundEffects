// Finds ground effects and matches them to the colour rules.
//
// Work is spread over frames: a pass takes one entity list (only Renderable
// entities, filtered before the SDK copies any strings), then walks it within a
// per-frame time budget. The GroundEffect component of an entity never changes,
// so it is read once per entity and remembered until the area changes; later
// passes only refresh positions. A finished pass replaces the marker list in one
// step, so markers never flicker while a pass is in progress.
#pragma once

#include "../../../POEFixer/plugin_sdk/PluginSDK.h"
#include "Settings.h"

#include <chrono>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace GroundEffects {

// A matched effect in world space; projected to the screen every frame.
struct Marker {
    uint32_t    entityId = 0;
    float       x = 0.0f, y = 0.0f, z = 0.0f;
    float       radius = 40.0f;
    int         rule = -1;       // index into Settings::rules at the rules revision of the pass
    std::string typeId;          // label fallback when the rule has no label
};

// One distinct kind of ground effect seen this session (Diagnostics).
struct DiscoveryRow {
    std::string typeId, endEffect, buffVisual1, buffVisual2, aoFile, path;
    float       radius = 0.0f;
    bool        positionedValid = false;
    bool        friendly = false;
    int         seen = 0;        // entities of this kind seen
    std::string key;             // dedupe key
};

struct ScanStats {
    size_t knownEntities = 0;    // remembered entity reads this area
    size_t lastPassEntities = 0;
    int    lastPassFrames = 0;
    double lastPassMs = 0.0;     // scan time summed over the frames of the pass
    int    componentReads = 0;   // GroundEffect reads in the last pass
};

class Scanner {
public:
    using Clock = std::chrono::steady_clock;

    // Drop everything (area change, leaving the game, disable).
    void Reset();
    // Rules were added, removed, enabled/disabled or their match text changed:
    // rematch and rebuild at once.
    void OnRulesChanged();
    // Start a new pass on the next Tick even if the interval has not elapsed.
    void RequestRescan() { m_ForceScan = true; }

    void Tick(const PluginSDK::Context& ctx, const Settings& s, Clock::time_point now);

    const std::vector<Marker>& Markers() const { return m_Markers; }
    const ScanStats& Stats() const { return m_Stats; }

    // Discovery (session only).
    void SetDiscovery(bool enabled);
    bool Discovery() const { return m_Discovery; }
    void SetDiscoveryLog(bool enabled, const std::filesystem::path& logFile);
    bool DiscoveryLog() const { return m_DiscoveryLog; }
    const std::vector<DiscoveryRow>& DiscoveryRows() const { return m_Rows; }
    void ClearDiscovery();
    void FlushDiscoveryLog();

private:
    struct Known {
        uintptr_t   address = 0;
        uint32_t    pass = 0;            // last pass that saw this entity
        bool        groundEffect = false;
        bool        positionedValid = false;
        bool        friendly = false;
        bool        discovered = false;
        bool        retry = false;       // component not readable yet: read again next pass
        int         readAttempts = 0;
        float       radius = 0.0f;
        std::string typeId, endEffect, buffVisual1, buffVisual2, aoFile, path;
        std::string searchLower;
        uint32_t    ruleRevision = 0;
        int         rule = -1;
    };

    void BeginPass(const PluginSDK::Context& ctx, const Settings& s, Clock::time_point now);
    void Process(const PluginSDK::Context& ctx, const Settings& s, const PluginSDK::Entity& e);
    void ReadKnown(const PluginSDK::Context& ctx, const PluginSDK::Entity& e, bool prefilter, Known& k);
    void PrepareRules(const Settings& s);
    int  MatchRule(const Known& k) const;
    void AddDiscovery(Known& k);
    void FinishPass();

    std::unordered_map<uint32_t, Known> m_Known;
    std::vector<PluginSDK::Entity> m_Pending;
    size_t m_Cursor = 0;
    bool   m_Active = false;
    bool   m_ForceScan = true;
    uint32_t m_Pass = 0;
    uint64_t m_AreaCounter = 0;
    bool   m_HaveArea = false;
    Clock::time_point m_NextScan{};

    std::vector<Marker> m_Markers;
    std::vector<Marker> m_Building;

    // Rules prepared for matching (lower-cased, trimmed match text).
    std::vector<std::string> m_RuleNeedles;
    std::vector<bool>        m_RuleEnabled;
    uint32_t m_RuleRevision = 1;
    bool     m_RulesDirty = true;

    ScanStats m_Stats;
    double    m_PassMs = 0.0;
    int       m_PassFrames = 0;
    int       m_PassReads = 0;

    bool m_Discovery = false;
    bool m_DiscoveryLog = false;
    std::filesystem::path m_LogFile;
    std::vector<DiscoveryRow> m_Rows;
    std::string m_LogBuffer;
    Clock::time_point m_NextLogFlush{};
};

} // namespace GroundEffects
