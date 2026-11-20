#pragma once
#include <algorithm>
#include <cstdint>
#include <map>
#include <set>
#include <utility>
#include <vector>

// L-116 groundwork: what a list of wanted items costs in raw materials, given
// one chosen recipe per craftable item. Which recipe is chosen, and how game
// items map to the ids, is the caller's business. Not used by any screen yet.
// `wanted` is everything still to be supplied, held or not: what is held is
// taken off here, once per item, so a caller must not subtract it first.
namespace lamium::recipes::rawMaterials {
using Item = int;
using Count = std::uint64_t;
struct Recipe {
    Count makes = 1; // Items per craft.
    std::vector<std::pair<Item, Count>> uses;
    Count batch = 0; // brewing: the first use serves up to this many crafts (one ingredient, three bottles); 0: none
};
using Recipes = std::map<Item, Recipe>;
struct Step {
    Item item = 0;
    Count crafts = 0, spare = 0; // spare: made beyond what was needed.
    bool operator==(Step const&) const = default;
};
struct Result {
    std::vector<std::pair<Item, Count>> raw; // By item id.
    std::vector<Step> steps;                 // An item comes before the items it is made from.
    std::vector<Item> looped;                // Left raw because their recipes lead back to themselves.
    std::vector<std::pair<Item, Count>> fromHeld; // What held stock covers, by item id.
};

// Needs are added up across the whole list before rounding to whole crafts,
// so two wanted items sharing an intermediate share its spare output.
// Held stock is used before anything is made or gathered: of the wanted item
// itself, of an intermediate (which is then not traced further for that
// part) and of a raw material.
inline Result reduce(std::vector<std::pair<Item, Count>> const& wanted, Recipes const& recipes,
                     std::vector<std::pair<Item, Count>> const& held = {}) {
    std::map<Item, Count> need, stock;
    for (auto [item, count] : held) stock[item] += count;
    std::set<Item> asked;
    for (auto [item, count] : wanted) if (count) { need[item] += count; asked.insert(item); }
    auto recipeOf = [&](Item item) -> Recipe const* {
        auto found = recipes.find(item);
        return found != recipes.end() && found->second.makes > 0 && !found->second.uses.empty() ? &found->second : nullptr;
    };
    // Every item reachable from the wanted ones, and how many of its users are still unprocessed.
    std::map<Item, int> users;
    std::vector<Item> open;
    for (auto const& [item, count] : need) { users.emplace(item, 0); open.push_back(item); }
    while (!open.empty()) {
        Item const item = open.back();
        open.pop_back();
        if (auto const* recipe = recipeOf(item))
            for (auto const& [part, count] : recipe->uses) {
                if (!count) continue;
                auto [slot, fresh] = users.emplace(part, 0);
                ++slot->second;
                if (fresh) open.push_back(part);
            }
    }
    Result result;
    std::set<Item> left;
    for (auto const& [item, count] : users) left.insert(item);
    auto release = [&](Recipe const& recipe) {
        for (auto const& [part, count] : recipe.uses) if (count) --users[part];
    };
    // Whether `item` leads back to itself through the recipes of items still left: only an item on a loop may
    // be kept raw to break it, never one that is merely made from a loop's item.
    auto onLoop = [&](Item item) {
        std::set<Item> seen;
        std::vector<Item> stack{item};
        while (!stack.empty()) {
            Item const at = stack.back();
            stack.pop_back();
            auto const* recipe = recipeOf(at);
            if (!recipe) continue;
            for (auto const& [part, count] : recipe->uses) {
                if (!count || !left.contains(part)) continue;
                if (part == item) return true;
                if (seen.insert(part).second) stack.push_back(part);
            }
        }
        return false;
    };
    while (!left.empty()) {
        Item next = 0;
        bool found = false, loop = false;
        for (Item item : left) if (users[item] == 0) { next = item; found = true; break; }
        if (!found) {
            // Only loops remain. Keep one item on a loop raw: one the player did not ask for, if any.
            loop = true;
            for (Item item : left) if (!asked.contains(item) && onLoop(item)) { next = item; found = true; break; }
            if (!found) for (Item item : left) if (onLoop(item)) { next = item; found = true; break; }
            if (!found) next = *left.begin();
        }
        left.erase(next);
        auto const* recipe = recipeOf(next);
        if (loop) {
            result.looped.push_back(next);
            if (recipe) release(*recipe);
            continue; // Its need is settled once its users are done; collected below.
        }
        if (!recipe) continue;
        release(*recipe);
        // Every user of `next` is done, so its need is whole: held stock comes off once.
        Count const wantedCount = need[next] - std::min(need[next], stock[next]);
        if (!wantedCount) continue;
        Count const crafts = (wantedCount + recipe->makes - 1) / recipe->makes;
        result.steps.push_back({next, crafts, crafts * recipe->makes - wantedCount});
        for (size_t i = 0; i < recipe->uses.size(); ++i) {
            auto const& [part, count] = recipe->uses[i];
            if (!count) continue;
            need[part] += i == 0 && recipe->batch > 1 ? (crafts + recipe->batch - 1) / recipe->batch * count : crafts * count;
        }
    }
    for (auto const& [item, count] : need) {
        Count const have = std::min(count, stock[item]);
        if (have) result.fromHeld.emplace_back(item, have);
        bool const looped = std::find(result.looped.begin(), result.looped.end(), item) != result.looped.end();
        if (count > have && (looped || !recipeOf(item))) result.raw.emplace_back(item, count - have);
    }
    return result;
}
}
