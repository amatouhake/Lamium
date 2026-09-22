#include "overlay/Geometry.h"
void check(bool, char const*);
void overlayGeometryTests() {
    using namespace lamium::overlay;
    check(snapped({-.1,-1,1.9}) == Point{-.5,-.5,1.5}, "block center snap floors negative coordinates");
    check(snapped({-.1,-1,1.9}, Snap::BlockCorner) == Point{-1,-1,1}, "corner snap uses lower grid corner");
    check(snapped({-.1,-1,1.9}, Snap::Off) == Point{-.1,-1,1.9}, "off retains arbitrary center");
    auto box = wireBox({-1,2,3}, {4,5,6});
    for (auto edge : box) {
        int varying = (edge.from.x != edge.to.x) + (edge.from.y != edge.to.y) + (edge.from.z != edge.to.z);
        check(varying == 1, "wire box edges follow one axis");
    }
    ShapeSpec sphere{Shape::Sphere, {0,0,0}, Snap::BlockCenter, 1, 1};
    auto cells = rasterize(sphere);
    check(cells.size() == 7 && cells.contains({0,0,0}) && cells.contains({-1,0,0}), "unit sphere uses block-center distance");
    check(boundaryFaces(cells).size() == 30, "shared faces are removed");
    check(boundaryCells(cells).size() == 6, "interior block omitted from shell");
    sphere.center = {-10,4,7};
    auto shifted = rasterize(sphere);
    for (auto cell : cells) check(shifted.contains({cell.x-10,cell.y+4,cell.z+7}), "integer translation preserves shape");
    ShapeSpec disk{Shape::Circle, {0,0,0}, Snap::BlockCenter, 1, 1};
    cells = rasterize(disk);
    check(cells.size() == 5 && boundaryCells(cells, true).size() == 4, "circle ring excludes disk interior");
    disk.shape = Shape::Cylinder; disk.height = 3;
    check(rasterize(disk).size() == 15, "cylinder spans requested grid layers");
    auto grid = gridPlane({-2,5,-3}, 5, 5, 2);
    check(grid.size() == 21 && !grid.contains({-1,5,-2}), "grid lines preserve open cells and negative origin");
    check(gridPlane({-2,5,-3}, 5, 5).size() == 25, "unit spacing forms a filled plane");
    check(gridPlane({0,0,0}, 2, 3, 1, Plane::XY).contains({1,2,0}), "XY plane maps grid axes");
    check(gridPlane({0,0,0}, 2, 3, 1, Plane::YZ).contains({0,1,2}), "YZ plane maps grid axes");
    for (size_t i=0; i<neighbours.size(); ++i) {
        auto corners = faceVertices({{0,0,0}, static_cast<Face>(i)});
        auto a = Point{corners[1].x-corners[0].x,corners[1].y-corners[0].y,corners[1].z-corners[0].z};
        auto b = Point{corners[2].x-corners[0].x,corners[2].y-corners[0].y,corners[2].z-corners[0].z};
        Point normal{a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x};
        check(normal == Point{double(neighbours[i].x),double(neighbours[i].y),double(neighbours[i].z)}, "face winding points outward");
    }
    sphere.radius = 100;
    bool rejected = false;
    try { (void)rasterize(sphere, 1000); } catch (std::length_error const&) { rejected = true; }
    check(rejected, "oversized shape rejected before enumeration");
    sphere.radius = std::numeric_limits<double>::quiet_NaN();
    rejected = false;
    try { (void)rasterize(sphere); } catch (std::invalid_argument const&) { rejected = true; }
    check(rejected, "NaN shape rejected");
    rejected = false;
    try { (void)boundaryFaces({{std::numeric_limits<int>::max(),0,0}}); } catch (std::out_of_range const&) { rejected = true; }
    check(rejected, "boundary neighbour arithmetic cannot overflow");
}
