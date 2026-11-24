#pragma once
// L-360: how the game hands items out in creative and whether Lamium can do
// the same for an item of its lists. Read-only: the screen's take and place
// calls with the cell's item, the requests sent and their answers, and on
// every item screen the creative collections (size, items shown) whenever
// they change. Not read-only: F7, F8 and F9 take the item under the pointer
// in Lamium's lists; F10 tries to make the pane fill its search list,
// writing the crafting screen's tab fields with Shift or Ctrl (see the
// .cpp). Only with `xmake f --creative_trace=y`.
namespace lamium::researchTrace::creative {
void start();
void stop();
}
