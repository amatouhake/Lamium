#pragma once
#include "features/schematic/Placement.h"
#include <algorithm>

// L-236: which layers of a ghost section a layer view shows, as a value two
// views can be compared by. Two views with equal values show the same cells
// of that section, so the renderer keeps its mesh when the cut moves; a
// section whose value changed is built again.
namespace lamium::schematic {
struct SectionLayers {
    enum class State : std::uint8_t { Hidden, Whole, Part };
    State state = State::Whole;
    // Only for Part: the axis and the shown range, in placed offsets.
    LayerAxis axis = LayerAxis::Y;
    int low = 0, high = 0;
    bool operator==(SectionLayers const&) const = default;
};
// `first`..`last`: the section's cells along each axis in placed offsets,
// already clipped to the placed box. A mesh that reads neighbours for face
// culling must pass the range widened by that halo, since a neighbour layer
// turning shown or hidden changes its faces too.
inline SectionLayers sectionLayers(PlacedSlab const& slab, Point first, Point last) {
    if (slab.all) return {};
    int const from = axisValue(first, slab.axis), to = axisValue(last, slab.axis);
    if (from > to) return {SectionLayers::State::Hidden};
    int const low = std::max(slab.low, from), high = std::min(slab.high, to);
    if (low > high) return {SectionLayers::State::Hidden};
    if (low == from && high == to) return {};
    return {SectionLayers::State::Part, slab.axis, low, high};
}
// A section's value as the renderer records and compares it: its cells from
// `sectionLow` (world) `size` a side, widened by `halo` on every side and
// clipped to the placed box at `origin`, in placed offsets.
inline SectionLayers sectionLayersAround(PlacedSlab const& slab, Size placed, Point origin, Point sectionLow, int size, int halo) {
    Point first{std::max(sectionLow.x - halo - origin.x, 0), std::max(sectionLow.y - halo - origin.y, 0),
                std::max(sectionLow.z - halo - origin.z, 0)};
    Point last{std::min(sectionLow.x + size - 1 + halo - origin.x, placed.x - 1),
               std::min(sectionLow.y + size - 1 + halo - origin.y, placed.y - 1),
               std::min(sectionLow.z + size - 1 + halo - origin.z, placed.z - 1)};
    return sectionLayers(slab, first, last);
}
}
