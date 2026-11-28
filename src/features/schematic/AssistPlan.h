#pragma once
// Placement assist (L-265, docs/PLACEMENT-ASSIST.md): the pure part. Which
// clicks and looks to try for a ghost cell, in which order, whether a
// predicted block counts as the ghost's, and the faked look's auth input
// values. The game glue (PlacementAssist.cpp) asks the block's own
// placement rule for each candidate and sends the first that matches.
#include <array>
#include <cmath>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace lamium::schematic::assist {
// Faces as the game numbers them: 0 down, 1 up, 2 north (-z), 3 south (+z),
// 4 west (-x), 5 east (+x).
inline constexpr std::array<std::array<int, 3>, 6> offsets{{{0, -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}, {-1, 0, 0}, {1, 0, 0}}};
inline constexpr unsigned char opposite(unsigned char face) { return static_cast<unsigned char>(face ^ 1); }

struct Look {
    float pitch = 0, yaw = 0;
    bool faked = false;
};
// One way to place into the cell: click the neighbor in direction `dir` from
// the cell on its face `face` (which looks at the cell), at `click` relative
// to that neighbor, while looking `look`.
struct Candidate {
    unsigned char dir = 0, face = 1;
    std::array<float, 3> click{};
    Look look;
};

// The point on `face` of a block, relative to the block; side faces at
// height `y` (0..1), top and bottom at their middle.
inline std::array<float, 3> clickOn(unsigned char face, float y) {
    std::array<float, 3> at{0.5f, y, 0.5f};
    switch (face) {
    case 0: at[1] = 0; break;
    case 1: at[1] = 1; break;
    case 2: at[2] = 0; break;
    case 3: at[2] = 1; break;
    case 4: at[0] = 0; break;
    default: at[0] = 1; break;
    }
    return at;
}
inline float wrapYaw(float yaw) {
    yaw = std::fmod(yaw, 360.0f);
    if (yaw >= 180.0f) yaw -= 360.0f;
    if (yaw < -180.0f) yaw += 360.0f;
    return yaw;
}
// The looks to try: the player's own first (nothing faked), then the four
// horizontal facings level, then each facing looking steeply down and up
// (blocks that face up or down follow the pitch).
inline std::vector<Look> looks(float pitch, float yaw) {
    std::vector<Look> out{{pitch, yaw, false}};
    for (float p : {0.0f, 80.0f, -80.0f})
        for (float y : {0.0f, 90.0f, 180.0f, -90.0f}) out.push_back({p, wrapYaw(y), true});
    return out;
}
// Neighbors in this order: below first (placing on the ground, as most
// building goes), then the four sides, then above.
inline constexpr std::array<unsigned char, 6> neighborOrder{0, 2, 3, 4, 5, 1};
// Every candidate for a cell whose neighbor in direction d can be clicked
// when clickable[d]. Unfaked looks come first, so a placement the player's
// own look already makes sends nothing extra.
inline std::vector<Candidate> candidates(std::array<bool, 6> const& clickable, float pitch, float yaw) {
    std::vector<Candidate> out;
    for (Look const& look : looks(pitch, yaw))
        for (unsigned char dir : neighborOrder) {
            if (!clickable[dir]) continue;
            unsigned char const face = opposite(dir);
            if (face <= 1) out.push_back({dir, face, clickOn(face, 0.5f), look});
            else
                for (float y : {0.25f, 0.75f}) out.push_back({dir, face, clickOn(face, y), look});
        }
    return out;
}

using States = std::map<std::string, std::string>;
// States a placement does not decide: the neighbors set them (a stair's
// corner, a wall's connections), or later use and redstone do (open,
// powered, signal, growth). A ghost differing only in these still counts as
// placed; the Check judges them as usual.
inline bool derivedState(std::string_view name) {
    static constexpr std::string_view names[] = {
        "minecraft:corner", "wall_post_bit", "wall_connection_type_east", "wall_connection_type_north",
        "wall_connection_type_south", "wall_connection_type_west", "redstone_signal", "powered_bit", "open_bit",
        "in_wall_bit", "toggle_bit", "output_lit_bit", "output_subtract_bit", "age", "growth", "age_bit",
        "attached_bit", "disarmed_bit", "suspended_bit", "button_pressed_bit", "occupied_bit",
        // A fence's connections follow its neighbors (2026-11-28 run).
        "minecraft:connection_east", "minecraft:connection_north", "minecraft:connection_south", "minecraft:connection_west"};
    for (auto n : names)
        if (n == name) return true;
    return false;
}
// Whether `predicted` (what the placement would make) counts as the ghost's
// block: the same block, and every state the placement decides equal.
inline bool placesAs(std::string_view predictedName, States const& predicted, std::string_view ghostName, States const& ghost) {
    if (predictedName != ghostName) return false;
    for (auto const& [name, value] : ghost) {
        if (derivedState(name)) continue;
        auto found = predicted.find(name);
        if (found == predicted.end() || found->second != value) return false;
    }
    for (auto const& [name, value] : predicted)
        if (!derivedState(name) && !ghost.contains(name)) return false;
    return true;
}

// The first state that keeps `predicted` from counting as the ghost's, for
// the log when nothing places it (so the derived list can grow); empty when
// the blocks differ or nothing does.
inline std::string firstDifference(States const& predicted, States const& ghost) {
    for (auto const& [name, value] : ghost) {
        if (derivedState(name)) continue;
        auto found = predicted.find(name);
        if (found == predicted.end()) return name + " missing";
        if (found->second != value) return name + " " + found->second + " not " + value;
    }
    for (auto const& [name, value] : predicted)
        if (!derivedState(name) && !ghost.contains(name)) return name + " not in the ghost";
    return {};
}

// The faked look's auth input values (L-265 traces: the move input turned
// with the look leaves the movement as it was). `strafe` is positive to the
// left, as the game's move vector; yaw in degrees, 0 facing +z.
struct Move {
    float strafe = 0, forward = 0;
};
inline constexpr float degrees = 3.14159265358979f / 180.0f;
inline Move turnedMove(Move move, float yaw, float newYaw) {
    float const a = yaw * degrees, b = newYaw * degrees;
    // World direction: forward along (-sin a, cos a), left along (cos a, sin a).
    float const wx = -std::sin(a) * move.forward + std::cos(a) * move.strafe;
    float const wz = std::cos(a) * move.forward + std::sin(a) * move.strafe;
    return {wx * std::cos(b) + wz * std::sin(b), -wx * std::sin(b) + wz * std::cos(b)};
}
// The direction keys a move vector reads as (forward, back, left, right).
struct Keys {
    bool up = false, down = false, left = false, right = false;
};
inline Keys keysOf(Move move) {
    constexpr float threshold = 0.3f;
    return {move.forward > threshold, move.forward < -threshold, move.strafe > threshold, move.strafe < -threshold};
}
// The unit view vector of a look, as the auth input's camera orientation.
inline std::array<float, 3> viewOf(float pitch, float yaw) {
    float const p = pitch * degrees, y = yaw * degrees;
    return {-std::sin(y) * std::cos(p), -std::sin(p), std::cos(y) * std::cos(p)};
}
}
