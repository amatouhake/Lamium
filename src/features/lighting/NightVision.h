#pragma once
#include <atomic>
class BaseLightData;
namespace lamium {
class NightVision {
    std::atomic<bool> running{false};
    std::atomic<bool> requested{false};
public:
    static NightVision& instance();
    bool start();
    void stop();
    void configure(bool enabled) { requested = enabled; }
    bool enabled() const { return running && requested; }
    void apply(BaseLightData* data) const;
};
}
