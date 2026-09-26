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
#include "mc/world/actor/provider/PlayerMoveInput.h"
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

// L-40: F9 toggles "report sneak-down" for the local player without real
// sneak input; call sites are grouped by return address to tell edge
// protection apart from speed, pose and network uses.
std::atomic<bool> forceSneak{false};
bool f9Down = false;
std::mutex callerMutex;
std::map<std::tuple<uintptr_t, bool, bool>, unsigned> callers;
Clock::time_point lastDump = Clock::now();
uintptr_t const moduleBase = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));

bool isLocal(EntityContext const& entity) {
    auto client = ll::service::getClientInstance();
    auto* player = client ? client->getLocalPlayer() : nullptr;
    if (!player) return false;
    auto const& own = player->getEntityContext();
    return &own.mEnTTRegistry == &entity.mEnTTRegistry && own.mEntity == entity.mEntity;
}
void dumpCallers(bool force) {
    std::lock_guard lock{callerMutex};
    if (callers.empty() || (!force && Clock::now() - lastDump < std::chrono::seconds(5))) return;
    lastDump = Clock::now();
    for (auto const& [key, count] : callers) {
        auto const& [offset, result, forced] = key;
        log("research L-40 isSneakDown caller=+0x{:x} vanilla={} forced={} calls={}", offset, result, forced, count);
    }
    callers.clear();
}
LL_STATIC_HOOK(SneakDownHook, ll::memory::HookPriority::Normal, &PlayerMoveInput::isSneakDown, bool,
    EntityContext const& entity) {
    bool result = origin(entity);
    static std::atomic<bool> seenAny{false}, seenLocal{false};
    bool local = isLocal(entity);
    if (!seenAny.exchange(true)) log("research L-40 isSneakDown first call (local={})", local);
    if (local && !seenLocal.exchange(true)) log("research L-40 isSneakDown first local call");
    if (!local) return result;
    bool forced = forceSneak.load() && !result;
    {
        std::lock_guard lock{callerMutex};
        auto offset = reinterpret_cast<uintptr_t>(_ReturnAddress()) - moduleBase;
        auto& count = callers[{offset, result, forced}];
        if (count < 1000000) ++count;
    }
    return result || forced;
}
void pollKeys() {
    bool down = (GetAsyncKeyState(VK_F9) & 0x8000) != 0;
    if (down != f9Down) log("research L-40 F9 {}", down ? "down" : "up");
    if (down && !f9Down) {
        dumpCallers(true);
        forceSneak = !forceSneak.load();
        log("research L-40 forced sneak-down {}", forceSneak.load() ? "ON" : "OFF");
    }
    f9Down = down;
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
        dumpCallers(false);
        sample(*this, variable, result);
    } catch (...) {}
    return result;
}
bool sneakInstalled = false, fovInstalled = false;
}
void start() {
    sneakInstalled = SneakDownHook::hook(true) == 0;
    fovInstalled = FovSampleHook::hook(true) == 0;
    if (!sneakInstalled || !fovInstalled) {
        stop();
        throw std::runtime_error("Could not install research diagnostics");
    }
    Runtime::instance().self().getLogger().warn("Research diagnostics enabled (L-37, L-40, L-44); F9 toggles forced sneak-down");
}
void stop() {
    forceSneak = false;
    if (fovInstalled && FovSampleHook::unhook(true)) fovInstalled = false;
    if (sneakInstalled && SneakDownHook::unhook(true)) sneakInstalled = false;
}
}
#else
namespace lamium::researchTrace { void start() {} void stop() {} }
#endif
