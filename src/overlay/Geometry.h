#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <compare>
#include <cstdint>
#include <limits>
#include <set>
#include <unordered_set>
#include <stdexcept>
#include <utility>
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
// Blocks a round shape is drawn from, as in MiniHUD: circles and cylinders are
// rings, spheres the filled volume whose exposed faces form the outer surface.
inline std::set<Cell> displayCells(ShapeSpec const& spec) {
    auto cells = rasterize(spec);
    return spec.shape == Shape::Sphere ? cells : boundaryCells(cells, true);
}
// Round shapes by column: every (x, z) column holds one contiguous y range, so
// surfaces come from comparing neighbouring columns without enumerating the
// volume. Work grows with the footprint and surface, not the volume, so large
// radii (for example a 128-block despawn sphere) stay practical. Circles and
// cylinders keep only the columns on the disk's edge (a ring); spheres keep all
// columns, whose exposed faces form the outer surface.
inline constexpr double maximumRoundRadius = 1024;
struct RoundColumns {
    int x0 = 0, z0 = 0, width = 0, depth = 0;
    int low = 1, high = 0; // y extent over all kept columns
    std::vector<std::pair<int,int>> ranges; // empty when first > second
    std::pair<int,int> at(int x, int z) const {
        if (x < x0 || z < z0 || x >= x0 + width || z >= z0 + depth) return {1, 0};
        return ranges[static_cast<size_t>(x - x0) * depth + (z - z0)];
    }
};
inline RoundColumns roundColumns(ShapeSpec const& spec) {
    if (!std::isfinite(spec.radius) || spec.radius < 0 || spec.height < 1)
        throw std::invalid_argument("Invalid shape dimensions");
    if (spec.radius > maximumRoundRadius || spec.height > 4096) throw std::length_error("Shape exceeds work limit");
    auto center = snapped(spec.center, spec.snap);
    double r2 = spec.radius * spec.radius;
    RoundColumns result;
    result.x0 = checkedCoordinate(std::ceil(center.x - spec.radius - .5));
    result.z0 = checkedCoordinate(std::ceil(center.z - spec.radius - .5));
    int x1 = checkedCoordinate(std::floor(center.x + spec.radius - .5));
    int z1 = checkedCoordinate(std::floor(center.z + spec.radius - .5));
    if (result.x0 > x1 || result.z0 > z1) return result;
    result.width = x1 - result.x0 + 1;
    result.depth = z1 - result.z0 + 1;
    int base = checkedCoordinate(std::floor(center.y));
    checkedCoordinate(double(base) + spec.height);
    std::vector<std::pair<int,int>> disk(static_cast<size_t>(result.width) * result.depth, {1, 0});
    for (int i = 0; i < result.width; ++i) for (int k = 0; k < result.depth; ++k) {
        double dx = result.x0 + i + .5 - center.x, dz = result.z0 + k + .5 - center.z;
        double d2 = dx*dx + dz*dz;
        if (d2 > r2) continue;
        auto& range = disk[static_cast<size_t>(i) * result.depth + k];
        if (spec.shape == Shape::Sphere) {
            double s = std::sqrt(r2 - d2);
            range = {checkedCoordinate(std::ceil(center.y - .5 - s)), checkedCoordinate(std::floor(center.y - .5 + s))};
        } else range = {base, spec.shape == Shape::Cylinder ? base + spec.height - 1 : base};
    }
    result.ranges = disk;
    if (spec.shape != Shape::Sphere) {
        // Keep the ring: disk columns with a horizontal neighbour outside the disk.
        auto inside = [&](int i, int k) {
            return i >= 0 && k >= 0 && i < result.width && k < result.depth
                && disk[static_cast<size_t>(i) * result.depth + k].first <= disk[static_cast<size_t>(i) * result.depth + k].second;
        };
        for (int i = 0; i < result.width; ++i) for (int k = 0; k < result.depth; ++k)
            if (inside(i, k) && inside(i-1, k) && inside(i+1, k) && inside(i, k-1) && inside(i, k+1))
                result.ranges[static_cast<size_t>(i) * result.depth + k] = {1, 0};
    }
    bool any = false;
    for (auto const& [lo, hi] : result.ranges) {
        if (lo > hi) continue;
        if (!any) { result.low = lo; result.high = hi; any = true; }
        result.low = std::min(result.low, lo); result.high = std::max(result.high, hi);
    }
    return result;
}
// Exposed block faces of a round shape. Throws std::length_error over the limit.
inline std::vector<CellFace> roundFaces(ShapeSpec const& spec, size_t faceLimit = 4000000) {
    auto columns = roundColumns(spec);
    std::vector<CellFace> faces;
    auto emit = [&](Cell cell, Face face) {
        if (faces.size() >= faceLimit) throw std::length_error("Surface exceeds face limit");
        faces.push_back({cell, face});
    };
    static constexpr std::array<std::pair<int,int>,4> sides{{{-1,0},{1,0},{0,-1},{0,1}}};
    static constexpr std::array<Face,4> sideFaces{Face::West, Face::East, Face::North, Face::South};
    for (int x = columns.x0; x < columns.x0 + columns.width; ++x)
        for (int z = columns.z0; z < columns.z0 + columns.depth; ++z) {
            auto [lo, hi] = columns.at(x, z);
            if (lo > hi) continue;
            emit({x, lo, z}, Face::Down);
            emit({x, hi, z}, Face::Up);
            for (size_t s = 0; s < sides.size(); ++s) {
                auto [nlo, nhi] = columns.at(x + sides[s].first, z + sides[s].second);
                for (int y = lo; y <= hi; ++y)
                    if (nlo > nhi || y < nlo || y > nhi) emit({x, y, z}, sideFaces[s]);
            }
        }
    return faces;
}
// Blocks of one layer that belong to the displayed surface, for previews.
inline std::vector<Cell> roundLayer(RoundColumns const& columns, int y, bool sphere) {
    std::vector<Cell> cells;
    for (int x = columns.x0; x < columns.x0 + columns.width; ++x)
        for (int z = columns.z0; z < columns.z0 + columns.depth; ++z) {
            auto [lo, hi] = columns.at(x, z);
            if (y < lo || y > hi) continue;
            bool surface = !sphere || y == lo || y == hi;
            for (auto [dx, dz] : {std::pair{-1,0}, std::pair{1,0}, std::pair{0,-1}, std::pair{0,1}}) {
                auto [nlo, nhi] = columns.at(x + dx, z + dz);
                surface = surface || y < nlo || y > nhi;
            }
            if (surface) cells.push_back({x, y, z});
        }
    return cells;
}
// Unit block edges of a set of faces, each drawn once.
inline std::vector<Line> faceLines(std::vector<CellFace> const& faces, size_t lineLimit = 1000000);
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

inline std::vector<Line> faceLines(std::vector<CellFace> const& faces, size_t lineLimit) {
    // Key an edge by its lower corner and axis; hashing keeps large surfaces fast.
    struct Edge { int x, y, z, axis; bool operator==(Edge const&) const = default; };
    struct EdgeHash {
        size_t operator()(Edge const& e) const noexcept {
            uint64_t h = static_cast<uint32_t>(e.x);
            h = h * 1000003u ^ static_cast<uint32_t>(e.y);
            h = h * 1000003u ^ static_cast<uint32_t>(e.z);
            return static_cast<size_t>(h * 4 + e.axis);
        }
    };
    std::unordered_set<Edge, EdgeHash> edges;
    edges.reserve(faces.size() * 2);
    for (auto const& face : faces) {
        auto vertices = faceVertices(face);
        for (size_t i = 0; i < vertices.size(); ++i) {
            auto a = vertices[i], b = vertices[(i+1) % vertices.size()];
            int axis = a.x != b.x ? 0 : a.y != b.y ? 1 : 2;
            Edge edge{static_cast<int>(std::min(a.x, b.x)), static_cast<int>(std::min(a.y, b.y)),
                      static_cast<int>(std::min(a.z, b.z)), axis};
            edges.insert(edge);
            if (edges.size() > lineLimit) throw std::length_error("Surface exceeds line limit");
        }
    }
    std::vector<Line> result;
    result.reserve(edges.size());
    for (auto const& e : edges) {
        Point from{double(e.x), double(e.y), double(e.z)}, to = from;
        (e.axis == 0 ? to.x : e.axis == 1 ? to.y : to.z) += 1;
        result.push_back({from, to});
    }
    return result;
}
// Keep the unit grid on exposed faces, including seams between adjacent surface
// blocks. Internal faces contribute no edges; shared surface edges draw once.
// Build outside the render loop and cache the result with its shape definition.
inline std::vector<Line> gridSurfaceLines(std::set<Cell> const& cells, size_t lineLimit = 1000000) {
    if (cells.size() > 250000) throw std::length_error("Surface exceeds cell limit");
    std::set<std::pair<Cell,Cell>> edges;
    for (auto face : boundaryFaces(cells)) {
        auto vertices = faceVertices(face);
        for (size_t i=0; i<vertices.size(); ++i) {
            auto a = vertices[i], b = vertices[(i+1)%vertices.size()];
            // boundaryFaces validated cell coordinates with neighbour headroom;
            // face vertices therefore remain representable as integer corners.
            Cell first{static_cast<int>(a.x),static_cast<int>(a.y),static_cast<int>(a.z)};
            Cell second{static_cast<int>(b.x),static_cast<int>(b.y),static_cast<int>(b.z)};
            if (second < first) std::swap(first, second);
            edges.emplace(first, second);
            if (edges.size() > lineLimit) throw std::length_error("Surface exceeds line limit");
        }
    }
    std::vector<Line> result;
    result.reserve(edges.size());
    for (auto [a,b] : edges)
        result.push_back({{double(a.x),double(a.y),double(a.z)}, {double(b.x),double(b.y),double(b.z)}});
    return result;
}
}
