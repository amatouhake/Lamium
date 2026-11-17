#pragma once
#include "features/recipes/RecipeIndex.h"

class Level;

// Reads the client's recipe table into a Source, a bounded number of recipes
// per call, so a world's four to five thousand recipes do not land in one
// tick. Keeps no game pointer between calls: it remembers how far it got and
// starts over when the table's size changes. Call on the client tick only.
namespace lamium::recipes::game {
class Snapshot {
public:
    // True once the whole table is read (or cannot be read: then `failed()`).
    bool step(Level& level, int budget) noexcept;
    bool failed() const { return broken; }
    Source const& source() const { return read; }
    void reset();

private:
    Source read;
    long long size = -1;
    size_t done = 0;
    bool broken = false;
};
}
