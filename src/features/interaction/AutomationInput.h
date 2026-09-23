#pragma once
#include <chrono>

namespace lamium::interaction {
// Runtime intent only: never serialize an armed automation across world loads.
// The adapter must cancel on focus loss, screen/world changes, and disable.
// Edges are contributions to vanilla input; Release must not release a key
// that the user is physically holding.
enum class InputEdge { None, Press, Release };
class AutomationInput {
public:
    using Clock = std::chrono::steady_clock;
    using Time = Clock::time_point;
    using Duration = Clock::duration;

    void arm() { armed = true; }
    bool active() const { return armed; }

    InputEdge cancel() {
        armed = false;
        scheduled = false;
        return release();
    }

    // Called once per native input update. A periodic press lasts one update;
    // a delayed update emits at most one press, never catch-up bursts.
    InputEdge update(Time now, bool eligible, bool physicalDown, bool hold, Duration interval) {
        if (!eligible) return cancel();
        if (!armed) return release();
        if (physicalDown) {
            scheduled = false;
            return release();
        }
        if (hold) {
            scheduled = false;
            if (down) return InputEdge::None;
            down = true;
            return InputEdge::Press;
        }
        if (interval <= Duration::zero()) return cancel();
        if (down) return release();
        if (scheduled && now < next) return InputEdge::None;
        down = true;
        scheduled = true;
        next = now + interval;
        return InputEdge::Press;
    }

private:
    InputEdge release() {
        if (!down) return InputEdge::None;
        down = false;
        return InputEdge::Release;
    }
    bool armed = false;
    bool down = false;
    bool scheduled = false;
    Time next{};
};
}
