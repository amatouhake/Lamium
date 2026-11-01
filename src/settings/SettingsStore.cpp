#include "settings/SettingsStore.h"
#include "settings/SettingsValue.h"
#include "nlohmann/json.hpp"
#include <array>
#include <fstream>
#include <stdexcept>
#include <system_error>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

namespace lamium {
namespace {
using Json = nlohmann::ordered_json;
Json parse(std::string_view text) {
    auto data = Json::parse(text, nullptr, true, true);
    if (!data.is_object()) throw std::runtime_error("Settings must be a JSON object");
    if (data.contains("version") && !data.at("version").is_number_integer())
        throw std::runtime_error("Settings version must be an integer");
    // Compare the JSON integer before conversion: narrowing a future version
    // to int could wrap back to 1 and allow an incompatible file to be rewritten.
    if (data.contains("version") && data.at("version") != Json(1))
        throw std::runtime_error("Unsupported settings version; original file preserved");
    return data;
}
std::string read(std::filesystem::path const& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) throw std::runtime_error("Could not open settings file");
    std::string text{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    if (stream.bad()) throw std::runtime_error("Could not read settings file");
    return text;
}
void preserveBroken(std::filesystem::path const& path) {
    auto broken = path;
    broken += ".broken";
#ifdef _WIN32
    if (!MoveFileExW(path.c_str(), broken.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "Preserving broken settings");
#else
    std::filesystem::rename(path, broken);
#endif
}
Json encodeHudElement(ui::HudElement const& element) {
    return {{"anchor", static_cast<int>(element.anchor)}, {"dx", element.dx},
            {"dy", element.dy}, {"scale", element.scale},
            {"background", static_cast<int>(element.background)}, {"shadow", element.shadow}};
}
void decodeHudElement(Json const& data, ui::HudElement& element, ui::HudElement defaultValue) {
    element.anchor = static_cast<ui::Anchor>(settingsValue(data, "anchor", static_cast<int>(defaultValue.anchor)));
    element.dx = settingsValue(data, "dx", defaultValue.dx);
    element.dy = settingsValue(data, "dy", defaultValue.dy);
    element.scale = settingsValue(data, "scale", defaultValue.scale);
    element.background = static_cast<ui::ElementBackground>(settingsValue(data, "background", static_cast<int>(defaultValue.background)));
    element.shadow = settingsValue(data, "shadow", defaultValue.shadow);
}
Json encode(Settings const& settings) {
    Json bindings = Json::object();
    for (size_t i = 0; i < input::actions.size(); ++i) {
        auto const& binding = settings.bindings[i];
        auto const id = std::string(input::actions[i].id);
        if (!binding) { bindings[id] = nullptr; continue; }
        bindings[id] = Json::array();
        for (auto token : input::canonicalChord(*binding, input::actions[i].behavior)) {
            auto device = token.device == input::Device::Key ? "key" : token.device == input::Device::Mouse ? "mouse" : "wheel";
            bindings[id].push_back({{"device", device}, {"code", token.code}});
        }
    }
    return Json{
        {"version", settings.version},
        {"orderedBindings", true},
        {"interaction", {{"attackTicks", settings.interaction.attackTicks}, {"useTicks", settings.interaction.useTicks},
                         {"attackClicks", settings.interaction.attackClicks}, {"useClicks", settings.interaction.useClicks},
                         {"attackMode", interaction::autoModeNames[static_cast<size_t>(settings.interaction.attackMode)]},
                         {"useMode", interaction::autoModeNames[static_cast<size_t>(settings.interaction.useMode)]},
                         {"attackHeldOnly", settings.interaction.attackHeldOnly}, {"useHeldOnly", settings.interaction.useHeldOnly},
                         {"breaking", settings.interaction.breaking}, {"edgeGuard", settings.interaction.edgeGuard},
                         {"edgeMargin", settings.interaction.edgeMargin},
                         {"sneakPlacement", settings.interaction.sneakPlacement},
                         {"sneakPlacementInvert", settings.interaction.sneakPlacementInvert},
                         {"sneakPlacementPacket", settings.interaction.sneakPlacementPacket},
                         {"toolGuard", settings.interaction.toolGuard}, {"elytraSwap", settings.interaction.elytraSwap},
                         {"toolGuardStrict", settings.interaction.toolGuardStrict},
                         {"elytraReturnSeconds", settings.interaction.elytraReturnSeconds},
                         {"elytraFireworkJump", settings.interaction.elytraFireworkJump},
                         {"elytraRiptide", settings.interaction.elytraRiptide},
                         {"breakingMode", interaction::restrictionNames[static_cast<size_t>(settings.interaction.breakingMode)]},
                         {"breakingBand", settings.interaction.breakingBand},
                         {"placementMode", interaction::restrictionNames[static_cast<size_t>(settings.interaction.placementMode)]}}},
        {"information", {{"hud", settings.information.hud}, {"coordinates", settings.information.coordinates},
                         {"scaledCoordinates", settings.information.scaledCoordinates},
                         {"debug", settings.information.debug}, {"debugLabels", settings.information.debugLabels},
                         {"debugHideHud", settings.information.debugHideHud},
                         {"debugHideTarget", settings.information.debugHideTarget},
                         {"debugHideVanilla", settings.information.debugHideVanilla},
                         {"hudHidePaperDoll", settings.information.hudHidePaperDoll},
                         {"debugShadow", settings.information.debugShadow},
                         {"debugBackground", settings.information.debugBackground},
                         {"debugEntityKinds", settings.information.debugEntityKinds},
                         {"debugEntityTypes", settings.information.debugEntityTypes},
                         {"target", settings.information.target}, {"targetIdentifier", settings.information.targetIdentifier},
                         {"targetIcon", settings.information.targetIcon},
                         {"targetHealth", settings.information.targetHealth},
                         {"targetArmor", settings.information.targetArmor},
                         {"targetGrowth", settings.information.targetGrowth},
                         {"targetDistance", settings.information.targetDistance},
                         {"targetStates", settings.information.targetStates},
                         {"targetCoordinates", settings.information.targetCoordinates},
                         {"targetSchematic", settings.information.targetSchematic},
                         {"targetLiquids", settings.information.targetLiquids},
                         {"durabilityHud", settings.information.durabilityHud},
                         {"durabilityLook", settings.information.durabilityLook},
                         {"durabilityOffhand", settings.information.durabilityOffhand},
                         {"durabilityArmor", settings.information.durabilityArmor},
                         {"durabilityAmmo", settings.information.durabilityAmmo},
                         {"durabilityElytra", settings.information.durabilityElytra},
                         {"offhandSlot", settings.information.offhandSlot},
                         {"offhandSlotEmpty", settings.information.offhandSlotEmpty},
                         {"playerListPlatform", settings.information.playerListPlatform},
                         {"playerListDimension", settings.information.playerListDimension},
                         {"playerListDistance", settings.information.playerListDistance},
                         {"playerListMembers", settings.information.playerListMembers},
                         {"inventoryHud", settings.information.inventoryHud},
                         {"usedSlots", settings.information.usedSlots},
                         {"inventoryHotbar", settings.information.inventoryHotbar},
                         {"usedSlotsHotbar", settings.information.usedSlotsHotbar},
                         {"effectsHud", settings.information.effectsHud},
                         {"chatPosition", settings.information.chatPosition},
                         {"effectsNames", settings.information.effectsNames},
                         {"effectsWarning", settings.information.effectsWarning},
                         {"effectsHideVanilla", settings.information.effectsHideVanilla},
                         {"saturation", settings.information.saturation},
                         {"saturationPreview", settings.information.saturationPreview},
                         {"lineOrder", settings.information.lineOrder},
                         {"biome", settings.information.biome}, {"biomeId", settings.information.biomeId},
                         {"biomeIdOnly", settings.information.biomeIdOnly},
                         {"difficulty", settings.information.difficulty}, {"facing", settings.information.facing},
                         {"yaw", settings.information.yaw}, {"pitch", settings.information.pitch},
                         {"sprinting", settings.information.sprinting},
                         {"fps", settings.information.fps}, {"frameTime", settings.information.frameTime},
                         {"light", settings.information.light},
                         {"ping", settings.information.ping},
                         {"rotation", settings.information.rotation}, {"block", settings.information.block},
                         {"targetBlock", settings.information.targetBlock},
                         {"chunk", settings.information.chunk}, {"slimeChunk", settings.information.slimeChunk},
                         {"speed", settings.information.speed},
                         {"horizontalSpeed", settings.information.horizontalSpeed},
                         {"verticalSpeed", settings.information.verticalSpeed},
                         {"time", settings.information.time}, {"realTime", settings.information.realTime},
                         {"realTimeDate", settings.information.realTimeDate},
                         {"weather", settings.information.weather},
                         {"moon", settings.information.moon},
                         {"dimension", settings.information.dimension}}},
        {"visuals", {{"hideOffhand", settings.visuals.hideOffhand},
                     {"hideEffects", settings.visuals.hideEffects},
                     {"connectedTextures", settings.visuals.connectedTextures},
                     {"connectedCorners", settings.visuals.connectedCorners},
                     {"connectedPaneCorners", settings.visuals.connectedPaneCorners},
                     {"hideWeather", settings.visuals.hideWeather}, {"hideParticles", settings.visuals.hideParticles},
                     {"hideBossBars", settings.visuals.hideBossBars}, {"hideNausea", settings.visuals.hideNausea},
                     {"hideWater", settings.visuals.hideWater}, {"hideLava", settings.visuals.hideLava},
                     {"hidePowderSnow", settings.visuals.hidePowderSnow},
                     {"hideDistanceFog", settings.visuals.hideDistanceFog},
                     {"hidePortal", settings.visuals.hidePortal},
                     {"skipServerPacks", settings.visuals.skipServerPacks}}},
        {"overlays", {{"chunkBorders", settings.overlays.chunkBorders}, {"hitboxes", settings.overlays.hitboxes}, {"shapes", settings.overlays.shapes},
                      {"light", settings.overlays.light},
                      {"lightValue", overlay::lightValueNames[static_cast<size_t>(settings.overlays.lightValue)]},
                      {"lightRange", settings.overlays.lightRange},
                      {"lightFacing", overlay::lightFacingNames[static_cast<size_t>(settings.overlays.lightFacing)]},
                      {"hitboxDistance", settings.overlays.hitboxDistance}}},
        {"bindings", std::move(bindings)},
        {"camera", {{"zoomToggle", settings.camera.zoomToggle}, {"freelookToggle", settings.camera.freelookToggle},
                    {"freelookStartPerspective", settings.camera.freelookStartPerspective}, {"freeCameraToggle", settings.camera.freeCameraToggle},
                    {"freeCameraSpeed", settings.camera.freeCameraSpeed},
                    {"freeCameraWorldFixed", settings.camera.freeCameraWorldFixed},
                    {"freeCameraLeaveOnHit", settings.camera.freeCameraLeaveOnHit},
                    {"freeCameraHide", settings.camera.freeCameraHide},
                    {"freeCameraUndergroundHold", settings.camera.freeCameraUndergroundHold},
                    {"magnification", settings.camera.magnification},
                    {"zoomWideAngle", settings.camera.zoomWideAngle},
                    {"showMagnification", settings.camera.showMagnification}}},
        {"lighting", {{"nightVision", settings.lighting.nightVision}, {"nightVisionEven", settings.lighting.nightVisionEven}}},
        {"inspection", {{"containerPreviews", settings.inspection.containerPreviews},
                        {"shulkerPreviews", settings.inspection.shulkerPreviews},
                        {"emptyShulkerPreviews", settings.inspection.emptyShulkerPreviews},
                        {"hideShulkerContents", settings.inspection.hideShulkerContents},
                        {"bundlePreviews", settings.inspection.bundlePreviews},
                        {"emptyBundlePreviews", settings.inspection.emptyBundlePreviews},
                        {"durability", settings.inspection.durability},
                        {"foodValues", settings.inspection.foodValues},
                        {"lockedTrades", settings.inspection.lockedTrades},
                        {"englishSearch", settings.inspection.englishSearch}}},
        {"inventory", {{"sorting", settings.inventory.sorting}, {"sortContainers", settings.inventory.sortContainers},
                       {"repeatTrades", settings.inventory.repeatTrades},
                       {"offhandSwap", settings.inventory.offhandSwap},
                       {"deathRestore", settings.inventory.deathRestore},
                       {"deathRestoreAll", settings.inventory.deathRestoreAll},
                       {"offhandSwapFireworks", settings.inventory.offhandSwapFireworks},
                       {"transfer", settings.inventory.transfer},
                       {"transferWheelOne", settings.inventory.transferWheelOne},
                       {"transferWheelStack", settings.inventory.transferWheelStack},
                       {"transferDragStack", settings.inventory.transferDragStack},
                       {"transferDragOne", settings.inventory.transferDragOne},
                       {"transferWheelMatching", settings.inventory.transferWheelMatching},
                       {"transferWheelAll", settings.inventory.transferWheelAll},
                       {"transferClickMatching", settings.inventory.transferClickMatching},
                       {"transferClickAll", settings.inventory.transferClickAll},
                       {"transferDragKeepOne", settings.inventory.transferDragKeepOne},
                       {"transferCursorMatching", settings.inventory.transferCursorMatching},
                       {"transferDropOne", settings.inventory.transferDropOne},
                       {"transferDropKeepOne", settings.inventory.transferDropKeepOne},
                       {"transferDropAll", settings.inventory.transferDropAll},
                       {"transferDropMatching", settings.inventory.transferDropMatching},
                       {"toolSwitch", settings.inventory.toolSwitch}, {"handRestock", settings.inventory.handRestock},
                       {"restockFromHotbar", settings.inventory.restockFromHotbar},
                       {"restockOffhand", settings.inventory.restockOffhand},
                       {"restockShulkerBoxes", settings.inventory.restockShulkerBoxes},
                       {"restockBuckets", settings.inventory.restockBuckets},
                       {"restockThreshold", settings.inventory.restockThreshold},
                       {"restockOrder", settings.inventory.restockOrder},
                       {"toolSwitchInventory", settings.inventory.toolSwitchInventory},
                       {"weaponSwitch", settings.inventory.weaponSwitch},
                       {"weaponSwitchInventory", settings.inventory.weaponSwitchInventory},
                       {"toolSwitchSlot", settings.inventory.toolSwitchSlot},
                       {"weaponSwitchSlot", settings.inventory.weaponSwitchSlot},
                       {"fakeOffhand", settings.inventory.fakeOffhand}, {"fakeOffhandSlot", settings.inventory.fakeOffhandSlot}}},
        {"interface", {{"toggleToasts", settings.ui.toggleToasts}, {"automationStatus", settings.ui.automationStatus},
                       {"bookPaste", settings.ui.bookPaste},
                       {"animations", settings.ui.animations}, {"hudRowHeight", settings.ui.hudRowHeight},
                       {"hudBackgroundOpacity", settings.ui.hudBackgroundOpacity}}},
        {"hud", {{"info", encodeHudElement(settings.hud.info)}, {"target", encodeHudElement(settings.hud.target)},
                   {"status", encodeHudElement(settings.hud.status)}, {"toast", encodeHudElement(settings.hud.toast)},
                   {"magnification", encodeHudElement(settings.hud.magnification)},
                   {"durability", encodeHudElement(settings.hud.durability)},
                   {"minimap", encodeHudElement(settings.hud.minimap)},
                   {"schematic", encodeHudElement(settings.hud.schematic)},
                   {"playerList", encodeHudElement(settings.hud.playerList)},
                   {"inventory", encodeHudElement(settings.hud.inventory)},
                   {"usedSlots", encodeHudElement(settings.hud.usedSlots)},
                   {"statusEffects", encodeHudElement(settings.hud.statusEffects)},
                   {"chat", encodeHudElement(settings.hud.chat)}}},
        {"map", {{"minimap", settings.map.minimap}, {"range", map::blocksAcross(settings.map.zoom)}, {"size", settings.map.size},
                 {"rotate", settings.map.rotate},
                 {"round", settings.map.round}, {"caveView", settings.map.caveView},
                 {"coordinates", settings.map.coordinates},
                 {"biome", settings.map.biome}, {"compass", settings.map.compass},
                 {"debugHide", settings.map.debugHide}, {"minimapShapes", settings.map.minimapShapes}, {"radar", settings.map.radar},
                 {"radarPlayers", settings.map.radarPlayers}, {"radarHostile", settings.map.radarHostile},
                 {"radarPassive", settings.map.radarPassive}, {"radarItems", settings.map.radarItems},
                 {"radarInvisible", settings.map.radarInvisible}, {"waypoints", settings.map.waypoints},
                 {"waypointsWorld", settings.map.waypointsWorld}, {"waypointDistance", settings.map.waypointDistance},
                 {"waypointsMinimap", settings.map.waypointsMinimap}, {"waypointsDeath", settings.map.waypointsDeath},
                 {"waypointsCrossScale", settings.map.waypointsCrossScale}, {"waypointArrival", settings.map.waypointArrival},
                 {"worldMap", settings.map.worldMap},
                 {"worldMapNetherAuto", settings.map.worldMapNetherAuto}, {"worldMapPanel", settings.map.worldMapPanel},
                 {"seedLink", settings.map.seedLink}, {"worldMapNames", settings.map.worldMapNames},
                 {"radarFaces", settings.map.radarFaces}, {"worldMapWaypoints", settings.map.worldMapWaypoints},
                 {"worldMapSchematics", settings.map.worldMapSchematics}, {"worldMapShapes", settings.map.worldMapShapes},
                 {"worldMapSlime", settings.map.worldMapSlime}, {"worldMapList", settings.map.worldMapList}}},
        {"schematic", {{"enabled", settings.schematic.enabled}, {"hud", settings.schematic.hud},
                       {"hudVerify", settings.schematic.hudVerify}, {"hudMaterials", settings.schematic.hudMaterials},
                       {"menuBackground", settings.schematic.menuBackground}, {"menuSmall", settings.schematic.menuSmall},
                       {"repeatAllSteps", settings.schematic.repeatAllSteps},
                       {"ghostTintPercent", settings.schematic.ghostTint}, {"ghostLines", settings.schematic.ghostLines},
                       {"menuReopen", settings.schematic.menuReopen}, {"outlineDistance", settings.schematic.outlineDistance},
                       {"mistakeMarks", settings.schematic.mistakeMarks}, {"ignoreSignal", settings.schematic.ignoreSignal},
                       {"litAsState", settings.schematic.litAsState}, {"ignoreGrowth", settings.schematic.ignoreGrowth}}}
    };
}
}
Settings decodeSettings(std::string_view text) {
    auto data = parse(text);
    Settings value;
    if (data.contains("interaction") && data.at("interaction").is_object()) {
        auto const& options = data.at("interaction");
        // Older files stored the periodic interval in seconds.
        auto ticks = [&](char const* key, char const* seconds) {
            if (options.contains(key)) return settingsValue(options, key, 10.f);
            return settingsValue(options, seconds, .5f) * 20;
        };
        value.interaction.attackTicks = ticks("attackTicks", "attackInterval");
        value.interaction.useTicks = ticks("useTicks", "useInterval");
        value.interaction.attackClicks = settingsValue(options, "attackClicks", value.interaction.attackClicks);
        value.interaction.useClicks = settingsValue(options, "useClicks", value.interaction.useClicks);
        // An unknown mode name falls back to Periodic rather than failing the load.
        auto autoMode = [&](char const* key) {
            auto name = settingsValue(options, key, std::string("periodic"));
            for (size_t i = 0; i < interaction::autoModeNames.size(); ++i)
                if (interaction::autoModeNames[i] == name) return static_cast<interaction::AutoMode>(i);
            return interaction::AutoMode::Periodic;
        };
        value.interaction.attackMode = autoMode("attackMode");
        value.interaction.useMode = autoMode("useMode");
        // A short-lived build saved this as "attackTrigger": "held".
        auto heldOnly = [&](char const* key, char const* trigger) {
            if (options.contains(key)) return settingsValue(options, key, false);
            return settingsValue(options, trigger, std::string("always")) == "held";
        };
        value.interaction.attackHeldOnly = heldOnly("attackHeldOnly", "attackTrigger");
        value.interaction.useHeldOnly = heldOnly("useHeldOnly", "useTrigger");
        value.interaction.breaking = settingsValue(options, "breaking", value.interaction.breaking);
        value.interaction.edgeGuard = settingsValue(options, "edgeGuard", value.interaction.edgeGuard);
        value.interaction.edgeMargin = settingsValue(options, "edgeMargin", value.interaction.edgeMargin);
        value.interaction.sneakPlacement = settingsValue(options, "sneakPlacement", value.interaction.sneakPlacement);
        value.interaction.sneakPlacementInvert = settingsValue(options, "sneakPlacementInvert", value.interaction.sneakPlacementInvert);
        value.interaction.sneakPlacementPacket = settingsValue(options, "sneakPlacementPacket", value.interaction.sneakPlacementPacket);
        value.interaction.toolGuard = settingsValue(options, "toolGuard", value.interaction.toolGuard);
        value.interaction.toolGuardStrict = settingsValue(options, "toolGuardStrict", value.interaction.toolGuardStrict);
        value.interaction.elytraSwap = settingsValue(options, "elytraSwap", value.interaction.elytraSwap);
        value.interaction.elytraReturnSeconds = settingsValue(options, "elytraReturnSeconds", value.interaction.elytraReturnSeconds);
        value.interaction.elytraFireworkJump = settingsValue(options, "elytraFireworkJump", value.interaction.elytraFireworkJump);
        value.interaction.elytraRiptide = settingsValue(options, "elytraRiptide", value.interaction.elytraRiptide);
        auto mode = [&](char const* key) {
            auto name = settingsValue(options, key,std::string("plane"));
            for (size_t i=0;i<interaction::restrictionNames.size();++i)
                if (name == interaction::restrictionNames[i]) return static_cast<interaction::RestrictionMode>(i);
            // An unknown name keeps the rest of the file; only this mode falls
            // back, like an unknown auto mode above.
            return interaction::RestrictionMode::Plane;
        };
        value.interaction.breakingMode = mode("breakingMode");
        value.interaction.placementMode = mode("placementMode");
        value.interaction.breakingBand = settingsValue(options, "breakingBand", value.interaction.breakingBand);
    }
    if (data.contains("information") && data.at("information").is_object()) {
        auto const& info = data.at("information");
        value.information.debug = settingsValue(info, "debug", value.information.debug);
        value.information.debugLabels = settingsValue(info, "debugLabels", value.information.debugLabels);
        value.information.debugHideHud = settingsValue(info, "debugHideHud", value.information.debugHideHud);
        value.information.debugHideTarget = settingsValue(info, "debugHideTarget", value.information.debugHideTarget);
        value.information.debugHideVanilla = settingsValue(info, "debugHideVanilla", value.information.debugHideVanilla);
        value.information.hudHidePaperDoll = settingsValue(info, "hudHidePaperDoll", value.information.hudHidePaperDoll);
        value.information.debugShadow = settingsValue(info, "debugShadow", value.information.debugShadow);
        value.information.debugBackground = settingsValue(info, "debugBackground", value.information.debugBackground);
        value.information.debugEntityKinds = settingsValue(info, "debugEntityKinds", value.information.debugEntityKinds);
        value.information.debugEntityTypes = settingsValue(info, "debugEntityTypes", value.information.debugEntityTypes);
        value.information.target = settingsValue(info, "target", value.information.target);
        value.information.targetIdentifier = settingsValue(info, "targetIdentifier", value.information.targetIdentifier);
        value.information.targetStates = settingsValue(info, "targetStates", value.information.targetStates);
        value.information.targetIcon = settingsValue(info, "targetIcon", value.information.targetIcon);
        value.information.targetHealth = settingsValue(info, "targetHealth", value.information.targetHealth);
        value.information.targetArmor = settingsValue(info, "targetArmor", value.information.targetArmor);
        value.information.targetGrowth = settingsValue(info, "targetGrowth", value.information.targetGrowth);
        value.information.targetDistance = settingsValue(info, "targetDistance", value.information.targetDistance);
        value.information.targetCoordinates = settingsValue(info, "targetCoordinates", value.information.targetCoordinates);
        // Test builds of 2026-10-15 saved a number here (0 meant hidden).
        if (auto found = info.find("targetSchematic"); found != info.end())
            value.information.targetSchematic = found->is_number() ? found->get<double>() != 0
                : found->is_boolean() ? found->get<bool>() : value.information.targetSchematic;
        value.information.targetLiquids = settingsValue(info, "targetLiquids", value.information.targetLiquids);
        value.information.durabilityHud = settingsValue(info, "durabilityHud", value.information.durabilityHud);
        value.information.durabilityLook = settingsValue(info, "durabilityLook", value.information.durabilityLook);
        value.information.durabilityOffhand = settingsValue(info, "durabilityOffhand", value.information.durabilityOffhand);
        value.information.durabilityArmor = settingsValue(info, "durabilityArmor", value.information.durabilityArmor);
        value.information.durabilityAmmo = settingsValue(info, "durabilityAmmo", value.information.durabilityAmmo);
        value.information.durabilityElytra = settingsValue(info, "durabilityElytra", value.information.durabilityElytra);
        value.information.offhandSlot = settingsValue(info, "offhandSlot", value.information.offhandSlot);
        value.information.offhandSlotEmpty = settingsValue(info, "offhandSlotEmpty", value.information.offhandSlotEmpty);
        value.information.playerListPlatform = settingsValue(info, "playerListPlatform", value.information.playerListPlatform);
        value.information.playerListDimension = settingsValue(info, "playerListDimension", value.information.playerListDimension);
        value.information.playerListDistance = settingsValue(info, "playerListDistance", value.information.playerListDistance);
        value.information.playerListMembers = settingsValue(info, "playerListMembers", value.information.playerListMembers);
        value.information.inventoryHud = settingsValue(info, "inventoryHud", value.information.inventoryHud);
        value.information.usedSlots = settingsValue(info, "usedSlots", value.information.usedSlots);
        value.information.inventoryHotbar = settingsValue(info, "inventoryHotbar", value.information.inventoryHotbar);
        value.information.usedSlotsHotbar = settingsValue(info, "usedSlotsHotbar", value.information.usedSlotsHotbar);
        value.information.chatPosition = settingsValue(info, "chatPosition", value.information.chatPosition);
        value.information.effectsHud = settingsValue(info, "effectsHud", value.information.effectsHud);
        value.information.effectsNames = settingsValue(info, "effectsNames", value.information.effectsNames);
        value.information.effectsWarning = settingsValue(info, "effectsWarning", value.information.effectsWarning);
        value.information.effectsHideVanilla = settingsValue(info, "effectsHideVanilla", value.information.effectsHideVanilla);
        value.information.saturation = settingsValue(info, "saturation", value.information.saturation);
        value.information.saturationPreview = settingsValue(info, "saturationPreview", value.information.saturationPreview);
        value.information.hud = settingsValue(info, "hud", value.information.hud);
        value.information.coordinates = settingsValue(info, "coordinates", value.information.coordinates);
        value.information.scaledCoordinates = settingsValue(info, "scaledCoordinates", value.information.scaledCoordinates);
        value.information.dimension = settingsValue(info, "dimension", value.information.dimension);
        value.information.biome = settingsValue(info, "biome", value.information.biome);
        value.information.biomeId = settingsValue(info, "biomeId", value.information.biomeId);
        value.information.biomeIdOnly = settingsValue(info, "biomeIdOnly", value.information.biomeIdOnly);
        value.information.difficulty = settingsValue(info, "difficulty", value.information.difficulty);
        value.information.facing = settingsValue(info, "facing", value.information.facing);
        value.information.yaw = settingsValue(info, "yaw", value.information.yaw);
        value.information.pitch = settingsValue(info, "pitch", value.information.pitch);
        value.information.sprinting = settingsValue(info, "sprinting", value.information.sprinting);
        value.information.fps = settingsValue(info, "fps", value.information.fps);
        value.information.frameTime = settingsValue(info, "frameTime", value.information.frameTime);
        value.information.light = settingsValue(info, "light", value.information.light);
        value.information.ping = settingsValue(info, "ping", value.information.ping);
        value.information.rotation = settingsValue(info, "rotation", value.information.rotation);
        value.information.block = settingsValue(info, "block", value.information.block);
        value.information.targetBlock = settingsValue(info, "targetBlock", value.information.targetBlock);
        value.information.chunk = settingsValue(info, "chunk", value.information.chunk);
        value.information.slimeChunk = settingsValue(info, "slimeChunk", value.information.slimeChunk);
        value.information.speed = settingsValue(info, "speed", value.information.speed);
        value.information.horizontalSpeed = settingsValue(info, "horizontalSpeed", value.information.horizontalSpeed);
        value.information.verticalSpeed = settingsValue(info, "verticalSpeed", value.information.verticalSpeed);
        value.information.time = settingsValue(info, "time", value.information.time);
        value.information.realTime = settingsValue(info, "realTime", value.information.realTime);
        value.information.realTimeDate = settingsValue(info, "realTimeDate", value.information.realTimeDate);
        value.information.weather = settingsValue(info, "weather", value.information.weather);
        value.information.moon = settingsValue(info, "moon", value.information.moon);
        if (info.contains("lineOrder") && info.at("lineOrder").is_array()) {
            value.information.lineOrder.clear();
            for (auto const& item : info.at("lineOrder"))
                if (item.is_string()) value.information.lineOrder.push_back(item.get<std::string>());
        }
    }
    if (data.contains("visuals") && data.at("visuals").is_object()) {
        auto const& visuals = data.at("visuals");
        value.visuals.hideOffhand = settingsValue(visuals, "hideOffhand", value.visuals.hideOffhand);
        value.visuals.hideEffects = settingsValue(visuals, "hideEffects", value.visuals.hideEffects);
        value.visuals.connectedTextures = settingsValue(visuals, "connectedTextures", value.visuals.connectedTextures);
        value.visuals.connectedCorners = settingsValue(visuals, "connectedCorners", value.visuals.connectedCorners);
        value.visuals.connectedPaneCorners = settingsValue(visuals, "connectedPaneCorners", value.visuals.connectedPaneCorners);
        value.visuals.skipServerPacks = settingsValue(visuals, "skipServerPacks", value.visuals.skipServerPacks);
        value.visuals.hideWeather = settingsValue(visuals, "hideWeather", value.visuals.hideWeather);
        value.visuals.hideParticles = settingsValue(visuals, "hideParticles", value.visuals.hideParticles);
        value.visuals.hideBossBars = settingsValue(visuals, "hideBossBars", value.visuals.hideBossBars);
        value.visuals.hideNausea = settingsValue(visuals, "hideNausea", value.visuals.hideNausea);
        value.visuals.hideWater = settingsValue(visuals, "hideWater", value.visuals.hideWater);
        value.visuals.hideLava = settingsValue(visuals, "hideLava", value.visuals.hideLava);
        value.visuals.hidePowderSnow = settingsValue(visuals, "hidePowderSnow", value.visuals.hidePowderSnow);
        value.visuals.hideDistanceFog = settingsValue(visuals, "hideDistanceFog", value.visuals.hideDistanceFog);
        value.visuals.hidePortal = settingsValue(visuals, "hidePortal", value.visuals.hidePortal);
    }
    if (data.contains("overlays") && data.at("overlays").is_object()) {
        auto const& overlays = data.at("overlays");
        value.overlays.chunkBorders = settingsValue(overlays, "chunkBorders", value.overlays.chunkBorders);
        value.overlays.hitboxes = settingsValue(overlays, "hitboxes", value.overlays.hitboxes);
        value.overlays.shapes = settingsValue(overlays, "shapes", value.overlays.shapes);
        value.overlays.light = settingsValue(overlays, "light", value.overlays.light);
        // Older files had a sky-light switch instead of the value choice.
        auto lightValue = settingsValue(overlays, "lightValue", std::string(settingsValue(overlays, "skyLight", false) ? "sky" : "block"));
        for (size_t i = 0; i < overlay::lightValueNames.size(); ++i)
            if (overlay::lightValueNames[i] == lightValue) value.overlays.lightValue = static_cast<overlay::LightValue>(i);
        value.overlays.lightRange = settingsValue(overlays, "lightRange", value.overlays.lightRange);
        auto lightFacing = settingsValue(overlays, "lightFacing", std::string("view"));
        for (size_t i = 0; i < overlay::lightFacingNames.size(); ++i)
            if (overlay::lightFacingNames[i] == lightFacing) value.overlays.lightFacing = static_cast<overlay::LightFacing>(i);
        value.overlays.hitboxDistance = settingsValue(overlays, "hitboxDistance", value.overlays.hitboxDistance);
    }
    if (data.contains("bindings") && data.at("bindings").is_object()) {
        auto const& bindings = data.at("bindings");
        // Older files stored chords sorted by code; press order was lost.
        bool const ordered = settingsValue(data, "orderedBindings", false);
        for (size_t i = 0; i < input::actions.size(); ++i) {
            auto found = bindings.find(std::string(input::actions[i].id));
            if (found == bindings.end() || found->is_null()) continue;
            if (!found->is_array() || found->size() > 8) continue;
            input::Chord chord;
            bool valid = true;
            int wheels = 0;
            for (auto const& item : *found) {
                if (!item.is_object()) { valid = false; break; }
                auto device = settingsValue(item, "device", std::string{});
                int code = settingsValue(item, "code", 0);
                input::Device kind;
                if (device == "key" && code >= 1 && code <= 255) kind = input::Device::Key;
                else if (device == "mouse" && code >= 1 && code <= 5) kind = input::Device::Mouse;
                else if (device == "wheel" && (code == -1 || code == 1)) { kind = input::Device::Wheel; ++wheels; }
                else { valid = false; break; }
                chord.push_back({kind, code});
            }
            if (!valid || wheels > 1 || (wheels && input::actions[i].behavior == input::Behavior::Hold)) continue;
            if (!ordered) chord = input::legacyChordOrder(std::move(chord));
            auto stored = input::canonicalChord(std::move(chord), input::actions[i].behavior);
            // An empty settings binding would lock the screen shut; treat it
            // as absent so the default key applies.
            if (stored.empty() && i == static_cast<size_t>(input::Action::Settings)) continue;
            value.bindings[i] = std::move(stored);
        }
    }
    if (data.contains("camera") && data.at("camera").is_object()) {
        auto const& camera = data.at("camera");
        value.camera.freelookToggle = settingsValue(camera, "freelookToggle", value.camera.freelookToggle);
        value.camera.freelookStartPerspective = settingsValue(camera, "freelookStartPerspective", value.camera.freelookStartPerspective);
        value.camera.freeCameraToggle = settingsValue(camera, "freeCameraToggle", value.camera.freeCameraToggle);
        value.camera.freeCameraWorldFixed = settingsValue(camera, "freeCameraWorldFixed", value.camera.freeCameraWorldFixed);
        value.camera.freeCameraLeaveOnHit = settingsValue(camera, "freeCameraLeaveOnHit", value.camera.freeCameraLeaveOnHit);
        value.camera.freeCameraHide = settingsValue(camera, "freeCameraHide", value.camera.freeCameraHide);
        value.camera.freeCameraUndergroundHold = settingsValue(camera, "freeCameraUndergroundHold", value.camera.freeCameraUndergroundHold);
        value.camera.freeCameraSpeed = settingsValue(camera, "freeCameraSpeed", value.camera.freeCameraSpeed);
        value.camera.zoomToggle = settingsValue(camera, "zoomToggle", value.camera.zoomToggle);
        value.camera.magnification = settingsValue(camera, "magnification", value.camera.magnification);
        value.camera.zoomWideAngle = settingsValue(camera, "zoomWideAngle", value.camera.zoomWideAngle);
        value.camera.showMagnification = settingsValue(camera, "showMagnification", value.camera.showMagnification);
    }
    if (data.contains("map") && data.at("map").is_object()) {
        auto const& map = data.at("map");
        value.map.minimap = settingsValue(map, "minimap", value.map.minimap);
        // "zoom" was an index into 32/64/128/256/512 before the finer steps.
        if (map.contains("range")) value.map.zoom = map::zoomIndexFor(settingsValue(map, "range", 128));
        else if (map.contains("zoom"))
            value.map.zoom = map::zoomIndexFor(32 << std::clamp(settingsValue(map, "zoom", 2), 0, 4));
        value.map.size = settingsValue(map, "size", value.map.size);
        value.map.rotate = settingsValue(map, "rotate", value.map.rotate);
        value.map.round = settingsValue(map, "round", value.map.round);
        value.map.caveView = std::clamp(settingsValue(map, "caveView", value.map.caveView), 0, 2);
        value.map.coordinates = settingsValue(map, "coordinates", value.map.coordinates);
        value.map.biome = settingsValue(map, "biome", value.map.biome);
        value.map.compass = settingsValue(map, "compass", value.map.compass);
        value.map.debugHide = settingsValue(map, "debugHide", value.map.debugHide);
        value.map.minimapShapes = settingsValue(map, "minimapShapes", value.map.minimapShapes);
        value.map.radar = settingsValue(map, "radar", value.map.radar);
        value.map.radarPlayers = settingsValue(map, "radarPlayers", value.map.radarPlayers);
        value.map.radarHostile = settingsValue(map, "radarHostile", value.map.radarHostile);
        value.map.radarPassive = settingsValue(map, "radarPassive", value.map.radarPassive);
        value.map.radarItems = settingsValue(map, "radarItems", value.map.radarItems);
        value.map.radarInvisible = settingsValue(map, "radarInvisible", value.map.radarInvisible);
        value.map.waypoints = settingsValue(map, "waypoints", value.map.waypoints);
        // A switch in the first 5b build: on is "always", off is "off".
        if (auto world = map.find("waypointsWorld"); world != map.end()) {
            if (world->is_boolean()) value.map.waypointsWorld = world->get<bool>() ? 0 : 2;
            else value.map.waypointsWorld = settingsValue(map, "waypointsWorld", value.map.waypointsWorld);
        }
        value.map.waypointDistance = settingsValue(map, "waypointDistance", value.map.waypointDistance);
        value.map.waypointsMinimap = settingsValue(map, "waypointsMinimap", value.map.waypointsMinimap);
        value.map.waypointsDeath = settingsValue(map, "waypointsDeath", value.map.waypointsDeath);
        value.map.waypointsCrossScale = settingsValue(map, "waypointsCrossScale", value.map.waypointsCrossScale);
        value.map.waypointArrival = settingsValue(map, "waypointArrival", value.map.waypointArrival);
        value.map.worldMap = settingsValue(map, "worldMap", value.map.worldMap);
        value.map.worldMapNetherAuto = settingsValue(map, "worldMapNetherAuto", value.map.worldMapNetherAuto);
        value.map.worldMapPanel = settingsValue(map, "worldMapPanel", value.map.worldMapPanel);
        value.map.seedLink = settingsValue(map, "seedLink", value.map.seedLink);
        value.map.worldMapNames = settingsValue(map, "worldMapNames", value.map.worldMapNames);
        value.map.radarFaces = settingsValue(map, "radarFaces", value.map.radarFaces);
        value.map.worldMapWaypoints = settingsValue(map, "worldMapWaypoints", value.map.worldMapWaypoints);
        value.map.worldMapSchematics = settingsValue(map, "worldMapSchematics", value.map.worldMapSchematics);
        value.map.worldMapShapes = settingsValue(map, "worldMapShapes", value.map.worldMapShapes);
        value.map.worldMapSlime = settingsValue(map, "worldMapSlime", value.map.worldMapSlime);
        value.map.worldMapList = std::clamp(settingsValue(map, "worldMapList", value.map.worldMapList), 0, 2);
    }
    if (data.contains("schematic") && data.at("schematic").is_object()) {
        auto const& schematic = data.at("schematic");
        value.schematic.enabled = settingsValue(schematic, "enabled", value.schematic.enabled);
        value.schematic.hud = settingsValue(schematic, "hud", value.schematic.hud);
        value.schematic.hudVerify = settingsValue(schematic, "hudVerify", value.schematic.hudVerify);
        value.schematic.hudMaterials = settingsValue(schematic, "hudMaterials", value.schematic.hudMaterials);
        value.schematic.menuBackground = settingsValue(schematic, "menuBackground", value.schematic.menuBackground);
        // Builds of 2026-10-15 saved a choice: 0 strong, 1 weak, 2 none.
        if (int choice = settingsValue(schematic, "ghostTint", -1); choice >= 0 && choice <= 2)
            value.schematic.ghostTint = std::array{100, 40, 0}[static_cast<size_t>(choice)];
        value.schematic.ghostTint = settingsValue(schematic, "ghostTintPercent", value.schematic.ghostTint);
        value.schematic.ghostLines = settingsValue(schematic, "ghostLines", value.schematic.ghostLines);
        value.schematic.menuSmall = settingsValue(schematic, "menuSmall", value.schematic.menuSmall);
        value.schematic.repeatAllSteps = settingsValue(schematic, "repeatAllSteps", value.schematic.repeatAllSteps);
        value.schematic.menuReopen = settingsValue(schematic, "menuReopen", value.schematic.menuReopen);
        value.schematic.outlineDistance = settingsValue(schematic, "outlineDistance", value.schematic.outlineDistance);
        value.schematic.mistakeMarks = settingsValue(schematic, "mistakeMarks", value.schematic.mistakeMarks);
        value.schematic.ignoreSignal = settingsValue(schematic, "ignoreSignal", value.schematic.ignoreSignal);
        value.schematic.litAsState = settingsValue(schematic, "litAsState", value.schematic.litAsState);
        value.schematic.ignoreGrowth = settingsValue(schematic, "ignoreGrowth", value.schematic.ignoreGrowth);
    }
    if (data.contains("lighting") && data.at("lighting").is_object()) {
        value.lighting.nightVision = settingsValue(data.at("lighting"), "nightVision", value.lighting.nightVision);
        value.lighting.nightVisionEven = settingsValue(data.at("lighting"), "nightVisionEven", value.lighting.nightVisionEven);
    }
    if (data.contains("inspection") && data.at("inspection").is_object()) {
        value.inspection.containerPreviews = settingsValue(data.at("inspection"), "containerPreviews", value.inspection.containerPreviews);
        value.inspection.durability = settingsValue(data.at("inspection"), "durability", value.inspection.durability);
        value.inspection.foodValues = settingsValue(data.at("inspection"), "foodValues", value.inspection.foodValues);
        value.inspection.lockedTrades = settingsValue(data.at("inspection"), "lockedTrades", value.inspection.lockedTrades);
        value.inspection.englishSearch = settingsValue(data.at("inspection"), "englishSearch", value.inspection.englishSearch);
        value.inspection.shulkerPreviews = settingsValue(data.at("inspection"), "shulkerPreviews", value.inspection.shulkerPreviews);
        value.inspection.emptyShulkerPreviews = settingsValue(data.at("inspection"), "emptyShulkerPreviews", value.inspection.emptyShulkerPreviews);
        value.inspection.hideShulkerContents = settingsValue(data.at("inspection"), "hideShulkerContents", value.inspection.hideShulkerContents);
        value.inspection.bundlePreviews = settingsValue(data.at("inspection"), "bundlePreviews", value.inspection.bundlePreviews);
        value.inspection.emptyBundlePreviews = settingsValue(data.at("inspection"), "emptyBundlePreviews", value.inspection.emptyBundlePreviews);
    }
    if (data.contains("inventory") && data.at("inventory").is_object()) {
        value.inventory.sorting = settingsValue(data.at("inventory"), "sorting", value.inventory.sorting);
        value.inventory.repeatTrades = settingsValue(data.at("inventory"), "repeatTrades", value.inventory.repeatTrades);
        value.inventory.offhandSwap = settingsValue(data.at("inventory"), "offhandSwap", value.inventory.offhandSwap);
        value.inventory.deathRestore = settingsValue(data.at("inventory"), "deathRestore", value.inventory.deathRestore);
        value.inventory.deathRestoreAll = settingsValue(data.at("inventory"), "deathRestoreAll", value.inventory.deathRestoreAll);
        value.inventory.offhandSwapFireworks = settingsValue(data.at("inventory"), "offhandSwapFireworks", value.inventory.offhandSwapFireworks);
        value.inventory.sortContainers = settingsValue(data.at("inventory"), "sortContainers", value.inventory.sortContainers);
        value.inventory.transfer = settingsValue(data.at("inventory"), "transfer", value.inventory.transfer);
        value.inventory.transferWheelOne = settingsValue(data.at("inventory"), "transferWheelOne", value.inventory.transferWheelOne);
        value.inventory.transferWheelStack = settingsValue(data.at("inventory"), "transferWheelStack", value.inventory.transferWheelStack);
        value.inventory.transferDragStack = settingsValue(data.at("inventory"), "transferDragStack", value.inventory.transferDragStack);
        value.inventory.transferDragOne = settingsValue(data.at("inventory"), "transferDragOne", value.inventory.transferDragOne);
        value.inventory.transferWheelMatching = settingsValue(data.at("inventory"), "transferWheelMatching", value.inventory.transferWheelMatching);
        value.inventory.transferWheelAll = settingsValue(data.at("inventory"), "transferWheelAll", value.inventory.transferWheelAll);
        value.inventory.transferClickMatching = settingsValue(data.at("inventory"), "transferClickMatching", value.inventory.transferClickMatching);
        value.inventory.transferClickAll = settingsValue(data.at("inventory"), "transferClickAll", value.inventory.transferClickAll);
        value.inventory.transferDragKeepOne = settingsValue(data.at("inventory"), "transferDragKeepOne", value.inventory.transferDragKeepOne);
        value.inventory.transferCursorMatching = settingsValue(data.at("inventory"), "transferCursorMatching", value.inventory.transferCursorMatching);
        value.inventory.transferDropOne = settingsValue(data.at("inventory"), "transferDropOne", value.inventory.transferDropOne);
        value.inventory.transferDropKeepOne = settingsValue(data.at("inventory"), "transferDropKeepOne", value.inventory.transferDropKeepOne);
        value.inventory.transferDropAll = settingsValue(data.at("inventory"), "transferDropAll", value.inventory.transferDropAll);
        value.inventory.transferDropMatching = settingsValue(data.at("inventory"), "transferDropMatching", value.inventory.transferDropMatching);
        value.inventory.toolSwitch = settingsValue(data.at("inventory"), "toolSwitch", value.inventory.toolSwitch);
        value.inventory.handRestock = settingsValue(data.at("inventory"), "handRestock", value.inventory.handRestock);
        value.inventory.restockFromHotbar = settingsValue(data.at("inventory"), "restockFromHotbar", value.inventory.restockFromHotbar);
        value.inventory.restockOffhand = settingsValue(data.at("inventory"), "restockOffhand", value.inventory.restockOffhand);
        value.inventory.restockShulkerBoxes = settingsValue(data.at("inventory"), "restockShulkerBoxes", value.inventory.restockShulkerBoxes);
        value.inventory.restockBuckets = settingsValue(data.at("inventory"), "restockBuckets", value.inventory.restockBuckets);
        value.inventory.restockThreshold = settingsValue(data.at("inventory"), "restockThreshold", value.inventory.restockThreshold);
        value.inventory.restockOrder = settingsValue(data.at("inventory"), "restockOrder", value.inventory.restockOrder);
        value.inventory.toolSwitchInventory = settingsValue(data.at("inventory"), "toolSwitchInventory", value.inventory.toolSwitchInventory);
        value.inventory.weaponSwitch = settingsValue(data.at("inventory"), "weaponSwitch", value.inventory.weaponSwitch);
        value.inventory.weaponSwitchInventory = settingsValue(data.at("inventory"), "weaponSwitchInventory", value.inventory.weaponSwitchInventory);
        value.inventory.toolSwitchSlot = settingsValue(data.at("inventory"), "toolSwitchSlot", value.inventory.toolSwitchSlot);
        value.inventory.weaponSwitchSlot = settingsValue(data.at("inventory"), "weaponSwitchSlot", value.inventory.weaponSwitchSlot);
        value.inventory.fakeOffhand = settingsValue(data.at("inventory"), "fakeOffhand", value.inventory.fakeOffhand);
        value.inventory.fakeOffhandSlot = settingsValue(data.at("inventory"), "fakeOffhandSlot", value.inventory.fakeOffhandSlot);
    }
    if (data.contains("interface") && data.at("interface").is_object()) {
        value.ui.toggleToasts = settingsValue(data.at("interface"), "toggleToasts", value.ui.toggleToasts);
        value.ui.animations = settingsValue(data.at("interface"), "animations", value.ui.animations);
        value.ui.automationStatus = settingsValue(data.at("interface"), "automationStatus", value.ui.automationStatus);
        value.ui.bookPaste = settingsValue(data.at("interface"), "bookPaste", value.ui.bookPaste);
        value.ui.hudRowHeight = settingsValue(data.at("interface"), "hudRowHeight", value.ui.hudRowHeight);
        value.ui.hudBackgroundOpacity = settingsValue(data.at("interface"), "hudBackgroundOpacity", value.ui.hudBackgroundOpacity);
    }
    if (data.contains("hud") && data.at("hud").is_object()) {
        auto const& hud = data.at("hud");
        auto element = [&](char const* key, ui::HudElement& target, ui::HudElementId id) {
            auto found = hud.find(key);
            if (found != hud.end() && found->is_object())
                decodeHudElement(*found, target, ui::defaultHudElement(id));
        };
        element("info", value.hud.info, ui::HudElementId::Info);
        element("target", value.hud.target, ui::HudElementId::Target);
        element("status", value.hud.status, ui::HudElementId::Status);
        element("toast", value.hud.toast, ui::HudElementId::Toast);
        element("magnification", value.hud.magnification, ui::HudElementId::Magnification);
        element("durability", value.hud.durability, ui::HudElementId::Durability);
        element("minimap", value.hud.minimap, ui::HudElementId::Minimap);
        element("schematic", value.hud.schematic, ui::HudElementId::Schematic);
        element("playerList", value.hud.playerList, ui::HudElementId::PlayerList);
        element("inventory", value.hud.inventory, ui::HudElementId::Inventory);
        element("usedSlots", value.hud.usedSlots, ui::HudElementId::UsedSlots);
        element("chat", value.hud.chat, ui::HudElementId::Chat);
        element("statusEffects", value.hud.statusEffects, ui::HudElementId::StatusEffects);
    }
    value.normalize();
    return value;
}
Settings readSettings(std::filesystem::path const& path, bool* recovered) {
    if (recovered) *recovered = false;
    auto text = read(path);
    // Anything decodeSettings rejects (bad JSON, a future version, an unknown
    // stored name) is kept aside: saving defaults over it would lose it.
    try { return decodeSettings(text); }
    catch (std::exception const&) {
        preserveBroken(path);
        if (recovered) *recovered = true;
        writeSettings(path, Settings{});
        return {};
    }
}
void writeSettings(std::filesystem::path const& path, Settings const& settings) {
    auto normalized = settings;
    normalized.normalize();
    auto data = std::filesystem::exists(path) ? parse(read(path)) : Json::object();
    // Retain unknown keys when adding settings in a later development build.
    data.merge_patch(encode(normalized));
    auto temporary = path;
    temporary += ".tmp";
    if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path());
    try {
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        stream.exceptions(std::ios::badbit | std::ios::failbit);
        stream << data.dump(4) << '\n';
        stream.flush();
        stream.close();
#ifdef _WIN32
        if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "Replacing settings");
#else
        std::filesystem::rename(temporary, path);
#endif
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        throw;
    }
}
}
