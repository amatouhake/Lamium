#pragma once
#include "features/map/MapColors.h"
#include "features/map/MapTiles.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <span>

namespace lamium::map {
// Cave view (BACKLOG L-60 step 3): under a ceiling the map shows the floors
// around the player's height instead of the top of the world. Pure.

enum class ViewMode { Surface, Cave };
// Covered: a block above the head. Sky light keeps open ravines and tree
// shade on the surface; the gap between the two thresholds stops flicker at
// cave mouths.
inline ViewMode chooseView(ViewMode current, bool nether, bool covered, int skyLight) {
    if (nether) return ViewMode::Cave;
    if (!covered) return ViewMode::Surface;
    if (current == ViewMode::Cave) return skyLight < 10 ? ViewMode::Cave : ViewMode::Surface;
    return skyLight <= 6 ? ViewMode::Cave : ViewMode::Surface;
}
// Automatic switches wait this long after the last one, so walking along a
// cave mouth does not flip the map back and forth.
inline constexpr double viewSwitchDelay = 1.0;
struct ViewSwitch {
    ViewMode mode = ViewMode::Surface;
    double changedAt = -1e9;
    ViewMode update(ViewMode wanted, double now) {
        if (wanted != mode && now - changedAt >= viewSwitchDelay) {
            mode = wanted;
            changedAt = now;
        }
        return mode;
    }
};
// Covered when most of the 3x3 columns around the player have a roof: a
// single overhang or a leaf above the head does not make a cave.
inline bool coveredByMost(int coveredColumns) { return coveredColumns >= 5; }
// The view the player chose; Auto follows chooseView. The saved setting is
// this enum's index, so the order is the settings row's order (L-253).
enum class ViewForce { Auto, Surface, Cave };
// A corrupt saved value reads as Auto rather than as an accidental view.
inline ViewForce viewForceOf(int saved) {
    return saved >= 0 && saved <= static_cast<int>(ViewForce::Cave) ? static_cast<ViewForce>(saved) : ViewForce::Auto;
}
inline ViewMode applyForce(ViewForce force, ViewMode automatic) {
    if (force == ViewForce::Cave) return ViewMode::Cave;
    if (force == ViewForce::Surface) return ViewMode::Surface;
    return automatic;
}
// The key and the settings stepper walk Automatic -> Surface -> Cave.
inline ViewForce cycleForce(ViewForce force) {
    switch (force) {
    case ViewForce::Auto: return ViewForce::Surface;
    case ViewForce::Surface: return ViewForce::Cave;
    default: return ViewForce::Auto;
    }
}

// The height the cave view is drawn around. It stays put while the player
// moves within `caveLayerSlack` blocks of it, so steps, slopes and jumps do
// not redraw the map; beyond that it moves to the player.
inline constexpr int caveLayerSlack = 3;
inline int stableLayer(int current, int playerY, bool fresh) {
    if (fresh || std::abs(playerY - current) > caveLayerSlack) return playerY;
    return current;
}
// The height the cave map should open at: the recorder's stable band while it
// is close to the player, otherwise the player's own height.
inline int caveReferenceHeight(int stableLayer, int playerY, bool haveLayer) {
    if (haveLayer && std::abs(stableLayer - playerY) <= caveLayerSlack) return stableLayer;
    return playerY;
}
// Blocks examined per column around the layer: enough above for the head of
// a player standing up to the slack higher, and a deep window below so the
// floor or lava under a bridge is still part of the occupied cavern (L-228).
// The read stops at the first floor; only drops read the whole window.
inline constexpr int caveAbove = caveLayerSlack + 2, caveBelow = 128;
inline constexpr std::uint32_t caveWall = packColor(34, 34, 38), caveDeep = packColor(18, 18, 22);
// The world map's layer view dims a floor farther than this from the
// selected reference; broad view keeps every floor.
inline constexpr int caveFocusSlack = 8;
inline bool caveAtSelectedHeight(int floorY, int focusY) {
    return std::abs(floorY - focusY) <= caveFocusSlack;
}
// Image variants only need the slack's resolution: a FreeCamera moving one
// block must not rebuild every tile for an almost identical dim pattern
// (L-242). Nearest multiple of the slack, so the boundary moves at most half.
inline int caveFocusKey(int focus) {
    return floorDiv(focus + caveFocusSlack / 2, caveFocusSlack) * caveFocusSlack;
}
// A column's solid blocks from y = top downward (index 0 is top). Rock at
// the player's feet and head is a wall. Otherwise the floor is the first
// solid block below open space; open all the way down is a drop.
struct CaveHit {
    enum class Kind { Wall, Floor, Drop } kind;
    int y;
};
// Lazy form: the caller reads one block per call and can stop as soon as the
// floor is found. This keeps the deeper window cheap near ordinary floors.
template <class SolidAt>
CaveHit caveFloorFrom(SolidAt&& solidAt, int top, int playerY, int bottom) {
    if (solidAt(playerY) && solidAt(playerY + 1)) return {CaveHit::Kind::Wall, top};
    bool open = false, anyOpen = false;
    for (int y = top; y >= bottom; --y) {
        if (!solidAt(y)) { open = anyOpen = true; continue; }
        if (open) return {CaveHit::Kind::Floor, y};
    }
    if (!anyOpen) return {CaveHit::Kind::Wall, top};
    return {CaveHit::Kind::Drop, bottom};
}
inline CaveHit caveFloor(std::span<bool const> solid, int top, int playerY) {
    auto solidAt = [&](int y) {
        int i = top - y;
        return i >= 0 && i < static_cast<int>(solid.size()) && solid[static_cast<size_t>(i)];
    };
    return caveFloorFrom(solidAt, top, playerY, top - static_cast<int>(solid.size()) + 1);
}
// Floors near the player's height are bright, lower or higher ones dimmer;
// the block under the feet is at full brightness.
inline float caveBrightness(int y, int playerY) {
    return 1.f - .5f * std::min(1.f, std::abs(y - (playerY - 1)) / float(caveBelow));
}
// The column as the map shows it, given the floor block's own color.
inline Column caveColumn(CaveHit hit, int playerY, std::uint32_t floorColor) {
    if (hit.kind == CaveHit::Kind::Wall) return {caveWall, static_cast<std::int16_t>(hit.y)};
    if (hit.kind == CaveHit::Kind::Drop) return {caveDeep, static_cast<std::int16_t>(hit.y)};
    float k = caveBrightness(hit.y, playerY);
    return {tinted(floorColor ? floorColor : caveWall, k, k, k), static_cast<std::int16_t>(hit.y)};
}
}
