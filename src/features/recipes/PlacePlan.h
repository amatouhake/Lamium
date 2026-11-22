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
// A step to make (L-385): the recipes that make it, a traced group's members in order of preference (L-386), and
// one count of rounds for them all; at the step's turn the screen lays in the one it can lay most of, so what an
// earlier step made counts.
struct MakeRun {
    std::vector<int> recipes;
    int rounds = 0;
    bool operator==(MakeRun const&) const = default;
};
struct PlaceMove { int slot = 0, amount = 0, cell = 0; bool operator==(PlaceMove const&) const = default; };
struct PlacePlan {
    bool fits = false;         // the grid is large enough for the recipe's shape
    std::vector<int> missing;  // positions in Recipe::cells nothing carried serves
    int crafts = 0;            // how many times the laid grid makes the recipe
    std::vector<PlaceMove> moves;
    bool ready() const { return fits && missing.empty() && crafts > 0; }
};
// Whether the grid (`grid`: per cell row by row, -1 empty) already holds `recipe` as placePlan lays it:
// each ingredient's cell has it, or any member of its group, and every other cell is empty. A press to
// lay one of each then has nothing to do.
inline bool alreadyLaid(Index const& index, Recipe const& recipe, int side, std::vector<Item> const& grid) {
    int const count = static_cast<int>(recipe.cells.size()), across = recipe.width ? recipe.width : side;
    if (recipe.station != Station::Crafting || side < 2 || side > 3 || !fitsGrid(recipe, side) || grid.size() != static_cast<size_t>(side * side))
        return false;
    std::vector<bool> used(grid.size(), false);
    for (int i = 0; i < count; ++i) {
        auto const& part = recipe.cells[static_cast<size_t>(i)];
        if (part.id < 0) continue;
        size_t const cell = static_cast<size_t>(gridCell(i, across, side));
        if (!serves(index, part, grid[cell])) return false;
        used[cell] = true;
    }
    for (size_t cell = 0; cell < grid.size(); ++cell) if (!used[cell] && grid[cell] >= 0) return false;
    return true;
}
// One more of each ingredient onto a grid that already holds the recipe (alreadyLaid; `counts` per cell):
// each cell takes one of the item lying in it, from the stacks carried in order. Empty when a cell is a
// full stack or something is short, so a press then changes nothing (L-367).
inline std::vector<PlaceMove> oneMore(Index const& index, Recipe const& recipe, int side, std::vector<Item> const& grid,
                                      std::vector<int> const& counts, std::vector<PlaceSource> const& carried) {
    if (!alreadyLaid(index, recipe, side, grid) || counts.size() != grid.size()) return {};
    std::vector<PlaceSource> stock = carried;
    std::vector<PlaceMove> out;
    for (size_t cell = 0; cell < grid.size(); ++cell) {
        if (grid[cell] < 0) continue;
        auto source = std::ranges::find_if(stock, [&](PlaceSource const& s) { return s.item == grid[cell] && s.count > 0; });
        if (source == stock.end() || counts[cell] >= source->stack) return {};
        --source->count;
        out.push_back({source->slot, 1, static_cast<int>(cell)});
    }
    return out;
}
// How many of `product` the inventory can take once `moves` took their items into the grid (L-385, 2026-11-22): its
// `empty` slots, the slots the moves empty (sixty-four planks laid in leave room for sixty-four buttons), and what
// stacks of the product can still take; `stack` a stack where none is carried.
inline long long roomAfter(std::vector<PlaceSource> const& carried, std::vector<PlaceMove> const& moves, int empty, Item product, int stack) {
    std::map<int, int> taken;
    for (auto const& move : moves) taken[move.slot] += move.amount;
    for (auto const& source : carried) if (source.item == product) stack = source.stack;
    long long room = 0, slots = std::max(0, empty);
    for (auto const& source : carried) {
        auto const found = taken.find(source.slot);
        int const left = source.count - (found == taken.end() ? 0 : found->second);
        if (left <= 0) ++slots;
        else if (source.item == product) room += std::max(0, source.stack - left);
    }
    return room + slots * std::max(1, stack);
}
// `side`: the grid's, 2 or 3. `picks`: the member the player picked for a
// group. One of each ingredient, or with `many` as many as divide evenly from
// every stack carried, a cell's stack at most. An ingredient any of several
// items satisfies takes the picked member while it lasts, else the one most
// carried; `many` then takes the one with most left, so several kinds add up.
// `most`: with `many`, no more crafts than that (a plan's step lays what it still needs, L-385).
inline PlacePlan placePlan(Index const& index, Recipe const& recipe, int side, std::vector<PlaceSource> const& carried,
                           std::map<int, Item> const& picks, bool many, int most = 64) {
    PlacePlan out;
    int const count = static_cast<int>(recipe.cells.size());
    // A station's recipe (`side` 0, L-380): each cell into a slot of its own, in order (the furnaces' input; the
    // smithing table's template, base and addition).
    bool const station = recipe.station != Station::Crafting;
    int const across = station ? 3 : recipe.width ? recipe.width : side;
    if (station ? side != 0 || count < 1 || count > 3 : side < 2 || side > 3 || !fitsGrid(recipe, side)) return out;
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
            if (auto picked = picks.find(part.id); picked != picks.end() && left(picked->second) > 0 && index.isMember(part.id, picked->second))
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
        served.emplace_back(station ? i : gridCell(i, across, side), chosen);
    }
    if (!out.missing.empty()) return out;
    int each = 1;
    if (many) {
        each = std::clamp(most, 1, 64);
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
// Crafting one round onto the pointer (L-379): `ready`, the round can be laid from what is carried; the pointer
// holds `heldCount` of `held` (at most `heldMost` a stack). A pointer that holds another item, or the product
// with no room for the round, gets the round laid in only (L-381); nothing it holds is set down.
enum class CraftOnto { Ready, Missing, PlaceOnly };
inline CraftOnto craftOnto(bool ready, Item product, int makes, Item held, int heldCount, int heldMost) {
    if (!ready) return CraftOnto::Missing;
    if (heldCount <= 0 || (held == product && heldCount + makes <= heldMost)) return CraftOnto::Ready;
    return CraftOnto::PlaceOnly;
}
}
