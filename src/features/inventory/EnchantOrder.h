#pragma once
#include "features/inventory/EnchantPlan.h"
#include <algorithm>
#include <optional>
#include <span>
#include <utility>
#include <vector>

// L-170: what the enchantment order screen edits, without game types. The
// item is input 0 of the planner and wanted[i] is input i + 1.
namespace lamium::inventory::enchantOrder {
using enchantPlan::Enchantment;
inline constexpr int maxBooks = enchantPlan::maxInputs - 1;

// An enchantment that goes on the chosen item.
struct Offer {
    int type = 0, maxLevel = 1, frequency = 0;
};
// Pairs the game calls incompatible, in either order.
struct Conflicts {
    std::vector<std::pair<int, int>> pairs;
    bool between(int a, int b) const {
        return std::ranges::any_of(pairs, [&](auto const& p) { return (p.first == a && p.second == b) || (p.first == b && p.second == a); });
    }
};
inline bool has(std::span<Enchantment const> list, int type) {
    return std::ranges::any_of(list, [&](Enchantment const& e) { return e.type == type; });
}
// The enchantment already on the item or wanted that keeps `type` off, or -1.
inline int blocker(int type, std::span<Enchantment const> base, std::span<Enchantment const> wanted, Conflicts const& conflicts) {
    for (auto list : {base, wanted})
        for (auto const& e : list)
            if (e.type != type && conflicts.between(e.type, type)) return e.type;
    return -1;
}
enum class Change { Added, Removed, Fixed, Blocked, Full };
// Turns an offered enchantment on at its highest level, or off.
inline Change toggle(std::vector<Enchantment>& wanted, Offer const& offer, std::span<Enchantment const> base, Conflicts const& conflicts) {
    if (has(base, offer.type)) return Change::Fixed;
    if (auto found = std::ranges::find_if(wanted, [&](Enchantment const& e) { return e.type == offer.type; }); found != wanted.end()) {
        wanted.erase(found);
        return Change::Removed;
    }
    if (blocker(offer.type, base, wanted, conflicts) >= 0) return Change::Blocked;
    if (static_cast<int>(wanted.size()) >= maxBooks) return Change::Full;
    wanted.push_back({offer.type, std::max(1, offer.maxLevel)});
    return Change::Added;
}
// One level up or down, within 1 and the highest; false when nothing changed.
inline bool step(std::vector<Enchantment>& wanted, Offer const& offer, int delta) {
    for (auto& e : wanted) {
        if (e.type != offer.type) continue;
        int const next = std::clamp(e.level + delta, 1, std::max(1, offer.maxLevel));
        return std::exchange(e.level, next) != next;
    }
    return false;
}
// Every offered enchantment that conflicts with nothing else offered and
// nothing on the item, at its highest level. Groups that exclude each other
// are left for the player to choose from.
inline std::vector<Enchantment> allHighest(std::span<Offer const> offers, std::span<Enchantment const> base, Conflicts const& conflicts) {
    std::vector<Enchantment> out;
    for (auto const& offer : offers) {
        if (has(base, offer.type) || static_cast<int>(out.size()) >= maxBooks) continue;
        bool free = blocker(offer.type, base, {}, conflicts) < 0;
        for (auto const& other : offers) free = free && (other.type == offer.type || !conflicts.between(other.type, offer.type));
        if (free) out.push_back({offer.type, std::max(1, offer.maxLevel)});
    }
    return out;
}
// What is still offered, not on the item and not blocked by it; a kind
// change or another held item can leave stale entries behind.
inline void prune(std::vector<Enchantment>& wanted, std::span<Offer const> offers, std::span<Enchantment const> base, Conflicts const& conflicts) {
    std::erase_if(wanted, [&](Enchantment const& e) {
        auto offer = std::ranges::find_if(offers, [&](Offer const& o) { return o.type == e.type; });
        return offer == offers.end() || e.level < 1 || e.level > offer->maxLevel || has(base, e.type)
            || blocker(e.type, base, {}, conflicts) >= 0;
    });
}
// The planner's inputs: the item with its earlier anvil uses, then a fresh
// book per wanted enchantment. Nothing when a frequency is not one the cost
// rule covers.
inline std::optional<std::vector<enchantPlan::Input>> inputs(std::span<Enchantment const> wanted, std::span<Offer const> offers, int itemWork) {
    std::vector<enchantPlan::Input> out{{0, std::max(0, itemWork), true}};
    for (auto const& e : wanted) {
        auto offer = std::ranges::find_if(offers, [&](Offer const& o) { return o.type == e.type; });
        if (offer == offers.end()) return std::nullopt;
        auto const cost = enchantPlan::levelCost(offer->frequency, true);
        if (!cost) return std::nullopt;
        out.push_back({e.level * *cost, 0, false});
    }
    return out;
}
}
