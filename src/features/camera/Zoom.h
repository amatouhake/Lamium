#pragma once
#include "features/camera/ZoomState.h"
#include "features/camera/DetachedLookState.h"
#include "features/camera/DetachedCameraMotion.h"
#include "ll/api/event/ListenerBase.h"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
namespace mce { class Camera; }
class Actor;
class IClientInstance;
class LocalPlayer;
class Player;
struct MoveInputComponent;
struct RawMoveInputComponent;
namespace lamium {
struct Settings;
class Zoom {
    ZoomState state;
    DetachedLookState look;
    // FreeCamera shares Freelook's angular session. Only one owner
    // runs at a time; the owner decides which enable flag keeps it alive.
    enum class DetachedOwner { None, Freelook, FreeCamera };
    std::atomic<DetachedOwner> lookOwner{DetachedOwner::None};
    std::atomic<bool> lookToggle{false};
    // Wanted state of each session (BACKLOG L-47): keys and the settings
    // switch flip these; reconcile() starts or ends the sessions when the
    // game allows. Never saved; death, dimension change and leaving the
    // world clear them.
    std::atomic<bool> wantZoom{false}, wantLook{false}, wantFree{false};
    std::atomic<bool> zoomToggle{false};
    // Latest extracted movement axes while FreeCamera owns the session.
    // Written by the input extraction hook, read by the render hook.
    std::mutex freeInputMutex;
    DetachedCameraMotion::Vector freeCameraInput{};
    bool hasFreeCameraInput = false;
    unsigned freeMoveSamples = 0;
    // Latest session displacement for the entity-offset writer below.
    DetachedCameraMotion::Vector lastDisplacement{};
    bool hasDisplacement = false;
    // Last two render eyes: a perspective switch has settled once they match.
    DetachedCameraMotion::Vector lastEye{}, prevEye{};
    // Forward of the camera actually rendered last frame. The detached camera
    // entity turns natively, so this (not the session's start angles) is
    // where it looks.
    DetachedCameraMotion::Vector lastForward{};
    bool hasForward = false;
    // Session displacement for the moving camera. The motion state
    // is advanced per render frame from the stashed input above.
    DetachedCameraMotion motion;
    std::atomic<std::uint64_t> freeMotionOwner{0};
    std::chrono::steady_clock::time_point freeMotionTime{};
    bool freeMotionTimed = false;
    // Perspective travel state (frame listener completes activation).
    std::atomic<bool> pendingFreeCamera{false};
    std::atomic<int> freePerspective{-1}; // Perspective saved at activation.
    std::chrono::steady_clock::time_point freeTravelStart{};
    std::atomic<bool> running{false};
    std::atomic<IClientInstance*> client{nullptr};
    std::atomic<float> lockedHead{0.f};
    void endLookCamera();
    void logFreeCameraSamples();
    void endFreeCameraMotion(bool wasFreeCamera);
    bool beginLook(IClientInstance&);
    bool startFreeCamera(IClientInstance&);
    ll::event::ListenerPtr wheelListener, screenListener, exitListener;
public:
    static Zoom& instance();
    bool start();
    void stop();
    void configure(Settings const&);
    enum class Session { Zoom, Freelook, FreeCamera };
    bool wanted(Session) const;
    void toggleWanted(Session);
    // Starts or ends sessions to match the wanted state; runs every frame.
    void reconcile();
    // Focus loss pauses Zoom and Freelook; FreeCamera keeps its position.
    void suspendForFocus();
    void press(IClientInstance&);
    void pressLook(IClientInstance&);
    void pressFreeCamera(IClientInstance&); // Toggle: press again to return to the player
    // True while FreeCamera owns the detached session (perspective is locked).
    bool blocksPerspective() const;
    // True while Freelook or FreeCamera detaches the view from the player.
    bool detachedCameraActive() const { return lookOwner.load() != DetachedOwner::None; }
    // Perspective travel: FreeCamera always flies first-person. Returns true
    // when the first-person rig is already active; otherwise requests vanilla
    // toggles and completes activation from the frame listener.
    bool ensureFirstPerson(IClientInstance&, LocalPlayer&);
    bool beginFreeCameraSession(IClientInstance&, LocalPlayer&);
    void pollFreeTravel();
    void abortPendingTravel();
    void restoreFreePerspective(IClientInstance&);
    // Extraction-hook entry: consumes movement only for the FreeCamera owner.
    void consumeFreeCameraInput(MoveInputComponent const&, RawMoveInputComponent&);
    // Render-hook entry: advances the session displacement from the stashed
    // input. Returns false when vanilla rendering must stay untouched.
    bool freeCameraView(IClientInstance const&, mce::Camera&);
    // Frame writer: carries the latest displacement into the detached camera
    // entity's offset component on the UI-render thread (mirrors keepHead).
    void writeFreeCameraOffset();
    // Records every render eye; the poll loop reads the history.
    void recordRenderEye(mce::Camera&);
    void releaseLook();
    void releaseLookKey(); // Key release: ends a held session, ignored in toggle mode
    void cancelLook();
    bool turnLook(LocalPlayer&, float pitchDelta, float yawDelta);
    bool blocksLookInteraction(Player&);
    std::optional<float> lockedHeadFor(Actor const&) const;
    void keepHead(LocalPlayer&);
    std::optional<DetachedLookState::Angles> lookAngles();
    std::optional<DetachedLookState::Angles> lookAnglesFor(IClientInstance const&);
    // Where the detached camera looks from and toward (Freelook or FreeCamera),
    // for readouts that should follow the camera rather than the body.
    struct ViewRay { double x, y, z, dx, dy, dz; };
    std::optional<ViewRay> detachedViewRay(IClientInstance&);
    void release(); // Zoom key release: ends a held Zoom, ignored in toggle mode
    void reset() {
        wantZoom = false;
        wantLook = false;
        wantFree = false;
        // A pending perspective travel restores first (it needs the client).
        if (pendingFreeCamera.load()) abortPendingTravel();
        state.reset();
        cancelLook();
        client = nullptr;
    }
    float fov(IClientInstance const&, float base) const;
    float sensitivity(LocalPlayer const&) const;
    // The shown magnification while Zoom is held for this client.
    std::optional<float> magnification(IClientInstance const&) const;
#if defined(LAMIUM_CAMERA_PROBE) || defined(LAMIUM_CAMERA_POSITION_PROBE)
    bool viewProbeActive() const;
#endif
};
}
