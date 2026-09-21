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
    return Json{
        {"version", settings.version},
        {"camera", {{"zoom", settings.camera.zoom}, {"magnification", settings.camera.magnification},
                    {"wheelStep", settings.camera.wheelStep}}},
        {"lighting", {{"nightVision", settings.lighting.nightVision}}},
        {"inspection", {{"containerPreviews", settings.inspection.containerPreviews},
                        {"durability", settings.inspection.durability}}},
        {"inventory", {{"sorting", settings.inventory.sorting}, {"sortContainers", settings.inventory.sortContainers}}}
    };
}
}
Settings decodeSettings(std::string_view text) {
    auto data = parse(text);
    Settings value;
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
    }
    if (data.contains("inventory")) {
        value.inventory.sorting = data.at("inventory").value("sorting", true);
        value.inventory.sortContainers = data.at("inventory").value("sortContainers", true);
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
