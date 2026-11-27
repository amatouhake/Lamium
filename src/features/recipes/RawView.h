#pragma once
#include <algorithm>
#include "features/recipes/RecipeIndex.h"

// L-359: a plan as the lists a screen shows: what to gather, most first, and
// what to make, in the order it can be made. Pure.
namespace lamium::recipes {
struct GatherLine {
    Item id = 0; // past Index::items: a group
    Count need = 0, held = 0, left = 0;
    bool operator==(GatherLine const&) const = default;
};
struct StepLine {
    Item item = 0;
    int recipe = 0; // position in Index::recipes
    Count crafts = 0, made = 0, spare = 0;
    Count held = 0; // of the product itself, used besides what is made
    bool operator==(StepLine const&) const = default;
};
struct RawView {
    std::vector<GatherLine> gather;
    std::vector<StepLine> steps;
    // What held stock covers whole: neither left to gather nor a step, so done (L-367: the plan window lists
    // it faint under what is still to gather). Kept apart, so `gather` stays what is left.
    std::vector<GatherLine> done;
};
// The whole of a list of what to gather: how many in all and how many are left (L-367).
struct GatherTotal {
    Count need = 0, left = 0;
    float fraction() const { return need > 0 ? static_cast<float>(need - std::min(left, need)) / static_cast<float>(need) : 1.f; }
};
inline GatherTotal gatherTotal(std::vector<GatherLine> const& lines) {
    GatherTotal out;
    for (auto const& line : lines) out.need += line.need, out.left += line.left;
    return out;
}
// The recipes an item can be made by in a plan, in the table's order: those that use their own product left
// out. The plan's rows step through them (L-367).
inline std::vector<int> recipeOptions(Index const& index, Item item) {
    std::vector<int> out;
    if (item < 0 || static_cast<size_t>(item) >= index.makers.size()) return out;
    for (int number : index.makers[static_cast<size_t>(item)]) if (!index.recipes[static_cast<size_t>(number)].own) out.push_back(number);
    return out;
}
inline RawView rawView(Index const& index, Choice const& choice, Planned plan) {
    RawView out;
    auto heldOf = [&](Item item) {
        for (auto [id, count] : plan.result.fromHeld) if (id == item) return count;
        return Count{0};
    };
    for (auto [item, left] : plan.result.raw) out.gather.push_back({item, left + heldOf(item), heldOf(item), left});
    std::stable_sort(out.gather.begin(), out.gather.end(), [](GatherLine const& a, GatherLine const& b) { return a.left > b.left; });
    // reduce() lists a product before what it is made from; making goes the other way.
    for (auto step = plan.result.steps.rbegin(); step != plan.result.steps.rend(); ++step) {
        // A traced group's step (L-386) lays in the member's recipe planned() named for it.
        auto const grouped = plan.groupRecipes.find(step->item);
        int const recipe = grouped != plan.groupRecipes.end() ? grouped->second
                         : step->item >= 0 && step->item < static_cast<Item>(choice.recipe.size()) ? choice.recipe[step->item] : -1;
        if (recipe < 0) continue;
        out.steps.push_back({step->item, recipe, step->crafts, step->crafts * index.recipes[recipe].count, step->spare, heldOf(step->item)});
    }
    for (auto [item, have] : plan.result.fromHeld) {
        bool const listed = std::ranges::any_of(out.gather, [&](GatherLine const& line) { return line.id == item; })
            || std::ranges::any_of(out.steps, [&](StepLine const& step) { return step.item == item; });
        if (!listed) out.done.push_back({item, have, have, 0});
    }
    return out;
}
// What held stock the plan leaves unused, by item (maintainer 2026-11-27: the rows' held counts stop at the need).
// A group's members lent to it count as used; `heldOf` adds up a group's members.
inline std::map<Item, Count> unusedStock(Index const& index, Request const& request, Planned const& plan) {
    std::map<Item, Count> out;
    Item const items = static_cast<Item>(index.items.size());
    for (auto [item, have] : request.held) out[item] += have;
    auto take = [&](Item item, Count used) { auto& have = out[item]; have -= std::min(have, used); };
    for (auto [item, used] : plan.result.fromHeld) if (item < items) take(item, used);
    for (auto const& cover : plan.result.covered) take(cover.member, cover.count);
    std::erase_if(out, [](auto const& entry) { return entry.second <= 0; });
    return out;
}
inline RawView rawView(Index const& index, Choice const& choice, Request const& request) {
    return rawView(index, choice, planned(index, choice, request));
}
// How many of a recipe's `i`-th use `crafts` take: a brewing ingredient serves a batch of crafts (L-361).
inline Count partNeed(Recipe const& recipe, size_t i, Count crafts) {
    Count const count = recipe.uses[i].count;
    return i == 0 && recipe.batch > 1 ? (crafts + recipe.batch - 1) / recipe.batch * count : crafts * count;
}
// One input of a step as the plan counts it: the part as its slot shows it (a group: any member, with how many a
// craft takes), the item it stands for, and how many all the step's crafts take. A traced group's step (L-386) takes
// its kind, whichever member's recipe it lays in; `batch`: a brewing ingredient's (L-361), 0 for every other.
struct StepPart {
    Part part;
    Item item = 0;
    Count need = 0, batch = 0;
};
// Whether a step makes a traced group (its item past Index::items) rather than an item.
inline bool groupStep(Index const& index, StepLine const& step) { return step.item >= static_cast<Item>(index.items.size()); }
inline std::vector<StepPart> stepParts(Index const& index, Request const& request, StepLine const& step) {
    std::vector<StepPart> out;
    if (groupStep(index, step)) {
        size_t const group = static_cast<size_t>(step.item - static_cast<Item>(index.items.size()));
        if (group < index.traces.size() && index.traces[group].kind >= 0) {
            auto const& trace = index.traces[group];
            Part const kind{true, trace.kind, trace.uses};
            out.push_back({kind, partItem(index, request, kind), step.crafts * trace.uses, 0});
            return out;
        }
    }
    auto const& recipe = index.recipes[static_cast<size_t>(step.recipe)];
    for (size_t i = 0; i < recipe.uses.size(); ++i)
        out.push_back({recipe.uses[i], partItem(index, request, recipe.uses[i]), partNeed(recipe, i, step.crafts), i == 0 ? recipe.batch : 0});
    return out;
}
// What a step takes in all its crafts, each part as the item it stands for in the plan.
inline std::vector<std::pair<Item, Count>> stepInputs(Index const& index, Request const& request, StepLine const& step) {
    std::vector<std::pair<Item, Count>> out;
    for (auto const& part : stepParts(index, request, step)) out.emplace_back(part.item, part.need);
    return out;
}
}
