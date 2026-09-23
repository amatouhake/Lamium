#pragma once
#include "settings/Options.h"
#include "ui/SearchQuery.h"
#include <set>
#include <string>
#include <vector>

namespace lamium::ui {
struct FeatureInfo { std::string_view id, name, description, toggle; bool experimental = false; };
inline constexpr auto sections = std::to_array<std::string_view>({
    "section.camera", "section.inventory", "section.interaction", "section.information", "section.interface"});
inline constexpr std::string_view featureSection(std::string_view id) {
    if (id == "zoom" || id == "freelook" || id == "nightVision" || id == "hideOffhand") return "section.camera";
    if (id == "previews" || id == "durability" || id == "sorting" || id == "toolSwitch" || id == "handRestock") return "section.inventory";
    if (id == "restrictions" || id == "permanentSneak" || id == "periodicAttack" || id == "periodicUse") return "section.interaction";
    if (id == "settings" || id == "gameplayHints" || id == "automationStatus") return "section.interface";
    return "section.information";
}
inline constexpr auto features = std::to_array<FeatureInfo>({
    {"zoom", "feature.zoom", "help.zoom", "camera.zoom"},
    {"freelook", "feature.freelook", "help.freelook", "camera.freelook", true},
    {"nightVision", "feature.nightVision", "help.nightVision", "lighting.nightVision"},
    {"hideOffhand", "feature.hideOffhand", "help.hideOffhand", "visuals.hideOffhand"},
    {"previews", "feature.previews", "help.previews", "inspection.containerPreviews"},
    {"durability", "feature.durability", "help.durability", "inspection.durability"},
    {"sorting", "feature.sorting", "help.sorting", "inventory.sorting"},
    {"toolSwitch", "feature.toolSwitch", "help.toolSwitch", "inventory.toolSwitch"},
    {"handRestock", "feature.handRestock", "help.handRestock", "inventory.handRestock", true},
    {"restrictions", "feature.restrictions", "help.restrictions", ""},
    {"permanentSneak", "feature.permanentSneak", "help.permanentSneak", "", true},
    {"periodicAttack", "feature.periodicAttack", "help.periodicInput", "", true},
    {"periodicUse", "feature.periodicUse", "help.periodicInput", "", true},
    {"infoHud", "feature.infoHud", "help.infoHud", "information.hud"},
    {"targetInfo", "feature.targetInfo", "help.targetInfo", "information.target"},
    {"debugView", "feature.debugView", "help.debugView", "information.debug"},
    {"chunkBorders", "feature.chunkBorders", "help.chunkBorders", "overlays.chunkBorders"},
    {"hitboxes", "feature.hitboxes", "help.hitboxes", "overlays.hitboxes"},
    {"lightOverlay", "feature.lightOverlay", "help.lightOverlay", "overlays.light", true},
    {"shapes", "shape.manager", "shape.description", ""},
    {"gameplayHints", "feature.gameplayHints", "help.gameplayHints", "interface.gameplayHints"},
    {"automationStatus", "feature.automationStatus", "help.automationStatus", "interface.automationStatus"},
    {"settings", "feature.settings", "help.settings", ""},
});
// The Shape Manager is a dedicated screen opened from its feature row.
inline constexpr bool isTool(FeatureInfo const& feature) { return feature.id == "shapes"; }
// The first action of a feature is its main binding, shown on the feature row.
inline std::optional<input::Action> primaryAction(FeatureInfo const& feature) {
    for (size_t i = 0; i < input::actions.size(); ++i)
        if (input::actions[i].feature == feature.id) return static_cast<input::Action>(i);
    return {};
}

enum class RowKind { Section, Feature, Option, Action };
struct SettingsRow {
    RowKind kind;
    FeatureInfo const* feature = nullptr;
    settings::Option const* option = nullptr;
    std::optional<input::Action> action;
    std::string_view section;
    int children = 0;       // Feature rows: expandable content
    bool expanded = false;  // Feature rows
    bool lastChild = false; // Child rows: end of the tree guide
    bool heading() const { return kind == RowKind::Feature; }
    bool selectable() const { return kind != RowKind::Section; }
    bool child() const { return kind == RowKind::Option || kind == RowKind::Action; }
    bool operator==(SettingsRow const& other) const {
        return kind == other.kind && feature == other.feature && option == other.option
            && action == other.action && section == other.section;
    }
};
// Category is a section key, or empty for all sections. A query searches every
// section and expands features whose settings, but not name, match it.
// Presentation independent of Minecraft objects, so it is testable without rendering.
template<class Translate>
std::vector<SettingsRow> buildSettingsRows(bool hotkeys, std::string_view category, SearchQuery const& query,
    std::set<std::string_view> const& expanded, Translate translate) {
    std::vector<SettingsRow> rows;
    bool const searching = query.value().find_first_not_of(' ') != std::string::npos;
    if (searching) category = {};
    std::string_view lastSection;
    auto sectionRow = [&](std::string_view section) {
        if (category.empty() && section != lastSection) rows.push_back({RowKind::Section, nullptr, nullptr, {}, section});
        lastSection = section;
    };
    for (auto const& section : sections) {
        if (!category.empty() && category != section) continue;
        for (auto const& feature : features) {
            if (featureSection(feature.id) != section) continue;
            std::string scope = std::string(feature.id) + " " + translate(feature.name) + " "
                + translate(feature.description) + " " + translate(section);
            auto primary = primaryAction(feature);
            if (hotkeys) {
                for (size_t i = 0; i < input::actions.size(); ++i) {
                    auto const& action = input::actions[i];
                    if (action.feature != feature.id || !query.matches(scope + " " + std::string(action.id) + " "
                        + translate("key.Lamium." + std::string(action.id)))) continue;
                    sectionRow(section);
                    rows.push_back({RowKind::Action, &feature, nullptr, static_cast<input::Action>(i), section});
                }
                continue;
            }
            std::vector<SettingsRow> children;
            for (auto const& option : settings::options) {
                if (option.feature != feature.id || option.id == feature.toggle) continue;
                children.push_back({RowKind::Option, &feature, &option, {}, section});
            }
            for (size_t i = 0; i < input::actions.size(); ++i) {
                auto action = static_cast<input::Action>(i);
                if (input::actions[i].feature == feature.id && action != primary)
                    children.push_back({RowKind::Action, &feature, nullptr, action, section});
            }
            auto childText = [&](SettingsRow const& row) {
                return row.option ? std::string(row.option->id) + " " + translate(row.option->label)
                    : std::string(input::actions[static_cast<size_t>(*row.action)].id) + " "
                        + translate("key.Lamium." + std::string(input::actions[static_cast<size_t>(*row.action)].id));
            };
            bool self = query.matches(scope);
            std::vector<SettingsRow> matching;
            if (!self) {
                for (auto const& child : children)
                    if (query.matches(scope + " " + childText(child))) matching.push_back(child);
                if (matching.empty()) continue;
            }
            bool open = !children.empty() && (!self || expanded.contains(feature.id));
            auto const& shown = self ? children : matching;
            sectionRow(section);
            SettingsRow heading{RowKind::Feature, &feature, nullptr, {}, section};
            heading.children = static_cast<int>(children.size());
            heading.expanded = open;
            rows.push_back(heading);
            if (!open) continue;
            for (size_t i = 0; i < shown.size(); ++i) {
                rows.push_back(shown[i]);
                rows.back().lastChild = i + 1 == shown.size();
            }
        }
    }
    return rows;
}

// Option labels are "Name: {value}" patterns. Split them into a column name and
// a value pattern without adding a second catalog of translations.
struct LabelParts { std::string name, value; };
inline LabelParts splitLabel(std::string_view pattern) {
    auto split = pattern.find(": ");
    if (split == std::string_view::npos) {
        auto brace = pattern.find('{');
        std::string name(pattern.substr(0, brace));
        while (!name.empty() && name.back() == ' ') name.pop_back();
        return {name, "{}"};
    }
    return {std::string(pattern.substr(0, split)), std::string(pattern.substr(split + 2))};
}
// Action labels are registered for Minecraft's controls screen with a prefix.
inline std::string actionName(std::string label) {
    constexpr std::string_view prefix = "Lamium: ";
    if (label.starts_with(prefix)) label.erase(0, prefix.size());
    return label;
}
}
