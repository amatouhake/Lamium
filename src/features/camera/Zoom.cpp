#include "features/camera/Zoom.h"
#include "settings/Settings.h"
#include "input/Actions.h"
#include "app/Runtime.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/client/ClientExitLevelEvent.h"
#include "ll/api/event/input/MouseInputEvent.h"
#include "ll/api/event/render/UIRenderEvent.h"
#include "ll/api/memory/Hook.h"
#include "mc/client/game/IClientInstance.h"
#include "mc/client/game/MinecraftGame.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/client/renderer/game/LevelRendererPlayer.h"
#include "mc/deps/core/math/Vec2.h"
#include "mc/deps/input/MouseAction.h"
#ifdef LAMIUM_CAMERA_TRACE
#include "mc/deps/renderer/Camera.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#endif

namespace lamium {
namespace {
#ifdef LAMIUM_CAMERA_TRACE
struct CameraTraceContext {
    mce::Camera const* setupCamera = nullptr;
    unsigned setupSerial = 0;
    bool seenSetup = false;
};
thread_local CameraTraceContext cameraTraceContext;

// Keep camera identity only for the duration of the synchronous setup callback.
struct CameraTraceScope {
    mce::Camera const* previous;
    explicit CameraTraceScope(mce::Camera const& camera)
    : previous(cameraTraceContext.setupCamera) {
        cameraTraceContext.setupCamera = &camera;
        cameraTraceContext.seenSetup = true;
        ++cameraTraceContext.setupSerial;
    }
    ~CameraTraceScope() { cameraTraceContext.setupCamera = previous; }
};

LL_TYPE_INSTANCE_HOOK(CameraDependenciesTraceHook, ll::memory::HookPriority::Normal, mce::Camera,
    &mce::Camera::updateViewMatrixDependencies, void) {
    origin();
    if (!cameraTraceContext.seenSetup) return;
    static std::atomic<unsigned> calls{0};
    auto count = calls.load(std::memory_order_relaxed);
    while (count < 64 && !calls.compare_exchange_weak(
        count, count + 1, std::memory_order_relaxed)) {}
    if (count >= 64 || viewMatrixStack->stack->empty()) return;
    try {
        auto product = *viewMatrixStack->top()._m * *mInverseViewMatrix;
        float inverseError = 0;
        bool finite = true;
        for (int column = 0; column < 4; ++column) {
            for (int row = 0; row < 4; ++row) {
                finite = finite && std::isfinite(product[column][row]);
                inverseError = std::max(inverseError, std::abs(product[column][row] - (column == row ? 1.f : 0.f)));
            }
        }
        Runtime::instance().self().getLogger().info(
            "Camera dependencies: sample={} setupSerial={} insideSetup={} sameCamera={} finite={} inverseError={} basisLengths={}/{}/{}",
            count, cameraTraceContext.setupSerial, cameraTraceContext.setupCamera != nullptr,
            cameraTraceContext.setupCamera == this, finite, inverseError,
            glm::length(*mRight), glm::length(*mUp), glm::length(*mForward));
    } catch (...) {}
}

// Observe only: never modify matrices, dependency caches, or the player pose.
LL_TYPE_INSTANCE_HOOK(CameraTraceHook, ll::memory::HookPriority::Normal, LevelRendererPlayer,
    &LevelRendererPlayer::setupCamera, void, mce::Camera& camera, float alpha) {
    CameraTraceScope scope{camera};
    static std::atomic<unsigned> calls{0};
    auto count = calls.load(std::memory_order_relaxed);
    while (count < 3840 && !calls.compare_exchange_weak(
        count, count + 1, std::memory_order_relaxed)) {}
    bool sample = count < 3840 && count % 120 == 0;
    bool beforeValid = sample && !camera.viewMatrixStack->stack->empty();
    glm::mat4 before{1};
    if (beforeValid) before = *camera.viewMatrixStack->top()._m;
    origin(camera, alpha);
    if (!sample || camera.viewMatrixStack->stack->empty()) return;
    try {
        auto const& view = *camera.viewMatrixStack->top()._m;
        auto product = view * *camera.mInverseViewMatrix;
        float inverseError = 0, change = 0;
        bool finite = true;
        for (int column = 0; column < 4; ++column) {
            for (int row = 0; row < 4; ++row) {
                finite = finite && std::isfinite(view[column][row]) && std::isfinite(product[column][row]);
                inverseError = std::max(inverseError, std::abs(product[column][row] - (column == row ? 1.f : 0.f)));
                if (beforeValid) change = std::max(change, std::abs(view[column][row] - before[column][row]));
            }
        }
        Runtime::instance().self().getLogger().info(
            "Camera trace: sample={} setupSerial={} alpha={} before={} finite={} viewChange={} inverseError={} basisLengths={}/{}/{}",
            count / 120, cameraTraceContext.setupSerial, alpha, beforeValid, finite, change, inverseError,
            glm::length(*camera.mRight), glm::length(*camera.mUp), glm::length(*camera.mForward));
    } catch (...) {
        // Diagnostics must not interrupt rendering or expose native text/paths.
    }
}
#endif
LL_TYPE_INSTANCE_HOOK(FovHook, ll::memory::HookPriority::Normal, LevelRendererPlayer,
    &LevelRendererPlayer::getFov, float, float alpha, bool variable) {
    return Zoom::instance().fov(origin(alpha, variable));
}
LL_TYPE_INSTANCE_HOOK(TurnHook, ll::memory::HookPriority::Normal, LocalPlayer,
    &LocalPlayer::_applyTurnDelta, void, Vec2 const& delta) {
    float scale = Zoom::instance().sensitivity();
    origin(Vec2{delta.x * scale, delta.z * scale});
}
LL_TYPE_INSTANCE_HOOK(DimensionHook, ll::memory::HookPriority::Normal, LevelRendererPlayer,
    &LevelRendererPlayer::$onWillChangeDimension, void, Player& player) {
    Zoom::instance().reset();
    origin(player);
}
LL_TYPE_INSTANCE_HOOK(FocusHook, ll::memory::HookPriority::Normal, MinecraftGame,
    &MinecraftGame::$onAppFocusLost, void) {
    Zoom::instance().reset();
    origin();
}
struct HookEntry {
    int (*install)(bool);
    bool (*remove)(bool);
    bool installed = false;
};
HookEntry hooks[] = {
#ifdef LAMIUM_CAMERA_TRACE
    {CameraDependenciesTraceHook::hook, CameraDependenciesTraceHook::unhook},
    {CameraTraceHook::hook, CameraTraceHook::unhook},
#endif
    {FovHook::hook, FovHook::unhook},
    {TurnHook::hook, TurnHook::unhook},
    {DimensionHook::hook, DimensionHook::unhook},
    {FocusHook::hook, FocusHook::unhook}
};
}
Zoom& Zoom::instance() { static Zoom value; return value; }
void Zoom::configure(Settings const& settings) {
    allowed = settings.camera.zoom;
    state.configure(settings.camera.magnification, settings.camera.wheelStep);
}
void Zoom::press(IClientInstance& current) {
    if (!running || !allowed || !gameplayScreen(current.getScreenName())) return;
    client = &current;
    state.press();
}
bool Zoom::start() {
    if (running) return true;
    try {
        for (auto& hook : hooks) {
            if (hook.installed) continue;
            int result = hook.install(true);
            if (result != 0) {
                Runtime::instance().self().getLogger().error("Camera hook failed with code {}", result);
                stop();
                return false;
            }
            hook.installed = true;
        }
        auto& bus = ll::event::EventBus::getInstance();
        wheelListener = bus.emplaceListener<ll::event::input::MouseInputEvent>([this](auto& event) {
            if (!running || !state.held() || event.actionButtonId() != MouseAction::ActionWheel) return;
            auto* current = client.load();
            if (!current || !gameplayScreen(current->getScreenName())) { release(); return; }
            if (event.buttonData() == 0) return;
            state.wheel(event.buttonData() > 0 ? 1 : -1);
            event.cancel();
        });
        screenListener = bus.emplaceListener<ll::event::AfterUIRenderEvent>([this](auto&) {
            if (!state.held()) return;
            auto* current = client.load();
            if (!current || !gameplayScreen(current->getScreenName())) release();
        });
        exitListener = bus.emplaceListener<ll::event::ClientExitLevelEvent>([this](auto&) { reset(); });
        running = true;
        return true;
    } catch (std::exception const& error) {
        Runtime::instance().self().getLogger().error("Zoom initialization failed: {}", error.what());
        stop();
        return false;
    }
}
void Zoom::stop() {
    running = false;
    reset();
    auto& bus = ll::event::EventBus::getInstance();
    for (auto* listener : {&wheelListener, &screenListener, &exitListener}) {
        if (*listener) { bus.removeListener(*listener); listener->reset(); }
    }
    for (auto it = std::rbegin(hooks); it != std::rend(hooks); ++it) {
        if (it->installed) {
            if (it->remove(true)) it->installed = false;
            else Runtime::instance().self().getLogger().error("Could not remove a camera hook");
        }
    }
}
}
