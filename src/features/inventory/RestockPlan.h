#pragma once
#include <array>
#include <cstdint>
#include <optional>

namespace lamium::inventory {
// Kind IDs are local to a pair of snapshots. The native adapter must group
// stacks by vanilla item equivalence (including components), not just type ID.
struct RestockSlot {
    int kind = -1;
    int count = 0;
    bool locked = false;
    bool empty() const { return count == 0; }
    bool valid() const { return count >= 0 && (empty() || kind >= 0); }
    bool operator==(RestockSlot const&) const = default;
};
struct RestockSnapshot {
    // A generation owned by the adapter, changed on player/world/dimension or
    // input-ownership transitions. Never use a raw actor pointer as identity.
    std::uint64_t context = 0;
    int selected = -1;
    std::array<RestockSlot,36> slots;
};
struct RestockPlan {
    int source;
    int destination;
    RestockSlot expectedSource;
    std::uint64_t context;

    bool stillValid(RestockSnapshot const& current) const {
        return current.context == context && current.selected == destination
            && source >= 9 && source < 36 && destination >= 0 && destination < 9
            && current.slots[source] == expectedSource
            && current.slots[destination].empty() && !current.slots[destination].locked;
    }
};

// Hotbar fallback (L-17): HUD-controller transfers are unsupported, so when
// the consumed stack has a compatible reserve in another hotbar slot, move the
// selection there through the proven selectSlot API instead of transferring.
// Main-inventory reserves stay an unresolved open issue.
struct HotbarSelectPlan {
    int source;
    int depleted;
    RestockSlot expectedSource;
    std::uint64_t context;

    bool stillValid(RestockSnapshot const& current) const {
        return current.context == context && current.selected == depleted
            && source >= 0 && source < 9 && source != depleted
            && current.slots[source] == expectedSource && !current.slots[source].empty()
            && current.slots[depleted].empty();
    }
};

inline bool depletedByUse(
    RestockSnapshot const& before, RestockSnapshot const& after, bool useSucceeded
) {
    if (!useSucceeded || !before.context || before.context != after.context
        || before.selected < 0 || before.selected >= 9 || before.selected != after.selected) return false;
    for (auto const* snapshot : {&before,&after})
        for (auto const& slot : snapshot->slots) if (!slot.valid()) return false;
    auto const& used = before.slots[before.selected];
    auto const& depleted = after.slots[after.selected];
    // A replacement such as an empty bucket/bowl is not an empty hand.
    if (used.count != 1 || used.locked || !depleted.empty() || depleted.locked) return false;
    // An unrelated mutation breaks correlation with the observed use. Do not
    // silently select another reserve after a manual move or correction.
    for (int slot = 0; slot < 36; ++slot)
        if (slot != before.selected && before.slots[slot] != after.slots[slot]) return false;
    return true;
}

inline std::optional<HotbarSelectPlan> planHotbarSelect(
    RestockSnapshot const& before, RestockSnapshot const& after, bool useSucceeded
) {
    if (!depletedByUse(before,after,useSucceeded)) return {};
    auto const& used = before.slots[before.selected];
    // Preserve equipment and main inventory. Select the first unchanged,
    // compatible hotbar stack; never rearrange unrelated items.
    for (int source = 0; source < 9; ++source) {
        if (source == after.selected) continue;
        auto const& candidate = after.slots[source];
        if (!candidate.empty() && !candidate.locked && candidate.kind == used.kind
            && candidate == before.slots[source])
            return HotbarSelectPlan{source,after.selected,candidate,after.context};
    }
    return {};
}

// Call only around an observed vanilla use/consumption operation. Ordinary
// inventory ticks, dropping, manual moves and server corrections are not use
// evidence. Empty-slot polling alone must never invoke replenishment.
inline std::optional<RestockPlan> planRestock(
    RestockSnapshot const& before, RestockSnapshot const& after, bool useSucceeded
) {
    if (!depletedByUse(before,after,useSucceeded)) return {};
    auto const& used = before.slots[before.selected];
    // Preserve the other hotbar slots and equipment. Select the first unchanged,
    // compatible main-inventory stack; never rearrange unrelated items.
    for (int source = 9; source < 36; ++source) {
        auto const& candidate = after.slots[source];
        if (!candidate.empty() && !candidate.locked && candidate.kind == used.kind
            && candidate == before.slots[source])
            return RestockPlan{source,after.selected,candidate,after.context};
    }
    return {};
}
}
