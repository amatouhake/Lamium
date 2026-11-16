#pragma once
#include "features/map/MapRegion.h"
#include "features/map/Waypoints.h"
#include "features/schematic/PlacementStore.h"
#include <algorithm>
#include <vector>

// Merging another saved data into the data in use (BACKLOG L-343 part 4): only
// on the player's request, the other data copied in and left as it was. Every
// step is idempotent, so a merge cut short is finished by merging again.
namespace lamium::map::merge {
struct Counts {
    int taken = 0;   // Added (waypoints, placements) or chunks changed (map).
    int full = 0;    // Left out because the list was full.
};
// Equal in position and content: dimension, block and name. Color, visibility and kind do not tell two apart.
inline bool sameWaypoint(Waypoint const& a, Waypoint const& b) {
    return a.dimension == b.dimension && a.x == b.x && a.y == b.y && a.z == b.z && a.name == b.name;
}
inline Counts mergeWaypoints(WaypointSet& into, std::vector<Waypoint> const& from) {
    Counts counts;
    for (auto const& waypoint : from) {
        bool const known = std::any_of(into.waypoints.begin(), into.waypoints.end(),
            [&](Waypoint const& w) { return sameWaypoint(w, waypoint); });
        if (known) continue;
        if (into.waypoints.size() >= maxWaypoints) { ++counts.full; continue; }
        auto copy = waypoint;
        copy.id = 0;
        into.waypoints.push_back(std::move(copy));
        ++counts.taken;
    }
    return counts;
}
// Equal when the same file is placed the same way in the same dimension; name and layers do not count.
inline bool samePlacement(schematic::SavedPlacement const& a, schematic::SavedPlacement const& b) {
    return a.file == b.file && a.dimension == b.dimension && a.placement.origin == b.placement.origin
        && a.placement.rotation == b.placement.rotation && a.placement.mirror == b.placement.mirror;
}
inline Counts mergePlacements(schematic::PlacementSet& into, std::vector<schematic::SavedPlacement> const& from) {
    Counts counts;
    for (auto const& placement : from) {
        bool const known = std::any_of(into.placements.begin(), into.placements.end(),
            [&](schematic::SavedPlacement const& p) { return samePlacement(p, placement); });
        if (known) continue;
        if (into.placements.size() >= schematic::maxPlacements) { ++counts.full; continue; }
        auto copy = placement;
        copy.id = 0;
        into.placements.push_back(std::move(copy));
        ++counts.taken;
    }
    return counts;
}
// Per chunk, the record seen later wins: a chunk the other data saw later than the data in use lays its
// recorded columns over it (columns it lacks keep what is there, so a partial record never blanks a full
// one); an older or equally old chunk only fills columns nothing was recorded in. The region's own legacy
// facts and unknown sections stay. Returns how many chunks changed.
inline int mergeRegion(RegionData& into, RegionData const& from) {
    std::array<bool, 256> has{}, newer{}, changed{}, kept{};
    for (size_t i = 0; i < static_cast<size_t>(regionColumns); ++i) {
        if (from.colors[i] >> 24) has[chunkOfIndex(i)] = true;
    }
    for (size_t c = 0; c < 256; ++c) {
        if (!has[c]) continue;
        newer[c] = from.chunks[c].time > into.chunks[c].time;
    }
    for (size_t i = 0; i < static_cast<size_t>(regionColumns); ++i) {
        size_t const c = chunkOfIndex(i);
        if (!(from.colors[i] >> 24)) {
            if (into.colors[i] >> 24) kept[c] = true; // A column only the data in use has.
            continue;
        }
        bool const empty = !(into.colors[i] >> 24);
        if (!newer[c] && !empty) continue;
        if (into.colors[i] == from.colors[i] && into.heights[i] == from.heights[i]) continue;
        into.colors[i] = from.colors[i];
        into.heights[i] = from.heights[i];
        changed[c] = true;
    }
    int count = 0;
    for (size_t c = 0; c < 256; ++c) {
        if (!changed[c]) continue;
        ++count;
        // The other stamp whole only where nothing of the data in use is left; a mixed chunk, as the scan's
        // underlay does, keeps the later time and the lower revision.
        if (newer[c] && !kept[c]) into.chunks[c] = from.chunks[c];
        else into.chunks[c] = {std::max(into.chunks[c].time, from.chunks[c].time),
                               std::min(into.chunks[c].revision, from.chunks[c].revision)};
    }
    return count;
}
}
