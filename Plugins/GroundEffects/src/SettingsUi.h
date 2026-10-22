// The Ground Effects tab in the host's Plugins window. One small function per
// card, so each card can move to the shared plugin UI kit on its own.
#pragma once

#include "Overlay.h"
#include "Scanner.h"
#include "Settings.h"

#include <filesystem>

namespace GroundEffects {

struct PanelResult {
    bool changed = false;        // persist when no widget is being edited
    bool rulesChanged = false;   // rematch markers now
};

struct PanelContext {
    Settings&                    settings;
    Scanner&                     scanner;
    const OverlayStats&          overlay;
    const std::filesystem::path& discoveryLogFile;
    bool                         hostCompatible;
};

void DrawSettingsPanel(PanelContext& pc, PanelResult& out);

} // namespace GroundEffects
