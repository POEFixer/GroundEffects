#include "Scanner.h"

#include "AtomicFile.h"
#include "TextMatch.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace GroundEffects {

namespace {

constexpr float  kFallbackRadius = 40.0f;
constexpr float  kMinRadius = 4.0f;
constexpr float  kMaxRadius = 500.0f;
constexpr size_t kMaxDiscoveryRows = 200;
constexpr int    kMaxReadAttempts = 5;   // a ground-effect path whose component is not readable yet

std::string WideToUtf8(const std::wstring& ws) {
    if (ws.empty()) return {};
    const int needed = ::WideCharToMultiByte(CP_UTF8, 0, ws.data(), static_cast<int>(ws.size()),
                                             nullptr, 0, nullptr, nullptr);
    if (needed <= 0) return {};
    std::string out(static_cast<size_t>(needed), '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, ws.data(), static_cast<int>(ws.size()), out.data(), needed, nullptr, nullptr);
    return out;
}

} // namespace

void Scanner::Reset() {
    m_Known.clear();
    m_Pending.clear();
    m_Markers.clear();
    m_Building.clear();
    m_Cursor = 0;
    m_Active = false;
    m_ForceScan = true;
    m_HaveArea = false;
    m_Stats = ScanStats{};
}

void Scanner::OnRulesChanged() {
    m_RulesDirty = true;
}

void Scanner::PrepareRules(const Settings& s) {
    m_RuleNeedles.clear();
    m_RuleEnabled.clear();
    m_RuleNeedles.reserve(s.rules.size());
    for (const Rule& r : s.rules) {
        m_RuleNeedles.push_back(GroundText::NormalizeNeedle(r.match));
        m_RuleEnabled.push_back(r.enabled);
    }
    ++m_RuleRevision;
    m_RulesDirty = false;

    // Rematch what is on screen now so edits show at once; the forced pass then
    // adds effects that only the new rules match.
    m_Markers.erase(std::remove_if(m_Markers.begin(), m_Markers.end(), [this](Marker& m) {
        auto it = m_Known.find(m.entityId);
        if (it == m_Known.end()) return true;
        it->second.rule = MatchRule(it->second);
        it->second.ruleRevision = m_RuleRevision;
        m.rule = it->second.rule;
        return m.rule < 0;
    }), m_Markers.end());
    m_Building.clear();
    m_Pending.clear();
    m_Active = false;
    m_ForceScan = true;
}

int Scanner::MatchRule(const Known& k) const {
    for (size_t i = 0; i < m_RuleNeedles.size(); ++i) {
        if (!m_RuleEnabled[i]) continue;
        if (GroundText::ContainsLower(k.searchLower, m_RuleNeedles[i])) return static_cast<int>(i);
    }
    return -1;
}

void Scanner::Tick(const PluginSDK::Context& ctx, const Settings& s, Clock::time_point now) {
    if (m_RulesDirty || m_RuleNeedles.size() != s.rules.size()) PrepareRules(s);

    if (!m_Active && (m_ForceScan || now >= m_NextScan)) BeginPass(ctx, s, now);

    if (m_Active) {
        const auto start = Clock::now();
        const auto budget = std::chrono::milliseconds(std::clamp(s.scanBudgetMs, Limits::kScanBudgetMin,
                                                                 Limits::kScanBudgetMax));
        int work = 0;
        while (m_Cursor < m_Pending.size()) {
            const int readsBefore = m_PassReads;
            Process(ctx, s, m_Pending[m_Cursor++]);
            work += (m_PassReads != readsBefore) ? 8 : 1;   // a component read costs far more than a lookup
            if (work >= 32) {
                work = 0;
                if (Clock::now() - start >= budget) break;
            }
        }
        m_PassMs += std::chrono::duration<double, std::milli>(Clock::now() - start).count();
        ++m_PassFrames;
        if (m_Cursor >= m_Pending.size()) FinishPass();
    }

    if (m_DiscoveryLog && !m_LogBuffer.empty() && now >= m_NextLogFlush) {
        FlushDiscoveryLog();
        m_NextLogFlush = now + std::chrono::seconds(2);
    }
}

void Scanner::BeginPass(const PluginSDK::Context& ctx, const Settings& s, Clock::time_point now) {
    m_ForceScan = false;
    m_NextScan = now + std::chrono::milliseconds(std::clamp(s.scanIntervalMs, Limits::kScanIntervalMin,
                                                            Limits::kScanIntervalMax));

    // Towns and hideouts: a cheap flag read instead of a full entity list.
    if (s.skipTownHideout && ctx.Game.IsTownOrHideout()) {
        m_Markers.clear();
        return;
    }
    bool anyRule = false;
    for (size_t i = 0; i < m_RuleNeedles.size(); ++i) anyRule |= (m_RuleEnabled[i] && !m_RuleNeedles[i].empty());
    if (!anyRule && !m_Discovery) {
        m_Markers.clear();
        return;
    }

    // Ground effects carry Render but no Life / Chest / NPC / item component, so the
    // host types them Renderable; the filter drops every other entity before the
    // SDK copies its strings.
    const auto snapStart = Clock::now();
    PluginSDK::Snapshot snap = ctx.Game.GetSnapshot(PluginSDK::EntityType::Renderable);
    const double snapMs = std::chrono::duration<double, std::milli>(Clock::now() - snapStart).count();
    if (!snap.IsAttached || snap.State != PluginSDK::GameState::InGame) {
        Reset();
        m_ForceScan = false;
        return;
    }
    if (!m_HaveArea || snap.AreaChangeCounter != m_AreaCounter) {
        m_Known.clear();
        m_Markers.clear();
        m_AreaCounter = snap.AreaChangeCounter;
        m_HaveArea = true;
    }
    if (s.skipTownHideout && (snap.IsTown || snap.IsHideout)) {
        m_Markers.clear();
        return;
    }

    m_Pending = std::move(snap.Entities);
    m_Cursor = 0;
    m_Active = true;
    ++m_Pass;
    m_Building.clear();
    m_Building.reserve(std::min<size_t>(static_cast<size_t>(s.maxMarkers), 128));
    m_PassMs = snapMs;
    m_PassFrames = 0;
    m_PassReads = 0;
}

void Scanner::ReadKnown(const PluginSDK::Context& ctx, const PluginSDK::Entity& e, bool prefilter, Known& k) {
    k.address = e.Address;
    const bool pathLooksRight = GroundText::LooksLikeGroundEffectPath(e.Path);
    if (prefilter && !pathLooksRight) return;   // not a ground effect, never read

    const PluginSDK::GroundEffect ge = ctx.Components.ReadGroundEffect(e.Address);
    ++m_PassReads;
    if (!ge.Valid) {
        // A just-spawned effect may not be readable yet: try again on later passes.
        if (pathLooksRight && ++k.readAttempts < kMaxReadAttempts) k.retry = true;
        return;
    }
    k.retry = false;
    k.groundEffect = true;
    k.typeId = ge.TypeId;
    k.endEffect = ge.EndEffect;
    k.buffVisual1 = ge.BuffVisual1;
    k.buffVisual2 = ge.BuffVisual2;
    k.aoFile = ge.AoFile;
    k.path = WideToUtf8(e.Path);

    float radius = ge.Radius;
    if (!(radius > 0.0f) && e.Components.HasRender()) {
        const PluginSDK::Render r = ctx.Components.ReadRender(e.Components.Render);
        if (r.Valid) radius = std::max(std::fabs(r.ModelBoundsX), std::fabs(r.ModelBoundsY));
    }
    if (!(radius > 0.0f)) radius = kFallbackRadius;
    k.radius = std::clamp(radius, kMinRadius, kMaxRadius);

    if (e.Components.HasPositioned()) {
        const PluginSDK::Positioned p = ctx.Components.ReadPositioned(e.Components.Positioned);
        k.positionedValid = p.Valid;
        k.friendly = p.Valid && p.IsFriendly;
    }

    std::string search;
    search.reserve(k.typeId.size() + k.endEffect.size() + k.buffVisual1.size() + k.buffVisual2.size() +
                   k.aoFile.size() + k.path.size() + e.TgtPath.size() + 8);
    const std::string* parts[] = { &k.typeId, &k.endEffect, &k.buffVisual1, &k.buffVisual2, &k.aoFile, &k.path,
                                   &e.TgtPath };
    for (const std::string* part : parts) {
        search += *part;
        search += ' ';
    }
    k.searchLower = GroundText::ToLower(search);
}

void Scanner::Process(const PluginSDK::Context& ctx, const Settings& s, const PluginSDK::Entity& e) {
    if (!e.IsValid || e.Address == 0) return;
    auto [it, inserted] = m_Known.try_emplace(e.Id);
    Known& k = it->second;
    if (!inserted && k.address != e.Address) k = Known{};     // the id now names another entity
    if (k.address == 0 || k.retry) {
        k.retry = false;
        ReadKnown(ctx, e, s.usePathPrefilter, k);
    }
    k.pass = m_Pass;
    if (!k.groundEffect) return;

    if (m_Discovery && !k.discovered) AddDiscovery(k);
    if (s.onlyHostile && (!k.positionedValid || k.friendly)) return;

    if (k.ruleRevision != m_RuleRevision) {
        k.rule = MatchRule(k);
        k.ruleRevision = m_RuleRevision;
    }
    if (k.rule < 0 || static_cast<int>(m_Building.size()) >= s.maxMarkers) return;

    Marker m;
    m.entityId = e.Id;
    m.x = e.WorldX;
    m.y = e.WorldY;
    m.z = e.WorldZ;
    m.radius = k.radius;
    m.rule = k.rule;
    m.typeId = k.typeId;
    m_Building.push_back(std::move(m));
}

void Scanner::FinishPass() {
    m_Markers.swap(m_Building);
    m_Building.clear();
    m_Active = false;

    m_Stats.lastPassEntities = m_Pending.size();
    m_Stats.lastPassFrames = m_PassFrames;
    m_Stats.lastPassMs = m_PassMs;
    m_Stats.componentReads = m_PassReads;
    m_Pending.clear();

    // Entities missing from this pass are gone: forget them.
    for (auto it = m_Known.begin(); it != m_Known.end();) {
        if (it->second.pass != m_Pass) it = m_Known.erase(it);
        else ++it;
    }
    m_Stats.knownEntities = m_Known.size();
}

void Scanner::AddDiscovery(Known& k) {
    // Remember even when the table is full so the same entity is not re-checked.
    k.discovered = true;
    std::string key = k.typeId + '|' + k.buffVisual1 + '|' + k.buffVisual2 + '|' + k.aoFile + '|' +
                      (k.friendly ? '1' : '0');
    for (DiscoveryRow& r : m_Rows) {
        if (r.key == key) { ++r.seen; return; }
    }
    if (m_Rows.size() >= kMaxDiscoveryRows) return;

    DiscoveryRow row;
    row.typeId = k.typeId;
    row.endEffect = k.endEffect;
    row.buffVisual1 = k.buffVisual1;
    row.buffVisual2 = k.buffVisual2;
    row.aoFile = k.aoFile;
    row.path = k.path;
    row.radius = k.radius;
    row.positionedValid = k.positionedValid;
    row.friendly = k.friendly;
    row.seen = 1;
    row.key = std::move(key);

    if (m_DiscoveryLog) {
        char radius[32];
        std::snprintf(radius, sizeof(radius), "%.1f", row.radius);
        m_LogBuffer += "TypeId=" + row.typeId + " | Radius=" + radius + " | Friendly=" +
                       (row.positionedValid ? (row.friendly ? "yes" : "no") : "unknown") +
                       " | EndEffect=" + row.endEffect + " | BuffVisual1=" + row.buffVisual1 +
                       " | BuffVisual2=" + row.buffVisual2 + " | AoFile=" + row.aoFile +
                       " | Path=" + row.path + "\r\n";
    }
    m_Rows.push_back(std::move(row));
}

void Scanner::SetDiscovery(bool enabled) {
    if (enabled && !m_Discovery) {
        for (auto& [id, k] : m_Known) k.discovered = false;
        m_ForceScan = true;
    }
    m_Discovery = enabled;
}

void Scanner::SetDiscoveryLog(bool enabled, const std::filesystem::path& logFile) {
    if (!enabled) FlushDiscoveryLog();
    m_LogFile = logFile;
    m_DiscoveryLog = enabled;
    if (enabled) {
        // Log what is already in the table once, then every new kind as it appears.
        for (const DiscoveryRow& row : m_Rows) {
            m_LogBuffer += "TypeId=" + row.typeId + " | EndEffect=" + row.endEffect + " | BuffVisual1=" +
                           row.buffVisual1 + " | BuffVisual2=" + row.buffVisual2 + " | AoFile=" + row.aoFile +
                           " | Path=" + row.path + "\r\n";
        }
    }
}

void Scanner::ClearDiscovery() {
    m_Rows.clear();
    for (auto& [id, k] : m_Known) k.discovered = false;
}

void Scanner::FlushDiscoveryLog() {
    if (m_LogBuffer.empty()) return;
    if (m_DiscoveryLog && !m_LogFile.empty()) AtomicFile::AppendAll(m_LogFile, m_LogBuffer);
    m_LogBuffer.clear();
}

} // namespace GroundEffects
