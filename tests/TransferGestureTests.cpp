#include "features/inventory/transfer/TransferGesture.h"
#include <array>

extern void check(bool, const char*);

void transferGestureTests() {
    using namespace lamium::inventory::transfer;
    check(enabled(Gesture::OneWheel, {true, true, true, true})
        && enabled(Gesture::StackWheel, {true, true, true, true})
        && enabled(Gesture::StackDrag, {true, true, true, true})
        && enabled(Gesture::OneDrag, {true, true, true, true}),
        "all transfer gestures can be enabled independently");
    check(!enabled(Gesture::OneWheel, {false, true, true, true})
        && !enabled(Gesture::StackWheel, {true, false, true, true})
        && !enabled(Gesture::StackDrag, {true, true, false, true})
        && !enabled(Gesture::OneDrag, {true, true, true, false})
        && !enabled(Gesture::None, {true, true, true, true}),
        "disabled gestures leave their input to vanilla");
    check(dragGesture(1, true, false) == Gesture::StackDrag, "Shift left drag transfers stacks");
    check(dragGesture(1, false, true) == Gesture::OneDrag
        && dragGesture(2, false, true) == Gesture::None,
        "Ctrl left drag transfers one while Ctrl right drag stays vanilla");
    check(dragGesture(1, true, true) == Gesture::OneDrag
        && dragGesture(2, true, false) == Gesture::KeepOneDrag
        && dragGesture(2, true, true) == Gesture::None,
        "Ctrl takes priority on left drag; only Shift without Ctrl uses keep-one right drag");
    check(wheelGesture(false, true) == Gesture::MatchingWheel && wheelGesture(true, true) == Gesture::AllWheel
        && mouseGesture(1, false, false, true) == Gesture::MatchingClick
        && mouseGesture(1, true, false, true) == Gesture::AllClick
        && mouseGesture(2, true, false, true) == Gesture::None,
        "bulk wheel and Alt clicks use their own gestures without starting an Alt right drag");
    GestureOptions disabled{true, true, true, true, false, false, false, false, false};
    ConsumedButtons buttons;
    buttons.consume(1); buttons.consume(2);
    check(buttons.release(1) && buttons.consumed(2) && buttons.release(2) && !buttons.release(1),
        "overlapping left and right transfer presses each consume exactly their own release");
    buttons.consume(1); buttons.press(1);
    check(!buttons.release(1) && !buttons.release(4),
        "a fresh vanilla press clears a lost release without swallowing another button");
    check(!enabled(Gesture::MatchingWheel, disabled) && !enabled(Gesture::AllWheel, disabled)
        && !enabled(Gesture::MatchingClick, disabled) && !enabled(Gesture::AllClick, disabled)
        && !enabled(Gesture::KeepOneDrag, disabled) && enabled(Gesture::OneWheel, disabled),
        "new switches leave modified inputs vanilla without falling back to an old gesture");
    for (int count = 0; count <= 64; ++count)
        check(amount(Gesture::KeepOneDrag, count) == std::max(0, count - 1), "keep-one never requests the last item");
    std::array<SweepSource, 6> sweep{{{true, true, true}, {true, true, false}, {false, true, false},
                                    {true, false, false}, {true, true, false}, {true, true, false}}};
    check(sweepOrder(sweep, false, 5) == std::vector<int>{4, 1, 0, 5},
        "matching sweep includes hotbar, skips other kinds and empties, and leaves hovered source last");
    check(sweepOrder(sweep, true, 5) == std::vector<int>{4, 3, 1, 5} && sweepOrder(sweep, true) == std::vector<int>{5, 4, 3, 1},
        "everything from the main inventory leaves the hotbar and includes other kinds");
    check(sweepOrder(sweep, true, 0) == std::vector<int>{0},
        "everything with a hotbar slot hovered moves the hotbar only");
    check(hotbarSlot("inventory_items", 8, 36) && !hotbarSlot("inventory_items", 9, 36)
        && !hotbarSlot("inventory_items", 8, 27) && hotbarSlot("combined_hotbar_and_inventory_items", 8, 0)
        && hotbarSlot("hotbar_items", 0, 27), "hotbar detection works for combined and split inventories");
    check(collectionSide("furnace_fuel_items", 0, 27, false, true) == Side::Storage
        && !collectionSide("furnace_fuel_items", 0, 27, false)
        && !collectionSide("recipe_blocks", 0, 27, false, true)
        && !collectionSide("cursor_items", 0, 27, false, true)
        && !collectionSide("creative_output_items", 0, 27, false, true)
        && !collectionSide("inventory_items", -1, 27, false)
        && readOnlyCollection("furnace_output_items") && !readOnlyCollection("brewing_result_items"),
        "native routes enable real role slots but not catalogs, cursor or receiving output slots");
    check(routeTarget("furnace_fuel_items", Side::Storage, true)
        && routeTarget("furnace_ingredient_items", Side::Storage, true)
        && routeTarget("brewing_result_items", Side::Storage, true)
        && !routeTarget("furnace_output_items", Side::Storage, true)
        && !routeTarget("inventory_items", Side::Storage, true)
        && !routeTarget("hotbar_items", Side::Storage, true)
        && !routeTarget("recipe_blocks", Side::Storage, true)
        && !routeTarget("beacon_payment_items", Side::Storage, false)
        && routeTarget("combined_hotbar_and_inventory_items", Side::Player, false)
        && !routeTarget("beacon_payment_items", Side::Player, true),
        "native order filtering retains role order and rejects same-side fallbacks and output destinations");
    using C = CountChange;
    check(conservedMove(std::array{C{true, 64, 1}, C{false, 62, 64}, C{false, 0, 61}}, 63),
        "one source can leave one while distributing the requested amount across targets");
    check(!conservedMove(std::array{C{true, 64, 0}, C{false, 0, 64}}, 63)
        && !conservedMove(std::array{C{true, 64, 1}, C{false, 0, 62}}, 63)
        && !conservedMove(std::array{C{true, 64, 1}, C{false, 0, 64}}, 63)
        && !conservedMove(std::array{C{true, 64, 64}, C{false, 0, 0}}, 64)
        && !conservedMove(std::array{C{true, 10, 9}, C{false, 4, 5, false}}, 1)
        && !conservedMove(std::array{C{true, 10, 9}, C{false, 4, 4, false}, C{false, 0, 1}}, 1)
        && !conservedMove(std::array{C{true, 10, 9}, C{false, 5, 4}, C{false, 0, 2}}, 1),
        "candidate transfers must conserve counts without deleting, adding, swapping or changing unrelated stacks");
    check(wheelDestination(1) == Side::Storage && wheelDestination(-1) == Side::Player
        && otherSide(Side::Storage) == Side::Player,
        "wheel direction chooses the destination regardless of hover side");
    check(canReceive(63, 64) && !canReceive(64, 64) && !canReceive(1, 1),
        "ordinary wheel cannot bring an item into a full hovered stack");
    constexpr std::array<unsigned char, 6> matches{1, 1, 0, 1, 0, 1};
    check(chooseSource(matches, 5) == 3 && chooseSource(matches, 3) == 5
        && chooseSource(std::span<unsigned char const>{matches.data(), 1}, 0) == 0,
        "shift wheel scans high slots first and uses hovered source last");
    check(amount(wheelGesture(false), 64) == 1 && amount(wheelGesture(true), 64) == 64
        && amount(Gesture::StackDrag, 12) == 12 && amount(Gesture::OneDrag, 12) == 1,
        "gesture chooses one item or the current stack");
    check(collectionSide("container_items", 0, 36, false) == Side::Storage
        && collectionSide("inventory_items", 20, 36, false) == Side::Player
        && collectionSide("hotbar_items", 3, 36, false) == Side::Player,
        "storage screens keep storage above and the whole player inventory below");
    check(collectionSide("inventory_items", 20, 36, true) == Side::Storage
        && collectionSide("inventory_items", 4, 36, true) == Side::Player
        && collectionSide("hotbar_items", 4, 36, true) == Side::Player
        && collectionSide("inventory_items", 4, 27, true) == Side::Storage,
        "the inventory screen puts the main inventory above and the hotbar below");
    check(!collectionSide("armor_items", 0, 36, true) && !collectionSide("container_items", 0, 36, true)
        && !collectionSide("crafting_input_items", 0, 36, false),
        "armor, crafting and unknown grids never take part");
    using D = Destination;
    std::array<D, 4> slots{D{false, false, 10, 64}, D{true}, D{false, true, 64, 64}, D{false, true, 30, 64}};
    check(chooseDestination(slots) == 3, "a matching stack with room is filled before an empty slot");
    slots[3].count = 64;
    check(chooseDestination(slots) == 1, "full matching stacks fall back to the first empty slot");
    slots[1] = D{false, false, 1, 64};
    check(chooseDestination(slots) == -1, "no room leaves the item where it is");
    check(vanillaShift(Gesture::StackDrag, true, true) && !vanillaShift(Gesture::StackDrag, false, true)
        && !vanillaShift(Gesture::OneDrag, true, true) && !vanillaShift(Gesture::StackDrag, true, false),
        "only Shift on worn items in the inventory screen stays vanilla");
    check(wornItem("minecraft:diamond_chestplate", true) && wornItem("minecraft:elytra", false)
        && wornItem("minecraft:zombie_head", false) && wornItem("minecraft:carved_pumpkin", false)
        && !wornItem("minecraft:pumpkin", false) && !wornItem("minecraft:stick", false)
        && !wornItem("custom:elytra", false),
        "worn items are armor, elytra, heads and carved pumpkins");
    check(room(D{true, false, 0, 16}) == 16 && room(D{false, true, 10, 16}) == 6,
        "room follows the item's own stack limit");
    // L-169 2026-10-15: drop gestures with the drop key held, the held stack's Shift + click.
    check(mouseGesture(1, false, true, false, true) == Gesture::DropOneDrag
        && mouseGesture(1, true, false, false, true) == Gesture::DropAllDrag
        && mouseGesture(2, true, false, false, true) == Gesture::DropKeepOneDrag
        && mouseGesture(1, false, false, false, true) == Gesture::None
        && mouseGesture(1, false, false, true, true) == Gesture::None,
        "the drop key turns the drags into drops and leaves other presses vanilla");
    check(mouseGesture(1, true, false, false, false, true) == Gesture::CursorMatchingClick
        && mouseGesture(1, false, false, false, false, true) == Gesture::None
        && mouseGesture(2, true, false, false, false, true) == Gesture::None
        && mouseGesture(1, true, false, true, false, true) == Gesture::None,
        "with items on the cursor only Shift + left click is taken");
    check(dragging(Gesture::DropAllDrag) && !dragging(Gesture::DropMatching) && dropping(Gesture::DropMatching)
        && !dropping(Gesture::StackDrag) && extended(Gesture::CursorMatchingClick) && extended(Gesture::DropOneDrag),
        "drops drag like moves and need the native extensions");
    check(amount(Gesture::DropOneDrag, 12) == 1 && amount(Gesture::DropKeepOneDrag, 12) == 11
        && amount(Gesture::DropKeepOneDrag, 1) == 0 && amount(Gesture::DropAllDrag, 12) == 12,
        "drops ask for one, all but one or the stack");
    GestureOptions off{true, true, true, true};
    off.dropMatching = false;
    check(!enabled(Gesture::DropMatching, off) && enabled(Gesture::DropOneDrag, off)
        && enabled(Gesture::CursorMatchingClick, off), "each drop gesture has its own switch");
    std::vector<CountChange> drop{{true, 12, 11}, {false, 5, 5}, {false, 0, 0}};
    check(droppedMove(drop, 1) && !droppedMove(drop, 0), "a drop only shrinks its source");
    drop[1].after = 6;
    check(!droppedMove(drop, 1), "a drop that moves items elsewhere is not a drop");
    Routes grid{false, true}; // a crafting grid: nothing is routed into it
    check(!routed(Gesture::StackDrag, Side::Player, grid) && !routed(Gesture::MatchingClick, Side::Player, grid)
        && routed(Gesture::StackDrag, Side::Storage, grid) && routed(Gesture::AllClick, Side::Storage, grid),
        "a click moving into a side without routes stays vanilla, the other direction is taken");
    check(!routed(Gesture::StackWheel, Side::Player, grid, 1) && !routed(Gesture::OneWheel, Side::Player, grid, 1)
        && routed(Gesture::OneWheel, Side::Storage, grid, 1) && routed(Gesture::StackWheel, Side::Storage, grid, -1)
        && !routed(Gesture::MatchingWheel, Side::Storage, grid, 1),
        "the wheel needs a route toward its direction unless it fills the hovered stack");
    check(routed(Gesture::DropAllDrag, Side::Player, Routes{false, false})
        && !routed(Gesture::CursorMatchingClick, Side::Storage, grid) && routed(Gesture::StackDrag, Side::Player, Routes{}),
        "drops need no route, the held stack needs one toward its slot, and routes default open");
    auto bytes = dropAmountBytes(true);
    check(bytes[0] == 1 && bytes[4] == 2 && dropAmountBytes(false)[4] == 0 && bytes[8] == 0,
        "drop amounts follow the traced take type and kind");
}
