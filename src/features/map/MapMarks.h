#pragma once
#include <cstdint>

// Marks on the minimap and the world map (BACKLOG L-139). A mark is named by
// its layer and the session id its provider gave it (app/SessionIds.h),
// never by a list position, so adding or removing another entry cannot move
// a selection or an open menu to a different one.
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
}
