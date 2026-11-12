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
    bool curse = false;
    int reach = 0; // how many kinds of item it goes on; fewer is more particular
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
// The game's order with each enchantment moved next to the ones it excludes
// (the four protections together), curses last.
inline std::vector<Offer> grouped(std::span<Offer const> offers, Conflicts const& conflicts) {
    std::vector<Offer> out;
    for (bool curses : {false, true})
        for (auto const& offer : offers) {
            if (offer.curse != curses) continue;
            auto at = out.end();
            for (auto it = out.begin(); it != out.end(); ++it)
                if (it->curse == curses && conflicts.between(it->type, offer.type)) at = it + 1;
            out.insert(at, offer);
        }
    return out;
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
// One level up or down. Going below 1 turns the enchantment off, so the
// level buttons are enough to drop one; above the highest nothing happens.
// False when nothing changed.
inline bool step(std::vector<Enchantment>& wanted, Offer const& offer, int delta) {
    for (auto it = wanted.begin(); it != wanted.end(); ++it) {
        if (it->type != offer.type) continue;
        if (it->level + delta < 1) { wanted.erase(it); return true; }
        int const next = std::min(it->level + delta, std::max(1, offer.maxLevel));
        return std::exchange(it->level, next) != next;
    }
    return false;
}
// Every offered enchantment at its highest level, without curses. Of a group
// that excludes each other one is taken: the most common in the game's own
// weights, then the most particular to the item, then the first.
inline std::vector<Enchantment> allHighest(std::span<Offer const> offers, std::span<Enchantment const> base, Conflicts const& conflicts) {
    std::vector<Offer const*> order;
    for (auto const& offer : offers)
        if (!offer.curse && !has(base, offer.type) && blocker(offer.type, base, {}, conflicts) < 0) order.push_back(&offer);
    std::ranges::stable_sort(order, [](Offer const* a, Offer const* b) {
        return a->frequency != b->frequency ? a->frequency > b->frequency : a->reach < b->reach;
    });
    std::vector<int> taken;
    for (auto const* offer : order) {
        if (static_cast<int>(taken.size()) >= maxBooks) break;
        if (std::ranges::none_of(taken, [&](int type) { return conflicts.between(type, offer->type); })) taken.push_back(offer->type);
    }
    std::vector<Enchantment> out;
    for (auto const& offer : offers)
        if (std::ranges::find(taken, offer.type) != taken.end()) out.push_back({offer.type, std::max(1, offer.maxLevel)});
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
// Leaving the held item's state: what it carries becomes wanted, ahead of
// the rest, so the plan turns into "this item from fresh" and nothing chosen
// is lost. Levels above the offered highest are lowered to it.
inline void release(std::vector<Enchantment>& wanted, std::span<Enchantment const> base, std::span<Offer const> offers) {
    std::vector<Enchantment> out;
    for (auto const& e : base) {
        auto offer = std::ranges::find_if(offers, [&](Offer const& o) { return o.type == e.type; });
        if (offer != offers.end() && e.level >= 1) out.push_back({e.type, std::min(e.level, std::max(1, offer->maxLevel))});
    }
    for (auto const& e : wanted)
        if (!has(out, e.type)) out.push_back(e);
    if (static_cast<int>(out.size()) > maxBooks) out.resize(static_cast<size_t>(maxBooks));
    wanted = std::move(out);
}
// The planner's inputs: the item with its earlier anvil uses when there is
// one (a plan of books alone has none), then a fresh book per wanted
// enchantment. Nothing when a frequency is not one the cost rule covers.
// A held enchanted book is input 0 as well, but with a value: unlike an item
// it may be the sacrificed side.
inline std::optional<std::vector<enchantPlan::Input>> inputs(std::span<Enchantment const> wanted, std::span<Offer const> offers, int itemWork,
    bool withItem = true, std::optional<int> bookValue = std::nullopt) {
    std::vector<enchantPlan::Input> out;
    if (withItem) out.push_back({bookValue.value_or(0), std::max(0, itemWork), !bookValue});
    for (auto const& e : wanted) {
        auto offer = std::ranges::find_if(offers, [&](Offer const& o) { return o.type == e.type; });
        if (offer == offers.end()) return std::nullopt;
        auto const cost = enchantPlan::levelCost(offer->frequency, true);
        if (!cost) return std::nullopt;
        out.push_back({e.level * *cost, 0, false});
    }
    return out;
}
// A finished item at hand (a stack holding every input) says the plan is
// done. But with another item of the plan at hand that is not finished, the
// player is making a second one, so the finished ones are left out.
inline std::vector<unsigned> progress(std::span<unsigned const> stacks, enchantPlan::Plan const& plan) {
    std::vector<unsigned> out(stacks.begin(), stacks.end());
    if (plan.steps.empty()) return out;
    unsigned const full = plan.steps.back().target | plan.steps.back().sacrifice;
    if ((full & 1u) && std::ranges::any_of(out, [&](unsigned mask) { return (mask & 1u) && mask != full; }))
        std::erase(out, full);
    return out;
}
// What a book costs as the sacrificed side: its levels at the book rate.
// Nothing when an enchantment on it is not offered or has an unknown rate.
inline std::optional<int> bookValue(std::span<Enchantment const> on, std::span<Offer const> offers) {
    int value = 0;
    for (auto const& e : on) {
        auto offer = std::ranges::find_if(offers, [&](Offer const& o) { return o.type == e.type; });
        if (offer == offers.end()) return std::nullopt;
        auto const cost = enchantPlan::levelCost(offer->frequency, true);
        if (!cost) return std::nullopt;
        value += e.level * *cost;
    }
    return value;
}
// Following a plan with what the player carries: a step is done when one
// stack holds everything the step joins. `stacks` are the planner-input
// masks of the stacks at hand (enchantPlan::carried).
inline bool done(enchantPlan::Step const& step, std::span<unsigned const> stacks) {
    unsigned const joined = step.target | step.sacrifice;
    return std::ranges::any_of(stacks, [&](unsigned mask) { return (mask & joined) == joined; });
}
// The first step not done yet, or -1 when the plan is finished.
inline int nextStep(enchantPlan::Plan const& plan, std::span<unsigned const> stacks) {
    for (size_t i = 0; i < plan.steps.size(); ++i)
        if (!done(plan.steps[i], stacks)) return static_cast<int>(i);
    return -1;
}
inline bool atHand(unsigned side, std::span<unsigned const> stacks) {
    return std::ranges::find(stacks, side) != stacks.end();
}
}
