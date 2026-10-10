#pragma once
#include "settings/Options.h"
#include "ui/SearchQuery.h"
#include "ui/HudElement.h"
#include <algorithm>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace lamium::ui {
struct FeatureInfo {
    std::string_view id, name, description, toggle;
    bool experimental = false;
    std::optional<input::Action> primary = std::nullopt;
};
// Seeing first (camera, HUD and overlays, map), then doing (inventory,
// actions), then General (L-83, decided 2026-10-01).
inline constexpr auto sections = std::to_array<std::string_view>({
    "section.camera", "section.information", "section.map", "section.schematic", "section.inventory",
    "section.interaction", "section.interface"});
inline constexpr std::string_view featureSection(std::string_view id) {
    if (id == "zoom" || id == "freelook" || id == "freecamera" || id == "nightVision" || id == "hideOffhand" || id == "hideEffects" || id == "connectedTextures") return "section.camera";
    if (id == "previews" || id == "durability" || id == "foodValues" || id == "lockedTrades" || id == "englishSearch" || id == "sorting" || id == "transfer" || id == "toolSwitch" || id == "weaponSwitch" || id == "handRestock" || id == "fakeOffhand" || id == "offhandSwap" || id == "deathRestore") return "section.inventory";
    if (id == "restrictions" || id == "permanentSneak" || id == "permanentSprint" || id == "edgeGuard" || id == "toolGuard" || id == "elytraSwap" || id == "periodicAttack" || id == "periodicUse") return "section.interaction";
    if (id == "settings" || id == "toasts" || id == "hudText") return "section.interface";
    if (id.starts_with("schematic")) return "section.schematic";
    if (id == "minimap" || id == "mapText" || id == "caveView" || id == "radar" || id == "waypoints" || id == "worldMap") return "section.map";
    return "section.information";
}
inline constexpr auto features = std::to_array<FeatureInfo>({
    // Session features: no saved switch; the state column shows and flips
    // whether they are wanted (input::sessionState, BACKLOG L-47).
    {"zoom", "feature.zoom", "help.zoom", "", false, input::Action::Zoom},
    {"freelook", "feature.freelook", "help.freelook", "", false, input::Action::Freelook},
    {"freecamera", "feature.freecamera", "help.freecamera", "", true, input::Action::FreeCamera},
    {"nightVision", "feature.nightVision", "help.nightVision", "lighting.nightVision", false, input::Action::NightVision},
    {"hideOffhand", "feature.hideOffhand", "help.hideOffhand", "visuals.hideOffhand", false, input::Action::HideOffhand},
    {"hideEffects", "feature.hideEffects", "help.hideEffects", "visuals.hideEffects", true, input::Action::ToggleHideEffects},
    {"connectedTextures", "feature.connectedTextures", "help.connectedTextures", "visuals.connectedTextures", true, input::Action::ToggleConnectedTextures},
    {"previews", "feature.previews", "help.previews", "inspection.containerPreviews", false, input::Action::TogglePreviews},
    {"durability", "feature.durability", "help.durability", "inspection.durability", false, input::Action::ToggleDurability},
    {"foodValues", "feature.foodValues", "help.foodValues", "inspection.foodValues", false, input::Action::ToggleFoodValues},
    {"lockedTrades", "feature.lockedTrades", "help.lockedTrades", "inspection.lockedTrades", false, input::Action::ToggleLockedTrades},
    {"englishSearch", "feature.englishSearch", "help.englishSearch", "inspection.englishSearch", false, input::Action::ToggleEnglishSearch},
    {"sorting", "feature.sorting", "help.sorting", "inventory.sorting", false, input::Action::ToggleSorting},
    {"transfer", "feature.transfer", "help.transfer", "inventory.transfer", true, input::Action::Transfer},
    {"toolSwitch", "feature.toolSwitch", "help.toolSwitch", "inventory.toolSwitch", false, input::Action::ToolSwitch},
    {"weaponSwitch", "feature.weaponSwitch", "help.weaponSwitch", "inventory.weaponSwitch", false, input::Action::WeaponSwitch},
    {"handRestock", "feature.handRestock", "help.handRestock", "inventory.handRestock", true, input::Action::HandRestock},
    {"fakeOffhand", "feature.fakeOffhand", "help.fakeOffhand", "inventory.fakeOffhand", true, input::Action::FakeOffhand},
    {"offhandSwap", "feature.offhandSwap", "help.offhandSwap", "inventory.offhandSwap", false, input::Action::ToggleOffhandSwap},
    {"deathRestore", "feature.deathRestore", "help.deathRestore", "inventory.deathRestore", true, input::Action::ToggleDeathRestore},
    {"restrictions", "feature.restrictions", "help.restrictions", ""},
    {"permanentSneak", "feature.permanentSneak", "help.permanentSneak", "", false, input::Action::PermanentSneak},
    {"permanentSprint", "feature.permanentSprint", "help.permanentSprint", "", false, input::Action::PermanentSprint},
    {"edgeGuard", "feature.edgeGuard", "help.edgeGuard", "interaction.edgeGuard", true, input::Action::EdgeGuard},
    {"toolGuard", "feature.toolGuard", "help.toolGuard", "interaction.toolGuard", true, input::Action::ToolGuard},
    {"elytraSwap", "feature.elytraSwap", "help.elytraSwap", "interaction.elytraSwap", true, input::Action::ElytraSwap},
    {"periodicAttack", "feature.periodicAttack", "help.periodicInput", "interaction.autoAttack", false, input::Action::PeriodicAttack},
    {"periodicUse", "feature.periodicUse", "help.periodicInput", "interaction.autoUse", false, input::Action::PeriodicUse},
    {"infoHud", "feature.infoHud", "help.infoHud", "information.hud", false, input::Action::InfoHud},
    {"targetInfo", "feature.targetInfo", "help.targetInfo", "information.target", false, input::Action::TargetInfo},
    {"playerList", "feature.playerList", "help.playerList", "", false, input::Action::PlayerList},
    {"durabilityHud", "feature.durabilityHud", "help.durabilityHud", "information.durabilityHud", false, input::Action::ToggleDurabilityHud},
    {"inventoryHud", "feature.inventoryHud", "help.inventoryHud", "information.inventoryHud", false, input::Action::ToggleInventoryHud},
    {"usedSlots", "feature.usedSlots", "help.usedSlots", "information.usedSlots", false, input::Action::ToggleUsedSlots},
    {"offhandSlot", "feature.offhandSlot", "help.offhandSlot", "information.offhandSlot", false, input::Action::ToggleOffhandSlot},
    {"saturation", "feature.saturation", "help.saturation", "information.saturation", false, input::Action::ToggleSaturation},
    {"debugView", "feature.debugView", "help.debugView", "information.debug", false, input::Action::DebugView},
    {"chunkBorders", "feature.chunkBorders", "help.chunkBorders", "overlays.chunkBorders", false, input::Action::ChunkBorders},
    {"hitboxes", "feature.hitboxes", "help.hitboxes", "overlays.hitboxes", false, input::Action::Hitboxes},
    {"lightOverlay", "feature.lightOverlay", "help.lightOverlay", "overlays.light", false, input::Action::LightOverlay},
    {"shapes", "feature.shapes", "help.shapes", "overlays.shapes", false, input::Action::ToggleShapes},
    {"minimap", "feature.minimap", "help.minimap", "map.minimap", true, input::Action::Minimap},
    // A heading for the minimap's text, without a switch of its own.
    {"mapText", "feature.mapText", "help.mapText", ""},
    // Automatic; the key forces the other view.
    {"caveView", "feature.caveView", "help.caveView", "", false, input::Action::MinimapView},
    {"radar", "feature.radar", "help.radar", "map.radar", true, input::Action::ToggleRadar},
    {"waypoints", "feature.waypoints", "help.waypoints", "map.waypoints", true, input::Action::ToggleWaypoints},
    {"worldMap", "feature.worldMap", "help.worldMap", "map.worldMap", true, input::Action::ToggleWorldMap},
    {"schematic", "feature.schematic", "help.schematic", "schematic.enabled", true, input::Action::ToggleSchematic},
    // The menu and the adjust key first: where to start (L-93, 2026-10-07).
    {"schematicMenu", "feature.schematicMenu", "help.schematicMenu", "", false, input::Action::SchematicMenu},
    {"schematicHud", "feature.schematicHud", "help.schematicHud", "schematic.hud", true, input::Action::ToggleSchematicHud},
    // Key groups without a switch of their own (DESIGN: a keyless group heading).
    {"schematicPlacement", "feature.schematicPlacement", "help.schematicPlacement", ""},
    {"schematicLayers", "feature.schematicLayers", "help.schematicLayers", ""},
    {"schematicCheck", "feature.schematicCheck", "help.schematicCheck", ""},
    {"schematicSave", "feature.schematicSave", "help.schematicSave", ""},
    {"automationStatus", "feature.automationStatus", "help.automationStatus", "interface.automationStatus", false, input::Action::ToggleAutomationStatus},
    {"settings", "feature.settings", "help.settings", "", false, input::Action::Settings},
    // Headings without a switch: their first row is the switch (no key of its own).
    {"toasts", "feature.toasts", "help.toasts", ""},
    // Text of the line elements (Info HUD, Status, Debug View) and HUD backgrounds.
    {"hudText", "feature.hudText", "help.hudText", ""},
});
// Display order of action rows where catalog order (fixed: ids are saved)
// would scatter related keys; others keep catalog order after these.
inline int actionRank(input::Action action) {
    using A = input::Action;
    static constexpr A order[] = {A::SchematicMenu, A::AdjustSchematic, A::OpenSchematics, A::OpenSchematicFiles, A::OpenSchematicPlaced, A::OpenSchematicCheck,
        A::OpenSchematicMaterials,
        A::SelectLookedPlacement, A::NextPlacement, A::MovePlacementForward, A::MovePlacementBack, A::MovePlacementLeft,
        A::MovePlacementRight, A::MovePlacementUp, A::MovePlacementDown, A::MovePlacementHere, A::RotatePlacement,
        A::MirrorPlacement, A::LayerUp, A::LayerDown, A::LayerHere, A::SchematicCorner1, A::SchematicCorner2, A::SaveSchematicArea};
    for (size_t i = 0; i < std::size(order); ++i) if (order[i] == action) return static_cast<int>(i);
    return static_cast<int>(std::size(order)) + static_cast<int>(action);
}
// A feature row only carries the binding named by that row. Other actions
// remain under the feature, even if they were registered first.
inline std::optional<input::Action> primaryAction(FeatureInfo const& feature) {
    return feature.primary;
}

// Option rows that carry a hotkey in their own key cell, so a setting and
// the key that changes it read as one item. Such an action gets no row of its
// own under the feature; Hotkeys still lists it.
inline std::optional<input::Action> optionAction(std::string_view option) {
    if (option == "visuals.hideWeather") return input::Action::HideWeather;
    if (option == "visuals.hideParticles") return input::Action::HideParticles;
    if (option == "visuals.hideBossBars") return input::Action::HideBossBars;
    if (option == "visuals.hideNausea") return input::Action::HideNausea;
    if (option == "visuals.hideWater") return input::Action::HideWater;
    if (option == "visuals.hideLava") return input::Action::HideLava;
    if (option == "visuals.hidePowderSnow") return input::Action::HidePowderSnow;
    if (option == "visuals.hideDistanceFog") return input::Action::HideDistanceFog;
    if (option == "interaction.breaking") return input::Action::BreakingRestriction;
    if (option == "interaction.attackMode") return input::Action::CycleAttackMode;
    if (option == "interaction.useMode") return input::Action::CycleUseMode;
    if (option == "interaction.attackHeldOnly") return input::Action::AttackHeldOnly;
    if (option == "interaction.useHeldOnly") return input::Action::UseHeldOnly;
    if (option == "interaction.breakingMode") return input::Action::CycleBreakingMode;
    if (option == "map.waypointsWorld") return input::Action::HideWaypoints;
    if (option == "map.radarFaces") return input::Action::RadarFaces;
    return {};
}
inline bool shownOnOption(input::Action action) {
    return std::any_of(settings::options.begin(), settings::options.end(),
        [&](settings::Option const& option) { return optionAction(option.id) == action; });
}
// An option whose value collides with another setting (L-97): its stepper is
// drawn in the warning color and its description explains, like a hotkey
// conflict. A fetch slot equal to Fake Offhand's target slot is not used.
inline std::optional<std::string_view> optionWarning(std::string_view option, Settings const& value) {
    auto const& inv = value.inventory;
    if (!inv.fakeOffhand) return {};
    bool tool = inv.toolSwitch && inv.toolSwitchInventory && inv.toolSwitchSlot == inv.fakeOffhandSlot;
    bool weapon = inv.weaponSwitch && inv.weaponSwitchInventory && inv.weaponSwitchSlot == inv.fakeOffhandSlot;
    if ((option == "inventory.toolSwitchSlot" && tool) || (option == "inventory.weaponSwitchSlot" && weapon))
        return "warning.fetchSlotFakeOffhand";
    if (option == "inventory.fakeOffhandSlot" && (tool || weapon)) return "warning.fakeOffhandSlotFetch";
    return {};
}
inline bool effectsPaused(std::string_view feature, Settings const& value) {
    return feature == "hideEffects" && !value.visuals.hideEffects;
}

// MapCache: the world map's saved data for this world, with a delete button.
enum class RowKind { Section, Feature, Option, Action, Layout, MapCache };
// HUD features link to their element in the layout editor instead of listing
// placement and look rows (DESIGN "HUD").
inline std::optional<HudElementId> layoutElement(std::string_view feature) {
    if (feature == "infoHud") return HudElementId::Info;
    if (feature == "targetInfo") return HudElementId::Target;
    if (feature == "automationStatus") return HudElementId::Status;
    if (feature == "toasts") return HudElementId::Toast;
    if (feature == "zoom") return HudElementId::Magnification;
    if (feature == "durabilityHud") return HudElementId::Durability;
    if (feature == "minimap") return HudElementId::Minimap;
    if (feature == "schematicHud") return HudElementId::Schematic;
    if (feature == "playerList") return HudElementId::PlayerList;
    if (feature == "inventoryHud") return HudElementId::Inventory;
    if (feature == "usedSlots") return HudElementId::UsedSlots;
    return std::nullopt;
}
inline constexpr std::string_view layoutLinkLabel(HudElementId id) {
    return id == HudElementId::Toast ? "layoutLinkToast"
        : id == HudElementId::Magnification ? "layoutLinkMagnification" : "layoutLink";
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
    bool child() const {
        return kind == RowKind::Option || kind == RowKind::Action || kind == RowKind::Layout || kind == RowKind::MapCache;
    }
    bool operator==(SettingsRow const& other) const {
        return kind == other.kind && feature == other.feature && option == other.option
            && action == other.action && section == other.section && layout == other.layout;
    }
};
// Category is a section key, or empty for all sections. A query searches every
// section and initially expands features whose settings, but not name, match it.
// Presentation independent of Minecraft objects, so it is testable without rendering.
template<class Translate>
std::vector<SettingsRow> buildSettingsRows(bool hotkeys, std::string_view category, SearchQuery const& query,
    std::set<std::string_view> const& expanded, Translate translate,
    std::vector<std::string> const& lineOrder = {},
    std::set<std::string_view> const& searchCollapsed = {}) {
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
            std::vector<size_t> actionOrder;
            for (size_t i = 0; i < input::actions.size(); ++i)
                if (input::actions[i].feature == feature.id) actionOrder.push_back(i);
            std::stable_sort(actionOrder.begin(), actionOrder.end(), [](size_t a, size_t b) {
                return actionRank(static_cast<input::Action>(a)) < actionRank(static_cast<input::Action>(b));
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
            if (feature.id == "sorting" || feature.id == "fakeOffhand" || feature.id == "offhandSwap")
                std::stable_partition(children.begin(), children.end(), [](SettingsRow const& row) {
                    return row.action == input::Action::Sort || row.action == input::Action::FakeOffhandUse
                        || row.action == input::Action::SwapOffhand;
                });
            // Info lines follow the user-ordered list; every other child
            // keeps catalog order (stable). Unknown ids sort last.
            if (!lineOrder.empty()) {
                auto orderKey = [&](SettingsRow const& row) {
                    if (!row.option) return lineOrder.size();
                    constexpr std::string_view prefix = "information.";
                    if (!row.option->id.starts_with(prefix)) return lineOrder.size();
                    auto id = row.option->id.substr(prefix.size());
                    if (id == "biomeDisplay") id = "biome";
                    if (id == "realTimeDisplay") id = "realTime";
                    auto at = std::find(lineOrder.begin(), lineOrder.end(), id);
                    return at == lineOrder.end() ? lineOrder.size()
                                                 : static_cast<size_t>(at - lineOrder.begin());
                };
                std::stable_sort(children.begin(), children.end(),
                    [&](SettingsRow const& a, SettingsRow const& b) { return orderKey(a) < orderKey(b); });
            }
            if (auto element = layoutElement(feature.id))
                children.push_back({RowKind::Layout, &feature, nullptr, {}, section, element});
            if (feature.id == "worldMap") children.push_back({RowKind::MapCache, &feature, nullptr, {}, section});
            auto childText = [&](SettingsRow const& row) {
                if (row.kind == RowKind::MapCache) return std::string(translate("mapCache"));
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
            bool open = !children.empty() && (expanded.contains(feature.id)
                || (!self && !searchCollapsed.contains(feature.id)));
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
inline std::string actionTranslationKey(input::Action action, bool child = false) {
    if (child && action == input::Action::FreeCameraSpeedUp) return "freeCameraSpeedUp";
    if (child && action == input::Action::FreeCameraSpeedDown) return "freeCameraSpeedDown";
    return "key.Lamium." + std::string(input::actions[static_cast<size_t>(action)].id);
}
// Reset a settings category: every option owned by its features and the HUD
// placement of their elements. Key bindings stay; Hotkeys has its own reset.
inline void resetSection(Settings& value, std::string_view section) {
    Settings const defaults{};
    for (auto const& option : settings::options)
        if (featureSection(option.feature) == section) settings::resetOption(value, option, defaults);
    for (auto const& feature : features)
        if (featureSection(feature.id) == section)
            if (auto element = layoutElement(feature.id))
                settings::hudElement(value, *element) = settings::hudElement(defaults, *element);
}
}
