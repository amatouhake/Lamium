#pragma once
#include "features/schematic/Structure.h"
#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>

// Where a schematic sits in the world and which part of it is shown
// (BACKLOG L-93). Positions only; block states are turned by the game's own
// transform in the render/verify glue.
namespace lamium::schematic {
struct Point {
    int x = 0, y = 0, z = 0;
    bool operator==(Point const&) const = default;
};
// X flips east-west, Z flips north-south. Applied before rotation.
enum class Mirror : std::uint8_t { None, X, Z };
struct Placement {
    Point origin;      // lower north-west corner of the placed (turned) box
    int rotation = 0;  // clockwise quarter turns seen from above, 0-3
    Mirror mirror = Mirror::None;
};

inline int quarterTurns(int rotation) { return ((rotation % 4) + 4) % 4; }
inline Size placedSize(Size size, int rotation) {
    return quarterTurns(rotation) % 2 ? Size{size.z, size.y, size.x} : size;
}
// The placed box seen from above, in world blocks: [x0, x1) by [z0, z1).
struct Footprint {
    int x0 = 0, z0 = 0, x1 = 0, z1 = 0;
    bool operator==(Footprint const&) const = default;
};
inline Footprint footprint(Size size, Placement const& placement) {
    auto placed = placedSize(size, placement.rotation);
    return {placement.origin.x, placement.origin.z, placement.origin.x + placed.x, placement.origin.z + placed.z};
}
// Structure-local cell -> world cell.
inline Point toWorld(Size size, Placement const& placement, Point local) {
    int x = local.x, z = local.z;
    if (placement.mirror == Mirror::X) x = size.x - 1 - x;
    if (placement.mirror == Mirror::Z) z = size.z - 1 - z;
    // One clockwise turn takes east to south: (x, z) -> (depth-1-z, x).
    int width = size.x, depth = size.z;
    for (int i = 0; i < quarterTurns(placement.rotation); ++i) {
        int turnedX = depth - 1 - z, turnedZ = x;
        x = turnedX; z = turnedZ;
        std::swap(width, depth);
    }
    return {placement.origin.x + x, placement.origin.y + local.y, placement.origin.z + z};
}
// A free position in structure space (an entity's) -> world, turned and
// mirrored like cells: a cell's center lands on the center of its world cell.
struct Position {
    double x = 0, y = 0, z = 0;
    bool operator==(Position const&) const = default;
};
inline Position toWorldPosition(Size size, Placement const& placement, Position local) {
    double x = local.x, z = local.z;
    if (placement.mirror == Mirror::X) x = size.x - x;
    if (placement.mirror == Mirror::Z) z = size.z - z;
    double width = size.x, depth = size.z;
    for (int i = 0; i < quarterTurns(placement.rotation); ++i) {
        double turnedX = depth - z, turnedZ = x;
        x = turnedX; z = turnedZ;
        std::swap(width, depth);
    }
    return {placement.origin.x + x, placement.origin.y + local.y, placement.origin.z + z};
}
// An entity's facing (yaw in degrees: 0 faces south, 90 west) after the same
// mirror and turns as its position. Mirrored as the game's structure block
// does: on the other axis, which is a true mirror turned half round (seen
// in game: a south-facing armor stand faces north under the X mirror).
inline float toWorldYaw(float yaw, Placement const& placement) {
    if (placement.mirror == Mirror::X) yaw = 180 - yaw;
    if (placement.mirror == Mirror::Z) yaw = -yaw;
    yaw += 90.f * quarterTurns(placement.rotation);
    while (yaw >= 180) yaw -= 360;
    while (yaw < -180) yaw += 360;
    return yaw;
}
// A block face (the game's order: down, up, north, south, west, east) after
// the same mirror and turns as cells, for a direction kept in block entity
// data rather than in block states (a shulker box's opening, L-179).
inline int toWorldFace(int face, Placement const& placement) {
    if (face < 2 || face > 5) return face;
    if (placement.mirror == Mirror::X && face >= 4) face = 9 - face;
    if (placement.mirror == Mirror::Z && face <= 3) face = 5 - face;
    constexpr int clockwise[6] = {0, 1, 5, 4, 2, 3}; // north -> east -> south -> west
    for (int i = 0; i < quarterTurns(placement.rotation); ++i) face = clockwise[face];
    return face;
}
// The placement transform without bounds: a placed-box offset (world - origin)
// to a structure-local offset. Accepts points outside the box, which a cut
// plane may need while the view is outside.
inline Point placedToLocal(Size structure, Placement const& placement, Point placed) {
    int x = placed.x, z = placed.z;
    int width = placedSize(structure, placement.rotation).x, depth = placedSize(structure, placement.rotation).z;
    for (int i = 0; i < quarterTurns(placement.rotation); ++i) {
        // Inverse of one clockwise turn: (x, z) -> (z, width-1-x).
        int localX = z, localZ = width - 1 - x;
        x = localX; z = localZ;
        std::swap(width, depth);
    }
    if (placement.mirror == Mirror::X) x = structure.x - 1 - x;
    if (placement.mirror == Mirror::Z) z = structure.z - 1 - z;
    return {x, placed.y, z};
}
// World cell -> structure-local cell, or nullopt outside the placed box.
inline std::optional<Point> toLocal(Size size, Placement const& placement, Point world) {
    Size placed = placedSize(size, placement.rotation);
    int x = world.x - placement.origin.x, y = world.y - placement.origin.y, z = world.z - placement.origin.z;
    if (x < 0 || y < 0 || z < 0 || x >= placed.x || y >= placed.y || z >= placed.z) return std::nullopt;
    return placedToLocal(size, placement, {x, y, z});
}

// The cut plane is attached to the structure: `cut` is a structure-local
// offset from the low corner along one of the structure's own axes, so turning
// or mirroring a placement keeps the same build cells. The old six-direction
// fields remain for older readers and for files written before `layerView`.
enum class LayerAxis : std::uint8_t { Y, X, Z };
enum class LayerKeep : std::uint8_t { Low, High };
enum class LayerMode : std::uint8_t { All, Only, UpTo };
// The old model's direction: which side the index counted from.
enum class LegacyLayerAxis : std::uint8_t {
    UpFromBottom, DownFromTop, EastFromWest, WestFromEast, SouthFromNorth, NorthFromSouth
};
struct Layers {
    LayerAxis axis = LayerAxis::Y;
    LayerMode mode = LayerMode::All;
    LayerKeep keep = LayerKeep::Low;
    int cut = 0;          // 0 = the low layer along the structure axis
    bool legacy = false;  // still the old six-direction form; needs the placement
    LegacyLayerAxis legacyAxis = LegacyLayerAxis::UpFromBottom;
    LayerMode legacyMode = LayerMode::All;
    int legacyIndex = 0;
    LayerMode lastShown = LayerMode::Only; // world-session memory, not saved
    bool operator==(Layers const&) const = default;
};
inline void noteLayerEdit(Layers& layers) {
    if (layers.mode == LayerMode::Only || layers.mode == LayerMode::UpTo) layers.lastShown = layers.mode;
}
inline bool leaveAll(Layers& layers) {
    if (layers.mode != LayerMode::All) return false;
    layers.mode = layers.lastShown;
    return true;
}
inline int layerCount(Size structure, LayerAxis axis) {
    switch (axis) {
    case LayerAxis::Y: return structure.y;
    case LayerAxis::X: return structure.x;
    default: return structure.z;
    }
}
// A cell's coordinate along an axis, in structure-local space.
inline int axisValue(Point local, LayerAxis axis) {
    switch (axis) {
    case LayerAxis::Y: return local.y;
    case LayerAxis::X: return local.x;
    default: return local.z;
    }
}
// The same coordinate as a point (other axes 0), for mapping a plane.
inline Point axisPoint(LayerAxis axis, int value) {
    switch (axis) {
    case LayerAxis::Y: return {0, value, 0};
    case LayerAxis::X: return {value, 0, 0};
    default: return {0, 0, value};
    }
}
inline LayerAxis layerAxisOf(LegacyLayerAxis axis) {
    switch (axis) {
    case LegacyLayerAxis::UpFromBottom: case LegacyLayerAxis::DownFromTop: return LayerAxis::Y;
    case LegacyLayerAxis::EastFromWest: case LegacyLayerAxis::WestFromEast: return LayerAxis::X;
    default: return LayerAxis::Z;
    }
}
inline bool legacyHighSide(LegacyLayerAxis axis) {
    return axis == LegacyLayerAxis::DownFromTop || axis == LegacyLayerAxis::WestFromEast
        || axis == LegacyLayerAxis::NorthFromSouth;
}
inline LegacyLayerAxis lowLegacyAxis(LayerAxis axis) {
    return axis == LayerAxis::Y ? LegacyLayerAxis::UpFromBottom
        : axis == LayerAxis::X ? LegacyLayerAxis::EastFromWest : LegacyLayerAxis::SouthFromNorth;
}
inline LegacyLayerAxis highLegacyAxis(LayerAxis axis) {
    return axis == LayerAxis::Y ? LegacyLayerAxis::DownFromTop
        : axis == LayerAxis::X ? LegacyLayerAxis::WestFromEast : LegacyLayerAxis::NorthFromSouth;
}
// The inverse of placedToLocal, also without bounds.
inline Point localToPlaced(Size structure, Placement const& placement, Point local) {
    int x = local.x, z = local.z;
    if (placement.mirror == Mirror::X) x = structure.x - 1 - x;
    if (placement.mirror == Mirror::Z) z = structure.z - 1 - z;
    int width = structure.x, depth = structure.z;
    for (int i = 0; i < quarterTurns(placement.rotation); ++i) {
        int turnedX = depth - 1 - z, turnedZ = x;
        x = turnedX; z = turnedZ;
        std::swap(width, depth);
    }
    return {x, local.y, z};
}
// Which structure axis a placed-box axis maps to, and the sign of increasing
// coordinates; used to translate the old side-based fields.
struct AxisMap {
    LayerAxis axis = LayerAxis::Y;
    int sign = 1;
};
// The axis along which two images of neighboring cells differ.
inline AxisMap axisStep(Point a, Point b) {
    int dx = b.x - a.x, dy = b.y - a.y, dz = b.z - a.z;
    if (dx) return {LayerAxis::X, dx};
    if (dy) return {LayerAxis::Y, dy};
    return {LayerAxis::Z, dz};
}
inline AxisMap placedToLocalAxis(LayerAxis placedAxis, Size structure, Placement const& placement) {
    return axisStep(placedToLocal(structure, placement, axisPoint(placedAxis, 0)),
        placedToLocal(structure, placement, axisPoint(placedAxis, 1)));
}
inline AxisMap localToPlacedAxis(LayerAxis localAxis, Size structure, Placement const& placement) {
    return axisStep(localToPlaced(structure, placement, axisPoint(localAxis, 0)),
        localToPlaced(structure, placement, axisPoint(localAxis, 1)));
}
// The old reader's fields for the current cut plane, with the current
// placement's rotation and mirror.
inline void dualWrite(Layers& layers, Size structure, Placement const& placement) {
    int count = std::max(1, layerCount(structure, layers.axis));
    layers.cut = std::clamp(layers.cut, 0, count - 1);
    if (layers.mode == LayerMode::All) {
        layers.legacyAxis = LegacyLayerAxis::UpFromBottom;
        layers.legacyMode = LayerMode::All;
        layers.legacyIndex = 0;
        return;
    }
    AxisMap map = localToPlacedAxis(layers.axis, structure, placement);
    Size placed = placedSize(structure, placement.rotation);
    int extent = std::max(1, layerCount(placed, map.axis));
    Point placedPlane = localToPlaced(structure, placement, axisPoint(layers.axis, layers.cut));
    int p = std::clamp(axisValue(placedPlane, map.axis), 0, extent - 1);
    bool localLow = layers.keep == LayerKeep::Low;
    bool placedLow = map.sign > 0 ? localLow : !localLow;
    layers.legacyAxis = placedLow ? lowLegacyAxis(map.axis) : highLegacyAxis(map.axis);
    layers.legacyIndex = placedLow ? p : extent - 1 - p;
    layers.legacyMode = layers.mode;
}
// The old six-direction view as a structure-local cut plane. The old fields
// are placed-box based; the plane is mapped through the current placement.
inline Layers fromLegacy(Layers const& old, Size structure, Placement const& placement) {
    Layers out;
    out.legacy = false;
    out.legacyAxis = old.legacyAxis;
    out.legacyMode = old.legacyMode;
    out.legacyIndex = old.legacyIndex;
    out.mode = old.legacyMode;
    if (out.mode == LayerMode::All) {
        out.axis = LayerAxis::Y;
        out.cut = 0;
        out.keep = LayerKeep::Low;
        dualWrite(out, structure, placement);
        return out;
    }
    LayerAxis placedAxis = layerAxisOf(old.legacyAxis);
    Size placed = placedSize(structure, placement.rotation);
    int extent = std::max(1, layerCount(placed, placedAxis));
    int index = std::clamp(old.legacyIndex, 0, extent - 1);
    bool oldHigh = legacyHighSide(old.legacyAxis);
    int p = oldHigh ? extent - 1 - index : index;
    AxisMap map = placedToLocalAxis(placedAxis, structure, placement);
    Point localPlane = placedToLocal(structure, placement, axisPoint(placedAxis, p));
    out.axis = map.axis;
    int count = std::max(1, layerCount(structure, map.axis));
    out.cut = std::clamp(axisValue(localPlane, map.axis), 0, count - 1);
    bool localLow = oldHigh ? map.sign < 0 : map.sign > 0;
    out.keep = localLow ? LayerKeep::Low : LayerKeep::High;
    dualWrite(out, structure, placement);
    return out;
}
// A world position's structure-local cut along the axis, clamped.
inline int cutAlong(Size structure, Placement const& placement, LayerAxis axis, Point world) {
    Point placed{world.x - placement.origin.x, world.y - placement.origin.y, world.z - placement.origin.z};
    Point local = placedToLocal(structure, placement, placed);
    return std::clamp(axisValue(local, axis), 0, std::max(0, layerCount(structure, axis) - 1));
}
inline Layers withAxis(Layers layers, Size structure, LayerAxis next) {
    layers.axis = next;
    layers.cut = std::clamp(layers.cut, 0, std::max(0, layerCount(structure, next) - 1));
    return layers;
}
// The view as a cut plane: converted from the legacy form, cut clamped.
inline Layers resolvedLayers(Layers const& layers, Size structure, Placement const& placement) {
    Layers view = layers.legacy ? fromLegacy(layers, structure, placement) : layers;
    view.cut = std::clamp(view.cut, 0, std::max(0, layerCount(structure, view.axis) - 1));
    return view;
}
// `placedOffset` is the cell's world position minus the placement origin and
// `local` the same cell in structure space (callers that already ran toLocal
// pass it). A legacy view is still read the old way; a new view is cut in
// structure space, so turning the placement does not change the shown cells.
inline bool layerShown(Layers const& layers, Size structure, Placement const& placement, Point placedOffset, Point local) {
    if (layers.legacy) {
        if (layers.legacyMode == LayerMode::All) return true;
        Size placed = placedSize(structure, placement.rotation);
        LayerAxis axis = layerAxisOf(layers.legacyAxis);
        int layer = legacyHighSide(layers.legacyAxis)
            ? layerCount(placed, axis) - 1 - axisValue(placedOffset, axis)
            : axisValue(placedOffset, axis);
        return layers.legacyMode == LayerMode::Only ? layer == layers.legacyIndex : layer <= layers.legacyIndex;
    }
    if (layers.mode == LayerMode::All) return true;
    int cut = std::clamp(layers.cut, 0, std::max(0, layerCount(structure, layers.axis) - 1));
    int at = axisValue(local, layers.axis);
    if (layers.mode == LayerMode::Only) return at == cut;
    return layers.keep == LayerKeep::Low ? at <= cut : at >= cut;
}
inline bool layerShown(Layers const& layers, Size structure, Placement const& placement, Point placedOffset) {
    bool cutInStructure = !layers.legacy && layers.mode != LayerMode::All;
    return layerShown(layers, structure, placement, placedOffset,
        cutInStructure ? placedToLocal(structure, placement, placedOffset) : placedOffset);
}
// Whether the view shows every layer, in either form; needs no structure.
inline bool showsAllLayers(Layers const& layers) {
    return (layers.legacy ? layers.legacyMode : layers.mode) == LayerMode::All;
}
// A set cut plane must be rewritten for the new turn, which needs the size.
inline bool turnNeedsStructure(Layers const& layers) { return !layers.legacy && layers.mode != LayerMode::All; }
// The shown cells as a slab of the placed box, resolved once per placement so
// a scan can reject hidden cells before mapping them into the structure.
struct PlacedSlab {
    bool all = true;
    LayerAxis axis = LayerAxis::Y;
    int low = 0, high = 0;
    bool shown(Point placedOffset) const {
        if (all) return true;
        int at = axisValue(placedOffset, axis);
        return at >= low && at <= high;
    }
};
inline PlacedSlab placedSlab(Layers const& layers, Size structure, Placement const& placement) {
    PlacedSlab slab;
    if (showsAllLayers(layers)) return slab;
    slab.all = false;
    constexpr int below = std::numeric_limits<int>::min(), above = std::numeric_limits<int>::max();
    if (layers.legacy) {
        slab.axis = layerAxisOf(layers.legacyAxis);
        int extent = layerCount(placedSize(structure, placement.rotation), slab.axis);
        bool high = legacyHighSide(layers.legacyAxis);
        int p = high ? extent - 1 - layers.legacyIndex : layers.legacyIndex;
        if (layers.legacyMode == LayerMode::Only) slab.low = slab.high = p;
        else if (high) { slab.low = p; slab.high = above; }
        else { slab.low = below; slab.high = p; }
        return slab;
    }
    AxisMap map = localToPlacedAxis(layers.axis, structure, placement);
    int cut = std::clamp(layers.cut, 0, std::max(0, layerCount(structure, layers.axis) - 1));
    int p = axisValue(localToPlaced(structure, placement, axisPoint(layers.axis, cut)), map.axis);
    slab.axis = map.axis;
    bool placedLow = (layers.keep == LayerKeep::Low) == (map.sign > 0);
    if (layers.mode == LayerMode::Only) slab.low = slab.high = p;
    else if (placedLow) { slab.low = below; slab.high = p; }
    else { slab.low = p; slab.high = above; }
    return slab;
}
// Turns or mirrors a placement. With the structure known, a legacy view
// becomes a cut plane first so it stays on the same build cells, and the old
// fields are rewritten for the new turn. Without it a legacy view keeps its
// placed-box meaning; callers refuse to turn a set cut plane then.
template <class Turn>
void turnPlacement(Placement& placement, Layers& layers, Size const* structure, Turn&& turn) {
    if (structure && layers.legacy) layers = fromLegacy(layers, *structure, placement);
    turn(placement);
    if (structure && !layers.legacy) dualWrite(layers, *structure, placement);
}
// One edit of a placement's cut plane, as each control makes it (L-399): a
// legacy view becomes a cut plane first, `edit` changes it, the mode shown is
// remembered for leaving All (`note`; not when the edit is the mode itself)
// and the old reader's fields follow.
template <class Edit>
void editLayers(Layers& layers, Size structure, Placement const& placement, Edit&& edit, bool note = true) {
    if (layers.legacy) layers = fromLegacy(layers, structure, placement);
    edit(layers);
    if (note) noteLayerEdit(layers);
    dualWrite(layers, structure, placement);
}
}

