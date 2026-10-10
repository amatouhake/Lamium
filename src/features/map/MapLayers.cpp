#include "features/map/MapLayers.h"
#include "features/schematic/SchematicSession.h"
#include "overlay/ShapeSession.h"
#include "app/Runtime.h"
#include "app/SessionIds.h"

namespace lamium::map {
namespace {
std::uint32_t shapeMapColor(overlay::ShapeColor color) {
    // The colors shapes draw in the world (WorldOverlay shapeColor).
    switch (color) {
    case overlay::ShapeColor::Yellow: return packColor(242, 204, 61);
    case overlay::ShapeColor::Pink: return packColor(240, 128, 191);
    case overlay::ShapeColor::White: return packColor(242, 242, 242);
    default: return packColor(64, 209, 224);
    }
}
}
std::vector<AreaMark> placementMarks(int dimension) {
    std::vector<AreaMark> marks;
    auto& runtime = Runtime::instance();
    if (!runtime.enabled() || !runtime.snapshot()->schematic.enabled) return marks;
    auto shown = schematic::session::snapshot();
    for (size_t i = 0; i < shown.placements.size(); ++i) {
        auto const& [saved, structure] = shown.placements[i];
        if (!structure || saved.dimension != dimension) continue;
        marks.push_back({placementKey(saved.id), schematic::footprint(structure->size, saved.placement), saved.name, placementColor,
                         saved.visible, static_cast<int>(i) == shown.selected});
    }
    return marks;
}
std::vector<AreaMark> shapeMarks(int dimension) {
    std::vector<AreaMark> marks;
    auto& runtime = Runtime::instance();
    if (!runtime.enabled() || !runtime.snapshot()->overlays.shapes) return marks;
    for (auto const& shape : overlay::shapes::footprints(dimension))
        marks.push_back({shapeKey(shape.id), {shape.area.x0, shape.area.z0, shape.area.x1, shape.area.z1}, shape.name,
                         shapeMapColor(shape.color), shape.visible, false});
    return marks;
}
bool toggleVisible(MarkKey key) {
    if (key.layer == MarkLayer::Placement)
        return schematic::session::change([&](schematic::PlacementSet& set) {
            int index = indexOfId(set.placements, key.id);
            if (index < 0) return false;
            auto& placement = set.placements[static_cast<size_t>(index)];
            placement.visible = !placement.visible;
            return true;
        });
    if (key.layer == MarkLayer::Shape) {
        auto definition = overlay::shapes::find(key.id);
        if (!definition) return false;
        try {
            overlay::shapes::setVisible(key.id, !definition->visible);
            return true;
        } catch (std::exception const&) { return false; }
    }
    return false;
}
bool selectPlacement(MarkKey key) {
    if (key.layer != MarkLayer::Placement) return false;
    return schematic::session::change([&](schematic::PlacementSet& set) {
        int index = indexOfId(set.placements, key.id);
        if (index < 0) return false;
        set.selected = index;
        return true;
    });
}
}
