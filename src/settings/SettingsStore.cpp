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
        {"interaction", {{"attackInterval", settings.interaction.attackInterval}, {"useInterval", settings.interaction.useInterval},
                         {"breaking", settings.interaction.breaking}, {"breakingMode", interaction::restrictionNames[static_cast<size_t>(settings.interaction.breakingMode)]},
                         {"placementMode", interaction::restrictionNames[static_cast<size_t>(settings.interaction.placementMode)]}}},
        {"information", {{"hud", settings.information.hud}, {"coordinates", settings.information.coordinates},
                         {"debug", settings.information.debug},
                         {"target", settings.information.target}, {"targetIdentifier", settings.information.targetIdentifier},
                         {"targetIcon", settings.information.targetIcon},
                         {"targetHealth", settings.information.targetHealth},
                         {"targetGrowth", settings.information.targetGrowth},
                         {"targetRange", settings.information.targetRange},
                         {"targetStates", settings.information.targetStates},
                         {"targetCoordinates", settings.information.targetCoordinates},
                         {"lineOrder", settings.information.lineOrder},
                         {"biome", settings.information.biome}, {"facing", settings.information.facing},
                         {"fps", settings.information.fps}, {"frameTime", settings.information.frameTime},
                         {"light", settings.information.light},
                         {"ping", settings.information.ping},
                         {"rotation", settings.information.rotation}, {"block", settings.information.block},
                         {"chunk", settings.information.chunk}, {"speed", settings.information.speed},
                         {"time", settings.information.time}, {"weather", settings.information.weather},
                         {"moon", settings.information.moon},
                         {"dimension", settings.information.dimension}}},
        {"visuals", {{"hideOffhand", settings.visuals.hideOffhand}}},
        {"overlays", {{"chunkBorders", settings.overlays.chunkBorders}, {"hitboxes", settings.overlays.hitboxes}, {"shapes", settings.overlays.shapes},
                      {"light", settings.overlays.light}, {"skyLight", settings.overlays.skyLight},
                      {"hitboxDistance", settings.overlays.hitboxDistance}}},
        {"bindings", std::move(bindings)},
        {"camera", {{"zoom", settings.camera.zoom}, {"freelook", settings.camera.freelook}, {"freelookToggle", settings.camera.freelookToggle}, {"freecamera", settings.camera.freecamera}, {"magnification", settings.camera.magnification},
                    {"wheelStep", settings.camera.wheelStep}}},
        {"lighting", {{"nightVision", settings.lighting.nightVision}}},
        {"inspection", {{"containerPreviews", settings.inspection.containerPreviews},
                        {"shulkerPreviews", settings.inspection.shulkerPreviews},
                        {"emptyShulkerPreviews", settings.inspection.emptyShulkerPreviews},
                        {"hideShulkerContents", settings.inspection.hideShulkerContents},
                        {"bundlePreviews", settings.inspection.bundlePreviews},
                        {"emptyBundlePreviews", settings.inspection.emptyBundlePreviews},
                        {"durability", settings.inspection.durability}}},
        {"inventory", {{"sorting", settings.inventory.sorting}, {"sortContainers", settings.inventory.sortContainers},
                       {"toolSwitch", settings.inventory.toolSwitch}, {"handRestock", settings.inventory.handRestock}}},
        {"interface", {{"toggleToasts", settings.ui.toggleToasts}, {"automationStatus", settings.ui.automationStatus},
                       {"animations", settings.ui.animations}}},
        {"hud", {{"info", encodeHudElement(settings.hud.info)}, {"target", encodeHudElement(settings.hud.target)},
                   {"status", encodeHudElement(settings.hud.status)}, {"toast", encodeHudElement(settings.hud.toast)}}}
    };
}
}
Settings decodeSettings(std::string_view text) {
    auto data = parse(text);
    Settings value;
    if (data.contains("interaction")) {
        auto const& options = data.at("interaction");
        value.interaction.attackInterval = options.value("attackInterval", 0.5f);
        value.interaction.useInterval = options.value("useInterval", 0.5f);
        value.interaction.breaking = options.value("breaking",false);
        auto mode = [&](char const* key) {
            auto name = options.value(key,std::string("plane"));
            for (size_t i=0;i<interaction::restrictionNames.size();++i)
                if (name == interaction::restrictionNames[i]) return static_cast<interaction::RestrictionMode>(i);
            throw std::runtime_error("Unknown restriction mode");
        };
        value.interaction.breakingMode = mode("breakingMode");
        value.interaction.placementMode = mode("placementMode");
    }
    if (data.contains("information")) {
        auto const& info = data.at("information");
        value.information.debug = info.value("debug", false);
        value.information.target = info.value("target", false);
        value.information.targetIdentifier = info.value("targetIdentifier", true);
        value.information.targetStates = info.value("targetStates", false);
        value.information.targetIcon = info.value("targetIcon", true);
        value.information.targetHealth = info.value("targetHealth", 0);
        value.information.targetGrowth = info.value("targetGrowth", 0);
        value.information.targetRange = info.value("targetRange", 0);
        value.information.targetCoordinates = info.value("targetCoordinates", false);
        value.information.hud = info.value("hud", false);
        value.information.coordinates = info.value("coordinates", true);
        value.information.dimension = info.value("dimension", true);
        value.information.biome = info.value("biome", false);
        value.information.facing = info.value("facing", false);
        value.information.fps = info.value("fps", false);
        value.information.frameTime = info.value("frameTime", false);
        value.information.light = info.value("light", false);
        value.information.ping = info.value("ping", false);
        value.information.rotation = info.value("rotation", false);
        value.information.block = info.value("block", false);
        value.information.chunk = info.value("chunk", false);
        value.information.speed = info.value("speed", false);
        value.information.time = info.value("time", false);
        value.information.weather = info.value("weather", false);
        value.information.moon = info.value("moon", false);
        if (info.contains("lineOrder") && info.at("lineOrder").is_array()) {
            value.information.lineOrder.clear();
            for (auto const& item : info.at("lineOrder"))
                if (item.is_string()) value.information.lineOrder.push_back(item.get<std::string>());
        }
    }
    if (data.contains("visuals")) value.visuals.hideOffhand = data.at("visuals").value("hideOffhand", false);
    if (data.contains("overlays")) {
        auto const& overlays = data.at("overlays");
        value.overlays.chunkBorders = overlays.value("chunkBorders", false);
        value.overlays.hitboxes = overlays.value("hitboxes", false);
        value.overlays.shapes = overlays.value("shapes", true);
        value.overlays.light = overlays.value("light", false);
        value.overlays.skyLight = overlays.value("skyLight", false);
        value.overlays.hitboxDistance = overlays.value("hitboxDistance", 64.f);
    }
    if (data.contains("bindings")) {
        auto const& bindings = data.at("bindings");
        if (!bindings.is_object()) throw std::runtime_error("Bindings must be an object");
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
            auto stored = input::canonicalChord(std::move(chord), input::actions[i].behavior);
            // An empty settings binding would lock the screen shut; treat it
            // as absent so the default key applies.
            if (stored.empty() && i == static_cast<size_t>(input::Action::Settings)) continue;
            value.bindings[i] = std::move(stored);
        }
    }
    if (data.contains("camera")) {
        auto const& camera = data.at("camera");
        value.camera.zoom = camera.value("zoom", value.camera.zoom);
        value.camera.freelook = camera.value("freelook", value.camera.freelook);
        value.camera.freelookToggle = camera.value("freelookToggle", value.camera.freelookToggle);
        value.camera.freecamera = camera.value("freecamera", value.camera.freecamera);
        value.camera.magnification = camera.value("magnification", value.camera.magnification);
        value.camera.wheelStep = camera.value("wheelStep", value.camera.wheelStep);
    }
    if (data.contains("lighting")) {
        value.lighting.nightVision = data.at("lighting").value("nightVision", false);
    }
    if (data.contains("inspection")) {
        value.inspection.containerPreviews = data.at("inspection").value("containerPreviews", true);
        value.inspection.durability = data.at("inspection").value("durability", true);
        value.inspection.shulkerPreviews = data.at("inspection").value("shulkerPreviews", true);
        value.inspection.emptyShulkerPreviews = data.at("inspection").value("emptyShulkerPreviews", true);
        value.inspection.hideShulkerContents = data.at("inspection").value("hideShulkerContents", false);
        value.inspection.bundlePreviews = data.at("inspection").value("bundlePreviews", true);
        value.inspection.emptyBundlePreviews = data.at("inspection").value("emptyBundlePreviews", true);
    }
    if (data.contains("inventory")) {
        value.inventory.sorting = data.at("inventory").value("sorting", true);
        value.inventory.sortContainers = data.at("inventory").value("sortContainers", true);
        value.inventory.toolSwitch = data.at("inventory").value("toolSwitch", false);
        value.inventory.handRestock = data.at("inventory").value("handRestock", false);
    }
    if (data.contains("interface")) {
        value.ui.toggleToasts = data.at("interface").value("toggleToasts", true);
        value.ui.animations = data.at("interface").value("animations", 0);
        value.ui.automationStatus = data.at("interface").value("automationStatus", true);
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
