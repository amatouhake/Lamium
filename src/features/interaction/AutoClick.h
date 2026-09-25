#pragma once
#include <algorithm>
#include <vector>
#include "features/interaction/AutomationInput.h"

namespace lamium::interaction {
// One mouse action (attack or use) driven by Lamium. Periodic and Hold are
// session modes started by a hotkey and exclusive with each other; Fast click
// is a separate switch that turns a physically held button into several
// clicks per client tick. Timing counts client ticks, so intervals are whole
// ticks; the adapter emits the resulting edges on the next input update.
enum class AutoMode { Off, Periodic, Hold };
class AutoClick {
public:
    static constexpr int maxInterval = 1200, maxClicks = 10;

    AutoMode mode() const { return current; }
    bool fast() const { return fastOn; }
    // Starting a mode replaces the other one. The first periodic click is
    // immediate; the next follows after `interval` ticks.
    void start(AutoMode mode, int interval = 1) {
        current = mode;
        period = std::clamp(interval, 1, maxInterval);
        countdown = period;
        pressPending = mode == AutoMode::Periodic;
        // A press from the previous mode is released before the new one acts.
        restart = syntheticDown;
    }
    // Ends the session mode; Fast click stays as the user set it.
    void stop() { current = AutoMode::Off; pressPending = false; }
    void setFast(bool on, int clicksPerTick = 1) {
        fastOn = on;
        clicks = std::clamp(clicksPerTick, 1, maxClicks);
        if (!on) burst = 0;
    }
    // Menus, focus loss, world or dimension changes: nothing keeps running.
    void cancel() { stop(); setFast(false, clicks); }
    // "You touched it, you own it": a physical press ends Periodic and Hold.
    // Vanilla now holds the button, so a synthetic hold is no longer ours.
    void physical(bool down) {
        held = down;
        if (down) { stop(); syntheticDown = false; restart = false; }
        else burst = 0;
    }
    bool physicallyHeld() const { return held; }

    // Once per client tick. A late tick never queues catch-up clicks.
    void tick() {
        if (current == AutoMode::Periodic && --countdown <= 0) {
            pressPending = true;
            countdown = period;
        }
        if (fastOn && held) burst = clicks;
    }
    // Once per native input update: the edges to deliver, in order. Synthetic
    // releases never release a button the user holds, except to separate the
    // clicks of a Fast click burst, which ends pressed again.
    std::vector<InputEdge> update() {
        std::vector<InputEdge> edges;
        if (burst > 0 && held) {
            for (; burst > 0; --burst) { edges.push_back(InputEdge::Release); edges.push_back(InputEdge::Press); }
            return edges;
        }
        if (restart) {
            restart = false;
            syntheticDown = false;
            if (!held) { edges.push_back(InputEdge::Release); return edges; }
        }
        switch (current) {
        case AutoMode::Hold:
            if (!syntheticDown && !held) { syntheticDown = true; edges.push_back(InputEdge::Press); }
            break;
        case AutoMode::Periodic:
            if (syntheticDown) { syntheticDown = false; edges.push_back(InputEdge::Release); }
            else if (pressPending && !held) { pressPending = false; syntheticDown = true; edges.push_back(InputEdge::Press); }
            break;
        case AutoMode::Off:
            if (syntheticDown) { syntheticDown = false; if (!held) edges.push_back(InputEdge::Release); }
            break;
        }
        return edges;
    }
    // The release owed after stop()/cancel(), for callers that tear down
    // without another update.
    bool releaseOwed() const { return syntheticDown && !held; }
    void released() { syntheticDown = false; }

private:
    AutoMode current = AutoMode::Off;
    bool fastOn = false, held = false, syntheticDown = false, pressPending = false, restart = false;
    int period = 1, countdown = 1, clicks = 1, burst = 0;
};
}
