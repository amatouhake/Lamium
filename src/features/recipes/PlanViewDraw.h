#pragma once
#include "features/recipes/RecipeState.h"
#include "features/recipes/RecipeViewDraw.h"
#include <string>
class LocalPlayer;
class MinecraftUIRenderContext;

// L-359 step 6: the window of what is wanted: the list the player fills, what
// it takes to gather and what to make, in that order as tabs. It sits where
// the recipe view does and is hosted the same way; the recipe view opens over
// it and gives it back when closed. Client thread only.
namespace lamium::recipes::planView {
enum class Tab { Wanted, Gather, Make, HowTo, Places };
bool isOpen();
void open(Tab tab);
// Opens on "How to make" with the line of `key` (an item's key, or a group's: groupKey) open and lit (L-370).
void openHowTo(std::string const& key);
void close();
// A line of "to gather" under the pointer, the same in the window and on the band (L-369): its name (a group's
// members as icons), held and needed, what is left in chests and stacks, a group's choice, and where counted
// stores hold some. `key`: a wanted item the recipe table does not know, else empty.
void gatherTip(view::Late& late, state::Plan const& plan, GatherLine const& line, std::string const& key);
// `shift`: a stepper moves by 16. `showItem` is set to the item whose name was pressed, for the recipe
// view; `showChoice` with it when that was in the list of what to make.
void draw(MinecraftUIRenderContext& context, view::Input const& input, view::Late& late, LocalPlayer& player, bool shift,
          std::string& showItem, bool& showChoice);
}
