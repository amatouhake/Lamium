#pragma once
#include "features/schematic/Structure.h"
#include <algorithm>
#include <cstdint>
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
// World cell -> structure-local cell, or nullopt outside the placed box.
inline std::optional<Point> toLocal(Size size, Placement const& placement, Point world) {
    Size placed = placedSize(size, placement.rotation);
    int x = world.x - placement.origin.x, y = world.y - placement.origin.y, z = world.z - placement.origin.z;
    if (x < 0 || y < 0 || z < 0 || x >= placed.x || y >= placed.y || z >= placed.z) return std::nullopt;
    int width = placed.x, depth = placed.z;
    for (int i = 0; i < quarterTurns(placement.rotation); ++i) {
        // Inverse of one clockwise turn: (x, z) -> (z, width-1-x).
        int localX = z, localZ = width - 1 - x;
        x = localX; z = localZ;
        std::swap(width, depth);
    }
    if (placement.mirror == Mirror::X) x = size.x - 1 - x;
    if (placement.mirror == Mirror::Z) z = size.z - 1 - z;
    return Point{x, y, z};
}

// Which layers are shown, along a world axis of the placed box.
enum class LayerAxis : std::uint8_t { UpFromBottom, DownFromTop, EastFromWest, WestFromEast, SouthFromNorth, NorthFromSouth };
enum class LayerMode : std::uint8_t { All, Only, UpTo };
struct Layers {
    LayerAxis axis = LayerAxis::UpFromBottom;
    LayerMode mode = LayerMode::All;
    int index = 0; // 0 = the first layer from the axis' start side
    bool operator==(Layers const&) const = default;
};
inline int layerCount(Size placed, LayerAxis axis) {
    switch (axis) {
    case LayerAxis::UpFromBottom: case LayerAxis::DownFromTop: return placed.y;
    case LayerAxis::EastFromWest: case LayerAxis::WestFromEast: return placed.x;
    default: return placed.z;
    }
}
// `offset` is the cell's position inside the placed box.
inline int layerOf(Size placed, LayerAxis axis, Point offset) {
    switch (axis) {
    case LayerAxis::UpFromBottom: return offset.y;
    case LayerAxis::DownFromTop: return placed.y - 1 - offset.y;
    case LayerAxis::EastFromWest: return offset.x;
    case LayerAxis::WestFromEast: return placed.x - 1 - offset.x;
    case LayerAxis::SouthFromNorth: return offset.z;
    default: return placed.z - 1 - offset.z;
    }
}
// Changes the layer direction. Turning to the opposite side of the same axis
// (from below to from above, ...) keeps the same physical layer; another
// axis keeps the number, clamped to its count.
inline Layers withAxis(Layers layers, Size placed, LayerAxis next) {
    auto axisOf = [](LayerAxis a) { return static_cast<int>(a) / 2; };
    if (axisOf(layers.axis) == axisOf(next) && layers.axis != next)
        layers.index = layerCount(placed, next) - 1 - layers.index;
    layers.axis = next;
    layers.index = std::clamp(layers.index, 0, std::max(0, layerCount(placed, next) - 1));
    return layers;
}
inline bool layerShown(Layers const& layers, Size placed, Point offset) {
    if (layers.mode == LayerMode::All) return true;
    int layer = layerOf(placed, layers.axis, offset);
    return layers.mode == LayerMode::Only ? layer == layers.index : layer <= layers.index;
}
}
