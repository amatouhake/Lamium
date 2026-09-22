#pragma once
#include "features/camera/ZoomState.h"
#include "ll/api/event/ListenerBase.h"
#include <atomic>
class IClientInstance;
namespace lamium {
struct Settings;
class Zoom {
    ZoomState state;
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
    void release() { state.release(); }
    void reset() { state.reset(); client = nullptr; }
    float fov(float base) const { return running ? state.fov(base) : base; }
    float sensitivity() const { return running ? state.sensitivity() : 1.0f; }
#ifdef LAMIUM_CAMERA_PROBE
    bool viewProbeActive() const;
#endif
};
}
