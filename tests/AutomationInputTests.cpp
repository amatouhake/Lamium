#include "features/interaction/AutomationInput.h"
#include <stdexcept>

void automationInputTests() {
    using namespace lamium::interaction;
    using namespace std::chrono_literals;
    auto check = [](bool value) { if (!value) throw std::runtime_error("automation input lifecycle"); };
    AutomationInput input;
    AutomationInput::Time now{};
    auto tick = [&](auto time, bool eligible = true, bool physical = false, bool hold = false) {
        return input.update(time, eligible, physical, hold, 100ms);
    };
    check(tick(now) == InputEdge::None);
    input.arm();
    check(tick(now) == InputEdge::Press);
    check(tick(now + 10ms) == InputEdge::Release);
    check(tick(now + 99ms) == InputEdge::None);
    check(tick(now + 100ms) == InputEdge::Press);
    check(tick(now + 5s) == InputEdge::Release);
    check(tick(now + 5010ms) == InputEdge::Press);
    check(tick(now + 5020ms) == InputEdge::Release);
    check(tick(now + 5030ms) == InputEdge::None);
    // Losing input ownership disarms rather than resuming on the next frame.
    check(tick(now + 6s, false) == InputEdge::None);
    check(!input.active());
    check(tick(now + 7s) == InputEdge::None);
    input.arm();
    check(tick(now + 8s, true, false, true) == InputEdge::Press);
    check(tick(now + 9s, true, false, true) == InputEdge::None);
    check(tick(now + 10s, false, false, true) == InputEdge::Release);
    check(input.cancel() == InputEdge::None);
    // Physical input takes priority; only the synthetic contribution releases.
    input.arm();
    check(tick(now + 11s, true, true, true) == InputEdge::None);
    check(tick(now + 12s, true, false, true) == InputEdge::Press);
    check(tick(now + 13s, true, true, true) == InputEdge::Release);
    check(tick(now + 14s, true, true, true) == InputEdge::None);
    check(tick(now + 15s, true, false, true) == InputEdge::Press);
    check(input.cancel() == InputEdge::Release);
    input.arm();
    check(input.update(now, true, false, false, 0ms) == InputEdge::None);
    check(!input.active());
}
