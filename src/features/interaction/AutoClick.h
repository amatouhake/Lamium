#pragma once
#include <algorithm>
#include <vector>
#include "features/interaction/AutomationInput.h"
#include "features/interaction/AutoMode.h"

namespace lamium::interaction {
// One mouse action (attack or use) driven by Lamium. It follows the settings:
// a switch and one mode (Periodic, Hold or Fast click). Only the switch turns
// it off. A physical press takes priority while held and automation resumes
// on release; losing gameplay input suspends it until input returns. Timing
// counts client ticks; the adapter emits the edges on the next input update.
class AutoClick {
public:
    static constexpr int maxInterval = 1200, maxClicks = 10;

    bool on() const { return enabled; }
    AutoMode mode() const { return current; }
    // Called with the current settings every tick; only changes act. Turning
    // on or changing mode releases any press of the old mode first, and a
    // Periodic start clicks at once.
    void configure(bool on, AutoMode mode, int interval, int clicksPerTick, FastTrigger trigger = FastTrigger::Always) {
        period = std::clamp(interval, 1, maxInterval);
        countdown = std::min(countdown, period);
        clicks = std::clamp(clicksPerTick, 1, maxClicks);
        fastAlways = trigger == FastTrigger::Always;
        if (on == enabled && (!on || mode == current)) return;
        enabled = on;
        current = mode;
        restart = syntheticDown;
        burst = 0;
        countdown = period;
        pressPending = on && mode == AutoMode::Periodic;
    }
    // The user's own press owns the button while held. Vanilla then holds
    // (and on release, releases) it, so no synthetic press remains ours.
    void physical(bool down) {
        held = down;
        syntheticDown = false;
        restart = false;
        if (!down) burst = 0;
    }
    bool physicallyHeld() const { return held; }
    // After focus loss the physical release may never be reported.
    void forgetHeld() { held = false; burst = 0; }

    // Once per client tick. A late tick never queues catch-up clicks, and
    // nothing is queued while suspended or while the user holds the button.
    void tick() {
        if (!enabled || suspended) return;
        if (current == AutoMode::Periodic && --countdown <= 0) {
            countdown = period;
            if (!held) pressPending = true;
        }
        if (current == AutoMode::Fast && (held || fastAlways)) burst = clicks;
    }
    // Once per native input update while gameplay input is ours: the edges
    // to deliver, in order. A synthetic release never releases a button the
    // user holds, except between the clicks of a Fast click burst, which
    // ends pressed again. Unheld, a burst is press/release pairs.
    std::vector<InputEdge> update() {
        suspended = false;
        std::vector<InputEdge> edges;
        if (restart) {
            restart = false;
            syntheticDown = false;
            if (!held) { edges.push_back(InputEdge::Release); return edges; }
        }
        if (!enabled) {
            if (syntheticDown && !held) edges.push_back(InputEdge::Release);
            syntheticDown = false;
            return edges;
        }
        switch (current) {
        case AutoMode::Fast:
            for (; burst > 0; --burst) {
                if (held) { edges.push_back(InputEdge::Release); edges.push_back(InputEdge::Press); }
                else if (fastAlways) { edges.push_back(InputEdge::Press); edges.push_back(InputEdge::Release); }
            }
            break;
        case AutoMode::Hold:
            if (!syntheticDown && !held) { syntheticDown = true; edges.push_back(InputEdge::Press); }
            break;
        case AutoMode::Periodic:
            if (syntheticDown) { syntheticDown = false; edges.push_back(InputEdge::Release); }
            else if (pressPending && !held) { pressPending = false; syntheticDown = true; edges.push_back(InputEdge::Press); }
            break;
        }
        return edges;
    }
    // Gameplay input is not ours (menu, focus, death, other client): stop
    // acting but stay on. True when a release is owed now.
    bool suspend() {
        suspended = true;
        burst = 0;
        pressPending = false;
        restart = false;
        bool owed = syntheticDown && !held;
        syntheticDown = false;
        return owed;
    }
    bool isSuspended() const { return suspended; }
    bool releaseOwed() const { return (syntheticDown || restart) && !held; }

private:
    AutoMode current = AutoMode::Periodic;
    bool enabled = false, held = false, syntheticDown = false, pressPending = false, restart = false, suspended = false;
    bool fastAlways = true;
    int period = 1, countdown = 1, clicks = 1, burst = 0;
};
}
