#include "SettingsUi.h"

#include "../../../POEFixer/plugin_sdk/PluginUi.h"
#include "../../../POEFixer/imgui/IconsFontAwesome6.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

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
    PluginUi::Heading(ICON_FA_PALETTE, "Display");

    out.changed |= PluginUi::Check("Fill circles", "ge_fill", &s.fillCircles,
        "Paint the inside of each circle with the rule colour.\n"
        "Turn it off to see only the outline.");
    SameLineIfFitsCheckbox("Show names");
    out.changed |= PluginUi::Check("Show names", "ge_labels", &s.showLabels,
        "Write the rule name beside each circle (the effect's type id when the rule has no name).");

    ImGui::Spacing();
    const PluginUi::LabelColumn col{ "Outline width", "Opacity", "Smoothness", "Height offset" };

    out.changed |= PluginUi::SliderFloatRow(col, "Outline width", "###ge_outline", &s.outlineThickness,
        Limits::kOutlineMin, Limits::kOutlineMax, "%.1f px",
        "Thickness of the circle outline.\n"
        "A thin dark edge is drawn under it so it stays visible on bright ground.");

    col.Row("Opacity");
    int pct = static_cast<int>(std::lround(s.opacity * 100.0f));
    if (ImGui::SliderInt("###ge_opacity", &pct, 10, 100, "%d%%", ImGuiSliderFlags_AlwaysClamp)) {
        s.opacity = static_cast<float>(pct) / 100.0f;
        out.changed = true;
    }
    Tip("Fades every circle and name together.\n"
        "Each rule's own colour keeps its transparency on top of this.");

    out.changed |= PluginUi::SliderIntRow(col, "Smoothness", "###ge_segments", &s.circleSegments,
        Limits::kSegmentsMin, Limits::kSegmentsMax, "%d points",
        "Points per circle.\n"
        "More points look rounder and cost a little more time each frame.");

    out.changed |= PluginUi::SliderFloatRow(col, "Height offset", "###ge_height", &s.heightOffset,
        Limits::kHeightMin, Limits::kHeightMax, "%.0f",
        "Moves every circle up or down in the world (negative values lift it).\n"
        "Leave it at 0 unless circles float above or sink into the ground.");
    if (s.heightOffset != 0.0f) {
        ImGui::SameLine();
        if (PluginUi::SubtleButton(ICON_FA_ROTATE_LEFT "###ge_height_reset")) { s.heightOffset = 0.0f; out.changed = true; }
        Tip("Back to 0.");
    }
    PluginUi::EndCard();
}

void DrawSectionVisibility(Settings& s, PanelResult& out) {
    PluginUi::BeginCard("###ge_card_when");
    PluginUi::Heading(ICON_FA_EYE, "When to show");

    out.changed |= PluginUi::Check("Only hostile effects", "ge_hostile", &s.onlyHostile,
        "Skip ground effects your side created (your own and your allies' skills).\n"
        "Effects whose side cannot be read are skipped too.");
    out.changed |= PluginUi::Check("Hide in towns and hideouts", "ge_town", &s.skipTownHideout,
        "Draw nothing in towns and hideouts, where ground effects are harmless.");
    out.changed |= PluginUi::Check("Only while the game window is active", "ge_fg", &s.onlyWhenForeground,
        "Hide the circles while another window (a browser, the desktop) is in front of the game.");
    out.changed |= PluginUi::Check("Hide while the POEFixer menu is open", "ge_menu", &s.hideWhenMenuVisible,
        "Hide the circles while the POEFixer menu is shown.\n"
        "Leave it off to see colour changes live while you edit the rules.");
    PluginUi::EndCard();
}

void DrawRuleRow(Settings& s, size_t i, bool compact, PanelResult& out, int& moveFrom, int& moveTo, int& removeAt) {
    Rule& r = s.rules[i];
    const float fh = ImGui::GetFrameHeight();
    ImGui::PushID(static_cast<int>(i));
    ImGui::TableNextRow();

    ImGui::TableNextColumn();
    if (ImGui::Checkbox("###on", &r.enabled)) { out.changed = true; out.rulesChanged = true; }
    Tip("Use this rule. A switched-off rule keeps its settings.");

    ImGui::TableNextColumn();
    ImVec4 c = ToVec4(r.color);
    if (PluginUi::Swatch("###color", c)) {
        r.color = Color4{ c.x, c.y, c.z, c.w };
        out.changed = true;
    }
    Tip("Circle colour.\nIts transparency is the fill strength; the outline is drawn stronger.");

    ImGui::BeginDisabled(!r.enabled);
    ImGui::TableNextColumn();
    ImGui::SetNextItemWidth(-FLT_MIN);
    out.changed |= PluginUi::InputString("###label", "Name", r.label);
    if (ImGui::IsItemDeactivatedAfterEdit() && r.label.size() > Limits::kMaxLabelLen) r.label.resize(Limits::kMaxLabelLen);
    Tip("Name shown beside the circle when \"Show names\" is on.");

    if (!compact) ImGui::TableNextColumn();
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (PluginUi::InputString("###match", "e.g. ShockedGround", r.match)) { out.changed = true; out.rulesChanged = true; }
    if (ImGui::IsItemDeactivatedAfterEdit() && r.match.size() > Limits::kMaxMatchLen) r.match.resize(Limits::kMaxMatchLen);
    Tip("Text to look for in the effect's type id, visuals and path (not case-sensitive).\n"
        "Example: ShockedGround, IgnitedGround, CausticCloud.");
    ImGui::EndDisabled();

    ImGui::TableNextColumn();
    const ImVec2 btn(fh, fh);
    ImGui::BeginDisabled(i == 0);
    if (PluginUi::SubtleButton(ICON_FA_ARROW_UP "###up", btn)) { moveFrom = static_cast<int>(i); moveTo = moveFrom - 1; }
    Tip("Move up.\nRules are tried from the top; the first match wins.");
    ImGui::EndDisabled();
    ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
    ImGui::BeginDisabled(i + 1 >= s.rules.size());
    if (PluginUi::SubtleButton(ICON_FA_ARROW_DOWN "###down", btn)) { moveFrom = static_cast<int>(i); moveTo = moveFrom + 1; }
    Tip("Move down.");
    ImGui::EndDisabled();
    ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
    if (PluginUi::SubtleButton(ICON_FA_TRASH "###del", btn)) removeAt = static_cast<int>(i);
    Tip("Delete this rule.");

    ImGui::PopID();
}

void DrawSectionRules(Settings& s, PanelResult& out) {
    PluginUi::BeginCard("###ge_card_rules");
    PluginUi::Heading(ICON_FA_LIST, "Effect rules");
    PluginUi::HelpMarker(
        "Each rule colours the ground effects whose type id, visuals or path contain its match text, "
        "for example ShockedGround.\n\n"
        "Rules are tried from the top and the first enabled match wins, so put specific rules above "
        "general ones.\n\n"
        "To find the type id of an effect you see in game, open Diagnostics below and turn on "
        "\"Discover effects\".");

    int moveFrom = -1, moveTo = -1, removeAt = -1;
    if (s.rules.empty()) {
        PluginUi::Hint("No rules: nothing is marked. Add a rule or restore the defaults.");
    } else {
        const ImGuiStyle& st = ImGui::GetStyle();
        const float fh = ImGui::GetFrameHeight();
        const float actionsW = fh * 3.0f + st.ItemInnerSpacing.x * 2.0f;
        const float onW = std::max(fh, ImGui::CalcTextSize("On").x);
        const float colorW = std::max(fh, ImGui::CalcTextSize("Colour").x);
        const ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg |
                                      ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_PadOuterX;
        // On a narrow pane the name and the match text share one column, one above the other.
        const bool compact = ImGui::GetContentRegionAvail().x < ImGui::GetFontSize() * 30.0f;
        if (ImGui::BeginTable("###ge_rules", compact ? 4 : 5, flags)) {
            ImGui::TableSetupColumn("On", ImGuiTableColumnFlags_WidthFixed, onW);
            ImGui::TableSetupColumn("Colour", ImGuiTableColumnFlags_WidthFixed, colorW);
            if (compact) {
                ImGui::TableSetupColumn("Name / match text", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            } else {
                ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 1.0f);
                ImGui::TableSetupColumn("Match text", ImGuiTableColumnFlags_WidthStretch, 1.3f);
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
    const char* addLabel = ICON_FA_PLUS "  Add rule###ge_add";
    const char* resetLabel = ICON_FA_ROTATE_LEFT "  Restore default rules###ge_reset";
    ImGui::BeginDisabled(s.rules.size() >= Limits::kMaxRules);
    if (ImGui::Button(addLabel)) {
        Rule r;
        r.label = "New rule";
        s.rules.push_back(r);
        out.changed = out.rulesChanged = true;
    }
    ImGui::EndDisabled();
    Tip("Add an empty rule at the bottom.\nType a match text to make it work.");
    SameLineIfFitsWidth(PluginUi::ButtonWidth(resetLabel));
    if (PluginUi::SubtleButton(resetLabel)) ImGui::OpenPopup("###ge_reset_confirm");
    Tip("Replace all rules with the four default ones\n(shocked, chilled and ignited ground, caustic clouds).");
    if (PluginUi::ConfirmPopup("###ge_reset_confirm",
                               "Replace all rules with the four default rules? Your own rules are removed.",
                               "Replace", "Cancel")) {
        s.rules = DefaultRules();
        out.changed = out.rulesChanged = true;
    }
    PluginUi::EndCard();
}

void DrawSectionPerformance(Settings& s, PanelResult& out) {
    PluginUi::BeginCard("###ge_card_perf");
    PluginUi::Heading(ICON_FA_GAUGE, "Performance");
    const PluginUi::LabelColumn col{ "Look for new effects every", "Scan time per frame", "Most circles at once" };

    out.changed |= PluginUi::SliderIntRow(col, "Look for new effects every", "###ge_interval", &s.scanIntervalMs,
        Limits::kScanIntervalMin, Limits::kScanIntervalMax, "%d ms",
        "How often the plugin looks for ground effects that appeared.\n"
        "Circles already shown follow the camera every frame either way.");

    out.changed |= PluginUi::SliderIntRow(col, "Scan time per frame", "###ge_budget", &s.scanBudgetMs,
        Limits::kScanBudgetMin, Limits::kScanBudgetMax, "%d ms",
        "The most time one frame may spend looking.\n"
        "A longer search continues in the next frames, so the game never stutters.\n"
        "A scan normally needs far less than 1 ms.");

    out.changed |= PluginUi::SliderIntRow(col, "Most circles at once", "###ge_max", &s.maxMarkers,
        Limits::kMaxMarkersMin, Limits::kMaxMarkersMax, "%d",
        "Upper limit of circles drawn at the same time.");
    PluginUi::EndCard();
}

void DrawDiscoveryTable(Scanner& scanner, Settings& s, PanelResult& out) {
    const auto& rows = scanner.DiscoveryRows();
    if (rows.empty()) {
        PluginUi::Hint(scanner.Discovery() ? "Nothing seen yet. Walk near a ground effect in a map."
                                           : "Turn on \"Discover effects\" to list the ground effects around you.");
        return;
    }
    const float fh = ImGui::GetFrameHeight();
    const float rowH = ImGui::GetTextLineHeightWithSpacing() + ImGui::GetStyle().CellPadding.y * 2.0f;
    const int visibleRows = std::min<int>(static_cast<int>(rows.size()), 10) + 1;
    const ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
                                  ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_PadOuterX;
    const float actionsW = fh * 2.0f + ImGui::GetStyle().ItemInnerSpacing.x;
    if (!ImGui::BeginTable("###ge_discovery", 5, flags, ImVec2(0.0f, rowH * static_cast<float>(visibleRows)))) return;
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("Type id", ImGuiTableColumnFlags_WidthStretch, 1.0f);
    ImGui::TableSetupColumn("Radius", ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize("Radius").x);
    ImGui::TableSetupColumn("Side", ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize("Friendly").x);
    ImGui::TableSetupColumn("Seen", ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize("0000").x);
    ImGui::TableSetupColumn("###act", ImGuiTableColumnFlags_WidthFixed, actionsW);
    ImGui::TableHeadersRow();

    for (size_t i = 0; i < rows.size(); ++i) {
        const DiscoveryRow& r = rows[i];
        ImGui::PushID(static_cast<int>(i));
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(r.typeId.empty() ? "(no type id)" : r.typeId.c_str());
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 34.0f);
            ImGui::Text("End: %s", r.endEffect.empty() ? "-" : r.endEffect.c_str());
            ImGui::Text("Visual 1: %s", r.buffVisual1.empty() ? "-" : r.buffVisual1.c_str());
            ImGui::Text("Visual 2: %s", r.buffVisual2.empty() ? "-" : r.buffVisual2.c_str());
            ImGui::Text("Model: %s", r.aoFile.empty() ? "-" : r.aoFile.c_str());
            ImGui::Text("Path: %s", r.path.c_str());
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
        ImGui::TableNextColumn();
        ImGui::Text("%.0f", r.radius);
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(r.positionedValid ? (r.friendly ? "Friendly" : "Hostile") : "Unknown");
        ImGui::TableNextColumn();
        ImGui::Text("%d", r.seen);
        ImGui::TableNextColumn();
        if (PluginUi::SubtleButton(ICON_FA_COPY "###copy", ImVec2(fh, fh)))
            ImGui::SetClipboardText(r.typeId.c_str());
        Tip("Copy the type id.");
        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        ImGui::BeginDisabled(r.typeId.empty() || s.rules.size() >= Limits::kMaxRules);
        if (PluginUi::SubtleButton(ICON_FA_PLUS "###addrule", ImVec2(fh, fh))) {
            Rule rule;
            rule.label = r.typeId;
            rule.match = r.typeId;
            s.rules.push_back(rule);
            out.changed = out.rulesChanged = true;
        }
        Tip("Add a rule for this type id at the bottom of the rule list.");
        ImGui::EndDisabled();
        ImGui::PopID();
    }
    ImGui::EndTable();
}

void DrawSectionDiagnostics(PanelContext& pc, PanelResult& out) {
    Settings& s = pc.settings;
    Scanner& scanner = pc.scanner;
    PluginUi::BeginCard("###ge_card_diag");
    ImGui::PushStyleColor(ImGuiCol_Text, PluginUi::Colors().Accent);
    const bool open = ImGui::TreeNodeEx(ICON_FA_BUG "  Diagnostics###ge_diag",
                                        ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth);
    ImGui::PopStyleColor();
    Tip("Numbers and tools for checking what the plugin sees.\nNot needed for normal play.");
    if (open) {
        const ScanStats& st = scanner.Stats();
        char line[192];
        std::snprintf(line, sizeof(line), "Marked now: %d. Drawn this frame: %d (%.2f ms).",
                      static_cast<int>(scanner.Markers().size()), pc.overlay.drawn, pc.overlay.lastFrameUs / 1000.0);
        PluginUi::Hint(line);
        std::snprintf(line, sizeof(line), "Last scan: %d entities, %d component reads, %.2f ms over %d frame(s).",
                      static_cast<int>(st.lastPassEntities), st.componentReads, st.lastPassMs, st.lastPassFrames);
        PluginUi::Hint(line);
        std::snprintf(line, sizeof(line), "Entities remembered in this area: %d.", static_cast<int>(st.knownEntities));
        PluginUi::Hint(line);
        ImGui::Spacing();

        if (PluginUi::Check("Fast path filter", "ge_prefilter", &s.usePathPrefilter,
                            "Read only entities whose metadata path looks like a ground effect.\n"
                            "Turn it off only if an effect you see in game never shows up in the discovery list.")) {
            out.changed = true;
            scanner.Reset();
        }

        bool discovery = scanner.Discovery();
        if (PluginUi::Check("Discover effects", "ge_discover", &discovery,
                            "List every kind of ground effect near you this session, with its type id.\n"
                            "Use it to write rules for effects the defaults do not cover.\n"
                            "Not saved: it is off again next time."))
            scanner.SetDiscovery(discovery);
        SameLineIfFitsCheckbox("Write a log file");
        ImGui::BeginDisabled(!discovery);
        bool log = scanner.DiscoveryLog();
        if (PluginUi::Check("Write a log file", "ge_log", &log,
                            "Append each newly seen kind once to\n"
                            "config/GroundEffectsDiscovery.log in the plugin folder."))
            scanner.SetDiscoveryLog(log, pc.discoveryLogFile);
        ImGui::EndDisabled();
        SameLineIfFitsWidth(PluginUi::ButtonWidth("Clear list###ge_disc_clear"));
        if (PluginUi::SubtleButton("Clear list###ge_disc_clear")) scanner.ClearDiscovery();
        Tip("Empty the discovery list.\nEffects still around are listed again.");

        ImGui::Spacing();
        DrawDiscoveryTable(scanner, s, out);
    }
    PluginUi::EndCard();
}

} // namespace

void DrawSettingsPanel(PanelContext& pc, PanelResult& out) {
    if (!pc.hostCompatible) {
        ImGui::PushStyleColor(ImGuiCol_Text, PluginUi::Colors().Warning);
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextUnformatted("This POEFixer version is too old for Ground Effects: nothing is drawn. "
                               "Update POEFixer to use the plugin.");
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
}

} // namespace GroundEffects
