#pragma once
#include "features/recipes/RawView.h"
#include <algorithm>
#include <array>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

// L-389: what How to make lists for an item and a group, apart from drawing. Pure.
namespace lamium::recipes {
// A group's members in the order `rank` gives (the item list's creative order: oak, spruce, birch, ...); members it
// does not know keep their order after the rest.
inline std::vector<Item> rankedMembers(std::vector<Item> members, std::function<int(Item)> const& rank) {
    std::stable_sort(members.begin(), members.end(), [&](Item a, Item b) { return rank(a) < rank(b); });
    return members;
}
// An item's name with its color taken off: "minecraft:black_harness" is "harness", colored; a variant by aux
// ("minecraft:bed@14") is its name, colored where the aux is not 0.
struct ColorName {
    std::string base;
    bool colored = false;
};
inline ColorName colorName(std::string_view key) {
    auto const parts = splitItemKey(key);
    std::string_view name = parts.name;
    if (auto colon = name.find(':'); colon != std::string_view::npos) name = name.substr(colon + 1);
    // The longer names first, so "light_blue_" is not taken for "blue_".
    static constexpr std::array<std::string_view, 16> colors{"light_blue_", "light_gray_", "magenta_", "orange_", "yellow_", "purple_", "white_", "brown_",
                                                             "black_",      "green_",      "lime_",    "pink_",   "gray_",   "cyan_",   "blue_",  "red_"};
    for (auto color : colors)
        if (name.starts_with(color)) return {std::string(name.substr(color.size())), true};
    return {std::string(name), parts.aux != 0 && parts.aux != anyAux};
}
// Re-dyeing: the recipe takes another color of the same thing (a black harness for a white one, a red bed for a
// white one). From a thing with no color (terracotta, a candle) it is the main recipe, not a re-dye.
inline bool redyes(Index const& index, Recipe const& recipe) {
    auto const product = colorName(index.items[static_cast<size_t>(recipe.makes)]);
    auto other = [&](Item item) {
        if (item == recipe.makes) return false;
        auto const name = colorName(index.items[static_cast<size_t>(item)]);
        return name.colored && name.base == product.base;
    };
    for (auto const& part : recipe.uses) {
        if (!part.group) { if (other(part.id)) return true; continue; }
        auto const& members = index.groups[static_cast<size_t>(part.id)].members;
        if (std::any_of(members.begin(), members.end(), other)) return true;
    }
    return false;
}
// An item's recipes as How to make lists them: the one `rule` uses first, the rest in the rule's order (station,
// then the game's priority: `before`); re-dyeing apart, folded under one line.
struct HowToRecipes {
    std::vector<int> main, redye;
};
inline HowToRecipes howToRecipes(Index const& index, Item item, int rule) {
    HowToRecipes out;
    for (int number : recipeOptions(index, item)) (redyes(index, index.recipes[static_cast<size_t>(number)]) ? out.redye : out.main).push_back(number);
    auto order = [&](int a, int b) {
        if ((a == rule) != (b == rule)) return a == rule;
        return before(index, index.recipes[static_cast<size_t>(a)], index.recipes[static_cast<size_t>(b)]);
    };
    std::stable_sort(out.main.begin(), out.main.end(), order);
    std::stable_sort(out.redye.begin(), out.redye.end(), order);
    return out;
}
}
