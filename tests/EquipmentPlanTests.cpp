#include "features/inventory/EquipmentPlan.h"
#include "features/inventory/ToolChoice.h"
#include "features/inventory/RestockPlan.h"
void check(bool, char const*);

void equipmentPlanTests() {
    using namespace lamium::inventory;
    check(aboutToBreak(1561,1560) && aboutToBreak(1561,1561) && !aboutToBreak(1561,1559) && !aboutToBreak(0,0),
          "one durability left (or none) is about to break; non-damageable items never are");

    std::vector<EnchantLevel> efficiency5{{15,5}}, efficiency4unbreaking3{{15,4},{17,3}};
    check(enchantDistance(efficiency5,efficiency5) == 0 && enchantDistance(efficiency5,efficiency4unbreaking3) == 4
          && enchantDistance(efficiency4unbreaking3,efficiency5) == 4 && enchantDistance({},efficiency5) == 5,
          "enchantment distance counts level gaps and unshared levels symmetrically");
    check(chooseReplacement({{12,4,900},{20,0,100},{30,0,100}}) == 30,
          "closest enchantments win before durability; ties take the higher slot");
    check(chooseReplacement({{12,0,100},{20,0,900}}) == 20, "more durability wins among equal enchantments");
    check(!chooseReplacement({{12,0,1},{13,0,0}}), "a replacement that is itself about to break is never chosen");
    check(guardMining(true,true,false) == GuardMining::Wait && guardMining(true,false,false) == GuardMining::Stop
          && guardMining(true,false,true) == GuardMining::Continue && guardMining(false,false,false) == GuardMining::Continue,
          "mining waits for a swap, stops without one, and a new press after a stop mines on");

    std::array<ToolCandidate,36> tools{};
    tools[20] = {8,true}; tools[30] = {8,true}; tools[10] = {4,true};
    check(chooseInventoryTool(tools,0) == 30, "the fastest inventory tool is fetched; ties take the higher slot");
    tools[5] = {2,true};
    check(!chooseInventoryTool(tools,0), "an effective hotbar tool is selected instead of fetching one");
    tools[5] = {}; tools[0] = {3,true};
    check(!chooseInventoryTool(tools,0), "an effective held tool stays");

    ElytraSwapState state;
    ElytraInput in;
    in.enabled = true; in.wearingElytra = true; in.landingTicks = 2; in.chestplateAvailable = true;
    in.onGround = true;
    check(elytraStep(state,in) == ElytraStep::None && !state.active, "an elytra worn by hand on the ground is not followed");
    in.onGround = false; in.gliding = true;
    check(elytraStep(state,in) == ElytraStep::None && state.active, "an elytra worn by hand is followed once it glides");
    in.gliding = false; in.onGround = true;
    check(elytraStep(state,in) == ElytraStep::None && elytraStep(state,in) == ElytraStep::WearChestplate,
          "after landing, a hand-worn elytra gives way to the best chestplate");
    state = {true,20,false,false,0};
    check(elytraStep(state,in) == ElytraStep::None && elytraStep(state,in) == ElytraStep::None,
          "an elytra put on by key on the ground waits for the key");
    state = {true,20,true,false,0}; in.returnSlotHoldsChest = true;
    check(elytraStep(state,in) == ElytraStep::None && elytraStep(state,in) == ElytraStep::TakeOff,
          "after a firework jump or glide, the remembered chest item returns");
    state = {true,20,true,false,0}; in.landingTicks = 60;
    for (int tick = 1; tick < 60; ++tick) {
        in.onGround = tick % 12 < 3; // Sprint jumping.
        check(elytraStep(state,in) == ElytraStep::None, "the delay runs while sprint jumping");
    }
    in.onGround = false;
    check(elytraStep(state,in) == ElytraStep::None, "the swap waits for the ground after the delay");
    in.onGround = true;
    check(elytraStep(state,in) == ElytraStep::TakeOff, "the next touch of the ground after the delay swaps back");
    state = {true,20,true,true,50}; in.gliding = true; in.onGround = false;
    check(elytraStep(state,in) == ElytraStep::None && state.ticks == 0 && !state.landed, "a new glide restarts the delay");
    in.gliding = false; in.onGround = true; in.landingTicks = 2;
    state = {true,20,true,false,0}; in.returnSlotHoldsChest = false;
    elytraStep(state,in);
    check(!state.returnSlot && elytraStep(state,in) == ElytraStep::WearChestplate,
          "a changed return slot falls back to the best chestplate");
    state = {true,std::nullopt,true,false,0}; in.chestplateAvailable = false;
    check(elytraStep(state,in) == ElytraStep::None && elytraStep(state,in) == ElytraStep::Forget,
          "without any chestplate the elytra stays on");
    state = {true,20,true,false,0}; in.wearingElytra = false;
    check(elytraStep(state,in) == ElytraStep::Forget, "taking the elytra off by hand forgets it");
    state = {true,20,true,false,0}; in.wearingElytra = true; in.enabled = false;
    check(elytraStep(state,in) == ElytraStep::Forget, "disabling the feature leaves the equipment as it is");
    // L-280: a Riptide launch waits for a frame the player can actually fly.
    in = {};
    in.enabled = true; in.landingTicks = 2; in.chestplateAvailable = true;
    in.returnSlotHoldsChest = true;
    state = {};
    in.riptideLaunch = true; in.inWater = true; in.spinning = true;
    check(elytraStep(state,in) == ElytraStep::None && state.riptideWait,
          "a Riptide launch in water does not swap");
    in.riptideLaunch = false; in.spinning = false; in.rising = true;
    check(elytraStep(state,in) == ElytraStep::None && state.riptideWait,
          "the launch keeps waiting while it still lifts");
    in.rising = false;
    for (int tick = 0; tick < 9; ++tick) elytraStep(state,in);
    check(state.riptideWait, "a short stall does not cancel the wait");
    check(elytraStep(state,in) == ElytraStep::None && !state.riptideWait,
          "swimming ends the wait without swapping");
    state = {};
    in.riptideLaunch = true; in.inWater = true; in.spinning = true; in.rising = false;
    elytraStep(state,in);
    in.riptideLaunch = false; in.spinning = false; in.inWater = false; in.onGround = false;
    check(elytraStep(state,in) == ElytraStep::PutOn,
          "the elytra goes on once the player is out of the water in the air");
    // L-280: the launch frame itself has to reach the state machine, even with
    // nothing active yet; that was the bug that made the wait never start.
    {
        ElytraSwapState fresh;
        ElytraInput launch;
        launch.enabled = true; launch.riptideLaunch = true; launch.inWater = true;
        check(followsElytra(fresh, launch), "a launch alone keeps the tick running");
        launch.riptideLaunch = false;
        check(!followsElytra(fresh, launch), "without an elytra or a launch the tick stops");
    }
    // L-281: landing in water starts the same delay as landing on ground.
    in = {};
    in.enabled = true; in.wearingElytra = true; in.landingTicks = 2; in.chestplateAvailable = true;
    in.returnSlotHoldsChest = true;
    state = {true,20,true,false,0};
    in.inWater = true;
    check(elytraStep(state,in) == ElytraStep::None && state.landed, "landing in water starts the delay");
    check(elytraStep(state,in) == ElytraStep::TakeOff, "the chestplate comes back after the delay in water");
    state = {true,20,true,false,0}; in.gliding = true; in.inWater = true;
    check(elytraStep(state,in) == ElytraStep::None && !state.landed, "gliding over water is not a landing");
    check(chooseChestplate({{10,6,0,0,100},{20,8,2,0,50},{30,8,3,0,10}}) == 30
          && chooseChestplate({{10,8,3,1,10},{20,8,3,0,500}}) == 10 && !chooseChestplate({}),
          "the chestplate with the highest protection is chosen");

    check(jumpHasFireworks(true,false,false) && jumpHasFireworks(true,false,true)
              && jumpHasFireworks(true,true,false) && jumpHasFireworks(true,true,true),
          "rockets in the main hand always drive the firework jump");
    check(jumpHasFireworks(false,true,true),
          "L-246: with Fake Offhand on, its rockets drive the jump with any main-hand item");
    check(!jumpHasFireworks(false,false,false) && !jumpHasFireworks(false,false,true)
              && !jumpHasFireworks(false,true,false),
          "with Fake Offhand off or its slot empty, only the main hand counts");

    RestockSnapshot before; before.context = 3; before.selected = 2;
    before.slots[offhandSlot] = {1,1}; before.slots[20] = {1,1}; before.slots[25] = {1,1}; before.slots[2] = {2,10};
    auto after = before; after.slots[offhandSlot] = {};
    auto plan = planRestock(before,after,true,1,false,-1,restockThreshold,1,offhandSlot);
    check(plan && plan->destination == offhandSlot && plan->source == 25 && plan->stillValid(after)
          && plan->predicted().slots[offhandSlot] == RestockSlot{1,1,false},
          "a consumed offhand totem is refilled in the offhand from the main inventory");
    check(!planRestock(before,after,true,1) , "an offhand change is not a main-hand consumption");
    auto changed = after; changed.slots[2].count = 9;
    check(!planRestock(before,changed,true,1,false,-1,restockThreshold,1,offhandSlot),
          "the offhand plan requires every other slot unchanged, the hand included");
    before.slots[offhandSlot] = {3,16}; after = before; after.slots[offhandSlot].count = 5; before.slots[offhandSlot].count = 6;
    before.slots[20] = after.slots[20] = {3,16}; before.slots[25] = after.slots[25] = {};
    plan = planRestock(before,after,true,16,false,-1,restockThreshold,1,offhandSlot);
    check(plan && plan->source == 20 && plan->destinationAfter.count == 16 && plan->sourceAfter.count == 5,
          "an offhand stack tops up like the hand when planned");
    before.slots[20] = after.slots[20] = {}; before.slots[2] = after.slots[2] = {3,16};
    check(!planRestock(before,after,true,16,true,-1,restockThreshold,1,offhandSlot),
          "the held stack never supplies the offhand");
}
