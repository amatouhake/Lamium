#include "features/interaction/RestrictionRegion.h"
void check(bool,char const*);
void restrictionRegionTests() {
    using namespace lamium::interaction;
    using lamium::overlay::Cell;
    using lamium::overlay::Face;
    Cell anchor{-17,64,31};
    for (auto axis : {Axis::X,Axis::Y,Axis::Z}) {
        RestrictionRegion plane{RestrictionMode::Plane,anchor,axis}, line{RestrictionMode::Line,anchor,axis};
        check(plane.contains(anchor) && line.contains(anchor), "anchor is in every restriction");
        auto planeCells = plane.preview(2), lineCells = line.preview(2);
        check(planeCells.size() == 25 && lineCells.size() == 5, "plane and line preview dimensions");
        for (auto cell : planeCells) check(plane.contains(cell), "overlay never advertises disallowed plane cells");
        for (auto cell : lineCells) check(line.contains(cell), "overlay never advertises disallowed line cells");
        Cell along = anchor;
        if (axis == Axis::X) along.x += 100;
        else if (axis == Axis::Y) along.y += 100;
        else along.z += 100;
        check(!plane.contains(along) && line.contains(along), "line follows face normal and is not preview-radius limited");
    }
    RestrictionRegion column{RestrictionMode::Column,anchor,Axis::X}, layer{RestrictionMode::Layer,anchor,Axis::Z};
    check(column.effectiveAxis() == Axis::Y && layer.effectiveAxis() == Axis::Y,
          "vertical modes report the effective axis rather than the captured face");
    check(column.contains({-17,-100,31}) && !column.contains({-16,64,31}), "column follows world vertical independently of face");
    check(layer.contains({500,64,-500}) && !layer.contains({-17,65,31}), "layer fixes world Y independently of face");
    check(normalAxis(Face::West) == normalAxis(Face::East) && normalAxis(Face::Up) == Axis::Y
        && normalAxis(Face::North) == Axis::Z, "opposing faces select the same restriction axis");
    check(layer.preview(0) == std::set<Cell>{anchor}, "zero-radius preview is the anchor");
    bool rejected = false;
    try { (void)layer.preview(17); } catch (std::invalid_argument const&) { rejected = true; }
    check(rejected, "preview rejects excessive work rather than silently truncating");
    RestrictionRegion edge{RestrictionMode::Layer,{std::numeric_limits<int>::max()-1,0,0},Axis::Y};
    check(edge.preview(1).size() == 6, "preview avoids integer overflow and retains face headroom");
}
