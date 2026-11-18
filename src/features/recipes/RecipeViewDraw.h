#pragma once
#include "features/inspection/render/ItemIcon.h"
#include <string>
#include <vector>
class MinecraftUIRenderContext;

// L-359 step 3: the recipe view, drawn by whatever hosts it (the item list
// beside a game screen today). It knows nothing of the screen under it: the
// host hands it the pointer and the input it kept from the game, and draws
// what must lie over the item icons afterwards. Client thread only.
namespace lamium::recipes::view {
struct Input {
    float screenWidth = 0, screenHeight = 0, pointerX = -1, pointerY = -1;
    std::vector<bool> clicks; // presses since the last frame; true: the left button
    int wheel = 0;            // pages to turn
    bool back = false, close = false;
};
// Drawn by the host after its own: item icons, then counts and the name under the pointer on top of them.
struct Late {
    struct Count { float x, y; std::string text; };
    std::vector<inspection::render::IconAt> icons;
    std::vector<Count> counts;
    std::string tip;
    std::vector<std::string> tipLines; // further lines under `tip`
    std::string hovered; // the item under the pointer, by its id
};
bool isOpen();
// Shows an item's recipes, or what it is used in; the item shown before stays one step back.
// `choose`: asked from the list of what to make, where an item with several recipes says which is
// in use and offers the others.
void open(std::string item, bool uses, bool choose = false);
void close();
void draw(MinecraftUIRenderContext& context, Input const& input, Late& late);
}
