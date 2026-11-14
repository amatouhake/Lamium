#include "features/map/WaypointSession.h"
#include <algorithm>
#include "features/map/WaypointStore.h"
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
#ifdef LAMIUM_SERVER_TRACE
#include "mc/world/level/storage/LevelData.h"
#include <format>
#include <variant>
#endif
#include <atomic>
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
// The key this join's data is under (L-343), or none. A server: by its address, through the record of which
// key each server was reached by. No address (a friend's world): by the level id, as a local world is.
std::optional<std::string> serverKey(ll::event::ClientJoinLevelEvent& event) {
    auto connection = event.self().getGameConnectionInfo();
    std::string const levelId = event.player().getLevel().getLevelId();
    std::string address = connection ? *connection->mUnresolvedUrl : std::string();
    std::string ip = connection ? *connection->mHostIpAddress : std::string();
    int const port = connection ? static_cast<int>(connection->mPort) : 0;
    if (port <= 0 || (address.empty() && ip.empty())) {
        // Seen for a friend's world, whose level id is as distinct as a local world's. A world that sent a
        // plain name here would share its data with every world sending the same; the log tells which it was.
        auto key = servers::worldKey(levelId);
        log("joined without an address; its data goes by the level id '" + levelId + "'");
        return key.empty() ? std::nullopt : std::optional(key);
    }
    auto config = Runtime::instance().self().getConfigDir();
    auto list = config / "servers.json";
    std::vector<servers::Entry> entries;
    bool writable = true;
    std::error_code ignored;
    if (std::filesystem::exists(list, ignored)) {
        try { entries = readServers(list); }
        catch (std::exception const& error) {
            // Never overwrite a list that could not be read; this join goes by the names alone.
            writable = false;
            log(std::string("could not read the server list: ") + error.what());
        }
    }
    auto const before = entries;
    auto key = servers::choose(entries, address, ip, port, levelId, [&](std::string const& k) { return held(config, k); });
    if (key.empty()) return std::nullopt;
    if (writable && entries != before) {
        try { writeServers(list, entries); }
        catch (std::exception const& error) { log(std::string("could not save the server list: ") + error.what()); }
    }
    return key;
}
// Where this world's data lives: a local world's Lamium folder, or for a
// server a file or folder named by its key in Lamium's config folder.
std::optional<std::filesystem::path> resolve(ll::event::ClientJoinLevelEvent& event, std::optional<std::string> const& key,
                                             std::string_view localName, std::string_view serverFolder, bool serverFile) {
    if (joiningLocal) {
        auto paths = event.self().getMinecraftGame_DEPRECATED().getFilePathManager();
        return overlay::localWorldFile(std::filesystem::u8path(paths->mWorlds->value),
                                       event.player().getLevel().getLevelId(), localName);
    }
    if (!key) return std::nullopt;
    return Runtime::instance().self().getConfigDir() / std::filesystem::u8path(serverFolder)
        / std::filesystem::path(serverFile ? *key + ".json" : *key);
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
        set = {};
        destination.reset();
        loadFailed = false;
        legacyDeathKept.reset();
        pendingDeath.reset();
        deathWatch.reset();
        arrivalWatch.reset();
        joined = {joined.world + 1, true, std::nullopt};
        std::optional<std::string> key;
        if (!joiningLocal) {
            try { key = serverKey(event); }
            catch (std::exception const& error) { log(std::string("no name for this server's data: ") + error.what()); }
            // Which folder and files to look in when data seems to be missing (L-343).
            log(key ? "this server's data is under '" + *key + "'" : std::string("this server has nothing to name its data by; nothing is saved"));
        }
        try { joined.mapFolder = resolve(event, key, "map", mapFolder, false); }
        catch (std::exception const& error) { log(std::string("no folder for the world map: ") + error.what()); }
        try { joined.schematicFile = resolve(event, key, "schematics.json", schematicFolder, true); }
        catch (std::exception const& error) { log(std::string("no file for schematic placements: ") + error.what()); }
        try { joined.deathLayoutFile = resolve(event, key, "death-layout.json", deathFolder, true); }
        catch (std::exception const& error) { log(std::string("no file for the death layout: ") + error.what()); }
        try {
            destination = resolve(event, key, "waypoints.json", waypointFolder, true);
            if (destination && std::filesystem::exists(*destination)) set = readWaypoints(*destination);
            assignSessionIds(set.waypoints, nextId);
        } catch (std::exception const& error) {
            loadFailed = true;
            log(std::string("could not load: ") + error.what());
        }
    } catch (...) {}
}
void leave() {
    std::lock_guard lock(mutex);
    joined.active = false;
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
}
