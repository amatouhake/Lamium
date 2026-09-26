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
#include "mc/entity/systems/sneak_movement_system/SneakMovementSystem.h"
#include "mc/world/actor/provider/PlayerMovement.h"
#include "mc/world/phys/AABB.h"
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

// L-40: SneakMovementSystem looks like vanilla edge protection and
// PlayerMovement::calculateMoveVector takes the SneakingComponent (speed).
// F9 gives the local player a SneakingComponent with a movement factor of 1
// each input tick, without sneak input, to see whether edge protection
// follows the component alone.
Toggle fakeSneak{VK_F9, "L-40 fake SneakingComponent (factor 1)"};
bool addedSneaking = false;
std::atomic<unsigned> volumeCalls{0}, moveWithSneak{0}, moveWithout{0};
float lastFactor = -1;
Clock::time_point lastL40 = Clock::now();
void dumpL40() {
    if (Clock::now() - lastL40 < std::chrono::seconds(2)) return;
    lastL40 = Clock::now();
    unsigned volume = volumeCalls.exchange(0), with = moveWithSneak.exchange(0), without = moveWithout.exchange(0);
    if (!volume && !with) return;
    auto* player = localPlayer();
    log("research L-40 per2s edgeVolumeCalls={} moveVector(sneakComponent)={} moveVector(none)={} factor={:.2f} "
        "fake={} sneakingFlag={}", volume, with, without, lastFactor, fakeSneak.on.load(),
        player ? player->isSneaking() : false);
}
LL_STATIC_HOOK(SneakVolumeHook, ll::memory::HookPriority::Normal, &SneakMovementSystem::getMaxCollisionVolume, AABB,
    Vec3 const& speed, MaxAutoStepComponent const& step, AABBShapeComponent const& shape) {
    ++volumeCalls;
    return origin(speed, step, shape);
}
LL_STATIC_HOOK(MoveVectorHook, ll::memory::HookPriority::Normal, &PlayerMovement::calculateMoveVector, Vec2,
    MoveInputState const& input, bool flying, ActorDataFlagComponent const& flags, bool water,
    SneakingComponent const* sneaking) {
    if (sneaking) { ++moveWithSneak; lastFactor = sneaking->mSneakingMovementFactor; }
    else ++moveWithout;
    return origin(input, flying, flags, water, sneaking);
}
LL_STATIC_HOOK(FakeSneakInputHook, ll::memory::HookPriority::Low, &ClientInputUpdateSystem::extractRawHIDInput, void,
    MovementAbilitiesComponent const& abilities, MoveInputComponent const& input, ActorDataFlagComponent const& flags,
    RawMoveInputComponent& raw, Optional<SneakingComponent const> sneaking, Optional<WasInWaterFlagComponent const> water) {
    origin(abilities, input, flags, raw, sneaking, water);
    try {
        auto* player = localPlayer();
        if (!player) return;
        auto& context = player->getEntityContext();
        if (fakeSneak.on.load()) {
            if (!context.hasComponent<SneakingComponent>()) addedSneaking = true;
            context.getOrAddComponent<SneakingComponent>().mSneakingMovementFactor = 1.0f;
        } else if (addedSneaking) {
            addedSneaking = false;
            if (!player->isSneaking()) context.removeComponent<SneakingComponent>();
        }
    } catch (...) {}
}

// L-37: spectator switches the renderer to culler type 5. F10 makes
// Actor::isSpectator answer true for the local player (all callers) so the
// culler switch can be observed from FreeCamera; callers are logged so a
// later build can limit the answer to the renderer's call site.
Toggle fakeSpectator{VK_F10, "L-37 fake spectator"};
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
        log("research L-37 isSpectator caller=+0x{:x} answered={} calls={}", key.first, key.second, count);
    }
    spectatorCallers.clear();
}
LL_TYPE_INSTANCE_HOOK(SpectatorHook, ll::memory::HookPriority::Normal, Actor, &Actor::isSpectator, bool) {
    bool result = origin();
    auto* player = localPlayer();
    if (player != static_cast<Actor const*>(this)) return result;
    bool answer = result || fakeSpectator.on.load();
    {
        std::lock_guard lock{spectatorMutex};
        auto& count = spectatorCallers[{callerOffset(_ReturnAddress()), answer}];
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
bool volumeInstalled = false, moveInstalled = false, inputInstalled = false, spectatorInstalled = false,
     fovInstalled = false;
}
void start() {
    volumeInstalled = SneakVolumeHook::hook(true) == 0;
    moveInstalled = MoveVectorHook::hook(true) == 0;
    inputInstalled = FakeSneakInputHook::hook(true) == 0;
    spectatorInstalled = SpectatorHook::hook(true) == 0;
    fovInstalled = FovSampleHook::hook(true) == 0;
    if (!volumeInstalled || !moveInstalled || !inputInstalled || !spectatorInstalled || !fovInstalled) {
        stop();
        throw std::runtime_error("Could not install research diagnostics");
    }
    Runtime::instance().self().getLogger().warn("Research diagnostics enabled (L-37, L-40, L-44); F9 fake sneak component, F10 fake spectator");
}
void stop() {
    fakeSneak.on = false;
    fakeSpectator.on = false;
    if (fovInstalled && FovSampleHook::unhook(true)) fovInstalled = false;
    if (spectatorInstalled && SpectatorHook::unhook(true)) spectatorInstalled = false;
    if (inputInstalled && FakeSneakInputHook::unhook(true)) inputInstalled = false;
    if (moveInstalled && MoveVectorHook::unhook(true)) moveInstalled = false;
    if (volumeInstalled && SneakVolumeHook::unhook(true)) volumeInstalled = false;
}
}
#else
namespace lamium::researchTrace { void start() {} void stop() {} }
#endif
