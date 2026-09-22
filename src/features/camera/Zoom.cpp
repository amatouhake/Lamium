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
#include "mc/deps/renderer/Camera.h"
#include "ui/SettingsScreen.h"
#include <cmath>
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

// The trace is read-only; the separately enabled probe modifies only the fresh view.
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
#ifdef LAMIUM_CAMERA_PROBE
    if (Zoom::instance().viewProbeActive() && !camera.viewMatrixStack->stack->empty()) {
        // Camera-local 20-degree yaw. Pre-multiplication rotates the view without
        // translating its eye. Always compose with this call's vanilla result.
        auto view = *camera.viewMatrixStack->top()._m;
        bool finite = true;
        for (int column = 0; column < 4; ++column)
            for (int row = 0; row < 4; ++row)
                finite = finite && std::isfinite(view[column][row]);
        if (finite) {
            constexpr float angle = 0.3490658504f;
            glm::mat4 rotation{1.f};
            rotation[0][0] = rotation[2][2] = std::cos(angle);
            rotation[0][2] = -std::sin(angle);
            rotation[2][0] = std::sin(angle);
            *camera.viewMatrixStack->getTop()._m = rotation * view;
        }
    }
#endif
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
LL_TYPE_INSTANCE_HOOK(FreelookCameraHook, ll::memory::HookPriority::Normal, LevelRendererPlayer,
    &LevelRendererPlayer::setupCamera, void, mce::Camera& camera, float alpha) {
    origin(camera, alpha);
    auto pose = Zoom::instance().lookAngles();
    if (!pose || camera.viewMatrixStack->stack->empty()) return;
    auto view = *camera.viewMatrixStack->top()._m;
    for (int column = 0; column < 4; ++column)
        for (int row = 0; row < 4; ++row)
            if (!std::isfinite(view[column][row])) { Zoom::instance().releaseLook(); return; }
    constexpr float radians = 0.01745329252f;
    float yaw = pose->yaw * radians, pitch = pose->pitch * radians;
    glm::mat4 horizontal{1.f}, vertical{1.f};
    horizontal[0][0] = horizontal[2][2] = std::cos(yaw);
    horizontal[0][2] = -std::sin(yaw);
    horizontal[2][0] = std::sin(yaw);
    vertical[1][1] = vertical[2][2] = std::cos(pitch);
    vertical[1][2] = std::sin(pitch);
    vertical[2][1] = -std::sin(pitch);
    *camera.viewMatrixStack->getTop()._m = vertical * horizontal * view;
}
LL_TYPE_INSTANCE_HOOK(FovHook, ll::memory::HookPriority::Normal, LevelRendererPlayer,
    &LevelRendererPlayer::getFov, float, float alpha, bool variable) {
    return Zoom::instance().fov(origin(alpha, variable));
}
LL_TYPE_INSTANCE_HOOK(TurnHook, ll::memory::HookPriority::Normal, LocalPlayer,
    &LocalPlayer::_applyTurnDelta, void, Vec2 const& delta) {
    if (Zoom::instance().turnLook(*this, delta.x, delta.z)) return;
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
    {FreelookCameraHook::hook, FreelookCameraHook::unhook},
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
#ifdef LAMIUM_CAMERA_PROBE
bool Zoom::viewProbeActive() const {
    auto* current = client.load();
    return running && allowed && state.held() && current && gameplayScreen(current->getScreenName());
}
#endif
void Zoom::configure(Settings const& settings) {
    lookAllowed = settings.camera.freelook;
    releaseLook();
    allowed = settings.camera.zoom;
    state.configure(settings.camera.magnification, settings.camera.wheelStep);
}
void Zoom::pressLook(IClientInstance& current) {
    if (!running || !lookAllowed || ui::ownsInput() || !gameplayScreen(current.getScreenName())
        || !current.getLocalPlayer()) return;
    client = &current;
    look.begin(0, 0);
}
std::optional<DetachedLookState::Angles> Zoom::lookAngles() {
    if (!look.snapshot()) return {};
    auto* current = client.load();
    if (!running || !lookAllowed || !current || ui::ownsInput()
        || !gameplayScreen(current->getScreenName()) || !current->getLocalPlayer()) {
        releaseLook();
        return {};
    }
    return look.snapshot();
}
bool Zoom::turnLook(LocalPlayer& player, float pitchDelta, float yawDelta) {
    auto* current = client.load();
    if (!current || current->getLocalPlayer() != &player) return false;
    if (!lookAngles()) return false;
    // Experimental input calibration: native turn units still need runtime verification.
    look.turn(pitchDelta * .15f, yawDelta * .15f);
    return true;
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
            (void)lookAngles();
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
