#include "features/map/WaypointSession.h"
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
#include <atomic>
#include <cmath>
#include <mutex>

namespace lamium::map::waypoints {
namespace {
std::mutex mutex;
WaypointSet set;
std::optional<std::filesystem::path> destination;
Place joined;
bool loadFailed = false; // Never overwrite a file that could not be read.
std::optional<DeathPoint> pendingDeath;
DeathWatch deathWatch;
std::atomic<bool> joiningLocal{false};
std::uint64_t nextId = 1; // never reset: ids are not reused within the session
ll::event::ListenerPtr startJoinListener, joinListener, exitListener;

void log(std::string const& text) {
    try { Runtime::instance().self().getLogger().info("Waypoints: {}", text); } catch (...) {}
}
// Where this world's data lives: a local world's Lamium folder, or for a
// server a file or folder named after its address in Lamium's config folder.
std::optional<std::filesystem::path> resolve(ll::event::ClientJoinLevelEvent& event, std::string_view localName,
                                             std::string_view serverFolder, bool serverFile) {
    auto& client = event.self();
    if (joiningLocal) {
        auto paths = client.getMinecraftGame_DEPRECATED().getFilePathManager();
        return overlay::localWorldFile(std::filesystem::u8path(paths->mWorlds->value),
                                       event.player().getLevel().getLevelId(), localName);
    }
    auto connection = client.getGameConnectionInfo();
    if (!connection) return std::nullopt;
    std::string host = connection->mUnresolvedUrl->empty() ? *connection->mHostIpAddress : *connection->mUnresolvedUrl;
    auto file = serverFileName(host, connection->mPort);
    if (file.empty()) return std::nullopt;
    if (!serverFile) file.resize(file.size() - std::string_view(".json").size());
    return Runtime::instance().self().getConfigDir() / std::filesystem::u8path(serverFolder) / std::filesystem::path(file);
}
void join(ll::event::ClientJoinLevelEvent& event) noexcept {
    try {
        if (event.self().getLocalPlayer() != &event.player()) return;
        std::lock_guard lock(mutex);
        set = {};
        destination.reset();
        loadFailed = false;
        pendingDeath.reset();
        deathWatch.reset();
        joined = {joined.world + 1, true, std::nullopt};
        try { joined.mapFolder = resolve(event, "map", "map", false); }
        catch (std::exception const& error) { log(std::string("no folder for the world map: ") + error.what()); }
        try { joined.schematicFile = resolve(event, "schematics.json", "schematics", true); }
        catch (std::exception const& error) { log(std::string("no file for schematic placements: ") + error.what()); }
        try { joined.deathLayoutFile = resolve(event, "death-layout.json", "death-layouts", true); }
        catch (std::exception const& error) { log(std::string("no file for the death layout: ") + error.what()); }
        try {
            destination = resolve(event, "waypoints.json", "waypoints", true);
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
    pendingDeath.reset();
    deathWatch.reset();
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
std::uint64_t add(Waypoint waypoint) {
    std::lock_guard lock(mutex);
    if (set.waypoints.size() >= maxWaypoints) return 0;
    auto candidate = set;
    waypoint.color = clampColor(waypoint.color);
    if (waypoint.name.size() > maxNameBytes) waypoint.name.resize(maxNameBytes);
    waypoint.id = 0;
    candidate.lastColor = waypoint.color;
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
void frame(bool recordDeath) {
    std::lock_guard lock(mutex);
    if (!pendingDeath) return;
    auto death = *std::exchange(pendingDeath, std::nullopt);
    if (!recordDeath) return;
    auto candidate = set;
    candidate.death = death;
    commit(std::move(candidate));
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
