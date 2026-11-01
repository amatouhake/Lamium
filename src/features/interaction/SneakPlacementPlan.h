#pragma once
#include <optional>

// L-293: a use on an interactive block behaves as if sneaking. The server
// decides with the sneak state of the auth inputs it received before the use
// (VALIDATION-LOG 2026-11-01), so a use that needs another state is held back
// until an auth input carrying that state has been sent, then let through.
namespace lamium::interaction::sneakPlacement {
// Ticks an override outlives the last use that needed it, so repeated
// placements along interactive blocks do not crouch and stand every tick.
inline constexpr int holdTicks = 3;
// Ticks a held use may wait for the server to see the state before the
// feature gives up and lets vanilla act for a while.
inline constexpr int waitTicks = 10, bypassTicks = 20;
// A use that was held back and not retried by vanilla (the button was let
// go) is replayed once the state has been sent for this many ticks.
inline constexpr int replayTicks = 2;

struct Pulse {
    std::optional<bool> wanted; // The sneak state the server must see.
    bool user = false;          // The player's own sneak when the override began.
    bool replay = false;        // A held-back use waits for a replay.
    int idle = 0, waited = 0, matched = 0, bypass = 0;
};
enum class Gate { Pass, Hold };

// The state a use on a block needs, or nothing when vanilla's own is right.
// An empty hand interacts even while sneaking, so it never needs one.
inline std::optional<bool> wantedSneak(bool enabled, bool invert, bool userSneak, bool interactive, bool emptyHand) {
    if (!enabled || !interactive || emptyHand) return {};
    return !(invert && userSneak);
}
// Called for each use on a block. `localSneak` is the client's sneak flag,
// `serverSneak` the state of the last auth input sent.
inline Gate gate(Pulse& p, bool enabled, bool invert, bool localSneak, bool interactive, bool emptyHand,
    bool serverSneak) {
    if (p.bypass > 0) return Gate::Pass;
    bool user = p.wanted ? p.user : localSneak;
    auto wanted = wantedSneak(enabled, invert, user, interactive, emptyHand);
    if (!wanted) return Gate::Pass;
    if (p.wanted) p.idle = 0;
    if (*wanted == serverSneak && (!p.wanted || *p.wanted == *wanted)) {
        p.replay = false;
        return Gate::Pass;
    }
    if (p.wanted != wanted) {
        p = Pulse{.wanted = wanted, .user = user};
    }
    p.replay = true;
    return Gate::Hold;
}
enum class Step { None, Release, Replay, GiveUp };
// Called once per sent auth input, after the override was applied to it.
inline Step afterAuth(Pulse& p, bool serverSneak) {
    if (p.bypass > 0) { --p.bypass; return Step::None; }
    if (!p.wanted) return Step::None;
    if (serverSneak != *p.wanted) {
        p.matched = 0;
        if (++p.waited > waitTicks) {
            p = Pulse{.bypass = bypassTicks};
            return Step::GiveUp;
        }
        return Step::None;
    }
    ++p.matched;
    if (p.replay && p.matched >= replayTicks) {
        p.replay = false;
        p.idle = 0;
        return Step::Replay;
    }
    if (!p.replay && ++p.idle > holdTicks) {
        p = {};
        return Step::Release;
    }
    return Step::None;
}
}
