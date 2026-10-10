#include "settings/SettingsStore.h"
#include "nlohmann/json.hpp"
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
Json encodeHudElement(ui::HudElement const& element) {
    return {{"anchor", static_cast<int>(element.anchor)}, {"dx", element.dx},
            {"dy", element.dy}, {"scale", element.scale},
            {"background", static_cast<int>(element.background)}, {"shadow", element.shadow}};
}
void decodeHudElement(Json const& data, ui::HudElement& element, ui::HudElement defaultValue) {
    element.anchor = static_cast<ui::Anchor>(data.value("anchor", static_cast<int>(defaultValue.anchor)));
    element.dx = data.value("dx", defaultValue.dx);
    element.dy = data.value("dy", defaultValue.dy);
    element.scale = data.value("scale", defaultValue.scale);
    element.background = static_cast<ui::ElementBackground>(data.value("background", static_cast<int>(defaultValue.background)));
    element.shadow = data.value("shadow", defaultValue.shadow);
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
                         {"toolGuard", settings.interaction.toolGuard}, {"elytraSwap", settings.interaction.elytraSwap},
                         {"toolGuardStrict", settings.interaction.toolGuardStrict},
                         {"elytraReturnSeconds", settings.interaction.elytraReturnSeconds},
                         {"elytraFireworkJump", settings.interaction.elytraFireworkJump},
                         {"breakingMode", interaction::restrictionNames[static_cast<size_t>(settings.interaction.breakingMode)]},
                         {"breakingBand", settings.interaction.breakingBand},
                         {"placementMode", interaction::restrictionNames[static_cast<size_t>(settings.interaction.placementMode)]}}},
        {"information", {{"hud", settings.information.hud}, {"coordinates", settings.information.coordinates},
                         {"scaledCoordinates", settings.information.scaledCoordinates},
                         {"debug", settings.information.debug}, {"debugLabels", settings.information.debugLabels},
                         {"debugHideHud", settings.information.debugHideHud},
                         {"debugHideTarget", settings.information.debugHideTarget},
                         {"debugShadow", settings.information.debugShadow},
                         {"debugBackground", settings.information.debugBackground},
                         {"target", settings.information.target}, {"targetIdentifier", settings.information.targetIdentifier},
                         {"targetIcon", settings.information.targetIcon},
                         {"targetHealth", settings.information.targetHealth},
                         {"targetArmor", settings.information.targetArmor},
                         {"targetGrowth", settings.information.targetGrowth},
                         {"targetDistance", settings.information.targetDistance},
                         {"targetStates", settings.information.targetStates},
                         {"targetCoordinates", settings.information.targetCoordinates},
                         {"durabilityHud", settings.information.durabilityHud},
                         {"durabilityLook", settings.information.durabilityLook},
                         {"durabilityOffhand", settings.information.durabilityOffhand},
                         {"durabilityArmor", settings.information.durabilityArmor},
                         {"offhandSlot", settings.information.offhandSlot},
                         {"offhandSlotEmpty", settings.information.offhandSlotEmpty},
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
                         {"chunk", settings.information.chunk}, {"speed", settings.information.speed},
                         {"horizontalSpeed", settings.information.horizontalSpeed},
                         {"verticalSpeed", settings.information.verticalSpeed},
                         {"time", settings.information.time}, {"realTime", settings.information.realTime},
                         {"realTimeDate", settings.information.realTimeDate},
                         {"weather", settings.information.weather},
                         {"moon", settings.information.moon},
                         {"dimension", settings.information.dimension}}},
        {"visuals", {{"hideOffhand", settings.visuals.hideOffhand},
                     {"hideEffects", settings.visuals.hideEffects},
                     {"hideWeather", settings.visuals.hideWeather}, {"hideParticles", settings.visuals.hideParticles},
                     {"hideBossBars", settings.visuals.hideBossBars}, {"hideNausea", settings.visuals.hideNausea},
                     {"hideWater", settings.visuals.hideWater}, {"hideLava", settings.visuals.hideLava},
                     {"hidePowderSnow", settings.visuals.hidePowderSnow},
                     {"hideDistanceFog", settings.visuals.hideDistanceFog}}},
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
                    {"magnification", settings.camera.magnification},
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
                        {"lockedTrades", settings.inspection.lockedTrades}}},
        {"inventory", {{"sorting", settings.inventory.sorting}, {"sortContainers", settings.inventory.sortContainers},
                       {"offhandSwap", settings.inventory.offhandSwap},
                       {"deathRestore", settings.inventory.deathRestore},
                       {"deathRestoreAll", settings.inventory.deathRestoreAll},
                       {"offhandSwapFireworks", settings.inventory.offhandSwapFireworks},
                       {"transfer", settings.inventory.transfer},
                       {"transferWheelOne", settings.inventory.transferWheelOne},
                       {"transferWheelStack", settings.inventory.transferWheelStack},
                       {"transferDragStack", settings.inventory.transferDragStack},
                       {"transferDragOne", settings.inventory.transferDragOne},
                       {"toolSwitch", settings.inventory.toolSwitch}, {"handRestock", settings.inventory.handRestock},
                       {"restockFromHotbar", settings.inventory.restockFromHotbar},
                       {"restockOffhand", settings.inventory.restockOffhand},
                       {"restockThreshold", settings.inventory.restockThreshold},
                       {"restockOrder", settings.inventory.restockOrder},
                       {"toolSwitchInventory", settings.inventory.toolSwitchInventory},
                       {"weaponSwitch", settings.inventory.weaponSwitch},
                       {"weaponSwitchInventory", settings.inventory.weaponSwitchInventory},
                       {"toolSwitchSlot", settings.inventory.toolSwitchSlot},
                       {"weaponSwitchSlot", settings.inventory.weaponSwitchSlot},
                       {"fakeOffhand", settings.inventory.fakeOffhand}, {"fakeOffhandSlot", settings.inventory.fakeOffhandSlot}}},
        {"interface", {{"toggleToasts", settings.ui.toggleToasts}, {"automationStatus", settings.ui.automationStatus},
                       {"animations", settings.ui.animations}, {"hudRowHeight", settings.ui.hudRowHeight},
                       {"hudBackgroundOpacity", settings.ui.hudBackgroundOpacity}}},
        {"hud", {{"info", encodeHudElement(settings.hud.info)}, {"target", encodeHudElement(settings.hud.target)},
                   {"status", encodeHudElement(settings.hud.status)}, {"toast", encodeHudElement(settings.hud.toast)},
                   {"magnification", encodeHudElement(settings.hud.magnification)},
                   {"durability", encodeHudElement(settings.hud.durability)},
                   {"minimap", encodeHudElement(settings.hud.minimap)},
                   {"schematic", encodeHudElement(settings.hud.schematic)}}},
        {"map", {{"minimap", settings.map.minimap}, {"range", map::blocksAcross(settings.map.zoom)}, {"size", settings.map.size},
                 {"rotate", settings.map.rotate},
                 {"round", settings.map.round}, {"coordinates", settings.map.coordinates},
                 {"biome", settings.map.biome}, {"compass", settings.map.compass},
                 {"debugHide", settings.map.debugHide}, {"radar", settings.map.radar},
                 {"radarPlayers", settings.map.radarPlayers}, {"radarHostile", settings.map.radarHostile},
                 {"radarPassive", settings.map.radarPassive}, {"radarItems", settings.map.radarItems},
                 {"radarInvisible", settings.map.radarInvisible}, {"waypoints", settings.map.waypoints},
                 {"waypointsWorld", settings.map.waypointsWorld}, {"waypointDistance", settings.map.waypointDistance},
                 {"waypointsMinimap", settings.map.waypointsMinimap}, {"waypointsDeath", settings.map.waypointsDeath},
                 {"waypointsCrossScale", settings.map.waypointsCrossScale}, {"worldMap", settings.map.worldMap},
                 {"worldMapNetherAuto", settings.map.worldMapNetherAuto}, {"worldMapPanel", settings.map.worldMapPanel},
                 {"seedLink", settings.map.seedLink},
                 {"radarFaces", settings.map.radarFaces}}},
        {"schematic", {{"enabled", settings.schematic.enabled}, {"hud", settings.schematic.hud},
                       {"hudVerify", settings.schematic.hudVerify}, {"hudMaterials", settings.schematic.hudMaterials},
                       {"menuBackground", settings.schematic.menuBackground}, {"menuSmall", settings.schematic.menuSmall},
                       {"menuReopen", settings.schematic.menuReopen}, {"outlineDistance", settings.schematic.outlineDistance}}}
    };
}
}
Settings decodeSettings(std::string_view text) {
    auto data = parse(text);
    Settings value;
    if (data.contains("interaction")) {
        auto const& options = data.at("interaction");
        // Older files stored the periodic interval in seconds.
        auto ticks = [&](char const* key, char const* seconds) {
            if (options.contains(key)) return options.value(key, 10.f);
            return options.value(seconds, .5f) * 20;
        };
        value.interaction.attackTicks = ticks("attackTicks", "attackInterval");
        value.interaction.useTicks = ticks("useTicks", "useInterval");
        value.interaction.attackClicks = options.value("attackClicks", value.interaction.attackClicks);
        value.interaction.useClicks = options.value("useClicks", value.interaction.useClicks);
        // An unknown mode name falls back to Periodic rather than failing the load.
        auto autoMode = [&](char const* key) {
            auto name = options.value(key, std::string("periodic"));
            for (size_t i = 0; i < interaction::autoModeNames.size(); ++i)
                if (interaction::autoModeNames[i] == name) return static_cast<interaction::AutoMode>(i);
            return interaction::AutoMode::Periodic;
        };
        value.interaction.attackMode = autoMode("attackMode");
        value.interaction.useMode = autoMode("useMode");
        // A short-lived build saved this as "attackTrigger": "held".
        auto heldOnly = [&](char const* key, char const* trigger) {
            if (options.contains(key)) return options.value(key, false);
            return options.value(trigger, std::string("always")) == "held";
        };
        value.interaction.attackHeldOnly = heldOnly("attackHeldOnly", "attackTrigger");
        value.interaction.useHeldOnly = heldOnly("useHeldOnly", "useTrigger");
        value.interaction.breaking = options.value("breaking", value.interaction.breaking);
        value.interaction.edgeGuard = options.value("edgeGuard", value.interaction.edgeGuard);
        value.interaction.toolGuard = options.value("toolGuard", value.interaction.toolGuard);
        value.interaction.toolGuardStrict = options.value("toolGuardStrict", value.interaction.toolGuardStrict);
        value.interaction.elytraSwap = options.value("elytraSwap", value.interaction.elytraSwap);
        value.interaction.elytraReturnSeconds = options.value("elytraReturnSeconds", value.interaction.elytraReturnSeconds);
        value.interaction.elytraFireworkJump = options.value("elytraFireworkJump", value.interaction.elytraFireworkJump);
        auto mode = [&](char const* key) {
            auto name = options.value(key,std::string("plane"));
            for (size_t i=0;i<interaction::restrictionNames.size();++i)
                if (name == interaction::restrictionNames[i]) return static_cast<interaction::RestrictionMode>(i);
            throw std::runtime_error("Unknown restriction mode");
        };
        value.interaction.breakingMode = mode("breakingMode");
        value.interaction.placementMode = mode("placementMode");
        value.interaction.breakingBand = options.value("breakingBand", value.interaction.breakingBand);
    }
    if (data.contains("information")) {
        auto const& info = data.at("information");
        value.information.debug = info.value("debug", value.information.debug);
        value.information.debugLabels = info.value("debugLabels", value.information.debugLabels);
        value.information.debugHideHud = info.value("debugHideHud", value.information.debugHideHud);
        value.information.debugHideTarget = info.value("debugHideTarget", value.information.debugHideTarget);
        value.information.debugShadow = info.value("debugShadow", value.information.debugShadow);
        value.information.debugBackground = info.value("debugBackground", value.information.debugBackground);
        value.information.target = info.value("target", value.information.target);
        value.information.targetIdentifier = info.value("targetIdentifier", value.information.targetIdentifier);
        value.information.targetStates = info.value("targetStates", value.information.targetStates);
        value.information.targetIcon = info.value("targetIcon", value.information.targetIcon);
        value.information.targetHealth = info.value("targetHealth", value.information.targetHealth);
        value.information.targetArmor = info.value("targetArmor", value.information.targetArmor);
        value.information.targetGrowth = info.value("targetGrowth", value.information.targetGrowth);
        value.information.targetDistance = info.value("targetDistance", value.information.targetDistance);
        value.information.targetCoordinates = info.value("targetCoordinates", value.information.targetCoordinates);
        value.information.durabilityHud = info.value("durabilityHud", value.information.durabilityHud);
        value.information.durabilityLook = info.value("durabilityLook", value.information.durabilityLook);
        value.information.durabilityOffhand = info.value("durabilityOffhand", value.information.durabilityOffhand);
        value.information.durabilityArmor = info.value("durabilityArmor", value.information.durabilityArmor);
        value.information.offhandSlot = info.value("offhandSlot", value.information.offhandSlot);
        value.information.offhandSlotEmpty = info.value("offhandSlotEmpty", value.information.offhandSlotEmpty);
        value.information.saturation = info.value("saturation", value.information.saturation);
        value.information.saturationPreview = info.value("saturationPreview", value.information.saturationPreview);
        value.information.hud = info.value("hud", value.information.hud);
        value.information.coordinates = info.value("coordinates", value.information.coordinates);
        value.information.scaledCoordinates = info.value("scaledCoordinates", value.information.scaledCoordinates);
        value.information.dimension = info.value("dimension", value.information.dimension);
        value.information.biome = info.value("biome", value.information.biome);
        value.information.biomeId = info.value("biomeId", value.information.biomeId);
        value.information.biomeIdOnly = info.value("biomeIdOnly", value.information.biomeIdOnly);
        value.information.difficulty = info.value("difficulty", value.information.difficulty);
        value.information.facing = info.value("facing", value.information.facing);
        value.information.yaw = info.value("yaw", value.information.yaw);
        value.information.pitch = info.value("pitch", value.information.pitch);
        value.information.sprinting = info.value("sprinting", value.information.sprinting);
        value.information.fps = info.value("fps", value.information.fps);
        value.information.frameTime = info.value("frameTime", value.information.frameTime);
        value.information.light = info.value("light", value.information.light);
        value.information.ping = info.value("ping", value.information.ping);
        value.information.rotation = info.value("rotation", value.information.rotation);
        value.information.block = info.value("block", value.information.block);
        value.information.chunk = info.value("chunk", value.information.chunk);
        value.information.speed = info.value("speed", value.information.speed);
        value.information.horizontalSpeed = info.value("horizontalSpeed", value.information.horizontalSpeed);
        value.information.verticalSpeed = info.value("verticalSpeed", value.information.verticalSpeed);
        value.information.time = info.value("time", value.information.time);
        value.information.realTime = info.value("realTime", value.information.realTime);
        value.information.realTimeDate = info.value("realTimeDate", value.information.realTimeDate);
        value.information.weather = info.value("weather", value.information.weather);
        value.information.moon = info.value("moon", value.information.moon);
        if (info.contains("lineOrder") && info.at("lineOrder").is_array()) {
            value.information.lineOrder.clear();
            for (auto const& item : info.at("lineOrder"))
                if (item.is_string()) value.information.lineOrder.push_back(item.get<std::string>());
        }
    }
    if (data.contains("visuals")) {
        auto const& visuals = data.at("visuals");
        value.visuals.hideOffhand = visuals.value("hideOffhand", value.visuals.hideOffhand);
        value.visuals.hideEffects = visuals.value("hideEffects", value.visuals.hideEffects);
        value.visuals.hideWeather = visuals.value("hideWeather", value.visuals.hideWeather);
        value.visuals.hideParticles = visuals.value("hideParticles", value.visuals.hideParticles);
        value.visuals.hideBossBars = visuals.value("hideBossBars", value.visuals.hideBossBars);
        value.visuals.hideNausea = visuals.value("hideNausea", value.visuals.hideNausea);
        value.visuals.hideWater = visuals.value("hideWater", value.visuals.hideWater);
        value.visuals.hideLava = visuals.value("hideLava", value.visuals.hideLava);
        value.visuals.hidePowderSnow = visuals.value("hidePowderSnow", value.visuals.hidePowderSnow);
        value.visuals.hideDistanceFog = visuals.value("hideDistanceFog", value.visuals.hideDistanceFog);
    }
    if (data.contains("overlays")) {
        auto const& overlays = data.at("overlays");
        value.overlays.chunkBorders = overlays.value("chunkBorders", value.overlays.chunkBorders);
        value.overlays.hitboxes = overlays.value("hitboxes", value.overlays.hitboxes);
        value.overlays.shapes = overlays.value("shapes", value.overlays.shapes);
        value.overlays.light = overlays.value("light", value.overlays.light);
        // Older files had a sky-light switch instead of the value choice.
        auto lightValue = overlays.value("lightValue", std::string(overlays.value("skyLight", false) ? "sky" : "block"));
        for (size_t i = 0; i < overlay::lightValueNames.size(); ++i)
            if (overlay::lightValueNames[i] == lightValue) value.overlays.lightValue = static_cast<overlay::LightValue>(i);
        value.overlays.lightRange = overlays.value("lightRange", value.overlays.lightRange);
        auto lightFacing = overlays.value("lightFacing", std::string("view"));
        for (size_t i = 0; i < overlay::lightFacingNames.size(); ++i)
            if (overlay::lightFacingNames[i] == lightFacing) value.overlays.lightFacing = static_cast<overlay::LightFacing>(i);
        value.overlays.hitboxDistance = overlays.value("hitboxDistance", value.overlays.hitboxDistance);
    }
    if (data.contains("bindings")) {
        auto const& bindings = data.at("bindings");
        if (!bindings.is_object()) throw std::runtime_error("Bindings must be an object");
        // Older files stored chords sorted by code; press order was lost.
        bool const ordered = data.value("orderedBindings", false);
        for (size_t i = 0; i < input::actions.size(); ++i) {
            auto found = bindings.find(std::string(input::actions[i].id));
            if (found == bindings.end() || found->is_null()) continue;
            if (!found->is_array()) throw std::runtime_error("Binding must be an input array");
            input::Chord chord;
            for (auto const& item : *found) {
                auto device = item.at("device").get<std::string>();
                auto const& code = item.at("code");
                if (!code.is_number_integer() || code < -1 || code > 255)
                    throw std::runtime_error("Invalid binding code");
                input::Device kind;
                if (device == "key") kind = input::Device::Key;
                else if (device == "mouse") kind = input::Device::Mouse;
                else if (device == "wheel") kind = input::Device::Wheel;
                else throw std::runtime_error("Unknown binding device");
                chord.push_back({kind, code.get<int>()});
            }
            if (!ordered) chord = input::legacyChordOrder(std::move(chord));
            auto stored = input::canonicalChord(std::move(chord), input::actions[i].behavior);
            // An empty settings binding would lock the screen shut; treat it
            // as absent so the default key applies.
            if (stored.empty() && i == static_cast<size_t>(input::Action::Settings)) continue;
            value.bindings[i] = std::move(stored);
        }
    }
    if (data.contains("camera")) {
        auto const& camera = data.at("camera");
        value.camera.freelookToggle = camera.value("freelookToggle", value.camera.freelookToggle);
        value.camera.freelookStartPerspective = camera.value("freelookStartPerspective", value.camera.freelookStartPerspective);
        value.camera.freeCameraToggle = camera.value("freeCameraToggle", value.camera.freeCameraToggle);
        value.camera.freeCameraWorldFixed = camera.value("freeCameraWorldFixed", value.camera.freeCameraWorldFixed);
        value.camera.freeCameraLeaveOnHit = camera.value("freeCameraLeaveOnHit", value.camera.freeCameraLeaveOnHit);
        value.camera.freeCameraSpeed = camera.value("freeCameraSpeed", value.camera.freeCameraSpeed);
        value.camera.zoomToggle = camera.value("zoomToggle", value.camera.zoomToggle);
        value.camera.magnification = camera.value("magnification", value.camera.magnification);
        value.camera.showMagnification = camera.value("showMagnification", value.camera.showMagnification);
    }
    if (data.contains("map") && data.at("map").is_object()) {
        auto const& map = data.at("map");
        value.map.minimap = map.value("minimap", value.map.minimap);
        // "zoom" was an index into 32/64/128/256/512 before the finer steps.
        if (map.contains("range")) value.map.zoom = map::zoomIndexFor(map.value("range", 128));
        else if (map.contains("zoom"))
            value.map.zoom = map::zoomIndexFor(32 << std::clamp(map.value("zoom", 2), 0, 4));
        value.map.size = map.value("size", value.map.size);
        value.map.rotate = map.value("rotate", value.map.rotate);
        value.map.round = map.value("round", value.map.round);
        value.map.coordinates = map.value("coordinates", value.map.coordinates);
        value.map.biome = map.value("biome", value.map.biome);
        value.map.compass = map.value("compass", value.map.compass);
        value.map.debugHide = map.value("debugHide", value.map.debugHide);
        value.map.radar = map.value("radar", value.map.radar);
        value.map.radarPlayers = map.value("radarPlayers", value.map.radarPlayers);
        value.map.radarHostile = map.value("radarHostile", value.map.radarHostile);
        value.map.radarPassive = map.value("radarPassive", value.map.radarPassive);
        value.map.radarItems = map.value("radarItems", value.map.radarItems);
        value.map.radarInvisible = map.value("radarInvisible", value.map.radarInvisible);
        value.map.waypoints = map.value("waypoints", value.map.waypoints);
        // A switch in the first 5b build: on is "always", off is "off".
        if (auto world = map.find("waypointsWorld"); world != map.end()) {
            if (world->is_boolean()) value.map.waypointsWorld = world->get<bool>() ? 0 : 2;
            else if (world->is_number_integer()) value.map.waypointsWorld = world->get<int>();
        }
        value.map.waypointDistance = map.value("waypointDistance", value.map.waypointDistance);
        value.map.waypointsMinimap = map.value("waypointsMinimap", value.map.waypointsMinimap);
        value.map.waypointsDeath = map.value("waypointsDeath", value.map.waypointsDeath);
        value.map.waypointsCrossScale = map.value("waypointsCrossScale", value.map.waypointsCrossScale);
        value.map.worldMap = map.value("worldMap", value.map.worldMap);
        value.map.worldMapNetherAuto = map.value("worldMapNetherAuto", value.map.worldMapNetherAuto);
        value.map.worldMapPanel = map.value("worldMapPanel", value.map.worldMapPanel);
        value.map.seedLink = map.value("seedLink", value.map.seedLink);
        value.map.radarFaces = map.value("radarFaces", value.map.radarFaces);
    }
    if (data.contains("schematic") && data.at("schematic").is_object()) {
        auto const& schematic = data.at("schematic");
        value.schematic.enabled = schematic.value("enabled", value.schematic.enabled);
        value.schematic.hud = schematic.value("hud", value.schematic.hud);
        value.schematic.hudVerify = schematic.value("hudVerify", value.schematic.hudVerify);
        value.schematic.hudMaterials = schematic.value("hudMaterials", value.schematic.hudMaterials);
        value.schematic.menuBackground = schematic.value("menuBackground", value.schematic.menuBackground);
        value.schematic.menuSmall = schematic.value("menuSmall", value.schematic.menuSmall);
        value.schematic.menuReopen = schematic.value("menuReopen", value.schematic.menuReopen);
        value.schematic.outlineDistance = schematic.value("outlineDistance", value.schematic.outlineDistance);
    }
    if (data.contains("lighting")) {
        value.lighting.nightVision = data.at("lighting").value("nightVision", value.lighting.nightVision);
        value.lighting.nightVisionEven = data.at("lighting").value("nightVisionEven", value.lighting.nightVisionEven);
    }
    if (data.contains("inspection")) {
        value.inspection.containerPreviews = data.at("inspection").value("containerPreviews", value.inspection.containerPreviews);
        value.inspection.durability = data.at("inspection").value("durability", value.inspection.durability);
        value.inspection.foodValues = data.at("inspection").value("foodValues", value.inspection.foodValues);
        value.inspection.lockedTrades = data.at("inspection").value("lockedTrades", value.inspection.lockedTrades);
        value.inspection.shulkerPreviews = data.at("inspection").value("shulkerPreviews", value.inspection.shulkerPreviews);
        value.inspection.emptyShulkerPreviews = data.at("inspection").value("emptyShulkerPreviews", value.inspection.emptyShulkerPreviews);
        value.inspection.hideShulkerContents = data.at("inspection").value("hideShulkerContents", value.inspection.hideShulkerContents);
        value.inspection.bundlePreviews = data.at("inspection").value("bundlePreviews", value.inspection.bundlePreviews);
        value.inspection.emptyBundlePreviews = data.at("inspection").value("emptyBundlePreviews", value.inspection.emptyBundlePreviews);
    }
    if (data.contains("inventory")) {
        value.inventory.sorting = data.at("inventory").value("sorting", value.inventory.sorting);
        value.inventory.offhandSwap = data.at("inventory").value("offhandSwap", value.inventory.offhandSwap);
        value.inventory.deathRestore = data.at("inventory").value("deathRestore", value.inventory.deathRestore);
        value.inventory.deathRestoreAll = data.at("inventory").value("deathRestoreAll", value.inventory.deathRestoreAll);
        value.inventory.offhandSwapFireworks = data.at("inventory").value("offhandSwapFireworks", value.inventory.offhandSwapFireworks);
        value.inventory.sortContainers = data.at("inventory").value("sortContainers", value.inventory.sortContainers);
        value.inventory.transfer = data.at("inventory").value("transfer", value.inventory.transfer);
        value.inventory.transferWheelOne = data.at("inventory").value("transferWheelOne", value.inventory.transferWheelOne);
        value.inventory.transferWheelStack = data.at("inventory").value("transferWheelStack", value.inventory.transferWheelStack);
        value.inventory.transferDragStack = data.at("inventory").value("transferDragStack", value.inventory.transferDragStack);
        value.inventory.transferDragOne = data.at("inventory").value("transferDragOne", value.inventory.transferDragOne);
        value.inventory.toolSwitch = data.at("inventory").value("toolSwitch", value.inventory.toolSwitch);
        value.inventory.handRestock = data.at("inventory").value("handRestock", value.inventory.handRestock);
        value.inventory.restockFromHotbar = data.at("inventory").value("restockFromHotbar", value.inventory.restockFromHotbar);
        value.inventory.restockOffhand = data.at("inventory").value("restockOffhand", value.inventory.restockOffhand);
        value.inventory.restockThreshold = data.at("inventory").value("restockThreshold", value.inventory.restockThreshold);
        value.inventory.restockOrder = data.at("inventory").value("restockOrder", value.inventory.restockOrder);
        value.inventory.toolSwitchInventory = data.at("inventory").value("toolSwitchInventory", value.inventory.toolSwitchInventory);
        value.inventory.weaponSwitch = data.at("inventory").value("weaponSwitch", value.inventory.weaponSwitch);
        value.inventory.weaponSwitchInventory = data.at("inventory").value("weaponSwitchInventory", value.inventory.weaponSwitchInventory);
        value.inventory.toolSwitchSlot = data.at("inventory").value("toolSwitchSlot", value.inventory.toolSwitchSlot);
        value.inventory.weaponSwitchSlot = data.at("inventory").value("weaponSwitchSlot", value.inventory.weaponSwitchSlot);
        value.inventory.fakeOffhand = data.at("inventory").value("fakeOffhand", value.inventory.fakeOffhand);
        value.inventory.fakeOffhandSlot = data.at("inventory").value("fakeOffhandSlot", value.inventory.fakeOffhandSlot);
    }
    if (data.contains("interface")) {
        value.ui.toggleToasts = data.at("interface").value("toggleToasts", value.ui.toggleToasts);
        value.ui.animations = data.at("interface").value("animations", value.ui.animations);
        value.ui.automationStatus = data.at("interface").value("automationStatus", value.ui.automationStatus);
        value.ui.hudRowHeight = data.at("interface").value("hudRowHeight", value.ui.hudRowHeight);
        value.ui.hudBackgroundOpacity = data.at("interface").value("hudBackgroundOpacity", value.ui.hudBackgroundOpacity);
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
    }
    value.normalize();
    return value;
}
Settings readSettings(std::filesystem::path const& path) { return decodeSettings(read(path)); }
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
