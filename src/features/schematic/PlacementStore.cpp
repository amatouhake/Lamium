#include "features/schematic/PlacementStore.h"
#include "app/AtomicFile.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <format>
#include <fstream>
#include <limits>
#include <stdexcept>

namespace lamium::schematic {
namespace {
using Json = nlohmann::json;
constexpr size_t maxDocument = 1024 * 1024;
constexpr int coordinateLimit = 30'000'000;
int integer(Json const& value, int fallback) {
    if (!value.is_number_integer()) return fallback;
    auto number = value.get<double>();
    if (number < std::numeric_limits<int>::min() || number > std::numeric_limits<int>::max()) return fallback;
    return static_cast<int>(number);
}
bool flag(Json const& value, bool fallback) { return value.is_boolean() ? value.get<bool>() : fallback; }
std::string textOf(Json const& value) { return value.is_string() ? value.get<std::string>() : std::string{}; }
char const* axisName(LayerAxis axis) {
    return axis == LayerAxis::Y ? "y" : axis == LayerAxis::X ? "x" : "z";
}
LayerAxis axisFromName(std::string const& name) {
    return name == "x" ? LayerAxis::X : name == "z" ? LayerAxis::Z : LayerAxis::Y;
}
char const* keepName(LayerKeep keep) { return keep == LayerKeep::High ? "high" : "low"; }
LayerKeep keepFromName(std::string const& name) { return name == "high" ? LayerKeep::High : LayerKeep::Low; }
char const* modeName(LayerMode mode) { return mode == LayerMode::Only ? "only" : mode == LayerMode::UpTo ? "upTo" : "all"; }
LayerMode modeFromName(std::string const& name) {
    return name == "only" ? LayerMode::Only : name == "upTo" ? LayerMode::UpTo : LayerMode::All;
}
// What the ghost mesh actually depends on: All ignores the cut; Only ignores
// the kept side; UpTo needs both. Legacy views use the old side/index fields.
std::string layerVisibilityKey(Layers const& layers) {
    if (layers.legacy) {
        if (layers.legacyMode == LayerMode::All) return "a";
        return std::format("l{}:{}:{}", static_cast<int>(layers.legacyAxis), layers.legacyIndex,
            static_cast<int>(layers.legacyMode));
    }
    if (layers.mode == LayerMode::All) return "a";
    if (layers.mode == LayerMode::Only)
        return std::format("v{}:{}", static_cast<int>(layers.axis), layers.cut);
    return std::format("v{}:{}:{}", static_cast<int>(layers.axis), layers.cut, static_cast<int>(layers.keep));
}
}

std::string drawKey(SavedPlacement const& p) {
    auto const& o = p.placement.origin;
    return std::format("{}|{}|{},{},{}|{}|{}|{}|{}{}", p.file, p.dimension, o.x, o.y, o.z,
        p.placement.rotation, static_cast<int>(p.placement.mirror), layerVisibilityKey(p.layers),
        p.countExtras ? 'x' : '-', p.entities ? 'e' : '-');
}
std::string checkKey(SavedPlacement const& p) { return drawKey(p) + "|" + p.ignored.fingerprint(); }
bool safeSchematicPath(std::string_view relative) {
    if (relative.empty() || relative.size() > maxFileBytes || relative.front() == '/' || relative.front() == '\\') return false;
    if (relative.find(':') != std::string_view::npos || relative.find('\\') != std::string_view::npos) return false;
    size_t start = 0;
    while (start <= relative.size()) {
        size_t end = relative.find('/', start);
        auto part = relative.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start);
        if (part.empty() || part == "." || part == "..") return false;
        if (end == std::string_view::npos) break;
        start = end + 1;
    }
    return true;
}

void normalize(PlacementSet& set) {
    if (set.placements.size() > maxPlacements) set.placements.resize(maxPlacements);
    for (auto& p : set.placements) {
        if (p.name.size() > maxPlacementName) p.name.resize(maxPlacementName);
        p.placement.rotation = quarterTurns(p.placement.rotation);
        p.dimension = std::clamp(p.dimension, 0, 2);
        auto& o = p.placement.origin;
        o.x = std::clamp(o.x, -coordinateLimit, coordinateLimit);
        o.y = std::clamp(o.y, -4096, 4096);
        o.z = std::clamp(o.z, -coordinateLimit, coordinateLimit);
        p.layers.cut = std::max(0, p.layers.cut);
        p.layers.legacyIndex = std::max(0, p.layers.legacyIndex);
    }
    if (set.selected < -1 || set.selected >= static_cast<int>(set.placements.size())) set.selected = -1;
}

std::string encodePlacements(PlacementSet const& set) {
    Json list = Json::array();
    for (auto const& p : set.placements) {
        Json entry{
            {"name", p.name}, {"file", p.file}, {"dimension", p.dimension},
            {"x", p.placement.origin.x}, {"y", p.placement.origin.y}, {"z", p.placement.origin.z},
            {"rotation", p.placement.rotation}, {"mirror", static_cast<int>(p.placement.mirror)},
            {"layerAxis", static_cast<int>(p.layers.legacyAxis)}, {"layerMode", static_cast<int>(p.layers.legacyMode)},
            {"layer", p.layers.legacyIndex}, {"visible", p.visible}, {"countExtras", p.countExtras}, {"entities", p.entities},
        };
        // A placement still on the old form keeps only the old fields until its
        // size is known; a converted view carries both for rollback.
        if (!p.layers.legacy)
            entry["layerView"] = {{"space", "structure"}, {"axis", axisName(p.layers.axis)}, {"cut", p.layers.cut},
                {"keep", keepName(p.layers.keep)}, {"mode", modeName(p.layers.mode)}};
        // Optional (L-183): left out when empty, so older documents read and
        // write back unchanged.
        if (!p.ignored.empty()) {
            Json ignored = Json::array();
            for (auto const& e : p.ignored.entries()) ignored.push_back(encodeIgnored(e));
            entry["ignored"] = std::move(ignored);
        }
        list.push_back(std::move(entry));
    }
    return Json{{"version", 1}, {"placements", std::move(list)}, {"selected", set.selected}}.dump(2);
}

PlacementSet decodePlacements(std::string_view text) {
    if (text.size() > maxDocument) throw std::length_error("Placement document exceeds size limit");
    auto root = Json::parse(text);
    if (!root.is_object() || integer(root.value("version", Json()), 0) != 1)
        throw std::invalid_argument("Unsupported placement document");
    PlacementSet set;
    if (auto found = root.find("placements"); found != root.end() && found->is_array())
        for (auto const& entry : *found) {
            if (!entry.is_object() || set.placements.size() >= maxPlacements) continue;
            auto file = entry.value("file", Json());
            if (!file.is_string() || !safeSchematicPath(file.get<std::string>())) continue;
            SavedPlacement p;
            p.file = file.get<std::string>();
            auto name = entry.value("name", Json());
            p.name = name.is_string() ? name.get<std::string>() : p.file;
            p.dimension = integer(entry.value("dimension", Json()), 0);
            p.placement.origin = {integer(entry.value("x", Json()), 0), integer(entry.value("y", Json()), 0),
                                  integer(entry.value("z", Json()), 0)};
            p.placement.rotation = integer(entry.value("rotation", Json()), 0);
            p.placement.mirror = static_cast<Mirror>(std::clamp(integer(entry.value("mirror", Json()), 0), 0, 2));
            p.layers.legacyAxis = static_cast<LegacyLayerAxis>(std::clamp(integer(entry.value("layerAxis", Json()), 0), 0, 5));
            p.layers.legacyMode = static_cast<LayerMode>(std::clamp(integer(entry.value("layerMode", Json()), 0), 0, 2));
            p.layers.legacyIndex = integer(entry.value("layer", Json()), 0);
            auto view = entry.find("layerView");
            if (view != entry.end() && view->is_object() && textOf(view->value("space", Json())) == "structure") {
                p.layers.legacy = false;
                p.layers.axis = axisFromName(textOf(view->value("axis", Json())));
                p.layers.cut = integer(view->value("cut", Json()), 0);
                p.layers.keep = keepFromName(textOf(view->value("keep", Json())));
                p.layers.mode = modeFromName(textOf(view->value("mode", Json())));
            } else {
                // No space marker: an old placed-box layerView from the first
                // cut-plane build. Its old fields carry the same view.
                p.layers.legacy = true;
            }
            p.visible = flag(entry.value("visible", Json()), true);
            p.countExtras = flag(entry.value("countExtras", Json()), true);
            p.entities = flag(entry.value("entities", Json()), true);
            if (auto ignored = entry.find("ignored"); ignored != entry.end() && ignored->is_array()) {
                std::vector<IgnoredDifference> entries;
                for (auto const& item : *ignored)
                    if (item.is_string() && entries.size() < maxIgnored)
                        if (auto e = decodeIgnored(item.get<std::string>())) entries.push_back(std::move(*e));
                p.ignored = IgnoreList(std::move(entries));
            }
            set.placements.push_back(std::move(p));
        }
    set.selected = integer(root.value("selected", Json()), -1);
    normalize(set);
    return set;
}

PlacementSet readPlacements(std::filesystem::path const& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("Could not open placement document");
    std::string text(maxDocument + 1, '\0');
    file.read(text.data(), static_cast<std::streamsize>(text.size()));
    if (file.bad()) throw std::runtime_error("Could not read placement document");
    text.resize(static_cast<size_t>(file.gcount()));
    return decodePlacements(text);
}

void writePlacements(std::filesystem::path const& path, PlacementSet const& set) {
    writeFileReplacing(path, encodePlacements(set), "schematic placements");
}
}
