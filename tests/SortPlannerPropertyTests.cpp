#include "features/inventory/sort/SortPlanner.h"
#include <algorithm>
#include <array>
#include <iostream>
#include <map>
#include <random>
#include <stdexcept>

namespace {
using namespace lamium::inventory::sort;
using Slots = std::vector<SlotStack>;
constexpr unsigned seed = 0x1a61u;

bool equal(SlotStack const& a, SlotStack const& b) {
    if (a.empty() || b.empty()) return a.empty() && b.empty();
    return a.group == b.group && a.count == b.count && a.key == b.key
        && a.locked == b.locked && a.maxStackSize == b.maxStackSize;
}
std::map<int, int> totals(Slots const& slots) {
    std::map<int, int> result;
    for (auto const& slot : slots) if (!slot.empty()) result[slot.group] += slot.count;
    return result;
}
void reclassify(Slots& slots) {
    std::map<int, int> renumber;
    for (auto& slot : slots) if (!slot.empty()) {
        auto [it, inserted] = renumber.try_emplace(slot.group, static_cast<int>(renumber.size()));
        slot.group = it->second;
    }
}
void verify(Slots const& input, int caseIndex) {
    auto require = [caseIndex](bool ok, char const* message) {
        if (!ok) throw std::runtime_error("sort property seed=" + std::to_string(seed)
            + " case=" + std::to_string(caseIndex) + ": " + message);
    };
    auto const plan = planSort(input);
    auto state = input;
    auto const originalTotals = totals(input);
    std::map<int, size_t> groupOrder;
    for (auto const& slot : input)
        if (!slot.empty() && !slot.fixed()) groupOrder.try_emplace(slot.group, groupOrder.size());
    // Independent interpreter: do not use the planner's applyOperation helper.
    // Validate every prefix, since vanilla applies a plan one transfer at a time.
    for (auto const& op : plan.ops) {
        require(op.from >= 0 && op.to >= 0 && op.from != op.to
            && op.from < static_cast<int>(state.size()) && op.to < static_cast<int>(state.size()), "slot bounds");
        auto& from = state[op.from];
        auto& to = state[op.to];
        require(!from.fixed() && !to.fixed() && !from.empty(), "touches movable occupied source");
        if (op.kind == OpKind::Swap) {
            require(!to.empty() && op.count == 0, "valid swap");
            std::swap(from, to);
        } else {
            require(op.kind == OpKind::Move && op.count > 0 && op.count <= from.count, "valid move count");
            if (to.empty()) {
                to = from;
                to.count = 0;
            } else {
                require(from.group == to.group && from.maxStackSize > 1 && to.maxStackSize > 1, "merge compatibility");
            }
            require(to.count + op.count <= to.maxStackSize, "destination capacity");
            to.count += op.count;
            from.count -= op.count;
            if (!from.count) from = SlotStack::emptySlot();
        }
        require(totals(state) == originalTotals, "per-operation conservation");
        for (size_t i = 0; i < input.size(); ++i)
            if (input[i].fixed()) require(equal(state[i], input[i]), "fixed slot unchanged");
    }
    require(state.size() == plan.expected.size(), "region size unchanged");
    for (size_t i = 0; i < state.size(); ++i) require(equal(state[i], plan.expected[i]), "independent replay agrees");

    // Oracle: count movable items by kind and compute the minimum number of
    // stacks arithmetically, independently of the planner's donor/receiver walk.
    std::map<int, int> movableTotals, stackCounts, capacities;
    bool seenEmpty = false;
    SlotStack const* previous = nullptr;
    for (auto const& slot : state) {
        if (slot.fixed()) continue;
        if (slot.empty()) { seenEmpty = true; continue; }
        require(!seenEmpty, "empty slots last");
        if (previous) {
            require(!(slot.key < previous->key), "key ordering");
            if (slot.key == previous->key) {
                require(groupOrder.at(slot.group) >= groupOrder.at(previous->group), "equal-key movable group order");
                if (slot.group == previous->group) require(slot.count <= previous->count, "full stacks first");
            }
        }
        previous = &slot;
        movableTotals[slot.group] += slot.count;
        ++stackCounts[slot.group];
        capacities[slot.group] = slot.maxStackSize;
    }
    for (auto [group, total] : movableTotals)
        require(stackCounts[group] == (total + capacities[group] - 1) / capacities[group], "minimal movable stack count");

    // Runtime classification numbers groups in first-appearance order on each
    // request. Repeating a sort must remain a no-op after that renumbering.
    reclassify(state);
    require(planSort(state).ops.empty(), "repeat sort after reclassification is a no-op");
}
}

void sortPlannerPropertyTests() {
    SlotStack a;
    a.count = 1; a.maxStackSize = 64; a.group = 0; a.key.typeName = "test:z";
    auto b = a; b.group = 1;
    auto fixedB = b; fixedB.locked = true;
    auto c = a; c.group = 2; c.key.typeName = "test:a";
    // Moving C before A makes the fixed B classify before A on the next sort.
    verify({a, fixedB, c, b}, -1);
    std::mt19937 random(seed);
    constexpr std::array sizes{0, 1, 9, 27, 54};
    constexpr std::array capacities{1, 16, 64};
    for (int sample = 0; sample < 3000; ++sample) {
        Slots input(sizes[sample % sizes.size()]);
        for (auto& slot : input) {
            if (random() % 5 == 0) continue;
            slot.group = static_cast<int>(random() % 12);
            slot.maxStackSize = capacities[(slot.group / 2) % capacities.size()];
            slot.count = 1 + static_cast<int>(random() % slot.maxStackSize);
            // Adjacent groups intentionally share keys, but cannot merge.
            slot.key.typeName = "test:item_" + std::to_string(slot.group / 2);
            if (slot.maxStackSize == 1) slot.group = 12 + static_cast<int>(&slot - input.data());
            slot.locked = random() % 7 == 0;
        }
        reclassify(input);
        verify(input, sample);
    }
    std::cout << "SortPlanner properties: 3000 reproducible layouts passed\n";
}
