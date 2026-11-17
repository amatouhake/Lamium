#pragma once
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
    std::vector<Cover> covered;
    size_t heldKinds = 0; // items and groups that held stock covers a part of
};
inline RawView rawView(Index const& index, Choice const& choice, Request const& request) {
    Planned plan = planned(index, choice, request);
    RawView out;
    out.covered = std::move(plan.covered);
    out.heldKinds = plan.result.fromHeld.size();
    auto heldOf = [&](Item item) {
        for (auto [id, count] : plan.result.fromHeld) if (id == item) return count;
        return Count{0};
    };
    for (auto [item, left] : plan.result.raw) out.gather.push_back({item, left + heldOf(item), heldOf(item), left});
    std::stable_sort(out.gather.begin(), out.gather.end(), [](GatherLine const& a, GatherLine const& b) { return a.left > b.left; });
    // reduce() lists a product before what it is made from; making goes the other way.
    for (auto step = plan.result.steps.rbegin(); step != plan.result.steps.rend(); ++step) {
        int const recipe = step->item >= 0 && step->item < static_cast<Item>(choice.recipe.size()) ? choice.recipe[step->item] : -1;
        if (recipe < 0) continue;
        out.steps.push_back({step->item, recipe, step->crafts, step->crafts * index.recipes[recipe].count, step->spare, heldOf(step->item)});
    }
    return out;
}
// The item a recipe's part stands for in a plan: itself, the picked member of its group, or the group.
inline Item partItem(Index const& index, Request const& request, Part const& part) {
    if (!part.group) return part.id;
    auto picked = request.picks.find(part.id);
    return picked != request.picks.end() ? picked->second : index.groupItem(part.id);
}
// The steps that use `id` and how many each takes, in the view's order.
inline std::vector<std::pair<Item, Count>> usersOf(Index const& index, Request const& request, RawView const& view, Item id) {
    std::vector<std::pair<Item, Count>> out;
    for (auto const& step : view.steps) {
        Count used = 0;
        for (auto const& part : index.recipes[step.recipe].uses) if (partItem(index, request, part) == id) used += step.crafts * part.count;
        if (used) out.emplace_back(step.item, used);
    }
    return out;
}
}
