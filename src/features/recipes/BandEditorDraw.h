#pragma once
#include "features/recipes/BandLayout.h"
#include "features/recipes/RecipeViewDraw.h"
#include <array>
#include <optional>
#include <vector>
class MinecraftUIRenderContext;

// L-375: the bands' editor over an item screen (BandEditor.h has its rules). `ItemList.cpp` hosts it: while
// it is active the host draws the bands from `layout()` and hands every press and Esc here. Client thread.
namespace lamium::recipes::bandEditor {
bool active();
void start(band::Page page);
void stop();
// What the bands show this frame: the layout after the drop while a panel is dragged, else the saved one.
band::Layout layout(band::Layout const& saved);
struct Input {
    float px = -1, py = -1;
    bool press = false; // a press since the last frame
    bool held = false;  // the left button is down
    bool close = false; // Esc or the game's inventory key
};
// Takes the frame's input and draws the editor over the bands and the game's window (`window`).
void draw(MinecraftUIRenderContext& context, Input const& input, band::Layout const& saved, band::Regions const& regions,
          std::array<bool, band::pageCount> const& on, std::vector<band::Placed> const& placed, band::Box window, view::Late& late);
}
