#pragma once
#include "features/recipes/PlacePlan.h"
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
// The shares worked out an entry at a time (L-390 1-12): a long list is spread over frames, each step resuming
// with the stock the entries above left. `wanted`: items in the order added (one entry each, a feature's and the
// player's apart). Held stock goes to them from the top down: what one uses is gone for those below it.
struct TargetPlanning {
    std::vector<std::pair<Item, Count>> wanted, held;
    std::map<int, Item> picks;
    std::set<Item> stops;
    std::map<Item, Count> stock;
    std::vector<TargetPlan> out;
    TargetPlanning(std::vector<std::pair<Item, Count>> wanted_, std::vector<std::pair<Item, Count>> held_, std::map<int, Item> picks_,
                   std::set<Item> stops_)
        : wanted(std::move(wanted_)), held(std::move(held_)), picks(std::move(picks_)), stops(std::move(stops_)) {
        for (auto const& [id, count] : held) stock[id] += count;
    }
    bool done() const { return out.size() >= wanted.size(); }
    // Plans the next entry; nothing once done.
    void step(Index const& index, Choice const& choice) {
        if (done()) return;
        Item const items = static_cast<Item>(index.items.size());
        auto const [item, want] = wanted[out.size()];
        TargetPlan target;
        target.item = item;
        if (item < 0 || want <= 0) { out.push_back(std::move(target)); return; }
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
};
inline std::vector<TargetPlan> targetPlans(Index const& index, Choice const& choice, std::vector<std::pair<Item, Count>> const& wanted,
                                           std::vector<std::pair<Item, Count>> const& held, std::map<int, Item> const& picks, std::set<Item> const& stops) {
    TargetPlanning planning{wanted, held, picks, stops};
    while (!planning.done()) planning.step(index, choice);
    return std::move(planning.out);
}
// What `stock` holds of a part: the item it stands for, a group not picked by any of its members together.
inline Count stockOf(Index const& index, Request const& request, Part const& part, std::map<Item, Count> const& stock) {
    return heldOf(index, stock, partItem(index, request, part));
}
// How many of a step's crafts `stock` makes now, at most all of them.
inline Count stepNow(Index const& index, Request const& request, StepLine const& step, std::map<Item, Count> const& stock) {
    Count now = step.crafts;
    for (auto const& part : stepParts(index, request, step)) {
        Count const each = part.part.count;
        if (!each) continue;
        Count const got = heldOf(index, stock, part.item);
        // A brewing ingredient serves a batch of crafts (L-361).
        now = std::min(now, part.batch > 1 ? got / each * part.batch : got / each);
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
    std::vector<bool> out;
    for (auto const& part : stepParts(index, request, step)) out.push_back(heldOf(index, stock, part.item) < part.need);
    return out;
}
// A traced group's step (L-386): its members' recipes with what `carried` holds of each one's input, most first; a
// member of the kind picked in How to make (oak logs) keeps them to the recipes that take it. Empty for another step.
inline std::vector<std::pair<Count, int>> memberRecipes(Index const& index, Request const& request, StepLine const& step,
                                                        std::map<Item, Count> const& carried) {
    size_t const group = static_cast<size_t>(step.item - static_cast<Item>(index.items.size()));
    if (!groupStep(index, step) || group >= index.traces.size() || index.traces[group].kind < 0) return {};
    auto const& trace = index.traces[group];
    Item const kind = partItem(index, request, Part{true, trace.kind, trace.uses});
    bool const picked = kind < static_cast<Item>(index.items.size());
    std::vector<std::pair<Count, int>> byHeld; // what is carried of a recipe's input, the recipe
    for (int number : trace.recipes) {
        Item const input = partItem(index, request, index.recipes[static_cast<size_t>(number)].uses[0]);
        if (!picked || input == kind) byHeld.emplace_back(heldOf(index, carried, input), number);
    }
    std::stable_sort(byHeld.begin(), byHeld.end(), [](auto const& a, auto const& b) { return a.first > b.first; });
    return byHeld;
}
// What "make" hands the screen for a step (L-385): its recipe for all its crafts; a traced group's step every member's
// recipe, the one whose input is most carried first. The screen picks at the step's turn (MakeRun).
inline MakeRun makeRun(Index const& index, Request const& request, StepLine const& step, std::map<Item, Count> const& carried) {
    MakeRun out{{}, static_cast<int>(std::min<Count>(step.crafts, 1 << 20))};
    for (auto const& [have, number] : memberRecipes(index, request, step, carried)) out.recipes.push_back(number);
    if (out.recipes.empty()) out.recipes.push_back(step.recipe);
    return out;
}
// A step split as `carried` holds each member's input now: a traced group's step over its members' recipes, the one
// whose input is most carried first, each as far as its input goes; another step its recipe for all its crafts.
inline std::vector<std::pair<int, int>> stepRuns(Index const& index, Request const& request, StepLine const& step,
                                                 std::map<Item, Count> const& carried) {
    int const all = static_cast<int>(std::min<Count>(step.crafts, 1 << 20));
    auto const byHeld = memberRecipes(index, request, step, carried);
    if (byHeld.empty()) return {{step.recipe, all}};
    Count const uses = index.traces[static_cast<size_t>(step.item - static_cast<Item>(index.items.size()))].uses;
    std::vector<std::pair<int, int>> out;
    int left = all;
    for (auto const& [have, number] : byHeld) {
        if (left <= 0 || have < uses) break;
        int const rounds = static_cast<int>(std::min<Count>(have / uses, static_cast<Count>(left)));
        out.emplace_back(number, rounds);
        left -= rounds;
    }
    if (out.empty()) out.emplace_back(step.recipe, all);
    return out;
}
// The recipe "lay in" uses for a step, and that the screen is asked about: the first of stepRuns, so a traced
// group's step lays in the member whose input is carried, not one only the stores hold (L-386).
inline int layRecipe(Index const& index, Request const& request, StepLine const& step, std::map<Item, Count> const& carried) {
    return stepRuns(index, request, step, carried).front().first;
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
