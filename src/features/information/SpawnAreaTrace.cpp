#include "features/information/SpawnAreaTrace.h"
#ifdef LAMIUM_SPAWN_AREA_TRACE
#include "app/TraceLog.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/world/ClientLevelTickEvent.h"
#include "ll/api/service/TargetedBedrock.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/chunk/LevelChunk.h"
#include <cmath>
#include <format>
#include <string>

namespace lamium::information::spawnAreaTrace {
namespace {
// Reads only: the spawn areas each loaded chunk around the player carries
// (`LevelChunk::mSpawningAreas`), every 5 s, logged when the picture changes.
// A local world fills them from its own generator; whether a server sends
// them is the question.
constexpr int reach = 8; // chunks each way
TraceBudget summaryBudget, areaBudget, failBudget;
ll::event::ListenerPtr tickListener;
unsigned ticks = 0;
std::string lastSummary;
int chunkOf(double block) { return static_cast<int>(std::floor(block / 16.0)); }
void probe(LocalPlayer& player) {
    auto& region = player.getDimensionBlockSource();
    auto const& feet = player.getPosition();
    int px = chunkOf(feet.x), pz = chunkOf(feet.z);
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
    auto summary = std::format("dimension {} around chunk {} {}: loaded {} chunks, {} with spawn areas, {} areas",
        static_cast<int>(player.getDimensionId()), px, pz, loaded, withAreas, areas);
    if (summary == lastSummary) return;
    lastSummary = summary;
    traceLog(summaryBudget, 200, "#32 spawn areas: {}", summary);
    // Types: 1 nether fortress, 2 witch hut, 3 ocean monument, 5 pillager outpost.
    if (!found.empty()) traceLog(areaBudget, 100, "#32 spawn areas found:{}", found.size() > 3000 ? found.substr(0, 3000) + " ..." : found);
}
void tick() noexcept {
    try {
        auto client = ll::service::getClientInstance();
        auto* player = client ? client->getLocalPlayer() : nullptr;
        if (!player || ++ticks % 100) return;
        probe(*player);
    } catch (std::exception const& error) {
        traceLog(failBudget, 20, "#32 spawn area probe failed: {}", error.what());
    } catch (...) {}
}
}
void start() {
    tickListener = ll::event::EventBus::getInstance().emplaceListener<ll::event::ClientLevelTickEvent>([](auto&) { tick(); });
    Runtime::instance().self().getLogger().warn("#32 spawn area diagnostics enabled");
}
void stop() {
    if (tickListener) { ll::event::EventBus::getInstance().removeListener(tickListener); tickListener.reset(); }
    lastSummary.clear();
}
}
#else
namespace lamium::information::spawnAreaTrace { void start() {} void stop() {} }
#endif
