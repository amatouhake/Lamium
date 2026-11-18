#pragma once
#include "features/recipes/RecipeViewDraw.h"
#include <string>
class LocalPlayer;
class MinecraftUIRenderContext;

// L-359 step 6: the window of what is wanted: the list the player fills, what
// it takes to gather and what to make, in that order as tabs. It sits where
// the recipe view does and is hosted the same way; the recipe view opens over
// it and gives it back when closed. Client thread only.
namespace lamium::recipes::planView {
enum class Tab { Wanted, Gather, Make };
bool isOpen();
void open(Tab tab);
void close();
// `shift`: a stepper moves by 16. `showItem` is set to the item whose name was pressed, for the recipe
// view; `showChoice` with it when that was in the list of what to make.
void draw(MinecraftUIRenderContext& context, view::Input const& input, view::Late& late, LocalPlayer& player, bool shift,
          std::string& showItem, bool& showChoice);
}
