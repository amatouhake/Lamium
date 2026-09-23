#include "features/inventory/RestockPlan.h"
#include <stdexcept>

void restockPlanTests() {
    using namespace lamium::inventory;
    auto check = [](bool pass) { if (!pass) throw std::runtime_error("restock planning invariant"); };
    RestockSnapshot before;
    before.context = 7;
    before.selected = 3;
    before.slots[3] = {2,1};
    before.slots[0] = {2,64}; // Other hotbar slots stay untouched.
    before.slots[9] = {1,64}; // Different components/kind.
    before.slots[10] = {2,32,true};
    before.slots[11] = {2,16};
    before.slots[12] = {2,64};
    auto after = before;
    after.slots[3] = {};
    auto plan = planRestock(before,after,true);
    check(plan && plan->source == 11 && plan->destination == 3 && plan->stillValid(after));
    check(!planRestock(before,after,false));
    auto changed = after;
    changed.context++;
    check(!planRestock(before,changed,true) && !plan->stillValid(changed));
    changed = after;
    changed.selected = 4;
    check(!planRestock(before,changed,true) && !plan->stillValid(changed));
    changed = after;
    changed.slots[3] = {3,1}; // Bowl, bucket, or a manually placed stack.
    check(!planRestock(before,changed,true) && !plan->stillValid(changed));
    changed = after;
    changed.slots[11].count--;
    check(!plan->stillValid(changed));
    check(!planRestock(before,changed,true)); // No alternate after interference.
    changed.slots[12] = {};
    check(!planRestock(before,changed,true));
    changed = after;
    changed.slots[0].count--;
    check(!planRestock(before,changed,true)); // Other hotbar mutation cancels.
    changed = after;
    changed.slots[20] = {4,1};
    check(!planRestock(before,changed,true)); // Pickup/manual move cancels.
    changed = after;
    changed.slots[3].locked = true;
    check(!planRestock(before,changed,true) && !plan->stillValid(changed));
    before.slots[3].count = 2;
    check(!planRestock(before,after,true));
    before.slots[3] = {};
    check(!planRestock(before,after,true));
    before.selected = after.selected = -1;
    check(!planRestock(before,after,true));
    before.selected = after.selected = 36;
    check(!planRestock(before,after,true));
}
