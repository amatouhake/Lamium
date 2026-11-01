#include "features/inventory/RestockPlan.h"
#include "features/inventory/RestockRemainder.h"
#include <utility>
#include <vector>
void check(bool, char const*);

void restockRemainderTests() {
    using namespace lamium::inventory;
    // Kind 1 is the refill the swap put in the hand, kind 2 the consumption
    // remainder the swap put in the slot the refill came from.
    auto snap = [](int selected, std::vector<std::pair<int,RestockSlot>> const& slots) {
        RestockSnapshot result;
        result.context = 21;
        result.selected = selected;
        result.slots[selected] = {1,1};
        for (auto const& [slot,value] : slots) result.slots[slot] = value;
        return result;
    };
    auto pending = [](int selected, int source, int kind, int count, int frame = 100) {
        RemainderPending result;
        result.context = 21;
        result.selected = selected;
        result.source = source;
        result.kind = kind;
        result.count = count;
        result.frame = frame;
        return result;
    };
    struct Row {
        char const* what;
        std::vector<std::pair<int,RestockSlot>> slots;
        int destination; // -1: no merge
    };
    // The remainder sits in slot 14, where the swap's refill came from; the
    // hand (slot 3) holds the refill.
    for (auto const& row : std::vector<Row>{
        {"an existing stack of the remainder with room takes it", {{14,{2,1}},{20,{2,5}}}, 20},
        {"a stack one short of full is enough", {{14,{2,1}},{20,{2,15}}}, 20},
        {"no stack of the remainder leaves it alone", {{14,{2,1}},{20,{7,5}}}, -1},
        {"a full stack cannot take the remainder", {{14,{2,1}},{20,{2,16}}}, -1},
        {"the remainder stack alone has nothing to merge into", {{14,{2,1}}}, -1},
        {"a locked remainder slot is never touched", {{14,{2,1,true}},{20,{2,5}}}, -1},
        {"a locked stack cannot take the remainder", {{14,{2,1}},{20,{2,5,true}}}, -1},
        {"the offhand is not a merge destination", {{14,{2,1}},{offhandSlot,{2,5}}}, -1},
        {"an empty slot is not a stack to merge into", {{14,{2,1}},{20,{}}}, -1},
        {"the hand is never a merge destination", {{3,{2,4}},{14,{2,1}}}, -1},
        {"a different item in the source slot is not the remainder", {{14,{4,1}},{20,{2,5}}}, -1},
        {"a remainder that moved away is not followed", {{20,{2,5}}}, -1},
        {"a changed remainder count is not the remainder", {{14,{2,2}},{20,{2,5}}}, -1},
        {"the fullest main-inventory stack wins", {{14,{2,1}},{20,{2,4}},{30,{2,9}}}, 30},
        {"equal stacks take the lower slot", {{14,{2,1}},{20,{2,4}},{30,{2,4}}}, 20},
        {"the main inventory wins over a hotbar stack", {{14,{2,1}},{1,{2,9}},{30,{2,3}}}, 30},
        {"a hotbar stack is used when nothing else can take it", {{14,{2,1}},{1,{2,3}}}, 1},
        {"a stack too full for the whole remainder is skipped", {{14,{2,1}},{20,{2,16}},{30,{2,7}}}, 30},
    }) {
        auto state = snap(3,row.slots);
        auto merge = planRemainderMerge(state,pending(3,14,2,1),16);
        if (row.destination < 0) check(!merge,row.what);
        else check(merge && merge->source == 14 && merge->destination == row.destination
            && merge->count == 1 && merge->destinationCount == state.slots[row.destination].count + 1,row.what);
    }
    // Everything the wait depends on is revalidated, not remembered.
    auto state = snap(3,{{14,{2,1}},{20,{2,5}}});
    auto changed = state;
    changed.context = 22;
    check(!planRemainderMerge(changed,pending(3,14,2,1),16), "a new world ends the wait");
    check(!planRemainderMerge(snap(4,{{14,{2,1}},{20,{2,5}}}),pending(3,14,2,1),16),
          "a changed hand ends the wait");
    check(!planRemainderMerge(state,pending(3,14,2,1),0) && !planRemainderMerge(state,pending(3,14,2,1),-1),
          "an invalid stack limit fails open");
    check(!planRemainderMerge(snap(3,{{14,{2,2}},{20,{2,5}}}),pending(3,14,2,2),1),
          "a remainder larger than a slot is refused");
    check(!planRemainderMerge(state,pending(3,3,2,1),16), "the hand is never the remainder source");
    check(!planRemainderMerge(state,pending(3,14,2,0),16), "an empty remainder is nothing to merge");
    check(!planRemainderMerge(state,RemainderPending{},16), "a cleared remainder plans nothing");

    // The merge waits for the swap's own tick and gives up after the deadline.
    auto wait = pending(3,14,2,1,100);
    check(!wait.due(100) && wait.due(101) && !wait.expired(100 + remainderMergeFrames)
          && wait.expired(100 + remainderMergeFrames + 1),
          "the merge waits for the swap's tick and gives up after the deadline");
    wait.clear();
    check(!wait.waiting() && !wait.due(1000) && !wait.expired(1000), "a cleared remainder does nothing");

    // L-291 keeps the existing moves exactly as they were: the replacement is
    // still one swap and the ordinary refill still one top-up.
    auto swapBefore = snap(3,{{3,{1,1}},{14,{1,1}}});
    auto swapAfter = swapBefore;
    swapAfter.slots[3] = {2,1};
    auto swap = planRestock(swapBefore,swapAfter,true,1,false,2);
    check(swap && swap->source == 14 && swap->destination == 3
          && swap->sourceAfter == RestockSlot{2,1} && swap->destinationAfter == RestockSlot{1,1},
          "the replacement refill is still the same single swap");
    auto before = snap(3,{{3,{1,7}},{20,{1,32}}});
    auto after = before;
    after.slots[3].count = 6;
    auto plan = planRestock(before,after,true,64);
    check(plan && plan->source == 20 && plan->destination == 3
          && plan->sourceAfter.empty() && plan->destinationAfter == RestockSlot{1,38},
          "an ordinary refill is still one unchanged move");
}
