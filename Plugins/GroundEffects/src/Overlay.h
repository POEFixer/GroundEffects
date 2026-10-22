// Draws the ground-effect circles. Every frame each circle is projected again
// from its world position, so circles stay glued to the ground while the camera
// moves; a circle that leaves the screen edge keeps its visible arc.
#pragma once

#include "../../../POEFixer/plugin_sdk/PluginSDK.h"
#include "Scanner.h"
#include "Settings.h"

namespace GroundEffects {

struct OverlayStats {
    int    drawn = 0;
    double lastFrameUs = 0.0;
};

void DrawOverlay(const PluginSDK::Context& ctx, const Settings& s, const std::vector<Marker>& markers,
                 OverlayStats& stats);

} // namespace GroundEffects
