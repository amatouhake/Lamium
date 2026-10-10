#pragma once
#include "features/schematic/Placement.h"
#include <cmath>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// Comparing a placed schematic with what the client sees, and counting what
// is left to place (BACKLOG L-93). Blocks are identified by name and by the
// state key from PaletteBlock::key(); reading them from the world is glue.
namespace lamium::schematic {
enum class CellState : std::uint8_t {
    Ignored,  // structure void, or an extra block while extras are ignored
    Unknown,  // chunk not loaded; never counted as placed or wrong
    Correct,
    Missing,  // nothing (air) where a block belongs
    Wrong,    // a different block
    State,    // the right block with other states (facing, half...)
    Extra,    // a block where the schematic has air
};
struct WorldBlock {
    std::string_view name;
    std::string_view key;
    bool air() const { return name == "minecraft:air"; }
};
// `expected` is null for structure void. `world` is null when not loaded.
inline CellState classify(PaletteBlock const* expected, std::string_view expectedKey,
                          std::optional<WorldBlock> world, bool countExtras) {
    if (!expected) return CellState::Ignored;
    if (!world) return CellState::Unknown;
    if (expected->isAir()) {
        if (world->air()) return CellState::Correct;
        return countExtras ? CellState::Extra : CellState::Ignored;
    }
    if (world->air()) return CellState::Missing;
    if (world->name != expected->name) return CellState::Wrong;
    return world->key == expectedKey ? CellState::Correct : CellState::State;
}

// The second layer (water in a waterlogged block), judged once the block
// itself is right: the file's liquid there and the world's must be the same
// (none on both sides included). Any difference is a state mistake, like
// any other differing state (strict check, decided 2026-10-09).
inline CellState withLiquid(CellState block, bool liquidMatches) {
    return block == CellState::Correct && !liquidMatches ? CellState::State : block;
}

// What the client reads at a placement's cell, compared by the glue: `same`
// and `sameType` against the expected block, or against the file's liquid
// where the file has air with a liquid in the second layer.
struct CellReading {
    bool placeholder = false; // the client's stand-in block while a chunk arrives
    bool expectsAir = false, expectsLiquid = false;
    bool known = true;        // the expected block is a known game block
    bool same = false, sameType = false, actualAir = false;
    bool liquidsMatch = true; // the file's liquid and the world's are the same (none on both included)
};
// A loaded cell's state. Air with a liquid in the second layer is water
// standing there, judged against the liquid; any other block is judged
// first, then its waterlogging (withLiquid).
inline CellState classifyReading(CellReading const& r, bool countExtras) {
    CellState state;
    if (r.placeholder) state = CellState::Unknown;
    else if (r.expectsAir && r.expectsLiquid)
        return r.same ? CellState::Correct : r.actualAir ? CellState::Missing : r.sameType ? CellState::State : CellState::Wrong;
    else if (r.expectsAir) state = r.actualAir ? CellState::Correct : countExtras ? CellState::Extra : CellState::Ignored;
    else if (!r.known) state = CellState::Unknown;
    else if (r.same) state = CellState::Correct;
    else if (r.actualAir) state = CellState::Missing;
    else state = r.sameType ? CellState::State : CellState::Wrong;
    return r.expectsAir ? state : withLiquid(state, r.liquidsMatch);
}

// For a block of the right kind in the wrong state: which states differ, in
// key order, at most `limit` of them. A state only one side has shows "-".
struct StateDifference {
    std::string key, expected, actual;
    bool operator==(StateDifference const&) const = default;
};
inline std::vector<StateDifference> stateDifferences(std::map<std::string, std::string> const& expected,
                                                     std::map<std::string, std::string> const& actual,
                                                     size_t limit = 3) {
    std::vector<StateDifference> result;
    auto e = expected.begin(), a = actual.begin();
    while ((e != expected.end() || a != actual.end()) && result.size() < limit) {
        if (a == actual.end() || (e != expected.end() && e->first < a->first)) {
            result.push_back({e->first, e->second, "-"}); ++e;
        } else if (e == expected.end() || a->first < e->first) {
            result.push_back({a->first, "-", a->second}); ++a;
        } else {
            if (e->second != a->second) result.push_back({e->first, e->second, a->second});
            ++e; ++a;
        }
    }
    return result;
}

// The row naming a waterlogging difference in a mistake's state list.
inline StateDifference liquidDifference(bool expected, bool actual) {
    return {"waterlogged", expected ? "true" : "false", actual ? "true" : "false"};
}

struct Tally {
    std::uint64_t correct = 0, missing = 0, wrong = 0, state = 0, extra = 0, unknown = 0;
    // Blocks the schematic asks for (air excluded): what "correct / total" counts.
    std::uint64_t total() const { return correct + missing + wrong + state + unknown; }
    std::uint64_t mistakes() const { return wrong + state + extra; }
    void add(CellState state, bool expectsBlock) {
        switch (state) {
        case CellState::Correct: if (expectsBlock) ++correct; break;
        case CellState::Missing: ++missing; break;
        case CellState::Wrong: ++wrong; break;
        case CellState::State: ++this->state; break;
        case CellState::Extra: ++extra; break;
        case CellState::Unknown: if (expectsBlock) ++unknown; break;
        case CellState::Ignored: break;
        }
    }
};

// Entities are checked by type and place only: one of the same type standing
// at the spot counts, and each entity in the world counts for one spot.
// "At" is within half a block across (a neighbor on the next block is
// another spot) and a block up or down (slabs, carpets).
struct EntitySpot {
    std::string_view identifier;
    double x = 0, y = 0, z = 0;
};
inline constexpr double entityReach = 1.0, entityReachAcross = .5;
inline std::vector<bool> matchEntities(std::span<EntitySpot const> expected, std::span<EntitySpot const> actual,
                                       double reach = entityReach) {
    std::vector<bool> placed(expected.size()), used(actual.size());
    for (size_t i = 0; i < expected.size(); ++i) {
        auto const& want = expected[i];
        size_t best = actual.size();
        double bestDistance = entityReachAcross * entityReachAcross;
        for (size_t j = 0; j < actual.size(); ++j) {
            auto const& have = actual[j];
            if (used[j] || have.identifier != want.identifier || std::abs(have.y - want.y) > reach) continue;
            double dx = have.x - want.x, dz = have.z - want.z, distance = dx * dx + dz * dz;
            if (distance <= bestDistance) { bestDistance = distance; best = j; }
        }
        if (best < actual.size()) { used[best] = true; placed[i] = true; }
    }
    return placed;
}
// The language key of an entity's name: vanilla keys leave out "minecraft:".
inline std::string entityNameKey(std::string_view identifier) {
    if (identifier.starts_with("minecraft:")) identifier.remove_prefix(10);
    return "entity." + std::string(identifier) + ".name";
}

// Per palette block name: how many the schematic needs and how many are placed.
struct Material {
    std::uint64_t needed = 0, placed = 0;
    std::uint64_t remaining() const { return needed - placed; }
};
using Materials = std::map<std::string, Material>;
inline void addMaterial(Materials& materials, PaletteBlock const& expected, CellState state) {
    if (expected.isAir()) return;
    auto& material = materials[expected.name];
    ++material.needed;
    if (state == CellState::Correct) ++material.placed;
}
}
