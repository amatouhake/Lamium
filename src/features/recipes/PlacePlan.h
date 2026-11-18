#pragma once
#include "features/recipes/RecipeIndex.h"
#include <algorithm>
#include <map>
#include <vector>

// L-359 step 9, L-165: how a crafting recipe is laid into the grid from what
// the player carries. Pure: the slots are numbers the caller gives a meaning.
namespace lamium::recipes {
// One carried stack: where it is, what and how many, and how many of it one grid cell holds.
struct PlaceSource { int slot = 0; Item item = -1; int count = 0, stack = 64; };
// `amount` from `slot` into grid cell `cell` (row by row).
struct PlaceMove { int slot = 0, amount = 0, cell = 0; bool operator==(PlaceMove const&) const = default; };
struct PlacePlan {
    bool fits = false;         // the grid is large enough for the recipe's shape
    std::vector<int> missing;  // positions in Recipe::cells nothing carried serves
    int crafts = 0;            // how many times the laid grid makes the recipe
    std::vector<PlaceMove> moves;
    bool ready() const { return fits && missing.empty() && crafts > 0; }
};
// `side`: the grid's, 2 or 3. `picks`: the member the player picked for a
// group. One of each ingredient, or with `many` as many as divide evenly from
// every stack carried, a cell's stack at most. An ingredient any of several
// items satisfies takes the picked member while it lasts, else the one most
// carried; `many` then takes the one with most left, so several kinds add up.
inline PlacePlan placePlan(Index const& index, Recipe const& recipe, int side, std::vector<PlaceSource> const& carried,
                           std::map<int, Item> const& picks, bool many) {
    PlacePlan out;
    int const count = static_cast<int>(recipe.cells.size());
    int const across = recipe.width ? recipe.width : side;
    if (recipe.station != Station::Crafting || side < 2 || side > 3 || across < 1 || across > side || (count + across - 1) / across > side) return out;
    out.fits = true;
    std::map<Item, int> total, used; // carried of an item; cells it serves
    std::map<Item, int> stack;
    for (auto const& source : carried) {
        if (source.item < 0 || source.count <= 0) continue;
        total[source.item] += source.count;
        auto [slot, fresh] = stack.emplace(source.item, source.stack);
        if (!fresh) slot->second = std::min(slot->second, source.stack);
    }
    auto left = [&](Item item) {
        auto found = total.find(item);
        return found == total.end() ? 0 : found->second - used[item];
    };
    std::vector<std::pair<int, Item>> served; // grid cell, item
    for (int i = 0; i < count; ++i) {
        auto const& part = recipe.cells[static_cast<size_t>(i)];
        if (part.id < 0) continue;
        Item chosen = -1;
        if (!part.group) chosen = left(part.id) > 0 ? part.id : -1;
        else {
            auto const& members = index.groups[static_cast<size_t>(part.id)].members;
            if (auto picked = picks.find(part.id); picked != picks.end() && left(picked->second) > 0
                && std::find(members.begin(), members.end(), picked->second) != members.end())
                chosen = picked->second;
            else {
                int best = 0;
                for (Item member : members) {
                    if (left(member) <= 0) continue;
                    int const score = many ? left(member) : total[member];
                    if (score > best) best = score, chosen = member;
                }
            }
        }
        if (chosen < 0) { out.missing.push_back(i); continue; }
        ++used[chosen];
        served.emplace_back(i / across * side + i % across, chosen);
    }
    if (!out.missing.empty()) return out;
    int each = 1;
    if (many) {
        each = 64;
        for (auto const& [item, cells] : used) if (cells > 0) each = std::min({each, total[item] / cells, stack[item]});
    }
    out.crafts = each;
    // Each cell draws from the item's stacks in the order given, a stack after the one before it is used up.
    std::vector<PlaceSource> stock = carried;
    for (auto const& [cell, item] : served) {
        int need = each;
        for (auto& source : stock) {
            if (need <= 0) break;
            if (source.item != item || source.count <= 0) continue;
            int const take = std::min(need, source.count);
            out.moves.push_back({source.slot, take, cell});
            source.count -= take;
            need -= take;
        }
    }
    return out;
}
}
