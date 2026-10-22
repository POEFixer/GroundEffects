#include "Overlay.h"

#include "../../../POEFixer/plugin_sdk/PluginUi.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>

namespace GroundEffects {

namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr int   kMaxLabels = 64;
// The outline is centred on the effect's real edge (ImGui 1.93: thickness comes
// before the flags; the stroke position is spelled out so the dark edge stays
// half outside, half inside the coloured line).
constexpr ImDrawFlags kClosedCentered = ImDrawFlags_Closed | ImDrawFlags_StrokeCenter;

ImU32 ToU32(const Color4& c, float alpha) {
    return ImGui::ColorConvertFloat4ToU32(ImVec4(c.r, c.g, c.b, std::clamp(alpha, 0.0f, 1.0f)));
}

bool Overlaps(const ImVec4& a, const ImVec4& b) {
    return a.x < b.z && b.x < a.z && a.y < b.w && b.y < a.w;
}

} // namespace

void DrawOverlay(const PluginSDK::Context& ctx, const Settings& s, const std::vector<Marker>& markers,
                 OverlayStats& stats) {
    const auto t0 = std::chrono::steady_clock::now();
    stats.drawn = 0;
    if (markers.empty()) {
        stats.lastFrameUs = 0.0;
        return;
    }
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    if (!dl) return;

    const int segs = std::clamp(s.circleSegments, Limits::kSegmentsMin, Limits::kSegmentsMax);
    std::array<float, Limits::kSegmentsMax> cosT{}, sinT{};
    for (int i = 0; i < segs; ++i) {
        const float a = 2.0f * kPi * static_cast<float>(i) / static_cast<float>(segs);
        cosT[static_cast<size_t>(i)] = std::cos(a);
        sinT[static_cast<size_t>(i)] = std::sin(a);
    }

    const float opacity = std::clamp(s.opacity, Limits::kOpacityMin, Limits::kOpacityMax);
    const float thickness = std::clamp(s.outlineThickness, Limits::kOutlineMin, Limits::kOutlineMax);
    const float fontSize = ImGui::GetFontSize();
    std::array<ImVec4, kMaxLabels> labelRects{};
    int labelCount = 0;

    std::array<ImVec2, Limits::kSegmentsMax> pts{};
    std::array<ImVec2, Limits::kSegmentsMax> run{};
    std::array<bool, Limits::kSegmentsMax> vis{};

    for (const Marker& m : markers) {
        if (m.rule < 0 || static_cast<size_t>(m.rule) >= s.rules.size()) continue;
        const Rule& rule = s.rules[static_cast<size_t>(m.rule)];
        if (!rule.enabled) continue;

        const float z = m.z + s.heightOffset;
        int visible = 0;
        float maxX = -1.0e9f, sumY = 0.0f;
        for (int i = 0; i < segs; ++i) {
            const size_t k = static_cast<size_t>(i);
            float sx = 0.0f, sy = 0.0f;
            vis[k] = ctx.Render.WorldToScreen(m.x + cosT[k] * m.radius, m.y + sinT[k] * m.radius, z, sx, sy) &&
                     std::isfinite(sx) && std::isfinite(sy);
            if (!vis[k]) continue;
            pts[k] = ImVec2(sx, sy);
            ++visible;
            maxX = std::max(maxX, sx);
            sumY += sy;
        }
        if (visible < 2) continue;
        ++stats.drawn;

        const ImU32 fill = ToU32(rule.color, rule.color.a * opacity);
        const ImU32 line = ToU32(rule.color, std::min(1.0f, rule.color.a + 0.30f) * opacity);
        const ImU32 edge = IM_COL32(0, 0, 0, static_cast<int>(110.0f * opacity));

        if (visible == segs) {
            if (s.fillCircles) dl->AddConvexPolyFilled(pts.data(), segs, fill);
            dl->AddPolyline(pts.data(), segs, edge, thickness + 2.0f, kClosedCentered);
            dl->AddPolyline(pts.data(), segs, line, thickness, kClosedCentered);
        } else {
            // Partly off screen: the visible vertices of a convex polygon, in order,
            // still form a convex polygon; outline only the unbroken visible arcs.
            if (s.fillCircles && visible >= 3) {
                int n = 0;
                for (int i = 0; i < segs; ++i)
                    if (vis[static_cast<size_t>(i)]) run[static_cast<size_t>(n++)] = pts[static_cast<size_t>(i)];
                dl->AddConvexPolyFilled(run.data(), n, fill);
            }
            int gap = 0;
            while (gap < segs && vis[static_cast<size_t>(gap)]) ++gap;
            int n = 0;
            for (int step = 1; step <= segs; ++step) {
                const size_t i = static_cast<size_t>((gap + step) % segs);
                if (vis[i]) run[static_cast<size_t>(n++)] = pts[i];
                if (!vis[i] || step == segs) {
                    if (n >= 2) {
                        dl->AddPolyline(run.data(), n, edge, thickness + 2.0f, ImDrawFlags_StrokeCenter);
                        dl->AddPolyline(run.data(), n, line, thickness, ImDrawFlags_StrokeCenter);
                    }
                    n = 0;
                }
            }
        }

        if (s.showLabels && labelCount < kMaxLabels) {
            const std::string& text = rule.label.empty() ? m.typeId : rule.label;
            if (text.empty()) continue;
            const ImVec2 size = ImGui::CalcTextSize(text.c_str());
            ImVec2 pos(maxX + fontSize * 0.35f, sumY / static_cast<float>(visible) - size.y * 0.5f);
            ImVec4 rect(pos.x, pos.y, pos.x + size.x, pos.y + size.y);
            bool placed = false;
            for (int attempt = 0; attempt < 4 && !placed; ++attempt) {
                placed = true;
                for (int i = 0; i < labelCount; ++i) {
                    if (Overlaps(rect, labelRects[static_cast<size_t>(i)])) { placed = false; break; }
                }
                if (!placed) {
                    const float dy = size.y + 2.0f;
                    rect.y += dy; rect.w += dy; pos.y += dy;
                }
            }
            if (!placed) continue;
            labelRects[static_cast<size_t>(labelCount++)] = rect;
            PluginUi::TextOutlined(dl, nullptr, fontSize, pos, ToU32(rule.color, opacity), text.c_str());
        }
    }
    stats.lastFrameUs = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count();
}

} // namespace GroundEffects
