#include "features/map/WaypointSession.h"
#include <algorithm>
#include "features/map/WaypointStore.h"
#include "features/map/MapStore.h"
#include "features/map/DataMerge.h"
#include "features/map/MapRegionFiles.h"
#include "features/map/SavedData.h"
#include "features/map/RealmCookie.h"
#include "features/schematic/PlacementStore.h"
#include "overlay/LocalShapePath.h"
#include "overlay/ShapeStore.h"
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
#include "mc/world/level/LevelSeed64.h"
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
    deathFolder = "death-layouts", shapeFolder = "shapes", wantedFolder = "wanted",
    placesFolder = "places";
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
// Whether a key holds any data at all, so a new data set does not take it: `held` ranks only what makes a
// server's data fuller, not its shapes, wanted items or counted places.
bool anyData(std::filesystem::path const& config, std::string const& key) {
    if (held(config, key) >= 0) return true;
    std::error_code ignored;
    for (auto folder : {shapeFolder, wantedFolder, placesFolder})
        if (std::filesystem::is_regular_file(config / folder / (key + ".json"), ignored)) return true;
    return false;
}
// What was joined, kept for as long as it is: the saved data page lists and switches data without a join event.
struct Identity {
    bool local = false;
    std::string address, ip, levelId, levelName;
    int port = 0;
    std::int64_t realm = 0;         // A Realm's id (L-358); its data goes by it, whatever the address.
    prints::Print print;            // The world joined (L-365).
    std::string base;               // The key the identity names; `key` is the data set of it in use.
    std::optional<std::string> key; // The config key of a server's or a friend's world's data.
    std::filesystem::path worlds;   // The game's local worlds folder.
    bool hasAddress() const { return !local && port > 0 && !(address.empty() && ip.empty()); }
    // Named by its address through the server records, which "Use this data" changes.
    bool byAddress() const { return hasAddress() && !realm; }
} identity;
// The joined world's print decided for now and not answered yet (L-365): the tool screens ask.
std::optional<prints::Pick> question;
std::string questionName; // The data compared with, as shown; worked out when asked, not every frame.
bool questionToast = false;
std::filesystem::path configFolder() { return Runtime::instance().self().getConfigDir(); }
// Fuller data that may be the joined server's, per kind (SavedData.h); refreshed by suggest().
std::array<std::optional<SavedData>, 4> fullerFound;
void suggest();
// A map merge in progress (mergeData, stepMerge): the regions still to merge, from the other data's map folder
// into the one in use, which the store is kept off meanwhile (joined.mapFolder empty).
struct MapMerge {
    std::string key;
    std::filesystem::path from, into;
    struct File { std::string folder; RegionKey region; };
    std::vector<File> files;
    size_t next = 0;
    int chunks = 0, skipped = 0;
    MergeStart start;
    std::unique_ptr<RegionCodec> codec = std::make_unique<RegionCodec>();
    MergeProgress progress() const {
        return {next < files.size(), key, static_cast<int>(next), static_cast<int>(files.size()), chunks, skipped,
                start.waypoints, start.waypointsFull, start.placements, start.placementsFull, start.shapes, start.shapesFull};
    }
};
std::optional<MapMerge> mapMerge;
std::optional<MergeProgress> mergeFinished;
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
// Records a world's name and kind for the saved data list. The caller holds the lock.
void recordWorld(servers::List& list, std::string const& key, Identity const& who) {
    auto found = std::find_if(list.worlds.begin(), list.worlds.end(), [&](servers::World const& w) { return w.key == key; });
    if (found != list.worlds.end()) {
        found->name = who.levelName;
        found->realm = found->realm || who.realm;
    } else if (list.worlds.size() < servers::maxEntries) list.worlds.push_back({key, who.levelName, who.realm != 0});
}
// The key the joined identity names (L-343): a Realm by its id (L-358); a server by its address, through the record
// of which key each server was reached by; no address (a friend's world) by the level id, as a local world is.
std::optional<std::string> serverKey(Identity const& who) {
    if (who.realm) {
        std::string key = "realm_" + std::to_string(who.realm);
        withList([&](servers::List& list) { recordWorld(list, key, who); });
        return key;
    }
    if (!who.hasAddress()) {
        // Seen for a friend's world, whose level id is as distinct as a local world's. A world that sent a
        // plain name here would share its data with every world sending the same; the log tells which it was.
        auto key = servers::worldKey(who.levelId);
        log("joined without an address; its data goes by the level id '" + who.levelId + "'");
        if (key.empty()) return std::nullopt;
        withList([&](servers::List& list) { recordWorld(list, key, who); });
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
// How a key reads in the notice: a world's name, else the server data's.
std::string shownKey(servers::List const& list, std::string const& key) {
    auto world = std::find_if(list.worlds.begin(), list.worlds.end(), [&](servers::World const& w) { return w.key == key; });
    return world != list.worlds.end() && !world->name.empty() ? world->name : servers::dataName(list, key);
}
// The data set of the joined identity for the world joined (L-365): `who.base` is set to the identity's key.
std::optional<std::string> dataKey(Identity& who) {
    auto base = serverKey(who);
    if (!base) return std::nullopt;
    who.base = *base;
    auto config = configFolder();
    prints::Pick pick{*base};
    withList([&](servers::List& list) {
        pick = prints::pick(list.prints, list.unsplit, *base, who.print, [&](std::string const& k) { return anyData(config, k); });
        if (!who.byAddress() && pick.key != *base && prints::ownSet(*base, pick.key)) recordWorld(list, pick.key, who);
        if (pick.asked) questionName = shownKey(list, pick.other);
    });
    question = pick.asked ? std::optional(pick) : std::nullopt;
    if (pick.asked)
        log("this world differs from its saved data (" + std::to_string(pick.diff) + "); "
            + (pick.fresh ? "new data under '" + pick.key + "' for now" : "the data under '" + pick.key + "' for now") + ", asked");
    return pick.key;
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
    mapMerge.reset(); // What was merged is saved; merging again finishes it.
    mergeFinished.reset();
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
    try { joined.shapeFile = resolve(identity, "shapes.json", shapeFolder, true); }
    catch (std::exception const& error) { log(std::string("no file for shapes: ") + error.what()); }
    try { joined.wantedFile = resolve(identity, "wanted.json", wantedFolder, true); }
    catch (std::exception const& error) { log(std::string("no file for wanted items: ") + error.what()); }
    try { joined.placesFile = resolve(identity, "places.json", placesFolder, true); }
    catch (std::exception const& error) { log(std::string("no file for counted places: ") + error.what()); }
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
        question.reset();
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
            try {
                auto cookie = realm::read(event.self().getMinecraftGame_DEPRECATED());
                // How a released build is checked with a Realm nobody here has (L-358).
                log("in a Realm: " + std::string(cookie.inRealm ? "yes" : "no") + "; reconnect cookie kind "
                    + std::to_string(cookie.kind) + ", Realm id " + std::to_string(cookie.id));
                if (cookie.inRealm && cookie.id > 0) identity.realm = cookie.id;
            } catch (...) {}
            try {
                identity.print.seed = static_cast<std::uint64_t>(level.getLevelSeed64().mValue);
                identity.print.name = identity.levelName;
                identity.print.mode = static_cast<int>(level.getDefaultGameType());
                identity.print.hardcore = level.isHardcore();
            } catch (...) {}
            // Whether the world can be told from another behind the same key (L-365): a hidden seed cannot.
            log("world: seed " + std::string(identity.print.seed ? "known" : "hidden") + ", game mode "
                + std::to_string(identity.print.mode) + (identity.print.hardcore ? ", hardcore" : ""));
            questionToast = false;
            try { identity.key = dataKey(identity); questionToast = question.has_value(); }
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
    question.reset();
    questionToast = false;
    mapMerge.reset();
    mergeFinished.reset();
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
    stepMerge(); // A map merge goes on while playing too (L-343).
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
// One data's five kinds, given where each lies. Bounded: the map walk stops at 2000 files.
void measure(SavedData& item, std::filesystem::path const& waypointFile, std::filesystem::path const& schematicFile,
             std::filesystem::path const& deathFile, std::filesystem::path const& map, std::filesystem::path const& shapeFile) {
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
    if (std::filesystem::is_regular_file(shapeFile, ignored)) {
        item.used = std::max(item.used, writtenAt(shapeFile));
        try { item.shapes = static_cast<int>(overlay::readShapes(shapeFile).size()); } catch (...) {}
    }
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
            config / deathFolder / (key + ".json"), config / mapFolder / key, config / shapeFolder / (key + ".json"));
    return item;
}
// Works out fullerFound for the joined server. Reads a few files: at a join and after a change only.
void suggest() {
    fullerFound = {};
    if (!joined.active || !identity.byAddress() || !identity.key) return;
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
        amounts.push_back({others.back().waypoints, others.back().placements, others.back().mapBytes, others.back().shapes});
    }
    auto best = servers::fuller({mine.waypoints, mine.placements, mine.mapBytes, mine.shapes}, amounts);
    for (auto [index, slot] : {std::pair{best.waypoints, 0}, std::pair{best.placements, 1}, std::pair{best.map, 2},
                               std::pair{best.shapes, 3}})
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
    if (!identity.byAddress() || !identity.key) return;
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
    // Every key any of the folders holds: data with only wanted items or counted places can be deleted too.
    std::set<std::string> keys;
    for (auto folder : {mapFolder, waypointFolder, schematicFolder, deathFolder, shapeFolder, wantedFolder, placesFolder}) {
        std::filesystem::directory_iterator it(config / folder, ignored), end;
        for (; !ignored && it != end && keys.size() < savedDataLimit; it.increment(ignored)) {
            auto name = it->path().filename().u8string();
            std::string key(name.begin(), name.end());
            if (folder != mapFolder) {
                if (!key.ends_with(".json")) continue;
                key.resize(key.size() - 5);
                // The counted places every server once shared, left unread (PLAN.md "Limits"): no data set.
                if (folder == wantedFolder && key == "places") continue;
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
        auto world = std::find_if(list.worlds.begin(), list.worlds.end(), [&](servers::World const& w) { return w.key == key; });
        item.kind = world != list.worlds.end() ? (world->realm ? SavedData::Kind::Realm : SavedData::Kind::Friend)
                  : key.starts_with("world_") ? SavedData::Kind::Friend : SavedData::Kind::Server;
        if (item.kind != SavedData::Kind::Server) {
            if (world != list.worlds.end()) item.name = world->name;
        } else {
            item.name = servers::dataName(list, key);
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
                config / deathFolder / (key + ".json"), config / mapFolder / key, config / shapeFolder / (key + ".json"));
        item.inUse = !current.empty() && key == current;
        // A server takes another server's data (L-343) or another data set of its own (L-365); a Realm or a
        // friend's world any data but a local world's.
        item.canUse = !item.inUse && joined.active && !identity.local && identity.key
            && (!identity.byAddress() || item.kind == SavedData::Kind::Server);
        item.canDelete = !item.inUse;
        item.splittable = item.kind != SavedData::Kind::Local;
        auto const base = prints::baseOf(list.prints, key);
        item.split = std::find(list.unsplit.begin(), list.unsplit.end(), base) == list.unsplit.end();
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
            measure(item, folder / "waypoints.json", folder / "schematics.json", folder / "death-layout.json", folder / "map",
                    folder / "shapes.json");
            if (!item.waypoints && !item.placements && !item.shapes && !item.mapBytes && !item.used) continue;
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
namespace {
// The joined world uses `key` from now on. The caller holds the lock.
bool switchTo(std::string const& key) {
    question.reset();
    if (identity.key && *identity.key == key) {
        try { suggest(); } catch (...) { fullerFound = {}; }
        return true;
    }
    log("this world now uses the data under '" + key + "'");
    identity.key = key;
    attach();
    return true;
}
}
bool useData(std::string const& key) {
    std::lock_guard lock(mutex);
    if (!joined.active || identity.local || !identity.key || !safeKey(key)) return false;
    if (*identity.key == key && !question) return true;
    bool done = false;
    bool const saved = withList([&](servers::List& list) {
        bool const own = key == identity.base || std::any_of(list.prints.begin(), list.prints.end(),
            [&](prints::Row const& r) { return r.base == identity.base && r.key == key; });
        if (identity.byAddress() && !own) {
            // Another server's data: this server's record takes it (L-343), and the data's own key names the server.
            auto world = std::find_if(list.worlds.begin(), list.worlds.end(), [&](servers::World const& w) { return w.key == key; });
            if (key.starts_with("world_") || world != list.worlds.end()) return;
            if (!servers::assign(list.servers, identity.address, identity.ip, identity.port, identity.levelId, key)) return;
            identity.base = key;
        }
        done = !prints::choose(list.prints, list.unsplit, identity.base, identity.print, key).empty();
    });
    if (!done || !saved) return false;
    return switchTo(key);
}
std::optional<WorldQuestion> worldQuestion() {
    std::lock_guard lock(mutex);
    if (!question || !joined.active) return std::nullopt;
    return WorldQuestion{question->fresh, question->other, questionName, question->diff};
}
bool takeWorldToast() {
    std::lock_guard lock(mutex);
    return std::exchange(questionToast, false) && question.has_value();
}
bool setDataSplit(std::string const& key, bool on) {
    std::lock_guard lock(mutex);
    if (!safeKey(key)) return false;
    std::string base;
    bool const saved = withList([&](servers::List& list) {
        base = prints::baseOf(list.prints, key);
        if (on) prints::split(list.unsplit, base);
        else prints::noSplit(list.unsplit, list.prints, base);
    });
    if (!saved) return false;
    log("worlds behind '" + base + "' " + (on ? "are told apart" : "use one data set"));
    // As the notice's answer does, at once for the joined world: the join's choice again, now with the switch.
    if (joined.active && !identity.local && identity.base == base) {
        auto now = dataKey(identity);
        if (now && now != identity.key) {
            log("this world now uses the data under '" + *now + "'");
            identity.key = now;
            attach();
        } else {
            try { suggest(); } catch (...) { fullerFound = {}; }
        }
    }
    return true;
}
bool answerWorld(WorldAnswer answer) {
    std::lock_guard lock(mutex);
    if (!question || !joined.active || identity.base.empty()) return false;
    auto config = configFolder();
    auto exists = [&](std::string const& k) { return anyData(config, k); };
    std::string key;
    bool const saved = withList([&](servers::List& list) {
        switch (answer) {
        case WorldAnswer::Keep: key = prints::keep(list.prints, identity.base, identity.print); break;
        case WorldAnswer::New: key = prints::startNew(list.prints, identity.base, identity.print, exists); break;
        case WorldAnswer::UseOther: key = prints::use(list.prints, identity.base, identity.print, question->other); break;
        case WorldAnswer::NoSplit: key = prints::noSplit(list.unsplit, list.prints, identity.base); break;
        }
        if (!key.empty() && !identity.byAddress() && key != identity.base && prints::ownSet(identity.base, key)) recordWorld(list, key, identity);
    });
    if (key.empty() || !saved || !safeKey(key)) return false;
    return switchTo(key);
}
bool removeData(SavedData const& item) {
    std::lock_guard lock(mutex);
    // The map store may still hold the folder (the server was given other data and no frame has followed):
    // it saves and lets go first, or its worker would write the folder back; while it cannot, nothing is
    // deleted. A missing file is no error; what could not be deleted is, and the data stays listed.
    auto remove = [](std::vector<std::filesystem::path> const& files, std::optional<std::filesystem::path> const& map) {
        if (map && !store::release(*map)) { log("the map under " + map->string() + " is still being saved"); return false; }
        bool done = true;
        auto check = [&](std::filesystem::path const& what, std::error_code const& error) {
            if (!error) return;
            done = false;
            log("could not delete " + what.string() + ": " + error.message());
        };
        std::error_code error;
        for (auto const& file : files) { std::filesystem::remove(file, error); check(file, error); }
        if (map) { std::filesystem::remove_all(*map, error); check(*map, error); }
        return done;
    };
    if (item.kind == SavedData::Kind::Local) {
        if (identity.local && identity.levelId == item.key) return false;
        try {
            // localWorldFile checks that the folder is a world of the game's own worlds folder.
            std::vector<std::filesystem::path> files;
            for (std::string_view name : {"waypoints.json", "schematics.json", "death-layout.json", "shapes.json", "wanted.json",
                                           "places.json"})
                if (auto file = overlay::localWorldFile(identity.worlds, item.key, name)) files.push_back(*file);
            if (!remove(files, overlay::localWorldFile(identity.worlds, item.key, "map"))) return false;
        } catch (std::exception const& error) { log(std::string("could not delete a world's data: ") + error.what()); return false; }
        log("deleted the saved data of the local world '" + item.key + "'");
        return true;
    }
    if (!safeKey(item.key) || (!identity.local && identity.key && *identity.key == item.key)) return false;
    if (mapMerge && mapMerge->key == item.key) return false;
    bool removed = false;
    try {
        auto config = configFolder();
        std::vector<std::filesystem::path> files;
        for (auto folder : {waypointFolder, schematicFolder, deathFolder, shapeFolder, wantedFolder, placesFolder})
            files.push_back(config / folder / (item.key + ".json"));
        // The same path as resolve() gives the store.
        removed = remove(files, config / std::filesystem::u8path(mapFolder) / std::filesystem::path(item.key));
    } catch (std::exception const& error) { log(std::string("could not delete saved data: ") + error.what()); }
    if (removed) {
        withList([&](servers::List& list) { servers::forget(list, item.key); });
        if (question && question->other == item.key) question.reset(); // Nothing left to use instead.
        log("deleted the saved data under '" + item.key + "'");
    }
    // Also after a part of it went: the notice of fuller data counts what is left.
    try { suggest(); } catch (...) { fullerFound = {}; }
    return removed;
}
int setDataAnyPort(std::string const& key, bool on) {
    std::lock_guard lock(mutex);
    int released = -1;
    if (!withList([&](servers::List& list) { released = servers::setAnyPort(list.servers, key, on); }) || released < 0) return -1;
    // The joined server may be one of the ports that now follow the switch.
    if (joined.active && identity.byAddress()) {
        auto now = dataKey(identity);
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
namespace {
// The regions of a map folder, once each (a region saved as .lm2 and .lmr is one), in layers the store knows.
std::vector<MapMerge::File> regionFiles(std::filesystem::path const& folder) {
    std::vector<MapMerge::File> files;
    std::set<std::pair<std::string, std::pair<int, int>>> seen;
    std::error_code ignored;
    std::filesystem::recursive_directory_iterator it(folder, ignored), end;
    for (; !ignored && it != end; it.increment(ignored)) {
        if (!it->is_regular_file(ignored)) continue;
        auto name = it->path().filename().u8string();
        auto region = regionOfFileName(std::string_view(reinterpret_cast<char const*>(name.data()), name.size()));
        if (!region) continue;
        auto relative = it->path().parent_path().lexically_relative(folder).generic_u8string();
        std::string layer(relative.begin(), relative.end());
        if (!layerOfFolder(layer)) continue;
        if (seen.insert({layer, {region->x, region->z}}).second) files.push_back({layer, *region});
    }
    return files;
}
}
MergeStart mergeData(std::string const& key) {
    std::lock_guard lock(mutex);
    MergeStart start;
    if (!joined.active || identity.local || !identity.key || !safeKey(key) || *identity.key == key || loadFailed || mapMerge)
        return start;
    auto config = configFolder();
    std::error_code ignored;
    auto const waypointFile = config / waypointFolder / (key + ".json");
    auto const schematicFile = config / schematicFolder / (key + ".json");
    auto const shapeFile = config / shapeFolder / (key + ".json");
    // Everything is read before anything is written: a file that cannot be read stops the whole merge.
    WaypointSet incoming;
    schematic::PlacementSet placementsIn, placementsInUse;
    std::vector<overlay::ShapeDefinition> shapesIn, shapesInUse;
    try {
        if (std::filesystem::is_regular_file(shapeFile, ignored)) shapesIn = overlay::readShapes(shapeFile);
        if (joined.shapeFile && std::filesystem::is_regular_file(*joined.shapeFile, ignored))
            shapesInUse = overlay::readShapes(*joined.shapeFile);
        if (std::filesystem::is_regular_file(waypointFile, ignored)) incoming = readWaypoints(waypointFile);
        if (std::filesystem::is_regular_file(schematicFile, ignored)) placementsIn = schematic::readPlacements(schematicFile);
        if (joined.schematicFile && std::filesystem::is_regular_file(*joined.schematicFile, ignored))
            placementsInUse = schematic::readPlacements(*joined.schematicFile);
    } catch (std::exception const& error) {
        log(std::string("could not read data to merge: ") + error.what());
        return start;
    }
    auto candidate = set;
    auto waypointCounts = merge::mergeWaypoints(candidate, incoming.waypoints);
    auto placementCounts = merge::mergePlacements(placementsInUse, placementsIn.placements);
    auto shapeCounts = merge::mergeShapes(shapesInUse, shapesIn);
    if (waypointCounts.taken && !commit(std::move(candidate))) return start;
    if (placementCounts.taken) {
        if (!joined.schematicFile) return start;
        try {
            schematic::normalize(placementsInUse);
            schematic::writePlacements(*joined.schematicFile, placementsInUse);
        } catch (std::exception const& error) {
            log(std::string("could not save merged placements: ") + error.what());
            return start;
        }
    }
    if (shapeCounts.taken) {
        if (!joined.shapeFile) return start;
        try { overlay::writeShapes(*joined.shapeFile, shapesInUse); }
        catch (std::exception const& error) {
            log(std::string("could not save merged shapes: ") + error.what());
            return start;
        }
    }
    start = {true, waypointCounts.taken, waypointCounts.full, placementCounts.taken, placementCounts.full,
             shapeCounts.taken, shapeCounts.full, 0};
    bool moved = placementCounts.taken > 0 || shapeCounts.taken > 0; // Followers reload what changed under them.
    // The map: the store saves and lets go of both folders, and records to session files until the merge ends.
    auto const from = config / std::filesystem::u8path(mapFolder) / std::filesystem::path(key);
    if (joined.mapFolder && std::filesystem::is_directory(from, ignored)) {
        if (!store::release(*joined.mapFolder) || !store::release(from)) {
            log("the map is still being saved; it was not merged");
        } else {
            MapMerge job;
            job.key = key;
            job.from = from;
            job.into = *joined.mapFolder;
            job.files = regionFiles(from);
            start.regions = static_cast<int>(job.files.size());
            job.start = start;
            moved = true; // The store let go: it attaches again, to session files while the merge runs.
            if (!job.files.empty()) {
                mapMerge = std::move(job);
                mergeFinished.reset();
                joined.mapFolder.reset();
            }
        }
    }
    if (moved) ++joined.world;
    log("merged the data under '" + key + "': " + std::to_string(start.waypoints) + " waypoints, "
        + std::to_string(start.placements) + " placements, " + std::to_string(start.shapes) + " shapes, "
        + std::to_string(start.regions) + " map regions to go");
    try { suggest(); } catch (...) { fullerFound = {}; }
    return start;
}
void stepMerge(double budgetMs) {
    std::lock_guard lock(mutex);
    if (!mapMerge) return;
    auto& job = *mapMerge;
    auto const until = std::chrono::steady_clock::now() + std::chrono::duration<double, std::milli>(budgetMs);
    while (job.next < job.files.size() && std::chrono::steady_clock::now() < until) {
        auto const& file = job.files[job.next++];
        auto const relative = std::filesystem::u8path(file.folder) / regionFileName(file.region);
        try {
            auto source = readRegionFiles(job.from / relative, file.region, *job.codec);
            if (source.status != RegionStatus::Loaded || source.damaged || !source.data) { ++job.skipped; continue; }
            auto target = readRegionFiles(job.into / relative, file.region, *job.codec);
            if (target.status == RegionStatus::Refused || target.damaged) { ++job.skipped; continue; }
            RegionData data = target.data ? std::move(*target.data) : RegionData{};
            int const changed = merge::mergeRegion(data, *source.data);
            if (!changed && !target.needsSave) continue;
            std::filesystem::create_directories((job.into / relative).parent_path());
            writeRegionFiles(job.into / relative, file.region, data, *job.codec);
            job.chunks += changed;
        } catch (std::exception const& error) {
            ++job.skipped;
            log("could not merge map region " + relative.string() + ": " + error.what());
        }
    }
    if (job.next >= job.files.size()) {
        mergeFinished = job.progress();
        joined.mapFolder = job.into;
        ++joined.world;
        log("merged the map under '" + job.key + "': " + std::to_string(job.chunks) + " chunks, "
            + std::to_string(job.skipped) + " regions skipped");
        mapMerge.reset();
        try { suggest(); } catch (...) { fullerFound = {}; }
    }
}
MergeProgress mergeProgress() {
    std::lock_guard lock(mutex);
    return mapMerge ? mapMerge->progress() : MergeProgress{};
}
std::optional<MergeProgress> takeFinishedMerge() {
    std::lock_guard lock(mutex);
    return std::exchange(mergeFinished, std::nullopt);
}
std::string dataName() {
    std::lock_guard lock(mutex);
    if (!joined.active || identity.local || !identity.key) return {};
    if (!identity.byAddress()) return identity.levelName;
    return servers::shownName(identity.base.empty() ? *identity.key : identity.base);
}
}
