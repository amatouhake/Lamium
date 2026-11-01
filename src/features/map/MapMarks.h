#pragma once
#include <cmath>
#include <cstdint>
#include <optional>
#include <span>

// Marks on the minimap and the world map (BACKLOG L-139). A mark is named by
// its layer and the session id its provider gave it (app/SessionIds.h,
// overlay ShapeId), never by a list position, so adding or removing another
// entry cannot move a selection or an open menu to a different one.
namespace lamium::map {
enum class MarkLayer : std::uint8_t { Shape, Placement, Waypoint, Death };
struct MarkKey {
    MarkLayer layer = MarkLayer::Waypoint;
    std::uint64_t id = 0;
    bool operator==(MarkKey const&) const = default;
};
// There is one death point at most; it has a fixed key.
inline constexpr MarkKey deathKey{MarkLayer::Death, 0};
inline constexpr MarkKey waypointKey(std::uint64_t id) { return {MarkLayer::Waypoint, id}; }
inline constexpr MarkKey placementKey(std::uint64_t id) { return {MarkLayer::Placement, id}; }
inline constexpr MarkKey shapeKey(std::uint64_t id) { return {MarkLayer::Shape, id}; }
inline constexpr bool pointLayer(MarkLayer layer) { return layer == MarkLayer::Waypoint || layer == MarkLayer::Death; }
// The world map's layer switches (L-146): which marks it draws. The death
// point goes with waypoints.
struct LayerSwitches {
    bool waypoints = true, schematics = true, shapes = true;
};
inline constexpr bool layerShown(LayerSwitches on, MarkLayer layer) {
    return pointLayer(layer) ? on.waypoints : layer == MarkLayer::Placement ? on.schematics : on.shapes;
}
// Which maps a heading's items show on (L-297): Map marks, Radar and Slime
// chunks each choose one. Saved as the index.
enum class ShownOn { Both, WorldMap, Minimap };
inline constexpr int clampShownOn(int value) { return value < 0 || value > 2 ? 0 : value; }
inline constexpr bool onWorldMap(int shownOn) { return clampShownOn(shownOn) != static_cast<int>(ShownOn::Minimap); }
inline constexpr bool onMinimap(int shownOn) { return clampShownOn(shownOn) != static_cast<int>(ShownOn::WorldMap); }
// The marks a map draws: the Map marks heading, its map choice, then each
// mark's own switch (the world map's top bar shows those switches).
inline constexpr LayerSwitches shownMarks(LayerSwitches switches, bool heading, int shownOn, bool minimap) {
    bool here = heading && (minimap ? onMinimap(shownOn) : onWorldMap(shownOn));
    return {here && switches.waypoints, here && switches.schematics, here && switches.shapes};
}
// Why the world map cannot change an item from its top bar, if it cannot:
// the heading is off, or the item is not on this map at all.
enum class Blocked { No, HeadingOff, MinimapOnly };
inline constexpr Blocked worldMapBlocked(bool heading, int shownOn) {
    return !heading ? Blocked::HeadingOff : !onWorldMap(shownOn) ? Blocked::MinimapOnly : Blocked::No;
}
// The world map's side panel lists one layer at a time (L-204): 0 waypoints
// and the death point, 1 schematic placements, 2 shapes.
inline constexpr int listKind(MarkLayer layer) { return pointLayer(layer) ? 0 : layer == MarkLayer::Placement ? 1 : 2; }
inline constexpr int clampListKind(int kind) { return kind < 0 || kind > 2 ? 0 : kind; }

// A mark as last drawn on the world map, in screen units: a point (its
// center twice) or a footprint (its rectangle as drawn).
struct ScreenMark {
    MarkKey key;
    float x0 = 0, y0 = 0, x1 = 0, y1 = 0;
};
// The order overlapping marks of different layers take the cursor:
// waypoints and the death point, then schematic placements, then shapes.
inline constexpr int layerRank(MarkLayer layer) {
    return pointLayer(layer) ? 0 : layer == MarkLayer::Placement ? 1 : 2;
}
// The mark under (x, y). Points count within `reach` of their center, the
// nearest first; footprints when the cursor is inside, the smallest first,
// so a placement inside a larger one stays reachable.
inline std::optional<MarkKey> markAt(std::span<ScreenMark const> marks, float x, float y, float reach = 6) {
    std::optional<MarkKey> best;
    int bestRank = 3;
    float bestMeasure = 0;
    for (auto const& mark : marks) {
        int rank = layerRank(mark.key.layer);
        float measure;
        if (pointLayer(mark.key.layer)) {
            measure = std::hypot(mark.x0 - x, mark.y0 - y);
            if (!(measure < reach)) continue;
        } else {
            if (x < mark.x0 || y < mark.y0 || x >= mark.x1 || y >= mark.y1) continue;
            measure = (mark.x1 - mark.x0) * (mark.y1 - mark.y0);
        }
        if (rank < bestRank || (rank == bestRank && measure < bestMeasure)) {
            best = mark.key;
            bestRank = rank;
            bestMeasure = measure;
        }
    }
    return best;
}
// Whether a key is still among the marks: a selection or a menu on a mark
// whose provider turned off, or whose entry is gone, is dropped.
inline bool present(std::span<ScreenMark const> marks, MarkKey key) {
    for (auto const& mark : marks) if (mark.key == key) return true;
    return false;
}
}
