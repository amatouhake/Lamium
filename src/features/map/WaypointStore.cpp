#include "features/map/WaypointStore.h"
#include "app/AtomicFile.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <limits>
#include <stdexcept>

namespace lamium::map {
namespace {
using Json = nlohmann::json;
constexpr size_t maxDocument = 1024 * 1024;
int integer(Json const& value, int fallback) {
    if (!value.is_number_integer()) return fallback;
    auto number = value.get<double>();
    if (number < std::numeric_limits<int>::min() || number > std::numeric_limits<int>::max()) return fallback;
    return static_cast<int>(number);
}
Json point(int x, int y, int z, int dimension) { return Json{{"x", x}, {"y", y}, {"z", z}, {"dimension", dimension}}; }
}
std::string encodeWaypoints(WaypointSet const& set) {
    Json list = Json::array();
    for (auto const& w : set.waypoints) {
        auto entry = point(w.x, w.y, w.z, w.dimension);
        entry["name"] = w.name;
        entry["color"] = w.color;
        entry["visible"] = w.visible;
        if (w.temporary) entry["temporary"] = true;
        if (w.death) entry["death"] = true;
        list.push_back(std::move(entry));
    }
    Json root{{"version", 1}, {"waypoints", std::move(list)}, {"lastColor", set.lastColor}};
    if (set.lastTemporary) root["lastTemporary"] = true;
    if (set.death) root["death"] = point(set.death->x, set.death->y, set.death->z, set.death->dimension);
    return root.dump(2);
}
WaypointSet decodeWaypoints(std::string_view text) {
    if (text.size() > maxDocument) throw std::length_error("Waypoint document exceeds size limit");
    auto root = Json::parse(text);
    if (!root.is_object() || integer(root.value("version", Json()), 0) != 1)
        throw std::invalid_argument("Unsupported waypoint document");
    WaypointSet set;
    set.lastColor = std::clamp(integer(root.value("lastColor", Json()), -1), -1, static_cast<int>(waypointColors.size()) - 1);
    auto lastTemporary = root.value("lastTemporary", Json());
    set.lastTemporary = lastTemporary.is_boolean() && lastTemporary.get<bool>();
    if (auto found = root.find("waypoints"); found != root.end() && found->is_array())
        for (auto const& entry : *found) {
            if (!entry.is_object() || set.waypoints.size() >= maxWaypoints) continue;
            Waypoint w;
            auto name = entry.value("name", Json());
            if (!name.is_string()) continue;
            w.name = name.get<std::string>().substr(0, maxNameBytes);
            w.color = clampColor(integer(entry.value("color", Json()), 0));
            w.x = integer(entry.value("x", Json()), 0);
            w.y = integer(entry.value("y", Json()), 0);
            w.z = integer(entry.value("z", Json()), 0);
            w.dimension = std::clamp(integer(entry.value("dimension", Json()), 0), 0, 2);
            auto visible = entry.value("visible", Json());
            w.visible = !visible.is_boolean() || visible.get<bool>();
            auto temporary = entry.value("temporary", Json());
            w.temporary = temporary.is_boolean() && temporary.get<bool>();
            auto death = entry.value("death", Json());
            w.death = death.is_boolean() && death.get<bool>();
            set.waypoints.push_back(std::move(w));
        }
    if (auto found = root.find("death"); found != root.end() && found->is_object())
        set.death = DeathPoint{integer(found->value("x", Json()), 0), integer(found->value("y", Json()), 0),
                               integer(found->value("z", Json()), 0),
                               std::clamp(integer(found->value("dimension", Json()), 0), 0, 2)};
    return set;
}
WaypointSet readWaypoints(std::filesystem::path const& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("Could not open waypoint document");
    std::string text(maxDocument + 1, '\0');
    file.read(text.data(), static_cast<std::streamsize>(text.size()));
    if (file.bad()) throw std::runtime_error("Could not read waypoint document");
    text.resize(static_cast<size_t>(file.gcount()));
    return decodeWaypoints(text);
}
void writeWaypoints(std::filesystem::path const& path, WaypointSet const& set) {
    writeFileReplacing(path, encodeWaypoints(set), "waypoints");
}
namespace {
// A key is a file name written by keyOf or worldKey: nothing that could leave the folder.
bool safeKey(std::string const& key) {
    return !key.empty() && key.size() <= 140 && key.find_first_of("/\\:") == std::string::npos && key != ".." && key != ".";
}
}
std::string encodeServers(servers::List const& all) {
    Json list = Json::array(), worlds = Json::array();
    for (auto const& e : all.servers)
        list.push_back({{"key", e.key}, {"hosts", e.hosts}, {"ip", e.ip}, {"port", e.port}, {"levelId", e.levelId},
                        {"others", e.others}, {"anyPort", e.anyPort}, {"quiet", e.quiet}, {"pinned", e.pinned}});
    for (auto const& w : all.worlds) worlds.push_back({{"key", w.key}, {"name", w.name}});
    return Json{{"version", 1}, {"servers", std::move(list)}, {"worlds", std::move(worlds)}}.dump(2);
}
servers::List decodeServers(std::string_view text) {
    if (text.size() > maxDocument) throw std::length_error("Server list document exceeds size limit");
    auto root = Json::parse(text);
    if (!root.is_object() || integer(root.value("version", Json()), 0) != 1)
        throw std::invalid_argument("Unsupported server list document");
    servers::List all;
    auto& entries = all.servers;
    auto names = [](Json const& entry, char const* field) {
        std::vector<std::string> out;
        if (auto found = entry.find(field); found != entry.end() && found->is_array())
            for (auto const& name : *found)
                if (name.is_string() && out.size() < servers::maxNames) out.push_back(name.get<std::string>());
        return out;
    };
    if (auto found = root.find("servers"); found != root.end() && found->is_array())
        for (auto const& entry : *found) {
            if (!entry.is_object() || entries.size() >= servers::maxEntries) continue;
            servers::Entry e;
            e.key = entry.value("key", std::string());
            e.port = integer(entry.value("port", Json()), 0);
            if (!safeKey(e.key) || e.port <= 0 || e.port > 65535) continue;
            e.hosts = names(entry, "hosts");
            e.others = names(entry, "others");
            if (auto ip = entry.find("ip"); ip != entry.end() && ip->is_string()) e.ip = ip->get<std::string>();
            if (auto id = entry.find("levelId"); id != entry.end() && id->is_string()) e.levelId = id->get<std::string>();
            if (auto wide = entry.find("anyPort"); wide != entry.end() && wide->is_boolean()) e.anyPort = wide->get<bool>();
            if (auto quiet = entry.find("quiet"); quiet != entry.end() && quiet->is_boolean()) e.quiet = quiet->get<bool>();
            if (auto pinned = entry.find("pinned"); pinned != entry.end() && pinned->is_boolean()) e.pinned = pinned->get<bool>();
            entries.push_back(std::move(e));
        }
    if (auto found = root.find("worlds"); found != root.end() && found->is_array())
        for (auto const& entry : *found) {
            if (!entry.is_object() || all.worlds.size() >= servers::maxEntries) continue;
            servers::World w{entry.value("key", std::string()), entry.value("name", std::string())};
            if (safeKey(w.key)) all.worlds.push_back(std::move(w));
        }
    return all;
}
servers::List readServers(std::filesystem::path const& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("Could not open server list document");
    std::string text(maxDocument + 1, '\0');
    file.read(text.data(), static_cast<std::streamsize>(text.size()));
    if (file.bad()) throw std::runtime_error("Could not read server list document");
    text.resize(static_cast<size_t>(file.gcount()));
    return decodeServers(text);
}
void writeServers(std::filesystem::path const& path, servers::List const& all) {
    writeFileReplacing(path, encodeServers(all), "server list");
}
}
