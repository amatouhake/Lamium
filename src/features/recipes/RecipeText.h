#pragma once
#include "features/recipes/RecipeIndex.h"
#include <string>

// L-359: names the recipe screens share. Client thread only (they ask the game for item names).
namespace lamium::recipes {
// A station by the name of its block's item, so it reads as the game names it; "-" for one without.
std::string stationName(Station station);
// An ingredient any of several items satisfies: a tag by its translated name with "(any)", any other
// group by its first member "and N others".
std::string groupName(Index const& index, int group);
}
