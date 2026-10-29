#include "SettingsUi.h"

#include "Text.h"

#include "../../../POEFixer/plugin_sdk/PluginUi.h"
#include "../../../POEFixer/imgui/IconsFontAwesome6.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace GroundEffects {

namespace {

using PluginUi::Tip;

// Flow layout the kit has no helper for: the next item goes on the same row when
// it still fits, else on a new row.
void SameLineIfFitsWidth(float itemWidth) {
    ImGui::SameLine();
    if (ImGui::GetContentRegionAvail().x < itemWidth + ImGui::GetStyle().ItemSpacing.x) ImGui::NewLine();
}

// Same, for a checkbox with this label.
void SameLineIfFitsCheckbox(const char* nextLabel) {
    const ImGuiStyle& st = ImGui::GetStyle();
    SameLineIfFitsWidth(ImGui::GetFrameHeight() + st.ItemInnerSpacing.x + ImGui::CalcTextSize(nextLabel, nullptr, true).x);
}

ImVec4 ToVec4(const Color4& c) { return ImVec4(c.r, c.g, c.b, c.a); }

void DrawSectionDisplay(Settings& s, PanelResult& out) {
    PluginUi::BeginCard("###ge_card_display");
    PluginUi::Heading(ICON_FA_PALETTE, I18n::Tr("display_heading"));

    out.changed |= PluginUi::Check(I18n::Tr("display_fill"), "ge_fill", &s.fillCircles, I18n::Tr("display_fill_tip"));
    const char* showNames = I18n::Tr("display_show_names");
    SameLineIfFitsCheckbox(showNames);
    out.changed |= PluginUi::Check(showNames, "ge_labels", &s.showLabels, I18n::Tr("display_show_names_tip"));

    ImGui::Spacing();
    const char* outline = I18n::Tr("display_outline_width");
    const char* opacity = I18n::Tr("display_opacity");
    const char* smoothness = I18n::Tr("display_smoothness");
    const char* height = I18n::Tr("display_height_offset");
    const PluginUi::LabelColumn col{ outline, opacity, smoothness, height };

    out.changed |= PluginUi::SliderFloatRow(col, outline, "###ge_outline", &s.outlineThickness,
        Limits::kOutlineMin, Limits::kOutlineMax, UnitFormat(I18n::Tr("unit_pixels"), "%.1f").c_str(),
        I18n::Tr("display_outline_width_tip"));

    col.Row(opacity);
    int pct = static_cast<int>(std::lround(s.opacity * 100.0f));
    if (ImGui::SliderInt("###ge_opacity", &pct, 10, 100, "%d%%", ImGuiSliderFlags_AlwaysClamp)) {
        s.opacity = static_cast<float>(pct) / 100.0f;
        out.changed = true;
    }
    Tip(I18n::Tr("display_opacity_tip"));

    out.changed |= PluginUi::SliderIntRow(col, smoothness, "###ge_segments", &s.circleSegments,
        Limits::kSegmentsMin, Limits::kSegmentsMax, UnitFormat(I18n::Tr("unit_points"), "%d").c_str(),
        I18n::Tr("display_smoothness_tip"));

    out.changed |= PluginUi::SliderFloatRow(col, height, "###ge_height", &s.heightOffset,
        Limits::kHeightMin, Limits::kHeightMax, "%.0f", I18n::Tr("display_height_offset_tip"));
    if (s.heightOffset != 0.0f) {
        ImGui::SameLine();
        if (PluginUi::SubtleButton(ICON_FA_ROTATE_LEFT "###ge_height_reset")) { s.heightOffset = 0.0f; out.changed = true; }
        Tip(I18n::Tr("display_height_reset_tip"));
    }
    PluginUi::EndCard();
}

void DrawSectionVisibility(Settings& s, PanelResult& out) {
    PluginUi::BeginCard("###ge_card_when");
    PluginUi::Heading(ICON_FA_EYE, I18n::Tr("visibility_heading"));

    out.changed |= PluginUi::Check(I18n::Tr("visibility_hostile"), "ge_hostile", &s.onlyHostile,
        I18n::Tr("visibility_hostile_tip"));
    out.changed |= PluginUi::Check(I18n::Tr("visibility_town"), "ge_town", &s.skipTownHideout,
        I18n::Tr("visibility_town_tip"));
    out.changed |= PluginUi::Check(I18n::Tr("visibility_foreground"), "ge_fg", &s.onlyWhenForeground,
        I18n::Tr("visibility_foreground_tip"));
    out.changed |= PluginUi::Check(I18n::Tr("visibility_hide_menu"), "ge_menu", &s.hideWhenMenuVisible,
        I18n::Tr("visibility_hide_menu_tip"));
    PluginUi::EndCard();
}

// The rule's name field. A default rule's stored English name is shown in the program
// language (display only); typing that shown name back stores the English name again.
bool RuleLabelField(Rule& r) {
    std::string shown = ShownRuleLabel(r.label);
    if (!PluginUi::InputString("###label", I18n::Tr("rules_name_hint"), shown)) return false;
    r.label = StoredRuleLabel(shown);
    return true;
}

void DrawRuleRow(Settings& s, size_t i, bool compact, PanelResult& out, int& moveFrom, int& moveTo, int& removeAt) {
    Rule& r = s.rules[i];
    const float fh = ImGui::GetFrameHeight();
    ImGui::PushID(static_cast<int>(i));
    ImGui::TableNextRow();

    ImGui::TableNextColumn();
    if (ImGui::Checkbox("###on", &r.enabled)) { out.changed = true; out.rulesChanged = true; }
    Tip(I18n::Tr("rules_on_tip"));

    ImGui::TableNextColumn();
    ImVec4 c = ToVec4(r.color);
    if (PluginUi::Swatch("###color", c)) {
        r.color = Color4{ c.x, c.y, c.z, c.w };
        out.changed = true;
    }
    Tip(I18n::Tr("rules_colour_tip"));

    ImGui::BeginDisabled(!r.enabled);
    ImGui::TableNextColumn();
    ImGui::SetNextItemWidth(-FLT_MIN);
    out.changed |= RuleLabelField(r);
    if (ImGui::IsItemDeactivatedAfterEdit()) Utf8Truncate(r.label, Limits::kMaxLabelLen);
    Tip(I18n::TrF("rules_name_tip", { { "option", I18n::Tr("display_show_names") } }).c_str());

    if (!compact) ImGui::TableNextColumn();
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (PluginUi::InputString("###match", I18n::Tr("rules_match_hint"), r.match)) { out.changed = true; out.rulesChanged = true; }
    if (ImGui::IsItemDeactivatedAfterEdit()) Utf8Truncate(r.match, Limits::kMaxMatchLen);
    Tip(I18n::Tr("rules_match_tip"));
    ImGui::EndDisabled();

    ImGui::TableNextColumn();
    const ImVec2 btn(fh, fh);
    ImGui::BeginDisabled(i == 0);
    if (PluginUi::SubtleButton(ICON_FA_ARROW_UP "###up", btn)) { moveFrom = static_cast<int>(i); moveTo = moveFrom - 1; }
    Tip(I18n::Tr("rules_up_tip"));
    ImGui::EndDisabled();
    ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
    ImGui::BeginDisabled(i + 1 >= s.rules.size());
    if (PluginUi::SubtleButton(ICON_FA_ARROW_DOWN "###down", btn)) { moveFrom = static_cast<int>(i); moveTo = moveFrom + 1; }
    Tip(I18n::Tr("rules_down_tip"));
    ImGui::EndDisabled();
    ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
    if (PluginUi::SubtleButton(ICON_FA_TRASH "###del", btn)) removeAt = static_cast<int>(i);
    Tip(I18n::Tr("rules_delete_tip"));

    ImGui::PopID();
}

void DrawSectionRules(Settings& s, PanelResult& out) {
    PluginUi::BeginCard("###ge_card_rules");
    const std::string help = I18n::TrF("rules_help", { { "option", I18n::Tr("diag_discover") } });
    PluginUi::Heading(ICON_FA_LIST, I18n::Tr("rules_heading"), help.c_str());

    int moveFrom = -1, moveTo = -1, removeAt = -1;
    if (s.rules.empty()) {
        PluginUi::Hint(I18n::Tr("rules_none"));
    } else {
        const ImGuiStyle& st = ImGui::GetStyle();
        const float fh = ImGui::GetFrameHeight();
        const float actionsW = fh * 3.0f + st.ItemInnerSpacing.x * 2.0f;
        // Not resizable: the fixed columns (width 0) fit their translated header and cells every frame.
        const ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg |
                                      ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_PadOuterX;
        // On a narrow pane the name and the match text share one column, one above the other.
        const bool compact = ImGui::GetContentRegionAvail().x < ImGui::GetFontSize() * 30.0f;
        if (ImGui::BeginTable("###ge_rules", compact ? 4 : 5, flags)) {
            ImGui::TableSetupColumn(I18n::TrId("rules_col_on"), ImGuiTableColumnFlags_WidthFixed, 0.0f);
            ImGui::TableSetupColumn(I18n::TrId("rules_col_colour"), ImGuiTableColumnFlags_WidthFixed, 0.0f);
            if (compact) {
                ImGui::TableSetupColumn(I18n::TrId("rules_col_name_match"), ImGuiTableColumnFlags_WidthStretch, 1.0f);
            } else {
                ImGui::TableSetupColumn(I18n::TrId("rules_col_name"), ImGuiTableColumnFlags_WidthStretch, 1.0f);
                ImGui::TableSetupColumn(I18n::TrId("rules_col_match"), ImGuiTableColumnFlags_WidthStretch, 1.3f);
            }
            ImGui::TableSetupColumn("###actions", ImGuiTableColumnFlags_WidthFixed, actionsW);
            ImGui::TableHeadersRow();
            for (size_t i = 0; i < s.rules.size(); ++i) DrawRuleRow(s, i, compact, out, moveFrom, moveTo, removeAt);
            ImGui::EndTable();
        }
    }

    if (moveFrom >= 0 && moveTo >= 0 && moveTo < static_cast<int>(s.rules.size())) {
        std::swap(s.rules[static_cast<size_t>(moveFrom)], s.rules[static_cast<size_t>(moveTo)]);
        out.changed = out.rulesChanged = true;
    }
    if (removeAt >= 0 && removeAt < static_cast<int>(s.rules.size())) {
        s.rules.erase(s.rules.begin() + removeAt);
        out.changed = out.rulesChanged = true;
    }

    ImGui::Spacing();
    const std::string addLabel = std::string(ICON_FA_PLUS "  ") + I18n::Tr("rules_add") + "###ge_add";
    const std::string resetLabel = std::string(ICON_FA_ROTATE_LEFT "  ") + I18n::Tr("rules_restore") + "###ge_reset";
    ImGui::BeginDisabled(s.rules.size() >= Limits::kMaxRules);
    if (ImGui::Button(addLabel.c_str())) {
        Rule r;
        r.label = I18n::Tr("rules_new_rule");   // the user's own name from now on, in the language of the moment
        s.rules.push_back(r);
        out.changed = out.rulesChanged = true;
    }
    ImGui::EndDisabled();
    Tip(I18n::Tr("rules_add_tip"));
    SameLineIfFitsWidth(PluginUi::ButtonWidth(resetLabel.c_str()));
    if (PluginUi::SubtleButton(resetLabel.c_str())) ImGui::OpenPopup("###ge_reset_confirm");
    Tip(I18n::Tr("rules_restore_tip"));
    if (PluginUi::ConfirmPopup("###ge_reset_confirm", I18n::Tr("rules_restore_confirm"), I18n::Tr("rules_replace"),
                               nullptr)) {
        s.rules = DefaultRules();
        out.changed = out.rulesChanged = true;
    }
    PluginUi::EndCard();
}

void DrawSectionPerformance(Settings& s, PanelResult& out) {
    PluginUi::BeginCard("###ge_card_perf");
    PluginUi::Heading(ICON_FA_GAUGE, I18n::Tr("perf_heading"));
    const char* interval = I18n::Tr("perf_interval");
    const char* budget = I18n::Tr("perf_budget");
    const char* maxMarkers = I18n::Tr("perf_max_circles");
    const PluginUi::LabelColumn col{ interval, budget, maxMarkers };

    out.changed |= PluginUi::SliderIntRow(col, interval, "###ge_interval", &s.scanIntervalMs,
        Limits::kScanIntervalMin, Limits::kScanIntervalMax, UnitFormat(I18n::Tr("unit_ms"), "%d").c_str(),
        I18n::Tr("perf_interval_tip"));

    out.changed |= PluginUi::SliderIntRow(col, budget, "###ge_budget", &s.scanBudgetMs,
        Limits::kScanBudgetMin, Limits::kScanBudgetMax, UnitFormat(I18n::Tr("unit_ms"), "%d").c_str(),
        I18n::Tr("perf_budget_tip"));

    out.changed |= PluginUi::SliderIntRow(col, maxMarkers, "###ge_max", &s.maxMarkers,
        Limits::kMaxMarkersMin, Limits::kMaxMarkersMax, "%d", I18n::Tr("perf_max_circles_tip"));
    PluginUi::EndCard();
}

void DrawDiscoveryTable(Scanner& scanner, Settings& s, PanelResult& out) {
    const auto& rows = scanner.DiscoveryRows();
    if (rows.empty()) {
        if (scanner.Discovery())
            PluginUi::Hint(I18n::Tr("discovery_nothing_yet"));
        else
            PluginUi::Hint(I18n::TrF("discovery_turn_on", { { "option", I18n::Tr("diag_discover") } }).c_str());
        return;
    }
    const float fh = ImGui::GetFrameHeight();
    const float rowH = ImGui::GetTextLineHeightWithSpacing() + ImGui::GetStyle().CellPadding.y * 2.0f;
    const int visibleRows = std::min<int>(static_cast<int>(rows.size()), 10) + 1;
    // Not resizable: the fixed columns (width 0) fit their translated header and cells every frame.
    const ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
                                  ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_PadOuterX;
    const float actionsW = fh * 2.0f + ImGui::GetStyle().ItemInnerSpacing.x;
    if (!ImGui::BeginTable("###ge_discovery", 5, flags, ImVec2(0.0f, rowH * static_cast<float>(visibleRows)))) return;
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn(I18n::TrId("discovery_col_type"), ImGuiTableColumnFlags_WidthStretch, 1.0f);
    ImGui::TableSetupColumn(I18n::TrId("discovery_col_radius"), ImGuiTableColumnFlags_WidthFixed, 0.0f);
    ImGui::TableSetupColumn(I18n::TrId("discovery_col_side"), ImGuiTableColumnFlags_WidthFixed, 0.0f);
    ImGui::TableSetupColumn(I18n::TrId("discovery_col_seen"), ImGuiTableColumnFlags_WidthFixed, 0.0f);
    ImGui::TableSetupColumn("###act", ImGuiTableColumnFlags_WidthFixed, actionsW);
    ImGui::TableHeadersRow();

    for (size_t i = 0; i < rows.size(); ++i) {
        const DiscoveryRow& r = rows[i];
        ImGui::PushID(static_cast<int>(i));
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(r.typeId.empty() ? I18n::Tr("discovery_no_type_id") : r.typeId.c_str());
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
            const auto orDash = [](const std::string& v) { return v.empty() ? std::string("-") : v; };
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 34.0f);
            ImGui::TextUnformatted(I18n::TrF("discovery_end", { { "value", orDash(r.endEffect) } }).c_str());
            ImGui::TextUnformatted(I18n::TrF("discovery_visual_1", { { "value", orDash(r.buffVisual1) } }).c_str());
            ImGui::TextUnformatted(I18n::TrF("discovery_visual_2", { { "value", orDash(r.buffVisual2) } }).c_str());
            ImGui::TextUnformatted(I18n::TrF("discovery_model", { { "value", orDash(r.aoFile) } }).c_str());
            ImGui::TextUnformatted(I18n::TrF("discovery_path", { { "value", r.path } }).c_str());
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
        ImGui::TableNextColumn();
        ImGui::Text("%.0f", r.radius);
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(!r.positionedValid ? I18n::Tr("discovery_side_unknown")
                               : r.friendly        ? I18n::Tr("discovery_side_friendly")
                                                   : I18n::Tr("discovery_side_hostile"));
        ImGui::TableNextColumn();
        ImGui::Text("%d", r.seen);
        ImGui::TableNextColumn();
        if (PluginUi::SubtleButton(ICON_FA_COPY "###copy", ImVec2(fh, fh)))
            ImGui::SetClipboardText(r.typeId.c_str());
        Tip(I18n::Tr("discovery_copy_tip"));
        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        ImGui::BeginDisabled(r.typeId.empty() || s.rules.size() >= Limits::kMaxRules);
        if (PluginUi::SubtleButton(ICON_FA_PLUS "###addrule", ImVec2(fh, fh))) {
            Rule rule;
            rule.label = r.typeId;
            rule.match = r.typeId;
            s.rules.push_back(rule);
            out.changed = out.rulesChanged = true;
        }
        Tip(I18n::Tr("discovery_add_rule_tip"));
        ImGui::EndDisabled();
        ImGui::PopID();
    }
    ImGui::EndTable();
}

void DrawSectionDiagnostics(PanelContext& pc, PanelResult& out) {
    Settings& s = pc.settings;
    Scanner& scanner = pc.scanner;
    if (PluginUi::BeginDiagnostics(I18n::Tr("diag_tip"), "ge_diag")) {
        const ScanStats& st = scanner.Stats();
        PluginUi::Hint(I18n::TrF("diag_marked_now", {
            { "marked", Num("%d", static_cast<int>(scanner.Markers().size())) },
            { "drawn", Num("%d", pc.overlay.drawn) },
            { "ms", Num("%.2f", pc.overlay.lastFrameUs / 1000.0) } }).c_str());
        PluginUi::Hint(I18n::TrF("diag_last_scan", {
            { "entities", Num("%d", static_cast<int>(st.lastPassEntities)) },
            { "reads", Num("%d", st.componentReads) },
            { "ms", Num("%.2f", st.lastPassMs) },
            { "frames", Num("%d", st.lastPassFrames) } }).c_str());
        PluginUi::Hint(I18n::TrF("diag_remembered", { { "count", Num("%d", static_cast<int>(st.knownEntities)) } }).c_str());
        ImGui::Spacing();

        if (PluginUi::Check(I18n::Tr("diag_prefilter"), "ge_prefilter", &s.usePathPrefilter, I18n::Tr("diag_prefilter_tip"))) {
            out.changed = true;
            scanner.Reset();
        }

        bool discovery = scanner.Discovery();
        if (PluginUi::Check(I18n::Tr("diag_discover"), "ge_discover", &discovery, I18n::Tr("diag_discover_tip")))
            scanner.SetDiscovery(discovery);
        const char* writeLog = I18n::Tr("diag_write_log");
        SameLineIfFitsCheckbox(writeLog);
        ImGui::BeginDisabled(!discovery);
        bool log = scanner.DiscoveryLog();
        if (PluginUi::Check(writeLog, "ge_log", &log, I18n::Tr("diag_write_log_tip")))
            scanner.SetDiscoveryLog(log, pc.discoveryLogFile);
        ImGui::EndDisabled();
        const char* clearLabel = I18n::TrId("diag_clear_list");
        SameLineIfFitsWidth(PluginUi::ButtonWidth(clearLabel));
        if (PluginUi::SubtleButton(clearLabel)) scanner.ClearDiscovery();
        Tip(I18n::Tr("diag_clear_list_tip"));

        ImGui::Spacing();
        DrawDiscoveryTable(scanner, s, out);
        PluginUi::EndDiagnostics();
    }
}

} // namespace

void DrawSettingsPanel(PanelContext& pc, PanelResult& out) {
    if (!pc.hostCompatible) {
        ImGui::PushStyleColor(ImGuiCol_Text, PluginUi::Colors().Warning);
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextUnformatted(I18n::Tr("host_too_old"));
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }
    DrawSectionDisplay(pc.settings, out);
    ImGui::Spacing();
    DrawSectionVisibility(pc.settings, out);
    ImGui::Spacing();
    DrawSectionRules(pc.settings, out);
    ImGui::Spacing();
    DrawSectionPerformance(pc.settings, out);
    ImGui::Spacing();
    DrawSectionDiagnostics(pc, out);
    ImGui::Spacing();
    PluginUi::Hint("Originally created by 1k4ru5g3. Maintained by POEFixer.");
    ImGui::Spacing();
}

} // namespace GroundEffects
