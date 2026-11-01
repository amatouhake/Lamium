#pragma once
#include "features/schematic/Verify.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

// The result of comparing the selected placement with the world (BACKLOG
// L-93), published by the world render and read by the screen and HUD.
namespace lamium::schematic {
struct Mismatch {
    CellState state = CellState::Missing;
    Point position;
    std::string expected, actual; // item icons as binary NBT ("" when there is none)
    std::string expectedName, actualName; // display names
    bool entity = false; // a missing entity rather than a block
    std::vector<StateDifference> states; // CellState::State: what differs
    std::string identifier; // the expected block's name (minecraft:oak_trapdoor), for naming its states
};
struct MaterialLine {
    std::string item;      // item name, e.g. minecraft:oak_stairs; empty when the block has no item
    std::string name;      // display name
    std::string icon;      // the item as binary NBT, for drawing its icon
    std::uint64_t needed = 0, placed = 0;
    bool entity = false; // an entity line; `item` is the item that places it, if one exists
    std::uint64_t remaining() const { return needed - std::min(needed, placed); }
};
struct Verification {
    std::uint64_t revision = 0; // the placement snapshot this belongs to
    int placement = -1;         // index of the selected placement, -1 when none is checked
    bool complete = false;      // a whole pass finished
    // The placement moved or its layers changed since: the rows are the last
    // pass's, kept on screen while the next counts, but no longer where the
    // ghosts are, so nothing points into the world from them (L-177).
    bool stale = false;
    Tally visible;              // counts within the shown layers
    std::vector<Mismatch> mismatches; // within the shown layers, nearest first, capped
    std::vector<MaterialLine> materials, visibleMaterials;
    bool ready(int selected) const { return selected >= 0 && placement == selected && complete && !stale; }
};
inline constexpr size_t maxMismatches = 2000;

// Mistakes before missing blocks, then by distance from `eye`.
inline void sortMismatches(std::vector<Mismatch>& list, double x, double y, double z) {
    auto distance = [&](Mismatch const& m) {
        double dx = m.position.x + .5 - x, dy = m.position.y + .5 - y, dz = m.position.z + .5 - z;
        return dx * dx + dy * dy + dz * dz;
    };
    std::stable_sort(list.begin(), list.end(), [&](Mismatch const& a, Mismatch const& b) {
        bool am = a.state == CellState::Missing, bm = b.state == CellState::Missing;
        if (am != bm) return !am;
        return distance(a) < distance(b);
    });
}
// Blocks before entities; then by remaining count, then name, so finished
// lines go last within each group.
inline void sortMaterials(std::vector<MaterialLine>& lines) {
    std::stable_sort(lines.begin(), lines.end(), [](MaterialLine const& a, MaterialLine const& b) {
        if (a.entity != b.entity) return !a.entity;
        if (a.remaining() != b.remaining()) return a.remaining() > b.remaining();
        return a.name < b.name;
    });
}
// How many items one placed block stands for: two for a double slab, none
// for the second half of a door or bed (the first half counts the item).
inline int itemsPerBlock(std::string_view blockName, bool secondHalf) {
    if (secondHalf) return 0;
    return blockName.ends_with("double_slab") ? 2 : 1;
}
// A liquid cell that is not a source (liquid_depth above 0) is filled by the
// game from a source, so it asks for no item of its own (L-289). A depth of 0
// is a source, and a plain liquid entry without the state counts as one.
inline bool flowingLiquid(PaletteBlock const& entry) {
    if (entry.name != "minecraft:water" && entry.name != "minecraft:flowing_water"
        && entry.name != "minecraft:lava" && entry.name != "minecraft:flowing_lava")
        return false;
    std::int64_t depth = 0;
    auto const* state = entry.states.find("liquid_depth");
    return state && state->integer(depth) && depth > 0;
}
// How many items one placed cell of `entry` asks for: a flowing liquid cell
// asks for none, every other cell for what its block does (L-289). The
// materials line stays as long as the schematic holds the liquid, so a pool
// whose cells all flow shows with nothing left to place.
inline int materialsCount(PaletteBlock const& entry, bool secondHalf) {
    return flowingLiquid(entry) ? 0 : itemsPerBlock(entry.name, secondHalf);
}
}
