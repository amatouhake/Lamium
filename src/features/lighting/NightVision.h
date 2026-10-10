#pragma once
#include <atomic>
class BaseLightData;
namespace lamium {
class NightVision {
    std::atomic<bool> running{false};
    std::atomic<bool> requested{false};
    std::atomic<bool> even{true};
    mutable std::atomic<float> loggedGamma{-1.f};
public:
    static NightVision& instance();
    bool start();
    void stop();
    void configure(bool enabled, bool evenBrightness) { requested = enabled; even = evenBrightness; }
    bool enabled() const { return running && requested; }
    void apply(BaseLightData* data) const;
};
}
