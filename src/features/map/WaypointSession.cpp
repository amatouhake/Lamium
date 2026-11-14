#include "features/map/WaypointSession.h"
#include <algorithm>
#include "features/map/WaypointStore.h"
#include "features/map/SavedData.h"
#include "features/schematic/PlacementStore.h"
#include "overlay/LocalShapePath.h"
#include "app/Runtime.h"
#include "app/SessionIds.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/client/ClientExitLevelEvent.h"
#include "ll/api/event/client/ClientJoinLevelEvent.h"
#include "ll/api/event/client/ClientStartJoinLevelEvent.h"
#include "mc/client/game/IClientInstance.h"
#include "mc/client/game/IMinecraftGame.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/deps/core/utility/FilePathManager.h"
#include "mc/network/GameConnectionInfo.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/storage/LevelData.h"
#ifdef LAMIUM_SERVER_TRACE
#include <format>
#include <variant>
#endif
#include <array>
#include <atomic>
#include <chrono>
#include <fstream>
#include <set>
#include <cmath>
#include <ctime>
#include <mutex>

namespace lamium::map::waypoints {
namespace {
std::mutex mutex;
WaypointSet set;
std::optional<std::filesystem::path> destination;
Place joined;
bool loadFailed = false; // Never overwrite a file that could not be read.
// A death point from before L-328 that could not become a waypoint (full list,
// failed save): kept in the file, and tried again when the list changes.
std::optional<size_t> legacyDeathKept;
std::optional<DeathPoint> pendingDeath;
DeathWatch deathWatch;
ArrivalWatch arrivalWatch;
std::atomic<bool> joiningLocal{false};
std::uint64_t nextId = 1; // never reset: ids are not reused within the session
ll::event::ListenerPtr startJoinListener, joinListener, exitListener;

void log(std::string const& text) {
    try { Runtime::instance().self().getLogger().info("Waypoints: {}", text); } catch (...) {}
}
// The data kinds kept per server, each in its own folder of the config folder.
constexpr std::string_view mapFolder = "map", waypointFolder = "waypoints", schematicFolder = "schematics",
    deathFolder = "death-layouts";
// How much a key holds (servers::choose): its documents first, then the size of its map, counted up to a limit
// so that a large map costs a bounded walk at join. Below zero: nothing is there.
long long held(std::filesystem::path const& config, std::string const& key) {
    std::error_code ignored;
    long long documents = 0;
    for (auto folder : {waypointFolder, schematicFolder, deathFolder})
        if (std::filesystem::is_regular_file(config / folder / (key + ".json"), ignored)) ++documents;
    auto map = config / mapFolder / key;
    bool const hasMap = std::filesystem::is_directory(map, ignored);
    if (!documents && !hasMap) return -1;
    long long bytes = 0;
    int files = 0;
    if (hasMap)
        for (std::filesystem::recursive_directory_iterator it(map, ignored), end; !ignored && it != end && files < 2000;
             it.increment(ignored), ++files)
            if (it->is_regular_file(ignored)) bytes += static_cast<long long>(it->file_size(ignored));
    constexpr long long step = 1'000'000'000'000;
    return documents * step + std::min(bytes, step - 1);
}
// What was joined, kept for as long as it is: the saved data page lists and switches data without a join event.
struct Identity {
    bool local = false;
    std::string address, ip, levelId, levelName;
    int port = 0;
    std::optional<std::string> key; // The config key of a server's or a friend's world's data.
    std::filesystem::path worlds;   // The game's local worlds folder.
    bool hasAddress() const { return !local && port > 0 && !(address.empty() && ip.empty()); }
} identity;
std::filesystem::path configFolder() { return Runtime::instance().self().getConfigDir(); }
// Fuller data that may be the joined server's, per kind (SavedData.h); refreshed by suggest().
std::array<std::optional<SavedData>, 3> fullerFound;
void suggest();
// Reads the server list, lets `edit` change it and saves it when it did. A list that cannot be read is never
// overwritten: `edit` then works on an empty one and nothing is saved. The caller holds the lock.
template<class Edit>
bool withList(Edit edit) {
    auto file = configFolder() / "servers.json";
    servers::List list;
    bool writable = true;
    std::error_code ignored;
    if (std::filesystem::exists(file, ignored)) {
        try { list = readServers(file); }
        catch (std::exception const& error) {
            writable = false;
            log(std::string("could not read the server list: ") + error.what());
        }
    }
    auto const before = list;
    edit(list);
    if (list == before) return writable;
    if (!writable) return false;
    try { writeServers(file, list); }
    catch (std::exception const& error) { log(std::string("could not save the server list: ") + error.what()); return false; }
    return true;
}
// The key this join's data is under (L-343), or none. A server: by its address, through the record of which
// key each server was reached by. No address (a friend's world): by the level id, as a local world is.
std::optional<std::string> serverKey(Identity const& who) {
    if (!who.hasAddress()) {
        // Seen for a friend's world, whose level id is as distinct as a local world's. A world that sent a
        // plain name here would share its data with every world sending the same; the log tells which it was.
        auto key = servers::worldKey(who.levelId);
        log("joined without an address; its data goes by the level id '" + who.levelId + "'");
        if (key.empty()) return std::nullopt;
        withList([&](servers::List& list) {
            auto found = std::find_if(list.worlds.begin(), list.worlds.end(), [&](servers::World const& w) { return w.key == key; });
            if (found != list.worlds.end()) found->name = who.levelName;
            else if (list.worlds.size() < servers::maxEntries) list.worlds.push_back({key, who.levelName});
        });
        return key;
    }
    auto config = configFolder();
    std::string key;
    withList([&](servers::List& list) {
        key = servers::choose(list.servers, who.address, who.ip, who.port, who.levelId,
                              [&](std::string const& k) { return held(config, k); });
    });
    return key.empty() ? std::nullopt : std::optional(key);
}
// Where this world's data lives: a local world's Lamium folder, or for a
// server a file or folder named by its key in Lamium's config folder.
std::optional<std::filesystem::path> resolve(Identity const& who, std::string_view localName, std::string_view serverFolder,
                                             bool serverFile) {
    if (who.local) return overlay::localWorldFile(who.worlds, who.levelId, localName);
    if (!who.key) return std::nullopt;
    return configFolder() / std::filesystem::u8path(serverFolder) / std::filesystem::path(serverFile ? *who.key + ".json" : *who.key);
}
// Points every kind of data at `identity` and loads the waypoints: at a join, and when the player gives the
// joined server other data. Everything else follows `joined.world`. The caller holds the lock.
void attach() {
    set = {};
    destination.reset();
    loadFailed = false;
    legacyDeathKept.reset();
    pendingDeath.reset();
    deathWatch.reset();
    arrivalWatch.reset();
    joined = {joined.world + 1, true, std::nullopt};
    try { joined.mapFolder = resolve(identity, "map", mapFolder, false); }
    catch (std::exception const& error) { log(std::string("no folder for the world map: ") + error.what()); }
    try { joined.schematicFile = resolve(identity, "schematics.json", schematicFolder, true); }
    catch (std::exception const& error) { log(std::string("no file for schematic placements: ") + error.what()); }
    try { joined.deathLayoutFile = resolve(identity, "death-layout.json", deathFolder, true); }
    catch (std::exception const& error) { log(std::string("no file for the death layout: ") + error.what()); }
    try {
        destination = resolve(identity, "waypoints.json", waypointFolder, true);
        if (destination && std::filesystem::exists(*destination)) set = readWaypoints(*destination);
        assignSessionIds(set.waypoints, nextId);
    } catch (std::exception const& error) {
        loadFailed = true;
        log(std::string("could not load: ") + error.what());
    }
    try { suggest(); } catch (...) { fullerFound = {}; }
}
#ifdef LAMIUM_SERVER_TRACE
// L-343: everything the client has at join that could tell one server from another, to see what stays the same
// across visits. Read-only; a trace build's log names the server, so it is not for sharing as it is.
void traceIdentity(ll::event::ClientJoinLevelEvent& event) {
    try {
        auto& level = event.player().getLevel();
        std::string line = std::format("{} level id '{}' name '{}' seed {}", joiningLocal ? "local" : "remote",
            level.getLevelId(), *level.getLevelData().mLevelName, level.getSeed());
        if (auto c = event.self().getGameConnectionInfo()) {
            std::string nether = "none";
            auto const& id = *c->mNetherNetId;
            if (auto* realm = std::get_if<NetherNet::Realms::NetworkID>(&id)) nether = "realm " + realm->mValue.asString();
            else if (auto* peer = std::get_if<NetherNet::P2P::NetworkID>(&id)) nether = std::format("p2p {}", peer->mValue);
            line += std::format("; type {} transport {} host '{}' url '{}' port {} region '{}' external name '{}' raknet guid {}"
                " nethernet {} experience info {}", static_cast<int>(c->mType), static_cast<int>(c->mTransportType),
                *c->mHostIpAddress, *c->mUnresolvedUrl, static_cast<int>(c->mPort), *c->mServerRegion, *c->mExternalServerName,
                (*c->mRakNetGUID).g, nether, c->mResolvedExperienceInfo->has_value() ? "present" : "absent");
        } else line += "; no connection info";
        log("identity: " + line);
    } catch (...) { log("identity: could not be read"); }
}
#endif
void join(ll::event::ClientJoinLevelEvent& event) noexcept {
    try {
        if (event.self().getLocalPlayer() != &event.player()) return;
#ifdef LAMIUM_SERVER_TRACE
        traceIdentity(event);
#endif
        std::lock_guard lock(mutex);
        identity = {};
        identity.local = joiningLocal;
        auto& level = event.player().getLevel();
        identity.levelId = level.getLevelId();
        try { identity.levelName = *level.getLevelData().mLevelName; } catch (...) {}
        try { identity.worlds = std::filesystem::u8path(event.self().getMinecraftGame_DEPRECATED().getFilePathManager()->mWorlds->value); }
        catch (...) {}
        if (!identity.local) {
            if (auto connection = event.self().getGameConnectionInfo()) {
                identity.address = *connection->mUnresolvedUrl;
                identity.ip = *connection->mHostIpAddress;
                identity.port = static_cast<int>(connection->mPort);
            }
            try { identity.key = serverKey(identity); }
            catch (std::exception const& error) { log(std::string("no name for this server's data: ") + error.what()); }
            // Which folder and files to look in when data seems to be missing (L-343).
            log(identity.key ? "this server's data is under '" + *identity.key + "'"
                             : std::string("this server has nothing to name its data by; nothing is saved"));
        }
        attach();
    } catch (...) {}
}
void leave() {
    std::lock_guard lock(mutex);
    joined.active = false;
    identity = {};
    fullerFound = {};
    set = {};
    destination.reset();
    loadFailed = false;
    legacyDeathKept.reset();
    pendingDeath.reset();
    deathWatch.reset();
    arrivalWatch.reset();
}
// Saves a candidate, then publishes it; the caller holds the lock.
bool commit(WaypointSet candidate) {
    if (loadFailed) return false;
    if (destination) {
        try { writeWaypoints(*destination, candidate); }
        catch (std::exception const& error) { log(std::string("could not save: ") + error.what()); return false; }
    }
    assignSessionIds(candidate.waypoints, nextId);
    set = std::move(candidate);
    return true;
}
}
Place place() {
    std::lock_guard lock(mutex);
    return joined;
}
WaypointSet current() {
    std::lock_guard lock(mutex);
    return set;
}
std::uint64_t add(Waypoint waypoint, bool rememberKind) {
    std::lock_guard lock(mutex);
    if (set.waypoints.size() >= maxWaypoints) return 0;
    auto candidate = set;
    waypoint.color = clampColor(waypoint.color);
    if (waypoint.name.size() > maxNameBytes) waypoint.name.resize(maxNameBytes);
    waypoint.id = 0;
    candidate.lastColor = waypoint.color;
    if (rememberKind) candidate.lastTemporary = waypoint.temporary;
    candidate.waypoints.push_back(std::move(waypoint));
    if (!commit(std::move(candidate))) return 0;
    return set.waypoints.back().id;
}
bool change(std::function<bool(WaypointSet&)> const& mutation) {
    std::lock_guard lock(mutex);
    auto candidate = set;
    if (!mutation(candidate)) return false;
    for (auto& w : candidate.waypoints) {
        w.color = clampColor(w.color);
        if (w.name.size() > maxNameBytes) w.name.resize(maxNameBytes);
    }
    return commit(std::move(candidate));
}
void watchDeath(IClientInstance& client) {
    auto* player = client.getLocalPlayer();
    if (!player) return;
    bool alive = player->isAlive();
    std::lock_guard lock(mutex);
    if (!deathWatch.update(alive)) return;
    auto feet = player->getFeetPos();
    if (!std::isfinite(feet.x) || !std::isfinite(feet.y) || !std::isfinite(feet.z)) return;
    pendingDeath = DeathPoint{static_cast<int>(std::floor(feet.x)), static_cast<int>(std::floor(feet.y)),
                              static_cast<int>(std::floor(feet.z)), static_cast<int>(player->getDimensionId())};
}
void frame(bool recordDeath, bool temporary, std::string const& name) {
    std::lock_guard lock(mutex);
    // A death point saved before L-328 becomes its waypoint.
    if (set.death && legacyDeathKept != set.waypoints.size()) {
        auto candidate = set;
        auto old = *std::exchange(candidate.death, std::nullopt);
        if (!map::recordDeath(candidate, old, name, temporary) || !commit(std::move(candidate)))
            legacyDeathKept = set.waypoints.size();
    }
    if (!pendingDeath) return;
    auto death = *std::exchange(pendingDeath, std::nullopt);
    if (!recordDeath) return;
    auto candidate = set;
    // The older file's death point above has no time; this one is now.
    std::string dated = name;
    std::time_t const now = std::time(nullptr);
    if (std::tm local{}; localtime_s(&local, &now) == 0)
        dated = deathName(name, local.tm_mon + 1, local.tm_mday, local.tm_hour, local.tm_min);
    if (map::recordDeath(candidate, death, std::move(dated), temporary)) commit(std::move(candidate));
}
std::vector<std::string> arrive(int dimension, double x, double y, double z, double distance) {
    std::lock_guard lock(mutex);
    return arrivalStep(arrivalWatch, set, dimension, x, y, z, distance,
                       [](WaypointSet const& candidate) { return commit(candidate); });
}
void start() {
    auto& bus = ll::event::EventBus::getInstance();
    startJoinListener = bus.emplaceListener<ll::event::ClientStartJoinLevelEvent>(
        [](auto& event) { leave(); joiningLocal = event.isJoiningLocalServer(); });
    joinListener = bus.emplaceListener<ll::event::ClientJoinLevelEvent>(join);
    exitListener = bus.emplaceListener<ll::event::ClientExitLevelEvent>([](auto&) { leave(); });
    if (!startJoinListener || !joinListener || !exitListener) {
        stop();
        throw std::runtime_error("Could not subscribe waypoint world changes");
    }
}
void stop() {
    for (auto* listener : {&startJoinListener, &joinListener, &exitListener})
        if (*listener) {
            ll::event::EventBus::getInstance().removeListener(*listener);
            listener->reset();
        }
    leave();
}

// ---- Saved data (L-343 part 3) ----
namespace {
bool safeKey(std::string const& key) {
    // A name as written by keyOf or worldKey. Windows drops a trailing dot or space from a path's last part.
    return !key.empty() && key.size() <= 140 && key.find_first_of("/\\:") == std::string::npos && key.back() != '.'
        && key.back() != ' ';
}
std::int64_t writtenAt(std::filesystem::path const& path) {
    std::error_code ignored;
    auto time = std::filesystem::last_write_time(path, ignored);
    if (ignored) return 0;
    // file_clock to system_clock without clock_cast: the difference of the two "now"s.
    auto system = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        time - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
    return std::chrono::duration_cast<std::chrono::seconds>(system.time_since_epoch()).count();
}
// One data's four kinds, given where each lies. Bounded: the map walk stops at 2000 files.
void measure(SavedData& item, std::filesystem::path const& waypointFile, std::filesystem::path const& schematicFile,
             std::filesystem::path const& deathFile, std::filesystem::path const& map) {
    std::error_code ignored;
    if (std::filesystem::is_regular_file(waypointFile, ignored)) {
        item.used = std::max(item.used, writtenAt(waypointFile));
        try {
            auto set = readWaypoints(waypointFile);
            item.waypoints = static_cast<int>(set.waypoints.size());
            for (auto const& w : set.waypoints) {
                if (item.marks.size() < savedMarks) item.marks.push_back({w.name, clampColor(w.color)});
                if (w.dimension == 0 && item.dots.size() < savedDots) item.dots.push_back({w.x, w.z, clampColor(w.color)});
            }
        } catch (...) {}
    }
    if (std::filesystem::is_regular_file(schematicFile, ignored)) {
        item.used = std::max(item.used, writtenAt(schematicFile));
        try { item.placements = static_cast<int>(schematic::readPlacements(schematicFile).placements.size()); } catch (...) {}
    }
    if (std::filesystem::is_regular_file(deathFile, ignored)) item.used = std::max(item.used, writtenAt(deathFile));
    if (std::filesystem::is_directory(map, ignored)) {
        item.mapFolder = map;
        int files = 0;
        std::filesystem::recursive_directory_iterator it(map, ignored), end;
        for (; !ignored && it != end; it.increment(ignored)) {
            if (++files > 2000) { item.mapMore = true; break; }
            if (!it->is_regular_file(ignored)) continue;
            item.mapBytes += static_cast<long long>(it->file_size(ignored));
            item.used = std::max(item.used, writtenAt(it->path()));
        }
    }
}
std::string firstLine(std::filesystem::path const& path) {
    std::ifstream file(path, std::ios::binary);
    std::string line;
    if (file) std::getline(file, line);
    while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
    if (line.size() > 80) line.resize(80);
    return line;
}
}
namespace {
// One server data's summary by key. The caller holds the lock.
SavedData summary(std::filesystem::path const& config, servers::List const& list, std::string const& key, std::string const& current) {
    SavedData item;
    item.key = key;
    item.name = servers::shownName(key);
    for (auto const& e : list.servers) {
        if (e.key != key) continue;
        item.anyPort = item.anyPort || e.anyPort;
        if (item.levelId.empty()) item.levelId = e.levelId;
        if (item.host.empty()) item.host = e.hosts.empty() ? e.ip : e.hosts.back();
        item.users.push_back({e.hosts.empty() ? e.ip : e.hosts.back(), e.port, e.anyPort});
    }
    item.maybe = servers::maybe(list.servers, current, key);
    measure(item, config / waypointFolder / (key + ".json"), config / schematicFolder / (key + ".json"),
            config / deathFolder / (key + ".json"), config / mapFolder / key);
    return item;
}
// Works out fullerFound for the joined server. Reads a few files: at a join and after a change only.
void suggest() {
    fullerFound = {};
    if (!joined.active || !identity.hasAddress() || !identity.key) return;
    std::error_code ignored;
    auto config = configFolder();
    servers::List list;
    if (std::filesystem::exists(config / "servers.json", ignored)) list = readServers(config / "servers.json");
    std::string const current = *identity.key;
    for (auto const& e : list.servers) if (e.key == current && e.quiet) return;
    auto keys = servers::candidates(list.servers, current);
    if (keys.size() > 12) keys.resize(12);
    auto mine = summary(config, list, current, current);
    std::vector<SavedData> others;
    std::vector<servers::Amounts> amounts;
    for (auto const& key : keys) {
        if (!safeKey(key)) continue;
        others.push_back(summary(config, list, key, current));
        amounts.push_back({others.back().waypoints, others.back().placements, others.back().mapBytes});
    }
    auto best = servers::fuller({mine.waypoints, mine.placements, mine.mapBytes}, amounts);
    for (auto [index, slot] : {std::pair{best.waypoints, 0}, std::pair{best.placements, 1}, std::pair{best.map, 2}})
        if (index >= 0) {
            fullerFound[static_cast<size_t>(slot)] = others[static_cast<size_t>(index)];
            fullerFound[static_cast<size_t>(slot)]->canUse = true;
        }
}
}
std::optional<SavedData> fullerData(DataKind kind) {
    std::lock_guard lock(mutex);
    return fullerFound[static_cast<size_t>(kind)];
}
void quietFullerData() {
    std::lock_guard lock(mutex);
    fullerFound = {};
    if (!identity.hasAddress() || !identity.key) return;
    withList([&](servers::List& list) {
        for (auto& e : list.servers) if (e.key == *identity.key) e.quiet = true;
    });
}
std::vector<SavedData> savedData() {
    std::lock_guard lock(mutex);
    std::vector<SavedData> items;
    std::error_code ignored;
    auto config = configFolder();
    servers::List list;
    try { if (std::filesystem::exists(config / "servers.json", ignored)) list = readServers(config / "servers.json"); } catch (...) {}
    std::string const current = !identity.local && identity.key ? *identity.key : std::string();
    // Every key any of the four folders holds.
    std::set<std::string> keys;
    for (auto folder : {mapFolder, waypointFolder, schematicFolder, deathFolder}) {
        std::filesystem::directory_iterator it(config / folder, ignored), end;
        for (; !ignored && it != end && keys.size() < savedDataLimit; it.increment(ignored)) {
            auto name = it->path().filename().u8string();
            std::string key(name.begin(), name.end());
            if (folder != mapFolder) {
                if (!key.ends_with(".json")) continue;
                key.resize(key.size() - 5);
            }
            if (safeKey(key)) keys.insert(std::move(key));
        }
        ignored.clear();
    }
    // The data in use is listed even before anything is saved under it.
    if (!current.empty() && safeKey(current)) keys.insert(current);
    for (auto const& key : keys) {
        SavedData item;
        item.key = key;
        item.kind = key.starts_with("world_") ? SavedData::Kind::Friend : SavedData::Kind::Server;
        if (item.kind == SavedData::Kind::Friend) {
            auto found = std::find_if(list.worlds.begin(), list.worlds.end(), [&](servers::World const& w) { return w.key == key; });
            if (found != list.worlds.end()) item.name = found->name;
        } else {
            item.name = servers::shownName(key);
            for (auto const& e : list.servers) {
                if (e.key != key) continue;
                item.anyPort = item.anyPort || e.anyPort;
                if (item.levelId.empty()) item.levelId = e.levelId;
                if (item.host.empty()) item.host = e.hosts.empty() ? e.ip : e.hosts.back();
                item.users.push_back({e.hosts.empty() ? e.ip : e.hosts.back(), e.port, e.anyPort});
            }
            item.maybe = servers::maybe(list.servers, current, key);
        }
        measure(item, config / waypointFolder / (key + ".json"), config / schematicFolder / (key + ".json"),
                config / deathFolder / (key + ".json"), config / mapFolder / key);
        item.inUse = !current.empty() && key == current;
        item.canUse = item.kind == SavedData::Kind::Server && identity.hasAddress() && !item.inUse;
        item.canDelete = !item.inUse;
        items.push_back(std::move(item));
    }
    // Local worlds that have a Lamium folder.
    if (!identity.worlds.empty()) {
        std::filesystem::directory_iterator it(identity.worlds, ignored), end;
        for (; !ignored && it != end && items.size() < savedDataLimit; it.increment(ignored)) {
            auto folder = it->path() / "lamium";
            if (!std::filesystem::is_directory(folder, ignored)) { ignored.clear(); continue; }
            SavedData item;
            item.kind = SavedData::Kind::Local;
            auto name = it->path().filename().u8string();
            item.key.assign(name.begin(), name.end());
            item.name = firstLine(it->path() / "levelname.txt");
            item.levelId = item.key;
            measure(item, folder / "waypoints.json", folder / "schematics.json", folder / "death-layout.json", folder / "map");
            if (!item.waypoints && !item.placements && !item.mapBytes && !item.used) continue;
            item.inUse = identity.local && identity.levelId == item.key;
            item.canDelete = !item.inUse;
            items.push_back(std::move(item));
        }
    }
    // In use first, then what may be this server's, then the latest used.
    std::stable_sort(items.begin(), items.end(), [](SavedData const& a, SavedData const& b) {
        auto rank = [](SavedData const& d) { return d.inUse ? 0 : d.maybe != servers::Maybe::No ? 1 : 2; };
        return rank(a) != rank(b) ? rank(a) < rank(b) : a.used > b.used;
    });
    return items;
}
bool useData(std::string const& key) {
    std::lock_guard lock(mutex);
    if (!joined.active || !identity.hasAddress() || !safeKey(key) || key.starts_with("world_")) return false;
    if (identity.key && *identity.key == key) return true;
    bool assigned = false;
    bool const saved = withList([&](servers::List& list) {
        assigned = servers::assign(list.servers, identity.address, identity.ip, identity.port, identity.levelId, key);
    });
    if (!assigned || !saved) return false;
    log("this server now uses the data under '" + key + "'");
    identity.key = key;
    attach();
    return true;
}
bool removeData(SavedData const& item) {
    std::lock_guard lock(mutex);
    std::error_code ignored;
    if (item.kind == SavedData::Kind::Local) {
        if (identity.local && identity.levelId == item.key) return false;
        try {
            // localWorldFile checks that the folder is a world of the game's own worlds folder.
            for (std::string_view name : {"waypoints.json", "schematics.json", "death-layout.json"})
                if (auto file = overlay::localWorldFile(identity.worlds, item.key, name)) std::filesystem::remove(*file, ignored);
            if (auto map = overlay::localWorldFile(identity.worlds, item.key, "map")) std::filesystem::remove_all(*map, ignored);
        } catch (std::exception const& error) { log(std::string("could not delete a world's data: ") + error.what()); return false; }
        log("deleted the saved data of the local world '" + item.key + "'");
        return true;
    }
    if (!safeKey(item.key) || (!identity.local && identity.key && *identity.key == item.key)) return false;
    auto config = configFolder();
    for (auto folder : {waypointFolder, schematicFolder, deathFolder}) std::filesystem::remove(config / folder / (item.key + ".json"), ignored);
    std::filesystem::remove_all(config / mapFolder / item.key, ignored);
    withList([&](servers::List& list) { servers::forget(list, item.key); });
    log("deleted the saved data under '" + item.key + "'");
    try { suggest(); } catch (...) { fullerFound = {}; }
    return true;
}
int setDataAnyPort(std::string const& key, bool on) {
    std::lock_guard lock(mutex);
    int released = -1;
    if (!withList([&](servers::List& list) { released = servers::setAnyPort(list.servers, key, on); }) || released < 0) return -1;
    // The joined server may be one of the ports that now follow the switch.
    if (joined.active && identity.hasAddress()) {
        auto now = serverKey(identity);
        if (now && now != identity.key) {
            log("this server now uses the data under '" + *now + "'");
            identity.key = now;
            attach();
        } else {
            try { suggest(); } catch (...) { fullerFound = {}; }
        }
    }
    return released;
}
std::string dataName() {
    std::lock_guard lock(mutex);
    if (!joined.active || identity.local || !identity.key) return {};
    if (!identity.hasAddress()) return identity.levelName;
    return servers::shownName(*identity.key);
}
}
