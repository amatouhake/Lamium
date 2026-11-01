#include "features/interaction/SneakPlacementPlan.h"
void check(bool, char const*);
void sneakPlacementTests() {
    using namespace lamium::interaction::sneakPlacement;
    check(!wantedSneak(false, false, false, true, false) && !wantedSneak(true, false, false, false, false)
        && !wantedSneak(true, false, false, true, true),
        "L-293: off, a plain block or an empty hand need no sneak state");
    check(wantedSneak(true, false, false, true, false) == true && wantedSneak(true, false, true, true, false) == true,
        "L-293: on, a held item on an interactive block needs sneaking");
    check(wantedSneak(true, true, true, true, false) == false && wantedSneak(true, true, false, true, false) == true,
        "L-293: the invert option opens while the player sneaks");

    Pulse p;
    check(gate(p, true, false, false, true, false, false) == Gate::Hold && p.wanted == true,
        "L-293: a use the server would read as a click is held back");
    check(afterAuth(p, false) == Step::None && gate(p, true, false, false, true, false, false) == Gate::Hold,
        "L-293: the use stays held until an auth input carries the sneak");
    check(afterAuth(p, true) == Step::None && gate(p, true, false, true, true, false, true) == Gate::Pass,
        "L-293: once the sneak has been sent, the retried use passes");
    for (int i = 0; i < holdTicks; ++i) afterAuth(p, true);
    check(afterAuth(p, true) == Step::Release && !p.wanted,
        "L-293: the override ends a few ticks after the last use that needed it");

    Pulse sendOnly;
    gate(sendOnly, true, false, false, true, false, false);
    check(!releaseBeforeAuth(sendOnly), "L-293: send-only keeps the sneak while the use still waits");
    afterAuth(sendOnly, true);
    gate(sendOnly, true, false, false, true, false, true);
    check(releaseBeforeAuth(sendOnly) && !sendOnly.wanted,
        "L-293: send-only drops the sneak on the auth input right after the use went out");

    Pulse quick;
    gate(quick, true, false, false, true, false, false);
    afterAuth(quick, true);
    check(afterAuth(quick, true) == Step::Replay,
        "L-293: a held-back click that vanilla does not retry is replayed once the sneak is sent");
    check(gate(quick, true, false, true, true, false, true) == Gate::Pass,
        "L-293: the replayed use passes");

    Pulse stuck;
    gate(stuck, true, false, false, true, false, false);
    Step last = Step::None;
    for (int i = 0; i <= waitTicks; ++i) last = afterAuth(stuck, false);
    check(last == Step::GiveUp && gate(stuck, true, false, false, true, false, false) == Gate::Pass,
        "L-293: a sneak the server never sees gives up and lets vanilla act");

    Pulse invert;
    check(gate(invert, true, true, true, true, false, true) == Gate::Hold && invert.wanted == false,
        "L-293: inverted, a sneaking use waits for an auth input without sneak");
    afterAuth(invert, false);
    check(gate(invert, true, true, false, true, false, false) == Gate::Pass,
        "L-293: the player's sneak at the start still decides while the override stands them up");

    Pulse plain;
    check(gate(plain, true, false, true, true, false, true) == Gate::Pass && !plain.wanted,
        "L-293: a use the server already reads as sneaking passes without an override");
}
