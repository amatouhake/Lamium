#pragma once
// L-359: every item in a band right of the game's window on the item
// screens, in pages, with the name under the pointer; a press shows the
// item's recipes, a right press what it is used in (RecipeViewDraw.h). Not a
// setting yet: only in builds made with `xmake f --item_list=y`.
namespace lamium::recipes::itemList {
void start();
void stop();
// Input over the list, and all of it while the recipe view is open: true
// keeps the event from the game. Called from the input listeners, off the
// client thread; what was kept acts in the next frame. Nothing is kept once
// the list has not been drawn for a moment.
bool wheel(int pages);
bool press(bool left, bool right, bool down);
bool key(int code, bool down);
}
