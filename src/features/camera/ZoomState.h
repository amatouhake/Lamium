#pragma once
#include <algorithm>
#include <atomic>
#include <cmath>

namespace lamium {
class ZoomState {
    std::atomic<bool> active{false};
    std::atomic<float> factor{3.0f};
    std::atomic<float> initial{3.0f};
    std::atomic<float> step{0.5f};
public:
    void configure(float value, float increment) {
        release();
        initial = std::isfinite(value) ? std::clamp(value, 1.0f, 10.0f) : 3.0f;
        step = std::isfinite(increment) ? std::clamp(increment, 0.1f, 2.0f) : 0.5f;
        factor = initial.load();
    }
    void press() { active = true; }
    void release() { active = false; }
    void reset() { release(); factor = initial.load(); }
    bool held() const { return active.load(); }
    float level() const { return factor.load(); }
    void wheel(int direction) {
        if (held() && direction != 0)
            factor = std::clamp(level() + (direction > 0 ? step.load() : -step.load()), 1.0f, 10.0f);
    }
    float fov(float base) const {
        if (!held() || !std::isfinite(base) || base <= 0.0f) return base;
        return std::clamp(base / level(), std::min(1.0f, base), base);
    }
    float sensitivity() const { return held() ? 1.0f / level() : 1.0f; }
};
}
