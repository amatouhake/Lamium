#pragma once

#include "features/inventory/sort/SortKey.h"

#include <cstddef>
#include <string>
#include <vector>

// Pure inventory planning. This header has no Minecraft or LeviLamina
// dependency so the planner can be exercised with synthetic slot states
// (see tests/SortPlannerTest.cpp).
//
// The planner never decides whether two stacks may be merged: that is the
// game's decision (ItemStackBase::isStackable) and arrives here as an opaque
// `group` id. Equal non-negative group ids mean "vanilla lets these stack".
namespace lamium::inventory::sort {

/// One slot of the sortable region.
struct SlotStack {
    /// Item count; 0 means the slot is empty and the other fields are ignored.
    int count{0};
    /// Vanilla max stack size of the item (1 for unstackable items).
    int maxStackSize{1};
    /// Mergeability class assigned by the caller: two stacks can be merged
    /// only if they share a non-negative group. Empty slots use -1.
    int group{-1};
    /// Ordering key; all stacks in a group must carry an equal key.
    SortKey key;
    /// The slot may not be touched at all (vanilla "lock in slot"): its
    /// contents stay where they are, nothing is merged into or out of it,
    /// and the rest of the region is sorted around it. Ignored when empty.
    bool locked{false};

    [[nodiscard]] bool empty() const { return count <= 0; }
    [[nodiscard]] bool fixed() const { return locked && !empty(); }

    static SlotStack emptySlot() { return SlotStack{}; }
};

enum class OpKind {
    /// Move `count` items from slot `from` into slot `to`. `to` is either
    /// empty or holds a stack of the same group with room for `count`.
    Move,
    /// Exchange the contents of two occupied slots of different groups.
    Swap,
};

struct Operation {
    OpKind kind{OpKind::Move};
    int    from{-1};
    int    to{-1};
    int    count{0}; ///< Items moved (Move only).
};

struct Plan {
    std::vector<Operation> ops;
    /// Slot layout after every operation has been applied.
    std::vector<SlotStack> expected;
};

/// Returns whether the two stacks are the same kind and count (an empty slot
/// only matches an empty slot). Used for verifying simulated state.
[[nodiscard]] bool sameStack(SlotStack const& a, SlotStack const& b);

/// Applies one operation to `slots`. Returns false (leaving `slots` untouched)
/// if the operation is not valid for the current state.
[[nodiscard]] bool applyOperation(std::vector<SlotStack>& slots, Operation const& op);

/// Computes the operations that consolidate compatible partial stacks and
/// arrange the region in sorted order (key ascending, then group, then count
/// descending, empty slots last). Fixed (locked, non-empty) slots are left
/// exactly as they are and never addressed by any operation; the movable
/// slots are sorted around them. The result is deterministic for a given
/// input and the total item count of every group is preserved.
[[nodiscard]] Plan planSort(std::vector<SlotStack> const& slots);

/// Formats an operation for logs, e.g. "move 22 #7 -> #3" or "swap #1 <-> #9".
[[nodiscard]] std::string describe(Operation const& op);

} // namespace lamium::inventory::sort
