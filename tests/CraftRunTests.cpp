#include "features/recipes/CraftRun.h"
#include <string_view>
void check(bool, char const*);
void craftRunTests() {
    using namespace lamium::recipes::craftRun;
    auto is = [](Act const& act, Do what) { return act.what == what; };
    auto ends = [](Act const& act, std::string_view why) { return act.what == Do::Finish && act.why && why == act.why; };
    Seen empty, filled;
    filled.gridEmpty = false;

    // The patience each run was tuned with stays its own.
    auto const lay = rulesFor(Kind::Lay), make = rulesFor(Kind::Make), onto = rulesFor(Kind::Onto);
    check(lay.pace == 0 && lay.stale == 300 && lay.clearTries == 20, "a recipe waiting to be laid in looks every frame, 20 frames");
    check(make.pace == 50 && make.stale == 300 && make.clearTries == 20 && make.takeTries == 20 && make.resendEvery == 5 && make.mostRefused == 3,
          "a plan's making steps every 50 ms with its own patience");
    check(onto.pace == 0 && onto.stale == 500 && onto.confirm == 200 && onto.giveUp == 1500, "crafting onto the pointer keeps its waits");

    {
        Run unnoted;
        check(stale(unnoted, lay, 1000), "a run whose start was not noted is stale");
        Run run;
        run.seen = 1000;
        check(!stale(run, lay, 1000) &&!stale(run, lay, 1300) && stale(run, lay, 1601), "a run not drawn for longer than its limit is stale");
        Run paced;
        check(due(paced, make, 0) && !due(paced, make, 49) && due(paced, make, 50), "a make run steps every 50 ms");
    }
    {
        // Lay: the grid empties within 20 frames, else the recipe is given up without a word.
        Run run;
        for (int frame = 0; frame < 20; ++frame) check(is(step(run, lay, filled, 0), Do::Wait), "a grid not empty yet is waited for");
        auto const last = step(run, lay, filled, 0);
        check(last.what == Do::Finish && !last.why, "after 20 frames the waiting recipe is dropped silently");
        Run soon;
        check(is(step(soon, lay, empty, 0), Do::Lay), "an empty grid gets the recipe");
        check(laid(soon, lay, 1, 0).what == Do::Finish, "laid in, the waiting recipe is done");
    }
    {
        // Make: the grid is sent back on the first step only; it gives up on the 21st.
        Run run;
        check(is(step(run, make, filled, 0), Do::Evacuate), "making first sends the grid back");
        for (int i = 2; i <= 20; ++i) check(is(step(run, make, filled, 0), Do::Wait), "and then waits for it");
        auto const end = step(run, make, filled, 0);
        check(ends(end, "the grid did not empty") && !end.tidy, "a grid that stays full ends the run");
    }
    {
        Run run;
        check(is(step(run, make, empty, 0), Do::Lay), "an empty grid gets a step's rounds");
        check(is(laid(run, make, 0, 0), Do::Wait) && run.phase == Phase::Clear, "nothing to lay: the next step is the glue's");
        check(is(step(run, make, empty, 0), Do::Lay), "and the next step is laid in");
        laid(run, make, 4, 0);
        check(run.phase == Phase::Take && run.laid == 4, "four rounds laid in wait for the take");
        // The take goes out at once, then again every fifth step while the grid stays laid.
        int takes = 0;
        for (int i = 0; i < 20; ++i) {
            auto const act = step(run, make, filled, 0);
            if (act.what == Do::Take) {
                ++takes;
                check(i == 0 || i % 5 == 4, "the take is sent first and again every fifth step");
            } else check(is(act, Do::Wait), "between takes the run waits");
        }
        check(takes == 5, "five takes in twenty steps");
        auto const end = step(run, make, filled, 0);
        check(ends(end, "the grid did not empty after the take") && end.tidy, "a grid still laid after twenty steps goes back and the run ends");
    }
    {
        Run run;
        step(run, make, empty, 0), laid(run, make, 3, 0);
        check(is(step(run, make, filled, 0), Do::Take), "the take");
        auto const made = step(run, make, empty, 0);
        check(made.what == Do::Made && made.amount == 3 && run.phase == Phase::Clear && run.tries == 0, "an emptied grid after the take made the rounds");
        check(laid(run, make, -1, 0).what == Do::Finish, "a lay the game did not do ends the run");
    }
    {
        // L-390 1-7: rounds the grid gives back before any take are laid in again, three times at most.
        Run run;
        for (int round = 1; round <= 3; ++round) {
            step(run, make, empty, 0), laid(run, make, 2, 0);
            check(is(step(run, make, empty, 0), Do::Wait) && run.phase == Phase::Clear, "a round given back is laid in again");
        }
        step(run, make, empty, 0), laid(run, make, 2, 0);
        check(ends(step(run, make, empty, 0), "the rounds laid in were given back"), "the fourth one given back ends the run");
        Run reset;
        step(reset, make, empty, 0), laid(reset, make, 2, 0), step(reset, make, empty, 0);
        step(reset, make, empty, 0), laid(reset, make, 2, 0), step(reset, make, filled, 0), step(reset, make, empty, 0);
        check(reset.refused == 0, "a round made counts the refusals from nothing again");
    }

    // Onto.
    Seen laidIn = filled;
    laidIn.makesProduct = true;
    auto pointer = [](Seen seen, int held, bool product) { seen.held = held, seen.heldProduct = product; return seen; };
    auto toTake = [&](Run& run) {
        step(run, onto, empty, 0);
        laid(run, onto, 1, 0);
        return step(run, onto, laidIn, 0);
    };
    {
        Run run;
        auto const first = step(run, onto, filled, 0);
        check(is(first, Do::Evacuate) && first.next, "crafting onto the pointer sends the grid back and looks again in the frame");
        for (int frame = 1; frame <= 20; ++frame) check(is(step(run, onto, filled, 0), Do::Wait), "a grid not empty yet is waited for");
        auto const end = step(run, onto, filled, 0);
        check(ends(end, "given up: the grid did not empty") && end.tidy, "for twenty frames");
    }
    {
        Run run;
        check(is(step(run, onto, empty, 0), Do::Lay), "an empty grid gets the round");
        check(ends(laid(run, onto, 0, 0), "given up: the round could not be laid in"), "a round not laid in ends the run");
    }
    {
        Run run;
        step(run, onto, empty, 0);
        auto const after = laid(run, onto, 1, 0);
        check(after.what == Do::Wait && after.next && run.phase == Phase::Confirm, "laid in, the product is looked for in the same frame");
        check(is(step(run, onto, filled, 200), Do::Wait), "the product may take a moment");
        check(ends(step(run, onto, filled, 201), "given up: the grid did not make it"), "but not more than 200 ms");
    }
    {
        Run run;
        auto const take = toTake(run);
        check(is(take, Do::TakeOne) && take.next, "the product shown, it is taken onto the pointer and looked at in the frame");
        check(ends(step(run, onto, pointer(empty, 1, true), 0), "by taking the output"), "crafted at once");
        // The pointer held 4 of the product before.
        Run four;
        step(four, onto, empty, 0), laid(four, onto, 1, 0);
        step(four, onto, pointer(laidIn, 4, true), 0);
        check(four.heldBefore == 4 && ends(step(four, onto, pointer(empty, 5, true), 0), "onto the held stack by taking the output"),
              "crafted onto what the pointer held");
    }
    {
        Run run;
        toTake(run);
        check(is(step(run, onto, laidIn, 0), Do::Wait), "nothing shown in the frame of the take");
        check(ends(step(run, onto, pointer(empty, 1, true), 50), "by taking the output, a moment later"), "crafted a frame later");
    }
    {
        Run run;
        toTake(run);
        step(run, onto, laidIn, 0);
        check(is(step(run, onto, pointer(laidIn, 1, true), 199), Do::Wait), "more on the pointer while the grid still makes it is waited on");
        auto const end = step(run, onto, pointer(laidIn, 1, true), 200);
        check(ends(end, "not confirmed: the pointer shows more but the grid still holds the round") && end.tidy, "then not confirmed");
    }
    {
        Run run;
        step(run, onto, empty, 0), laid(run, onto, 1, 0), step(run, onto, pointer(laidIn, 3, false), 0), step(run, onto, pointer(laidIn, 3, false), 0);
        auto const end = step(run, onto, pointer(laidIn, 3, false), 200);
        check(ends(end, "not taken onto the held stack: the round is left laid in") && !end.tidy, "another item held leaves the round laid in");
    }
    {
        // Nothing shown on an empty pointer: the Shift take, then the round picked up from the inventory.
        Run run;
        Seen shown = laidIn;
        shown.products.assign(36, 0);
        shown.products[3] = 10;
        toTake(run);
        step(run, onto, shown, 0);
        auto const shift = step(run, onto, shown, 200);
        check(is(shift, Do::Take) && shift.next && run.phase == Phase::Fetch && run.before == shown.products, "the Shift take, the product counted first");
        check(is(step(run, onto, shown, 200), Do::Wait), "nothing arrived yet");
        Seen came = empty;
        came.products = shown.products;
        came.products[3] = 11;
        auto const pick = step(run, onto, came, 300);
        check(pick.what == Do::Pick && pick.slot == 3 && pick.amount == 1 && pick.next, "what arrived is picked up");
        check(ends(step(run, onto, pointer(empty, 1, true), 300), "by the Shift take, then from the inventory"), "and it is on the pointer");
        Run lost;
        toTake(lost), step(lost, onto, shown, 0), step(lost, onto, shown, 200), step(lost, onto, came, 300);
        check(ends(step(lost, onto, empty, 300), "given up: the Shift take's product could not be picked up"), "a pick that shows nothing");
        Run never;
        toTake(never), step(never, onto, shown, 0), step(never, onto, shown, 200);
        check(is(step(never, onto, shown, 1700), Do::Wait) && ends(step(never, onto, shown, 1701), "given up: nothing came"), "1.5 s for it");
    }
}
