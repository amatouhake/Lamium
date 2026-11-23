#pragma once
#include <vector>

// L-388: the runs that drive the crafting screen over several frames, as one planner. The glue reads what the
// screen shows into Seen, step() answers one action, the glue does it and, where the action says `next`, asks
// again in the same frame. Each run keeps its own patience and counting (Rules): they were tuned in game on
// timings the tests cannot reproduce. Pure: times are milliseconds on the caller's clock.
namespace lamium::recipes::craftRun {
// Lay: a recipe waiting for the grid to empty before it is laid in (`place::run`). Make: a plan's steps, rounds
// laid in and taken until made (L-385). Onto: one round crafted onto the pointer (L-379).
enum class Kind { Lay, Make, Onto };
struct Rules {
    Kind kind = Kind::Lay;
    int clearTries = 20;    // looks at a grid that does not read empty before the run gives up
    long long pace = 0;     // ms between two steps; 0, every frame
    long long stale = 300;  // ms without a frame: its screen went
    int takeTries = 20;     // Make: steps the grid may stay laid after the take
    int resendEvery = 5;    // Make: the take is sent again every that many steps
    int mostRefused = 3;    // Make: lays in a row the grid gave back before any take
    long long confirm = 200, giveUp = 1500; // Onto: for the product to show, for the Shift take's product to arrive
};
constexpr Rules rulesFor(Kind kind) {
    Rules out;
    out.kind = kind;
    if (kind == Kind::Make) out.pace = 50;
    if (kind == Kind::Onto) out.stale = 500;
    return out;
}
// What the screen shows this frame.
struct Seen {
    bool gridEmpty = true;
    bool makesProduct = false; // Onto: the grid makes the run's product
    int held = 0;              // what the pointer holds
    bool heldProduct = false;  // Onto: it is the product
    std::vector<int> products; // Onto: the product per slot (0-8 the hotbar, 9-35 the inventory)
};
enum class Phase { Clear, Confirm, Take, Fetch, Picked };
struct Run {
    Phase phase = Phase::Clear;
    int tries = 0;
    bool evacuated = false; // Onto: the grid was sent back once
    bool took = false;      // Make: a take went out since the round was laid in
    bool fresh = false;     // Onto: the take went out this frame
    int laid = 0;           // Make: rounds laid in by the last lay
    int refused = 0;        // Make: lays in a row given back before a take
    int heldBefore = 0;     // Onto: of the product on the pointer before the take
    std::vector<int> before; // Onto: `products` before the Shift take
    long long since = 0, next = 0, seen = 0;
};
// Evacuate: what the grid holds back to the inventory. Lay: lay the recipe in, then tell laid(). Take: the output
// as a Shift click takes it. TakeOne: the output onto the pointer, the recipe's count. Pick: `amount` from slot
// `slot` onto the pointer. Made: `amount` rounds were made. Finish: the run is over, `why` for the log (none:
// nothing to log); `tidy`, what the grid holds goes back first.
enum class Do { Wait, Evacuate, Lay, Take, TakeOne, Pick, Made, Finish };
struct Act {
    Do what = Do::Wait;
    bool next = false;
    char const* why = nullptr;
    bool tidy = false;
    int slot = 0, amount = 0;
};
inline Act wait() { return {}; }
inline Act go(Do what, bool next = false) { return {what, next}; }
inline Act finish(char const* why, bool tidy = false) { return {Do::Finish, false, why, tidy}; }

// Whether a frame comes too long after the last one (its screen was closed meanwhile); else the frame is noted.
inline bool stale(Run& run, Rules const& rules, long long now) {
    if (run.seen && now - run.seen > rules.stale) return true;
    run.seen = now;
    return false;
}
// Whether this frame takes a step, by the run's pace.
inline bool due(Run& run, Rules const& rules, long long now) {
    if (now < run.next) return false;
    run.next = now + rules.pace;
    return true;
}

inline Act step(Run& run, Rules const& rules, Seen const& seen, long long now) {
    switch (rules.kind) {
    case Kind::Lay:
        // Waiting for what lay in the grid to be back in the inventory (no room for it: given up).
        if (!seen.gridEmpty) return ++run.tries > rules.clearTries ? finish(nullptr) : wait();
        return go(Do::Lay);
    case Kind::Make:
        if (run.phase == Phase::Clear) {
            if (!seen.gridEmpty) {
                if (run.tries++ == 0) return go(Do::Evacuate);
                return run.tries > rules.clearTries ? finish("the grid did not empty") : wait();
            }
            run.tries = 0;
            return go(Do::Lay);
        }
        if (seen.gridEmpty) {
            run.phase = Phase::Clear;
            run.tries = 0;
            if (!run.took) {
                // Empty before any take was sent: the lay was refused and the inputs went back (L-390 1-7). Nothing
                // was made; it is laid in again, and the run ends when that keeps happening.
                return ++run.refused > rules.mostRefused ? finish("the rounds laid in were given back") : wait();
            }
            run.refused = 0;
            return {Do::Made, false, nullptr, false, 0, run.laid};
        } else {
            // The client's output slot does not show the product to read (seen 2026-11-22), so the take is sent
            // blind, and again every few steps while the grid stays laid.
            int const tries = run.tries++;
            // No room for the product, or the recipe leaves something in the grid (a bucket, L-362).
            if (run.tries > rules.takeTries) return finish("the grid did not empty after the take", true);
            if (!run.took || tries % rules.resendEvery == rules.resendEvery - 1) { run.took = true; return go(Do::Take); }
            return wait();
        }
    case Kind::Onto: {
        bool const more = seen.heldProduct && seen.held > run.heldBefore;
        // Crafted: more of the product on the pointer and the round gone from the grid (one round was laid in).
        bool const crafted = more && !seen.makesProduct;
        switch (run.phase) {
        case Phase::Clear:
            if (!seen.gridEmpty) {
                if (!run.evacuated) { run.evacuated = true; return go(Do::Evacuate, true); }
                return ++run.tries > rules.clearTries ? finish("given up: the grid did not empty", true) : wait();
            }
            return go(Do::Lay);
        case Phase::Confirm:
            // The output may show a few frames after the round is laid in.
            if (!seen.makesProduct) return now - run.since > rules.confirm ? finish("given up: the grid did not make it", true) : wait();
            run.heldBefore = seen.heldProduct ? seen.held : 0;
            run.phase = Phase::Take, run.since = now, run.fresh = true;
            return go(Do::TakeOne, true);
        case Phase::Take:
            if (crafted) {
                if (!run.fresh) return finish("by taking the output, a moment later", true);
                return finish(run.heldBefore ? "onto the held stack by taking the output" : "by taking the output", true);
            }
            if (run.fresh) { run.fresh = false; return wait(); }
            if (now - run.since < rules.confirm) return wait();
            // More on the pointer while the grid still makes it: shown by the client only, not crafted.
            if (more) return finish("not confirmed: the pointer shows more but the grid still holds the round", true);
            // With something held the round stays laid in, as when the pointer had no room.
            if (seen.held) return finish("not taken onto the held stack: the round is left laid in");
            run.before = seen.products;
            run.phase = Phase::Fetch, run.since = now;
            return go(Do::Take, true);
        case Phase::Fetch:
            for (size_t i = 0; i < seen.products.size(); ++i) {
                int const added = seen.products[i] - (i < run.before.size() ? run.before[i] : 0);
                if (added <= 0) continue;
                run.phase = Phase::Picked;
                return {Do::Pick, true, nullptr, false, static_cast<int>(i), added};
            }
            return now - run.since > rules.giveUp ? finish("given up: nothing came", true) : wait();
        case Phase::Picked:
            return finish(crafted ? "by the Shift take, then from the inventory" : "given up: the Shift take's product could not be picked up", true);
        }
        return wait();
    }
    }
    return wait();
}
// What the Lay act did: `crafts` rounds laid in, 0 nothing (Make: the step has nothing more to lay, the next
// one is up), -1 the game did not do as planned.
inline Act laid(Run& run, Rules const& rules, int crafts, long long now) {
    switch (rules.kind) {
    case Kind::Lay: return finish(nullptr);
    case Kind::Make:
        if (crafts < 0) return finish("laying a round in failed");
        if (crafts > 0) run.laid = crafts, run.took = false, run.phase = Phase::Take;
        return wait();
    case Kind::Onto:
        if (crafts <= 0) return finish("given up: the round could not be laid in", true);
        run.phase = Phase::Confirm, run.since = now;
        return go(Do::Wait, true);
    }
    return wait();
}
}
