#include "features/inventory/sort/SortPlanner.h"

#include <algorithm>
#include <map>
#include <numeric>

namespace lamium::inventory::sort {

namespace {

bool mergeable(SlotStack const& a, SlotStack const& b) {
    return !a.empty() && !b.empty() && a.group >= 0 && a.group == b.group && a.maxStackSize > 1 && b.maxStackSize > 1;
}

// Phase 1: fill the lowest-index stacks of every group from the highest-index
// ones, so each group ends with at most one partial stack and the earlier
// slots are full. This mirrors what a player does by hand: top up existing
// stacks before leaving new partial ones around.
void planConsolidation(std::vector<SlotStack>& state, std::vector<Operation>& ops) {
    std::map<int, std::vector<int>> groups;
    for (int i = 0; i < static_cast<int>(state.size()); ++i) {
        auto const& s = state[static_cast<size_t>(i)];
        if (s.empty() || s.group < 0 || s.maxStackSize <= 1) continue;
        groups[s.group].push_back(i);
    }
    for (auto& [group, indices] : groups) {
        size_t lo = 0;
        size_t hi = indices.size() - 1;
        while (lo < hi) {
            auto&     receiver = state[static_cast<size_t>(indices[lo])];
            auto&     donor    = state[static_cast<size_t>(indices[hi])];
            int const space    = receiver.maxStackSize - receiver.count;
            if (space <= 0) {
                ++lo;
                continue;
            }
            if (donor.empty()) {
                --hi;
                continue;
            }
            Operation op{OpKind::Move, indices[hi], indices[lo], std::min(space, donor.count)};
            // Cannot fail: both slots were just checked.
            (void)applyOperation(state, op);
            ops.push_back(op);
        }
    }
}

// Phase 2: bring the region into sorted order. Stacks are identified by
// (group, count): two full stacks of one group are interchangeable, so no
// operation is spent on swapping them.
//
// Order: key, then group (so the stacks of one kind stay together even when
// two vanilla-distinct kinds happen to share a key; group ids follow first
// appearance, which keeps a sorted region stable), then count descending.
void planArrangement(std::vector<SlotStack>& state, std::vector<Operation>& ops) {
    std::vector<int> order;
    for (int i = 0; i < static_cast<int>(state.size()); ++i) {
        if (!state[static_cast<size_t>(i)].empty()) order.push_back(i);
    }
    std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
        auto const& sa = state[static_cast<size_t>(a)];
        auto const& sb = state[static_cast<size_t>(b)];
        if (sa.key < sb.key) return true;
        if (sb.key < sa.key) return false;
        if (sa.group != sb.group) return sa.group < sb.group;
        return sa.count > sb.count;
    });
    std::vector<SlotStack> desired;
    desired.reserve(order.size());
    for (int i : order) desired.push_back(state[static_cast<size_t>(i)]);

    for (size_t t = 0; t < desired.size(); ++t) {
        auto const& want = desired[t];
        if (sameStack(state[t], want)) continue;

        // Positions before t are final, so the wanted stack sits after t.
        size_t j = t + 1;
        while (j < state.size() && !sameStack(state[j], want)) ++j;
        if (j >= state.size()) return; // Unreachable: the multiset is preserved.

        auto const& cur = state[t];
        Operation   op;
        if (cur.empty()) {
            op = Operation{OpKind::Move, static_cast<int>(j), static_cast<int>(t), want.count};
        } else if (mergeable(cur, want)) {
            // Same group: `cur` is the group's partial stack (fulls sort
            // first and are already placed), so top it up from j. Slot j is
            // left holding the partial amount, keeping the multiset intact.
            op = Operation{OpKind::Move, static_cast<int>(j), static_cast<int>(t), want.count - cur.count};
        } else {
            op = Operation{OpKind::Swap, static_cast<int>(t), static_cast<int>(j), 0};
        }
        if (!applyOperation(state, op)) return; // Unreachable for a consistent state.
        ops.push_back(op);
    }
}

} // namespace

bool sameStack(SlotStack const& a, SlotStack const& b) {
    if (a.empty() || b.empty()) return a.empty() && b.empty();
    return a.group == b.group && a.count == b.count && a.key == b.key;
}

bool applyOperation(std::vector<SlotStack>& slots, Operation const& op) {
    auto const n = static_cast<int>(slots.size());
    if (op.from < 0 || op.from >= n || op.to < 0 || op.to >= n || op.from == op.to) return false;
    auto& src = slots[static_cast<size_t>(op.from)];
    auto& dst = slots[static_cast<size_t>(op.to)];
    if (src.fixed() || dst.fixed()) return false;

    switch (op.kind) {
    case OpKind::Move: {
        if (src.empty() || op.count <= 0 || op.count > src.count) return false;
        if (dst.empty()) {
            dst       = src;
            dst.count = op.count;
        } else {
            if (!mergeable(src, dst) || dst.count + op.count > dst.maxStackSize) return false;
            dst.count += op.count;
        }
        src.count -= op.count;
        if (src.empty()) src = SlotStack::emptySlot();
        return true;
    }
    case OpKind::Swap: {
        if (src.empty() || dst.empty()) return false;
        std::swap(src, dst);
        return true;
    }
    }
    return false;
}

Plan planSort(std::vector<SlotStack> const& slots) {
    Plan plan;
    plan.expected = slots;
    for (auto& s : plan.expected) {
        if (s.empty()) s = SlotStack::emptySlot();
    }

    // Fixed slots are cut out: the planner works on the movable slots as if
    // they were a contiguous region and the operations are mapped back, so
    // no step can ever address a fixed slot.
    std::vector<int> movable;
    for (int i = 0; i < static_cast<int>(plan.expected.size()); ++i) {
        if (!plan.expected[static_cast<size_t>(i)].fixed()) movable.push_back(i);
    }
    std::vector<SlotStack> view;
    view.reserve(movable.size());
    for (int i : movable) view.push_back(plan.expected[static_cast<size_t>(i)]);

    std::vector<Operation> ops;
    planConsolidation(view, ops);
    planArrangement(view, ops);

    for (size_t k = 0; k < movable.size(); ++k) {
        plan.expected[static_cast<size_t>(movable[k])] = view[k];
    }
    plan.ops.reserve(ops.size());
    for (auto op : ops) {
        op.from = movable[static_cast<size_t>(op.from)];
        op.to   = movable[static_cast<size_t>(op.to)];
        plan.ops.push_back(op);
    }
    return plan;
}

std::string describe(Operation const& op) {
    if (op.kind == OpKind::Swap) {
        return "swap #" + std::to_string(op.from) + " <-> #" + std::to_string(op.to);
    }
    return "move " + std::to_string(op.count) + " #" + std::to_string(op.from) + " -> #" + std::to_string(op.to);
}

} // namespace lamium::inventory::sort
