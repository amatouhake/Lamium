#pragma once
#include "features/recipes/PlacePlan.h"
#include <algorithm>
#include <map>
#include <string_view>
#include <utility>
#include <vector>

// L-165: what a mass craft decides without the game: which recipe lies in the
// grid, what of the product is thrown out, and which stacks of an ingredient
// are more than the recipe's share of the inventory. Pure.
namespace lamium::recipes {
inline bool serves(Index const& index, Part const& part, Item item) {
    if (part.id < 0 || item < 0) return false;
    if (!part.group) return part.id == item;
    auto const& members = index.groups[static_cast<size_t>(part.id)].members;
    return std::find(members.begin(), members.end(), item) != members.end();
}
// The crafting recipe the grid's items lay out, -1 for none. `grid`: side x
// side items row by row, -1 an empty cell. A shape may sit anywhere in the
// grid and mirrored, as the game takes it. Of several that fit, the one
// with the id the game names for the grid, else the first.
inline int recipeInGrid(Index const& index, int side, std::vector<Item> const& grid, std::string_view named = {}) {
    if (side < 2 || side > 3 || static_cast<int>(grid.size()) != side * side) return -1;
    int left = side, right = -1, top = side, bottom = -1;
    std::vector<Item> loose;
    for (int i = 0; i < side * side; ++i) {
        if (grid[static_cast<size_t>(i)] < 0) continue;
        left = std::min(left, i % side), right = std::max(right, i % side);
        top = std::min(top, i / side), bottom = std::max(bottom, i / side);
        loose.push_back(grid[static_cast<size_t>(i)]);
    }
    if (loose.empty()) return -1;
    int const wide = right - left + 1, high = bottom - top + 1;
    int first = -1;
    auto fits = [&](size_t at) {
        if (first < 0) first = static_cast<int>(at);
        return index.recipes[at].id == named;
    };
    for (size_t at = 0; at < index.recipes.size(); ++at) {
        auto const& recipe = index.recipes[at];
        if (recipe.station != Station::Crafting) continue;
        if (recipe.width > 0 && !recipe.anyOrder) {
            int const cells = static_cast<int>(recipe.cells.size());
            if (recipe.width != wide || (cells + wide - 1) / wide != high) continue;
            for (bool mirrored : {false, true}) {
                bool same = true;
                for (int row = 0; row < high && same; ++row)
                    for (int column = 0; column < wide && same; ++column) {
                        int const cell = row * wide + (mirrored ? wide - 1 - column : column);
                        Item const item = grid[static_cast<size_t>((top + row) * side + left + column)];
                        bool const empty = cell >= cells || recipe.cells[static_cast<size_t>(cell)].id < 0;
                        same = empty ? item < 0 : serves(index, recipe.cells[static_cast<size_t>(cell)], item);
                    }
                if (same) { if (fits(at)) return static_cast<int>(at); break; }
            }
            continue;
        }
        // Any order: each ingredient takes one of the grid's items, the named ones before the groups.
        std::vector<Part> parts;
        for (auto const& part : recipe.cells) if (part.id >= 0) parts.push_back(part);
        if (parts.size() != loose.size()) continue;
        std::stable_partition(parts.begin(), parts.end(), [](Part const& part) { return !part.group; });
        auto rest = loose;
        bool same = true;
        for (auto const& part : parts) {
            auto found = std::find_if(rest.begin(), rest.end(), [&](Item item) { return serves(index, part, item); });
            if (found == rest.end()) { same = false; break; }
            rest.erase(found);
        }
        if (same && fits(at)) return static_cast<int>(at);
    }
    return first;
}
// What is thrown out of a slot: all of it, or `amount` of a stack that stays.
struct Throw { int slot = 0, amount = 0; bool whole = false; bool operator==(Throw const&) const = default; };
// The product beyond the `keep` the player carried when the run began, from
// the last slot back: what a take added, and what was thrown out and came
// back (picked up again, or a throw the server refused). `now`: per slot,
// its item and count.
inline std::vector<Throw> excess(std::vector<std::pair<Item, int>> const& now, Item product, int keep) {
    int over = -keep;
    for (auto const& [item, count] : now) if (item == product) over += count;
    std::vector<Throw> out;
    for (size_t slot = now.size(); slot-- > 0 && over > 0;) {
        if (now[slot].first != product || now[slot].second <= 0) continue;
        int const amount = std::min(over, now[slot].second);
        out.push_back({static_cast<int>(slot), amount, amount == now[slot].second});
        over -= amount;
    }
    return out;
}
// The slots to throw out whole when a round cannot be laid in and the
// inventory is full: of the slots the recipe's ingredients fill, each
// ingredient keeps the share its cells are of the recipe (one slot at least),
// and the slots beyond it go, so the others can be picked up. An ingredient
// is never thrown out altogether, a recipe of one ingredient throws nothing,
// and what does not stack (a filled shulker box to dye) is never thrown.
inline std::vector<int> surplusSlots(Index const& index, Recipe const& recipe, std::vector<PlaceSource> const& carried) {
    std::vector<std::pair<Part, int>> kinds; // ingredient, its cells
    int cells = 0;
    for (auto const& part : recipe.cells) {
        if (part.id < 0) continue;
        ++cells;
        auto found = std::find_if(kinds.begin(), kinds.end(), [&](auto const& kind) { return kind.first.group == part.group && kind.first.id == part.id; });
        if (found == kinds.end()) kinds.emplace_back(part, 1); else ++found->second;
    }
    std::stable_partition(kinds.begin(), kinds.end(), [](auto const& kind) { return !kind.first.group; });
    std::vector<std::vector<PlaceSource>> held(kinds.size());
    int filled = 0;
    for (auto const& source : carried) {
        if (source.count <= 0 || source.stack <= 1) continue;
        for (size_t kind = 0; kind < kinds.size(); ++kind)
            if (serves(index, kinds[kind].first, source.item)) { held[kind].push_back(source); ++filled; break; }
    }
    std::vector<int> out;
    for (size_t kind = 0; kind < kinds.size(); ++kind) {
        size_t const share = static_cast<size_t>(std::max(1, filled * kinds[kind].second / cells));
        if (held[kind].size() <= share) continue;
        // The fullest stacks stay.
        std::stable_sort(held[kind].begin(), held[kind].end(), [](PlaceSource const& a, PlaceSource const& b) { return a.count > b.count; });
        for (size_t i = share; i < held[kind].size(); ++i) out.push_back(held[kind][i].slot);
    }
    std::sort(out.begin(), out.end());
    return out;
}
}
