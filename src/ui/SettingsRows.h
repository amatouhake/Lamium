#pragma once
#include "settings/Options.h"
#include "ui/SearchQuery.h"
#include "ui/HudElement.h"
#include <algorithm>
#include <set>
#include <string>
#include <vector>

namespace lamium::ui {
struct FeatureInfo { std::string_view id, name, description, toggle; bool experimental = false; };
inline constexpr auto sections = std::to_array<std::string_view>({
    "section.camera", "section.inventory", "section.interaction", "section.information", "section.interface"});
inline constexpr std::string_view featureSection(std::string_view id) {
    if (id == "zoom" || id == "freelook" || id == "freecamera" || id == "nightVision" || id == "hideOffhand") return "section.camera";
    if (id == "previews" || id == "durability" || id == "sorting" || id == "toolSwitch" || id == "handRestock") return "section.inventory";
    if (id == "restrictions" || id == "permanentSneak" || id == "permanentSprint" || id == "periodicAttack" || id == "periodicUse") return "section.interaction";
    if (id == "settings" || id == "automationStatus") return "section.interface";
    return "section.information";
}
inline constexpr auto features = std::to_array<FeatureInfo>({
    {"zoom", "feature.zoom", "help.zoom", "camera.zoom"},
    {"freelook", "feature.freelook", "help.freelook", "camera.freelook", true},
    {"freecamera", "feature.freecamera", "help.freecamera", "camera.freecamera", true},
    {"nightVision", "feature.nightVision", "help.nightVision", "lighting.nightVision"},
    {"hideOffhand", "feature.hideOffhand", "help.hideOffhand", "visuals.hideOffhand"},
    {"previews", "feature.previews", "help.previews", "inspection.containerPreviews"},
    {"durability", "feature.durability", "help.durability", "inspection.durability"},
    {"sorting", "feature.sorting", "help.sorting", "inventory.sorting"},
    {"toolSwitch", "feature.toolSwitch", "help.toolSwitch", "inventory.toolSwitch"},
    {"handRestock", "feature.handRestock", "help.handRestock", "inventory.handRestock", true},
    {"restrictions", "feature.restrictions", "help.restrictions", ""},
    {"permanentSneak", "feature.permanentSneak", "help.permanentSneak", "", true},
    {"permanentSprint", "feature.permanentSprint", "help.permanentSprint", "", true},
    {"periodicAttack", "feature.periodicAttack", "help.periodicInput", "interaction.autoAttack", true},
    {"periodicUse", "feature.periodicUse", "help.periodicInput", "interaction.autoUse", true},
    {"infoHud", "feature.infoHud", "help.infoHud", "information.hud"},
    {"targetInfo", "feature.targetInfo", "help.targetInfo", "information.target"},
    {"debugView", "feature.debugView", "help.debugView", "information.debug"},
    {"chunkBorders", "feature.chunkBorders", "help.chunkBorders", "overlays.chunkBorders"},
    {"hitboxes", "feature.hitboxes", "help.hitboxes", "overlays.hitboxes"},
    {"lightOverlay", "feature.lightOverlay", "help.lightOverlay", "overlays.light", true},
    {"shapes", "feature.shapes", "help.shapes", "overlays.shapes"},
    {"automationStatus", "feature.automationStatus", "help.automationStatus", "interface.automationStatus"},
    {"settings", "feature.settings", "help.settings", ""},
});
// The first action of a feature is its main binding, shown on the feature row.
inline std::optional<input::Action> primaryAction(FeatureInfo const& feature) {
    for (size_t i = 0; i < input::actions.size(); ++i)
        if (input::actions[i].feature == feature.id) return static_cast<input::Action>(i);
    return {};
}

// Option rows that carry a hotkey in their own key cell, so a setting and
// the key that changes it read as one item. Such an action gets no row of its
// own under the feature; Hotkeys still lists it.
inline std::optional<input::Action> optionAction(std::string_view option) {
    if (option == "interaction.attackMode") return input::Action::CycleAttackMode;
    if (option == "interaction.useMode") return input::Action::CycleUseMode;
    if (option == "interaction.attackHeldOnly") return input::Action::AttackHeldOnly;
    if (option == "interaction.useHeldOnly") return input::Action::UseHeldOnly;
    if (option == "interaction.breakingMode") return input::Action::CycleBreakingMode;
    return {};
}
inline bool shownOnOption(input::Action action) {
    return std::any_of(settings::options.begin(), settings::options.end(),
        [&](settings::Option const& option) { return optionAction(option.id) == action; });
}

enum class RowKind { Section, Feature, Option, Action, Layout };
// HUD features link to their element in the layout editor instead of listing
// placement and look rows (DESIGN "HUD").
inline std::optional<HudElementId> layoutElement(std::string_view feature) {
    if (feature == "infoHud") return HudElementId::Info;
    if (feature == "targetInfo") return HudElementId::Target;
    if (feature == "automationStatus") return HudElementId::Status;
    if (feature == "settings") return HudElementId::Toast;
    return std::nullopt;
}
inline constexpr std::string_view layoutLinkLabel(HudElementId id) {
    return id == HudElementId::Toast ? "layoutLinkToast" : "layoutLink";
}
struct SettingsRow {
    RowKind kind;
    FeatureInfo const* feature = nullptr;
    settings::Option const* option = nullptr;
    std::optional<input::Action> action;
    std::string_view section;
    std::optional<HudElementId> layout; // Layout rows
    int children = 0;       // Feature rows: expandable content
    bool expanded = false;  // Feature rows
    bool lastChild = false; // Child rows: end of the tree guide
    bool heading() const { return kind == RowKind::Feature; }
    bool selectable() const { return kind != RowKind::Section; }
    bool child() const { return kind == RowKind::Option || kind == RowKind::Action || kind == RowKind::Layout; }
    bool operator==(SettingsRow const& other) const {
        return kind == other.kind && feature == other.feature && option == other.option
            && action == other.action && section == other.section && layout == other.layout;
    }
};
// Category is a section key, or empty for all sections. A query searches every
// section and expands features whose settings, but not name, match it.
// Presentation independent of Minecraft objects, so it is testable without rendering.
template<class Translate>
std::vector<SettingsRow> buildSettingsRows(bool hotkeys, std::string_view category, SearchQuery const& query,
    std::set<std::string_view> const& expanded, Translate translate,
    std::vector<std::string> const& lineOrder = {}) {
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
            // Action registration order is frozen for save compatibility, but
            // the settings openers read better with Hotkeys above Shapes,
            // matching the sidebar. This presentation exception lives here.
            std::vector<size_t> actionOrder;
            for (size_t i = 0; i < input::actions.size(); ++i)
                if (input::actions[i].feature == feature.id) actionOrder.push_back(i);
            if (feature.id == "settings")
                std::stable_sort(actionOrder.begin(), actionOrder.end(), [](size_t a, size_t b) {
                    auto key = [](size_t i) {
                        return i == static_cast<size_t>(input::Action::OpenShapes) ? i + 3 : i;
                    };
                    return key(a) < key(b);
                });
            if (hotkeys) {
                for (size_t i : actionOrder) {
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
                if (option.id.starts_with("hud.")) continue; // Edited in the layout editor.
                children.push_back({RowKind::Option, &feature, &option, {}, section});
            }
            for (size_t i : actionOrder) {
                auto action = static_cast<input::Action>(i);
                if (input::actions[i].feature == feature.id && action != primary && !shownOnOption(action))
                    children.push_back({RowKind::Action, &feature, nullptr, action, section});
            }
            // Info lines follow the user-ordered list; every other child
            // keeps catalog order (stable). Unknown ids sort last.
            if (!lineOrder.empty()) {
                auto orderKey = [&](SettingsRow const& row) {
                    if (!row.option) return lineOrder.size();
                    constexpr std::string_view prefix = "information.";
                    if (!row.option->id.starts_with(prefix)) return lineOrder.size();
                    auto id = row.option->id.substr(prefix.size());
                    auto at = std::find(lineOrder.begin(), lineOrder.end(), id);
                    return at == lineOrder.end() ? lineOrder.size()
                                                 : static_cast<size_t>(at - lineOrder.begin());
                };
                std::stable_sort(children.begin(), children.end(),
                    [&](SettingsRow const& a, SettingsRow const& b) { return orderKey(a) < orderKey(b); });
            }
            if (auto element = layoutElement(feature.id))
                children.push_back({RowKind::Layout, &feature, nullptr, {}, section, element});
            auto childText = [&](SettingsRow const& row) {
                if (row.layout) return std::string(translate(layoutLinkLabel(*row.layout))) + " " + translate("nav.hudLayout");
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
// Action labels carry a catalog prefix that the settings UI strips.
inline std::string actionName(std::string label) {
    constexpr std::string_view prefix = "Lamium: ";
    if (label.starts_with(prefix)) label.erase(0, prefix.size());
    return label;
}
}
