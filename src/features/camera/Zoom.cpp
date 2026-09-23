#include "features/camera/Zoom.h"
#include "features/camera/CameraInteraction.h"
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
#include "mc/deps/ecs/gamerefs_entity/EntityContext.h"
#include "mc/deps/minecraft_camera/components/ActiveCameraComponent.h"
#include "mc/deps/minecraft_camera/components/CameraDirectLookComponent.h"
#include "mc/deps/minecraft_camera/components/CameraOrbitComponent.h"
#include "mc/deps/vanilla_camera/components/UpdatePlayerFromCameraComponent.h"
#include "mc/deps/vanilla_camera/CameraClientInstance.h"
#include "mc/legacy/ActorRuntimeID.h"
#include "mc/world/actor/ActorFlags.h"
#include "ui/SettingsScreen.h"
#include <cmath>
#ifdef LAMIUM_CAMERA_TRACE
#include "mc/deps/renderer/Camera.h"
#include <algorithm>
#include <format>
#include <atomic>
#include <cmath>
#endif

namespace lamium {
namespace {
bool canDetachLook(LocalPlayer const& player) {
    // Leave an existing charge/eating action with vanilla. Do not finish or
    // release it on the player's behalf when entering a detached camera.
    return player.isAlive() && !player.isSleeping() && !player.getVehicle()
        && player.hasRuntimeID() && !player.getStatusFlag(ActorFlags::Usingitem);
}
// Vanilla turns the active camera from look input, then copies the camera's
// orientation to the player for cameras carrying UpdatePlayerFromCamera.
// Freelook withholds that component so only the camera turns. The camera's own
// look angles are saved first and written back before reattaching, so the view
// returns to the unchanged player orientation instead of turning the player.
struct DetachedCamera {
    EntityId entity;
    VanillaCamera::UpdatePlayerFromCameraComponent::LookMode mode;
    bool hasDirectLook;
    float yaw, pitch;
    // Third-person cameras keep their orientation in the orbit instead.
    bool hasOrbit = false;
    float currentAzimuth = 0, currentPolar = 0, idealAzimuth = 0, idealPolar = 0;
};
std::vector<DetachedCamera> detachedCameras; // Client thread only.
void detachCameras(LocalPlayer& player) {
    auto& registry = player.getEntityContext().getRegistry();
    std::vector<EntityId> targets;
    for (auto entity : registry.view<MinecraftCamera::ActiveCameraComponent,
                                     VanillaCamera::UpdatePlayerFromCameraComponent>())
        targets.push_back(entity);
    for (auto entity : targets) {
        DetachedCamera saved{entity, registry.get<VanillaCamera::UpdatePlayerFromCameraComponent>(entity).mLookMode, false, 0, 0};
        if (auto* look = registry.try_get<MinecraftCamera::CameraDirectLookComponent>(entity)) {
            saved.hasDirectLook = true;
            saved.yaw = look->mYaw;
            saved.pitch = look->mPitch;
        }
        if (auto* orbit = registry.try_get<MinecraftCamera::CameraOrbitComponent>(entity)) {
            saved.hasOrbit = true;
            saved.currentAzimuth = orbit->mCurrentSpherical->mAzimuth;
            saved.currentPolar = orbit->mCurrentSpherical->mPolarAngle;
            saved.idealAzimuth = orbit->mIdealSpherical->mAzimuth;
            saved.idealPolar = orbit->mIdealSpherical->mPolarAngle;
        }
        registry.remove<VanillaCamera::UpdatePlayerFromCameraComponent>(entity);
        detachedCameras.push_back(saved);
    }
#ifdef LAMIUM_CAMERA_TRACE
    try {
        Runtime::instance().self().getLogger().info("Freelook camera: detached={} directLook={} orbit={} yaw={} pitch={}",
            detachedCameras.size(), !detachedCameras.empty() && detachedCameras[0].hasDirectLook,
            !detachedCameras.empty() && detachedCameras[0].hasOrbit,
            detachedCameras.empty() ? 0.f : detachedCameras[0].yaw, detachedCameras.empty() ? 0.f : detachedCameras[0].pitch);
    } catch (...) {}
#endif
}
void restoreCameras(LocalPlayer* player) {
    if (detachedCameras.empty()) return;
    // Without the owning level the camera entities no longer exist.
    if (player) {
        auto& registry = player->getEntityContext().getRegistry();
        for (auto const& saved : detachedCameras) {
            if (!registry.valid(saved.entity)) continue;
            if (auto* look = registry.try_get<MinecraftCamera::CameraDirectLookComponent>(saved.entity);
                look && saved.hasDirectLook) {
#ifdef LAMIUM_CAMERA_TRACE
                try {
                    Runtime::instance().self().getLogger().info("Freelook camera: restore yaw={}->{} pitch={}->{}",
                        look->mYaw, saved.yaw, look->mPitch, saved.pitch);
                } catch (...) {}
#endif
                look->mYaw = saved.yaw;
                look->mPitch = saved.pitch;
                look->mYawDelta = 0.f;
            }
            if (auto* orbit = registry.try_get<MinecraftCamera::CameraOrbitComponent>(saved.entity);
                orbit && saved.hasOrbit) {
#ifdef LAMIUM_CAMERA_TRACE
                try {
                    Runtime::instance().self().getLogger().info("Freelook camera: restore orbit azimuth={}->{} polar={}->{}",
                        (float)orbit->mCurrentSpherical->mAzimuth, saved.currentAzimuth,
                        (float)orbit->mCurrentSpherical->mPolarAngle, saved.currentPolar);
                } catch (...) {}
#endif
                orbit->mCurrentSpherical->mAzimuth = saved.currentAzimuth;
                orbit->mCurrentSpherical->mPolarAngle = saved.currentPolar;
                orbit->mIdealSpherical->mAzimuth = saved.idealAzimuth;
                orbit->mIdealSpherical->mPolarAngle = saved.idealPolar;
                orbit->mAzimuthVelocity = 0.f;
                orbit->mPolarAngleVelocity = 0.f;
            }
            registry.emplace_or_replace<VanillaCamera::UpdatePlayerFromCameraComponent>(saved.entity).mLookMode = saved.mode;
        }
    }
    detachedCameras.clear();
}
#ifdef LAMIUM_CAMERA_TRACE
enum class LookTraceStage { Begin, Turn, Render };
void traceLook(LookTraceStage stage, float pitch, float yaw) noexcept {
    // Independent budgets: startup render sampling must not consume input evidence.
    static std::atomic<unsigned> counts[3]{};
    auto index = static_cast<unsigned>(stage);
    auto& counter = counts[index];
    auto count = counter.load(std::memory_order_relaxed);
    while (count < 32 && !counter.compare_exchange_weak(
        count, count + 1, std::memory_order_relaxed)) {}
    if (count >= 32) return;
    try {
        constexpr char const* names[] = {"begin", "turn-native-delta", "camera-rotation"};
        Runtime::instance().self().getLogger().info(
            "Freelook trace: stage={} sample={} pitch={} yaw={}", names[index], count, pitch, yaw);
    } catch (...) {}
}

// Bounded per-call-site budget for the Freelook source diagnostics below.
struct TraceBudget {
    std::atomic<unsigned> used{0};
    bool take(unsigned limit) noexcept {
        auto count = used.load(std::memory_order_relaxed);
        while (count < limit && !used.compare_exchange_weak(count, count + 1, std::memory_order_relaxed)) {}
        return count < limit;
    }
};
template <class Message>
void traceFreelookSource(TraceBudget& budget, unsigned limit, Message&& message) noexcept {
    if (!budget.take(limit)) return;
    try { Runtime::instance().self().getLogger().info("Freelook source: {}", message()); } catch (...) {}
}
// Read-only: does the camera consume look input independently of the player's turn?
LL_TYPE_INSTANCE_HOOK(LookDeltaTraceHook, ll::memory::HookPriority::Normal, CameraClientInstance,
    &CameraClientInstance::$getLookDelta, Vec2) {
    auto delta = origin();
    if (delta.x != 0 || delta.z != 0) {
        // Separate budgets: ordinary looking must not exhaust the detached evidence.
        static TraceBudget budgets[2];
        bool detached = Zoom::instance().lookAngles().has_value();
        traceFreelookSource(budgets[detached], 16, [&] { return std::format("camera-look-delta pitch={} yaw={} detached={}", delta.x, delta.z, detached); });
    }
    return delta;
}

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
#if defined(LAMIUM_CAMERA_PROBE) || defined(LAMIUM_CAMERA_POSITION_PROBE)
    if (Zoom::instance().viewProbeActive() && !camera.viewMatrixStack->stack->empty()) {
        // Camera-local 20-degree yaw. Pre-multiplication rotates the view without
        // translating its eye. Always compose with this call's vanilla result.
        auto view = *camera.viewMatrixStack->top()._m;
        bool finite = true;
        for (int column = 0; column < 4; ++column)
            for (int row = 0; row < 4; ++row)
                finite = finite && std::isfinite(view[column][row]);
        if (finite) {
#ifdef LAMIUM_CAMERA_PROBE
            constexpr float angle = 0.3490658504f;
            glm::mat4 rotation{1.f};
            rotation[0][0] = rotation[2][2] = std::cos(angle);
            rotation[0][2] = -std::sin(angle);
            rotation[2][0] = std::sin(angle);
            *camera.viewMatrixStack->getTop()._m = rotation * view;
#else
            // A bounded two-block camera-local displacement. Keep the original
            // world origin; test whether downstream view dependencies and
            // world-relative geometry agree before integrating free movement.
            glm::mat4 translation{1.f};
            translation[3][0] = -2.f;
            *camera.viewMatrixStack->getTop()._m = translation * view;
            static std::atomic<unsigned> positionSamples{0};
            auto sampleIndex = positionSamples.fetch_add(1, std::memory_order_relaxed);
            if (sampleIndex < 8) Runtime::instance().self().getLogger().info(
                "Camera position probe: sample={} appliedLocalRight=2", sampleIndex);
#endif
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
LL_TYPE_INSTANCE_HOOK(FovHook, ll::memory::HookPriority::Normal, LevelRendererPlayer,
    &LevelRendererPlayer::getFov, float, float alpha, bool variable) {
    return Zoom::instance().fov(mClientInstance, origin(alpha, variable));
}
LL_TYPE_INSTANCE_HOOK(TurnHook, ll::memory::HookPriority::Normal, LocalPlayer,
    &LocalPlayer::_applyTurnDelta, void, Vec2 const& delta) {
    // While detached, vanilla still turns the camera; only the copy to the
    // player is withheld (see detachCameras).
    if (Zoom::instance().turnLook(*this, delta.x, delta.z)) { origin(delta); return; }
    float scale = Zoom::instance().sensitivity(*this);
    origin(Vec2{delta.x * scale, delta.z * scale});
}
// Vanilla also turns the local player's head yaw toward the camera. Keep the
// head where it was when the detached look began; the body is already fixed.
#ifdef LAMIUM_CAMERA_TRACE
void traceHeadLock(char const* setter, float requested, float kept) noexcept {
    static TraceBudget budget;
    traceFreelookSource(budget, 16, [&] { return std::format("head-lock setter={} requested={} kept={}", setter, requested, kept); });
}
#endif
LL_TYPE_INSTANCE_HOOK(HeadRotHook, ll::memory::HookPriority::Normal, Actor,
    &Actor::setYHeadRot, void, float yHeadRot) {
    if (auto kept = Zoom::instance().lockedHeadFor(*this)) {
#ifdef LAMIUM_CAMERA_TRACE
        traceHeadLock("single", yHeadRot, *kept);
#endif
        yHeadRot = *kept;
    }
    origin(yHeadRot);
}
LL_TYPE_INSTANCE_HOOK(HeadRotationsHook, ll::memory::HookPriority::Normal, Actor,
    &Actor::setYHeadRotations, void, float yHeadRot, float oldYHeadRot) {
    if (auto kept = Zoom::instance().lockedHeadFor(*this)) {
#ifdef LAMIUM_CAMERA_TRACE
        traceHeadLock("pair", yHeadRot, *kept);
#endif
        yHeadRot = oldYHeadRot = *kept;
    }
    origin(yHeadRot, oldYHeadRot);
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
    {LookDeltaTraceHook::hook, LookDeltaTraceHook::unhook},
#endif
    {FovHook::hook, FovHook::unhook},
    {HeadRotHook::hook, HeadRotHook::unhook},
    {HeadRotationsHook::hook, HeadRotationsHook::unhook},
    {TurnHook::hook, TurnHook::unhook},
    {DimensionHook::hook, DimensionHook::unhook},
    {FocusHook::hook, FocusHook::unhook}
};
}
Zoom& Zoom::instance() { static Zoom value; return value; }
float Zoom::fov(IClientInstance const& renderedClient, float base) const {
    return running && client.load() == &renderedClient ? state.fov(base) : base;
}
float Zoom::sensitivity(LocalPlayer const& player) const {
    auto* current = client.load();
    return running && current && current->getLocalPlayer() == &player ? state.sensitivity() : 1.f;
}
std::optional<DetachedLookState::Angles> Zoom::lookAnglesFor(IClientInstance const& renderedClient) {
    // A different viewport must neither consume nor cancel the owner's session.
    if (client.load() != &renderedClient) return {};
    return lookAngles();
}
#if defined(LAMIUM_CAMERA_PROBE) || defined(LAMIUM_CAMERA_POSITION_PROBE)
bool Zoom::viewProbeActive() const {
    auto* current = client.load();
    return running && allowed && state.held() && current && gameplayScreen(current->getScreenName());
}
#endif
void Zoom::configure(Settings const& settings) {
    lookAllowed = settings.camera.freelook;
    cancelLook();
    allowed = settings.camera.zoom;
    state.configure(settings.camera.magnification, settings.camera.wheelStep);
}
void Zoom::pressLook(IClientInstance& current) {
    if (!running || !lookAllowed || ui::ownsInput() || !gameplayScreen(current.getScreenName())
        || !current.getLocalPlayer()) return;
    auto* player = current.getLocalPlayer();
    if (!canDetachLook(*player)) return;
    client = &current;
    if (!look.begin(player->getRotation().x, player->getRotation().z, player->getRuntimeID().rawID)) return;
#ifdef LAMIUM_CAMERA_TRACE
    traceLook(LookTraceStage::Begin, player->getRotation().x, player->getRotation().z);
    try { Runtime::instance().self().getLogger().info("Freelook body: begin head={}", player->getYHeadRot()); } catch (...) {}
#endif
    try {
        lockedHead = player->getYHeadRot();
        detachCameras(*player);
    } catch (...) {
        cancelLook();
        Runtime::instance().self().getLogger().error("Freelook could not detach the camera");
    }
}
void Zoom::releaseLook() {
    look.release();
    endLookCamera();
}
void Zoom::cancelLook() {
    look.cancel();
    endLookCamera();
}
void Zoom::endLookCamera() {
    auto* current = client.load();
#ifdef LAMIUM_CAMERA_TRACE
    if (auto* player = current ? current->getLocalPlayer() : nullptr; player && !detachedCameras.empty()) {
        try {
            Runtime::instance().self().getLogger().info("Freelook body: end pitch={} yaw={} head={}",
                player->getRotation().x, player->getRotation().z, player->getYHeadRot());
        } catch (...) {}
    }
#endif
    try {
        restoreCameras(current ? current->getLocalPlayer() : nullptr);
    } catch (...) {
        detachedCameras.clear();
        Runtime::instance().self().getLogger().error("Freelook could not restore the camera");
    }
}
std::optional<DetachedLookState::Angles> Zoom::lookAngles() {
    if (!look.snapshot()) return {};
    auto* current = client.load();
    if (!running || !lookAllowed || !current || ui::ownsInput()
        || !gameplayScreen(current->getScreenName()) || !current->getLocalPlayer()) {
        cancelLook();
        return {};
    }
    auto* player = current->getLocalPlayer();
    if (!canDetachLook(*player)
        || !look.retainOwner(player->getRuntimeID().rawID)) {
        cancelLook();
        return {};
    }
    return look.snapshot();
}
bool Zoom::turnLook(LocalPlayer& player, float pitchDelta, float yawDelta) {
    auto* current = client.load();
    if (!current || current->getLocalPlayer() != &player) return false;
    if (!lookAngles()) return false;
#ifdef LAMIUM_CAMERA_TRACE
    traceLook(LookTraceStage::Turn, pitchDelta, yawDelta);
#endif
    return true;
}
std::optional<float> Zoom::lockedHeadFor(Actor const& actor) const {
    // Snapshot only: a setter callback must not start cancellation/restoration.
    auto* current = client.load();
    if (!running || !current || static_cast<Actor const*>(current->getLocalPlayer()) != &actor
        || !look.snapshot()) return {};
    return lockedHead.load();
}
bool Zoom::blocksLookInteraction(Player& player) {
    auto* current = client.load();
    return current && current->getLocalPlayer() == &player && lookAngles().has_value();
}
void Zoom::press(IClientInstance& current) {
    if (!running || !allowed || !gameplayScreen(current.getScreenName())) return;
    client = &current;
    state.press();
}
bool Zoom::start() {
    if (running) return true;
    try {
        camera::startInteractionGuard();
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
    camera::stopInteractionGuard();
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
