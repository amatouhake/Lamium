#include "features/inventory/RestockPlan.h"
#include "features/inventory/RestockUse.h"
void check(bool, char const*);

void restockPlanTests() {
    using namespace lamium::inventory;
    RestockSnapshot before;
    before.context = 7;
    before.selected = 3;
    before.slots[3] = {2,7};
    before.slots[0] = {2,64};
    before.slots[9] = {1,64};
    before.slots[10] = {2,32,true};
    before.slots[11] = {2,32};
    before.slots[12] = {2,16};
    auto after = before;
    after.slots[3].count = 6;
    auto plan = planRestock(before,after,true,64);
    check(plan && plan->source == 11 && plan->destination == 3 && plan->stillValid(after),
          "main inventory supplies the same selected slot, skipping locks and different components");
    auto predicted = plan->predicted();
    check(predicted.selected == 3 && predicted.slots[3].count == 38 && predicted.slots[11].empty()
          && predicted.slots[0].count == 64, "partial refill consumes one source and preserves other hotbar stacks");
    before.slots[11].count = after.slots[11].count = 64;
    plan = planRestock(before,after,true,64,true);
    check(plan && plan->source == 11 && plan->sourceAfter.count == 6 && plan->destinationAfter.count == 64,
          "main inventory wins even with hotbar sources on; refill leaves the excess at source");
    check(!planRestock(before,after,false,64), "count changes without use evidence never authorize restock");
    for (int slot = 0; slot < 36; ++slot) {
        auto changed = after;
        changed.slots[slot] = {8,1};
        check(!plan->stillValid(changed), "every slot is revalidated before sending, including unrelated slots");
        check(!planRestock(before,changed,true,64), "unrelated mutations cannot silently choose another reserve");
    }
    auto changed = after;
    changed.context++;
    check(!plan->stillValid(changed) && !planRestock(before,changed,true,64), "world generation invalidates a plan");
    changed = after; changed.selected = 4;
    check(!plan->stillValid(changed) && !planRestock(before,changed,true,64), "manual selection changes invalidate a plan");
    changed = after; changed.slots[3].locked = true;
    check(!planRestock(before,changed,true,64), "locked destinations are never changed");
    changed = after; changed.slots[3].count = 5;
    check(!planRestock(before,changed,true,64), "multiple ambiguous decrements are not one consumption");
    plan = planRestock(before,changed,true,64,false,-1,restockThreshold,2);
    check(plan && plan->destinationAfter.count == 64 && plan->sourceAfter.count == 5,
          "two tracked continuous uses account for two decrements");
    check(!planRestock(before,changed,true,64,false,-1,restockThreshold,3) && !planRestock(before,after,true,64,false,-1,restockThreshold,0),
          "the decrement must equal the tracked use count");
    check(!planRestock(before,after,true,0) && !planRestock(before,after,true,256), "invalid stack limits fail open");
    check(!planRestock(before,before,true,64), "unchanged or failed use cannot refill");
    for (int slot = 9; slot < 36; ++slot) before.slots[slot] = after.slots[slot] = {};
    check(!planRestock(before,after,true,64), "hotbar reserves are excluded by default");
    plan = planRestock(before,after,true,64,true);
    check(plan && plan->source == 0 && plan->destination == 3, "opt-in hotbar reserve moves without selecting it");
    before.slots[0] = after.slots[0] = {};
    check(!planRestock(before,after,true,64,true), "no reserve leaves the hand alone");

    auto ordered = [](std::initializer_list<std::pair<int,int>> reserves, int left = 6, bool smallest = false) {
        RestockSnapshot b; b.context = 5; b.selected = 3; b.slots[3] = {2,left + 1};
        for (auto [slot,count] : reserves) b.slots[slot] = {2,count};
        auto a = b; a.slots[3].count = left; if (!left) a.slots[3] = {};
        auto p = planRestock(b,a,true,64,true,-1,restockThreshold,1,-1,smallest);
        return p ? p->source : -1;
    };
    check(ordered({{9,12},{20,32},{30,64}},6,true) == 9 && ordered({{9,12},{20,32},{30,64}}) == 30,
          "smallest first takes the smallest reserve; largest first the largest");
    check(ordered({{12,20},{30,20}},6,true) == 30,
          "smallest first keeps the lower-row tie-break");
    check(ordered({{9,64},{0,1}},6,true) == 9 && ordered({{1,30},{7,20}},6,true) == 7,
          "smallest first applies within each region; the inventory still comes first");
    {
        RestockSnapshot b; b.context = 5; b.selected = 3; b.slots[3] = {2,7};
        b.slots[9] = {2,12}; b.slots[20] = {2,32}; b.slots[30] = {2,64};
        auto a = b; a.slots[3].count = 6;
        auto p = planRestock(b,a,true,64,false,-1,restockThreshold,1,-1,true);
        check(p && p->source == 9 && p->sourceAfter.empty() && p->destinationAfter.count == 18,
              "smallest first empties a partial stack and frees its slot");
    }
    check(ordered({{9,1},{33,64}}) == 33, "the largest main-inventory stack supplies the refill");
    check(ordered({{12,64},{30,64}}) == 30 && ordered({{12,40},{30,40},{20,40}}) == 30,
          "equal main-inventory stacks are taken from the lower rows first");
    check(ordered({{9,1},{0,64}}) == 9, "any main-inventory reserve wins over hotbar reserves");
    check(ordered({{1,20},{7,30}}) == 7 && ordered({{7,20},{1,30}},0) == 1,
          "opt-in hotbar reserves top up and refill largest first");
    check(ordered({{0,20},{5,20}}) == 5 && ordered({{2,20},{6,20}}) == 2 && ordered({{1,20},{5,20}}) == 5,
          "equal hotbar reserves are taken nearest the selection, then from the higher slot");

    for (int maxStack : {1,16,64}) {
        before = {}; before.context = 8; before.selected = 0;
        before.slots[0] = {1,1}; before.slots[9] = {1,maxStack};
        after = before; after.slots[0] = {};
        plan = planRestock(before,after,true,maxStack);
        check(plan && plan->destinationAfter.count == maxStack && plan->sourceAfter.empty(),
              "depletion moves the reserve for single, sixteen and sixty-four stacks");
        if (maxStack == 1) continue;
        before.slots[0].count = 7; after = before; after.slots[0].count = 6;
        plan = planRestock(before,after,true,maxStack);
        check(plan && plan->destinationAfter.count == maxStack && plan->sourceAfter.count == 6,
              "partial refills respect each item's maximum stack size");
        check(!planRestock(before,after,true,maxStack,false,-1,0), "internal threshold zero disables partial refill only");
        before.slots[0].count = 8; after.slots[0].count = 7;
        check(!planRestock(before,after,true,maxStack), "seven left is above the default six-item threshold");
        check(planRestock(before,after,true,maxStack,false,-1,7).has_value(),
              "a higher user threshold refills at seven left");
        check(planRestock(before,after,true,maxStack,false,-1,200).has_value(),
              "a threshold above the stack limit still refills below a full stack");
    }
    before = {}; before.context = 9; before.selected = 2;
    before.slots[2] = {1,1}; before.slots[14] = {1,1};
    after = before; after.slots[2] = {2,1};
    check(!planRestock(before,after,true,1), "unrecognized replacement is not an empty slot");
    plan = planRestock(before,after,true,1,false,2);
    check(plan && plan->sourceAfter == RestockSlot{2,1} && plan->destinationAfter == RestockSlot{1,1},
          "known remainder exchanges with the reserve even without an empty inventory slot");
    for (int slot = 0; slot < 36; ++slot)
        if (slot != 2 && slot != 14) before.slots[slot] = after.slots[slot] = {7,64};
    check(planRestock(before,after,true,1,false,2).has_value(), "a full inventory still permits a two-slot exchange");
    before.slots[14].count = after.slots[14].count = 16;
    plan = planRestock(before,after,true,16,false,2);
    check(plan && plan->destinationAfter.count == 16 && plan->sourceAfter.count == 1,
          "a single final bottle can be exchanged with a whole compatible reserve");
    auto hotbarBefore = before, hotbarAfter = after;
    hotbarBefore.slots[14] = hotbarAfter.slots[14] = {7,64};
    hotbarBefore.slots[6] = hotbarAfter.slots[6] = {1,1};
    plan = planRestock(hotbarBefore,hotbarAfter,true,16,true,2);
    check(plan && plan->source == 6, "a hotbar reserve replaces a remainder when the main inventory has none");
    before.slots[2].count = 2;
    check(!planRestock(before,after,true,16,false,2), "replacement before the last consumed item is ambiguous");
    check(restockRemainder("minecraft:water_bucket") == "minecraft:bucket"
          && restockRemainder("minecraft:mushroom_stew") == "minecraft:bowl"
          && restockRemainder("minecraft:potion") == "minecraft:glass_bottle"
          && restockRemainder("addon:mushroom_stew").empty()
          && restockRemainder("minecraft:diamond_pickaxe").empty(),
          "only explicitly supported remainder transformations qualify");

    // L-251: a shulker box or a bucket can stay out of Hand Restock. The
    // caller names the refilled item's family and passes the three switches.
    auto exchange = [](std::string_view used, RestockSwitches switches) {
        RestockSnapshot b; b.context = 11; b.selected = 2;
        b.slots[2] = {1,1}; b.slots[14] = {1,1};
        auto a = b; a.slots[2] = {2,1}; // The last item turned into its container.
        return planRestock(b,a,true,1,false,2,restockThreshold,1,-1,false,
            restockFamily(used),switches).has_value();
    };
    auto refill = [](std::string_view used, RestockSwitches switches) {
        RestockSnapshot b; b.context = 12; b.selected = 2;
        b.slots[2] = {1,1}; b.slots[14] = {1,1};
        auto a = b; a.slots[2] = {};
        return planRestock(b,a,true,1,false,-1,restockThreshold,1,-1,false,
            restockFamily(used),switches).has_value();
    };
    check(restockFamily("minecraft:shulker_box") == RestockFamily::ShulkerBox
          && restockFamily("minecraft:undyed_shulker_box") == RestockFamily::ShulkerBox
          && restockFamily("minecraft:white_shulker_box") == RestockFamily::ShulkerBox
          && restockFamily("minecraft:bucket") == RestockFamily::Bucket
          && restockFamily("minecraft:water_bucket") == RestockFamily::Bucket
          && restockFamily("minecraft:cod_bucket") == RestockFamily::Bucket
          && restockFamily("minecraft:potion") == RestockFamily::Other
          && restockFamily("") == RestockFamily::Other
          && restockFamily("addon:shulker_box") == RestockFamily::Other
          && restockFamily("addon:fake_bucket") == RestockFamily::Other,
          "only vanilla shulker boxes and buckets belong to an excluded family");
    for (auto used : {"minecraft:water_bucket","minecraft:lava_bucket","minecraft:milk_bucket","minecraft:powder_snow_bucket"}) {
        check(exchange(used,{}) && exchange(used,{.shulkerBoxes = false}),
              "an emptied water, lava, milk or powder snow bucket is exchanged while the bucket switch is on");
        check(!exchange(used,{.buckets = false}),
              "the bucket switch off leaves the empty bucket in place");
    }
    check(refill("minecraft:white_shulker_box",{}) && !refill("minecraft:white_shulker_box",{.shulkerBoxes = false})
          && refill("minecraft:white_shulker_box",{.buckets = false}),
          "a placed or used shulker box is refilled only while its own switch is on");
    for (bool shulkerBoxes : {true,false}) for (bool buckets : {true,false})
        for (auto used : {"minecraft:potion","minecraft:honey_bottle","minecraft:mushroom_stew",
                          "minecraft:rabbit_stew","minecraft:beetroot_soup"})
            check(exchange(used,{.shulkerBoxes = shulkerBoxes,.buckets = buckets}),
                  "the other remainders are exchanged whatever the two switches say");
    check(refill("minecraft:diamond_pickaxe",{})
          && refill("minecraft:diamond_pickaxe",{.shulkerBoxes = false,.buckets = false}),
          "an ordinary item is refilled whatever the two switches say");
    for (auto used : {"minecraft:water_bucket","minecraft:potion","minecraft:honey_bottle","minecraft:mushroom_stew"})
        check(!exchange(used,{.master = false}), "the master switch still refuses every remainder exchange");
    check(!refill("minecraft:diamond_pickaxe",{.master = false}), "the master switch still refuses an ordinary refill");
    check(!refill("minecraft:water_bucket",{.buckets = false}),
          "a bucket that leaves no container is refused by the bucket switch too");

    RestockUseEvidence evidence{42,true};
    check(evidence.observe(RestockUseSend::Place,42,true) && evidence.ready(), "placement has one matching use send");
    check(evidence.observe(RestockUseSend::Use,42,true), "one same-tick secondary Use belongs to placement");
    check(!evidence.observe(RestockUseSend::Use,42,true), "a third send cannot be hidden as a duplicate");
    evidence = {42,true}; evidence.observe(RestockUseSend::Place,42,true);
    check(!evidence.observe(RestockUseSend::Use,43,true), "a later use cannot be deduplicated into an old placement");
    check(!evidence.observe(RestockUseSend::Place,42,true), "a second placement is a new action");
    evidence = {42,false};
    check(!evidence.observe(RestockUseSend::Use,42,false) && !evidence.ready(), "offhand and other slots do not match");
    evidence.timed = true;
    check(evidence.observe(RestockUseSend::Use,42,true) && !evidence.ready(), "starting food does not imply consumption");
    check(evidence.observe(RestockUseSend::Use,46,true) && !evidence.ready(),
          "a repeated Use while eating belongs to the same timed use");
    check(!evidence.observe(RestockUseSend::Use,46,false), "a Use for another slot while eating is not absorbed");
    check(!evidence.observe(RestockUseSend::Release,75,true) && !evidence.ready(),
          "a release before completion interrupts eating");
    evidence.completed = true;
    check(evidence.ready(), "completed eating is ready without a release while use is held");
    check(evidence.observe(RestockUseSend::Release,76,true) && evidence.ready(), "a release after completion belongs to it");
    check(!evidence.observe(RestockUseSend::Other,75,true), "unrelated transactions cancel correlation");
    check(!restockTimedCompletion(2,32) && restockTimedCompletion(16,32) && restockTimedCompletion(32,32)
          && !restockTimedCompletion(5,0),
          "a completion right after the next use starts does not complete that use");

    evidence = {42,true};
    evidence.observe(RestockUseSend::Place,42,true);
    evidence.observe(RestockUseSend::Use,42,true);
    check(evidence.beginSecondaryCallback(42),
          "the secondary send may precede its callback without discarding placement");
    check(!evidence.beginSecondaryCallback(42), "a second secondary callback is not deduplicated");
    evidence = {42,true};
    evidence.observe(RestockUseSend::Place,42,true);
    check(evidence.beginSecondaryCallback(42) && evidence.observe(RestockUseSend::Use,42,true),
          "the secondary callback may also precede its send");
    evidence = {42,true};
    check(!evidence.beginSecondaryCallback(43), "later-tick callbacks do not reuse an old placement");
    evidence = {42,false};
    check(!evidence.beginSecondaryCallback(42), "ordinary uses have no placement secondary callback");

    check(!restockSettled(false,0) && !restockSettled(false,restockQuietMs - 1)
          && restockSettled(false,restockQuietMs) && restockSettled(true,0),
          "a move waits for the server update or a quiet period after the last use");
    check(restockUseStarted(false,true) && restockUseStarted(true,false) && !restockUseStarted(false,false),
          "starting food fails the use callback yet is tracked; other failed uses are not");
}
