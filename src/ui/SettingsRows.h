#pragma once
#include "settings/Options.h"
#include "ui/SearchQuery.h"
#include <set>
#include <string>
#include <vector>

namespace lamium::ui {
struct FeatureInfo { std::string_view id, name, description, toggle; };
inline constexpr auto features = std::to_array<FeatureInfo>({
    {"chunkBorders", "feature.chunkBorders", "help.chunkBorders", "overlays.chunkBorders"},
    {"zoom", "feature.zoom", "help.zoom", "camera.zoom"},
    {"nightVision", "feature.nightVision", "help.nightVision", "lighting.nightVision"},
    {"previews", "feature.previews", "help.previews", "inspection.containerPreviews"},
    {"durability", "feature.durability", "help.durability", "inspection.durability"},
    {"sorting", "feature.sorting", "help.sorting", "inventory.sorting"},
    {"gameplayHints", "feature.gameplayHints", "help.gameplayHints", "interface.gameplayHints"},
    {"settings", "feature.settings", "help.settings", ""},
});
struct SettingsRow {
    FeatureInfo const* feature;
    settings::Option const* option = nullptr;
    std::optional<input::Action> action;
    bool heading() const { return !option && !action; }
};
// Presentation independent of Minecraft objects: search/collapse behavior can
// be verified without rendering, and reused by other settings surfaces.
template<class Translate>
std::vector<SettingsRow> buildSettingsRows(bool hotkeys, SearchQuery const& query,
    std::set<std::string_view> const& collapsed, Translate translate) {
    std::vector<SettingsRow> rows;
    bool const searching = query.value().find_first_not_of(' ') != std::string::npos;
    for (auto const& feature : features) {
        std::string scope = std::string(feature.id) + " " + translate(feature.name) + " " + translate(feature.description);
        std::vector<SettingsRow> children;
        if (!hotkeys) {
            for (auto const& option : settings::options)
                if (option.feature == feature.id && query.matches(scope + " " + std::string(option.id) + " " + translate(option.label)))
                    children.push_back({&feature, &option, {}});
        }
        for (size_t i = 0; i < input::actions.size(); ++i) {
            auto const& action = input::actions[i];
            if (action.feature == feature.id && query.matches(scope + " " + std::string(action.id) + " "
                + translate("key.Lamium." + std::string(action.id))))
                children.push_back({&feature, nullptr, static_cast<input::Action>(i)});
        }
        if (children.empty()) continue;
        if (!hotkeys) rows.push_back({&feature, nullptr, {}});
        if (hotkeys || searching || !collapsed.contains(feature.id)) rows.insert(rows.end(), children.begin(), children.end());
    }
    return rows;
}
}
