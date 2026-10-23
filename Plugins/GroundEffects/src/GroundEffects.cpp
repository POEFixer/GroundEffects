// Ground Effects: colours dangerous ground effects (shocked, chilled, ignited
// ground, caustic clouds, ...) with world-space circles sized from the effect's
// own radius, using colour rules the player edits.
//
// Originally created by 1k4ru5g3 (https://github.com/1k4ru5g3/GroundEffects).
// Maintained by POEFixer since October 2026.

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "../../../POEFixer/plugin_sdk/PluginSDK.h"
#include "../../../POEFixer/imgui/imgui.h"
#include "../../../POEFixer/imgui/imgui_internal.h"   // ImTextInitClassifiers

#include "Overlay.h"
#include "Scanner.h"
#include "Settings.h"
#include "SettingsUi.h"

#include <chrono>
#include <exception>
#include <string>

// Plugin info: the host checks it before CreatePlugin; the publish tool reads it.
POEFIXER_PLUGIN_INFO(
    "GroundEffects",
    "Ground Effects",
    "1.0.3",
    "1k4ru5g3, POEFixer",
    "Draws coloured circles on dangerous ground effects (shocked, chilled, ignited ground, caustic clouds and your own rules) at their real size.")

namespace {

using namespace GroundEffects;

class GroundEffectsPlugin final : public PluginSDK::Plugin {
public:
    const char* GetName() const override { return "Ground Effects"; }

    void OnEnable(bool /*isGameAttached*/) override {
        BindImGui();
        LoadSettings();
        m_Scanner.Reset();
        m_Enabled = true;
    }

    void OnDisable() override {
        SaveSettings();
        m_Scanner.FlushDiscoveryLog();
        m_Scanner.SetDiscoveryLog(false, {});
        m_Scanner.SetDiscovery(false);
        m_Scanner.ClearDiscovery();
        m_Scanner.Reset();
        m_Enabled = false;
    }

    bool WantsOverlay() const override { return true; }

    void DrawUI() override {
        if (!m_Enabled || !HostCompatible() || !BindImGui()) return;
        RetryBlockedLoad();
        const PluginSDK::Context& c = *ctx();
        if (!c.Game.IsAttached() || !c.Game.IsInGame()) {
            m_Scanner.Reset();   // cheap when already empty; area data never survives a loading screen
            return;
        }
        // Keep scanning while hidden so the circles are current the moment they show again.
        m_Scanner.Tick(c, m_Settings, std::chrono::steady_clock::now());

        if (m_Settings.onlyWhenForeground && !c.Game.IsForeground()) return;
        if (m_Settings.hideWhenMenuVisible && c.Game.IsMenuVisible()) return;
        DrawOverlay(c, m_Settings, m_Scanner.Markers(), m_OverlayStats);
    }

    void DrawSettings() override {
        if (!BindImGui()) return;
        RetryBlockedLoad();
        PanelContext pc{ m_Settings, m_Scanner, m_OverlayStats, m_DiscoveryLogFile, HostCompatible() };
        PanelResult result;
        DrawSettingsPanel(pc, result);
        if (result.rulesChanged) m_Scanner.OnRulesChanged();
        if (result.changed) m_Dirty = true;
        // Save once the edit is finished, not on every frame of a slider drag.
        if (m_Dirty && !ImGui::IsAnyItemActive()) {
            SaveSettings();
            m_Dirty = false;
        }
    }

    void SaveSettings() override {
        if (!m_Loaded) return;   // never overwrite the file with defaults before it was read
        try {
            m_Settings.Clamp();
            std::string json = ToJson(m_Settings);
            if (json == m_LastSaved) return;
            // The file could not be read at load: the player changed a setting
            // since, so it is replaced now, after keeping a copy of the old one.
            if (m_LoadBlocked) {
                m_LoadRetriesLeft = 0;   // the player's edit wins over a later read
                KeepUnreadableCopy(DirectoryPath());
            }
            if (Save(DirectoryPath(), json)) {
                m_LastSaved = std::move(json);
                if (m_LoadBlocked) {
                    m_LoadBlocked = false;
                    ctx()->Log.Warn("Ground Effects: replaced the unreadable GroundEffects.json after a settings change "
                                    "(a copy was kept as .unreadable where possible)");
                }
            }
        } catch (const std::exception&) {
            // Out of memory while serialising: keep the previous file.
        }
    }

private:
    bool BindImGui() const {
        if (ctx()->ImGuiContext) ImGui::SetCurrentContext(static_cast<ImGuiContext*>(ctx()->ImGuiContext));
        // ImGui 1.93 keeps its word-wrap character table in a static of each ImGui
        // copy, filled only when that copy builds a font atlas. This DLL never
        // does (it draws with the host's context), so without this every wrapped
        // text and tooltip breaks mid-word. Cheap: returns at once when filled.
        ImTextInitClassifiers();
        return ImGui::GetCurrentContext() != nullptr;
    }

    void LoadSettings() {
        m_LoadBlocked = false;
        m_LoadRetriesLeft = kLoadRetries;
        m_LoadRetryAt = std::chrono::steady_clock::now() + kLoadRetryEvery;
        try {
            const std::filesystem::path dir = DirectoryPath();
            m_DiscoveryLogFile = dir.empty() ? std::filesystem::path() : ConfigDir(dir) / L"GroundEffectsDiscovery.log";
            Settings loaded;
            const LoadSource src = Load(dir, loaded);
            AdoptLoaded(src, std::move(loaded));
        } catch (const std::exception&) {
            m_Settings = DefaultSettings();
            m_Loaded = true;
        }
        m_Scanner.OnRulesChanged();
    }

    void AdoptLoaded(LoadSource src, Settings&& loaded) {
        m_Settings = std::move(loaded);
        m_Loaded = true;
        if (src == LoadSource::Json) {
            m_LastSaved = ToJson(m_Settings);
        } else if (src == LoadSource::Unreadable) {
            // Defaults in memory, but nothing is written until a setting changes:
            // the file is the player's and is read again shortly.
            m_LoadBlocked = true;
            m_LastSaved = ToJson(m_Settings);
            ctx()->Log.Warn("Ground Effects: GroundEffects.json exists but could not be read (another program may "
                            "hold it); using defaults without overwriting it, reading it again shortly");
        } else {
            m_LastSaved.clear();
            if (src == LoadSource::LegacyIni)
                ctx()->Log.Info("Ground Effects: settings moved from GroundEffects.ini to GroundEffects.json");
            else if (src == LoadSource::CorruptJson)
                ctx()->Log.Warn("Ground Effects: GroundEffects.json was unreadable; kept as .corrupt, defaults used");
            SaveSettings();
        }
    }

    // A settings file that was locked at load is read again a few times; the
    // read is a small file every few seconds, and stops once a setting changes.
    void RetryBlockedLoad() {
        if (!m_LoadBlocked || m_LoadRetriesLeft <= 0 || m_Dirty) return;
        const auto now = std::chrono::steady_clock::now();
        if (now < m_LoadRetryAt) return;
        m_LoadRetryAt = now + kLoadRetryEvery;
        --m_LoadRetriesLeft;
        try {
            Settings loaded;
            const LoadSource src = Load(DirectoryPath(), loaded);
            if (src == LoadSource::Unreadable) {
                if (m_LoadRetriesLeft == 0)
                    ctx()->Log.Warn("Ground Effects: GroundEffects.json is still unreadable; defaults stay in use and "
                                    "the file is replaced only if a setting is changed");
                return;
            }
            m_LoadBlocked = false;
            AdoptLoaded(src, std::move(loaded));
            m_Scanner.OnRulesChanged();
            if (src == LoadSource::Json) ctx()->Log.Info("Ground Effects: GroundEffects.json read on a later attempt");
        } catch (const std::exception&) {
            // Out of memory: keep the defaults and try again later.
        }
    }

    static constexpr int kLoadRetries = 15;
    static constexpr std::chrono::seconds kLoadRetryEvery{ 2 };

    Settings     m_Settings = DefaultSettings();
    Scanner      m_Scanner;
    OverlayStats m_OverlayStats;
    std::filesystem::path m_DiscoveryLogFile;
    std::string  m_LastSaved;
    bool         m_Loaded = false;
    bool         m_LoadBlocked = false;   // the file exists but was unreadable at load
    int          m_LoadRetriesLeft = 0;
    std::chrono::steady_clock::time_point m_LoadRetryAt{};
    bool         m_Dirty = false;
    bool         m_Enabled = false;
};

} // namespace

extern "C" PLUGIN_API PluginSDK::Plugin* CreatePlugin() {
    return new (std::nothrow) GroundEffectsPlugin();
}

extern "C" PLUGIN_API void DestroyPlugin(PluginSDK::Plugin* plugin) {
    delete plugin;
}
