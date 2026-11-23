#pragma once
#include <string>

class ItemStackBase;

namespace lamium::recipes {
// L-363: a game stack's key (ItemKey.h): its name, its aux where it is neither 0 nor "any", and the data that
// makes it what it is where reading item data is confirmed for this version (else none, as before). Empty for an
// empty stack or a name too long to be real. Client thread.
std::string stackKey(ItemStackBase const& stack);
}
