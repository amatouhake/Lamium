#pragma once
#include "features/recipes/ItemListLayout.h"
#include "features/recipes/PlanViewDraw.h"
#include "features/recipes/RecipePlaces.h"
#include "features/recipes/RecipeState.h"
#include "features/recipes/RecipeViewDraw.h"
#include <functional>
#include <optional>
#include <string>
#include <vector>
class LocalPlayer;
class MinecraftUIRenderContext;

// L-374: the pages of the bands beside an item screen that are not the item list, each drawn into its
// own panel (BandLayout.h): the favorites, the plan's short list, and the open container's line at the
// left region's end. `ItemList.cpp` hosts them: it owns the input and the frame. Client thread only.
namespace lamium::recipes::bandPages {
// What a page needs of the host's frame.
struct Frame {
    MinecraftUIRenderContext& context;
    float px = -1, py = -1;
    bool leftPress = false;
    bool& pressed;          // a press no control has taken yet
    view::Late& late;
    std::string& hoverItem; // the item under the pointer, for the keys
    std::function<void(std::string const&, float, float, bool)> mark; // a cell's favorite frame and wanted count
    std::function<void(std::string const&)> show;                     // opens the recipe view on an item
    int sideTurns = 0;      // wheel notches over the plan's list
    bool shift = false;     // a stepper moves to the next stack
    bool held = false;      // the left button is down: a scrollbar is dragged
    int favoriteTurns = 0;  // wheel notches over the favorites
    bool took(ListBox const& box) {
        if (!pressed || !box.has(px, py)) return false;
        pressed = false;
        return true;
    }
};
// The panel's ground; `rule`: a line at its top, where it follows another panel of its region.
void ground(Frame& frame, band::Box box, bool rule);
void favorites(Frame& frame, SideLayout const& panel);
// `openPlan`: the window on a tab, in front of the recipe view.
void plan(Frame& frame, SideLayout const& panel, LocalPlayer& player, std::vector<Handed> const& handed,
          std::function<void(planView::Tab)> const& openPlan);
// The open container's switch: the last line of the left region (`region`, including the line's units).
void storeLine(Frame& frame, band::Box region, placesGame::Open const& open);
// The plan's page turns to the wanted items (after the wanted key), so what went in or out is seen.
void showWanted();
}
