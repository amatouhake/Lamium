#pragma once
#include "features/recipes/PlacePlan.h"
#include "features/recipes/RecipeService.h"
#include <map>
#include <memory>
#include <vector>
class ScreenView;

// L-359 step 9, L-165: a crafting recipe laid into the game's grid, and all of
// it crafted, through the slot calls Item transfer uses (take an amount onto
// the pointer, place it, Shift-take the output): the game sends its own
// requests and the server checks each. Behind the slot routes' capability.
// Client thread only; the screen is the focused item screen of this frame.
namespace lamium::recipes::place {
// The side of the screen's crafting grid, 2 or 3; 0 where it has none or the slot calls are not confirmed.
int gridSide(ScreenView& screen);
// What the player carries, slot by slot (0-8 the hotbar, 9-35 the inventory), as PlacePlan wants it.
std::vector<PlaceSource> carried(ScreenView& screen, Index const& index);
// Lays the recipe in now: one of each ingredient, or with `all` as many as divide evenly, then takes all
// the grid makes and goes on, a round every few frames, while ingredients are left outside the grid.
// False when nothing was done (something on the pointer, an ingredient missing, a grid that will not empty).
bool run(ScreenView& screen, std::shared_ptr<service::Table const> const& table, int recipe, std::map<int, Item> const& picks, bool all);
// The next round of a run, if one is going; called every frame the item screen is drawn.
void tick(ScreenView& screen);
// Ends a run: the screen went, the player pressed something, the window lost the focus.
void stop();
}
