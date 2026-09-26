#include "features/research/ResearchTrace.h"
#ifdef LAMIUM_RESEARCH_TRACE
#include "app/Runtime.h"
#include "features/camera/Zoom.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/service/TargetedBedrock.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/game/IClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/client/renderer/game/LevelRendererPlayer.h"
#include "mc/deps/ecs/gamerefs_entity/EntityContext.h"
#include "mc/deps/minecraft_renderer/game/LevelCullerType.h"
#include "mc/client/entity/systems/ClientInputUpdateSystem.h"
#include "mc/deps/core/math/Vec2.h"
#include "mc/entity/components/MoveInputComponent.h"
#include "mc/entity/components/RawMoveInputComponent.h"
#include "mc/entity/components/SneakingComponent.h"
#include "mc/entity/systems/move_collision_system/MoveCollisionSystem.h"
#include "mc/deps/vanilla_components/MoveRequestComponent.h"
#include "mc/deps/ecs/gamerefs_entity/EntityRegistry.h"
#include "mc/deps/ecs/strict/StrictEntityContext.h"
#include "mc/world/actor/provider/PlayerMovement.h"
#include "mc/world/phys/AABB.h"
#include "mc/world/level/GameType.h"
#include <Windows.h>
#pragma comment(lib, "user32.lib") // GetAsyncKeyState, trace builds only
#include <intrin.h>
#include <atomic>
#include <chrono>
#include <cmath>
#include <format>
#include <map>
#include <mutex>
#include <stdexcept>
#include <tuple>

namespace lamium::researchTrace {
namespace {
using Clock = std::chrono::steady_clock;
template <class... Args>
void log(std::format_string<Args...> format, Args&&... args) noexcept {
    try { Runtime::instance().self().getLogger().info("{}", std::format(format, std::forward<Args>(args)...)); }
    catch (...) {} // Diagnostics must not replace the vanilla result.
}

// Call sites are grouped by return address (offset into the game module).
uintptr_t const moduleBase = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
uintptr_t callerOffset(void* address) { return reinterpret_cast<uintptr_t>(address) - moduleBase; }
LocalPlayer* localPlayer() {
    auto client = ll::service::getClientInstance();
    return client ? client->getLocalPlayer() : nullptr;
}
struct Toggle {
    int key;
    char const* name;
    std::atomic<bool> on{false};
    bool down = false;
    void poll() {
        bool now = (GetAsyncKeyState(key) & 0x8000) != 0;
        if (now && !down) {
            on = !on.load();
            log("research {} {}", name, on.load() ? "ON" : "OFF");
        }
        down = now;
    }
};

// L-40: MoveCollisionSystem::fetchCollisionShapes receives the entity's
// MoveRequestComponent, whose mSneaking flag likely drives vanilla edge
// protection. F9 sets it for the local player only, without sneak input.
Toggle fakeSneak{VK_F9, "L-40 move-request sneaking flag"};
std::atomic<unsigned> localRequests{0}, sneakingRequests{0}, forcedRequests{0};
Clock::time_point lastL40 = Clock::now();
void dumpL40() {
    if (Clock::now() - lastL40 < std::chrono::seconds(2)) return;
    lastL40 = Clock::now();
    unsigned local = localRequests.exchange(0), sneaking = sneakingRequests.exchange(0), forced = forcedRequests.exchange(0);
    if (!local) return;
    auto* player = localPlayer();
    log("research L-40 per2s localMoveRequests={} vanillaSneaking={} forced={} fake={} sneakingFlag={}",
        local, sneaking, forced, fakeSneak.on.load(), player ? player->isSneaking() : false);
}
bool isLocalEntity(StrictEntityContext const& entity) {
    auto* player = localPlayer();
    if (!player) return false;
    auto const& own = player->getEntityContext();
    return static_cast<EntityId const&>(entity.mEntity) == own.mEntity
        && static_cast<uint const&>(entity.mRegistryId) == static_cast<uint const&>(own.mRegistry.mId);
}
LL_STATIC_HOOK(MoveRequestHook, ll::memory::HookPriority::Normal, &MoveCollisionSystem::fetchCollisionShapes, void,
    StrictEntityContext const& entity, AABBShapeComponent const& aabb, MaxAutoStepComponent const& autoStep,
    Optional<CollidableMobNearFlagComponent const> collidableMobNear, MoveRequestComponent& request,
    Optional<MinecartFlagComponent const> isMinecart,
    ViewT<StrictEntityContext, Include<CollidableMobFlagComponent>, AABBShapeComponent const> const& collidableMobs,
    ViewT<StrictEntityContext, AABBShapeComponent const, ActorDataFlagComponent const> const& stackableView,
    ViewT<StrictEntityContext, Include<FallingBlockFlagComponent>> const& fallingBlocks, IConstBlockSource const& region,
    LocalSpatialEntityFetcher& fetcher, GetCollisionShapeInterface const& collisionShape,
    std::vector<BlockSourceVisitor::CollisionShape>& tempCollisionShapes,
    std::vector<BlockSourceVisitor::CollisionShape>& scratchCollisionShapes, std::vector<AABB>& tempShapes) {
    try {
        if (isLocalEntity(entity)) {
            ++localRequests;
            bool& sneaking = request.mSneaking;
            if (sneaking) ++sneakingRequests;
            else if (fakeSneak.on.load()) { sneaking = true; ++forcedRequests; }
        }
    } catch (...) {}
    origin(entity, aabb, autoStep, collidableMobNear, request, isMinecart, collidableMobs, stackableView, fallingBlocks,
        region, fetcher, collisionShape, tempCollisionShapes, scratchCollisionShapes, tempShapes);
}

// L-37: spectator switches the renderer to culler type 5, and isSpectator
// alone did not (third trace). F10 now makes Player::getPlayerGameType answer
// Spectator for the local player; callers are logged per answer.
Toggle fakeSpectator{VK_F10, "L-37 fake spectator game type"};
std::mutex spectatorMutex;
std::map<std::pair<uintptr_t, bool>, unsigned> spectatorCallers;
Clock::time_point lastL37 = Clock::now();
unsigned l37Lines = 0;
void dumpL37(bool force) {
    std::lock_guard lock{spectatorMutex};
    if (spectatorCallers.empty() || (!force && Clock::now() - lastL37 < std::chrono::seconds(10))) return;
    lastL37 = Clock::now();
    for (auto const& [key, count] : spectatorCallers) {
        if (l37Lines >= 1500) break;
        ++l37Lines;
        log("research L-37 getPlayerGameType caller=+0x{:x} spectator={} calls={}", key.first, key.second, count);
    }
    spectatorCallers.clear();
}
LL_TYPE_INSTANCE_HOOK(GameTypeHook, ll::memory::HookPriority::Normal, Player, &Player::getPlayerGameType, GameType) {
    GameType result = origin();
    if (localPlayer() != static_cast<Player const*>(this)) return result;
    GameType answer = fakeSpectator.on.load() ? GameType::Spectator : result;
    {
        std::lock_guard lock{spectatorMutex};
        auto& count = spectatorCallers[{callerOffset(_ReturnAddress()), answer == GameType::Spectator}];
        if (count < 1000000) ++count;
    }
    return answer;
}
void pollKeys() {
    bool before = fakeSpectator.on.load();
    fakeSneak.poll();
    fakeSpectator.poll();
    if (before != fakeSpectator.on.load()) dumpL37(true);
    dumpL37(false);
    dumpL40();
}

// L-37 / L-44: sample the culler and FOV state once a change happens (and
// every 10 s), next to spectator/detached-camera state.
struct Sample {
    int culler = -1;
    bool forceCulling = false, spectator = false, detached = false, variable = false, sprinting = false, zoom = false;
    int fov = 0, modifier = 0;
    bool operator==(Sample const&) const = default;
};
// getFov runs twice a frame (world with variable FOV, hand without), so each
// kind keeps its own previous sample; the total is capped.
Sample previous[2];
Clock::time_point lastSample[2]{Clock::now() - std::chrono::hours(1), Clock::now() - std::chrono::hours(1)};
unsigned sampleLines = 0;
void sample(LevelRendererPlayer& renderer, bool variable, float fov) {
    auto& client = static_cast<IClientInstance&>(renderer.mClientInstance);
    auto* player = client.getLocalPlayer();
    if (!player) return;
    Sample now;
    now.culler = static_cast<int>(static_cast<LevelCullerType const&>(renderer.mLastCullerType));
    now.forceCulling = static_cast<bool const&>(renderer.mForceCulling);
    now.spectator = player->isSpectator();
    now.detached = Zoom::instance().detachedViewRay(client).has_value();
    now.variable = variable;
    now.sprinting = player->isSprinting();
    now.zoom = Zoom::instance().sensitivity(*player) != 1.f;
    now.fov = static_cast<int>(std::lround(fov * 10));
    now.modifier = static_cast<int>(std::lround(player->getFieldOfViewModifier() * 1000));
    auto kind = variable ? 1 : 0;
    if (now == previous[kind] && Clock::now() - lastSample[kind] < std::chrono::seconds(10)) return;
    if (sampleLines >= 3000) return;
    ++sampleLines;
    previous[kind] = now;
    lastSample[kind] = Clock::now();
    Vec3 const& camera = renderer.mCameraPos;
    log("research L-37/L-44 culler={} forceCulling={} spectator={} detached={} camera={:.1f},{:.1f},{:.1f} "
        "variableFov={} fov={:.1f} fovModifier={:.3f} sprinting={} zoom={}",
        now.culler, now.forceCulling, now.spectator, now.detached, camera.x, camera.y, camera.z,
        now.variable, now.fov / 10.0, now.modifier / 1000.0, now.sprinting, now.zoom);
}
LL_TYPE_INSTANCE_HOOK(FovSampleHook, ll::memory::HookPriority::Low, LevelRendererPlayer,
    &LevelRendererPlayer::getFov, float, float alpha, bool variable) {
    float result = origin(alpha, variable);
    try {
        pollKeys();
        sample(*this, variable, result);
    } catch (...) {}
    return result;
}
bool requestInstalled = false, spectatorInstalled = false, fovInstalled = false;
}
void start() {
    requestInstalled = MoveRequestHook::hook(true) == 0;
    spectatorInstalled = GameTypeHook::hook(true) == 0;
    fovInstalled = FovSampleHook::hook(true) == 0;
    if (!requestInstalled || !spectatorInstalled || !fovInstalled) {
        stop();
        throw std::runtime_error("Could not install research diagnostics");
    }
    Runtime::instance().self().getLogger().warn("Research diagnostics enabled (L-37, L-40, L-44); F9 move-request sneaking, F10 fake spectator game type");
}
void stop() {
    fakeSneak.on = false;
    fakeSpectator.on = false;
    if (fovInstalled && FovSampleHook::unhook(true)) fovInstalled = false;
    if (spectatorInstalled && GameTypeHook::unhook(true)) spectatorInstalled = false;
    if (requestInstalled && MoveRequestHook::unhook(true)) requestInstalled = false;
}
}
#else
namespace lamium::researchTrace { void start() {} void stop() {} }
#endif
