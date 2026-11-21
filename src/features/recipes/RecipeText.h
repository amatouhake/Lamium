#pragma once
#include "features/recipes/RawView.h"
#include "features/recipes/RecipeIndex.h"
#include <string>
#include <vector>

// L-359: names the recipe screens share. Client thread only (they ask the game for item names).
namespace lamium::recipes {
// A station by the name of its block's item, so it reads as the game names it; "-" for one without.
std::string stationName(Station station);
// A process's name: "Smelting" (L-376); its stations are shown as their blocks, not named here.
std::string kindName(Kind kind);
// A group by its name alone: a known tag's ("Planks"), else its first member's and "etc."; the Recipes screens
// mark its icon (anyMark) to say any member will do.
std::string groupName(Index const& index, int group);
// What a step of a plan takes in all its crafts, a line each: the count, then the name.
std::vector<std::string> stepInputLines(Index const& index, Request const& request, StepLine const& step);
// The same with what `stock` holds of each, "held/needed", and how many more where it is short (L-370).
std::vector<std::string> stepInputLines(Index const& index, Request const& request, StepLine const& step, std::map<Item, Count> const& stock);
}
