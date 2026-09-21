// Focused checks for the pure sort planner. These tests use synthetic slot
// states: the `group` id stands in for vanilla's stackability verdict, so they
// prove the planning arithmetic, NOT that any two real items may stack. That
// half is decided by Minecraft at runtime (ItemStackBase::isStackable) and is
// covered by in-game validation.
//
// Built with `xmake build LamiumTests`, run with `xmake run LamiumTests`.

#include "features/inventory/sort/SortPlanner.h"

#include <cstdio>
#include <map>
#include <string>
#include <vector>

using namespace lamium::inventory::sort;

// Semantic ordering checks live in SortKeyTest.cpp.
int runSortKeyTests();

namespace {

int gFailures = 0;

void check(bool ok, char const* what, int line) {
    if (!ok) {
        ++gFailures;
        std::printf("FAIL (line %d): %s\n", line, what);
    }
}

#define CHECK(expr) check((expr), #expr, __LINE__)

// Test vocabulary: a handful of item kinds with distinct keys. Two stacks are
// mergeable only when they share a group; `variant` produces stacks that look
// like the same item type (same name) but that vanilla keeps apart (different
// group and a distinguishing detail), e.g. enchanted vs plain, damaged, named.
SlotStack stack(std::string const& name, int count, int max = 64, int group = -1, std::string detail = "") {
    SlotStack s;
    s.count        = count;
    s.maxStackSize = max;
    s.group        = group;
    s.key.typeName = name;
    s.key.detail   = std::move(detail);
    return s;
}

SlotStack cobble(int count, int group = 1) { return stack("minecraft:cobblestone", count, 64, group); }
SlotStack egg(int count, int group = 2) { return stack("minecraft:egg", count, 16, group); }
SlotStack dirt(int count, int group = 3) { return stack("minecraft:dirt", count, 64, group); }
SlotStack sword(int group, std::string detail = "") { return stack("minecraft:diamond_sword", 1, 1, group, detail); }
SlotStack empty() { return SlotStack::emptySlot(); }

std::vector<SlotStack> region(std::vector<SlotStack> stacks, size_t size = 27) {
    stacks.resize(size);
    return stacks;
}

// Runs the plan through applyOperation from the original state and checks it
// lands exactly on plan.expected (the planner's own simulation).
bool replays(std::vector<SlotStack> slots, Plan const& plan) {
    for (auto const& op : plan.ops) {
        if (!applyOperation(slots, op)) return false;
    }
    if (slots.size() != plan.expected.size()) return false;
    for (size_t i = 0; i < slots.size(); ++i) {
        if (!sameStack(slots[i], plan.expected[i])) return false;
    }
    return true;
}

std::map<int, int> totalsByGroup(std::vector<SlotStack> const& slots) {
    std::map<int, int> totals;
    for (auto const& s : slots) {
        if (!s.empty()) totals[s.group] += s.count;
    }
    return totals;
}

// Fixed slots are not part of the sorted sequence: they are skipped here.
bool emptiesLast(std::vector<SlotStack> const& slots) {
    bool seenEmpty = false;
    for (auto const& s : slots) {
        if (s.fixed()) continue;
        if (s.empty()) seenEmpty = true;
        else if (seenEmpty) return false;
    }
    return true;
}

bool sorted(std::vector<SlotStack> const& slots) {
    SlotStack const* prev = nullptr;
    for (auto const& s : slots) {
        if (s.fixed() || s.empty()) continue;
        if (prev) {
            auto const& a = *prev;
            auto const& b = s;
            if (b.key < a.key) return false;
            if (a.key == b.key && b.group < a.group) return false;
            if (a.key == b.key && a.group == b.group && b.count > a.count) return false;
        }
        prev = &s;
    }
    return true;
}

std::vector<int> counts(std::vector<SlotStack> const& slots, size_t n) {
    std::vector<int> out;
    for (size_t i = 0; i < n && i < slots.size(); ++i) out.push_back(slots[i].count);
    return out;
}

// Common invariants every plan must satisfy.
void checkInvariants(std::vector<SlotStack> const& input, Plan const& plan, int line) {
    check(replays(input, plan), "plan replays onto expected state", line);
    for (size_t i = 0; i < input.size(); ++i) {
        if (!input[i].fixed()) continue;
        check(sameStack(plan.expected[i], input[i]), "fixed slot keeps its contents", line);
        for (auto const& op : plan.ops) {
            check(
                op.from != static_cast<int>(i) && op.to != static_cast<int>(i),
                "no operation touches a fixed slot",
                line
            );
        }
    }
    check(totalsByGroup(input) == totalsByGroup(plan.expected), "item totals preserved per group", line);
    check(emptiesLast(plan.expected), "empty slots are last", line);
    check(sorted(plan.expected), "expected layout is sorted", line);
    for (auto const& s : plan.expected) {
        check(s.empty() || s.count <= s.maxStackSize, "no stack exceeds its max size", line);
    }
    for (auto const& op : plan.ops) {
        check(op.kind == OpKind::Move || op.count == 0, "swap carries no count", line);
        check(op.from != op.to, "operation touches two distinct slots", line);
    }
}

void testEmptyInventory() {
    auto const input = region({});
    auto const plan  = planSort(input);
    CHECK(plan.ops.empty());
    checkInvariants(input, plan, __LINE__);
}

void testAlreadySorted() {
    auto const input = region({cobble(64), cobble(10), dirt(64), egg(16)});
    auto const plan  = planSort(input);
    CHECK(plan.ops.empty());
    checkInvariants(input, plan, __LINE__);
}

void testOneItem() {
    auto const input = region({empty(), empty(), dirt(5)});
    auto const plan  = planSort(input);
    CHECK(plan.ops.size() == 1);
    CHECK(plan.ops[0].kind == OpKind::Move && plan.ops[0].from == 2 && plan.ops[0].to == 0 && plan.ops[0].count == 5);
    CHECK(counts(plan.expected, 3) == std::vector<int>({5, 0, 0}));
    checkInvariants(input, plan, __LINE__);
}

void testMultipleDifferentItems() {
    auto const input = region({dirt(3), empty(), egg(2), empty(), cobble(7)});
    auto const plan  = planSort(input);
    CHECK(plan.expected[0].key.typeName == "minecraft:cobblestone");
    CHECK(plan.expected[1].key.typeName == "minecraft:dirt");
    CHECK(plan.expected[2].key.typeName == "minecraft:egg");
    CHECK(plan.expected[3].empty());
    checkInvariants(input, plan, __LINE__);
}

void testMerge42Plus10() {
    auto const input = region({cobble(42), cobble(10)});
    auto const plan  = planSort(input);
    CHECK(plan.ops.size() == 1);
    CHECK(plan.ops[0].kind == OpKind::Move && plan.ops[0].from == 1 && plan.ops[0].to == 0 && plan.ops[0].count == 10);
    CHECK(counts(plan.expected, 2) == std::vector<int>({52, 0}));
    checkInvariants(input, plan, __LINE__);
}

void testMerge42Plus30() {
    auto const input = region({cobble(42), cobble(30)});
    auto const plan  = planSort(input);
    CHECK(plan.ops.size() == 1);
    CHECK(plan.ops[0].kind == OpKind::Move && plan.ops[0].count == 22);
    CHECK(counts(plan.expected, 2) == std::vector<int>({64, 8}));
    checkInvariants(input, plan, __LINE__);
}

void testMergeSeparatedStacksPartialFirst() {
    // 10 sits before 42: the smaller stack is topped up first (it is the
    // lowest-index one), yielding 52 in slot 0 and nothing in slot 5.
    auto const input = region({cobble(10), dirt(1), empty(), empty(), empty(), cobble(42)});
    auto const plan  = planSort(input);
    CHECK(counts(plan.expected, 3) == std::vector<int>({52, 1, 0}));
    checkInvariants(input, plan, __LINE__);
}

void testMoreThanTwoPartialStacks() {
    auto const input = region({cobble(42), cobble(10), cobble(25)});
    auto const plan  = planSort(input);
    CHECK(counts(plan.expected, 3) == std::vector<int>({64, 13, 0}));
    checkInvariants(input, plan, __LINE__);
}

void testManyPartialStacksMinimalCount() {
    // 7 x 20 = 140 = 64 + 64 + 12 -> exactly three stacks remain.
    std::vector<SlotStack> stacks;
    for (int i = 0; i < 7; ++i) stacks.push_back(cobble(20));
    auto const input = region(stacks);
    auto const plan  = planSort(input);
    CHECK(counts(plan.expected, 4) == std::vector<int>({64, 64, 12, 0}));
    checkInvariants(input, plan, __LINE__);
}

void testAlreadyFullStacks() {
    auto const input = region({cobble(64), cobble(64), cobble(64)});
    auto const plan  = planSort(input);
    CHECK(plan.ops.empty());
    checkInvariants(input, plan, __LINE__);
}

void testFullPlusPartial() {
    auto const input = region({cobble(64), cobble(20), cobble(64)});
    auto const plan  = planSort(input);
    // 148 = 64 + 64 + 20: fulls first, partial last.
    CHECK(counts(plan.expected, 4) == std::vector<int>({64, 64, 20, 0}));
    checkInvariants(input, plan, __LINE__);
}

void testMaxStackSize16() {
    auto const input = region({egg(10), egg(9), egg(3)});
    auto const plan  = planSort(input);
    CHECK(counts(plan.expected, 3) == std::vector<int>({16, 6, 0}));
    checkInvariants(input, plan, __LINE__);
    for (auto const& op : plan.ops) CHECK(op.kind == OpKind::Move);
}

void testMaxStackSize16NoOvershoot() {
    auto const input = region({egg(15), egg(15)});
    auto const plan  = planSort(input);
    CHECK(counts(plan.expected, 2) == std::vector<int>({16, 14}));
    checkInvariants(input, plan, __LINE__);
}

void testNonStackableItemsNeverMerge() {
    // Distinct groups (vanilla says isStackable() is false for every pair).
    auto const input = region({sword(10), empty(), sword(11), sword(12)});
    auto const plan  = planSort(input);
    CHECK(counts(plan.expected, 4) == std::vector<int>({1, 1, 1, 0}));
    for (auto const& op : plan.ops) CHECK(op.kind == OpKind::Move || op.kind == OpKind::Swap);
    checkInvariants(input, plan, __LINE__);
}

void testNonStackableEvenWhenGivenSameGroupByMistake() {
    // Defensive: max stack 1 must never be merged even if a caller hands the
    // stacks a shared group.
    auto const input = region({stack("minecraft:shears", 1, 1, 5), stack("minecraft:shears", 1, 1, 5)});
    auto const plan  = planSort(input);
    CHECK(counts(plan.expected, 2) == std::vector<int>({1, 1}));
    checkInvariants(input, plan, __LINE__);
}

void testLookAlikeButIncompatibleStacksStaySeparate() {
    // Same item name, but vanilla keeps them apart (different groups: e.g. one
    // is renamed / enchanted). They must not be merged, and the detail key
    // keeps the order deterministic.
    auto const plainA = stack("minecraft:cobblestone", 30, 64, 1);
    auto const plainB = stack("minecraft:cobblestone", 30, 64, 1);
    auto const named  = stack("minecraft:cobblestone", 30, 64, 2, "name=Special");
    auto const input  = region({named, plainA, plainB});
    auto const plan   = planSort(input);
    CHECK(counts(plan.expected, 3) == std::vector<int>({60, 30, 0}));
    CHECK(plan.expected[0].group == 1 && plan.expected[1].group == 2);
    checkInvariants(input, plan, __LINE__);
}

void testLookAlikesWithEqualKeysStayContiguous() {
    // Two kinds that vanilla keeps apart but whose keys are identical (the
    // classifier could not tell them apart): each kind must still end up in
    // one contiguous run, ordered by first appearance, biggest stack first.
    auto const plainA   = stack("minecraft:cobblestone", 8, 64, 1);
    auto const specialA = stack("minecraft:cobblestone", 40, 64, 2);
    auto const plainB   = stack("minecraft:cobblestone", 64, 64, 1);
    auto const input    = region({plainA, specialA, plainB});
    auto const plan     = planSort(input);
    CHECK(counts(plan.expected, 4) == std::vector<int>({64, 8, 40, 0}));
    CHECK(plan.expected[0].group == 1 && plan.expected[1].group == 1 && plan.expected[2].group == 2);
    checkInvariants(input, plan, __LINE__);
    // Sorting the result again (groups re-numbered by first appearance)
    // changes nothing.
    auto again = plan.expected;
    for (auto& s : again) {
        if (!s.empty()) s.group = s.group == 1 ? 0 : 1;
    }
    CHECK(planSort(again).ops.empty());
}

void testEmptySlotPlacement() {
    auto const input = region({empty(), cobble(1), empty(), dirt(1), empty(), egg(1)});
    auto const plan  = planSort(input);
    CHECK(!plan.expected[0].empty() && !plan.expected[1].empty() && !plan.expected[2].empty());
    for (size_t i = 3; i < plan.expected.size(); ++i) CHECK(plan.expected[i].empty());
    checkInvariants(input, plan, __LINE__);
}

void testDeterministic() {
    auto const input = region({dirt(3), cobble(42), egg(4), sword(7), cobble(30), empty(), egg(13), sword(8)});
    auto const a     = planSort(input);
    auto const b     = planSort(input);
    CHECK(a.ops.size() == b.ops.size());
    for (size_t i = 0; i < a.ops.size() && i < b.ops.size(); ++i) {
        CHECK(
            a.ops[i].kind == b.ops[i].kind && a.ops[i].from == b.ops[i].from && a.ops[i].to == b.ops[i].to
            && a.ops[i].count == b.ops[i].count
        );
    }
    for (size_t i = 0; i < a.expected.size(); ++i) CHECK(sameStack(a.expected[i], b.expected[i]));
    checkInvariants(input, a, __LINE__);
}

void testRepeatSortIsIdempotent() {
    auto const input = region({dirt(3), cobble(42), egg(4), sword(7), cobble(30), empty(), egg(13), sword(8)});
    auto const first = planSort(input);
    checkInvariants(input, first, __LINE__);
    auto const second = planSort(first.expected);
    CHECK(second.ops.empty());
    for (size_t i = 0; i < first.expected.size(); ++i) CHECK(sameStack(first.expected[i], second.expected[i]));
}

void testOperationsStayInsideRegion() {
    // The planner only ever sees the sortable region; excluded slots (hotbar,
    // armor, offhand...) are simply not part of its input, so no operation
    // can address them. Check every op index is within the region.
    auto const input = region({dirt(3), cobble(42), egg(4), sword(7), cobble(30), egg(13)}, 9);
    auto const plan  = planSort(input);
    for (auto const& op : plan.ops) {
        CHECK(op.from >= 0 && op.from < 9);
        CHECK(op.to >= 0 && op.to < 9);
    }
    checkInvariants(input, plan, __LINE__);
}

void testFullRegionNoEmptySlots() {
    // 27 occupied slots: arranging must still work with swaps only.
    std::vector<SlotStack> stacks;
    for (int i = 0; i < 27; ++i) {
        switch (i % 3) {
        case 0:
            stacks.push_back(sword(100 + i));
            break;
        case 1:
            stacks.push_back(dirt(64));
            break;
        default:
            stacks.push_back(egg(16));
            break;
        }
    }
    auto const input = region(stacks);
    auto const plan  = planSort(input);
    checkInvariants(input, plan, __LINE__);
    CHECK(plan.expected[0].key.typeName == "minecraft:diamond_sword");
    CHECK(plan.expected[9].key.typeName == "minecraft:dirt");
    CHECK(plan.expected[18].key.typeName == "minecraft:egg");
}

void testCreativeIndexOrdersBeforeName() {
    auto a              = dirt(1);
    a.key.creativeIndex = 50;
    auto b              = cobble(1);
    b.key.creativeIndex = 10;
    auto       c        = egg(1); // unknown index: last
    auto const input    = region({a, b, c});
    auto const plan     = planSort(input);
    CHECK(plan.expected[0].key.typeName == "minecraft:cobblestone");
    CHECK(plan.expected[1].key.typeName == "minecraft:dirt");
    CHECK(plan.expected[2].key.typeName == "minecraft:egg");
    checkInvariants(input, plan, __LINE__);
}

SlotStack lockedStack(SlotStack s) {
    s.locked = true;
    return s;
}

void testLockedSlotStaysAndOthersSortAround() {
    // Slot 1 holds a locked oak-log-like stack; everything else must sort
    // into the remaining slots without ever touching slot 1.
    auto const input = region({egg(3), lockedStack(dirt(5)), cobble(42), empty(), cobble(10), sword(7)});
    auto const plan  = planSort(input);
    checkInvariants(input, plan, __LINE__);
    CHECK(plan.expected[1].count == 5 && plan.expected[1].key.typeName == "minecraft:dirt" && plan.expected[1].fixed());
    // Movable slots in order: 0, 2, 3, 4, 5 -> cobble 52, egg 3, sword, empty...
    CHECK(plan.expected[0].key.typeName == "minecraft:cobblestone" && plan.expected[0].count == 52);
    CHECK(plan.expected[2].key.typeName == "minecraft:diamond_sword");
    CHECK(plan.expected[3].key.typeName == "minecraft:egg");
    CHECK(plan.expected[4].empty());
}

void testLockedSlotIsNeverMergedIntoOrOutOf() {
    // A locked partial stack of the same kind as movable partials must keep
    // its exact count: no consolidation may use it as donor or receiver.
    auto const input = region({cobble(42), lockedStack(cobble(10)), cobble(30)});
    auto const plan  = planSort(input);
    checkInvariants(input, plan, __LINE__);
    CHECK(plan.expected[1].count == 10 && plan.expected[1].fixed());
    // The two movable stacks still consolidate: 42 + 30 -> 64 + 8.
    CHECK(plan.expected[0].count == 64 && plan.expected[2].count == 8);
}

void testLockedSlotsIdempotent() {
    auto const input = region({dirt(3), lockedStack(sword(9)), cobble(42), egg(4), lockedStack(cobble(7)), cobble(30)});
    auto const first = planSort(input);
    checkInvariants(input, first, __LINE__);
    auto const second = planSort(first.expected);
    CHECK(second.ops.empty());
    for (size_t i = 0; i < first.expected.size(); ++i) CHECK(sameStack(first.expected[i], second.expected[i]));
}

void testAllSlotsLockedIsNoOp() {
    auto const input = region({lockedStack(dirt(3)), lockedStack(cobble(1))}, 2);
    auto const plan  = planSort(input);
    CHECK(plan.ops.empty());
    checkInvariants(input, plan, __LINE__);
}

void testLockedEmptySlotIsJustEmpty() {
    // A lock is an item property; an empty slot cannot be fixed.
    auto slot        = SlotStack::emptySlot();
    slot.locked      = true;
    auto const input = region({slot, dirt(3)}, 2);
    auto const plan  = planSort(input);
    CHECK(plan.expected[0].key.typeName == "minecraft:dirt");
    checkInvariants(input, plan, __LINE__);
}

void testApplyOperationRejectsFixedSlots() {
    auto slots = region({lockedStack(cobble(42)), cobble(3), empty()}, 3);
    CHECK(!applyOperation(slots, Operation{OpKind::Move, 1, 0, 3}));
    CHECK(!applyOperation(slots, Operation{OpKind::Move, 0, 2, 3}));
    CHECK(!applyOperation(slots, Operation{OpKind::Swap, 0, 1, 0}));
    CHECK(slots[0].count == 42 && slots[1].count == 3 && slots[2].empty());
}

void testApplyOperationRejectsInvalid() {
    auto slots = region({cobble(42), dirt(3), empty()}, 3);
    CHECK(!applyOperation(slots, Operation{OpKind::Move, 0, 1, 5}));  // different groups
    CHECK(!applyOperation(slots, Operation{OpKind::Move, 0, 2, 50})); // more than available
    CHECK(!applyOperation(slots, Operation{OpKind::Move, 2, 0, 1}));  // from empty
    CHECK(!applyOperation(slots, Operation{OpKind::Swap, 0, 2, 0}));  // swap with empty
    CHECK(!applyOperation(slots, Operation{OpKind::Move, 0, 0, 1}));  // same slot
    CHECK(!applyOperation(slots, Operation{OpKind::Move, 0, 3, 1}));  // out of range
    CHECK(slots[0].count == 42 && slots[1].count == 3 && slots[2].empty());
}

} // namespace

int runSortPlannerTests() {
    testEmptyInventory();
    testAlreadySorted();
    testOneItem();
    testMultipleDifferentItems();
    testMerge42Plus10();
    testMerge42Plus30();
    testMergeSeparatedStacksPartialFirst();
    testMoreThanTwoPartialStacks();
    testManyPartialStacksMinimalCount();
    testAlreadyFullStacks();
    testFullPlusPartial();
    testMaxStackSize16();
    testMaxStackSize16NoOvershoot();
    testNonStackableItemsNeverMerge();
    testNonStackableEvenWhenGivenSameGroupByMistake();
    testLookAlikeButIncompatibleStacksStaySeparate();
    testLookAlikesWithEqualKeysStayContiguous();
    testEmptySlotPlacement();
    testDeterministic();
    testRepeatSortIsIdempotent();
    testOperationsStayInsideRegion();
    testFullRegionNoEmptySlots();
    testCreativeIndexOrdersBeforeName();
    testLockedSlotStaysAndOthersSortAround();
    testLockedSlotIsNeverMergedIntoOrOutOf();
    testLockedSlotsIdempotent();
    testAllSlotsLockedIsNoOp();
    testLockedEmptySlotIsJustEmpty();
    testApplyOperationRejectsFixedSlots();
    testApplyOperationRejectsInvalid();

    if (gFailures == 0) {
        std::printf("SortPlannerTest: all checks passed\n");
    } else {
        std::printf("SortPlannerTest: %d check(s) failed\n", gFailures);
    }
    return (gFailures + runSortKeyTests()) == 0 ? 0 : 1;
}
