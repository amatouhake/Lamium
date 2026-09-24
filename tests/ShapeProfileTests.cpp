#include "overlay/Geometry.h"
#include "ui/ShapeEditor.h"
#include <set>
void check(bool, char const*);
namespace {
// Independent volume enumeration for the new presets: the same inside-test
// math, but enumerating the whole bounding volume instead of column ranges.
// Agreement validates the column-range extraction, the actual algorithm.
std::set<lamium::overlay::Cell> bruteCells(lamium::overlay::ShapeSpec spec) {
    using namespace lamium::overlay;
    spec.axis = Axis::Y;
    Point center = snapped(spec.center, spec.snap);
    double r = spec.radius;
    int base = static_cast<int>(std::floor(center.y));
    double top = spec.topRadius;
    double vertical = spec.heightRadius > 0 ? spec.heightRadius : r;
    double extent = r;
    if (shapeProfile(spec.shape) == Profile::Taper) extent = std::max(r, top);
    int x0 = static_cast<int>(std::ceil(center.x - extent - .5));
    int x1 = static_cast<int>(std::floor(center.x + extent - .5));
    int z0 = static_cast<int>(std::ceil(center.z - extent - .5));
    int z1 = static_cast<int>(std::floor(center.z + extent - .5));
    int y0 = base, y1 = base + spec.height - 1;
    if (shapeProfile(spec.shape) == Profile::Round) {
        y0 = static_cast<int>(std::ceil(center.y - .5 - vertical));
        y1 = static_cast<int>(std::floor(center.y - .5 + vertical));
        if (spec.dome) y0 = std::max(y0, static_cast<int>(std::floor(center.y)));
    }
    bool square = shapeSection(spec.shape) == CrossSection::Square;
    std::set<Cell> cells;
    for (int x = x0; x <= x1; ++x)
        for (int y = y0; y <= y1; ++y)
            for (int z = z0; z <= z1; ++z) {
                double dx = x + .5 - center.x, dz = z + .5 - center.z;
                double distance = square ? std::max(std::abs(dx), std::abs(dz)) : std::sqrt(dx * dx + dz * dz);
                bool inside = false;
                if (shapeProfile(spec.shape) == Profile::Constant) {
                    inside = distance <= r && y >= base && y < base + spec.height;
                } else if (shapeProfile(spec.shape) == Profile::Taper) {
                    int k = y - base;
                    if (k >= 0 && k < spec.height) {
                        double rk = spec.height == 1 ? r : r + (top - r) * k / (spec.height - 1);
                        inside = distance <= rk;
                    }
                } else {
                    double s = vertical * std::sqrt(std::max(0., 1 - (r > 0 ? distance * distance / (r * r) : 0)));
                    inside = distance <= r && y >= std::ceil(center.y - .5 - s) && y <= std::floor(center.y - .5 + s)
                        && (!spec.dome || y >= std::floor(center.y));
                }
                if (inside) cells.insert({x, y, z});
            }
    return cells;
}
std::vector<lamium::overlay::CellFace> sortedFaces(std::vector<lamium::overlay::CellFace> faces) {
    std::sort(faces.begin(), faces.end(), [](auto const& a, auto const& b) {
        auto key = [](auto const& face) {
            return std::tuple(face.cell.x, face.cell.y, face.cell.z, static_cast<int>(face.face));
        };
        return key(a) < key(b);
    });
    return faces;
}
}
void shapeProfileTests() {
    using namespace lamium;
    using namespace lamium::overlay;
    auto matchesVolume = [&](ShapeSpec spec) {
        auto expected = boundaryFaces(bruteCells(spec));
        auto actual = roundFaces(spec);
        return sortedFaces(expected) == sortedFaces(actual);
    };
    ShapeSpec box{Shape::Box, {0.5, 0.5, 0.5}, Snap::BlockCenter, 2, 3};
    check(matchesVolume(box), "box columns match the volume");
    {
        std::set<Cell> cells;
        for (auto const& face : roundFaces(box)) cells.insert(face.cell);
        check(cells.size() == 66, "box spans its full 5x3x5 surface");
    }
    ShapeSpec cone{Shape::Cone, {0.5, 0.5, 0.5}, Snap::BlockCenter, 2, 3, Axis::Y, 0, 0, false};
    check(matchesVolume(cone), "cone columns match the volume");
    {
        std::set<Cell> cells;
        for (auto const& face : roundFaces(cone)) cells.insert(face.cell);
        check(cells.size() == 18 && cells.contains({0, 2, 0}), "cone narrows to a single apex cell");
    }
    ShapeSpec frustum{Shape::Frustum, {0.5, 0.5, 0.5}, Snap::BlockCenter, 4, 7, Axis::Y, 2, 0, false};
    check(matchesVolume(frustum), "frustum columns match the volume");
    ShapeSpec flared{Shape::Frustum, {0.5, 0.5, 0.5}, Snap::BlockCenter, 2, 3, Axis::Y, 4, 0, false};
    check(matchesVolume(flared), "flaring taper columns match the volume");
    ShapeSpec pyramid{Shape::Pyramid, {0.5, 0.5, 0.5}, Snap::BlockCenter, 2, 3, Axis::Y, 0, 0, false};
    check(matchesVolume(pyramid), "pyramid columns match the volume");
    {
        std::set<Cell> cells;
        for (auto const& face : roundFaces(pyramid)) cells.insert(face.cell);
        check(cells.contains({0, 2, 0}), "pyramid narrows to a single apex cell");
    }
    ShapeSpec ellipsoid{Shape::Ellipsoid, {0.5, 0.5, 0.5}, Snap::BlockCenter, 3, 1, Axis::Y, 0, 2, false};
    check(matchesVolume(ellipsoid), "ellipsoid columns match the volume");
    ShapeSpec dome{Shape::Dome, {0.5, 0.5, 0.5}, Snap::BlockCenter, 2, 1, Axis::Y, 0, 0, true};
    check(matchesVolume(dome), "dome columns match the volume");
    {
        std::set<Cell> cells;
        for (auto const& face : roundFaces(dome)) cells.insert(face.cell);
        bool upper = true;
        for (auto cell : cells) upper = upper && cell.y >= 0;
        ShapeSpec ball{Shape::Sphere, {0.5, 0.5, 0.5}, Snap::BlockCenter, 2, 1};
        std::set<Cell> full;
        for (auto const& face : roundFaces(ball)) full.insert(face.cell);
        check(upper && !cells.empty() && cells.size() < full.size(), "dome keeps the upper half only");
    }
    // X/Z shapes permute local Y-oriented faces back to the world.
    ShapeSpec flat = box;
    flat.axis = Axis::X;
    auto expected = roundFaces(box);
    for (auto& face : expected) face = permuteFace(face, Axis::X);
    check(sortedFaces(expected) == sortedFaces(roundFaces(flat)), "X-axis faces permute from the local frame");
    ShapeSpec tall = box;
    tall.axis = Axis::Z;
    auto upright = roundFaces(box);
    for (auto& face : upright) face = permuteFace(face, Axis::Z);
    check(sortedFaces(upright) == sortedFaces(roundFaces(tall)), "Z-axis faces permute from the local frame");
    check(permuteFace({{1, 2, 3}, Face::West}, Axis::X).face == Face::Down
              && permuteFace({{1, 2, 3}, Face::West}, Axis::X).cell == Cell{2, 1, 3}
              && permuteFace({{1, 2, 3}, Face::North}, Axis::Z).face == Face::Down,
          "face permutation swaps cells and normals");
    // Editor presets preview at least one layer.
    for (int type = 3; type <= 8; ++type) {
        overlay::ShapeDefinition definition{"P", 0, true};
        auto placed = ui::shape::withType(definition, type, {0.5, 0.5, 0.5}, ui::shape::Reference::StandingBlock);
        check(!ui::shape::preview(placed, 0).runs.empty(), "new presets preview their base layer");
    }
}
