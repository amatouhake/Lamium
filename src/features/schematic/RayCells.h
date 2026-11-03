#pragma once
#include "features/schematic/Placement.h"
#include <cmath>
#include <limits>
#include <vector>

// The cells a ray passes through, nearest first (BACKLOG L-184, the target
// card's ghost under the crosshair). Pure; the ghost pass looks them up.
namespace lamium::schematic {
// Cells entered from (x, y, z) along (dx, dy, dz) within `reach` blocks,
// the start cell first; empty for a zero direction.
inline std::vector<Point> cellsAlong(double x, double y, double z, double dx, double dy, double dz, double reach) {
    std::vector<Point> out;
    double length = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (!(length > 0) || !(reach > 0)) return out;
    dx /= length; dy /= length; dz /= length;
    Point cell{static_cast<int>(std::floor(x)), static_cast<int>(std::floor(y)), static_cast<int>(std::floor(z))};
    constexpr double never = std::numeric_limits<double>::infinity();
    auto setup = [&](double p, double d, int c, int& step, double& next, double& delta) {
        step = d > 0 ? 1 : d < 0 ? -1 : 0;
        delta = d != 0 ? std::abs(1 / d) : never;
        next = d > 0 ? (c + 1 - p) / d : d < 0 ? (p - c) / -d : never;
    };
    int sx, sy, sz;
    double nx, ny, nz, ddx, ddy, ddz;
    setup(x, dx, cell.x, sx, nx, ddx);
    setup(y, dy, cell.y, sy, ny, ddy);
    setup(z, dz, cell.z, sz, nz, ddz);
    out.push_back(cell);
    // A bound on the steps, so a degenerate ray cannot loop.
    for (int i = 0; i < 3 * static_cast<int>(reach + 2); ++i) {
        double t = std::min({nx, ny, nz});
        if (t > reach) break;
        if (t == nx) { cell.x += sx; nx += ddx; }
        else if (t == ny) { cell.y += sy; ny += ddy; }
        else { cell.z += sz; nz += ddz; }
        out.push_back(cell);
    }
    return out;
}
// Distance along the ray where it enters `cell`'s box, 0 when it starts inside
// (L-304: which of a real liquid and a ghost comes first). Pass `cellsAlong`
// output: a cell the ray never enters also gives 0, which no caller wants.
inline double entryDistance(double x, double y, double z, double dx, double dy, double dz, Point cell) {
    double length = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (!(length > 0)) return 0;
    dx /= length; dy /= length; dz /= length;
    double entered = 0;
    auto axis = [&](double origin, double dir, int at) {
        if (dir > 0) entered = std::max(entered, (at - origin) / dir);
        else if (dir < 0) entered = std::max(entered, (at + 1 - origin) / dir);
    };
    axis(x, dx, cell.x);
    axis(y, dy, cell.y);
    axis(z, dz, cell.z);
    return entered;
}
// How far the ghost search runs when a real liquid starts at `liquidEntered`
// (L-304): the nearest of the two wins and a ghost at the same distance keeps
// the target. The traversal adds its steps up and lands an ulp or so off the
// direct distance, so the cut is widened by `sameDistance` - far below anything
// visible across the 64-block reach. A liquid at the eye's own cell leaves 0:
// what the player is standing in is the target, not a ghost placed in it.
inline constexpr double sameDistance = 1e-9;
inline double ghostReach(double liquidEntered, double reach) {
    if (!(liquidEntered > 0)) return 0;
    return std::min(reach, liquidEntered + sameDistance);
}
}
