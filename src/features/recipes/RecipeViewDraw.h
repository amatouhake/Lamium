#pragma once
#include "features/inspection/render/ItemIcon.h"
#include "features/recipes/PlacePlan.h"
#include <string>
#include <vector>
class MinecraftUIRenderContext;

// L-359 step 3: the recipe view, drawn by whatever hosts it (the item list
// beside a game screen today). It knows nothing of the screen under it: the
// host hands it the pointer and the input it kept from the game, and draws
// what must lie over the item icons afterwards. Client thread only.
namespace lamium::recipes::view {
// The screen's crafting grid, as the host reaches it: whether a recipe can be laid into it from what
// is carried, and doing so. `many`: as many as divide evenly.
struct Placer {
    virtual ~Placer() = default;
    virtual PlacePlan preview(Recipe const& recipe, bool many) const = 0;
    // False when nothing was done.
    virtual bool run(int recipe, bool many) = 0;
};
struct Input {
    float screenWidth = 0, screenHeight = 0, pointerX = -1, pointerY = -1;
    std::vector<bool> clicks; // presses since the last frame; true: the left button
    int wheel = 0;            // pages to turn
    bool back = false, close = false;
    bool shift = false;       // held: "place" lays in as many as divide evenly
    bool held = false;        // the left button is down: a scrollbar that was pressed is dragged
    bool plan = true;         // the plan is in use: the item can be wanted
    bool favourites = true;   // favourites are in use: the item can be kept as one
    Placer* placer = nullptr; // null where the screen has no crafting grid
    // The span between the two bands. The window's grey covers all of it, so a game window wider than
    // the view (its recipe book out) does not show at the view's sides; what the view shows keeps its place.
    float coverLeft = 0, coverRight = 0;
};
// Drawn by the host after its own: item icons, then counts and the name under the pointer on top of them.
struct Late {
    struct Count { float x, y; std::string text; };
    std::vector<inspection::render::IconAt> icons;
    std::vector<Count> counts;
    std::string tip;
    std::vector<std::string> tipLines; // further lines under `tip`
    std::string hovered; // the item under the pointer, by its id
    bool placed = false; // a recipe was laid into the grid: the host clears the windows off it
};
bool isOpen();
// Shows an item's recipes, or what it is used in; the item shown before stays one step back.
// `choose`: asked from the list of what to make, where an item with several recipes says which is
// in use and offers the others.
void open(std::string item, bool uses, bool choose = false);
void close();
void draw(MinecraftUIRenderContext& context, Input const& input, Late& late);
}
