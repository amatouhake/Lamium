#pragma once
#include "input/FixedPresses.h"
#include "settings/Options.h"
#include "ui/SearchQuery.h"
#include "ui/HudElement.h"
#include <algorithm>
#include <format>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <vector>

namespace lamium::ui {
struct FeatureInfo {
    std::string_view id, name, description, toggle;
    bool experimental = false;
    std::optional<input::Action> primary = std::nullopt;
};
// Two levels (L-340): the sidebar's categories, seeing first, then doing,
// then General (L-83); inside each, groups headed in green in every view. A
// category of one group names that group with its own key.
inline constexpr auto sections = std::to_array<std::string_view>({
    "section.view", "section.hud", "section.world", "section.schematic", "section.items", "section.interaction", "section.interface"});
struct GroupInfo {
    std::string_view key, section;
    std::span<std::string_view const> features; // in display order
};
namespace group_members {
inline constexpr std::string_view camera[] = {"zoom", "freelook", "freecamera"};
inline constexpr std::string_view look[] = {"nightVision", "hideOffhand", "hideEffects", "connectedTextures"};
inline constexpr std::string_view wholeHud[] = {"hudText", "toasts", "gameHud", "automationStatus"};
inline constexpr std::string_view information[] = {"infoHud", "targetInfo", "playerList", "debugView"};
inline constexpr std::string_view handStatus[] = {"durabilityHud", "inventoryHud", "usedSlots", "offhandSlot", "saturation", "statusEffects"};
inline constexpr std::string_view inWorld[] = {"chunkBorders", "hitboxes", "lightOverlay", "shapes", "blockHighlight"};
inline constexpr std::string_view map[] = {"minimap", "mapText", "mapMarks", "radar", "slimeChunks", "waypoints", "worldMap"};
inline constexpr std::string_view schematics[] = {"schematic", "schematicMenu", "schematicRepeat", "schematicLook", "schematicCheck", "schematicAssist", "schematicHud",
    "schematicMove", "schematicPlacement", "schematicLayers", "schematicShow", "schematicSave", "schematicScreen"};
inline constexpr std::string_view itemScreens[] = {"previews", "itemDetails", "englishSearch", "sorting", "transfer", "lockedTrades",
    "repeatTrades", "enchantOrder", "bookPaste"};
inline constexpr std::string_view recipesPlan[] = {"recipes", "plan", "stores", "massCraft"};
inline constexpr std::string_view handsEquipment[] = {"toolSwitch", "weaponSwitch", "handRestock", "fakeOffhand", "offhandSwap",
    "deathRestore", "toolGuard", "elytraSwap"};
inline constexpr std::string_view actions[] = {"restrictions", "permanentSneak", "permanentSprint", "edgeGuard", "sneakPlacement",
    "periodicAttack", "periodicUse"};
inline constexpr std::string_view general[] = {"settings", "gameLanguage", "savedData", "skipServerPacks", "calculator"};
}
inline constexpr auto groups = std::to_array<GroupInfo>({
    {"group.camera", "section.view", group_members::camera},
    {"group.look", "section.view", group_members::look},
    {"group.wholeHud", "section.hud", group_members::wholeHud},
    {"group.information", "section.hud", group_members::information},
    {"group.handStatus", "section.hud", group_members::handStatus},
    {"group.inWorld", "section.world", group_members::inWorld},
    {"group.map", "section.world", group_members::map},
    {"section.schematic", "section.schematic", group_members::schematics},
    {"group.itemScreens", "section.items", group_members::itemScreens},
    {"group.recipesPlan", "section.items", group_members::recipesPlan},
    {"group.actionHelpers", "section.interaction", group_members::actions},
    {"group.handsEquipment", "section.interaction", group_members::handsEquipment},
    {"section.interface", "section.interface", group_members::general},
});
// The group a feature is listed in; null for an id no group lists (a test keeps that from happening).
inline constexpr GroupInfo const* featureGroup(std::string_view id) {
    for (auto const& group : groups)
        for (auto member : group.features)
            if (member == id) return &group;
    return nullptr;
}
inline constexpr std::string_view featureSection(std::string_view id) {
    auto group = featureGroup(id);
    return group ? group->section : std::string_view{};
}
inline constexpr int groupCount(std::string_view section) {
    int count = 0;
    for (auto const& group : groups) count += group.section == section;
    return count;
}
inline constexpr auto features = std::to_array<FeatureInfo>({
    // Session features: no saved switch; the state column shows and flips
    // whether they are wanted (input::sessionState, BACKLOG L-47).
    {"zoom", "feature.zoom", "help.zoom", "", false, input::Action::Zoom},
    {"freelook", "feature.freelook", "help.freelook", "", false, input::Action::Freelook},
    {"freecamera", "feature.freecamera", "help.freecamera", "", false, input::Action::FreeCamera},
    {"nightVision", "feature.nightVision", "help.nightVision", "lighting.nightVision", false, input::Action::NightVision},
    {"hideOffhand", "feature.hideOffhand", "help.hideOffhand", "visuals.hideOffhand", false, input::Action::HideOffhand},
    {"hideEffects", "feature.hideEffects", "help.hideEffects", "visuals.hideEffects", false, input::Action::ToggleHideEffects},
    {"connectedTextures", "feature.connectedTextures", "help.connectedTextures", "visuals.connectedTextures", false, input::Action::ToggleConnectedTextures},
    {"blockHighlight", "feature.blockHighlight", "help.blockHighlight", "visuals.blockHighlight", false, input::Action::ToggleBlockHighlight},
    {"previews", "feature.previews", "help.previews", "inspection.containerPreviews", false, input::Action::TogglePreviews},
    // The lines Lamium adds to an item's tooltip: each kind of line a switch of its own, kept while the main
    // switch is off (L-319).
    {"itemDetails", "feature.itemDetails", "help.itemDetails", "inspection.itemTooltip", false, input::Action::ToggleItemTooltip},
    {"lockedTrades", "feature.lockedTrades", "help.lockedTrades", "inspection.lockedTrades", false, input::Action::ToggleLockedTrades},
    {"repeatTrades", "feature.repeatTrades", "help.repeatTrades", "inventory.repeatTrades", false, input::Action::ToggleRepeatTrades},
    {"englishSearch", "feature.englishSearch", "help.englishSearch", "inspection.englishSearch", false, input::Action::ToggleEnglishSearch},
    {"sorting", "feature.sorting", "help.sorting", "inventory.sorting", false, input::Action::ToggleSorting},
    {"transfer", "feature.transfer", "help.transfer", "inventory.transfer", true, input::Action::Transfer},
    {"toolSwitch", "feature.toolSwitch", "help.toolSwitch", "inventory.toolSwitch", false, input::Action::ToolSwitch},
    {"weaponSwitch", "feature.weaponSwitch", "help.weaponSwitch", "inventory.weaponSwitch", false, input::Action::WeaponSwitch},
    {"handRestock", "feature.handRestock", "help.handRestock", "inventory.handRestock", false, input::Action::HandRestock},
    {"fakeOffhand", "feature.fakeOffhand", "help.fakeOffhand", "inventory.fakeOffhand", false, input::Action::FakeOffhand},
    {"offhandSwap", "feature.offhandSwap", "help.offhandSwap", "inventory.offhandSwap", false, input::Action::ToggleOffhandSwap},
    {"deathRestore", "feature.deathRestore", "help.deathRestore", "inventory.deathRestore", true, input::Action::ToggleDeathRestore},
    {"enchantOrder", "feature.enchantOrder", "help.enchantOrder", "", true, input::Action::OpenEnchantOrder},
    {"recipes", "feature.recipes", "help.recipes", "inventory.recipes", true, input::Action::ToggleRecipes},
    {"plan", "feature.plan", "help.plan", "inventory.plan", true, input::Action::TogglePlan},
    {"stores", "feature.stores", "help.stores", "inventory.stores", true, input::Action::ToggleStores},
    {"massCraft", "feature.massCraft", "help.massCraft", "inventory.massCraft", true, input::Action::ToggleMassCraft},
    {"restrictions", "feature.restrictions", "help.restrictions", ""},
    {"permanentSneak", "feature.permanentSneak", "help.permanentSneak", "", false, input::Action::PermanentSneak},
    {"permanentSprint", "feature.permanentSprint", "help.permanentSprint", "", false, input::Action::PermanentSprint},
    {"edgeGuard", "feature.edgeGuard", "help.edgeGuard", "interaction.edgeGuard", false, input::Action::EdgeGuard},
    {"sneakPlacement", "feature.sneakPlacement", "help.sneakPlacement", "interaction.sneakPlacement", false, input::Action::SneakPlacement},
    {"toolGuard", "feature.toolGuard", "help.toolGuard", "interaction.toolGuard", false, input::Action::ToolGuard},
    {"elytraSwap", "feature.elytraSwap", "help.elytraSwap", "interaction.elytraSwap", false, input::Action::ElytraSwap},
    {"periodicAttack", "feature.periodicAttack", "help.periodicInput", "interaction.autoAttack", false, input::Action::PeriodicAttack},
    {"periodicUse", "feature.periodicUse", "help.periodicInput", "interaction.autoUse", false, input::Action::PeriodicUse},
    {"infoHud", "feature.infoHud", "help.infoHud", "information.hud", false, input::Action::InfoHud},
    {"targetInfo", "feature.targetInfo", "help.targetInfo", "information.target", false, input::Action::TargetInfo},
    {"playerList", "feature.playerList", "help.playerList", "", false, input::Action::PlayerList},
    {"durabilityHud", "feature.durabilityHud", "help.durabilityHud", "information.durabilityHud", false, input::Action::ToggleDurabilityHud},
    {"inventoryHud", "feature.inventoryHud", "help.inventoryHud", "information.inventoryHud", false, input::Action::ToggleInventoryHud},
    {"usedSlots", "feature.usedSlots", "help.usedSlots", "information.usedSlots", false, input::Action::ToggleUsedSlots},
    {"gameHud", "feature.gameHud", "help.gameHud", "information.gameHud", false, input::Action::ToggleGameHud},
    {"statusEffects", "feature.statusEffects", "help.statusEffects", "information.effectsHud", false, input::Action::ToggleStatusEffects},
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
    {"mapMarks", "feature.mapMarks", "help.mapMarks", "map.mapMarks", true, input::Action::ToggleMapMarks},
    {"radar", "feature.radar", "help.radar", "map.radar", true, input::Action::ToggleRadar},
    {"slimeChunks", "feature.slimeChunks", "help.slimeChunks", "map.worldMapSlime", false, input::Action::ToggleSlimeChunks},
    {"waypoints", "feature.waypoints", "help.waypoints", "map.waypoints", true, input::Action::ToggleWaypoints},
    {"worldMap", "feature.worldMap", "help.worldMap", "", true, input::Action::OpenWorldMap},
    {"schematic", "feature.schematic", "help.schematic", "schematic.enabled", true, input::Action::ToggleSchematic},
    // The menu and the adjust key first: where to start (L-93, 2026-10-07).
    {"schematicMenu", "feature.schematicMenu", "help.schematicMenu", "", false, input::Action::SchematicMenu},
    {"schematicRepeat", "key.Lamium.adjustschematic", "help.key.adjustschematic", "", false, input::Action::AdjustSchematic},
    // How one ghost is drawn (L-424), then what counts as a mistake and how
    // it shows (L-266), neither with a switch of its own.
    {"schematicLook", "feature.schematicLook", "help.schematicLook", ""},
    {"schematicCheck", "feature.schematicCheck", "help.schematicCheck", ""},
    {"schematicAssist", "feature.schematicAssist", "help.schematicAssist", "schematic.placementAssist", true, input::Action::TogglePlacementAssist},
    {"schematicHud", "feature.schematicHud", "help.schematicHud", "schematic.hud", true, input::Action::ToggleSchematicHud},
    // Key groups without a switch of their own (DESIGN: a keyless group
    // heading), named and ordered like the menu's categories (L-238).
    {"schematicMove", "schematic.menu.cat.move", "help.schematicMove", ""},
    {"schematicPlacement", "schematic.menu.cat.placement", "help.schematicPlacement", ""},
    {"schematicLayers", "schematic.menu.cat.layers", "help.schematicLayers", ""},
    {"schematicShow", "schematic.menu.cat.show", "help.schematicShow", ""},
    {"schematicSave", "schematic.menu.cat.area", "help.schematicSave", ""},
    {"schematicScreen", "schematic.menu.cat.screen", "help.schematicScreen", ""},
    {"automationStatus", "feature.automationStatus", "help.automationStatus", "interface.automationStatus", false, input::Action::ToggleAutomationStatus},
    {"settings", "feature.settings", "help.settings", "", false, input::Action::Settings},
    {"gameLanguage", "feature.gameLanguage", "help.gameLanguage", "", false, input::Action::OpenGameLanguage},
    {"savedData", "feature.savedData", "help.savedData", "", false, input::Action::OpenSavedData},
    {"skipServerPacks", "feature.skipServerPacks", "help.skipServerPacks", "visuals.skipServerPacks", false, input::Action::ToggleSkipServerPacks},
    {"calculator", "feature.calculator", "help.calculator", "", false, input::Action::OpenCalculator},
    {"bookPaste", "feature.bookPaste", "help.bookPaste", "interface.bookPaste", false, input::Action::ToggleBookPaste},
    // Headings without a switch: their first row is the switch (no key of its own).
    {"toasts", "feature.toasts", "help.toasts", ""},
    // Text of the line elements (Info HUD, Status, Debug View) and HUD backgrounds.
    {"hudText", "feature.hudText", "help.hudText", ""},
});
// Display order of action rows where catalog order (fixed: ids are saved)
// would scatter related keys; others keep catalog order after these.
inline int actionRank(input::Action action) {
    using A = input::Action;
    // Schematic keys follow their menu items (L-238).
    static constexpr A order[] = {A::SchematicMenu, A::AdjustSchematic, A::OpenSchematics,
        A::MovePlacementForward, A::MovePlacementBack, A::MovePlacementLeft, A::MovePlacementRight, A::MovePlacementUp,
        A::MovePlacementDown, A::MovePlacementHere, A::CycleMoveTarget, A::SelectLookedPlacement,
        A::NextPlacement, A::PreviousPlacement, A::TogglePlacementShown, A::RotatePlacement, A::RotatePlacementBack, A::MirrorPlacement,
        A::ResetPlacementTurn, A::CycleLayerAxis, A::LayerUp, A::LayerDown, A::CycleLayerMode, A::ToggleLayerKeep, A::LayerHere,
        A::IgnoreLookedBlock, A::TogglePlacementEntities, A::TogglePlacementExtras, A::NearestMistake,
        A::SchematicCorner1, A::SchematicCorner2, A::SaveSchematicArea, A::ClearSchematicArea,
        A::OpenSchematicFiles, A::OpenSchematicPlaced, A::OpenSchematicCheck, A::OpenSchematicMaterials, A::OpenSchematicKeys};
    for (size_t i = 0; i < std::size(order); ++i) if (order[i] == action) return static_cast<int>(i);
    return static_cast<int>(std::size(order)) + static_cast<int>(action);
}
// A feature row only carries the binding named by that row. Other actions
// remain under the feature, even if they were registered first.
inline std::optional<input::Action> primaryAction(FeatureInfo const& feature) {
    return feature.primary;
}

// Actions that open one of Lamium's tools from the settings table get an
// Open button on their own row (L-231). Only the main opener per tool; the
// Schematics tab variants keep their keys alone.
inline bool opensTool(input::Action action) {
    switch (action) {
    case input::Action::OpenShapes:
    case input::Action::OpenWaypoints:
    case input::Action::OpenSchematics:
    case input::Action::OpenHotkeys:
    case input::Action::OpenHudLayout:
    case input::Action::OpenWorldMap:
    case input::Action::OpenCalculator:
    case input::Action::SchematicMenu:
    case input::Action::OpenBlockHighlight:
    case input::Action::OpenEnchantOrder:
    case input::Action::OpenSavedData:
    case input::Action::OpenGameLanguage:
        return true;
    default:
        return false;
    }
}

// Option rows that carry a hotkey in their own key cell, so a setting and
// the key that changes it read as one item. Such an action gets no row of its
// own under the feature; Hotkeys still lists it.
inline std::optional<input::Action> optionAction(std::string_view option) {
    if (option == "visuals.hideWeather") return input::Action::HideWeather;
    if (option == "visuals.hideParticles") return input::Action::HideParticles;
    if (option == "visuals.hideNausea") return input::Action::HideNausea;
    if (option == "information.chatPosition") return input::Action::ToggleChatPosition;
    if (option == "information.paperDollPosition") return input::Action::TogglePaperDollPosition;
    if (option == "information.scoreboardPosition") return input::Action::ToggleScoreboardPosition;
    if (option == "information.bossBarPosition") return input::Action::HideBossBars;
    if (option == "inspection.durability") return input::Action::ToggleDurability;
    if (option == "inspection.foodValues") return input::Action::ToggleFoodValues;
    if (option == "inspection.workPenalty") return input::Action::ToggleWorkPenalty;
    if (option == "inspection.itemDetails") return input::Action::ToggleItemDetails;
    if (option == "inspection.itemDetailsStew") return input::Action::ToggleItemStew;
    if (option == "inspection.itemDetailsId") return input::Action::ToggleItemId;
    if (option == "inspection.itemDetailsTags") return input::Action::ToggleItemTags;
    if (option == "inspection.itemDetailsNbt") return input::Action::ToggleItemNbt;
    if (option == "visuals.hideWater") return input::Action::HideWater;
    if (option == "visuals.hideLava") return input::Action::HideLava;
    if (option == "visuals.hidePowderSnow") return input::Action::HidePowderSnow;
    if (option == "visuals.hideDistanceFog") return input::Action::HideDistanceFog;
    if (option == "visuals.hidePortal") return input::Action::HidePortal;
    if (option == "interaction.breaking") return input::Action::BreakingRestriction;
    if (option == "interaction.attackMode") return input::Action::CycleAttackMode;
    if (option == "interaction.useMode") return input::Action::CycleUseMode;
    if (option == "interaction.attackHeldOnly") return input::Action::AttackHeldOnly;
    if (option == "interaction.useHeldOnly") return input::Action::UseHeldOnly;
    if (option == "interaction.breakingMode") return input::Action::CycleBreakingMode;
    if (option == "map.waypointsWorld") return input::Action::HideWaypoints;
    if (option == "map.radarFaces") return input::Action::RadarFaces;
    if (option == "map.worldMap") return input::Action::ToggleWorldMap;
    if (option == "information.targetMeasure") return input::Action::TargetMeasure;
    if (option == "information.targetStates") return input::Action::TargetDetails;
    if (option == "information.targetBlockData") return input::Action::TargetBlockData;
    if (option == "information.targetCoordinates") return input::Action::TargetPosition;
    if (option == "map.caveView") return input::Action::MinimapView;
    if (option == "map.netherView") return input::Action::MinimapNetherView;
    if (option == "inventory.toolSwitchDrop") return input::Action::CycleToolDrop;
    if (option == "schematic.mistakeMarks") return input::Action::ToggleMistakeMarks;
    if (option == "schematic.ignoreSignal") return input::Action::ToggleIgnoreSignal;
    if (option == "schematic.litAsState") return input::Action::ToggleLitAsState;
    if (option == "schematic.ignoreGrowth") return input::Action::ToggleIgnoreGrowth;
    if (option == "schematic.ignoreBlockEntities") return input::Action::ToggleIgnoreBlockEntities;
    if (option == "schematic.ignoreSignText") return input::Action::ToggleIgnoreSignText;
    return {};
}
using input::fixedPressOf;
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
// A feature's switch is off: its child settings are kept but do nothing (drawn dimmed, L-319).
inline bool effectsPaused(std::string_view id, Settings const& value) {
    for (auto const& feature : features) {
        if (feature.id != id || feature.toggle.empty()) continue;
        if (auto option = settings::find(feature.toggle)) {
            auto current = option->read(value);
            if (auto state = std::get_if<bool>(&current)) return !*state;
        }
    }
    return false;
}

// MapCache: the world map's saved data for this world, with a delete button.
// SavedData: which saved data the joined server uses, linking to General > Saved data (L-343).
// Fixed: a press that is not a binding (input::fixedPresses), read-only in Hotkeys (L-372).
enum class RowKind { Section, Feature, Option, Action, Layout, MapCache, SavedData, BandLayout, Fixed };
// While its feature's switch is off, a setting row is drawn dimmed whole;
// command rows (actions, the HUD layout link, the saved data and map data
// rows) stay at full strength (DESIGN "Screens", L-335).
inline constexpr bool dimsWhenPaused(RowKind kind) { return kind == RowKind::Option; }
// How strongly a dimmed row is drawn: its guide, name, control and key alike.
inline constexpr float pausedRowOpacity = .45f;
// The features whose data is kept per world and server.
inline constexpr bool keepsSavedData(std::string_view feature) {
    return feature == "savedData" || feature == "waypoints" || feature == "worldMap" || feature == "schematic";
}
// HUD features link to their element in the layout editor instead of listing
// placement and look rows (docs/HUD.md).
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
    if (feature == "gameHud") return HudElementId::Chat;
    if (feature == "statusEffects") return HudElementId::StatusEffects;
    return std::nullopt;
}
// Features with a page on the item screens' bands link to the bands' editor with that page (L-375).
inline std::optional<recipes::band::Page> bandPageOf(std::string_view feature) {
    if (feature == "recipes") return recipes::band::Page::Items;
    if (feature == "plan") return recipes::band::Page::Plan;
    return std::nullopt;
}
// Every element a feature places: Game HUD has four, its row links to the first.
inline std::vector<HudElementId> placedElements(std::string_view feature) {
    if (feature == "gameHud")
        return {HudElementId::Chat, HudElementId::PaperDoll, HudElementId::Scoreboard, HudElementId::BossBars};
    if (auto element = layoutElement(feature)) return {*element};
    return {};
}
inline constexpr std::string_view layoutLinkLabel(HudElementId id) {
    return id == HudElementId::Toast ? "layoutLinkToast"
        : id == HudElementId::Magnification ? "layoutLinkMagnification" : "layoutLink";
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
// The catalog's name key for a feature id; empty for an unknown id.
inline constexpr std::string_view featureNameKey(std::string_view id) {
    for (auto const& feature : features)
        if (feature.id == id) return feature.name;
    return {};
}
// What a toggle toast names: the feature; a child switch's label follows it (Actions.cpp).
inline std::string_view toggleToastKey(input::Action action) {
    return featureNameKey(input::actions[static_cast<size_t>(action)].feature);
}
// Hotkeys found by key: only actions whose binding uses these inputs.
struct KeyFilter {
    input::Chord pressed;
    input::Bindings bindings;
    std::vector<int> dropKeys, inventoryKeys; // the game's drop and inventory keys, for the fixed presses that use them
};
struct SettingsRow {
    RowKind kind;
    FeatureInfo const* feature = nullptr;
    settings::Option const* option = nullptr;
    std::optional<input::Action> action;
    std::string_view section;
    std::optional<HudElementId> layout; // Layout rows
    std::optional<recipes::band::Page> bandPage; // BandLayout rows: the page the bands' editor opens with
    int children = 0;       // Feature rows: expandable content
    bool expanded = false;  // Feature rows
    bool lastChild = false; // Child rows: end of the tree guide
    std::string_view group; // Section rows: the group they head
    int press = -1; // Fixed rows: the index in input::fixedPresses
    bool heading() const { return kind == RowKind::Feature; }
    bool selectable() const { return kind != RowKind::Section; }
    bool child() const {
        return kind == RowKind::Option || kind == RowKind::Action || kind == RowKind::Layout || kind == RowKind::MapCache
            || kind == RowKind::SavedData || kind == RowKind::BandLayout || kind == RowKind::Fixed;
    }
    bool operator==(SettingsRow const& other) const {
        return kind == other.kind && feature == other.feature && option == other.option
            && action == other.action && section == other.section && layout == other.layout && bandPage == other.bandPage
            && group == other.group && press == other.press;
    }
};
// The binding a row carries: its own action, a keyed option's, a feature's toggle key.
inline std::optional<input::Action> rowAction(SettingsRow const& row) {
    if (row.kind == RowKind::Action) return row.action;
    if (row.kind == RowKind::Option && row.option) return optionAction(row.option->id);
    if (row.kind == RowKind::Feature && row.feature) return primaryAction(*row.feature);
    return std::nullopt;
}
// Category is a section key, or empty for all sections. A query searches every
// section and initially expands features whose settings, but not name, match it.
// Rows are headed by group in every view, a category of one group by its own name (L-340).
// Presentation independent of Minecraft objects, so it is testable without rendering.
template<class Translate>
std::vector<SettingsRow> buildSettingsRows(bool hotkeys, std::string_view category, SearchQuery const& query,
    std::set<std::string_view> const& expanded, Translate translate,
    std::set<std::string_view> const& searchCollapsed = {}, KeyFilter const* keys = nullptr) {
    std::vector<SettingsRow> rows;
    bool const searching = query.value().find_first_not_of(' ') != std::string::npos;
    if (searching) category = {};
    std::string_view lastGroup;
    for (auto const& group : groups) {
        auto const section = group.section;
        if (!category.empty() && category != section) continue;
        auto sectionRow = [&] {
            if (group.key != lastGroup) {
                SettingsRow row{RowKind::Section, nullptr, nullptr, {}, section, std::nullopt, std::nullopt};
                row.group = group.key;
                rows.push_back(row);
            }
            lastGroup = group.key;
        };
        for (auto member : group.features) {
            auto found = std::find_if(features.begin(), features.end(), [&](FeatureInfo const& f) { return f.id == member; });
            if (found == features.end()) continue;
            auto const& feature = *found;
            std::string scope = std::string(feature.id) + " " + translate(feature.name) + " "
                + translate(feature.description) + " " + translate(section) + " " + translate(group.key);
            auto primary = primaryAction(feature);
            std::vector<size_t> actionOrder;
            for (size_t i = 0; i < input::actions.size(); ++i)
                if (input::actions[i].feature == feature.id) actionOrder.push_back(i);
            std::stable_sort(actionOrder.begin(), actionOrder.end(), [](size_t a, size_t b) {
                return actionRank(static_cast<input::Action>(a)) < actionRank(static_cast<input::Action>(b));
            });
            std::vector<SettingsRow> children;
            for (auto const& option : settings::options) {
                if (option.feature != feature.id || option.id == feature.toggle) continue;
                if (option.id.starts_with("hud.")) continue; // Edited in the layout editor.
                // Saved and loaded, but nothing reads it until placement restrictions are built (L-15, L-338).
                if (option.id == "interaction.placementMode") continue;
                // Hotkeys lists the options that carry a key; a gesture's switch is its fixed row there.
                if (hotkeys && !optionAction(option.id)) continue;
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
            // Fixed presses (L-372): read-only rows after the bindings. On the settings screen a gesture with
            // its own switch is that switch's row, which shows the keys.
            for (size_t i = 0; i < input::fixedPresses.size(); ++i) {
                auto const& press = input::fixedPresses[i];
                if (press.feature != feature.id || (!hotkeys && !press.option.empty())) continue;
                SettingsRow row{RowKind::Fixed, &feature, nullptr, {}, section, std::nullopt, std::nullopt};
                row.press = static_cast<int>(i);
                children.push_back(row);
            }
            if (!hotkeys) {
                // Info lines keep catalog order (grouped by kind), not the HUD's
                // order: the HUD layout arranges them (L-219).
                if (auto element = layoutElement(feature.id))
                    children.push_back({RowKind::Layout, &feature, nullptr, {}, section, element});
                if (auto page = bandPageOf(feature.id))
                    children.push_back({RowKind::BandLayout, &feature, nullptr, {}, section, std::nullopt, page});
                if (feature.id == "worldMap") children.push_back({RowKind::MapCache, &feature, nullptr, {}, section});
                if (keepsSavedData(feature.id)) children.push_back({RowKind::SavedData, &feature, nullptr, {}, section});
            }
            auto childText = [&](SettingsRow const& row) {
                if (row.kind == RowKind::MapCache) return std::string(translate("mapCache"));
                if (row.kind == RowKind::BandLayout) return std::string(translate("band.layoutLink"));
                if (row.kind == RowKind::SavedData) return std::string(translate("savedData.row")) + " " + translate("feature.savedData");
                if (row.layout) return std::string(translate(layoutLinkLabel(*row.layout))) + " " + translate("nav.hudLayout");
                if (row.kind == RowKind::Fixed) {
                    auto const& press = input::fixedPresses[static_cast<size_t>(row.press)];
                    auto const* option = press.option.empty() ? nullptr : settings::find(press.option);
                    return std::string(translate(option ? option->label : press.name));
                }
                if (row.option) return std::string(row.option->id) + " " + translate(row.option->label);
                // The child label shown here, plus the Hotkeys name as an alias.
                return std::string(input::actions[static_cast<size_t>(*row.action)].id) + " "
                    + translate(actionTranslationKey(*row.action, true)) + " " + translate(actionTranslationKey(*row.action));
            };
            if (hotkeys) {
                // Hotkeys (L-372): the same tree, always open, only rows with a key. A hit child shows under its
                // parent without its siblings; a parent hit, or a parent pressed while searching, shows them all.
                bool const hasPrimary = primary && !input::retired(*primary);
                if (!hasPrimary && children.empty()) continue;
                auto uses = [&](SettingsRow const& row) {
                    if (row.kind == RowKind::Fixed) return input::fixedUses(input::fixedPresses[static_cast<size_t>(row.press)], keys->pressed, {keys->dropKeys, keys->inventoryKeys});
                    auto action = row.option ? optionAction(row.option->id) : row.action;
                    return action && input::chordUses(input::effectiveChord(keys->bindings, *action), keys->pressed);
                };
                bool const self = keys ? hasPrimary && input::chordUses(input::effectiveChord(keys->bindings, *primary), keys->pressed)
                                       : query.matches(scope);
                std::vector<SettingsRow> matching;
                for (auto const& child : children)
                    if (keys ? uses(child) : query.matches(scope + " " + childText(child))) matching.push_back(child);
                if (!self && matching.empty()) continue;
                bool const all = (!keys && self) || (!keys && !searching) || expanded.contains(feature.id);
                auto const& shown = all ? children : matching;
                sectionRow();
                SettingsRow heading{RowKind::Feature, &feature, nullptr, {}, section};
                heading.children = static_cast<int>(children.size());
                heading.expanded = all || matching.size() == children.size();
                rows.push_back(heading);
                for (size_t i = 0; i < shown.size(); ++i) {
                    rows.push_back(shown[i]);
                    rows.back().lastChild = i + 1 == shown.size();
                }
                continue;
            }
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
            sectionRow();
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
// a value pattern without adding a second catalog of translations. The split
// is the last ": " before the value, so a name with a colon of its own
// ("Wheel: one item: {}") stays whole.
struct LabelParts { std::string name, value; };
inline LabelParts splitLabel(std::string_view pattern) {
    auto split = pattern.rfind(": ", pattern.find('{'));
    if (split == std::string_view::npos) {
        auto brace = pattern.find('{');
        std::string name(pattern.substr(0, brace));
        while (!name.empty() && name.back() == ' ') name.pop_back();
        return {name, "{}"};
    }
    return {std::string(pattern.substr(0, split)), std::string(pattern.substr(split + 2))};
}
// An option's value as its stepper or slider shows it: a choice's name, or a number in its label's pattern.
template<class Translate>
std::string optionValueText(settings::Option const& option, settings::OptionValue const& value, Translate translate) {
    auto pattern = splitLabel(std::string(translate(option.label))).value;
    try {
        if (auto choice = std::get_if<settings::ChoiceValue>(&value)) return std::string(translate(choice->label));
        if (auto number = std::get_if<float>(&value)) {
            if (option.numeric && option.numeric->names)
                return std::string(translate(option.numeric->names[static_cast<size_t>(
                    std::clamp(std::lround(*number - option.numeric->minimum), 0L,
                               std::lround(option.numeric->maximum - option.numeric->minimum)))]));
            float secondary = option.numeric ? *number * option.numeric->secondary : 0;
            return std::vformat(pattern, std::make_format_args(*number, secondary));
        }
    } catch (std::exception const&) {}
    return {};
}
// An action named alone (conflict tip, capture line, Hotkeys' footer) is composed from the tree like a
// toggle toast (L-372): the feature alone for its toggle key, "Feature > option" for a keyed option,
// "Feature > child" for a child action. ASCII " > ": another sign changes the font in English.
inline bool namedByTree(input::Action action) {
    auto const& id = input::actions[static_cast<size_t>(action)].feature;
    for (auto const& feature : features)
        if (feature.id == id && feature.primary == action) return true;
    return shownOnOption(action);
}
template<class Translate>
std::string actionTitle(input::Action action, Translate translate) {
    auto const& id = input::actions[static_cast<size_t>(action)].feature;
    auto const nameKey = featureNameKey(id);
    std::string const feature = nameKey.empty() ? std::string(id) : std::string(translate(nameKey));
    for (auto const& info : features)
        if (info.id == id && info.primary == action) return feature;
    for (auto const& option : settings::options)
        if (optionAction(option.id) == action) return feature + " > " + splitLabel(translate(option.label)).name;
    return feature + " > " + actionName(std::string(translate(actionTranslationKey(action, true))));
}
// Reset a settings category: every option owned by its features and the HUD
// placement of their elements. Key bindings stay; Hotkeys has its own reset.
inline void resetSection(Settings& value, std::string_view section) {
    Settings const defaults{};
    for (auto const& option : settings::options)
        if (featureSection(option.feature) == section) settings::resetOption(value, option, defaults);
    for (auto const& feature : features)
        if (featureSection(feature.id) == section)
            for (auto element : placedElements(feature.id))
                settings::hudElement(value, element) = settings::hudElement(defaults, element);
}
}
