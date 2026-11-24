#pragma once
#include "features/recipes/RecipeIndex.h"
#include <string>

class ItemStackBase;

namespace lamium::recipes {
// L-363: a game stack's key (ItemKey.h): its name, its aux where it is neither 0 nor "any", and the data that
// makes it what it is where reading item data is confirmed for this version (else none, as before). Empty for an
// empty stack or a name too long to be real. Client thread.
std::string stackKey(ItemStackBase const& stack);
// The table's item for a game stack, by its key: a firework rocket by its explosions, the ominous banner none.
inline Item itemOf(Index const& index, ItemStackBase const& stack) { return index.findKey(stackKey(stack)); }
}
