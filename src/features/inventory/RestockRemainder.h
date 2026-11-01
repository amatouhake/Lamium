#pragma once
#include <cstdint>
#include <optional>
#include "features/inventory/RestockPlan.h"

namespace lamium::inventory {
// L-291: a replacement refill swaps the consumption remainder into the slot
// the refill came from, where it sits alone even when another stack of the
// same item could take it. A second move merges it. That move cannot ride
// along with the swap: movePair refuses while the swap's transaction is
// outstanding, so the merge waits for a later client tick and gives up when
// the moment has passed instead of holding the remainder hostage.
// Client level ticks (20 per second, whatever the frame rate): two seconds.
inline constexpr int remainderMergeTicks = 40;
// A refused send (a transaction or request still in flight) may be retried on
// later ticks; a server round trip fits inside this, and the tick deadline
// still bounds the wait.
inline constexpr int remainderMergeAttempts = 12;
struct RemainderMerge {
    int source;
    int destination;
    int count;            // Items moved: the whole remainder stack.
    int destinationCount; // The destination's count after the merge.
};
// What a replacement refill left behind, and enough to recognise it again on
// a later tick: the swap's world, hand and remainder, plus the tick it was
// sent on. Kind ids belong to the restock's own kind table, which the caller
// keeps alive for as long as this is waiting.
struct RemainderPending {
    std::uint64_t context = 0;
    int selected = -1;
    int source = -1;
    int kind = -1;
    int count = 0;
    int frame = 0;
    int attempts = 0;
    bool waiting() const { return source >= 0 && kind >= 0 && count > 0; }
    // Never the tick the swap went out on: its transaction is still in flight.
    bool due(int now) const { return waiting() && now > frame; }
    bool expired(int now) const { return waiting() && now - frame > remainderMergeTicks; }
    void clear() { *this = RemainderPending{}; }
};
// The slot a later move would merge into, or nothing to do. Everything the
// merge depends on is read back from one snapshot: a remainder that moved,
// a hand that changed, a locked slot or a stack without room ends the wait.
inline std::optional<RemainderMerge> planRemainderMerge(
    RestockSnapshot const& current, RemainderPending const& pending, int maxStack
) {
    if (!validRestockSnapshot(current) || !pending.waiting() || maxStack < 1) return {};
    if (current.context != pending.context || current.selected != pending.selected) return {};
    if (pending.source >= 36 || pending.source == current.selected) return {};
    auto const& remainder = current.slots[pending.source];
    if (remainder.locked || remainder.kind != pending.kind || remainder.count != pending.count) return {};
    if (pending.count > maxStack) return {};
    // Main inventory first, then the hotbar, so a hotbar stack is only a
    // destination when nothing else can take the remainder. The fullest stack
    // wins its region; ties take the lower slot. The offhand is never used: it
    // holds what the player put there to use, not storage.
    int destination = -1;
    auto pick = [&](int first, int last) {
        for (int slot = first; slot < last; ++slot) {
            if (slot == pending.source || slot == current.selected) continue;
            auto const& candidate = current.slots[slot];
            if (candidate.empty() || candidate.locked || candidate.kind != pending.kind) continue;
            if (candidate.count + pending.count > maxStack) continue;
            if (destination < 0 || candidate.count > current.slots[destination].count) destination = slot;
        }
    };
    pick(9,36);
    if (destination < 0) pick(0,9);
    if (destination < 0) return {};
    return RemainderMerge{pending.source,destination,pending.count,
        current.slots[destination].count + pending.count};
}
}
