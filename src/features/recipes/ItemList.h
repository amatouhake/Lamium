#pragma once
// L-359 step 2: every item in a band right of the game's window on the item
// screens, in pages, with the name under the pointer. Not a setting yet: a
// list without the recipe view is not usable, so it is only in builds made
// with `xmake f --item_list=y`.
namespace lamium::recipes::itemList {
void start();
void stop();
// A wheel turn while the pointer is over the list: pages it and is kept
// from the game. Called from the mouse listener, off the client thread.
bool wheel(int pages);
}
