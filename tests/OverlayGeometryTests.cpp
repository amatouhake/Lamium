#include "overlay/Geometry.h"
#include "overlay/ChunkBorders.h"
#include "overlay/Hitboxes.h"
void check(bool, char const*);
void overlayGeometryTests() {
    using namespace lamium::overlay;
    check(hitboxInRange({-1,-1,-1},{1,1,1},{0,0,0},0), "camera inside hitbox is within range");
    check(hitboxInRange({-10,0,0},{-2,2,2},{0,1,1},2), "hitbox distance uses nearest face, not center");
    check(!hitboxInRange({-10,0,0},{-2,2,2},{0,1,1},1.9), "outside hitbox display distance is culled");
    check(!hitboxInRange({0,0,0},{0,1,1},{0,0,0},64), "degenerate hitboxes are skipped");
    check(!hitboxInRange({1,1,1},{-1,-1,-1},{0,0,0},64), "inverted hitboxes are skipped");
    check(!hitboxInRange({0,0,0},{1,1,1},{std::numeric_limits<double>::quiet_NaN(),0,0},64),
          "invalid camera cannot create hitbox vertices");
    check(gridSurfaceLines({}).empty(), "empty block surface has no lines");
    check(gridSurfaceLines({{0,0,0}}).size() == 12, "single block surface draws each edge once");
    check(gridSurfaceLines({{0,0,0},{1,0,0}}).size() == 20,
          "adjacent blocks preserve surface grid seams without duplicate edges");
    std::set<Cell> cube;
    for (int x=0;x<2;++x) for (int y=0;y<2;++y) for (int z=0;z<2;++z) cube.insert({x,y,z});
    auto surface = gridSurfaceLines(cube);
    check(surface.size() == 48, "solid cube draws exterior grid edges only");
    for (auto edge : surface) {
        auto a=edge.from, b=edge.to;
        check((a.x==b.x && (a.x==0 || a.x==2)) || (a.y==b.y && (a.y==0 || a.y==2))
              || (a.z==b.z && (a.z==0 || a.z==2)), "no grid line is buried inside a solid shape");
        check(std::abs(a.x-b.x)+std::abs(a.y-b.y)+std::abs(a.z-b.z) == 1,
              "block surface lines preserve unit grid positions");
    }
    bool limited = false;
    try { (void)gridSurfaceLines(cube, 47); } catch (std::length_error const&) { limited = true; }
    check(limited, "surface budget rejects instead of returning a partial shape");
    ChunkBorderCache cache;
    auto const* reused = cache.get({-.1,64,-16}, -64,320).data();
    check(cache.get({-15.9,200,-.1}, -64,320).data() == reused,
          "movement inside one chunk reuses geometry storage");
    for (auto position : {Point{0,64,0}, Point{-16.1,64,-16.1}, Point{16,64,16}}) {
        auto const& actual = cache.get(position, -64,320);
        auto expected = chunkBorders(position, -64,320);
        check(actual.size() == expected.size(), "crossing chunk edges refreshes geometry");
        for (size_t i=0; i<expected.size(); ++i)
            check(actual[i].from == expected[i].from && actual[i].to == expected[i].to,
                  "cached borders follow positive and negative chunk transitions");
    }
    auto const& shorter = cache.get({16,64,16}, 0,128);
    check(shorter.size() == 40, "dimension height changes regenerate section lines");
    reused = shorter.data();
    bool invalidCacheInput = false;
    try { (void)cache.get({16,std::numeric_limits<double>::quiet_NaN(),16}, 0,128); }
    catch (std::invalid_argument const&) { invalidCacheInput = true; }
    check(invalidCacheInput, "cache hit still validates position");
    check(cache.get({16,64,16}, 0,128).data() == reused,
          "invalid request preserves last valid geometry");
    auto chunk = chunkBorders({-.1,64,-16}, -64, 320);
    check(chunk.size() == 104, "chunk border includes section layers without duplicate end caps");
    for (auto line : chunk) for (auto p : {line.from,line.to})
        check(p.x >= -16 && p.x <= 0 && p.z >= -16 && p.z <= 0 && p.y >= -64 && p.y <= 320,
              "negative chunks use floor division and supplied dimension height");
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
