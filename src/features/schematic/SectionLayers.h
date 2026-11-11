#pragma once
#include "features/schematic/Placement.h"
#include <algorithm>

// L-236 groundwork: which layers of a ghost section a layer view shows, as a
// value two views can be compared by. Two views with equal values show the
// same cells of that section, so its mesh can be kept when the cut moves.
// The renderer's cache does not use it yet.
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
}
