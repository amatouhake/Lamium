#include "features/research/WorldProbe.h"
#ifdef LAMIUM_WORLD_PROBE
#include "app/Runtime.h"
#include "ll/api/memory/Hook.h"
#include "mc/client/game/IClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/client/renderer/game/LevelRendererPlayer.h"
#include "mc/deps/ecs/gamerefs_entity/EntityContext.h"
#include "mc/entity/components/AgeableComponent.h"
#include "mc/entity/components_json_legacy/BreedableComponent.h"
#include "mc/world/actor/Actor.h"
#include "mc/world/actor/monster/EnderDragon.h"
#include "mc/world/level/Level.h"
#include "mc/world/phys/AABB.h"
#include <chrono>
#include <format>
#include <stdexcept>
#include <utility>

// What the client can read, every 5 s and bounded; nothing is written:
// L-249 the client level's chunk tick range, L-33 the age and breeding
// components of nearby actors, L-30 the Ender Dragon's part boxes.
namespace lamium::researchTrace::worldProbe {
namespace {
using Clock = std::chrono::steady_clock;
template <class... Args>
void log(std::format_string<Args...> format, Args&&... args) noexcept {
    try { Runtime::instance().self().getLogger().info("{}", std::format(format, std::forward<Args>(args)...)); }
    catch (...) {}
}
Clock::time_point last = Clock::now() - std::chrono::hours(1);
unsigned lines = 0;
void probe(LocalPlayer& player) {
    if (Clock::now() - last < std::chrono::seconds(5) || lines >= 4000) return;
    last = Clock::now();
    auto& level = player.getLevel();
    ++lines;
    log("probe L-249 chunkTickRange={} dimension={}", level.getChunkTickRange(), static_cast<int>(player.getDimensionId()));
    Vec3 const here = player.getPosition();
    int listed = 0;
    for (auto* actor : level.getRuntimeActorList()) {
        if (!actor || actor == &player) continue;
        Vec3 const at = actor->getPosition();
        float const dx = at.x - here.x, dy = at.y - here.y, dz = at.z - here.z;
        bool const dragon = actor->getTypeName() == "minecraft:ender_dragon";
        // Fish and dropped items crowd out the actors worth reading.
        auto age = actor->getEntityContext().tryGetComponent<AgeableComponent>();
        auto breed = actor->getEntityContext().tryGetComponent<BreedableComponent>();
        bool const plain = !dragon && !actor->isBaby() && !age && !breed;
        if (!dragon && (dx * dx + dy * dy + dz * dz > 24 * 24 || (plain && listed >= 4))) continue;
        if (plain) ++listed;
        ++lines;
        log("probe L-33 actor={} baby={} ageable={} age={} breedable={} loveTimer={} breedCooldown={}",
            actor->getTypeName(), actor->isBaby(), static_cast<bool>(age), age ? static_cast<int>(age->mAge) : 0,
            static_cast<bool>(breed), breed ? static_cast<int>(breed->mLoveTimer) : 0,
            breed ? static_cast<int>(breed->mBreedCooldown) : 0);
        if (!dragon) continue;
        auto& parts = static_cast<EnderDragon&>(*actor);
        std::pair<char const*, AABB*> const boxes[]{{"head", parts.mHead}, {"neck", parts.mNeck}, {"body", parts.mBody},
            {"tail1", parts.mTail1}, {"tail2", parts.mTail2}, {"tail3", parts.mTail3}, {"wing1", parts.mWing1},
            {"wing2", parts.mWing2}};
        auto const& whole = actor->getAABB();
        ++lines;
        log("probe L-30 dragon pos={:.1f},{:.1f},{:.1f} aabb={:.1f},{:.1f},{:.1f}..{:.1f},{:.1f},{:.1f}", at.x, at.y, at.z,
            whole.min.x, whole.min.y, whole.min.z, whole.max.x, whole.max.y, whole.max.z);
        for (auto const& [name, box] : boxes) {
            ++lines;
            if (!box) { log("probe L-30 part={} null", name); continue; }
            log("probe L-30 part={} {:.1f},{:.1f},{:.1f}..{:.1f},{:.1f},{:.1f}", name,
                box->min.x, box->min.y, box->min.z, box->max.x, box->max.y, box->max.z);
        }
    }
}
LL_TYPE_INSTANCE_HOOK(ProbeHook, ll::memory::HookPriority::Low, LevelRendererPlayer,
    &LevelRendererPlayer::getFov, float, float alpha, bool variable) {
    float result = origin(alpha, variable);
    try {
        if (auto* player = static_cast<IClientInstance&>(mClientInstance).getLocalPlayer()) probe(*player);
    } catch (...) {}
    return result;
}
bool installed = false;
}
void start() {
    installed = ProbeHook::hook(true) == 0;
    if (!installed) throw std::runtime_error("Could not install the world probe");
    Runtime::instance().self().getLogger().warn("World probe enabled (L-249, L-33, L-30; read only)");
}
void stop() {
    if (installed && ProbeHook::unhook(true)) installed = false;
}
}
#else
namespace lamium::researchTrace::worldProbe { void start() {} void stop() {} }
#endif
