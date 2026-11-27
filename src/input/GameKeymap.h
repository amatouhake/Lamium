#pragma once
#include "input/Binding.h"
#include <vector>

class IClientInstance;

// The game's own keyboard controls as its keymap has them (L-372, L-412).
// Client thread.
namespace lamium::input {
// The keys the game binds to one of its actions (a keymap name such as "key.drop"); empty when unreadable.
std::vector<int> gameKeymapKeys(IClientInstance& client, char const* name);
// Every entry of gameKeys, read again at most once a second: key cells and key presses both ask.
GameKeyCodes const& gameKeyCodes(IClientInstance& client);
}
