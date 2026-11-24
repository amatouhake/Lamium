#pragma once
#include "features/recipes/ItemListLayout.h"
#include "features/recipes/RecipeIndex.h"
#include <algorithm>
#include <cmath>
#include <optional>
#include <string_view>
#include <vector>

// L-359: what the recipe view shows for an item and where, in GUI units. Pure.
namespace lamium::recipes {
// One kind of work's recipes: a tab (L-376). `station` stands for the kind (its picture and layout); `only`:
// the tab of what one station does, which a station's uses open with.
struct ViewTab {
    Station station = Station::Other;
    std::vector<int> recipes;
    Kind kind = Kind::Other;
    std::optional<Station> only;
};
// The station whose block an item is; none for other items.
inline std::optional<Station> stationOfItem(std::string_view key) {
    for (int s = 0; s < static_cast<int>(Station::Other); ++s)
        if (stationItem(static_cast<Station>(s)) == key) return static_cast<Station>(s);
    return std::nullopt;
}
// The stations of a tab's process its recipes are done at, in order (campfire cooking: both campfires). A
// station's own tab names them all too, so the process looks the same however it was opened.
inline std::vector<Station> tabStations(Index const& index, ViewTab const& tab) {
    std::vector<Station> out;
    for (int number : tab.recipes)
        for (Station s : index.recipes[static_cast<size_t>(number)].stations)
            if (kindOf(s) == tab.kind && std::find(out.begin(), out.end(), s) == out.end()) out.push_back(s);
    std::sort(out.begin(), out.end());
    return out;
}
inline bool usesItem(Index const& index, Recipe const& recipe, Item item) {
    return std::any_of(recipe.uses.begin(), recipe.uses.end(), [&](Part const& part) { return serves(index, part, item); });
}
// The recipes that make an item or that use it, by kind of work, the tabs in the kinds' own order whatever is in
// use (maintainer 2026-11-21: tabs that moved with the choice were hard to follow); in a tab the recipe in use comes
// first. A station's uses begin with what it does, none twice.
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
    if (uses)
        if (auto const station = stationOfItem(index.items[static_cast<size_t>(item)])) {
            ViewTab does{*station, {}, kindOf(*station), *station};
            for (size_t i = 0; i < index.recipes.size(); ++i) {
                auto const& stations = index.recipes[i].stations;
                if (std::find(stations.begin(), stations.end(), *station) != stations.end()) does.recipes.push_back(static_cast<int>(i));
            }
            if (!does.recipes.empty()) {
                std::erase_if(list, [&](int r) { return std::find(does.recipes.begin(), does.recipes.end(), r) != does.recipes.end(); });
                tabs.push_back(std::move(does));
            }
        }
    // A recipe goes into the tab of each process that does it (steak: smelting, smoking, campfire cooking).
    for (int recipe : list)
        for (Station s : index.recipes[static_cast<size_t>(recipe)].stations) {
            Kind const kind = kindOf(s);
            auto tab = std::find_if(tabs.begin(), tabs.end(), [&](ViewTab const& other) { return other.kind == kind && !other.only; });
            if (tab == tabs.end()) tab = tabs.insert(tabs.end(), ViewTab{usualStation(kind), {}, kind, std::nullopt});
            if (std::find(tab->recipes.begin(), tab->recipes.end(), recipe) == tab->recipes.end()) tab->recipes.push_back(recipe);
        }
    if (!uses) std::stable_sort(tabs.begin(), tabs.end(), [](ViewTab const& a, ViewTab const& b) { return a.kind < b.kind; });
    return tabs;
}
inline bool anyOrder(Recipe const& recipe) { return recipe.station == Station::Crafting && recipe.anyOrder; }

// A recipe's block: its slots and its buttons, as wide for every station, as high as the station's slots.
inline constexpr float viewBlockWidth = 92, viewButton = 11;
inline float viewBlockHeight(Station station) {
    return station == Station::Crafting || station == Station::BrewingStand ? 54.f : station == Station::Stonecutter ? 24.f : 48.f;
}
// One recipe as its station's screen lays it out, from the block's corner.
// `cell` is a position in Recipe::cells, -1 an empty slot. The arrow and the
// product stand at the middle of the ingredients' height. `place` is the
// recipe's "place" button at the block's bottom right, `choose` its check box
// (the same size) left of it (L-379), `note` the place of its small mark.
struct ViewSlot { float x = 0, y = 0; int cell = -1; };
struct ViewBlock {
    std::vector<ViewSlot> slots;
    float arrowX = 0, arrowY = 0, productX = 0, productY = 0, placeX = 0, placeY = 0, chooseX = 0, chooseY = 0, noteX = 0, noteY = 0;
    // Brewing (L-361): laid out as the stand (no right arrow; its parts drawn), the fuel's slot showing blaze powder.
    bool brewing = false;
    float fuelX = 0, fuelY = 0;
};
inline ViewBlock viewBlock(Recipe const& recipe) {
    constexpr float cell = 18;
    ViewBlock out;
    out.placeX = viewBlockWidth - viewButton, out.placeY = viewBlockHeight(recipe.station) - viewButton;
    out.chooseX = out.placeX - 2 - viewButton, out.chooseY = out.placeY;
    int const count = static_cast<int>(recipe.cells.size());
    if (recipe.station == Station::Crafting) {
        // A shaped recipe keeps its place in the 3x3 grid; one in any order is packed from the top left.
        int const across = recipe.width ? recipe.width : 3;
        for (int i = 0; i < 9; ++i) out.slots.push_back({static_cast<float>(i % 3) * cell, static_cast<float>(i / 3) * cell, -1});
        for (int i = 0; i < count && i / across < 3; ++i)
            if (recipe.cells[static_cast<size_t>(i)].id >= 0) out.slots[static_cast<size_t>(gridCell(i, across, 3))].cell = i;
        out.arrowX = 58, out.arrowY = 25, out.productX = 70, out.productY = 18;
        out.noteX = 56, out.noteY = out.chooseY;
    } else if (recipe.station == Station::Stonecutter) {
        out.slots.push_back({0, 3, count ? 0 : -1});
        out.arrowX = 22, out.arrowY = 10, out.productX = 34, out.productY = 3;
    } else if (recipe.station == Station::BrewingStand) {
        // As the stand (demos/brewing-view.html, F): the fuel at the top left, the ingredient at the top, the bottle
        // before at the left, the middle bottle empty, the bottle after at the right; all but the fuel 4 left of
        // the mockup, so the middle bottle keeps room from the check box (maintainer 2026-11-21).
        out.brewing = true, out.fuelX = 0, out.fuelY = 2;
        out.slots.push_back({46, 0, count > 0 ? 0 : -1});
        out.slots.push_back({24, 22, count > 1 ? 1 : -1});
        out.slots.push_back({46, 30, -1});
        out.productX = 68, out.productY = 22;
    } else if (smelts(recipe.station)) {
        // The input over the fuel's slot, which stays empty: fuel is not part of a recipe.
        out.slots.push_back({0, 0, count ? 0 : -1});
        out.slots.push_back({0, 30, -1});
        out.arrowX = 22, out.arrowY = 22, out.productX = 34, out.productY = 15;
    } else {
        int const shown = std::min(count, 4);
        for (int i = 0; i < shown; ++i) out.slots.push_back({static_cast<float>(i) * cell, 15, recipe.cells[static_cast<size_t>(i)].id >= 0 ? i : -1});
        out.arrowX = static_cast<float>(shown) * cell + 4, out.arrowY = 22;
        out.productX = out.arrowX + 12, out.productY = 15;
    }
    return out;
}

// The window: always two recipe columns wide and as tall as the screen
// allows above the search field's place, so no item changes its size. A
// station's recipes all take the same room; where one column holds them all
// it sits in the middle.
struct RecipeViewLayout {
    // Two columns of three rows a page, with room between them and inside the window's edge (L-367). Each
    // recipe stands on a raised face `pad` around its block, faces `gap` apart (L-379: 102 wide, 6 apart).
    static constexpr float width = 236, margin = 10, column = (width - 2 * margin) / 2, block = viewBlockWidth, head = 35, foot = 12;
    static constexpr float pad = 5, gap = column - block - 2 * pad;
    static constexpr int mostRows = 3;
    // The game's tabs stand on the window's top edge, above it (windowTab's rise).
    static constexpr float tabsAbove = 21;
    static float blockHeight(Station station) { return viewBlockHeight(station); }
    // A row's least room: the face and the gap under it.
    static float rowRoom(Station station) { return blockHeight(station) + 2 * pad + gap; }
    // Three rows of crafting recipes with room between them: taller only moved the eye further (L-367).
    static constexpr float mostHeight = head + foot + 3 * (54 + 2 * pad + gap);
    float left = 0, top = 0, height = 0;
    int rows(Station station) const {
        return std::clamp(static_cast<int>((height - head - foot) / rowRoom(station)), 1, mostRows);
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
// The Recipes window (L-370) holds more than a recipe: it always takes the width the game's inventory has with
// its recipe book open, whether the book is open or not. `coverLeft`/`coverRight`: what the game's window (or
// the recipe view, if wider) spans now. Far wider than the recipe view, the book is open: that span exactly.
// Otherwise `openWidth` (the book-open width last seen; before any, 326 units, as the game logged it) centred.
struct WindowSpan { float left = 0, width = 0; };
inline constexpr float planWindowWidth = 326;
inline bool bookOpenSpan(float coverLeft, float coverRight) { return coverRight - coverLeft > RecipeViewLayout::width + 40; }
inline WindowSpan planWindowAt(float screenWidth, float coverLeft, float coverRight, float openWidth = planWindowWidth) {
    if (bookOpenSpan(coverLeft, coverRight)) return {coverLeft, coverRight - coverLeft};
    return {std::floor((screenWidth - openWidth) / 2), openWidth};
}
inline RecipeViewLayout recipeViewAt(float screenWidth, float screenHeight) {
    RecipeViewLayout out;
    out.left = (screenWidth - RecipeViewLayout::width) / 2;
    float const roomTop = 1 + RecipeViewLayout::tabsAbove, room = screenHeight - searchTop - 4 - roomTop;
    out.height = std::min(room, RecipeViewLayout::mostHeight);
    out.top = std::floor(roomTop + (room - out.height) / 2);
    return out;
}
}
