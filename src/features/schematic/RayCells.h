#pragma once
#include "features/schematic/Placement.h"
#include <cmath>
#include <limits>
#include <vector>

// The cells a ray passes through, nearest first (BACKLOG L-184, the target
// card's ghost under the crosshair). Pure; the ghost pass looks them up.
namespace lamium::schematic {
// Cells entered from (x, y, z) along (dx, dy, dz) within `reach` blocks, one at a
// time, the start cell first; nothing for a zero direction. A caller that stops
// at its first answer costs only the cells it looked at (L-308's far reach).
class CellWalk {
    Point cell{};
    int sx = 0, sy = 0, sz = 0, left = 0;
    double nx = 0, ny = 0, nz = 0, ddx = 0, ddy = 0, ddz = 0, reach = 0;
    bool done = true;
    void advance() {
        double t = std::min({nx, ny, nz});
        // A bound on the steps, so a degenerate ray cannot loop.
        if (left-- <= 0 || t > reach) { done = true; return; }
        if (t == nx) { cell.x += sx; nx += ddx; }
        else if (t == ny) { cell.y += sy; ny += ddy; }
        else { cell.z += sz; nz += ddz; }
    }
public:
    CellWalk(double x, double y, double z, double dx, double dy, double dz, double limit) : reach(limit) {
        double length = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (!(length > 0) || !(limit > 0)) return;
        dx /= length; dy /= length; dz /= length;
        cell = {static_cast<int>(std::floor(x)), static_cast<int>(std::floor(y)), static_cast<int>(std::floor(z))};
        constexpr double never = std::numeric_limits<double>::infinity();
        auto setup = [&](double p, double d, int c, int& step, double& next, double& delta) {
            step = d > 0 ? 1 : d < 0 ? -1 : 0;
            delta = d != 0 ? std::abs(1 / d) : never;
            next = d > 0 ? (c + 1 - p) / d : d < 0 ? (p - c) / -d : never;
        };
        setup(x, dx, cell.x, sx, nx, ddx);
        setup(y, dy, cell.y, sy, ny, ddy);
        setup(z, dz, cell.z, sz, nz, ddz);
        left = 3 * static_cast<int>(limit + 2);
        done = false;
    }
    struct End {};
    struct Iterator {
        CellWalk* walk;
        Point const& operator*() const { return walk->cell; }
        Iterator& operator++() { walk->advance(); return *this; }
        bool operator!=(End) const { return !walk->done; }
    };
    Iterator begin() { return {this}; }
    End end() const { return {}; }
};
inline std::vector<Point> cellsAlong(double x, double y, double z, double dx, double dy, double dz, double reach) {
    std::vector<Point> out;
    for (auto const& cell : CellWalk(x, y, z, dx, dy, dz, reach)) out.push_back(cell);
    return out;
}
// The reach cut where the ray leaves the world's height and keeps going, plus a
// margin for what flies above it: past that there are no blocks to find, and
// looking at the sky is otherwise the longest walk there is.
inline double reachInHeight(double y, double dy, double reach, int bottom, int top) {
    constexpr double margin = 16;
    double const limit = dy > 0 ? (top + margin - y) / dy : dy < 0 ? (y - (bottom - margin)) / -dy : reach;
    return std::clamp(limit, std::min(reach, 1.0), reach);
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
