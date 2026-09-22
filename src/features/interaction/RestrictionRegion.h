#pragma once
#include "overlay/Geometry.h"
#include "features/interaction/RestrictionMode.h"
#include <optional>
namespace lamium::interaction {
enum class Axis { X, Y, Z };
inline Axis normalAxis(overlay::Face face) {
    switch (face) {
    case overlay::Face::West: case overlay::Face::East: return Axis::X;
    case overlay::Face::Down: case overlay::Face::Up: return Axis::Y;
    case overlay::Face::North: case overlay::Face::South: return Axis::Z;
    }
    throw std::invalid_argument("Unknown restriction face");
}
struct RestrictionRegion {
    RestrictionMode mode;
    overlay::Cell anchor;
    Axis axis;
    bool operator==(RestrictionRegion const&) const = default;
    bool contains(overlay::Cell cell) const {
        bool x = cell.x == anchor.x, y = cell.y == anchor.y, z = cell.z == anchor.z;
        switch (mode) {
        case RestrictionMode::Layer: return y;
        case RestrictionMode::Column: return x && z;
        case RestrictionMode::Plane:
            switch (axis) { case Axis::X: return x; case Axis::Y: return y; case Axis::Z: return z; }
            break;
        case RestrictionMode::Line:
            switch (axis) { case Axis::X: return y && z; case Axis::Y: return x && z; case Axis::Z: return x && y; }
            break;
        }
        return false;
    }
    // Bounded preview only. The actual restriction is not range-limited.
    // Both consumers use contains(), so the displayed cells match enforcement.
    std::set<overlay::Cell> preview(int radius) const {
        if (radius < 0 || radius > 16) throw std::invalid_argument("Restriction preview radius must be 0..16");
        std::set<overlay::Cell> result;
        for (int dx=-radius;dx<=radius;++dx) for (int dy=-radius;dy<=radius;++dy) for (int dz=-radius;dz<=radius;++dz) {
            auto x = int64_t(anchor.x)+dx, y = int64_t(anchor.y)+dy, z = int64_t(anchor.z)+dz;
            constexpr auto low = int64_t(std::numeric_limits<int>::min())+1;
            constexpr auto high = int64_t(std::numeric_limits<int>::max())-1;
            if (x < low || x > high || y < low || y > high || z < low || z > high) continue;
            overlay::Cell cell{static_cast<int>(x),static_cast<int>(y),static_cast<int>(z)};
            if (contains(cell)) result.insert(cell);
        }
        return result;
    }
};
}
