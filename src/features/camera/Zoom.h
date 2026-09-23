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
    // Stage 1 FreeCamera shares Freelook's angular session. Only one owner
    // runs at a time; the owner decides which enable flag keeps it alive.
    enum class DetachedOwner { None, Freelook, FreeCamera };
    std::atomic<DetachedOwner> lookOwner{DetachedOwner::None};
    std::atomic<bool> lookAllowed{false};
    std::atomic<bool> lookToggle{false};
    std::atomic<bool> freeCameraAllowed{false};
    // Stage 2: latest extracted movement axes while FreeCamera owns the
    // session, kept for the stage 3 camera adapter. Guarded because the
    // extraction hook runs outside the client thread.
    std::mutex freeInputMutex;
    DetachedCameraMotion::Vector freeCameraInput{};
    bool hasFreeCameraInput = false;
    unsigned freeMoveSamples = 0;
    // Latest session displacement for the entity-offset writer below.
    DetachedCameraMotion::Vector lastDisplacement{};
    bool hasDisplacement = false;
    // Stage 3: session displacement for the moving camera. The motion state
    // is advanced per render frame from the stashed input above.
    DetachedCameraMotion motion;
    std::atomic<std::uint64_t> freeMotionOwner{0};
    std::chrono::steady_clock::time_point freeMotionTime{};
    bool freeMotionTimed = false;
    // Perspective travel state (frame listener completes activation).
    std::atomic<bool> pendingFreeCamera{false};
    std::atomic<int> freeToggles{0};
    std::chrono::steady_clock::time_point freeTravelStart{};
    std::chrono::steady_clock::time_point freeLastToggle{};
    std::atomic<bool> running{false};
    std::atomic<bool> allowed{true};
    std::atomic<IClientInstance*> client{nullptr};
    std::atomic<float> lockedHead{0.f};
    void endLookCamera();
    void logFreeCameraSamples();
    void endFreeCameraMotion(bool wasFreeCamera);
    ll::event::ListenerPtr wheelListener, screenListener, exitListener;
public:
    static Zoom& instance();
    bool start();
    void stop();
    void configure(Settings const&);
    void press(IClientInstance&);
    void pressLook(IClientInstance&);
    void pressFreeCamera(IClientInstance&); // Toggle: press again to return to the player
    // True while FreeCamera owns the detached session (perspective is locked).
    bool blocksPerspective() const;
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
    void releaseLook();
    void releaseLookKey(); // Key release: ends a held session, ignored in toggle mode
    void cancelLook();
    bool turnLook(LocalPlayer&, float pitchDelta, float yawDelta);
    bool blocksLookInteraction(Player&);
    std::optional<float> lockedHeadFor(Actor const&) const;
    void keepHead(LocalPlayer&);
    std::optional<DetachedLookState::Angles> lookAngles();
    std::optional<DetachedLookState::Angles> lookAnglesFor(IClientInstance const&);
    void release() { state.release(); }
    void reset() {
        // A pending perspective travel restores first (it needs the client).
        if (pendingFreeCamera.load()) abortPendingTravel();
        state.reset();
        cancelLook();
        client = nullptr;
    }
    float fov(IClientInstance const&, float base) const;
    float sensitivity(LocalPlayer const&) const;
#if defined(LAMIUM_CAMERA_PROBE) || defined(LAMIUM_CAMERA_POSITION_PROBE)
    bool viewProbeActive() const;
#endif
};
}
