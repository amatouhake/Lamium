#pragma once
#include "features/recipes/RawView.h"
#include "features/recipes/RecipeIndex.h"
#include <string>
#include <vector>

// L-359: names the recipe screens share. Client thread only (they ask the game for item names).
namespace lamium::recipes {
// A station by the name of its block's item, so it reads as the game names it; "-" for one without.
std::string stationName(Station station);
// A kind of work with the stations that do it: "Cooking (Furnace, Smoker)" (L-376).
std::string kindName(Kind kind, std::vector<Station> const& stations);
// An ingredient any of several items satisfies: a tag by its translated name with "(any)", any other
// group by its first member "and N others".
// A group by its name alone: a known tag's ("Planks"), else its first member's and "etc."; the Recipes screens
// mark its icon (anyMark) to say any member will do.
std::string groupName(Index const& index, int group);
// The same with "(any)" after it, where no icon carries the mark (Schematics' text).
std::string groupNameAny(Index const& index, int group);
// What a step of a plan takes in all its crafts, a line each: the count, then the name.
std::vector<std::string> stepInputLines(Index const& index, Request const& request, StepLine const& step);
}
