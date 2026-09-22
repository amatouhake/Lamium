#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <compare>
#include <cstdint>
#include <limits>
#include <set>
#include <stdexcept>
#include <vector>

namespace lamium::overlay {
struct Point { double x{}, y{}, z{}; bool operator==(Point const&) const = default; };
struct Line { Point from, to; };
struct Cell { int x{}, y{}, z{}; auto operator<=>(Cell const&) const = default; };
enum class Snap { BlockCenter, BlockCorner, Off };
enum class Shape { Circle, Cylinder, Sphere };
enum class Face { West, East, Down, Up, North, South };
enum class Plane { XZ, XY, YZ };
struct CellFace { Cell cell; Face face; bool operator==(CellFace const&) const = default; };
inline constexpr std::array<Cell, 6> neighbours{{{-1,0,0}, {1,0,0}, {0,-1,0}, {0,1,0}, {0,0,-1}, {0,0,1}}};

inline bool finite(Point p) { return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z); }
inline Point snapped(Point p, Snap mode = Snap::BlockCenter) {
    if (!finite(p)) throw std::invalid_argument("Shape center must be finite");
    if (mode == Snap::Off) return p;
    double offset = mode == Snap::BlockCenter ? .5 : 0;
    return {std::floor(p.x)+offset, std::floor(p.y)+offset, std::floor(p.z)+offset};
}
inline std::array<Line,12> wireBox(Point min, Point max) {
    if (!finite(min) || !finite(max) || min.x > max.x || min.y > max.y || min.z > max.z)
        throw std::invalid_argument("Invalid wire box bounds");
    std::array<Point,8> corners;
    for (int i=0; i<8; ++i) corners[i] = {i&1 ? max.x:min.x, i&2 ? max.y:min.y, i&4 ? max.z:min.z};
    std::array<Line,12> result;
    size_t next = 0;
    for (int i=0; i<8; ++i)
        for (int bit : {1,2,4}) if (!(i&bit)) result[next++] = {corners[i], corners[i|bit]};
    return result;
}

struct ShapeSpec {
    Shape shape = Shape::Sphere;
    Point center;
    Snap snap = Snap::BlockCenter;
    double radius = 4;
    int height = 1; // Cylinder starts at the center's block Y and extends upward.
};
inline int checkedCoordinate(double value) {
    // Keep one cell of headroom for neighbour/face computations.
    if (!std::isfinite(value) || value < std::numeric_limits<int>::min()+1.0
        || value > std::numeric_limits<int>::max()-1.0) throw std::out_of_range("Shape exceeds grid coordinates");
    return static_cast<int>(value);
}
// Deterministic block-center sampling. Returns filled cells; circle is a single
// disk layer. Boundary extraction below supplies block-aligned surfaces/rings.
// A work limit rejects the whole operation before enumeration, never truncates
// a shape into a misleading partial result.
inline std::set<Cell> rasterize(ShapeSpec spec, uint64_t workLimit = 250000) {
    auto center = snapped(spec.center, spec.snap);
    if (!std::isfinite(spec.radius) || spec.radius < 0 || spec.height < 1)
        throw std::invalid_argument("Invalid shape dimensions");
    Cell low{checkedCoordinate(std::ceil(center.x-spec.radius-.5)), checkedCoordinate(std::floor(center.y)),
             checkedCoordinate(std::ceil(center.z-spec.radius-.5))};
    Cell high{checkedCoordinate(std::floor(center.x+spec.radius-.5)), low.y,
              checkedCoordinate(std::floor(center.z+spec.radius-.5))};
    if (spec.shape == Shape::Sphere) {
        low.y = checkedCoordinate(std::ceil(center.y-spec.radius-.5));
        high.y = checkedCoordinate(std::floor(center.y+spec.radius-.5));
    } else if (spec.shape == Shape::Cylinder) high.y = checkedCoordinate(double(low.y) + spec.height - 1);
    if (low.x > high.x || low.y > high.y || low.z > high.z) return {};
    uint64_t work = 1;
    for (auto extent : {int64_t(high.x)-low.x+1, int64_t(high.y)-low.y+1, int64_t(high.z)-low.z+1}) {
        if (static_cast<uint64_t>(extent) > workLimit / work) throw std::length_error("Shape exceeds work limit");
        work *= static_cast<uint64_t>(extent);
    }
    std::set<Cell> cells;
    for (int x=low.x; x<=high.x; ++x) for (int y=low.y; y<=high.y; ++y) for (int z=low.z; z<=high.z; ++z) {
        double dx = x+.5-center.x, dy = y+.5-center.y, dz = z+.5-center.z;
        if (dx*dx + dz*dz + (spec.shape == Shape::Sphere ? dy*dy : 0) <= spec.radius*spec.radius)
            cells.insert({x,y,z});
    }
    return cells;
}
inline std::vector<CellFace> boundaryFaces(std::set<Cell> const& cells) {
    std::vector<CellFace> faces;
    for (auto cell : cells) {
        checkedCoordinate(cell.x); checkedCoordinate(cell.y); checkedCoordinate(cell.z);
        for (size_t i=0; i<neighbours.size(); ++i) {
            auto d = neighbours[i];
            if (!cells.contains({cell.x+d.x,cell.y+d.y,cell.z+d.z})) faces.push_back({cell, static_cast<Face>(i)});
        }
    }
    return faces;
}
inline std::set<Cell> boundaryCells(std::set<Cell> const& cells, bool horizontalOnly = false) {
    std::set<Cell> result;
    for (auto face : boundaryFaces(cells))
        if (!horizontalOnly || (face.face != Face::Down && face.face != Face::Up)) result.insert(face.cell);
    return result;
}
inline std::set<Cell> gridPlane(Cell origin, int width, int depth, int spacing = 1,
    Plane plane = Plane::XZ, uint64_t workLimit = 250000) {
    if (width < 1 || depth < 1 || spacing < 1) throw std::invalid_argument("Invalid grid dimensions");
    if (static_cast<uint64_t>(width) * static_cast<uint64_t>(depth) > workLimit)
        throw std::length_error("Grid exceeds work limit");
    checkedCoordinate(origin.x); checkedCoordinate(origin.y); checkedCoordinate(origin.z);
    checkedCoordinate(double(plane == Plane::YZ ? origin.y : origin.x) + width - 1);
    checkedCoordinate(double(plane == Plane::XY ? origin.y : origin.z) + depth - 1);
    std::set<Cell> result;
    for (int u=0; u<width; ++u) for (int v=0; v<depth; ++v) {
        // spacing=1 fills the plane. Larger spacing draws grid lines and a
        // complete rectangular border, even for non-multiple dimensions.
        if (u%spacing && v%spacing && u!=width-1 && v!=depth-1) continue;
        if (plane == Plane::XZ) result.insert({origin.x+u,origin.y,origin.z+v});
        else if (plane == Plane::XY) result.insert({origin.x+u,origin.y+v,origin.z});
        else result.insert({origin.x,origin.y+u,origin.z+v});
    }
    return result;
}
// Ordered corners of the actual block face; no mathematical smooth surface.
// The renderer may use a line loop or two triangles from these coordinates.
inline std::array<Point,4> faceVertices(CellFace face) {
    double x=face.cell.x, y=face.cell.y, z=face.cell.z;
    switch (face.face) {
    case Face::West: return {{{x,y,z},{x,y,z+1},{x,y+1,z+1},{x,y+1,z}}};
    case Face::East: return {{{x+1,y,z+1},{x+1,y,z},{x+1,y+1,z},{x+1,y+1,z+1}}};
    case Face::Down: return {{{x,y,z+1},{x,y,z},{x+1,y,z},{x+1,y,z+1}}};
    case Face::Up: return {{{x,y+1,z},{x,y+1,z+1},{x+1,y+1,z+1},{x+1,y+1,z}}};
    case Face::North: return {{{x+1,y,z},{x,y,z},{x,y+1,z},{x+1,y+1,z}}};
    case Face::South: return {{{x,y,z+1},{x+1,y,z+1},{x+1,y+1,z+1},{x,y+1,z+1}}};
    }
    throw std::invalid_argument("Invalid block face");
}
}
