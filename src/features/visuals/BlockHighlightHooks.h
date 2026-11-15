#pragma once
// Block highlight glue (BACKLOG L-320): records where designated block
// types were drawn, for the glow overlay, and rebuilds chunks when the list
// or switch changes. Pure rules are in
// BlockHighlight.h.
#include "features/visuals/BlockHighlight.h"
#include <cstdint>
#include <functional>
#include <vector>
namespace lamium::visuals::highlight {
bool start();
void stop();
// Every mark recorded in `dimension`, copied into `out` when the marks changed
// since `revision` (which is then updated); false and `out` untouched otherwise.
bool marks(int dimension, std::uint64_t& revision, std::vector<Mark>& out);
// Drops every other dimension's sections, and the marks of `dimension` that
// `keep` rejects (the chunk left, or the block there is no longer designated).
// Client thread; `keep` runs without the build threads' lock held.
void pruneMarks(int dimension, std::function<bool(Mark const&)> const& keep);
void clearMarks();
// Set by the overlay when the nearest-faces limit left marks out; taken
// (and cleared) by the HUD to say so once.
void reportCapped(size_t shown);
size_t takeCapped();
}
