#include "features/map/MapLayers.h"
#include "features/map/WaypointSession.h"
#include "features/schematic/SchematicSession.h"
#include "features/schematic/SchematicActions.h"
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
                         shapeMapColor(shape.color), shape.visible, false, shape.round});
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
bool setVisible(std::vector<MarkKey> const& keys, bool visible) {
    auto has = [&](MarkLayer layer) { return std::ranges::any_of(keys, [&](MarkKey k) { return k.layer == layer; }); };
    auto listed = [&](MarkLayer layer, std::uint64_t id) { return std::ranges::find(keys, MarkKey{layer, id}) != keys.end(); };
    bool saved = true;
    if (has(MarkLayer::Waypoint))
        saved = waypoints::change([&](WaypointSet& set) {
            for (auto& w : set.waypoints) if (listed(MarkLayer::Waypoint, w.id)) w.visible = visible;
            return true;
        }) && saved;
    if (has(MarkLayer::Placement))
        saved = schematic::session::change([&](schematic::PlacementSet& set) {
            for (auto& p : set.placements) if (listed(MarkLayer::Placement, p.id)) p.visible = visible;
            return true;
        }) && saved;
    if (has(MarkLayer::Shape)) {
        std::vector<overlay::ShapeId> shapes;
        for (auto k : keys) if (k.layer == MarkLayer::Shape) shapes.push_back(k.id);
        try { overlay::shapes::setVisible(shapes, visible); }
        catch (std::exception const&) { saved = false; }
    }
    return saved;
}
bool remove(std::vector<MarkKey> const& keys) {
    auto ids = [&](MarkLayer layer) {
        std::vector<std::uint64_t> out;
        for (auto k : keys) if (k.layer == layer) out.push_back(k.id);
        return out;
    };
    bool saved = true;
    if (auto gone = ids(MarkLayer::Waypoint); !gone.empty())
        saved = waypoints::change([&](WaypointSet& set) {
            std::erase_if(set.waypoints, [&](Waypoint const& w) { return std::ranges::find(gone, w.id) != gone.end(); });
            return true;
        }) && saved;
    if (auto gone = ids(MarkLayer::Placement); !gone.empty())
        saved = schematic::session::change([&](schematic::PlacementSet& set) {
            schematic::removePlacements(set, gone);
            return true;
        }) && saved;
    if (auto gone = ids(MarkLayer::Shape); !gone.empty()) {
        std::vector<overlay::ShapeId> shapes(gone.begin(), gone.end());
        try { overlay::shapes::remove(shapes); }
        catch (std::exception const&) { saved = false; }
    }
    return saved;
}
bool selectPlacement(MarkKey key) {
    if (key.layer != MarkLayer::Placement) return false;
    if (!schematic::session::change([&](schematic::PlacementSet& set) {
            int index = indexOfId(set.placements, key.id);
            if (index < 0) return false;
            set.selected = index;
            return true;
        })) return false;
    // Choosing a placement also makes it the movement target (L-231).
    schematic::actions::placementSelected();
    return true;
}
}
