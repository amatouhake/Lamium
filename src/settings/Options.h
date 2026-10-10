#pragma once
#include "settings/Settings.h"
#include <array>
#include <optional>
#include <string_view>
#include <variant>

namespace lamium::settings {
struct ChoiceValue { std::string_view label; bool operator==(ChoiceValue const&) const = default; };
using OptionValue = std::variant<bool, float, ChoiceValue>;
struct NumericOption {
    float minimum, maximum;
    void (*write)(Settings&, float);
    float step = 0; // > 0: shown as a slider that snaps to this step
    float secondary = 0; // > 0: the label also formats value * secondary (ticks -> seconds)
};
struct Option {
    std::string_view id;
    std::string_view feature;
    std::string_view label;
    OptionValue (*read)(Settings const&);
    void (*adjust)(Settings&, int);
    std::optional<NumericOption> numeric = std::nullopt;
};

// Stable identifiers and feature ownership are independent of presentation
// order. Editors use the same accessors rather than maintaining row switches.
template<auto Group, auto Member>
constexpr Option toggle(std::string_view id, std::string_view feature, std::string_view label) {
    return {id, feature, label,
        [](Settings const& value) -> OptionValue { return (value.*Group).*Member; },
        [](Settings& value, int) { auto& field = (value.*Group).*Member; field = !field; }};
}
template<auto Group, auto Member, auto const& Labels>
constexpr Option choice(std::string_view id, std::string_view feature, std::string_view label) {
    return {id, feature, label,
        [](Settings const& value) -> OptionValue {
            auto index = static_cast<size_t>((value.*Group).*Member);
            return ChoiceValue{Labels[index < Labels.size() ? index : 0]};
        },
        [](Settings& value, int direction) {
            auto& field = (value.*Group).*Member;
            auto index = static_cast<size_t>(field);
            if (index >= Labels.size()) index = 0;
            index = (index + (direction < 0 ? Labels.size()-1 : 1)) % Labels.size();
            field = static_cast<std::remove_reference_t<decltype(field)>>(index);
        }};
}
inline constexpr std::array<std::string_view,2> activationLabels{"activation.hold","activation.toggle"};
inline constexpr std::array<std::string_view,2> cameraReferenceLabels{"cameraReference.player","cameraReference.world"};
inline constexpr std::array<std::string_view,3> perspectiveLabels{"perspective.first","perspective.rear","perspective.front"};
inline constexpr std::array<std::string_view,3> healthMeterLabels{"meter.hearts","meter.bar","meter.number"};
inline constexpr std::array<std::string_view,2> growthMeterLabels{"meter.bar","meter.number"};
inline constexpr std::array<std::string_view,3> durabilityLookLabels{"durabilityLook.barAndNumber","durabilityLook.number","durabilityLook.bar"};
inline constexpr std::array<std::string_view,3> armorMeterLabels{"meter.icons","meter.bar","meter.number"};
inline constexpr std::array<std::string_view,2> restockOrderLabels{"restockOrder.largest","restockOrder.smallest"};
inline constexpr std::array<std::string_view,2> debugLabelLabels{"debugLabels.game","debugLabels.java"};
inline constexpr std::array<std::string_view,3> animationLabels{"animations.follow","animations.on","animations.off"};
inline constexpr std::array<std::string_view,3> biomeDisplayLabels{
    "biomeDisplay.name", "biomeDisplay.nameAndId", "biomeDisplay.id"};
inline constexpr std::array<std::string_view,2> realTimeDisplayLabels{
    "realTimeDisplay.time", "realTimeDisplay.dateAndTime"};
inline constexpr auto anchorLabels = std::to_array<std::string_view>(
    {"anchor.topLeft", "anchor.topCenter", "anchor.topRight", "anchor.middleLeft", "anchor.center",
     "anchor.middleRight", "anchor.bottomLeft", "anchor.bottomCenter", "anchor.bottomRight"});
inline constexpr std::array<std::string_view,11> mapZoomLabels{
    "mapZoom.16", "mapZoom.24", "mapZoom.32", "mapZoom.48", "mapZoom.64", "mapZoom.96", "mapZoom.128",
    "mapZoom.192", "mapZoom.256", "mapZoom.384", "mapZoom.512"};
static_assert(mapZoomLabels.size() == map::zoomSteps.size());
inline constexpr std::array<std::string_view,3> worldMarkerLabels{
    "worldMarkers.always", "worldMarkers.whileHeld", "worldMarkers.off"};
inline constexpr auto elementBackgroundLabels = std::to_array<std::string_view>(
    {"hudBackgroundNone", "hudBackgroundCard"});
// Info, Status, the player list and the inventory HUD also offer a background behind each line (L-98, L-128, L-127).
inline constexpr auto lineBackgroundLabels = std::to_array<std::string_view>(
    {"hudBackgroundNone", "hudBackgroundCard", "hudBackgroundLine"});
inline constexpr std::array<std::string_view,2> debugBackgroundLabels{"hudBackgroundNone","hudBackgroundLine"};
inline constexpr std::array<std::string_view,3> menuBackgroundLabels{"menuBackground.none", "menuBackground.light", "menuBackground.dark"};
// L-97: the selected slot, then hotbar slots 1-9.
inline constexpr std::array<std::string_view,10> fetchSlotLabels{"fetchSlot.selected", "hotbarSlot.1", "hotbarSlot.2",
    "hotbarSlot.3", "hotbarSlot.4", "hotbarSlot.5", "hotbarSlot.6", "hotbarSlot.7", "hotbarSlot.8", "hotbarSlot.9"};
inline ui::HudElement const& hudElement(Settings const& value, ui::HudElementId id) {
    switch (id) {
    case ui::HudElementId::Info: return value.hud.info;
    case ui::HudElementId::Target: return value.hud.target;
    case ui::HudElementId::Status: return value.hud.status;
    case ui::HudElementId::Magnification: return value.hud.magnification;
    case ui::HudElementId::Durability: return value.hud.durability;
    case ui::HudElementId::Minimap: return value.hud.minimap;
    case ui::HudElementId::Schematic: return value.hud.schematic;
    case ui::HudElementId::PlayerList: return value.hud.playerList;
    case ui::HudElementId::Inventory: return value.hud.inventory;
    case ui::HudElementId::FreeSlots: return value.hud.freeSlots;
    default: return value.hud.toast;
    }
}
inline ui::HudElement& hudElement(Settings& value, ui::HudElementId id) {
    switch (id) {
    case ui::HudElementId::Info: return value.hud.info;
    case ui::HudElementId::Target: return value.hud.target;
    case ui::HudElementId::Status: return value.hud.status;
    case ui::HudElementId::Magnification: return value.hud.magnification;
    case ui::HudElementId::Durability: return value.hud.durability;
    case ui::HudElementId::Minimap: return value.hud.minimap;
    case ui::HudElementId::Schematic: return value.hud.schematic;
    case ui::HudElementId::PlayerList: return value.hud.playerList;
    case ui::HudElementId::Inventory: return value.hud.inventory;
    case ui::HudElementId::FreeSlots: return value.hud.freeSlots;
    default: return value.hud.toast;
    }
}
template<ui::HudElementId Id, auto Field>
constexpr Option hudToggle(std::string_view id, std::string_view feature, std::string_view label) {
    return {id, feature, label,
        [](Settings const& value) -> OptionValue { return hudElement(value, Id).*Field; },
        [](Settings& value, int) { auto& field = hudElement(value, Id).*Field; field = !field; }};
}
template<ui::HudElementId Id, auto Field, auto const& Labels>
constexpr Option hudChoice(std::string_view id, std::string_view feature, std::string_view label) {
    return {id, feature, label,
        [](Settings const& value) -> OptionValue {
            auto index = static_cast<size_t>(hudElement(value, Id).*Field);
            return ChoiceValue{Labels[index < Labels.size() ? index : 0]};
        },
        [](Settings& value, int direction) {
            auto& field = hudElement(value, Id).*Field;
            auto index = static_cast<size_t>(field);
            if (index >= Labels.size()) index = 0;
            index = (index + (direction < 0 ? Labels.size() - 1 : 1)) % Labels.size();
            field = static_cast<std::remove_reference_t<decltype(field)>>(index);
        }};
}
template<ui::HudElementId Id, auto Field, int Step>
constexpr Option hudNumeric(std::string_view id, std::string_view feature, std::string_view label,
                            float minimum, float maximum) {
    return {id, feature, label,
        [](Settings const& value) -> OptionValue { return hudElement(value, Id).*Field; },
        [](Settings& value, int direction) {
            auto& field = hudElement(value, Id).*Field;
            field += direction * Step;
            value.normalize();
        },
        NumericOption{minimum, maximum, [](Settings& value, float number) { hudElement(value, Id).*Field = number; }}};
}
inline constexpr auto options = std::to_array<Option>({
    toggle<&Settings::interaction, &Settings::Interaction::autoAttack>("interaction.autoAttack", "periodicAttack", "autoAttack"),
    choice<&Settings::interaction, &Settings::Interaction::attackMode, interaction::autoModeLabels>("interaction.attackMode", "periodicAttack", "autoModeRow"),
    {"interaction.attackTicks", "periodicAttack", "autoInterval",
        [](Settings const& s) -> OptionValue { return s.interaction.attackTicks; },
        [](Settings& s, int direction) { s.interaction.attackTicks += direction; s.normalize(); },
        NumericOption{1, 1200, [](Settings& s, float v) { s.interaction.attackTicks = v; }, 0, .05f}},
    {"interaction.attackClicks", "periodicAttack", "autoClicks",
        [](Settings const& s) -> OptionValue { return s.interaction.attackClicks; },
        [](Settings& s, int direction) { s.interaction.attackClicks += direction; s.normalize(); },
        NumericOption{1, 10, [](Settings& s, float v) { s.interaction.attackClicks = v; }, 0, 20}},
    toggle<&Settings::interaction, &Settings::Interaction::attackHeldOnly>("interaction.attackHeldOnly", "periodicAttack", "fastHeldOnly"),
    toggle<&Settings::interaction, &Settings::Interaction::autoUse>("interaction.autoUse", "periodicUse", "autoUse"),
    choice<&Settings::interaction, &Settings::Interaction::useMode, interaction::autoModeLabels>("interaction.useMode", "periodicUse", "autoModeRow"),
    {"interaction.useTicks", "periodicUse", "autoInterval",
        [](Settings const& s) -> OptionValue { return s.interaction.useTicks; },
        [](Settings& s, int direction) { s.interaction.useTicks += direction; s.normalize(); },
        NumericOption{1, 1200, [](Settings& s, float v) { s.interaction.useTicks = v; }, 0, .05f}},
    {"interaction.useClicks", "periodicUse", "autoClicks",
        [](Settings const& s) -> OptionValue { return s.interaction.useClicks; },
        [](Settings& s, int direction) { s.interaction.useClicks += direction; s.normalize(); },
        NumericOption{1, 10, [](Settings& s, float v) { s.interaction.useClicks = v; }, 0, 20}},
    toggle<&Settings::interaction, &Settings::Interaction::useHeldOnly>("interaction.useHeldOnly", "periodicUse", "fastHeldOnly"),
    toggle<&Settings::interaction, &Settings::Interaction::breaking>("interaction.breaking", "restrictions", "breakingRestriction"),
    choice<&Settings::interaction, &Settings::Interaction::breakingMode, interaction::restrictionLabels>("interaction.breakingMode", "restrictions", "breakingMode"),
    {"interaction.breakingBand", "restrictions", "breakingBand",
        [](Settings const& s) -> OptionValue { return s.interaction.breakingBand; },
        [](Settings& s, int direction) { s.interaction.breakingBand += direction; s.normalize(); },
        NumericOption{1, 16, [](Settings& s, float v) { s.interaction.breakingBand = v; }, 0, 20}},
    choice<&Settings::interaction, &Settings::Interaction::placementMode, interaction::placementLabels>("interaction.placementMode", "restrictions", "placementMode"),
    toggle<&Settings::information, &Settings::Information::debug>("information.debug", "debugView", "debugView"),
    choice<&Settings::information, &Settings::Information::debugLabels, debugLabelLabels>("information.debugLabels", "debugView", "debugLabelStyle"),
    toggle<&Settings::information, &Settings::Information::debugHideHud>("information.debugHideHud", "debugView", "debugHideHud"),
    toggle<&Settings::information, &Settings::Information::debugHideTarget>("information.debugHideTarget", "debugView", "debugHideTarget"),
    toggle<&Settings::information, &Settings::Information::debugShadow>("information.debugShadow", "debugView", "hudShadow"),
    choice<&Settings::information, &Settings::Information::debugBackground, debugBackgroundLabels>("information.debugBackground", "debugView", "hudBackground"),
    toggle<&Settings::information, &Settings::Information::target>("information.target", "targetInfo", "targetInfo"),
    toggle<&Settings::information, &Settings::Information::targetIdentifier>("information.targetIdentifier", "targetInfo", "targetIdentifier"),
    toggle<&Settings::information, &Settings::Information::targetIcon>("information.targetIcon", "targetInfo", "targetIcon"),
    choice<&Settings::information, &Settings::Information::targetHealth, healthMeterLabels>("information.targetHealth", "targetInfo", "targetHealth"),
    choice<&Settings::information, &Settings::Information::targetArmor, armorMeterLabels>("information.targetArmor", "targetInfo", "targetArmor"),
    choice<&Settings::information, &Settings::Information::targetGrowth, growthMeterLabels>("information.targetGrowth", "targetInfo", "targetGrowth"),
    {"information.targetDistance", "targetInfo", "targetDistance",
        [](Settings const& s) -> OptionValue { return s.information.targetDistance; },
        [](Settings& s, int direction) { s.information.targetDistance += direction; s.normalize(); },
        NumericOption{2, 64, [](Settings& s, float v) { s.information.targetDistance = v; }, 1}},
    toggle<&Settings::information, &Settings::Information::targetStates>("information.targetStates", "targetInfo", "targetStates"),
    toggle<&Settings::information, &Settings::Information::targetCoordinates>("information.targetCoordinates", "targetInfo", "targetCoordinates"),
    toggle<&Settings::information, &Settings::Information::durabilityHud>("information.durabilityHud", "durabilityHud", "durabilityHud"),
    choice<&Settings::information, &Settings::Information::durabilityLook, durabilityLookLabels>("information.durabilityLook", "durabilityHud", "durabilityLook"),
    toggle<&Settings::information, &Settings::Information::durabilityOffhand>("information.durabilityOffhand", "durabilityHud", "durabilityOffhand"),
    toggle<&Settings::information, &Settings::Information::durabilityArmor>("information.durabilityArmor", "durabilityHud", "durabilityArmor"),
    toggle<&Settings::information, &Settings::Information::offhandSlot>("information.offhandSlot", "offhandSlot", "offhandSlot"),
    toggle<&Settings::information, &Settings::Information::offhandSlotEmpty>("information.offhandSlotEmpty", "offhandSlot", "offhandSlotEmpty"),
    toggle<&Settings::information, &Settings::Information::playerListPlatform>("information.playerListPlatform", "playerList", "playerListPlatform"),
    toggle<&Settings::information, &Settings::Information::playerListDimension>("information.playerListDimension", "playerList", "playerListDimension"),
    toggle<&Settings::information, &Settings::Information::playerListDistance>("information.playerListDistance", "playerList", "playerListDistance"),
    toggle<&Settings::information, &Settings::Information::playerListMembers>("information.playerListMembers", "playerList", "playerListMembers"),
    hudNumeric<ui::HudElementId::PlayerList, &ui::HudElement::scale, 25>("hud.playerList.scale", "playerList", "hudScale", 75, 150),
    hudChoice<ui::HudElementId::PlayerList, &ui::HudElement::background, lineBackgroundLabels>("hud.playerList.background", "playerList", "hudBackground"),
    hudToggle<ui::HudElementId::PlayerList, &ui::HudElement::shadow>("hud.playerList.shadow", "playerList", "hudShadow"),
    // One hotbar switch shown under both elements: the counter counts what the grid shows (L-127).
    toggle<&Settings::information, &Settings::Information::inventoryHud>("information.inventoryHud", "inventoryHud", "inventoryHud"),
    toggle<&Settings::information, &Settings::Information::inventoryHotbar>("information.inventoryHotbar", "inventoryHud", "inventoryHotbar"),
    hudNumeric<ui::HudElementId::Inventory, &ui::HudElement::scale, 25>("hud.inventory.scale", "inventoryHud", "hudScale", 75, 150),
    hudChoice<ui::HudElementId::Inventory, &ui::HudElement::background, lineBackgroundLabels>("hud.inventory.background", "inventoryHud", "hudBackground"),
    hudToggle<ui::HudElementId::Inventory, &ui::HudElement::shadow>("hud.inventory.shadow", "inventoryHud", "hudShadow"),
    toggle<&Settings::information, &Settings::Information::freeSlots>("information.freeSlots", "freeSlots", "freeSlots"),
    toggle<&Settings::information, &Settings::Information::inventoryHotbar>("information.freeSlotsHotbar", "freeSlots", "inventoryHotbar"),
    hudNumeric<ui::HudElementId::FreeSlots, &ui::HudElement::scale, 25>("hud.freeSlots.scale", "freeSlots", "hudScale", 75, 150),
    hudChoice<ui::HudElementId::FreeSlots, &ui::HudElement::background, lineBackgroundLabels>("hud.freeSlots.background", "freeSlots", "hudBackground"),
    hudToggle<ui::HudElementId::FreeSlots, &ui::HudElement::shadow>("hud.freeSlots.shadow", "freeSlots", "hudShadow"),
    toggle<&Settings::information, &Settings::Information::saturation>("information.saturation", "saturation", "saturation"),
    toggle<&Settings::information, &Settings::Information::saturationPreview>("information.saturationPreview", "saturation", "saturationPreview"),
    toggle<&Settings::information, &Settings::Information::hud>("information.hud", "infoHud", "infoHud"),
    toggle<&Settings::information, &Settings::Information::coordinates>("information.coordinates", "infoHud", "hudCoordinates"),
    toggle<&Settings::information, &Settings::Information::scaledCoordinates>("information.scaledCoordinates", "infoHud", "hudScaledCoordinatesRow"),
    toggle<&Settings::information, &Settings::Information::dimension>("information.dimension", "infoHud", "hudDimension"),
    toggle<&Settings::information, &Settings::Information::biome>("information.biome", "infoHud", "hudBiome"),
    {"information.biomeDisplay", "infoHud", "hudBiomeDisplay",
        [](Settings const& s) -> OptionValue {
            size_t mode = !s.information.biomeId ? 0 : s.information.biomeIdOnly ? 2 : 1;
            return ChoiceValue{biomeDisplayLabels[mode]};
        },
        [](Settings& s, int direction) {
            int mode = !s.information.biomeId ? 0 : s.information.biomeIdOnly ? 2 : 1;
            mode = (mode + (direction < 0 ? 2 : 1)) % 3;
            s.information.biomeId = mode != 0;
            s.information.biomeIdOnly = mode == 2;
        }},
    toggle<&Settings::information, &Settings::Information::difficulty>("information.difficulty", "infoHud", "debugDifficulty"),
    toggle<&Settings::information, &Settings::Information::facing>("information.facing", "infoHud", "hudFacing"),
    toggle<&Settings::information, &Settings::Information::yaw>("information.yaw", "infoHud", "hudYaw"),
    toggle<&Settings::information, &Settings::Information::pitch>("information.pitch", "infoHud", "hudPitch"),
    toggle<&Settings::information, &Settings::Information::sprinting>("information.sprinting", "infoHud", "hudSprintingRow"),
    toggle<&Settings::information, &Settings::Information::fps>("information.fps", "infoHud", "hudFps"),
    toggle<&Settings::information, &Settings::Information::frameTime>("information.frameTime", "infoHud", "hudFrameTime"),
    toggle<&Settings::information, &Settings::Information::light>("information.light", "infoHud", "hudLight"),
    toggle<&Settings::information, &Settings::Information::ping>("information.ping", "infoHud", "hudPing"),
    toggle<&Settings::information, &Settings::Information::rotation>("information.rotation", "infoHud", "hudRotation"),
    toggle<&Settings::information, &Settings::Information::block>("information.block", "infoHud", "hudBlock"),
    toggle<&Settings::information, &Settings::Information::chunk>("information.chunk", "infoHud", "hudChunk"),
    toggle<&Settings::information, &Settings::Information::speed>("information.speed", "infoHud", "hudSpeed"),
    toggle<&Settings::information, &Settings::Information::horizontalSpeed>("information.horizontalSpeed", "infoHud", "hudHorizontalSpeed"),
    toggle<&Settings::information, &Settings::Information::verticalSpeed>("information.verticalSpeed", "infoHud", "hudVerticalSpeed"),
    toggle<&Settings::information, &Settings::Information::time>("information.time", "infoHud", "hudTime"),
    toggle<&Settings::information, &Settings::Information::realTime>("information.realTime", "infoHud", "hudRealTime"),
    choice<&Settings::information, &Settings::Information::realTimeDate, realTimeDisplayLabels>(
        "information.realTimeDisplay", "infoHud", "hudRealTimeDisplay"),
    toggle<&Settings::information, &Settings::Information::weather>("information.weather", "infoHud", "hudWeather"),
    toggle<&Settings::information, &Settings::Information::moon>("information.moon", "infoHud", "hudMoon"),
    toggle<&Settings::inventory, &Settings::Inventory::toolSwitch>("inventory.toolSwitch", "toolSwitch", "toolSwitch"),
    toggle<&Settings::inventory, &Settings::Inventory::toolSwitchInventory>("inventory.toolSwitchInventory", "toolSwitch", "toolSwitchInventory"),
    choice<&Settings::inventory, &Settings::Inventory::toolSwitchSlot, fetchSlotLabels>("inventory.toolSwitchSlot", "toolSwitch", "fetchSlot"),
    toggle<&Settings::inventory, &Settings::Inventory::weaponSwitch>("inventory.weaponSwitch", "weaponSwitch", "weaponSwitch"),
    toggle<&Settings::inventory, &Settings::Inventory::weaponSwitchInventory>("inventory.weaponSwitchInventory", "weaponSwitch", "weaponSwitchInventory"),
    choice<&Settings::inventory, &Settings::Inventory::weaponSwitchSlot, fetchSlotLabels>("inventory.weaponSwitchSlot", "weaponSwitch", "fetchSlot"),
    toggle<&Settings::inventory, &Settings::Inventory::handRestock>("inventory.handRestock", "handRestock", "handRestock"),
    toggle<&Settings::inventory, &Settings::Inventory::restockFromHotbar>("inventory.restockFromHotbar", "handRestock", "restockFromHotbar"),
    toggle<&Settings::inventory, &Settings::Inventory::restockOffhand>("inventory.restockOffhand", "handRestock", "restockOffhand"),
    {"inventory.restockThreshold", "handRestock", "restockThreshold",
        [](Settings const& s) -> OptionValue { return static_cast<float>(s.inventory.restockThreshold); },
        [](Settings& s, int direction) { s.inventory.restockThreshold += direction; s.normalize(); },
        NumericOption{0, 63, [](Settings& s, float v) { s.inventory.restockThreshold = static_cast<int>(v + .5f); }, 1}},
    choice<&Settings::inventory, &Settings::Inventory::restockOrder, restockOrderLabels>("inventory.restockOrder", "handRestock", "restockOrder"),
    toggle<&Settings::inventory, &Settings::Inventory::fakeOffhand>("inventory.fakeOffhand", "fakeOffhand", "fakeOffhand"),
    {"inventory.fakeOffhandSlot", "fakeOffhand", "fakeOffhandSlot",
        [](Settings const& s) -> OptionValue {
            constexpr std::array<std::string_view, 9> labels = {
                "hotbarSlot.1", "hotbarSlot.2", "hotbarSlot.3", "hotbarSlot.4", "hotbarSlot.5",
                "hotbarSlot.6", "hotbarSlot.7", "hotbarSlot.8", "hotbarSlot.9"};
            return ChoiceValue{labels[std::clamp(s.inventory.fakeOffhandSlot, 1, 9) - 1]};
        },
        [](Settings& s, int direction) {
            int next = s.inventory.fakeOffhandSlot + (direction < 0 ? -1 : 1);
            s.inventory.fakeOffhandSlot = next < 1 ? 9 : next > 9 ? 1 : next;
        }},
    toggle<&Settings::overlays, &Settings::Overlays::hitboxes>("overlays.hitboxes", "hitboxes", "hitboxes"),
    toggle<&Settings::overlays, &Settings::Overlays::light>("overlays.light", "lightOverlay", "lightOverlay"),
    choice<&Settings::overlays, &Settings::Overlays::lightValue, overlay::lightValueLabels>("overlays.lightValue", "lightOverlay", "lightValueRow"),
    {"overlays.lightRange", "lightOverlay", "lightRange",
        [](Settings const& s) -> OptionValue { return s.overlays.lightRange; },
        [](Settings& s, int direction) { s.overlays.lightRange += direction; s.normalize(); },
        NumericOption{4, 64, [](Settings& s, float v) { s.overlays.lightRange = v; }, 1}},
    choice<&Settings::overlays, &Settings::Overlays::lightFacing, overlay::lightFacingLabels>("overlays.lightFacing", "lightOverlay", "lightFacingRow"),
    {"overlays.hitboxDistance", "hitboxes", "hitboxDistance",
        [](Settings const& s) -> OptionValue { return s.overlays.hitboxDistance; },
        [](Settings& s, int direction) { s.overlays.hitboxDistance += direction * 8.f; s.normalize(); },
        NumericOption{8, 128, [](Settings& s, float v) { s.overlays.hitboxDistance = v; }, 8}},
    toggle<&Settings::visuals, &Settings::Visuals::hideOffhand>("visuals.hideOffhand", "hideOffhand", "hideOffhand"),
    toggle<&Settings::visuals, &Settings::Visuals::hideEffects>("visuals.hideEffects", "hideEffects", "hideEffects"),
    toggle<&Settings::visuals, &Settings::Visuals::hideBossBars>("visuals.hideBossBars", "hideEffects", "hideBossBars"),
    toggle<&Settings::visuals, &Settings::Visuals::hideNausea>("visuals.hideNausea", "hideEffects", "hideNausea"),
    toggle<&Settings::visuals, &Settings::Visuals::hideWeather>("visuals.hideWeather", "hideEffects", "hideWeather"),
    toggle<&Settings::visuals, &Settings::Visuals::hideParticles>("visuals.hideParticles", "hideEffects", "hideParticles"),
    toggle<&Settings::visuals, &Settings::Visuals::hideWater>("visuals.hideWater", "hideEffects", "hideWater"),
    toggle<&Settings::visuals, &Settings::Visuals::hideLava>("visuals.hideLava", "hideEffects", "hideLava"),
    toggle<&Settings::visuals, &Settings::Visuals::hidePowderSnow>("visuals.hidePowderSnow", "hideEffects", "hidePowderSnow"),
    toggle<&Settings::visuals, &Settings::Visuals::hideDistanceFog>("visuals.hideDistanceFog", "hideEffects", "hideDistanceFog"),
    toggle<&Settings::interaction, &Settings::Interaction::edgeGuard>("interaction.edgeGuard", "edgeGuard", "edgeGuard"),
    toggle<&Settings::interaction, &Settings::Interaction::toolGuard>("interaction.toolGuard", "toolGuard", "toolGuard"),
    toggle<&Settings::interaction, &Settings::Interaction::toolGuardStrict>("interaction.toolGuardStrict", "toolGuard", "toolGuardStrict"),
    toggle<&Settings::interaction, &Settings::Interaction::elytraSwap>("interaction.elytraSwap", "elytraSwap", "elytraSwap"),
    toggle<&Settings::interaction, &Settings::Interaction::elytraFireworkJump>("interaction.elytraFireworkJump", "elytraSwap", "elytraFireworkJump"),
    {"interaction.elytraReturnSeconds", "elytraSwap", "elytraReturnSeconds",
        [](Settings const& s) -> OptionValue { return s.interaction.elytraReturnSeconds; },
        [](Settings& s, int direction) { s.interaction.elytraReturnSeconds += direction * .5f; s.normalize(); },
        NumericOption{0, 10, [](Settings& s, float v) { s.interaction.elytraReturnSeconds = v; }, .5f}},
    toggle<&Settings::overlays, &Settings::Overlays::chunkBorders>("overlays.chunkBorders", "chunkBorders", "chunkBorders"),
    toggle<&Settings::overlays, &Settings::Overlays::shapes>("overlays.shapes", "shapes", "shapeRendering"),
    choice<&Settings::camera, &Settings::Camera::zoomToggle, activationLabels>("camera.zoomActivation", "zoom", "zoomActivation"),
    choice<&Settings::camera, &Settings::Camera::freelookToggle, activationLabels>("camera.freelookActivation", "freelook", "freelookActivation"),
    choice<&Settings::camera, &Settings::Camera::freelookStartPerspective, perspectiveLabels>("camera.freelookStartPerspective", "freelook", "freelookStartPerspective"),
    choice<&Settings::camera, &Settings::Camera::freeCameraToggle, activationLabels>("camera.freecameraActivation", "freecamera", "freecameraActivation"),
    choice<&Settings::camera, &Settings::Camera::freeCameraWorldFixed, cameraReferenceLabels>("camera.freeCameraWorldFixed", "freecamera", "freeCameraReference"),
    toggle<&Settings::camera, &Settings::Camera::freeCameraLeaveOnHit>("camera.freeCameraLeaveOnHit", "freecamera", "freeCameraLeaveOnHit"),
    {"camera.freeCameraSpeed", "freecamera", "freeCameraSpeed",
        [](Settings const& s) -> OptionValue { return s.camera.freeCameraSpeed; },
        [](Settings& s, int direction) { s.camera.freeCameraSpeed = camera::adjustFlightSpeed(s.camera.freeCameraSpeed, direction); },
        NumericOption{5, 100, [](Settings& s, float v) { s.camera.freeCameraSpeed = v; }, 5}},
    {"camera.magnification", "zoom", "magnification",
        [](Settings const& s) -> OptionValue { return s.camera.magnification; },
        [](Settings& s, int direction) { s.camera.magnification += direction * .5f; s.normalize(); },
        NumericOption{2, 50, [](Settings& s, float v) { s.camera.magnification = v; }, .5f}},
    toggle<&Settings::camera, &Settings::Camera::showMagnification>("camera.showMagnification", "zoom", "showMagnification"),
    toggle<&Settings::lighting, &Settings::Lighting::nightVision>("lighting.nightVision", "nightVision", "nightVision"),
    toggle<&Settings::lighting, &Settings::Lighting::nightVisionEven>("lighting.nightVisionEven", "nightVision", "nightVisionEven"),
    toggle<&Settings::inspection, &Settings::Inspection::containerPreviews>("inspection.containerPreviews", "previews", "previews"),
    toggle<&Settings::inspection, &Settings::Inspection::shulkerPreviews>("inspection.shulkerPreviews", "previews", "shulkerPreviews"),
    toggle<&Settings::inspection, &Settings::Inspection::emptyShulkerPreviews>("inspection.emptyShulkerPreviews", "previews", "emptyShulkerPreviews"),
    toggle<&Settings::inspection, &Settings::Inspection::hideShulkerContents>("inspection.hideShulkerContents", "previews", "hideShulkerContents"),
    toggle<&Settings::inspection, &Settings::Inspection::bundlePreviews>("inspection.bundlePreviews", "previews", "bundlePreviews"),
    toggle<&Settings::inspection, &Settings::Inspection::emptyBundlePreviews>("inspection.emptyBundlePreviews", "previews", "emptyBundlePreviews"),
    toggle<&Settings::inspection, &Settings::Inspection::durability>("inspection.durability", "durability", "durability"),
    toggle<&Settings::inspection, &Settings::Inspection::foodValues>("inspection.foodValues", "foodValues", "foodValues"),
    toggle<&Settings::inspection, &Settings::Inspection::lockedTrades>("inspection.lockedTrades", "lockedTrades", "lockedTrades"),
    toggle<&Settings::inspection, &Settings::Inspection::englishSearch>("inspection.englishSearch", "englishSearch", "englishSearch"),
    toggle<&Settings::inventory, &Settings::Inventory::sorting>("inventory.sorting", "sorting", "sorting"),
    toggle<&Settings::inventory, &Settings::Inventory::offhandSwap>("inventory.offhandSwap", "offhandSwap", "offhandSwap"),
    toggle<&Settings::inventory, &Settings::Inventory::offhandSwapFireworks>("inventory.offhandSwapFireworks", "offhandSwap", "offhandSwapFireworks"),
    toggle<&Settings::inventory, &Settings::Inventory::deathRestore>("inventory.deathRestore", "deathRestore", "deathRestore"),
    toggle<&Settings::inventory, &Settings::Inventory::deathRestoreAll>("inventory.deathRestoreAll", "deathRestore", "deathRestoreAll"),
    toggle<&Settings::inventory, &Settings::Inventory::sortContainers>("inventory.sortContainers", "sorting", "storage"),
    toggle<&Settings::inventory, &Settings::Inventory::transfer>("inventory.transfer", "transfer", "transfer"),
    toggle<&Settings::inventory, &Settings::Inventory::transferWheelOne>("inventory.transferWheelOne", "transfer", "transferWheelOne"),
    toggle<&Settings::inventory, &Settings::Inventory::transferWheelStack>("inventory.transferWheelStack", "transfer", "transferWheelStack"),
    toggle<&Settings::inventory, &Settings::Inventory::transferDragStack>("inventory.transferDragStack", "transfer", "transferDragStack"),
    toggle<&Settings::inventory, &Settings::Inventory::transferDragOne>("inventory.transferDragOne", "transfer", "transferDragOne"),
    toggle<&Settings::ui, &Settings::Interface::toggleToasts>("interface.toggleToasts", "toasts", "toggleToasts"),
    choice<&Settings::ui, &Settings::Interface::animations, animationLabels>("interface.animations", "settings", "animations"),
    {"interface.hudRowHeight", "hudText", "hudRowHeight",
        [](Settings const& s) -> OptionValue { return static_cast<float>(s.ui.hudRowHeight); },
        [](Settings& s, int direction) { s.ui.hudRowHeight += direction; s.normalize(); },
        NumericOption{9, 16, [](Settings& s, float v) { s.ui.hudRowHeight = static_cast<int>(std::lround(v)); }, 1}},
    {"interface.hudBackgroundOpacity", "hudText", "hudBackgroundOpacity",
        [](Settings const& s) -> OptionValue { return static_cast<float>(s.ui.hudBackgroundOpacity); },
        [](Settings& s, int direction) { s.ui.hudBackgroundOpacity += direction * 4; s.normalize(); },
        NumericOption{0, 100, [](Settings& s, float v) { s.ui.hudBackgroundOpacity = static_cast<int>(std::lround(v)); }, 4}},
    toggle<&Settings::ui, &Settings::Interface::automationStatus>("interface.automationStatus", "automationStatus", "automationStatus"),
    hudNumeric<ui::HudElementId::Info, &ui::HudElement::scale, 25>("hud.info.scale", "infoHud", "hudScale", 75, 150),
    hudChoice<ui::HudElementId::Info, &ui::HudElement::background, lineBackgroundLabels>("hud.info.background", "infoHud", "hudBackground"),
    hudToggle<ui::HudElementId::Info, &ui::HudElement::shadow>("hud.info.shadow", "infoHud", "hudShadow"),
    hudNumeric<ui::HudElementId::Target, &ui::HudElement::scale, 25>("hud.target.scale", "targetInfo", "hudScale", 75, 150),
    hudChoice<ui::HudElementId::Target, &ui::HudElement::background, elementBackgroundLabels>("hud.target.background", "targetInfo", "hudBackground"),
    hudToggle<ui::HudElementId::Target, &ui::HudElement::shadow>("hud.target.shadow", "targetInfo", "hudShadow"),
    hudNumeric<ui::HudElementId::Status, &ui::HudElement::scale, 25>("hud.status.scale", "automationStatus", "hudScale", 75, 150),
    hudChoice<ui::HudElementId::Status, &ui::HudElement::background, lineBackgroundLabels>("hud.status.background", "automationStatus", "hudBackground"),
    hudToggle<ui::HudElementId::Status, &ui::HudElement::shadow>("hud.status.shadow", "automationStatus", "hudShadow"),
    hudNumeric<ui::HudElementId::Toast, &ui::HudElement::scale, 25>("hud.toast.scale", "toasts", "hudScale", 75, 150),
    hudChoice<ui::HudElementId::Toast, &ui::HudElement::background, elementBackgroundLabels>("hud.toast.background", "toasts", "hudBackground"),
    hudToggle<ui::HudElementId::Toast, &ui::HudElement::shadow>("hud.toast.shadow", "toasts", "hudShadow"),
    hudNumeric<ui::HudElementId::Magnification, &ui::HudElement::scale, 25>("hud.magnification.scale", "zoom", "hudScale", 75, 150),
    hudChoice<ui::HudElementId::Magnification, &ui::HudElement::background, elementBackgroundLabels>("hud.magnification.background", "zoom", "hudBackground"),
    hudToggle<ui::HudElementId::Magnification, &ui::HudElement::shadow>("hud.magnification.shadow", "zoom", "hudShadow"),
    hudNumeric<ui::HudElementId::Durability, &ui::HudElement::scale, 25>("hud.durability.scale", "durabilityHud", "hudScale", 75, 150),
    hudChoice<ui::HudElementId::Durability, &ui::HudElement::background, elementBackgroundLabels>("hud.durability.background", "durabilityHud", "hudBackground"),
    hudToggle<ui::HudElementId::Durability, &ui::HudElement::shadow>("hud.durability.shadow", "durabilityHud", "hudShadow"),
    toggle<&Settings::map, &Settings::Map::minimap>("map.minimap", "minimap", "minimap"),
    choice<&Settings::map, &Settings::Map::zoom, mapZoomLabels>("map.zoom", "minimap", "mapZoom"),
    {"map.size", "minimap", "mapSize",
        [](Settings const& s) -> OptionValue { return s.map.size; },
        [](Settings& s, int direction) { s.map.size += direction; s.normalize(); },
        NumericOption{10, 50, [](Settings& s, float v) { s.map.size = v; }, 1}},
    toggle<&Settings::map, &Settings::Map::rotate>("map.rotate", "minimap", "mapRotate"),
    toggle<&Settings::map, &Settings::Map::round>("map.round", "minimap", "mapRound"),
    toggle<&Settings::map, &Settings::Map::compass>("map.compass", "mapText", "mapCompass"),
    toggle<&Settings::map, &Settings::Map::coordinates>("map.coordinates", "mapText", "mapCoordinates"),
    toggle<&Settings::map, &Settings::Map::biome>("map.biome", "mapText", "mapBiome"),
    toggle<&Settings::map, &Settings::Map::debugHide>("map.debugHide", "minimap", "mapDebugHide"),
    toggle<&Settings::map, &Settings::Map::radar>("map.radar", "radar", "mapRadar"),
    toggle<&Settings::map, &Settings::Map::radarFaces>("map.radarFaces", "radar", "mapRadarFaces"),
    toggle<&Settings::map, &Settings::Map::radarPlayers>("map.radarPlayers", "radar", "mapRadarPlayers"),
    toggle<&Settings::map, &Settings::Map::radarHostile>("map.radarHostile", "radar", "mapRadarHostile"),
    toggle<&Settings::map, &Settings::Map::radarPassive>("map.radarPassive", "radar", "mapRadarPassive"),
    toggle<&Settings::map, &Settings::Map::radarItems>("map.radarItems", "radar", "mapRadarItems"),
    toggle<&Settings::map, &Settings::Map::radarInvisible>("map.radarInvisible", "radar", "mapRadarInvisible"),
    toggle<&Settings::map, &Settings::Map::waypoints>("map.waypoints", "waypoints", "mapWaypoints"),
    choice<&Settings::map, &Settings::Map::waypointsWorld, worldMarkerLabels>("map.waypointsWorld", "waypoints", "mapWaypointsWorld"),
    {"map.waypointDistance", "waypoints", "mapWaypointDistance",
        [](Settings const& s) -> OptionValue { return s.map.waypointDistance; },
        [](Settings& s, int direction) { s.map.waypointDistance += direction * 100.f; s.normalize(); },
        NumericOption{0, 10000, [](Settings& s, float v) { s.map.waypointDistance = v; }, 100}},
    toggle<&Settings::map, &Settings::Map::waypointsMinimap>("map.waypointsMinimap", "waypoints", "mapWaypointsMinimap"),
    toggle<&Settings::map, &Settings::Map::waypointsDeath>("map.waypointsDeath", "waypoints", "mapWaypointsDeath"),
    toggle<&Settings::map, &Settings::Map::waypointsCrossScale>("map.waypointsCrossScale", "waypoints", "mapWaypointsCrossScale"),
    toggle<&Settings::map, &Settings::Map::worldMap>("map.worldMap", "worldMap", "worldMap"),
    toggle<&Settings::map, &Settings::Map::worldMapNetherAuto>("map.worldMapNetherAuto", "worldMap", "worldMapNetherAuto"),
    toggle<&Settings::map, &Settings::Map::seedLink>("map.seedLink", "worldMap", "mapSeedLink"),
    toggle<&Settings::schematic, &Settings::Schematic::enabled>("schematic.enabled", "schematic", "schematicShown"),
    choice<&Settings::schematic, &Settings::Schematic::menuBackground, menuBackgroundLabels>("schematic.menuBackground", "schematicMenu", "schematicMenuBackground"),
    toggle<&Settings::schematic, &Settings::Schematic::menuSmall>("schematic.menuSmall", "schematicMenu", "schematicMenuSmall"),
    toggle<&Settings::schematic, &Settings::Schematic::menuReopen>("schematic.menuReopen", "schematicMenu", "schematicMenuReopen"),
    {"schematic.outlineDistance", "schematic", "schematicOutlineDistance",
        [](Settings const& s) -> OptionValue { return s.schematic.outlineDistance; },
        [](Settings& s, int direction) { s.schematic.outlineDistance += direction * 16.f; s.normalize(); },
        NumericOption{0, 192, [](Settings& s, float v) { s.schematic.outlineDistance = v; }, 16}},
    toggle<&Settings::schematic, &Settings::Schematic::hud>("schematic.hud", "schematicHud", "schematicHud"),
    toggle<&Settings::schematic, &Settings::Schematic::hudVerify>("schematic.hudVerify", "schematicHud", "schematicHudVerify"),
    toggle<&Settings::schematic, &Settings::Schematic::hudMaterials>("schematic.hudMaterials", "schematicHud", "schematicHudMaterials"),
    hudNumeric<ui::HudElementId::Schematic, &ui::HudElement::scale, 25>("hud.schematic.scale", "schematicHud", "hudScale", 75, 150),
    hudChoice<ui::HudElementId::Schematic, &ui::HudElement::background, elementBackgroundLabels>("hud.schematic.background", "schematicHud", "hudBackground"),
    hudToggle<ui::HudElementId::Schematic, &ui::HudElement::shadow>("hud.schematic.shadow", "schematicHud", "hudShadow"),
    hudNumeric<ui::HudElementId::Minimap, &ui::HudElement::scale, 25>("hud.minimap.scale", "minimap", "hudScale", 75, 150),
    hudChoice<ui::HudElementId::Minimap, &ui::HudElement::background, elementBackgroundLabels>("hud.minimap.background", "minimap", "hudBackground"),
    hudToggle<ui::HudElementId::Minimap, &ui::HudElement::shadow>("hud.minimap.shadow", "minimap", "hudShadow"),
});
inline Option const* find(std::string_view id) {
    for (auto const& option : options) if (option.id == id) return &option;
    return nullptr;
}
// Set one option back to its value in `defaults` through its own accessors:
// numbers are written, switches and choices are stepped until they match.
inline void resetOption(Settings& value, Option const& option, Settings const& defaults) {
    auto target = option.read(defaults);
    if (auto const* number = std::get_if<float>(&target)) {
        if (option.numeric) option.numeric->write(value, *number);
        return;
    }
    for (int i = 0; i < 16 && option.read(value) != target; ++i) option.adjust(value, 1);
}
}
