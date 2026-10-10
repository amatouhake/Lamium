#pragma once
#include "overlay/Geometry.h"
#include <array>
#include <set>
#include <span>
#include <vector>

// The faces and outline of a set of cells (BACKLOG L-138): one builder for
// the overlays that mark cells (Shapes, the breaking restriction, the
// schematic save area). Faces lie on the cells' own planes, never inset into
// a cell that may be solid (Depth.h rule 1); the glue (CellMesh) pulls the
// whole mesh toward the eye (rule 2). Not for schematic ghost blocks, whose
// real block shapes stay in the schematic renderer.
namespace lamium::overlay {
struct CellSurface {
    std::vector<CellFace> faces;
    std::vector<Line> lines;
};
// The exposed faces of `cells` and the unit grid on them.
inline CellSurface cellSurface(std::set<Cell> const& cells) { return {boundaryFaces(cells), gridSurfaceLines(cells)}; }
// The twelve edges of a box of whole cells, `low` to `high` inclusive: the
// outline of an area too large to build cell by cell.
inline std::vector<Line> boxOutline(Cell low, Cell high) {
    auto edges = wireBox({double(low.x), double(low.y), double(low.z)},
                         {double(high.x) + 1, double(high.y) + 1, double(high.z) + 1});
    return {edges.begin(), edges.end()};
}
// The cell meshes are built relative to: the first face's cell, else the
// first line's start.
inline Cell surfaceOrigin(std::span<CellFace const> faces, std::span<Line const> lines) {
    if (!faces.empty()) return faces.front().cell;
    if (!lines.empty()) return {static_cast<int>(lines.front().from.x), static_cast<int>(lines.front().from.y),
                                static_cast<int>(lines.front().from.z)};
    return {};
}
// The corners to upload, four per face in its winding. A material that
// culls (`bothWindings`) gets each face again reversed so it shows from
// inside too; a material that draws both sides must not, as a second quad
// in the same plane flickers.
inline std::vector<Point> faceCorners(std::span<CellFace const> faces, bool bothWindings) {
    std::vector<Point> out;
    out.reserve(faces.size() * (bothWindings ? 8 : 4));
    for (auto const& face : faces) {
        auto corners = faceVertices(face);
        out.insert(out.end(), corners.begin(), corners.end());
        if (bothWindings) out.insert(out.end(), corners.rbegin(), corners.rend());
    }
    return out;
}
// An outline over faces is faint so the block grid stays readable; full
// without faces, or where faces may not show (Vibrant Visuals).
inline float outlineAlpha(bool faces, bool strongLines) { return faces && !strongLines ? .45f : 1.f; }
}
