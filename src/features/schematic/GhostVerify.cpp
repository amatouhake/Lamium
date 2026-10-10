#include "features/schematic/GhostVerify.h"
#include "features/schematic/GhostRenderer.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/world/actor/Actor.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/block/BlockType.h"
#include "mc/world/level/chunk/ChunkState.h"
#include "mc/world/level/chunk/LevelChunk.h"
#include "mc/world/level/material/Material.h"
#include "mc/world/phys/AABB.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <format>
#include <map>
#include <mutex>

namespace lamium::schematic::ghosts {
namespace {
// Entities are looked up this often, and only this close to the player: the
// client does not know entities beyond its tracking range.
constexpr std::chrono::milliseconds entityRefresh{250};
constexpr double entityRange = 48;
Clock::time_point entitiesChecked{};
// Verification of the selected placement, a bounded number of cells per
// frame; a finished pass is published and the next begins.
constexpr std::uint64_t scanBudget = 16384;
struct Cell {
    bool inside = false;      // false: outside the structure or a structure void
    CellState state = CellState::Unknown;
    int palette = -1;
    bool air = false, visible = false;
    Point world, offset;
    Block const* expected = nullptr;
    Block const* actual = nullptr;
    // The second layer: the file's liquid and the world's extra block (null: none).
    Block const* expectedLiquid = nullptr;
    Block const* actualLiquid = nullptr;
};
struct Scan {
    std::uint64_t revision = 0;
    std::string key; // drawKey of the placement being checked
    int placement = -1;
    std::uint64_t next = 0;
    Tally tally;
    std::vector<Mismatch> mismatches;
    // The nearest cells of each kind (missing, other mistakes), kept as
    // max-heaps on distance; turned into rows when the pass ends.
    std::vector<std::pair<double, Cell>> nearMissing, nearMistakes;
    std::map<std::string, MaterialLine> all, shown;
};
Scan scan;
std::mutex resultMutex;
std::shared_ptr<Verification const> published = std::make_shared<Verification const>();
void publish(std::shared_ptr<Verification const> value) {
    std::lock_guard lock(resultMutex);
    published = std::move(value);
}
}

void checkEntities(BlockSource& region, LocalPlayer& player, session::Snapshot const& snapshot, int dimension,
                   std::vector<Resolved>& resolved) {
    auto now = Clock::now();
    if (now - entitiesChecked < entityRefresh) return;
    entitiesChecked = now;
    Vec3 feet = player.getFeetPos();
    for (size_t i = 0; i < snapshot.placements.size() && i < resolved.size(); ++i) {
        auto const& shown = snapshot.placements[i];
        auto& r = resolved[i];
        std::fill(r.entityPlaced.begin(), r.entityPlaced.end(), std::nullopt);
        bool wanted = shown.placement.visible || static_cast<int>(i) == snapshot.selected;
        if (r.entities.empty() || !shown.structure || !shown.placement.entities || !wanted || shown.placement.dimension != dimension)
            continue;
        std::vector<EntitySpot> expected;
        std::vector<size_t> judged;
        double lowX = 1e18, lowY = 1e18, lowZ = 1e18, highX = -1e18, highY = -1e18, highZ = -1e18;
        for (size_t e = 0; e < r.entities.size(); ++e) {
            auto const& at = r.entities[e].at;
            double dx = at.x - feet.x, dy = at.y - feet.y, dz = at.z - feet.z;
            if (dx * dx + dy * dy + dz * dz > entityRange * entityRange) continue;
            BlockPos pos{static_cast<int>(std::floor(at.x)), static_cast<int>(std::floor(at.y)), static_cast<int>(std::floor(at.z))};
            auto* chunk = region.getChunkAt(pos);
            if (!chunk || chunk->mLoadState->load() < ChunkState::Loaded) continue;
            expected.push_back({r.entities[e].identifier, at.x, at.y, at.z});
            judged.push_back(e);
            lowX = std::min(lowX, at.x); lowY = std::min(lowY, at.y); lowZ = std::min(lowZ, at.z);
            highX = std::max(highX, at.x); highY = std::max(highY, at.y); highZ = std::max(highZ, at.z);
        }
        if (expected.empty()) continue;
        constexpr float grow = static_cast<float>(entityReach) + .5f;
        AABB area{Vec3{static_cast<float>(lowX) - grow, static_cast<float>(lowY) - grow, static_cast<float>(lowZ) - grow},
                  Vec3{static_cast<float>(highX) + grow, static_cast<float>(highY) + grow, static_cast<float>(highZ) + grow}};
        // Owned copies: the actors are not kept past this call.
        std::vector<std::string> names;
        std::vector<Vec3> positions;
        for (Actor* actor : region.fetchEntities(&player, area, false, false)) {
            if (!actor || actor->mRemoved || names.size() >= 4 * maxEntities) continue;
            names.push_back(actor->getTypeName());
            positions.push_back(actor->getFeetPos());
        }
        std::vector<EntitySpot> actual;
        for (size_t a = 0; a < names.size(); ++a) actual.push_back({names[a], positions[a].x, positions[a].y, positions[a].z});
        auto placed = matchEntities(expected, actual);
        for (size_t k = 0; k < judged.size(); ++k) r.entityPlaced[judged[k]] = placed[k];
    }
}
namespace {
// The schematic's entities in the finished pass: their own material lines
// and a "not placed" row for each one missing in the shown layers.
void addEntityResults(Verification& result, session::Shown const& shown, Resolved const& r) {
    if (!shown.placement.entities) return;
    Size placed = placedSize(shown.structure->size, shown.placement.placement.rotation);
    std::map<std::string, MaterialLine> all, visible;
    for (size_t e = 0; e < r.entities.size(); ++e) {
        auto const& ghost = r.entities[e];
        bool here = r.entityPlaced[e].value_or(false);
        bool inLayer = layerShown(shown.placement.layers, placed, ghost.offset);
        for (auto* lines : {&all, &visible}) {
            if (lines == &visible && !inLayer) continue;
            auto& line = (*lines)[ghost.identifier];
            if (line.name.empty()) { line.item = ghost.identifier; line.name = ghost.name; line.icon = ghost.icon; line.entity = true; }
            ++line.needed;
            if (here) ++line.placed;
        }
        if (inLayer && r.entityPlaced[e] == false && result.mismatches.size() < maxMismatches) {
            Point cell{static_cast<int>(std::floor(ghost.at.x)), static_cast<int>(std::floor(ghost.at.y)), static_cast<int>(std::floor(ghost.at.z))};
            result.mismatches.push_back({CellState::Missing, cell, ghost.icon, {}, ghost.name, {}, true});
        }
    }
    for (auto& [key, line] : all) result.materials.push_back(std::move(line));
    for (auto& [key, line] : visible) result.visibleMaterials.push_back(std::move(line));
}

// One cell of a placement, the n-th of its placed box (x, then y, then z
// fastest), compared with the world.
Cell classifyCell(BlockSource& region, session::Shown const& shown, Resolved const& blocks, Size placed, std::uint64_t n) {
    Cell c;
    auto const& structure = *shown.structure;
    auto const& placement = shown.placement;
    int ox = static_cast<int>(n / (static_cast<std::uint64_t>(placed.y) * placed.z));
    int oy = static_cast<int>(n / placed.z % placed.y);
    int oz = static_cast<int>(n % placed.z);
    c.offset = {ox, oy, oz};
    c.world = {placement.placement.origin.x + ox, placement.placement.origin.y + oy, placement.placement.origin.z + oz};
    auto local = toLocal(structure.size, placement.placement, c.world);
    if (!local) return c;
    auto paletteIndex = structure.blocks[static_cast<size_t>(structure.cell(local->x, local->y, local->z))];
    if (paletteIndex == voidCell || static_cast<size_t>(paletteIndex) >= blocks.blocks.size()) return c;
    c.inside = true;
    c.palette = static_cast<int>(paletteIndex);
    c.air = structure.palette[static_cast<size_t>(paletteIndex)].isAir();
    c.expected = blocks.blocks[static_cast<size_t>(paletteIndex)];
    c.visible = layerShown(placement.layers, placed, c.offset);
    BlockPos pos{c.world.x, c.world.y, c.world.z};
    auto* chunk = region.getChunkAt(pos);
    if (!chunk || chunk->mLoadState->load() < ChunkState::Loaded) { c.state = CellState::Unknown; return c; }
    c.actual = &region.getBlock(pos);
    auto const* actual = c.actual;
    auto const* expected = c.expected;
    auto cellIndex = static_cast<size_t>(structure.cell(local->x, local->y, local->z));
    if (cellIndex < structure.liquids.size())
        if (auto index = structure.liquids[cellIndex]; index != voidCell && static_cast<size_t>(index) < blocks.blocks.size())
            if (auto const* liquid = blocks.blocks[static_cast<size_t>(index)]; liquid && liquidKind(*liquid)) c.expectedLiquid = liquid;
    if (Block const& extra = region.getExtraBlock(pos); !extra.isAir()) c.actualLiquid = &extra;
    if (actual->getMaterial().mType == SharedTypes::v1_26_20::MaterialType::ClientRequestPlaceholder) c.state = CellState::Unknown;
    else if (c.air && c.expectedLiquid) {
        // Air with water in the second layer: water standing there.
        auto const* liquid = c.expectedLiquid;
        c.state = actual == liquid ? CellState::Correct : actual->isAir() ? CellState::Missing
            : &actual->getBlockType() == &liquid->getBlockType() ? CellState::State : CellState::Wrong;
        return c;
    }
    else if (c.air) c.state = actual->isAir() ? CellState::Correct : placement.countExtras ? CellState::Extra : CellState::Ignored;
    else if (!expected) c.state = CellState::Unknown;
    else if (actual == expected) c.state = CellState::Correct;
    else if (actual->isAir()) c.state = CellState::Missing;
    else c.state = &actual->getBlockType() == &expected->getBlockType() ? CellState::State : CellState::Wrong;
    if (!c.air) c.state = withLiquid(c.state, c.expectedLiquid == c.actualLiquid);
    return c;
}
// A classified cell as a row of the Check list.
Mismatch mismatchFor(Cell const& c, Resolved const& blocks) {
    Mismatch m{c.state, c.world, {}, {}, {}, {}};
    if (!c.air && c.palette >= 0) {
        auto const& info = blocks.items[static_cast<size_t>(c.palette)];
        m.expected = info.icon;
        m.expectedName = info.name;
    }
    if (c.actual && !c.actual->isAir()) {
        auto info = describe(*c.actual, c.actual->getTypeName());
        m.actual = info.icon;
        m.actualName = info.name;
        if (c.state == CellState::State && c.expected) {
            m.states = stateDifferences(blockStates(*c.expected), blockStates(*c.actual));
            if (m.states.empty() && c.expectedLiquid != c.actualLiquid)
                m.states.push_back(liquidDifference(c.expectedLiquid, c.actualLiquid));
            m.identifier = c.expected->getTypeName();
        }
    }
    return m;
}
// Progress of every placement for the Placed list: correct / total in its
// shown layers, counted in the background only while the list is on screen.
// The selected placement takes its numbers from the full check instead.
constexpr std::uint64_t progressBudget = 8192;
std::atomic<std::int64_t> progressWanted{0}; // steady-clock milliseconds of the last request
struct ProgressScan {
    size_t index = 0;
    std::string key;
    std::uint64_t next = 0;
    Tally tally;
} progressScan;
std::mutex progressMutex;
std::map<std::string, Tally> progressDone; // by drawKey
void recordProgress(std::string const& key, Tally const& tally) {
    std::lock_guard lock(progressMutex);
    progressDone[key] = tally;
}
std::int64_t steadyMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now().time_since_epoch()).count();
}
}

void stepProgress(BlockSource& region, session::Snapshot const& snapshot, int dimension, std::vector<Resolved> const& resolved) {
    if (steadyMs() - progressWanted.load() > 1000) return;
    size_t count = snapshot.placements.size();
    if (!count || resolved.size() != count) return;
    for (std::uint64_t budget = progressBudget, tries = 0; budget && tries <= count; ) {
        if (progressScan.index >= count) progressScan.index = 0;
        auto const& shown = snapshot.placements[progressScan.index];
        bool usable = static_cast<int>(progressScan.index) != snapshot.selected && shown.structure
            && shown.placement.dimension == dimension && resolved[progressScan.index].structure == shown.structure.get();
        auto key = drawKey(shown.placement);
        if (!usable) { ++progressScan.index; progressScan.key.clear(); ++tries; continue; }
        if (progressScan.key != key) { progressScan.key = key; progressScan.next = 0; progressScan.tally = {}; }
        Size placed = placedSize(shown.structure->size, shown.placement.placement.rotation);
        std::uint64_t total = static_cast<std::uint64_t>(placed.x) * placed.y * placed.z;
        for (; progressScan.next < total && budget; ++progressScan.next, --budget) {
            auto c = classifyCell(region, shown, resolved[progressScan.index], placed, progressScan.next);
            if (c.inside && c.visible) progressScan.tally.add(c.state, !c.air);
        }
        if (progressScan.next < total) return;
        recordProgress(key, progressScan.tally);
        progressScan.key.clear();
        ++progressScan.index;
        ++tries;
    }
}
void stepScan(BlockSource& region, session::Snapshot const& snapshot, int dimension, Vec3 const& camera,
              std::vector<Resolved> const& resolved) {
    int index = snapshot.selected;
    bool valid = index >= 0 && index < static_cast<int>(snapshot.placements.size())
        && snapshot.placements[static_cast<size_t>(index)].structure
        && snapshot.placements[static_cast<size_t>(index)].placement.dimension == dimension;
    if (!valid) {
        if (scan.placement != -1) {
            scan = {};
            scan.revision = snapshot.revision;
            auto none = std::make_shared<Verification>();
            none->revision = snapshot.revision;
            publish(std::move(none));
        }
        return;
    }
    // A change to another placement, or to this one's name or visibility,
    // keeps the pass going.
    auto key = drawKey(snapshot.placements[static_cast<size_t>(index)].placement)
        + std::format("|{}", static_cast<void const*>(snapshot.placements[static_cast<size_t>(index)].structure.get()));
    if (scan.key != key) {
        scan = {};
        scan.key = key;
    }
    scan.revision = snapshot.revision;
    scan.placement = index;
    auto const& shown = snapshot.placements[static_cast<size_t>(index)];
    auto const& structure = *shown.structure;
    auto const& placement = shown.placement;
    auto const& blocks = resolved[static_cast<size_t>(index)];
    Size placed = placedSize(structure.size, placement.placement.rotation);
    std::uint64_t total = static_cast<std::uint64_t>(placed.x) * placed.y * placed.z;
    auto addMaterial = [](std::map<std::string, MaterialLine>& lines, ItemInfo const& info, bool correct) {
        auto key = info.item.empty() ? "block:" + info.name : info.item;
        auto& line = lines[key];
        if (line.name.empty()) { line.item = info.item; line.name = info.name; line.icon = info.icon; }
        line.needed += static_cast<std::uint64_t>(info.perBlock);
        if (correct) line.placed += static_cast<std::uint64_t>(info.perBlock);
    };
    for (std::uint64_t budget = scanBudget; scan.next < total && budget; ++scan.next, --budget) {
        auto c = classifyCell(region, shown, blocks, placed, scan.next);
        if (!c.inside) continue;
        auto paletteIndex = c.palette;
        bool visible = c.visible, air = c.air;
        CellState state = c.state;
        Block const* actual = c.actual;
        Block const* expected = c.expected;
        Point world = c.world;
        if (!air) {
            auto const& info = blocks.items[static_cast<size_t>(paletteIndex)];
            addMaterial(scan.all, info, state == CellState::Correct);
            if (visible) addMaterial(scan.shown, info, state == CellState::Correct);
        }
        if (!visible) continue;
        scan.tally.add(state, !air);
        bool mistake = state == CellState::Missing || state == CellState::Wrong || state == CellState::State || state == CellState::Extra;
        if (!mistake) continue;
        // Capped per kind and nearest first: in a large build the missing
        // blocks alone filled the list and the wrong ones never showed.
        auto& near = state == CellState::Missing ? scan.nearMissing : scan.nearMistakes;
        double dx = world.x + .5 - camera.x, dy = world.y + .5 - camera.y, dz = world.z + .5 - camera.z;
        double distance = dx * dx + dy * dy + dz * dz;
        auto farther = [](auto const& a, auto const& b) { return a.first < b.first; };
        if (near.size() < maxMismatches) { near.push_back({distance, c}); std::push_heap(near.begin(), near.end(), farther); }
        else if (distance < near.front().first) {
            std::pop_heap(near.begin(), near.end(), farther);
            near.back() = {distance, c};
            std::push_heap(near.begin(), near.end(), farther);
        }
    }
    if (scan.next < total) return;
    auto result = std::make_shared<Verification>();
    result->revision = scan.revision;
    result->placement = index;
    result->complete = true;
    result->visible = scan.tally;
    recordProgress(drawKey(placement), scan.tally);
    for (auto* near : {&scan.nearMistakes, &scan.nearMissing})
        for (auto const& [distance, cell] : *near) scan.mismatches.push_back(mismatchFor(cell, blocks));
    result->mismatches = std::move(scan.mismatches);
    for (auto& [key, line] : scan.all) result->materials.push_back(std::move(line));
    for (auto& [key, line] : scan.shown) result->visibleMaterials.push_back(std::move(line));
    addEntityResults(*result, shown, blocks);
    sortMismatches(result->mismatches, camera.x, camera.y, camera.z);
    sortMaterials(result->materials);
    sortMaterials(result->visibleMaterials);
    publish(std::move(result));
    // Start the next pass.
    scan.next = 0;
    scan.tally = {};
    scan.mismatches.clear();
    scan.nearMissing.clear();
    scan.nearMistakes.clear();
    scan.all.clear();
    scan.shown.clear();
}

void wantProgress() { progressWanted = steadyMs(); }
std::optional<Tally> progress(SavedPlacement const& placement) {
    std::lock_guard lock(progressMutex);
    auto found = progressDone.find(drawKey(placement));
    if (found == progressDone.end()) return std::nullopt;
    return found->second;
}
std::shared_ptr<Verification const> verification() {
    std::lock_guard lock(resultMutex);
    return published;
}
std::optional<Mismatch> checkedCell(BlockSource& region, Point world, std::vector<Resolved> const& resolved) {
    std::shared_ptr<Verification const> result;
    {
        std::lock_guard lock(resultMutex);
        result = published;
    }
    auto snapshot = session::snapshot();
    int index = result->placement;
    if (index < 0 || static_cast<size_t>(index) >= snapshot.placements.size() || static_cast<size_t>(index) >= resolved.size()) return std::nullopt;
    auto const& shown = snapshot.placements[static_cast<size_t>(index)];
    if (!shown.structure || resolved[static_cast<size_t>(index)].structure != shown.structure.get()) return std::nullopt;
    Size placed = placedSize(shown.structure->size, shown.placement.placement.rotation);
    auto const& o = shown.placement.placement.origin;
    int ox = world.x - o.x, oy = world.y - o.y, oz = world.z - o.z;
    if (ox < 0 || oy < 0 || oz < 0 || ox >= placed.x || oy >= placed.y || oz >= placed.z) return std::nullopt;
    auto n = (static_cast<std::uint64_t>(ox) * placed.y + static_cast<std::uint64_t>(oy)) * placed.z + static_cast<std::uint64_t>(oz);
    auto c = classifyCell(region, shown, resolved[static_cast<size_t>(index)], placed, n);
    if (!c.inside) return std::nullopt;
    return mismatchFor(c, resolved[static_cast<size_t>(index)]);
}
void resetVerification() {
    entitiesChecked = {};
    scan = {};
    publish(std::make_shared<Verification const>());
}
}
