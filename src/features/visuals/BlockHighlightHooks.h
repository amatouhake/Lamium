#pragma once
// Block highlight glue (BACKLOG L-320): tints designated block types in the
// terrain mesh, records where they were drawn for the glow overlay, and
// rebuilds chunks when the list or switch changes. Pure rules are in
// BlockHighlight.h.
#include "features/visuals/BlockHighlight.h"
#include <cstdint>
#include <functional>
#include <vector>
namespace lamium::visuals::highlight {
bool start();
void stop();
// Every recorded mark, copied into `out` when they changed since `revision`
// (which is then updated); false and `out` untouched otherwise.
bool marks(std::uint64_t& revision, std::vector<Mark>& out);
// Drops each chunk section whose first mark `keep` rejects (its chunk left,
// or the block there is no longer designated: another dimension's leftovers).
void pruneMarks(std::function<bool(Mark const&)> const& keep);
void clearMarks();
}
