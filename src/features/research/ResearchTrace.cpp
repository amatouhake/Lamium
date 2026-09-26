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

// L-37: spectator selects culler type 5; neither isSpectator nor the game
// type drives it. F10 asks the renderer for type 5 through its virtual
// updateLevelCullerType while the view is detached, and counts how often it
// has to ask again (the renderer rebuilding type 3 each frame would show as
// one request per frame).
Toggle requestCuller{VK_F10, "L-37 request culler type 5"};
std::atomic<unsigned> cullerRequests{0};
Clock::time_point lastL37 = Clock::now();
void requestCullerType(LevelRendererPlayer& renderer, bool detached) {
    if (!requestCuller.on.load() || !detached) return;
    auto current = static_cast<int>(static_cast<LevelCullerType const&>(renderer.mLastCullerType));
    if (current == 5) return;
    ++cullerRequests;
    renderer.updateLevelCullerType(static_cast<LevelCullerType>(5));
}
void dumpL37() {
    if (Clock::now() - lastL37 < std::chrono::seconds(2)) return;
    lastL37 = Clock::now();
    if (auto count = cullerRequests.exchange(0); count || requestCuller.on.load())
        log("research L-37 per2s cullerRequests={} requesting={}", count, requestCuller.on.load());
}
void pollKeys() {
    requestCuller.poll();
    dumpL37();
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
        if (variable) {
            auto instance = ll::service::getClientInstance();
            requestCullerType(*this, instance && Zoom::instance().detachedViewRay(*instance).has_value());
        }
    } catch (...) {}
    return result;
}
bool fovInstalled = false;
}
void start() {
    fovInstalled = FovSampleHook::hook(true) == 0;
    if (!fovInstalled) {
        stop();
        throw std::runtime_error("Could not install research diagnostics");
    }
    Runtime::instance().self().getLogger().warn("Research diagnostics enabled (L-37); F10 requests culler type 5 while the view is detached");
}
void stop() {
    requestCuller.on = false;
    if (fovInstalled && FovSampleHook::unhook(true)) fovInstalled = false;
}
}
#else
namespace lamium::researchTrace { void start() {} void stop() {} }
#endif
