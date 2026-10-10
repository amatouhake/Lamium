#pragma once
#include "features/map/MapMarks.h"
#include "features/map/Waypoints.h"
#include "features/schematic/Placement.h"
#include <cstdint>
#include <string>
#include <vector>

// Layers of the minimap and the world map beside waypoints (BACKLOG L-139):
// marks filled by the features that own them. A provider whose feature is
// off returns nothing, so its marks, and any selection or menu on them, go
// with it. Storage stays with each feature.
namespace lamium::map {
// A footprint mark: a schematic placement or a shape seen from above.
struct AreaMark {
    MarkKey key;
    schematic::Footprint area; // world blocks, [x0, x1) by [z0, z1)
    std::string name;
    std::uint32_t color = 0;
    bool visible = true;
    bool selected = false; // placements: the schematic's selected one
};
// The map palette's cyan; the outline shape tells it from a waypoint.
inline constexpr std::uint32_t placementColor = waypointColors[5];
// Schematic placements in this dimension while Schematics are on. At most
// 64, and a footprint is arithmetic on the placement, so they are listed
// as they are.
std::vector<AreaMark> placementMarks(int dimension);
// Shapes in this dimension while shapes are drawn; their boxes are cached
// by the overlay until a shape changes.
std::vector<AreaMark> shapeMarks(int dimension);

// Actions the maps share. Each finds its entry by id when it runs: an
// entry that is gone is left alone (false).
bool toggleVisible(MarkKey key);
// Makes a placement the schematic's selected one (keys, HUD, Check tab).
bool selectPlacement(MarkKey key);
}
