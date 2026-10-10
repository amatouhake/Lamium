#include "features/camera/CameraSessions.h"
#include <algorithm>
#include <numbers>
#include "features/camera/CameraInteraction.h"
#include "features/camera/FreeCameraCulling.h"
#include "features/camera/CameraTrace.h"
#include "features/camera/DetachedCameraRig.h"
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
#include "mc/client/game/ClientInputCallbacks.h"
#include "mc/client/game/IClientInstance.h"
#include "mc/client/game/ClientInstance.h"
#include "ll/api/service/TargetedBedrock.h"
#include "mc/client/options/IOptionRegistry.h"
#include "mc/deps/shared_types/v1_21_100/camera/PlayerViewMode.h"
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
#include "mc/deps/vanilla_camera/CameraAPI.h"
#include "mc/legacy/ActorRuntimeID.h"
#include "mc/world/actor/ActorFlags.h"
#include "ui/SettingsScreen.h"
#include <cmath>

namespace lamium {
namespace {
bool canDetachLook(LocalPlayer const& player) {
    // Leave an existing charge/eating action with vanilla. Do not finish or
    // release it on the player's behalf when entering a detached camera.
    return player.isAlive() && !player.isSleeping() && !player.getVehicle()
        && player.hasRuntimeID() && !player.getStatusFlag(ActorFlags::Usingitem);
}
LL_TYPE_INSTANCE_HOOK(FovHook, ll::memory::HookPriority::Normal, LevelRendererPlayer,
    &LevelRendererPlayer::getFov, float, float alpha, bool variable) {
    return CameraSessions::instance().fov(mClientInstance, origin(alpha, variable));
}
LL_TYPE_INSTANCE_HOOK(FreeCameraSetupHook, ll::memory::HookPriority::Normal, LevelRendererPlayer,
    &LevelRendererPlayer::setupCamera, void, mce::Camera& camera, float alpha) {
    origin(camera, alpha);
    (void)alpha;
    try {
        CameraSessions::instance().recordRenderEye(camera);
        CameraSessions::instance().freeCameraView(mClientInstance, camera);
    } catch (...) {}
}
LL_TYPE_INSTANCE_HOOK(FreeCameraInterpolatedPosition, ll::memory::HookPriority::Normal, CameraAPI,
    &CameraAPI::$tryGetActorInterpolatedPosition, std::optional<Vec3>, WeakRef<EntityContext> actor, float alpha) {
    auto position = origin(actor, alpha);
    try {
        auto& zoom = CameraSessions::instance();
        auto* player = mClientInstance.getLocalPlayer();
        if (position && player && player->hasRuntimeID()
            && zoom.freeCameraFor(mClientInstance,player->getRuntimeID().rawID) && _getActor(actor) == player) {
            auto eye = player->getEyePos();
            auto body = player->getPosition();
            auto renderEye = camera::FreeCameraPosition::interpolatedEye(
                {eye.x, eye.y, eye.z}, {body.x, body.y, body.z}, {position->x, position->y, position->z});
            if (renderEye) zoom.writeFreeCameraOffset(renderEye);
        }
    } catch (...) {}
    return position;
}
LL_TYPE_INSTANCE_HOOK(TurnHook, ll::memory::HookPriority::Normal, LocalPlayer,
    &LocalPlayer::_applyTurnDelta, void, Vec2 const& delta) {
    float scale = CameraSessions::instance().sensitivity(*this);
    Vec2 applied{delta.x * scale, delta.z * scale};
    // While detached, vanilla still turns the camera; only the copy to the
    // player is withheld (see detachCameras). Zoom slows that path too.
    if (CameraSessions::instance().turnLook(*this, delta.x, delta.z)) {
        // Vanilla's look input also turns the head directly; undo that below.
        auto head = getEntityContext().tryGetComponent<ActorHeadRotationComponent>();
        float before = head ? static_cast<float>(head->mYHeadRot) : 0.f;
        origin(applied);
        if (head) {
            if (float after = head->mYHeadRot; after != before) camera::trace::headTurned(before, after);
            CameraSessions::instance().keepHead(*this);
        }
        return;
    }
    origin(applied);
}
LL_TYPE_INSTANCE_HOOK(DimensionHook, ll::memory::HookPriority::Normal, LevelRendererPlayer,
    &LevelRendererPlayer::$onWillChangeDimension, void, Player& player) {
    CameraSessions::instance().reset(true);
    origin(player);
}
LL_TYPE_INSTANCE_HOOK(FocusHook, ll::memory::HookPriority::Normal, MinecraftGame,
    &MinecraftGame::$onAppFocusLost, void) {
    CameraSessions::instance().suspendInput();
    origin();
}
// Perspective is locked while FreeCamera owns the session. F5 would
// hand the render to a rig the session never detached, so swallow the press.
LL_STATIC_HOOK(PerspectiveLockHook, ll::memory::HookPriority::Normal,
    &ClientInputCallbacks::handleTogglePerspectiveButtonPress, void, IClientInstance& client) {
    if (CameraSessions::instance().blocksPerspective()) return;
    origin(client);
}
// After vanilla HID extraction, withhold movement from the extracted
// output for the FreeCamera owner. Other players and Freelook pass through
// untouched; Freelook keeps its movement while looking around.
LL_STATIC_HOOK(ExtractFreeCameraInput, ll::memory::HookPriority::Normal,
    &ClientInputUpdateSystem::extractRawHIDInput, void,
    MovementAbilitiesComponent const& abilities, MoveInputComponent const& input,
    ActorDataFlagComponent const& flags, RawMoveInputComponent& raw,
    Optional<SneakingComponent const> sneaking, Optional<WasInWaterFlagComponent const> water) {
    origin(abilities, input, flags, raw, sneaking, water);
    try {
        CameraSessions::instance().consumeFreeCameraInput(input, raw);
    } catch (...) {}
}
struct HookEntry {
    int (*install)(bool);
    bool (*remove)(bool);
    bool installed = false;
};
HookEntry hooks[] = {
    {FovHook::hook, FovHook::unhook},
    {PerspectiveLockHook::hook, PerspectiveLockHook::unhook},
    {FreeCameraSetupHook::hook, FreeCameraSetupHook::unhook},
    {FreeCameraInterpolatedPosition::hook, FreeCameraInterpolatedPosition::unhook},
    {ExtractFreeCameraInput::hook, ExtractFreeCameraInput::unhook},
    {TurnHook::hook, TurnHook::unhook},
    {DimensionHook::hook, DimensionHook::unhook},
    {FocusHook::hook, FocusHook::unhook}
};
}
CameraSessions& CameraSessions::instance() { static CameraSessions value; return value; }
float CameraSessions::fov(IClientInstance const& renderedClient, float base) const {
    return running && client.load() == &renderedClient ? state.fov(base) : base;
}
std::optional<float> CameraSessions::magnification(IClientInstance const& current) const {
    if (!running || client.load() != &current || !state.held()) return {};
    return state.level();
}
float CameraSessions::sensitivity(LocalPlayer const& player) const {
    auto* current = client.load();
    return running && current && current->getLocalPlayer() == &player ? state.sensitivity() : 1.f;
}
std::optional<CameraSessions::ViewRay> CameraSessions::detachedViewRay(IClientInstance& current) {
    if (lookOwner.load() == DetachedOwner::None) return {};
    auto angles = lookAnglesFor(current);
    auto* player = current.getLocalPlayer();
    if (!angles || !player) return {};
    auto eye = player->getEyePos();
    ViewRay ray{eye.x, eye.y, eye.z, 0, 0, 0};
    if (lookOwner.load() == DetachedOwner::FreeCamera) {
        if (auto displacement = motion.snapshot()) {
            std::lock_guard lock{freeInputMutex};
            auto position = freeCameraPosition.position({eye.x, eye.y, eye.z}, *displacement);
            if (!position) return {};
            ray.x = (*position)[0];
            ray.y = (*position)[1];
            ray.z = (*position)[2];
        }
    }
    {
        std::lock_guard lock{freeInputMutex};
        if (hasForward) {
            ray.dx = lastForward[0];
            ray.dy = lastForward[1];
            ray.dz = lastForward[2];
        }
    }
    if (ray.dx == 0 && ray.dy == 0 && ray.dz == 0) {
        // No rendered frame yet: fall back to the session's angles.
        // Minecraft angles: yaw 0 faces +Z, positive pitch looks down.
        double pitch = angles->pitch * std::numbers::pi / 180, yaw = angles->yaw * std::numbers::pi / 180;
        ray.dx = -std::sin(yaw) * std::cos(pitch);
        ray.dy = -std::sin(pitch);
        ray.dz = std::cos(yaw) * std::cos(pitch);
    }
    if (!std::isfinite(ray.x + ray.y + ray.z + ray.dx + ray.dy + ray.dz)) return {};
    return ray;
}
std::optional<CameraSessions::Pose> CameraSessions::freeCameraPose(IClientInstance& current) {
    if (!blocksPerspective()) return {};
    auto ray = detachedViewRay(current);
    auto* player = current.getLocalPlayer();
    if (!ray || !player) return {};
    double feet = player->getFeetPos().y - player->getEyePos().y;
    double length = std::sqrt(ray->dx * ray->dx + ray->dy * ray->dy + ray->dz * ray->dz);
    if (!std::isfinite(feet) || !(length > 0)) return {};
    // Angles from the drawn view direction (yaw 0 faces +Z, positive pitch looks down).
    auto yaw = static_cast<float>(std::atan2(-ray->dx, ray->dz) * 180 / std::numbers::pi);
    auto pitch = static_cast<float>(-std::asin(std::clamp(ray->dy / length, -1.0, 1.0)) * 180 / std::numbers::pi);
    return Pose{ray->x, ray->y + feet, ray->z, yaw, pitch};
}
std::optional<DetachedLookState::Angles> CameraSessions::lookAnglesFor(IClientInstance const& renderedClient) {
    // A different viewport must neither consume nor cancel the owner's session.
    if (client.load() != &renderedClient) return {};
    return lookAngles();
}
#if defined(LAMIUM_CAMERA_PROBE) || defined(LAMIUM_CAMERA_POSITION_PROBE)
bool CameraSessions::viewProbeActive() const {
    auto* current = client.load();
    return running && state.held() && current && gameplayScreen(current->getScreenName());
}
#endif
void CameraSessions::configure(Settings const& settings) {
    // Settings change while a session may run (FreeCamera survives menus),
    // so only the modes are updated here.
    lookToggle = settings.camera.freelookToggle;
    lookStartPerspective = settings.camera.freelookStartPerspective;
    freeToggle = settings.camera.freeCameraToggle;
    freeSpeed = camera::normalizeFlightSpeed(settings.camera.freeCameraSpeed);
    freeWorldFixed = settings.camera.freeCameraWorldFixed;
    freeLeaveOnHit = settings.camera.freeCameraLeaveOnHit;
    zoomToggle = settings.camera.zoomToggle;
    state.configure(settings.camera.magnification);
}
bool CameraSessions::wanted(Session session) const {
    switch (session) {
    case Session::Zoom: return wantZoom.load();
    case Session::Freelook: return wantLook.load();
    default: return wantFree.load();
    }
}
void CameraSessions::toggleWanted(Session session) {
    auto& flag = session == Session::Zoom ? wantZoom : session == Session::Freelook ? wantLook : wantFree;
    flag = !flag.load();
    reconcile();
}
void CameraSessions::suspendInput() {
    state.release();
    { std::lock_guard lock{freeInputMutex}; freeCameraInput = {}; freeCameraSprint.cancel(); hasFreeCameraInput = false; freeMotionTimed = false; }
    if (lookOwner.load() == DetachedOwner::Freelook) cancelLook();
}
void CameraSessions::reconcile() {
    if (!running) return;
    auto instance = ll::service::getClientInstance();
    if (!instance) return;
    IClientInstance& current = *instance;
    auto* player = current.getLocalPlayer();
    if (player && !player->isAlive()) { wantZoom = false; wantLook = false; wantFree = false; }
    bool gameplay = player && !ui::ownsInput() && gameplayScreen(current.getScreenName());
    if (!gameplay) {
        std::lock_guard lock{freeInputMutex};
        freeCameraSprint.cancel();
        freeCameraInput = {};
        freeMotionTimed = false;
    }
    bool zoomOn = wantZoom.load() && gameplay;
    if (zoomOn && !state.held()) { client = &current; state.press(); }
    else if (!zoomOn && state.held()) state.release();
    auto owner = lookOwner.load();
    if ((owner == DetachedOwner::Freelook && (!wantLook.load() || wantFree.load()))
        || (owner == DetachedOwner::FreeCamera && !wantFree.load()))
        releaseLook();
    if (!wantFree.load() && pendingFreeCamera.load()) abortPendingTravel();
    if (!gameplay || lookOwner.load() != DetachedOwner::None || pendingFreeCamera.load()) return;
    if (wantFree.load()) startFreeCamera(current);
    else if (wantLook.load()) beginLook(current);
}
void CameraSessions::pressLook(IClientInstance& current) {
    if (!running) return;
    client = &current;
    // Toggle activation flips the wanted state; Hold wants it until release.
    wantLook = lookToggle.load() ? !wantLook.load() : true;
    reconcile();
}
bool CameraSessions::beginLook(IClientInstance& current) {
    // Freelook and FreeCamera share one session and never run together.
    auto* player = current.getLocalPlayer();
    if (!player || !canDetachLook(*player)) return false;
    client = &current;
    if (lookToggle) look.release();
    if (!look.begin(player->getRotation().x, player->getRotation().z, player->getRuntimeID().rawID)) return false;
    lookOwner.store(DetachedOwner::Freelook);
#ifdef LAMIUM_CAMERA_TRACE
    camera::trace::look(camera::trace::LookStage::Begin, player->getRotation().x, player->getRotation().z);
    try { Runtime::instance().self().getLogger().info("Freelook body: begin head={}", player->getYHeadRot()); } catch (...) {}
#endif
    try {
        lockedHead = player->getYHeadRot();
        camera::rig::detach(*player);
        lookPerspective.store(current.getOptions().getPlayerViewPerspective());
        current.getOptions().setPlayerViewPerspective(lookStartPerspective.load());
    } catch (...) {
        cancelLook();
        wantLook = false;
        Runtime::instance().self().getLogger().error("Freelook could not detach the camera");
        return false;
    }
    return true;
}
void CameraSessions::syncLookCameras(LocalPlayer& player) {
    if (lookOwner.load() != DetachedOwner::Freelook || !look.snapshot()) return;
    // F5 can activate another rig while Freelook is running. Detach it before
    // vanilla look input can copy the new camera orientation to the player.
    camera::rig::detach(player);
}
void CameraSessions::pressFreeCamera(IClientInstance& current) {
    // A Hold activation wants the camera only until its key is released.
    // Either mode takes over from Freelook and ends pending perspective travel.
    if (!running) return;
    client = &current;
    wantFree = freeToggle.load() ? !wantFree.load() : true;
    reconcile();
}
void CameraSessions::releaseFreeCameraKey() {
    if (freeToggle.load()) return;
    wantFree = false;
    reconcile();
}
void CameraSessions::bodyHit(int event) {
    if (!running || !freeLeaveOnHit.load() || !wantFree.load()) return;
    wantFree = false;
    try { Runtime::instance().self().getLogger().info("FreeCamera left: the body was hit (event {})", event); } catch (...) {}
}
bool CameraSessions::startFreeCamera(IClientInstance& current) {
    auto* player = current.getLocalPlayer();
    if (!player) return false;
    freePerspective.store(-1);
    if (!ensureFirstPerson(current, *player)) return true; // Travel completes it.
    if (beginFreeCameraSession(current, *player)) return true;
    wantFree = false;
    return false;
}
bool CameraSessions::ensureFirstPerson(IClientInstance& current, LocalPlayer& player) {
    try {
        if (camera::rig::firstPerson(player)) return true;
    } catch (...) {}
    // Travel: set first person directly (no cycle through front-third); the
    // frame listener completes activation once the rig arrives. No session
    // exists yet, so nothing is locked.
    freePerspective.store(-1);
    client = &current;
    pendingFreeCamera.store(true);
    {
        std::lock_guard lock{freeInputMutex};
        freeTravelStart = std::chrono::steady_clock::now();
    }
    try {
        using Mode = SharedTypes::v1_21_100::PlayerViewMode;
        freePerspective.store(current.getOptions().getPlayerViewPerspective());
        current.getOptions().setPlayerViewPerspective(static_cast<int>(Mode::FirstPerson));
    } catch (...) {
        pendingFreeCamera.store(false);
    }
    return false;
}
void CameraSessions::recordRenderEye(mce::Camera& camera) {
    auto eye = *camera.mPosition;
    if (!std::isfinite(eye.x) || !std::isfinite(eye.y) || !std::isfinite(eye.z)) return;
    DetachedCameraMotion::Vector forward{};
    bool haveForward = false;
    if (!camera.viewMatrixStack->stack->empty()) {
        auto view = *camera.viewMatrixStack->top()._m;
        double fx = -view[0][2], fy = -view[1][2], fz = -view[2][2];
        double length = std::sqrt(fx * fx + fy * fy + fz * fz);
        if (std::isfinite(length) && length > 1e-6) {
            forward = {fx / length, fy / length, fz / length};
            haveForward = true;
        }
    }
    std::lock_guard lock{freeInputMutex};
    prevEye = lastEye;
    lastEye = {eye.x, eye.y, eye.z};
    lastForward = forward;
    hasForward = haveForward;
}
void CameraSessions::pollFreeTravel() {
    auto* current = client.load();
    if (!current || !current->getLocalPlayer()) { abortPendingTravel(); return; }
    std::chrono::steady_clock::time_point start;
    DetachedCameraMotion::Vector eye{}, prev{};
    {
        std::lock_guard lock{freeInputMutex};
        start = freeTravelStart;
        eye = lastEye;
        prev = prevEye;
    }
    bool settled = false;
    try {
        settled = camera::rig::firstPerson(*current->getLocalPlayer());
    } catch (...) { abortPendingTravel(); return; }
    auto now = std::chrono::steady_clock::now();
    if (now - start > std::chrono::seconds(3)) {
        // Blend never settled; begin anyway.
        pendingFreeCamera.store(false);
        if (!beginFreeCameraSession(*current, *current->getLocalPlayer())) {
            wantFree = false;
            restoreFreePerspective(*current);
        }
        return;
    }
    if (!settled) return;
    double dx = eye[0] - prev[0], dy = eye[1] - prev[1], dz = eye[2] - prev[2];
    if (dx * dx + dy * dy + dz * dz > 1e-6) return; // Blend still running.
    pendingFreeCamera.store(false);
    if (!beginFreeCameraSession(*current, *current->getLocalPlayer())) {
        wantFree = false;
        restoreFreePerspective(*current);
    }
}
void CameraSessions::abortPendingTravel() {
    if (!pendingFreeCamera.load()) return;
    pendingFreeCamera.store(false);
    auto* current = client.load();
    if (current) {
        try { restoreFreePerspective(*current); } catch (...) {}
    }
    wantFree = false;
}
void CameraSessions::restoreFreePerspective(IClientInstance& current) {
    int saved = freePerspective.load();
    freePerspective.store(-1);
    if (saved < 0) return;
    try { current.getOptions().setPlayerViewPerspective(saved); } catch (...) {}
}
bool CameraSessions::beginFreeCameraSession(IClientInstance& current, LocalPlayer& player) {
    if (!running || ui::ownsInput() || !gameplayScreen(current.getScreenName()))
        return false;
    if (!canDetachLook(player)) return false;
    auto ownerId = player.getRuntimeID().rawID;
    client = &current;
    look.release();
    if (!look.begin(player.getRotation().x, player.getRotation().z, ownerId)) return false;
    lookOwner.store(DetachedOwner::FreeCamera);
    { std::lock_guard lock{freeInputMutex}; freeCameraInput = {}; freeCameraSprint.reset(); hasFreeCameraInput = false; freeMoveSamples = 0; freeMotionTimed = false; }
    // The displacement session never survives a previous one; a stale session
    // cancels the whole activation rather than flying from a wrong origin.
    motion.cancel();
    freeMotionOwner.store(0);
    if (!ownerId || !motion.begin(ownerId)) { cancelLook(); return false; }
    freeMotionOwner.store(ownerId);
    freeInterpolatedPosition = false;
    auto eye = player.getEyePos();
    bool positionReady = false;
    {
        std::lock_guard lock{freeInputMutex};
        positionReady = freeCameraPosition.begin({eye.x, eye.y, eye.z}, freeWorldFixed.load());
        lastDisplacement = {};
        hasDisplacement = positionReady;
    }
    if (!positionReady) { cancelLook(); return false; }
#ifdef LAMIUM_CAMERA_TRACE
    camera::trace::look(camera::trace::LookStage::Begin, player.getRotation().x, player.getRotation().z);
    try { Runtime::instance().self().getLogger().info("FreeCamera body: begin head={}", player.getYHeadRot()); } catch (...) {}
#endif
    try {
        lockedHead = player.getYHeadRot();
        camera::rig::detach(player);
        camera::rig::takeFreeCamera(player);
    } catch (...) {
        cancelLook();
        Runtime::instance().self().getLogger().error("FreeCamera could not detach the camera");
        return false;
    }
    return true;
}
bool CameraSessions::blocksPerspective() const {
    // Input-thread safe: atomics and a mutex-guarded snapshot only.
    if (lookOwner.load() != DetachedOwner::FreeCamera) return false;
    return look.snapshot().has_value();
}
void CameraSessions::releaseLookKey() {
    // Toggle mode ignores the key release; a held Freelook is no longer wanted.
    if (lookToggle.load()) return;
    wantLook = false;
    reconcile();
}
void CameraSessions::consumeFreeCameraInput(MoveInputComponent const& input, RawMoveInputComponent& raw) {
    // Only the FreeCamera owner loses movement; Freelook keeps vanilla motion.
    if (lookOwner.load() != DetachedOwner::FreeCamera || !look.snapshot()) return;
    auto* current = client.load();
    if (!running || !current) return;
    // The extraction runs once per local player; ignore other viewports.
    if (ClientMoveInputHandler::getMoveInput(*current) != &input) return;
    // Read the stash before consumption clears the extracted flags.
    auto axes = camera::freecameraInputAxes(raw);
    bool sprint = camera::freecameraSprintHeld(raw);
    camera::consumeMovement(raw);
    std::lock_guard lock{freeInputMutex};
    freeCameraInput = axes;
    freeCameraSprint.update(axes[2], sprint, !ui::ownsInput() && gameplayScreen(current->getScreenName()));
    hasFreeCameraInput = true;
    if (freeMoveSamples < 1000000) ++freeMoveSamples;
}
void CameraSessions::logFreeCameraSamples() {
    unsigned samples = 0;
    { std::lock_guard lock{freeInputMutex}; samples = freeMoveSamples; }
    try {
        Runtime::instance().self().getLogger().info("FreeCamera movement: consumedSamples={}", samples);
    } catch (...) {}
}
void CameraSessions::endFreeCameraMotion(bool wasFreeCamera) {
    if (!wasFreeCamera) return;
#ifdef LAMIUM_CAMERA_TRACE
    logFreeCameraSamples();
#endif
    motion.cancel();
    freeMotionOwner.store(0);
    auto* current = client.load();
    camera::rig::restoreFreeCamera(current ? current->getLocalPlayer() : nullptr);
    if (current) {
        try { restoreFreePerspective(*current); } catch (...) {}
    }
    std::lock_guard lock{freeInputMutex};
    freeMotionTimed = false;
    freeCameraSprint.reset();
    freeCameraInput = {};
    hasFreeCameraInput = false;
    hasDisplacement = false;
    freeCameraPosition.reset();
    freeInterpolatedPosition = false;
}
void CameraSessions::writeFreeCameraOffset(std::optional<DetachedCameraMotion::Vector> renderEye) {
    if (lookOwner.load() != DetachedOwner::FreeCamera) return;
    // World compensation must use the same interpolated body position that
    // vanilla consumes before applying its camera entity offset.
    if (!renderEye && freeWorldFixed.load() && freeInterpolatedPosition.load()) return;
    auto* current = client.load();
    if (!current || !current->getLocalPlayer() || !camera::rig::detached()) return;
    auto eye = current->getLocalPlayer()->getEyePos();
    DetachedCameraMotion::Vector displacement{};
    {
        std::lock_guard lock{freeInputMutex};
        if (!hasDisplacement) return;
        auto offset = freeCameraPosition.offset(renderEye.value_or(DetachedCameraMotion::Vector{eye.x, eye.y, eye.z}),
            lastDisplacement, freeWorldFixed.load());
        if (!offset) return;
        displacement = *offset;
    }
    try {
        bool same = camera::rig::writeOffset(*current->getLocalPlayer(), displacement);
        if (same && renderEye && !freeInterpolatedPosition.exchange(true)) {
            Runtime::instance().self().getLogger().info("FreeCamera position: native interpolation writer reached");
        }
    } catch (...) {}
}
void CameraSessions::releaseLook() {
    auto owner = lookOwner.load();
    look.release();
    lookOwner.store(DetachedOwner::None);
    endFreeCameraMotion(owner == DetachedOwner::FreeCamera);
    endLookCamera();
}
void CameraSessions::cancelLook() {
    auto owner = lookOwner.load();
    look.cancel();
    lookOwner.store(DetachedOwner::None);
    endFreeCameraMotion(owner == DetachedOwner::FreeCamera);
    endLookCamera();
}
bool CameraSessions::freeCameraView(IClientInstance const& renderedClient, mce::Camera& camera) {
    // A different viewport must neither advance nor translate the session.
    // This also keeps the angular session alive or cancels it on violations.
    if (!lookAnglesFor(renderedClient) || lookOwner.load() != DetachedOwner::FreeCamera) {
        camera::trace::freeCamera(0, 0, 0, 0);
        return false;
    }
    DetachedCameraMotion::Vector input{};
    bool sprint = false;
    double seconds = 0;
    {
        std::lock_guard lock{freeInputMutex};
        if (!hasFreeCameraInput) {
            camera::trace::freeCamera(1, 0, 0, 0);
            return false;
        }
        input = freeCameraInput;
        sprint = freeCameraSprint.active();
        // FreeCamera stays through menus (L-27); it must not keep flying on
        // the last movement keys while a screen owns input.
        if (auto* current = client.load(); !current || ui::ownsInput() || !gameplayScreen(current->getScreenName())) {
            input = {};
            freeCameraInput = {};
            freeCameraSprint.cancel();
            sprint = false;
        }
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
    double speed = freeSpeed.load();
    auto owner = freeMotionOwner.load();
    if (!owner || !motion.advance(owner, input, right, up, forward, speed, seconds, sprint)) {
        camera::trace::freeCamera(2, 0, 0, 0);
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
        hasDisplacement = true;
    }
    camera::trace::freeCamera(3, dx, dy, dz);
    return true;
}
void CameraSessions::endLookCamera() {
    auto* current = client.load();
#ifdef LAMIUM_CAMERA_TRACE
    if (auto* player = current ? current->getLocalPlayer() : nullptr; player && camera::rig::detached()) {
        try {
            Runtime::instance().self().getLogger().info("Freelook body: end pitch={} yaw={} head={}",
                player->getRotation().x, player->getRotation().z, player->getYHeadRot());
        } catch (...) {}
    }
#endif
    camera::rig::restore(current ? current->getLocalPlayer() : nullptr);
    int saved = lookPerspective.exchange(-1);
    if (current && saved >= 0) {
        try { current->getOptions().setPlayerViewPerspective(saved); } catch (...) {}
    }
}
std::optional<DetachedLookState::Angles> CameraSessions::lookAngles() {
    if (!look.snapshot()) return {};
    auto* current = client.load();
    if (!running || !current || !current->getLocalPlayer()) {
        cancelLook();
        return {};
    }
    // A menu pauses Freelook (it resumes when wanted); FreeCamera keeps its
    // position through menus, settings and focus changes (L-27).
    bool menu = ui::ownsInput() || !gameplayScreen(current->getScreenName());
    if (menu && lookOwner.load() != DetachedOwner::FreeCamera) {
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
bool CameraSessions::turnLook(LocalPlayer& player, float pitchDelta, float yawDelta) {
    auto* current = client.load();
    if (!current || current->getLocalPlayer() != &player) return false;
    if (!lookAngles()) return false;
    try { syncLookCameras(player); } catch (...) { cancelLook(); return false; }
    camera::trace::look(camera::trace::LookStage::Turn, pitchDelta, yawDelta);
    return true;
}
// Vanilla turns the local head toward the detached camera outside any setter.
// Rewrite the head component (current and previous, so interpolation cannot
// swing it) to the yaw captured when the detached look began.
void CameraSessions::keepHead(LocalPlayer& player) {
    auto kept = lockedHeadFor(player);
    if (!kept) return;
    if (auto head = player.getEntityContext().tryGetComponent<ActorHeadRotationComponent>()) {
        head->mYHeadRot = *kept;
        head->mYHeadRotO = *kept;
    }
}
std::optional<float> CameraSessions::lockedHeadFor(Actor const& actor) const {
    // Snapshot only: a setter callback must not start cancellation/restoration.
    auto* current = client.load();
    if (!running || !current || static_cast<Actor const*>(current->getLocalPlayer()) != &actor
        || !look.snapshot()) return {};
    return lockedHead.load();
}
bool CameraSessions::blocksLookInteraction(Player& player) {
    auto* current = client.load();
    return current && current->getLocalPlayer() == &player && lookAngles().has_value();
}
bool CameraSessions::press(IClientInstance& current) {
    if (!running) return false;
    client = &current;
    bool was = wantZoom.load();
    wantZoom = zoomKey.press(zoomToggle.load(), was);
    reconcile();
    return !was && wantZoom.load();
}
bool CameraSessions::release() {
    bool was = wantZoom.load();
    wantZoom = zoomKey.release(zoomToggle.load(), was);
    reconcile();
    return zoomToggle.load() && was && !wantZoom.load();
}
bool CameraSessions::start() {
    if (running) return true;
    try {
        camera::startInteractionGuard();
        camera::startTerrainCulling();
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
            if (!zoomKey.acceptsWheel(zoomToggle.load())) return;
            auto* current = client.load();
            if (!current || !gameplayScreen(current->getScreenName())) return;
            if (event.buttonData() == 0) return;
            state.wheel(event.buttonData() > 0 ? 1 : -1);
            zoomKey.wheel();
            event.cancel();
        });
        screenListener = bus.emplaceListener<ll::event::AfterUIRenderEvent>([this](auto&) {
            if (pendingFreeCamera.load()) {
                try { pollFreeTravel(); } catch (...) {}
            }
            if (lookAngles()) {
                // The head may also be turned outside the look-input path.
                if (auto* current = client.load(); current && current->getLocalPlayer()) {
                    try { syncLookCameras(*current->getLocalPlayer()); } catch (...) { cancelLook(); }
                    keepHead(*current->getLocalPlayer());
                }
                try { writeFreeCameraOffset(); } catch (...) {}
            }
            try { reconcile(); } catch (...) {}
            if (!state.held()) return;
            state.advance(std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count());
        });
        exitListener = bus.emplaceListener<ll::event::ClientExitLevelEvent>([this](auto&) { reset(); });
        running = true;
        return true;
    } catch (std::exception const& error) {
        Runtime::instance().self().getLogger().error("Camera initialization failed: {}", error.what());
        stop();
        return false;
    }
}
void CameraSessions::stop() {
    running = false;
    reset();
    camera::stopTerrainCulling();
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
