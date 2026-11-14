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
    std::atomic<bool> wide{false};
    std::atomic<bool> resetOnRelease{false};
    // The latest unzoomed FOV, so the wheel knows where maxFov is reached.
    mutable std::atomic<float> lastBase{0.0f};
public:
    // The setting starts at 2x or more, so pressing Zoom always visibly zooms
    // (L-80); with Wide-angle view on the wheel may go down to 0.5x (L-99, L-140).
    static constexpr float minLevel = 2.0f;
    static constexpr float maxLevel = 50.0f;
    static constexpr float wheelMinLevel = 0.5f;
    // Below 1x the projection widens; it never passes this (degrees).
    static constexpr float maxFov = 160.0f;
    // One wheel notch scales the magnification by the same ratio at 2x and 40x.
    static constexpr float notch = 1.15f;
    // Time constant of the easing toward the wheel target, in seconds.
    static constexpr double ease = 0.04;

    // Turning Wide-angle view off brings a level below 1x up to 1x, so an old
    // wide level does not come back on the next press.
    void allowWide(bool on) {
        wide = on;
        if (on) return;
        if (target.load() < 1.0f) target = 1.0f;
        if (shown.load() < 1.0f) shown = 1.0f;
    }
    // L-325: every press starts from the setting instead of the last wheel level.
    void resetEachRelease(bool on) { resetOnRelease = on; }
    void configure(float value) {
        float next = std::isfinite(value) ? std::clamp(value, minLevel, maxLevel) : 3.0f;
        // Other camera settings (FreeCamera speed keys) save mid-zoom; only a
        // new magnification may discard the held zoom and its wheel level.
        if (next == initial.load()) return;
        release();
        initial = next;
        target = initial.load();
        shown = initial.load();
    }
    // Press and release switch at once; an ease-in/out was tried and felt wrong
    // without a frame like the spyglass's (maintainer, 2026-10-06).
    void press() {
        // A level left at exactly 1x would reopen as no zoom at all; start from the setting instead.
        if (target.load() == 1.0f) target = initial.load();
        shown = target.load();
        last = -1.0;
        active = true;
    }
    void release() {
        active = false;
        if (resetOnRelease.load()) target = initial.load();
        shown = target.load();
    }
    void reset() { release(); target = initial.load(); shown = initial.load(); }
    bool held() const { return active.load(); }
    float level() const { return shown.load(); }
    float targetLevel() const { return target.load(); }
    // The lowest wheel level: 1x without Wide-angle view; with it 0.5x, or
    // higher where the view would pass maxFov.
    float wheelFloor() const {
        if (!wide.load()) return 1.0f;
        float base = lastBase.load();
        return base > 0.0f ? std::max(wheelMinLevel, std::min(1.0f, base / maxFov)) : wheelMinLevel;
    }
    void wheel(int direction) {
        if (!held() || direction == 0) return;
        float current = target.load();
        float next = direction > 0 ? current * notch : current / notch;
        // A notch that crosses 1x stops on it, so normal vision is easy to find.
        if ((current < 1.0f && next > 1.0f) || (current > 1.0f && next < 1.0f)) next = 1.0f;
        target = std::clamp(next, std::min(wheelFloor(), current), maxLevel);
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
        if (!std::isfinite(base) || base <= 0.0f) return base;
        lastBase = base;
        if (!held()) return base;
        return std::clamp(base / level(), std::min(1.0f, base), std::max(base, maxFov));
    }
    // Below 1x turning keeps the normal speed instead of speeding up (L-99).
    float sensitivity() const { return held() ? 1.0f / std::max(level(), 1.0f) : 1.0f; }
};
// The Zoom key while it is down. Holding it is what lets the wheel change the
// magnification, in both activation modes, so in toggle mode the wheel keeps
// its normal job (hotbar) while Zoom is on. Toggle mode switches on at the
// press and off at the release, unless the wheel was used meanwhile (L-99
// follow-up, 2026-10-06).
// The wheel arrives from the window's input thread, the key on the client thread.
struct ZoomKey {
    std::atomic<bool> down{false}, wheelUsed{false}, pendingOff{false};
    void clear() { down = false; wheelUsed = false; pendingOff = false; }
    // The wanted state after a press.
    bool press(bool toggle, bool wanted) {
        down = true;
        wheelUsed = false;
        pendingOff = toggle && wanted;
        return true;
    }
    // The wanted state after a release.
    bool release(bool toggle, bool wanted) {
        down = false;
        bool off = !toggle || (pendingOff.load() && !wheelUsed.load());
        pendingOff = false;
        return off ? false : wanted;
    }
    bool acceptsWheel(bool toggle) const { return !toggle || down.load(); }
    void wheel() { if (down.load()) wheelUsed = true; }
};
}
