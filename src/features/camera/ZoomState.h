#pragma once
#include <algorithm>
#include <atomic>
#include <cmath>

namespace lamium {
class ZoomState {
    std::atomic<bool> active{false};
    std::atomic<float> target{3.0f};
    std::atomic<float> shown{3.0f};
    std::atomic<float> initial{3.0f};
    std::atomic<double> last{-1.0};
public:
    static constexpr float minLevel = 1.0f;
    static constexpr float maxLevel = 50.0f;
    // One wheel notch scales the magnification by the same ratio at 2x and 40x.
    static constexpr float notch = 1.15f;
    // Time constant of the easing toward the wheel target, in seconds.
    static constexpr double ease = 0.04;

    void configure(float value) {
        release();
        initial = std::isfinite(value) ? std::clamp(value, minLevel, maxLevel) : 3.0f;
        target = initial.load();
        shown = initial.load();
    }
    void press() {
        shown = target.load();
        last = -1.0;
        active = true;
    }
    void release() {
        active = false;
        shown = target.load();
    }
    void reset() { release(); target = initial.load(); shown = initial.load(); }
    bool held() const { return active.load(); }
    float level() const { return shown.load(); }
    float targetLevel() const { return target.load(); }
    void wheel(int direction) {
        if (held() && direction != 0)
            target = std::clamp(direction > 0 ? target.load() * notch : target.load() / notch, minLevel, maxLevel);
    }
    // Eases in log space so a notch looks alike at any magnification; frame-rate independent.
    void advance(double now) {
        double previous = last.exchange(now);
        if (!held() || previous < 0.0 || !(now > previous)) return;
        float goal = target.load();
        float current = shown.load();
        float remaining = static_cast<float>(std::exp(-std::min(now - previous, 0.25) / ease));
        float next = goal * std::pow(current / goal, remaining);
        if (std::abs(std::log(next / goal)) < 1e-3f) next = goal;
        shown = next;
    }
    float fov(float base) const {
        if (!held() || !std::isfinite(base) || base <= 0.0f) return base;
        return std::clamp(base / level(), std::min(1.0f, base), base);
    }
    float sensitivity() const { return held() ? 1.0f / level() : 1.0f; }
};
}
