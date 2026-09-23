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
    std::atomic<bool> lookAllowed{false};
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
    void releaseLook();
    void cancelLook();
    bool turnLook(LocalPlayer&, float pitchDelta, float yawDelta);
    bool blocksLookInteraction(Player&);
    std::optional<float> lockedHeadFor(Actor const&) const;
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
