#pragma once
#include "features/recipes/RecipeState.h"
#include "features/recipes/RecipeViewDraw.h"
#include <functional>
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
// Opens on the tab shown last (the bottom row's button, L-382).
void openLast();
void close();
// A line of "to gather" under the pointer, the same in the window and on the band (L-369): what its row does not
// show (2026-11-22): its name (a group's members as icons), a group's choice, and where counted stores hold some. `key`: a wanted item the recipe table does not know, else empty.
void gatherTip(view::Late& late, state::Plan const& plan, GatherLine const& line, std::string const& key);
// A step of "to make" under the pointer, the same in the window and on the panel: the product and the inputs short
// of what is carried, by how many; the counts are the row's (2026-11-22).
void stepTip(view::Late& late, state::Plan const& plan, StepLine const& step);
// A wanted row under the pointer, the same in the window and on the panel: whose a handed row is, and whether rows
// above took a share of what is held; the counts are the row's.
void wantedTip(view::Late& late, WantedEntry const& entry, bool sharedAbove);
// An item or a group of the plan as its picture's key: a group as the member it shows (GroupLook.h).
std::string planPicture(state::Plan const& plan, Item id);
// What the counted stores hold of `item`, with what is carried, as tip lines (L-359, L-385).
void storedLines(view::Late& late, state::Plan const& plan, Item item);
// A step's two buttons (L-385), 11 units each, 2 apart, from `x`: lay in (one round; Shift what is still needed, as
// much as the grid takes; Ctrl one round onto the pointer) and make what is still needed; greyed where the screen
// cannot, the tip saying why. `took` takes the frame's press over a box. True when a recipe was laid in.
bool stepButtons(MinecraftUIRenderContext& context, view::Late& late, view::Placer* placer, state::Plan const& plan, StepLine const& step, float x,
                 float y, float px, float py, std::function<bool(float, float, float, float)> const& took, bool ctrl, bool shift);
// `shift`: a stepper moves by 16. `showItem` is set to the item whose name was pressed, for the recipe
// view; `showChoice` with it when that was in the list of what to make.
void draw(MinecraftUIRenderContext& context, view::Input const& input, view::Late& late, LocalPlayer& player, bool shift,
          std::string& showItem, bool& showChoice);
}
