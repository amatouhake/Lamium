#pragma once
#include "features/recipes/RawView.h"
#include "features/recipes/RecipeNotes.h"
#include <algorithm>
#include <map>
#include <string>
#include <vector>
// L-370: the plan by wanted item. The wanted items stand in an order the player sets (their priority);
// held stock goes to them from the top down, and each gets its own progress and breakdown. Pure.
namespace lamium::recipes {
// One wanted item as the list shows it: the player's own (`source` empty) or one a feature handed over.
struct WantedEntry {
    std::string key;
    Count count = 0;
    std::string source;
    bool operator==(WantedEntry const&) const = default;
};
inline std::string wantedEntryId(WantedEntry const& entry) { return entry.source.empty() ? entry.key : entry.source + "|" + entry.key; }
// The player's wanted items and what features handed over, in the saved order; the rest after, the player's
// own first, each in its own order.
inline std::vector<WantedEntry> prioritized(Wanted const& wanted, std::vector<Handed> const& handed) {
    std::vector<WantedEntry> all;
    for (auto const& [key, amount] : wanted.targets) all.push_back({key, amount, {}});
    for (auto const& given : handed) all.push_back({given.key, given.count, given.source});
    std::vector<WantedEntry> out;
    std::vector<bool> taken(all.size(), false);
    for (auto const& id : wanted.order)
        for (size_t i = 0; i < all.size(); ++i)
            if (!taken[i] && wantedEntryId(all[i]) == id) { out.push_back(all[i]), taken[i] = true; break; }
    for (size_t i = 0; i < all.size(); ++i) if (!taken[i]) out.push_back(all[i]);
    return out;
}
// The order after moving entry `id` by `delta` places (up is negative), as a full list of ids.
inline std::vector<std::string> movedOrder(std::vector<WantedEntry> const& entries, std::string const& id, int delta) {
    std::vector<std::string> ids;
    for (auto const& entry : entries) ids.push_back(wantedEntryId(entry));
    auto at = std::ranges::find(ids, id);
    if (at == ids.end()) return ids;
    long const from = at - ids.begin(), to = std::clamp<long>(from + delta, 0, static_cast<long>(ids.size()) - 1);
    if (from != to) {
        std::string moving = ids[static_cast<size_t>(from)];
        ids.erase(ids.begin() + from);
        ids.insert(ids.begin() + to, std::move(moving));
    }
    return ids;
}
// One wanted item's share of the plan: the finished items held for it, and how much of what it takes is
// gathered (`need` of all its raw materials as if nothing were held, `left` of them with the stock it got),
// with its own steps and materials.
struct TargetPlan {
    Item item = -1;
    Count want = 0, finished = 0, need = 0, left = 0;
    RawView view;
    float fraction() const { return need > 0 ? static_cast<float>(need - std::min(left, need)) / static_cast<float>(need) : 1.f; }
    int percent() const { return static_cast<int>(fraction() * 100 + .0001f); }
};
// `wanted`: items in priority order (one entry each, a feature's and the player's apart). Held stock goes to
// them from the top down: what one uses is gone for those below it.
inline std::vector<TargetPlan> targetPlans(Index const& index, Choice const& choice, std::vector<std::pair<Item, Count>> const& wanted,
                                           std::vector<std::pair<Item, Count>> const& held, std::map<int, Item> const& picks, std::set<Item> const& stops) {
    Item const items = static_cast<Item>(index.items.size());
    std::map<Item, Count> stock;
    for (auto const& [id, count] : held) stock[id] += count;
    std::vector<TargetPlan> out;
    for (auto const& [item, want] : wanted) {
        TargetPlan target;
        target.item = item, target.want = want;
        if (item < 0 || want <= 0) { out.push_back(std::move(target)); continue; }
        target.finished = std::min(want, stock[item]);
        Request one{{{item, want}}, {}, picks, stops};
        for (auto const& [id, count] : stock) if (count > 0) one.held.emplace_back(id, count);
        Planned plan = planned(index, choice, one);
        for (auto const& [id, used] : plan.result.fromHeld) if (id < items) stock[id] -= std::min(stock[id], used);
        for (auto const& cover : plan.covered) stock[cover.member] -= std::min(stock[cover.member], cover.count);
        target.view = rawView(index, choice, std::move(plan));
        for (auto const& line : target.view.gather) target.left += line.left;
        Request bare{{{item, want}}, {}, picks, stops};
        for (auto const& [id, count] : planned(index, choice, bare).result.raw) target.need += count;
        out.push_back(std::move(target));
    }
    return out;
}
// Whether a step can be made now, all its crafts, from `stock`: each part as the item it stands for, a group
// not picked by any of its members together.
inline bool stepReady(Index const& index, Request const& request, StepLine const& step, std::map<Item, Count> const& stock) {
    Item const items = static_cast<Item>(index.items.size());
    auto have = [&](Item id) { auto found = stock.find(id); return found == stock.end() ? Count{0} : found->second; };
    for (auto const& part : index.recipes[static_cast<size_t>(step.recipe)].uses) {
        Item const item = partItem(index, request, part);
        Count got = 0;
        if (item >= items) for (Item member : index.groups[static_cast<size_t>(item - items)].members) got += have(member);
        else got = have(item);
        if (got < step.crafts * part.count) return false;
    }
    return true;
}
}
