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
        {"information", {{"hud", settings.information.hud}, {"coordinates", settings.information.coordinates},
                         {"target", settings.information.target}, {"targetIdentifier", settings.information.targetIdentifier},
                         {"biome", settings.information.biome}, {"facing", settings.information.facing},
                         {"fps", settings.information.fps}, {"frameTime", settings.information.frameTime},
                         {"light", settings.information.light},
                         {"ping", settings.information.ping},
                         {"dimension", settings.information.dimension}, {"horizontal", settings.information.horizontal},
                         {"vertical", settings.information.vertical}}},
        {"visuals", {{"hideOffhand", settings.visuals.hideOffhand}}},
        {"overlays", {{"chunkBorders", settings.overlays.chunkBorders}, {"hitboxes", settings.overlays.hitboxes},
                      {"hitboxDistance", settings.overlays.hitboxDistance}}},
        {"bindings", std::move(bindings)},
        {"camera", {{"zoom", settings.camera.zoom}, {"magnification", settings.camera.magnification},
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
                       {"toolSwitch", settings.inventory.toolSwitch}}},
        {"interface", {{"gameplayHints", settings.ui.gameplayHints}}}
    };
}
}
Settings decodeSettings(std::string_view text) {
    auto data = parse(text);
    Settings value;
    if (data.contains("information")) {
        auto const& info = data.at("information");
        value.information.target = info.value("target", false);
        value.information.targetIdentifier = info.value("targetIdentifier", true);
        value.information.hud = info.value("hud", false);
        value.information.coordinates = info.value("coordinates", true);
        value.information.dimension = info.value("dimension", true);
        value.information.biome = info.value("biome", false);
        value.information.facing = info.value("facing", false);
        value.information.fps = info.value("fps", false);
        value.information.frameTime = info.value("frameTime", false);
        value.information.light = info.value("light", false);
        value.information.ping = info.value("ping", false);
        value.information.horizontal = info.value("horizontal", 2.f);
        value.information.vertical = info.value("vertical", 15.f);
    }
    if (data.contains("visuals")) value.visuals.hideOffhand = data.at("visuals").value("hideOffhand", false);
    if (data.contains("overlays")) {
        auto const& overlays = data.at("overlays");
        value.overlays.chunkBorders = overlays.value("chunkBorders", false);
        value.overlays.hitboxes = overlays.value("hitboxes", false);
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
            value.bindings[i] = input::canonicalChord(std::move(chord), input::actions[i].behavior);
        }
    }
    if (data.contains("camera")) {
        auto const& camera = data.at("camera");
        value.camera.zoom = camera.value("zoom", value.camera.zoom);
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
    }
    if (data.contains("interface")) {
        value.ui.gameplayHints = data.at("interface").value("gameplayHints", true);
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
