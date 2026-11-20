#pragma once
#include "features/recipes/ListFilter.h"
#include "features/recipes/PlacePlan.h"
#include "features/recipes/RecipeService.h"
#include <map>
#include <memory>
#include <vector>
class ScreenView;

// L-359 step 9, L-165: a crafting recipe laid into the game's grid, and Mass
// Craft, through the slot calls Item transfer uses (take an amount onto the
// pointer, place it, Shift-take the output, drop a slot): the game sends its
// own requests and the server checks each. Behind the slot routes' capability.
// Client thread only; the screen is the focused item screen of this frame.
namespace lamium::recipes {
namespace place {
// The open screen as a place to lay a recipe in (L-380): the crafting grid, or a furnace, blast furnace, smoker,
// stonecutter or smithing table; none where the slot calls are not confirmed.
Bench bench(ScreenView& screen);
// The side of the screen's crafting grid, 2 or 3; 0 where it has none or the slot calls are not confirmed.
int gridSide(ScreenView& screen);
// What the player carries, slot by slot (0-8 the hotbar, 9-35 the inventory), as PlacePlan wants it, and
// what lies in the grid (from 100), which goes back to the inventory before a recipe is laid in.
std::vector<PlaceSource> carried(ScreenView& screen, Index const& index);
// Lays the recipe in now: one of each ingredient, or with `many` as many as divide evenly.
// What lay in the grid goes back first; the recipe is then laid in once the grid reads empty. At a station
// (bench) its slots take the cells, a stack at most; a slot holding another item is emptied first.
// False when nothing was done (something on the pointer, an ingredient missing).
bool run(ScreenView& screen, std::shared_ptr<service::Table const> const& table, int recipe, std::map<int, Item> const& picks, bool many);
// Lays a waiting recipe in, if there is one; called every frame the item screen is drawn.
void tick(ScreenView& screen);
// Gives a waiting recipe up: the screen went, the player pressed something, the window lost the focus.
// A round being crafted onto the pointer goes on.
void stop();
// L-379: what crafting one round of the recipe onto the pointer meets now; `ready`, the round can be laid from
// what is carried (PlacePlan, the grid counted in).
CraftOnto craftCheck(ScreenView& screen, Index const& index, Recipe const& recipe, bool ready);
// Crafts one round onto the pointer, over the next frames (tick), the grid emptied first; where the pointer
// cannot take it (CraftOnto::PlaceOnly) the round is laid in only. False when it cannot start (an ingredient
// missing, a run going on).
bool craft(ScreenView& screen, std::shared_ptr<service::Table const> const& table, int recipe, std::map<int, Item> const& picks);
// Forgets a round being crafted, without a game call: its screen or world is gone.
void abandonCraft();
}
// Mass Craft (L-165): while its key is held, the recipe of the item under the pointer (over the output
// slot: the one lying in the grid) is laid in again as many times as divide evenly, all of it taken and the product thrown out of the
// inventory, a round every 50 ms at most. A round that cannot be laid in waits for more to arrive; with no slot
// free, the stacks of an ingredient beyond its share go out (MassCraftPlan.h). One log line per run.
namespace massCraft {
void start();
void stop();
// The key went down or up (client thread). Up ends the run, as does losing the focus or the screen.
void setHeld(bool held);
}
}
