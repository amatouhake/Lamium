#pragma once
#include "features/camera/ZoomState.h"
#include "features/camera/DetachedLookState.h"
#include "ll/api/event/ListenerBase.h"
#include <atomic>
class IClientInstance;
class LocalPlayer;
namespace lamium {
struct Settings;
class Zoom {
    ZoomState state;
    DetachedLookState look;
    std::atomic<bool> lookAllowed{false};
    std::atomic<bool> running{false};
    std::atomic<bool> allowed{true};
    std::atomic<IClientInstance*> client{nullptr};
    ll::event::ListenerPtr wheelListener, screenListener, exitListener;
public:
    static Zoom& instance();
    bool start();
    void stop();
    void configure(Settings const&);
    void press(IClientInstance&);
    void pressLook(IClientInstance&);
    void releaseLook() { look.cancel(); }
    bool turnLook(LocalPlayer&, float pitchDelta, float yawDelta);
    std::optional<DetachedLookState::Angles> lookAngles();
    void release() { state.release(); }
    void reset() { state.reset(); look.cancel(); client = nullptr; }
    float fov(float base) const { return running ? state.fov(base) : base; }
    float sensitivity() const { return running ? state.sensitivity() : 1.0f; }
#ifdef LAMIUM_CAMERA_PROBE
    bool viewProbeActive() const;
#endif
};
}
