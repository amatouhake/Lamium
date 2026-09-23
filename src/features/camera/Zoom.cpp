#include "features/camera/Zoom.h"
#include "features/camera/CameraInteraction.h"
#include "features/camera/CameraMovementInput.h"
#include "settings/Settings.h"
#include "input/Actions.h"
#include "app/Runtime.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/client/ClientExitLevelEvent.h"
#include "ll/api/event/input/MouseInputEvent.h"
#include "ll/api/event/render/UIRenderEvent.h"
#include "ll/api/memory/Hook.h"
#include "mc/client/entity/systems/ClientInputUpdateSystem.h"
#include "mc/client/game/IClientInstance.h"
#include "mc/client/game/MinecraftGame.h"
#include "mc/client/input/ClientMoveInputHandler.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/entity/components/MoveInputComponent.h"
#include "mc/entity/components/RawMoveInputComponent.h"
#include "mc/client/renderer/game/LevelRendererPlayer.h"
#include "mc/deps/core/math/Vec2.h"
#include "mc/deps/input/MouseAction.h"
#include "mc/deps/renderer/Camera.h"
#include "mc/deps/ecs/gamerefs_entity/EntityContext.h"
#include "mc/entity/components/ActorHeadRotationComponent.h"
#include "mc/deps/minecraft_camera/components/ActiveCameraComponent.h"
#include "mc/deps/minecraft_camera/components/CameraOffsetComponent.h"
#include "mc/deps/minecraft_camera/components/CameraDirectLookComponent.h"
#include "mc/deps/minecraft_camera/components/CameraOrbitComponent.h"
#include "mc/deps/vanilla_camera/components/UpdatePlayerFromCameraComponent.h"
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
template<class Registry>
bool hasActiveCamera(Registry& registry, EntityId entity) {
    // ActiveCameraComponent is an empty tag; entt cannot try_get it.
    for (auto candidate : registry.template view<MinecraftCamera::ActiveCameraComponent>())
        if (candidate == entity) return true;
    return false;
}
template<class Registry>
void detachOneCamera(Registry& registry, EntityId entity) {
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
void detachCameras(LocalPlayer& player) {
    auto& registry = player.getEntityContext().getRegistry();
    std::vector<EntityId> targets;
    for (auto entity : registry.view<MinecraftCamera::ActiveCameraComponent,
                                     VanillaCamera::UpdatePlayerFromCameraComponent>())
        targets.push_back(entity);
    for (auto entity : targets) detachOneCamera(registry, entity);
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
// Entity-side displacement for FreeCamera. Vanilla consumes the camera
// entity's own offset while building the render view, so culling and
// overlays follow by construction instead of fighting a post-setup edit.
struct SavedCameraOffset {
    bool active = false;
    bool added = false;
    bool orbit = false; // Third-person rigs pivot instead of offsetting.
    EntityId entity;
    float x = 0, y = 0, z = 0;
    float px = 0, py = 0, pz = 0;
};
SavedCameraOffset savedOffset; // Mirrors the detachedCameras lifetime rules.
void takeFreeCameraOffset(LocalPlayer& player, EntityId entity) {
    auto& registry = player.getEntityContext().getRegistry();
    if (!registry.valid(entity)) throw std::runtime_error("FreeCamera camera entity is gone");
    savedOffset = {};
    auto* offset = registry.try_get<MinecraftCamera::CameraOffsetComponent>(entity);
    if (!offset) {
        registry.emplace<MinecraftCamera::CameraOffsetComponent>(entity);
        offset = registry.try_get<MinecraftCamera::CameraOffsetComponent>(entity);
        if (!offset) throw std::runtime_error("FreeCamera cannot attach a camera offset");
        savedOffset.added = true;
    }
    savedOffset.active = true;
    savedOffset.entity = entity;
    savedOffset.x = (*offset->mEntityOffset).x;
    savedOffset.y = (*offset->mEntityOffset).y;
    savedOffset.z = (*offset->mEntityOffset).z;
    // Orbit cameras ignore the entity offset; their rig pivots instead.
    if (registry.try_get<MinecraftCamera::CameraOrbitComponent>(entity)) {
        savedOffset.orbit = true;
        savedOffset.px = (*offset->mPivot).x;
        savedOffset.py = (*offset->mPivot).y;
        savedOffset.pz = (*offset->mPivot).z;
    }
}
void restoreFreeCameraOffset(LocalPlayer* player) {
    if (!savedOffset.active) return;
    savedOffset.active = false;
    try {
        if (!player) return; // Level gone; its entities are dead anyway.
        auto& registry = player->getEntityContext().getRegistry();
        if (!registry.valid(savedOffset.entity)) return;
        auto* offset = registry.try_get<MinecraftCamera::CameraOffsetComponent>(savedOffset.entity);
        if (!offset) return;
        if (savedOffset.added) registry.remove<MinecraftCamera::CameraOffsetComponent>(savedOffset.entity);
        else {
            (*offset->mEntityOffset).x = savedOffset.x;
            (*offset->mEntityOffset).y = savedOffset.y;
            (*offset->mEntityOffset).z = savedOffset.z;
            if (savedOffset.orbit) {
                (*offset->mPivot).x = savedOffset.px;
                (*offset->mPivot).y = savedOffset.py;
                (*offset->mPivot).z = savedOffset.pz;
            }
        }
    } catch (...) {
        Runtime::instance().self().getLogger().error("FreeCamera could not restore the camera offset");
    }
}
void migrateFreeCamera(LocalPlayer& player) {
    // F5 hands the active role to another camera entity. Move the session:
    // restore the old rig, detach the new one, take its offset. The restore
    // path replays every list entry, so accumulation across switches is safe.
    auto& registry = player.getEntityContext().getRegistry();
    EntityId next{};
    bool found = false;
    unsigned count = 0;
    for (auto entity : registry.view<MinecraftCamera::ActiveCameraComponent>()) {
        if (!registry.valid(entity)) continue;
        if (!found) { next = entity; found = true; }
        ++count;
    }
#ifdef LAMIUM_CAMERA_TRACE
    static std::atomic<unsigned> migrations{0};
    if (migrations.load(std::memory_order_relaxed) < 8) {
        migrations.fetch_add(1, std::memory_order_relaxed);
        try {
            Runtime::instance().self().getLogger().info(
                "FreeCamera camera: retarget activeCameras={} found={}", count, found);
        } catch (...) {}
    }
#endif
    if (!found) throw std::runtime_error("FreeCamera found no active camera");
    restoreFreeCameraOffset(&player);
    auto& fresh = player.getEntityContext().getRegistry();
    if (fresh.try_get<VanillaCamera::UpdatePlayerFromCameraComponent>(next))
        detachOneCamera(fresh, next);
    else {
        bool known = false;
        for (auto const& saved : detachedCameras) known = known || saved.entity == next;
        if (!known) throw std::runtime_error("FreeCamera cannot take the new camera");
    }
    takeFreeCameraOffset(player, next);
}
#ifdef LAMIUM_CAMERA_TRACE
void traceFreeCamera(unsigned reason, double x, double y, double z) noexcept {
    // Bounded per-reason budget: distinguishes a hook that never fires (no
    // samples at all) from missing input, failed advance, or an
    // applied-but-invisible transform. Reasons: 0 no-session, 1 no-input,
    // 2 advance-fail, 3 applied with the displacement.
    static std::atomic<unsigned> counts[4]{};
    if (reason >= 4) return;
    auto& counter = counts[reason];
    auto count = counter.load(std::memory_order_relaxed);
    while (count < 4 && !counter.compare_exchange_weak(
        count, count + 1, std::memory_order_relaxed)) {}
    if (count >= 4) return;
    try {
        static constexpr char const* names[] = {"no-session", "no-input", "advance-fail", "applied"};
        Runtime::instance().self().getLogger().info(
            "FreeCamera trace: what={} sample={} dx={} dy={} dz={}", names[reason], count, x, y, z);
    } catch (...) {}
}
void traceWriter(bool same, bool orbit, DetachedCameraMotion::Vector const& displacement) noexcept {
    // Bounded: tells morph (same entity, changed shape) from swap (entity
    // replaced by the perspective switch) in a single session.
    static std::atomic<unsigned> count{0};
    auto taken = count.load(std::memory_order_relaxed);
    while (taken < 6 && !count.compare_exchange_weak(
        taken, taken + 1, std::memory_order_relaxed)) {}
    if (taken >= 6) return;
    try {
        Runtime::instance().self().getLogger().info(
            "FreeCamera writer: sample={} sameEntity={} orbit={} dx={} dy={} dz={}",
            taken, same, orbit, displacement[0], displacement[1], displacement[2]);
    } catch (...) {}
}
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
LL_TYPE_INSTANCE_HOOK(FreeCameraSetupHook, ll::memory::HookPriority::Normal, LevelRendererPlayer,
    &LevelRendererPlayer::setupCamera, void, mce::Camera& camera, float alpha) {
    origin(camera, alpha);
    (void)alpha;
    try {
        Zoom::instance().freeCameraView(mClientInstance, camera);
    } catch (...) {}
}
LL_TYPE_INSTANCE_HOOK(TurnHook, ll::memory::HookPriority::Normal, LocalPlayer,
    &LocalPlayer::_applyTurnDelta, void, Vec2 const& delta) {
    // While detached, vanilla still turns the camera; only the copy to the
    // player is withheld (see detachCameras).
    if (Zoom::instance().turnLook(*this, delta.x, delta.z)) {
        // Vanilla's look input also turns the head directly; undo that below.
        auto head = getEntityContext().tryGetComponent<ActorHeadRotationComponent>();
#ifdef LAMIUM_CAMERA_TRACE
        float before = head ? static_cast<float>(head->mYHeadRot) : 0.f;
#endif
        origin(delta);
        if (head) {
#ifdef LAMIUM_CAMERA_TRACE
            static TraceBudget budget;
            float after = head->mYHeadRot;
            if (after != before)
                traceFreelookSource(budget, 16, [&] { return std::format("head-turned-by-look before={} after={}", before, after); });
#endif
            Zoom::instance().keepHead(*this);
        }
        return;
    }
    float scale = Zoom::instance().sensitivity(*this);
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
// Stage 2: after vanilla HID extraction, withhold movement from the extracted
// output for the FreeCamera owner. Other players and Freelook pass through
// untouched; Freelook keeps its movement while looking around.
LL_STATIC_HOOK(ExtractFreeCameraInput, ll::memory::HookPriority::Normal,
    &ClientInputUpdateSystem::extractRawHIDInput, void,
    MovementAbilitiesComponent const& abilities, MoveInputComponent const& input,
    ActorDataFlagComponent const& flags, RawMoveInputComponent& raw,
    Optional<SneakingComponent const> sneaking, Optional<WasInWaterFlagComponent const> water) {
    origin(abilities, input, flags, raw, sneaking, water);
    try {
        Zoom::instance().consumeFreeCameraInput(input, raw);
    } catch (...) {}
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
    {FreeCameraSetupHook::hook, FreeCameraSetupHook::unhook},
    {ExtractFreeCameraInput::hook, ExtractFreeCameraInput::unhook},
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
    lookToggle = settings.camera.freelookToggle;
    freeCameraAllowed = settings.camera.freecamera;
    cancelLook();
    allowed = settings.camera.zoom;
    state.configure(settings.camera.magnification, settings.camera.wheelStep);
}
void Zoom::pressLook(IClientInstance& current) {
    // Freelook and FreeCamera share one session and never run together.
    if (lookOwner.load() == DetachedOwner::FreeCamera) return;
    // Toggle activation: a press always ends an active session, and a new
    // session never waits for a key release that this mode ignores.
    if (lookToggle && look.snapshot()) { releaseLook(); return; }
    if (!running || !lookAllowed || ui::ownsInput() || !gameplayScreen(current.getScreenName())
        || !current.getLocalPlayer()) return;
    auto* player = current.getLocalPlayer();
    if (!canDetachLook(*player)) return;
    client = &current;
    if (lookToggle) look.release();
    if (!look.begin(player->getRotation().x, player->getRotation().z, player->getRuntimeID().rawID)) return;
    lookOwner.store(DetachedOwner::Freelook);
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
void Zoom::pressFreeCamera(IClientInstance& current) {
    // Stage 1: rotation only, detached exactly like Freelook. Always toggles:
    // a press ends the FreeCamera session, the key release does nothing.
    if (lookOwner.load() == DetachedOwner::Freelook) return;
    if (look.snapshot()) { releaseLook(); return; }
    if (!running || !freeCameraAllowed || ui::ownsInput() || !gameplayScreen(current.getScreenName())
        || !current.getLocalPlayer()) return;
    auto* player = current.getLocalPlayer();
    if (!canDetachLook(*player)) return;
    auto ownerId = player->getRuntimeID().rawID;
    client = &current;
    look.release();
    if (!look.begin(player->getRotation().x, player->getRotation().z, ownerId)) return;
    lookOwner.store(DetachedOwner::FreeCamera);
    { std::lock_guard lock{freeInputMutex}; freeCameraInput = {}; hasFreeCameraInput = false; freeMoveSamples = 0; freeMotionTimed = false; }
    // The displacement session never survives a previous one; a stale session
    // cancels the whole activation rather than flying from a wrong origin.
    motion.cancel();
    freeMotionOwner.store(0);
    if (!ownerId || !motion.begin(ownerId)) { cancelLook(); return; }
    freeMotionOwner.store(ownerId);
#ifdef LAMIUM_CAMERA_TRACE
    traceLook(LookTraceStage::Begin, player->getRotation().x, player->getRotation().z);
    try { Runtime::instance().self().getLogger().info("FreeCamera body: begin head={}", player->getYHeadRot()); } catch (...) {}
#endif
    try {
        lockedHead = player->getYHeadRot();
        detachCameras(*player);
        if (detachedCameras.empty()) throw std::runtime_error("FreeCamera has no detached camera entity");
        takeFreeCameraOffset(*player, detachedCameras.back().entity);
    } catch (...) {
        cancelLook();
        Runtime::instance().self().getLogger().error("FreeCamera could not detach the camera");
    }
}
void Zoom::releaseLookKey() {
    // A Toggle-owned session (FreeCamera, or Freelook in toggle mode) ignores
    // the key release; only a held Freelook ends here.
    if (lookToggle || lookOwner.load() != DetachedOwner::Freelook) return;
    releaseLook();
}
void Zoom::consumeFreeCameraInput(MoveInputComponent const& input, RawMoveInputComponent& raw) {
    // Only the FreeCamera owner loses movement; Freelook keeps vanilla motion.
    if (lookOwner.load() != DetachedOwner::FreeCamera || !look.snapshot()) return;
    auto* current = client.load();
    if (!running || !freeCameraAllowed || !current) return;
    // The extraction runs once per local player; ignore other viewports.
    if (ClientMoveInputHandler::getMoveInput(*current) != &input) return;
    // Read the stash before consumption clears the extracted flags.
    auto axes = camera::freecameraInputAxes(raw);
    camera::consumeMovement(raw);
    std::lock_guard lock{freeInputMutex};
    freeCameraInput = axes;
    hasFreeCameraInput = true;
    if (freeMoveSamples < 1000000) ++freeMoveSamples;
}
void Zoom::logFreeCameraSamples() {
    unsigned samples = 0;
    { std::lock_guard lock{freeInputMutex}; samples = freeMoveSamples; }
    try {
        Runtime::instance().self().getLogger().info("FreeCamera movement: consumedSamples={}", samples);
    } catch (...) {}
}
void Zoom::endFreeCameraMotion(bool wasFreeCamera) {
    if (!wasFreeCamera) return;
    logFreeCameraSamples();
    motion.cancel();
    freeMotionOwner.store(0);
    auto* current = client.load();
    restoreFreeCameraOffset(current ? current->getLocalPlayer() : nullptr);
    std::lock_guard lock{freeInputMutex};
    freeMotionTimed = false;
    hasDisplacement = false;
}
void Zoom::writeFreeCameraOffset() {
    if (lookOwner.load() != DetachedOwner::FreeCamera) return;
    DetachedCameraMotion::Vector displacement{};
    {
        std::lock_guard lock{freeInputMutex};
        if (!hasDisplacement) return;
        displacement = lastDisplacement;
    }
    auto* current = client.load();
    if (!current || !current->getLocalPlayer() || detachedCameras.empty()) return;
    try {
        auto& registry = current->getLocalPlayer()->getEntityContext().getRegistry();
        auto entity = detachedCameras.back().entity;
        // F5 may hand the active role to another entity. Migrate the session
        // instead of steering a stale rig.
        if (!registry.valid(entity) || !hasActiveCamera(registry, entity)) {
            try {
                migrateFreeCamera(*current->getLocalPlayer());
            } catch (...) { return; }
            if (detachedCameras.empty()) return;
            entity = detachedCameras.back().entity;
            if (!registry.valid(entity)) return;
        }
        auto* offset = registry.try_get<MinecraftCamera::CameraOffsetComponent>(entity);
        if (!offset) return;
        // F5 may morph the rig (direct-look converts to orbit or back), so
        // read the live shape instead of the activation-time flag.
        bool orbit = registry.try_get<MinecraftCamera::CameraOrbitComponent>(entity) != nullptr;
        bool same = savedOffset.active && savedOffset.entity == entity;
#ifdef LAMIUM_CAMERA_TRACE
        traceWriter(same, orbit, displacement);
#endif
        if (same && orbit) {
            // Third person: swing the pivot, keep the vanilla entity offset.
            (*offset->mPivot).x = savedOffset.px + static_cast<float>(displacement[0]);
            (*offset->mPivot).y = savedOffset.py + static_cast<float>(displacement[1]);
            (*offset->mPivot).z = savedOffset.pz + static_cast<float>(displacement[2]);
        } else if (same) {
            (*offset->mEntityOffset).x = static_cast<float>(displacement[0]);
            (*offset->mEntityOffset).y = static_cast<float>(displacement[1]);
            (*offset->mEntityOffset).z = static_cast<float>(displacement[2]);
        }
        // A different entity (perspective swap) keeps vanilla values until a
        // re-take lands; writing blind would steer the wrong rig.
    } catch (...) {}
}
void Zoom::releaseLook() {
    auto owner = lookOwner.load();
    look.release();
    lookOwner.store(DetachedOwner::None);
    endFreeCameraMotion(owner == DetachedOwner::FreeCamera);
    endLookCamera();
}
void Zoom::cancelLook() {
    auto owner = lookOwner.load();
    look.cancel();
    lookOwner.store(DetachedOwner::None);
    endFreeCameraMotion(owner == DetachedOwner::FreeCamera);
    endLookCamera();
}
bool Zoom::freeCameraView(IClientInstance const& renderedClient, mce::Camera& camera) {
    // A different viewport must neither advance nor translate the session.
    // This also keeps the angular session alive or cancels it on violations.
    if (!lookAnglesFor(renderedClient) || lookOwner.load() != DetachedOwner::FreeCamera) {
#ifdef LAMIUM_CAMERA_TRACE
        traceFreeCamera(0, 0, 0, 0);
#endif
        return false;
    }
    DetachedCameraMotion::Vector input{};
    double seconds = 0;
    {
        std::lock_guard lock{freeInputMutex};
        if (!hasFreeCameraInput) {
#ifdef LAMIUM_CAMERA_TRACE
            traceFreeCamera(1, 0, 0, 0);
#endif
            return false;
        }
        input = freeCameraInput;
        auto now = std::chrono::steady_clock::now();
        if (freeMotionTimed)
            seconds = std::chrono::duration<double>(now - freeMotionTime).count();
        freeMotionTime = now;
        freeMotionTimed = true;
    }
    if (camera.viewMatrixStack->stack->empty()) return false;
    auto view = *camera.viewMatrixStack->top()._m;
    for (int column = 0; column < 4; ++column)
        for (int row = 0; row < 4; ++row)
            if (!std::isfinite(view[column][row])) return false;
    // Yaw-relative basis from the fresh vanilla view: horizontal right and
    // camera forward, world up for Space/Shift. The row layout matches the
    // validated position probe, which shifted the image toward camera-right.
    auto horizontal = [](double x, double z) {
        double length = std::hypot(x, z);
        if (!(length > 1e-6)) return DetachedCameraMotion::Vector{};
        return DetachedCameraMotion::Vector{x / length, 0, z / length};
    };
    auto right = horizontal(view[0][0], view[2][0]);
    auto forward = horizontal(-view[0][2], -view[2][2]);
    constexpr DetachedCameraMotion::Vector up{0, 1, 0};
    // Interim speed until L-26 makes it a setting; roughly creative flight.
    constexpr double speed = 20.0;
    auto owner = freeMotionOwner.load();
    if (!owner || !motion.advance(owner, input, right, up, forward, speed, seconds)) {
#ifdef LAMIUM_CAMERA_TRACE
        traceFreeCamera(2, 0, 0, 0);
#endif
        return false;
    }
    auto displacement = motion.snapshot();
    if (!displacement) return false;
    double dx = (*displacement)[0], dy = (*displacement)[1], dz = (*displacement)[2];
    if (!std::isfinite(dx) || !std::isfinite(dy) || !std::isfinite(dz)) return false;
    // Post-setup view edits never reached the detached render, so the render
    // hook only stages the displacement here; the UI-render writer carries it
    // into the camera entity's own offset for vanilla to consume.
    {
        std::lock_guard lock{freeInputMutex};
        lastDisplacement = *displacement;
        hasDisplacement = dx * dx + dy * dy + dz * dz >= 1e-18;
    }
#ifdef LAMIUM_CAMERA_TRACE
    traceFreeCamera(3, dx, dy, dz);
#endif
    return true;
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
    // The owner decides which enable flag keeps the shared session alive, so
    // disabling Freelook does not end FreeCamera and vice versa.
    bool allowed = lookOwner.load() == DetachedOwner::FreeCamera ? freeCameraAllowed.load() : lookAllowed.load();
    auto* current = client.load();
    if (!running || !allowed || !current || ui::ownsInput()
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
#else
    (void)pitchDelta;
    (void)yawDelta;
#endif
    return true;
}
// Vanilla turns the local head toward the detached camera outside any setter.
// Rewrite the head component (current and previous, so interpolation cannot
// swing it) to the yaw captured when the detached look began.
void Zoom::keepHead(LocalPlayer& player) {
    auto kept = lockedHeadFor(player);
    if (!kept) return;
    if (auto head = player.getEntityContext().tryGetComponent<ActorHeadRotationComponent>()) {
        head->mYHeadRot = *kept;
        head->mYHeadRotO = *kept;
    }
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
            if (lookAngles()) {
                // The head may also be turned outside the look-input path.
                if (auto* current = client.load(); current && current->getLocalPlayer())
                    keepHead(*current->getLocalPlayer());
                try { writeFreeCameraOffset(); } catch (...) {}
            }
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
