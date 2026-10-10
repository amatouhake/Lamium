#pragma once
#include "overlay/Geometry.h"
#include <cmath>

// How world-space overlays stay in front of terrain (BACKLOG L-110). Every
// overlay that draws faces (Shapes, the breaking restriction, the light
// overlay, schematic ghosts) follows these rules:
//
// 1. Never move a face along its normal into a cell that may be solid. Along
//    the view ray such an inset grows as 1 / cos of the viewing angle, so at
//    a grazing angle it outruns any pull toward the eye and the face falls
//    behind (or fights with) the block face it lies on. An offset toward the
//    side the camera must be on (a light tint lifted into its air cell) is
//    fine.
// 2. Separate from coplanar terrain by pulling the whole mesh toward the eye
//    by a fraction (`withTowardEye`): the screen position stays, the depth
//    moves nearer in proportion to distance, at any angle.
// 3. Layers drawn over each other use increasing pulls, nearest last.
// 4. Overlay faces that would share a plane with each other drop one of the
//    pair (schematic ghosts: `GhostFaces.h`).
namespace lamium::overlay::depth {
inline constexpr float ghostPull = .998f;
inline constexpr float facePull = .997f;   // Shapes, restriction faces, light tints
inline constexpr float digitPull = .995f;  // Light digits over their tint
inline constexpr float markPull = .995f;   // A cell mark over another overlay's outline (save-area corners)
inline constexpr float linePull = .993f;   // Light digit lines over both
// Where a point is drawn after the pull toward `eye`.
inline Point pulled(Point p, Point eye, float pull) {
    return {eye.x + (p.x - eye.x) * pull, eye.y + (p.y - eye.y) * pull, eye.z + (p.z - eye.z) * pull};
}
inline double distance(Point a, Point b) { return std::hypot(a.x - b.x, a.y - b.y, a.z - b.z); }
}
