#pragma once
#include "features/recipes/RawView.h"
#include "features/recipes/RecipeNotes.h"
#include <algorithm>
#include <map>
#include <string>
#include <vector>
// L-370: the plan by wanted item. The wanted items stand in the order added;
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
// The player's wanted items, then what features handed over, in the order each was added (L-370: held stock
// goes to them in this order; there is no reordering).
inline std::vector<WantedEntry> prioritized(Wanted const& wanted, std::vector<Handed> const& handed) {
    std::vector<WantedEntry> out;
    for (auto const& [key, amount] : wanted.targets) out.push_back({key, amount, {}});
    for (auto const& given : handed) out.push_back({given.key, given.count, given.source});
    return out;
}
// One wanted item's share of the plan: the finished items held for it, what is left to gather of its raw
// materials with the stock it got, and its own steps and materials.
struct TargetPlan {
    Item item = -1;
    Count finished = 0, left = 0;
    // Left with all the held stock to itself: more is left than that when rows above took a share of it.
    Count leftAlone = 0;
    bool sharedAbove() const { return left > leftAlone; }
    RawView view;
};
// `wanted`: items in the order added (one entry each, a feature's and the player's apart). Held stock goes to
// them from the top down: what one uses is gone for those below it.
inline std::vector<TargetPlan> targetPlans(Index const& index, Choice const& choice, std::vector<std::pair<Item, Count>> const& wanted,
                                           std::vector<std::pair<Item, Count>> const& held, std::map<int, Item> const& picks, std::set<Item> const& stops) {
    Item const items = static_cast<Item>(index.items.size());
    std::map<Item, Count> stock;
    for (auto const& [id, count] : held) stock[id] += count;
    std::vector<TargetPlan> out;
    for (auto const& [item, want] : wanted) {
        TargetPlan target;
        target.item = item;
        if (item < 0 || want <= 0) { out.push_back(std::move(target)); continue; }
        target.finished = std::min(want, stock[item]);
        Request one{{{item, want}}, {}, picks, stops};
        for (auto const& [id, count] : stock) if (count > 0) one.held.emplace_back(id, count);
        Planned plan = planned(index, choice, one);
        for (auto const& [id, used] : plan.result.fromHeld) if (id < items) stock[id] -= std::min(stock[id], used);
        for (auto const& cover : plan.covered) stock[cover.member] -= std::min(stock[cover.member], cover.count);
        target.view = rawView(index, choice, std::move(plan));
        for (auto const& line : target.view.gather) target.left += line.left;
        target.leftAlone = target.left;
        if (target.left > 0 && !out.empty()) {
            Request alone{{{item, want}}, held, picks, stops};
            target.leftAlone = 0;
            for (auto const& [id, count] : planned(index, choice, alone).result.raw) target.leftAlone += count;
        }
        out.push_back(std::move(target));
    }
    return out;
}
// What `stock` holds of a part: the item it stands for, a group not picked by any of its members together.
inline Count stockOf(Index const& index, Request const& request, Part const& part, std::map<Item, Count> const& stock) {
    Item const items = static_cast<Item>(index.items.size());
    auto have = [&](Item id) { auto found = stock.find(id); return found == stock.end() ? Count{0} : found->second; };
    Item const item = partItem(index, request, part);
    if (item < items) return have(item);
    Count got = 0;
    for (Item member : index.groups[static_cast<size_t>(item - items)].members) got += have(member);
    return got;
}
// How many of a step's crafts `stock` makes now, at most all of them.
inline Count stepNow(Index const& index, Request const& request, StepLine const& step, std::map<Item, Count> const& stock) {
    auto const& recipe = index.recipes[static_cast<size_t>(step.recipe)];
    Count now = step.crafts;
    for (size_t i = 0; i < recipe.uses.size(); ++i) {
        Count const each = recipe.uses[i].count;
        if (!each) continue;
        Count const got = stockOf(index, request, recipe.uses[i], stock);
        // A brewing ingredient serves a batch of crafts (L-361).
        now = std::min(now, i == 0 && recipe.batch > 1 ? got / each * recipe.batch : got / each);
    }
    return now;
}
// The plan's steps as "to make" shows them (L-370): all their crafts can be made now, some of them, none; and
// what held stock covers whole, which is not made at all. Stored (L-385): what is carried makes none, the stores
// would cover some.
enum class StepState { Ready, Partly, Stored, Missing, Held };
inline StepState stepState(Index const& index, Request const& request, StepLine const& step, std::map<Item, Count> const& stock) {
    Count const now = stepNow(index, request, step, stock);
    return now >= step.crafts ? StepState::Ready : now > 0 ? StepState::Partly : StepState::Missing;
}
// "Can make now" counts what is carried (L-385): a step it makes in full or in part is that; one it makes none of
// but the stores (in `all`, with what is carried) would is Stored; else Missing.
inline StepState stepState(Index const& index, Request const& request, StepLine const& step, std::map<Item, Count> const& carried,
                           std::map<Item, Count> const& all) {
    StepState const now = stepState(index, request, step, carried);
    if (now != StepState::Missing) return now;
    return stepNow(index, request, step, all) > 0 ? StepState::Stored : StepState::Missing;
}
// Which of a step's uses `stock` holds too few of for all its crafts: the slots shown in the game's red.
inline std::vector<bool> stepShort(Index const& index, Request const& request, StepLine const& step, std::map<Item, Count> const& stock) {
    auto const& recipe = index.recipes[static_cast<size_t>(step.recipe)];
    std::vector<bool> out(recipe.uses.size(), false);
    for (size_t i = 0; i < recipe.uses.size(); ++i) out[i] = stockOf(index, request, recipe.uses[i], stock) < partNeed(recipe, i, step.crafts);
    return out;
}
// What held stock covers whole and has a recipe: steps not to make, shown as held at the end of "to make".
inline std::vector<StepLine> heldSteps(Index const& index, Choice const& choice, RawView const& view) {
    std::vector<StepLine> out;
    Item const items = static_cast<Item>(index.items.size());
    for (auto const& line : view.done) {
        if (line.id < 0 || line.id >= items || static_cast<size_t>(line.id) >= choice.recipe.size()) continue;
        int const recipe = choice.recipe[static_cast<size_t>(line.id)];
        if (recipe < 0) continue;
        Count const makes = std::max<Count>(1, index.recipes[static_cast<size_t>(recipe)].count);
        Count const crafts = (line.held + makes - 1) / makes;
        out.push_back({line.id, recipe, crafts, line.held, 0, line.held});
    }
    return out;
}
}
