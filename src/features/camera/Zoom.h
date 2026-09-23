#pragma once
#include "features/camera/ZoomState.h"
#include "features/camera/DetachedLookState.h"
#include "ll/api/event/ListenerBase.h"
#include <atomic>
class Actor;
class IClientInstance;
class LocalPlayer;
class Player;
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
    std::atomic<bool> running{false};
    std::atomic<bool> allowed{true};
    std::atomic<IClientInstance*> client{nullptr};
    std::atomic<float> lockedHead{0.f};
    void endLookCamera();
    ll::event::ListenerPtr wheelListener, screenListener, exitListener;
public:
    static Zoom& instance();
    bool start();
    void stop();
    void configure(Settings const&);
    void press(IClientInstance&);
    void pressLook(IClientInstance&);
    void pressFreeCamera(IClientInstance&); // Toggle: press again to return to the player
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
    void reset() { state.reset(); cancelLook(); client = nullptr; }
    float fov(IClientInstance const&, float base) const;
    float sensitivity(LocalPlayer const&) const;
#if defined(LAMIUM_CAMERA_PROBE) || defined(LAMIUM_CAMERA_POSITION_PROBE)
    bool viewProbeActive() const;
#endif
};
}
