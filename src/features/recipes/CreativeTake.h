#pragma once
#include "features/recipes/TakePress.h"
#include <string>
class ScreenView;
// L-360: an item of Lamium's lists taken in creative through the creative
// inventory's own calls. The crafting screens (the inventory, the crafting
// table) hold the creative list's collections; `recipe_search` holds one
// cell per creative entry once its model is refreshed with the game's search
// empty, found by the key the list uses. Behind the CreativeTake capability.
// The caller checks that the player is in creative. Client thread, in a frame.
namespace lamium::recipes::creativeTake {
// `stack`: as many as the item stacks to, else one. `pointer`: onto the pointer where it is empty, else into the
// inventory. Refusal::None when the game was asked.
take::Refusal take(ScreenView& screen, std::string const& key, bool stack, bool pointer);
// Whether the capability lets taking run at all (Shift + press falls back to the recipes without it).
bool available();
}
