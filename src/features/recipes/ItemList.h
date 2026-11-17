#pragma once
#include <string>
// L-359: every item in a band right of the game's window on the item
// screens, in pages, with the name under the pointer; a press shows the
// item's recipes, a right press what it is used in (RecipeViewDraw.h).
// Settings::Inventory::recipes, off by default.
namespace lamium::recipes::itemList {
void start();
void stop();
// Input over the list, and all of it while the recipe view is open: true
// keeps the event from the game. Called from the input listeners, off the
// client thread; what was kept acts in the next frame. Nothing is kept once
// the list is not drawn, and a release only when its press was.
bool wheel(int pages);
bool press(int button, bool left, bool right, bool down);
bool key(int code, bool down);
// The search field: whether it is being typed in (keys are text then, not actions), and the text the
// game hands a focused field; true keeps it from the screen under the list.
bool typing();
bool type(std::string const& text);
// Whether the recipe view is open and drawn, for the key listener.
bool viewOpen();
// The recipe keys: shows the item under the pointer (in the list, the view
// or a slot of the game's), its recipes or what it is used in. Client thread.
void show(bool uses);
// The back key: the item the view showed before.
void back();
// The game's window lost the focus: releases that were on their way will not come, so nothing stays
// kept, and the search field gives up the keyboard. Any thread.
void focusLost();
// The favourite and wanted keys: the item under the pointer goes in, or out when it is in.
void favourite();
void want();
}
