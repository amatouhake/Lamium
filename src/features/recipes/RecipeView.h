#pragma once
#include "features/recipes/ItemListLayout.h"
#include "features/recipes/RecipeIndex.h"
#include <algorithm>
#include <cmath>
#include <vector>

// L-359: what the recipe view shows for an item and where, in GUI units. Pure.
namespace lamium::recipes {
// One station's recipes: a tab.
struct ViewTab { Station station = Station::Other; std::vector<int> recipes; };
inline bool usesItem(Index const& index, Recipe const& recipe, Item item) {
    for (auto const& part : recipe.uses) {
        if (!part.group) { if (part.id == item) return true; continue; }
        auto const& members = index.groups[static_cast<size_t>(part.id)].members;
        if (std::find(members.begin(), members.end(), item) != members.end()) return true;
    }
    return false;
}
// The recipes that make an item (the one in use first, so its station is the
// first tab) or that use it (stations in their own order), by station.
inline std::vector<ViewTab> viewTabs(Index const& index, Choice const& choice, Item item, bool uses) {
    if (item < 0 || item >= static_cast<Item>(index.items.size())) return {};
    std::vector<int> list;
    if (uses) {
        for (size_t i = 0; i < index.recipes.size(); ++i)
            if (usesItem(index, index.recipes[i], item)) list.push_back(static_cast<int>(i));
        std::stable_sort(list.begin(), list.end(), [&](int a, int b) {
            return index.recipes[static_cast<size_t>(a)].station < index.recipes[static_cast<size_t>(b)].station;
        });
    } else {
        list = index.makers[static_cast<size_t>(item)];
        auto const& copies = index.copies[static_cast<size_t>(item)];
        list.insert(list.end(), copies.begin(), copies.end());
        int const chosen = static_cast<size_t>(item) < choice.recipe.size() ? choice.recipe[static_cast<size_t>(item)] : -1;
        std::stable_sort(list.begin(), list.end(), [&](int a, int b) {
            if ((a == chosen) != (b == chosen)) return a == chosen;
            if (index.recipes[static_cast<size_t>(a)].own != index.recipes[static_cast<size_t>(b)].own) return index.recipes[static_cast<size_t>(b)].own;
            return before(index, index.recipes[static_cast<size_t>(a)], index.recipes[static_cast<size_t>(b)]);
        });
    }
    std::vector<ViewTab> tabs;
    for (int recipe : list) {
        Station const station = index.recipes[static_cast<size_t>(recipe)].station;
        auto tab = std::find_if(tabs.begin(), tabs.end(), [&](ViewTab const& other) { return other.station == station; });
        if (tab == tabs.end()) tab = tabs.insert(tabs.end(), ViewTab{station, {}});
        tab->recipes.push_back(recipe);
    }
    return tabs;
}
// A crafting recipe the inventory's own 2x2 grid can make.
inline bool fitsSmallGrid(Recipe const& recipe) {
    if (recipe.station != Station::Crafting) return false;
    if (!recipe.width) return recipe.cells.size() <= 4;
    return recipe.width <= 2 && recipe.cells.size() / static_cast<size_t>(recipe.width) <= 2;
}
inline bool anyOrder(Recipe const& recipe) { return recipe.station == Station::Crafting && recipe.anyOrder; }

// One recipe as its station's screen lays it out, from the block's corner.
// `cell` is a position in Recipe::cells, -1 an empty slot. The arrow and the
// product stand at the middle of the ingredients' height. `marks` is a line
// for the recipe's button, `note` the place of its small mark.
struct ViewSlot { float x = 0, y = 0; int cell = -1; };
struct ViewBlock {
    std::vector<ViewSlot> slots;
    float arrowX = 0, arrowY = 0, productX = 0, productY = 0, marksX = 0, marksY = 0, noteX = 0, noteY = 0;
};
inline ViewBlock viewBlock(Recipe const& recipe) {
    constexpr float cell = 18;
    ViewBlock out;
    int const count = static_cast<int>(recipe.cells.size());
    if (recipe.station == Station::Crafting) {
        // A shaped recipe keeps its place in the 3x3 grid; one in any order is packed from the top left.
        int const across = recipe.width ? recipe.width : 3;
        for (int i = 0; i < 9; ++i) out.slots.push_back({static_cast<float>(i % 3) * cell, static_cast<float>(i / 3) * cell, -1});
        for (int i = 0; i < count && i / across < 3; ++i)
            if (recipe.cells[static_cast<size_t>(i)].id >= 0) out.slots[static_cast<size_t>(i / across * 3 + i % across)].cell = i;
        out.arrowX = 58, out.arrowY = 25, out.productX = 70, out.productY = 18, out.marksX = 58, out.marksY = 2;
        out.noteX = 58, out.noteY = 42;
    } else if (recipe.station == Station::Stonecutter) {
        out.slots.push_back({0, 6, count ? 0 : -1});
        out.arrowX = 22, out.arrowY = 13, out.productX = 34, out.productY = 6, out.marksX = 58, out.marksY = 10;
    } else if (smelts(recipe.station)) {
        // The input over the fuel's slot, which stays empty: fuel is not part of a recipe.
        out.slots.push_back({0, 0, count ? 0 : -1});
        out.slots.push_back({0, 30, -1});
        out.arrowX = 22, out.arrowY = 22, out.productX = 34, out.productY = 15, out.marksX = 58, out.marksY = 2;
    } else {
        int const shown = std::min(count, 4);
        for (int i = 0; i < shown; ++i) out.slots.push_back({static_cast<float>(i) * cell, 15, recipe.cells[static_cast<size_t>(i)].id >= 0 ? i : -1});
        out.arrowX = static_cast<float>(shown) * cell + 4, out.arrowY = 22;
        out.productX = out.arrowX + 12, out.productY = 15, out.marksX = 0, out.marksY = 38;
    }
    return out;
}

// The window: always two recipe columns wide and as tall as the screen
// allows above the search field's place, so no item changes its size. A
// station's recipes all take the same room; where one column holds them all
// it sits in the middle.
struct RecipeViewLayout {
    // Two columns of three rows a page, with room between them and inside the window's edge (L-367).
    static constexpr float width = 220, margin = 10, column = (width - 2 * margin) / 2, block = 92, head = 31, foot = 12;
    static constexpr int mostRows = 3;
    // The game's tabs stand on the window's top edge, above it (windowTab's rise).
    static constexpr float tabsAbove = 21;
    // Three rows of crafting recipes with room between them: taller only moved the eye further (L-367).
    static constexpr float mostHeight = head + foot + 3 * 66;
    float left = 0, top = 0, height = 0;
    static float blockHeight(Station station) { return station == Station::Crafting ? 59.f : station == Station::Stonecutter ? 31.f : 53.f; }
    int rows(Station station) const {
        return std::clamp(static_cast<int>((height - head - foot) / blockHeight(station)), 1, mostRows);
    }
    // The rows share the height between the heading and the key hints.
    float rowStep(Station station) const { return (height - head - foot) / static_cast<float>(rows(station)); }
    int perPage(Station station) const { return rows(station) * 2; }
    int pages(Station station, int count) const { return std::max(1, (count + perPage(station) - 1) / perPage(station)); }
    int columns(Station station, int count) const { return count <= rows(station) ? 1 : 2; }
    // The i-th block of a page, of `count` recipes in the tab: left to right, then down.
    float blockX(Station station, int count, int i) const {
        return columns(station, count) == 1 ? left + (width - block) / 2 : left + margin + static_cast<float>(i % 2) * column + (column - block) / 2;
    }
    float blockY(Station station, int count, int i) const {
        float const step = rowStep(station);
        return top + head + static_cast<float>(columns(station, count) == 1 ? i : i / 2) * step + std::max(0.f, (step - blockHeight(station)) / 2);
    }
    bool has(float x, float y) const { return x >= left && x < left + width && y >= top && y < top + height; }
    // The key hints' line: the window's last, and the same place while it is closed; clear of the
    // window's three-unit edge below it (it reaches into the last row's spacing, which a block never fills).
    float hintY() const { return top + height - 13; }
};
// Between a unit below the screen's top (the tabs) and 4 above the search field: the window keeps the
// height of three rows and stands in the middle of that room, or fills it where it is lower.
inline RecipeViewLayout recipeViewAt(float screenWidth, float screenHeight) {
    RecipeViewLayout out;
    out.left = (screenWidth - RecipeViewLayout::width) / 2;
    float const roomTop = 1 + RecipeViewLayout::tabsAbove, room = screenHeight - searchTop - 4 - roomTop;
    out.height = std::min(room, RecipeViewLayout::mostHeight);
    out.top = std::floor(roomTop + (room - out.height) / 2);
    return out;
}
}
