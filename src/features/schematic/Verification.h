#pragma once
#include "features/schematic/LiquidName.h"
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
    // What an ignore entry (L-183) names: the structure cell and the file's
    // block there (minecraft:air for an extra); local.x is -1 for an entity.
    Point local{-1, -1, -1};
    std::string fileBlock;
    // L-183: ignored rows stay listed in their own kind and count as the
    // entries say (countedState); `states` then holds every difference,
    // each marked when a rule covers it.
    IgnoreMark ignore = IgnoreMark::None;
    bool statesCut = false; // `states` lists only the first of the differences the pass judged
};
// The Check list's filters (L-183): all rows (mistakes first, then not
// placed), wrong block (extras too), wrong state, not placed.
enum class CheckFilter { All, WrongBlock, WrongState, NotPlaced };
inline bool listedIn(CheckFilter filter, CellState state) {
    switch (filter) {
    case CheckFilter::WrongBlock: return state == CellState::Wrong || state == CellState::Extra;
    case CheckFilter::WrongState: return state == CellState::State;
    case CheckFilter::NotPlaced: return state == CellState::Missing;
    case CheckFilter::All: break;
    }
    return true;
}
// A row the counts, the HUD's nearest mistake and the nearest-mistake key
// treat as a mistake: wrong, extra or wrong state, not ignored. Every reader
// of the rows asks this, so an ignored difference is never one of them.
inline bool countsAsMistake(Mismatch const& m) {
    return m.ignore == IgnoreMark::None
        && (m.state == CellState::Wrong || m.state == CellState::Extra || m.state == CellState::State);
}
// The mistake nearest to (x, y, z), with its squared distance; null when none.
inline Mismatch const* nearestMistake(std::vector<Mismatch> const& rows, double x, double y, double z,
                                      double* distanceSquared = nullptr) {
    Mismatch const* nearest = nullptr;
    double best = 0;
    for (auto const& m : rows) {
        if (!countsAsMistake(m)) continue;
        double dx = m.position.x + .5 - x, dy = m.position.y + .5 - y, dz = m.position.z + .5 - z;
        double d = dx * dx + dy * dy + dz * dz;
        if (!nearest || d < best) { best = d; nearest = &m; }
    }
    if (distanceSquared) *distanceSquared = best;
    return nearest;
}
struct MaterialLine {
    std::string item;      // item name, e.g. minecraft:oak_stairs; empty when the block has no item
    std::string name;      // display name
    std::string icon;      // the item as binary NBT, for drawing its icon
    std::uint64_t needed = 0, placed = 0;
    bool entity = false; // an entity line; `item` is the item that places it (Verify.h entityPlacer), empty for a mob
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
    // L-183: the ignore entries changed since. The rows' cells are still where
    // the ghosts are, but their verdicts and the counts are the last pass's:
    // the counts, the HUD and the nearest-mistake key wait for the next one.
    bool outdated = false;
    Tally visible;              // counts within the shown layers
    std::vector<Mismatch> mismatches; // within the shown layers, nearest first, capped
    std::vector<MaterialLine> materials, visibleMaterials;
    bool ready(int selected) const { return selected >= 0 && placement == selected && complete && !stale && !outdated; }
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
// What is still to place, item by item, as the plan takes it (L-359): a line
// without an item (a block nothing places) is left out, lines of one item add up.
inline std::vector<std::pair<std::string, std::uint64_t>> planTargets(std::vector<MaterialLine> const& lines) {
    std::vector<std::pair<std::string, std::uint64_t>> out;
    for (auto const& line : lines) {
        if (line.item.empty() || !line.remaining()) continue;
        // Water and lava are placed, not gathered or made (L-391): they stay in the Materials tab only.
        std::string_view name = line.item;
        if (name.starts_with("minecraft:")) name.remove_prefix(10);
        if (name == "water" || name == "lava" || name == "flowing_water" || name == "flowing_lava") continue;
        auto found = std::find_if(out.begin(), out.end(), [&](auto const& target) { return target.first == line.item; });
        if (found == out.end()) out.emplace_back(line.item, line.remaining()); else found->second += line.remaining();
    }
    return out;
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
    if (!isLiquidName(entry.name)) return false;
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
