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
}
