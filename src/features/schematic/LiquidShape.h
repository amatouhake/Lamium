#pragma once
// The top of a liquid shell (B3): each top corner's height from the cells
// around it, so flowing water and lava slope like the game's. Pure, tested
// in tests/SchematicTests.cpp.
#include <algorithm>

namespace lamium::schematic::liquids {
// What one cell holds, as far as a liquid surface cares.
struct Cell {
    int kind = 0;       // 0 none, 1 water, 2 lava
    int depth = 0;      // liquid_depth: 0 source, 1-7 flowing, 8+ falling
    bool solid = false; // a solid block (not counted toward a corner)
};
// A liquid's surface in its own cell, from the floor.
inline float surface(int depth) {
    if (depth >= 8) depth = 0; // falling liquid stands full like a source
    return 1.f - static_cast<float>(std::clamp(depth, 0, 7) + 1) / 9.f;
}
// The height of the corner (cx, cz), each 0 or 1, of the cell at (0, 0, 0)
// holding `kind`. `at(dx, dy, dz)` describes a cell relative to it. The
// four cells sharing the corner count: the same liquid above any of them
// raises it to the full block; a source weighs ten times a flowing cell;
// an open cell (air, no liquid) pulls it down; solid cells do not count.
template <class At>
float corner(int kind, At&& at, int cx, int cz) {
    float sum = 0, weight = 0;
    for (int dx = cx - 1; dx <= cx; ++dx)
        for (int dz = cz - 1; dz <= cz; ++dz) {
            if (at(dx, 1, dz).kind == kind) return 1.f;
            Cell cell = at(dx, 0, dz);
            if (cell.kind == kind) {
                float w = cell.depth == 0 || cell.depth >= 8 ? 10.f : 1.f;
                sum += surface(cell.depth) * w;
                weight += w;
            } else if (!cell.solid) {
                weight += 1;
            }
        }
    return weight > 0 ? sum / weight : surface(0);
}
// Which way the liquid in the cell at (0, 0, 0) moves (x, z), from its four
// neighbors: toward the same liquid standing lower, or toward an open cell
// with the same liquid below it. A sloped edge alone is not flow: a pool's
// sources lean toward their walls and still show the still texture.
struct Flow { float x = 0, z = 0; };
template <class At>
Flow flow(int kind, At&& at) {
    float own = surface(at(0, 0, 0).depth);
    Flow out;
    for (auto [dx, dz] : {std::pair{-1, 0}, {1, 0}, {0, -1}, {0, 1}}) {
        Cell n = at(dx, 0, dz);
        float diff = 0;
        if (n.kind == kind) {
            diff = own - surface(n.depth);
        } else if (n.kind == 0 && !n.solid) {
            Cell below = at(dx, -1, dz);
            if (below.kind != kind) continue;
            diff = own - (surface(below.depth) - surface(0));
        } else {
            continue;
        }
        out.x += static_cast<float>(dx) * diff;
        out.z += static_cast<float>(dz) * diff;
    }
    return out;
}
}
