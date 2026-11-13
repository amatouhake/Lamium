#include "features/information/SpawnAreaTrace.h"
#ifdef LAMIUM_SPAWN_AREA_TRACE
#include "app/TraceLog.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/world/ClientLevelTickEvent.h"
#include "ll/api/event/world/ServerLevelTickEvent.h"
#include "ll/api/service/TargetedBedrock.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/chunk/LevelChunk.h"
#include "mc/world/level/dimension/Dimension.h"
#include <atomic>
#include <cmath>
#include <format>
#include <string>

namespace lamium::information::spawnAreaTrace {
namespace {
// Reads only: the spawn areas each loaded chunk around the player carries
// (`LevelChunk::mSpawningAreas`), every 5 s, logged when the picture changes.
// Two sides. "client": the chunks the client itself holds, in a local world
// and on a server alike; whether those carry the areas is the question.
// "server": in a local world the game's own server runs in this process and
// its chunks come from the generator, so they show that the read is right
// (it runs on that server's tick; there is none when joined to a server).
constexpr int reach = 8; // chunks each way
struct Side {
    char const* name;
    TraceBudget summaryBudget, areaBudget, failBudget;
    unsigned ticks = 0;
    std::string lastSummary;
};
Side clientSide{"client"}, serverSide{"server"};
ll::event::ListenerPtr clientListener, serverListener;
// Where the local player is, for the server's tick.
std::atomic<bool> known{false};
std::atomic<int> playerChunkX{0}, playerChunkZ{0}, playerDimension{0};
int chunkOf(double block) { return static_cast<int>(std::floor(block / 16.0)); }
void probe(Side& side, BlockSource& region, int dimension, int px, int pz) {
    int loaded = 0, withAreas = 0, areas = 0;
    std::string found;
    for (int dz = -reach; dz <= reach; ++dz)
        for (int dx = -reach; dx <= reach; ++dx) {
            int cx = px + dx, cz = pz + dz;
            auto const* chunk = region.getChunkAt(BlockPos{cx * 16 + 8, 0, cz * 16 + 8});
            if (!chunk) continue;
            ++loaded;
            auto const& list = *chunk->mSpawningAreas;
            // A list too long to be real means the member is not where this build expects it.
            if (list.empty() || list.size() > 64) {
                if (list.size() > 64) found += std::format(" chunk({},{}):unreadable({})", cx, cz, list.size());
                continue;
            }
            ++withAreas;
            for (auto const& area : list) {
                ++areas;
                auto const& box = *area.aabb;
                found += std::format(" chunk({},{}):type{}[{},{},{} .. {},{},{}]", cx, cz, static_cast<int>(area.type), box.min.x,
                    box.min.y, box.min.z, box.max.x, box.max.y, box.max.z);
            }
        }
    auto summary = std::format("dimension {} around chunk {} {}: loaded {} chunks, {} with spawn areas, {} areas", dimension, px, pz,
        loaded, withAreas, areas);
    if (summary == side.lastSummary) return;
    side.lastSummary = summary;
    traceLog(side.summaryBudget, 200, "#32 spawn areas ({}): {}", side.name, summary);
    // Types: 1 nether fortress, 2 witch hut, 3 ocean monument, 5 pillager outpost.
    if (!found.empty())
        traceLog(side.areaBudget, 100, "#32 spawn areas ({}) found:{}", side.name, found.size() > 3000 ? found.substr(0, 3000) + " ..." : found);
}
void clientTick() noexcept {
    try {
        auto client = ll::service::getClientInstance();
        auto* player = client ? client->getLocalPlayer() : nullptr;
        if (!player) { known = false; return; }
        auto const& feet = player->getPosition();
        int px = chunkOf(feet.x), pz = chunkOf(feet.z), dimension = static_cast<int>(player->getDimensionId());
        playerChunkX = px;
        playerChunkZ = pz;
        playerDimension = dimension;
        known = true;
        if (++clientSide.ticks % 100) return;
        probe(clientSide, player->getDimensionBlockSource(), dimension, px, pz);
    } catch (std::exception const& error) {
        traceLog(clientSide.failBudget, 20, "#32 spawn area probe (client) failed: {}", error.what());
    } catch (...) {}
}
void serverTick(Level& level) noexcept {
    try {
        if (!known.load() || ++serverSide.ticks % 100) return;
        int dimension = playerDimension.load();
        auto ref = level.getDimension(DimensionType{dimension}).lock();
        if (!ref) return;
        probe(serverSide, ref->getBlockSourceFromMainChunkSource(), dimension, playerChunkX.load(), playerChunkZ.load());
    } catch (std::exception const& error) {
        traceLog(serverSide.failBudget, 20, "#32 spawn area probe (server) failed: {}", error.what());
    } catch (...) {}
}
}
void start() {
    auto& bus = ll::event::EventBus::getInstance();
    clientListener = bus.emplaceListener<ll::event::ClientLevelTickEvent>([](auto&) { clientTick(); });
    serverListener = bus.emplaceListener<ll::event::ServerLevelTickEvent>([](auto& event) { serverTick(event.level()); });
    Runtime::instance().self().getLogger().warn("#32 spawn area diagnostics enabled");
}
void stop() {
    for (auto* listener : {&clientListener, &serverListener})
        if (*listener) { ll::event::EventBus::getInstance().removeListener(*listener); listener->reset(); }
    known = false;
    clientSide.lastSummary.clear();
    serverSide.lastSummary.clear();
}
}
#else
namespace lamium::information::spawnAreaTrace { void start() {} void stop() {} }
#endif
