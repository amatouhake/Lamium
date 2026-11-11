#include "features/visuals/BlockHighlightHooks.h"
#include "app/Capabilities.h"
#include "app/Runtime.h"
#include "app/Versions.h"
#include "ll/api/memory/Hook.h"
#include "mc/client/renderer/Tessellator.h"
#include "mc/client/renderer/block/BlockTessellator.h"
#include "mc/client/renderer/chunks/RenderChunkBuilder.h"
#include "mc/client/renderer/chunks/RenderChunkCoordinator.h"
#include "mc/deps/minecraft_renderer/renderer/MeshData.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/block/BlockType.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/phys/AABB.h"
#include <algorithm>
#include <atomic>
#include <array>
#include <bit>
#include <optional>
#include <cmath>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace lamium::visuals::highlight {
namespace {
// The designated type names chunk-build threads look for. Replaced, never
// changed, on the client thread; builders take a copy when `generation` moves.
struct Table {
    std::unordered_set<std::string> names;
};
std::mutex tableLock;
std::shared_ptr<Table const> table;
std::atomic<unsigned> generation{0};
bool installed = false;

std::shared_ptr<Table const> currentTable() {
    thread_local unsigned seen = ~0u;
    thread_local std::shared_ptr<Table const> cached;
    unsigned now = generation.load(std::memory_order_acquire);
    if (now != seen) {
        std::lock_guard lock(tableLock);
        cached = table;
        seen = now;
    }
    return cached;
}

// Marks per chunk section of a dimension, replaced whole each time the
// section is built. Written by build threads, read by the render thread.
using SectionKey = std::tuple<int, int, int, int>; // dimension, section x, y, z
std::mutex markLock;
std::map<SectionKey, std::vector<Mark>> sections;
std::uint64_t markRevision = 1;
// The section being built on this thread, if any: its marks and the first
// block tessellated, which names the section (L-320: the builder's bounding
// box gave keys that let other builds erase a section's marks).
struct Capture {
    std::vector<Mark> marks;
    std::optional<BlockPos> first;
    int dimension = 0;
};
thread_local Capture* capture = nullptr;
std::atomic<int> faceSamples{0};
// The game facing order: down, up, north, south, west, east.
constexpr std::array<std::array<int, 3>, 6> facingOffset{{{0, -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}, {-1, 0, 0}, {1, 0, 0}}};
// Faces not covered by an opaque full block. The `faces` argument of the
// tessellator is not this (it read all six while some blocks added no quad).
std::uint8_t exposedFaces(BlockSource const& region, BlockPos const& pos) {
    std::uint8_t bits = 0;
    for (size_t f = 0; f < facingOffset.size(); ++f) {
        auto const& o = facingOffset[f];
        auto const& next = region.getBlock({pos.x + o[0], pos.y + o[1], pos.z + o[2]});
        if (!next.getBlockType().mIsOpaqueFullBlock) bits |= static_cast<std::uint8_t>(1u << f);
    }
    return bits;
}

// One block's tessellation: names the section by its first block and marks
// a designated block that added vertices.
template <class Draw>
bool tessellateMarked(BlockSource const* region, Tessellator& tessellator, Block const& block, BlockPos const& pos, Draw&& draw) {
    if (capture && !capture->first) {
        capture->first = pos;
        try { if (region) capture->dimension = static_cast<int>(region->getDimensionId()); } catch (...) {}
    }
    bool wanted = false;
    try {
        if (auto current = currentTable(); current && !current->names.empty()) wanted = current->names.contains(block.getTypeName());
    } catch (...) {}
    if (!wanted) return draw();
    size_t before = tessellator.mMeshData->mPositions->size();
    bool result = draw();
    try {
        size_t added = tessellator.mMeshData->mPositions->size() - before;
        // A block that added no quad is hidden; nothing to glow.
        std::uint8_t bits = added && region ? exposedFaces(*region, pos) : 0;
        if (faceSamples.fetch_add(1, std::memory_order_relaxed) < 6)
            Runtime::instance().self().getLogger().debug("Block highlight: exposed faces {:06b} ({} set), {} vertices added",
                bits, std::popcount(bits), added);
        if (capture && bits) capture->marks.push_back({{pos.x, pos.y, pos.z}, bits});
    } catch (...) {}
    return result;
}
// Set while the shape dispatcher below runs on this thread: the cube calls
// it makes (a wall's boxes) belong to the block it is already marking.
thread_local int dispatching = 0;

// Full cubes; the chunk builder also calls this without the dispatcher.
LL_TYPE_INSTANCE_HOOK(HighlightBlock, ll::memory::HookPriority::Normal, BlockTessellator,
    &BlockTessellator::tessellateBlockInWorld, bool, Tessellator& tessellator, Block const& block, BlockPos const& pos,
    std::bitset<6> const faces, AirAndSimpleBlockBits const* simple) {
    if (dispatching) return origin(tessellator, block, pos, faces, simple);
    return tessellateMarked(mRegion, tessellator, block, pos, [&] { return origin(tessellator, block, pos, faces, simple); });
}
// Every other shape (cactus, panes, stairs): the dispatcher that picks the
// shape's own tessellation.
LL_TYPE_INSTANCE_HOOK(HighlightShape, ll::memory::HookPriority::Normal, BlockTessellator,
    &BlockTessellator::tessellateInWorld, bool, Tessellator& tessellator, Block const& block, BlockPos const& pos,
    bool useCalcWithCache) {
    return tessellateMarked(mRegion, tessellator, block, pos, [&] {
        ++dispatching;
        struct Leave { ~Leave() { --dispatching; } } leave;
        return origin(tessellator, block, pos, useCalcWithCache);
    });
}

LL_TYPE_INSTANCE_HOOK(HighlightSection, ll::memory::HookPriority::Normal, RenderChunkBuilder,
    &RenderChunkBuilder::build, void, RenderChunkGeometry& geometry, bool transparentLeaves, BakedBlockLightType lighting,
    bool forExport, mce::framebuilder::FrameLightingModelCapabilities const& capabilities) {
    if (forExport || capture) return origin(geometry, transparentLeaves, lighting, forExport, capabilities);
    Capture build;
    capture = &build;
    struct Reset { ~Reset() { capture = nullptr; } } reset;
    origin(geometry, transparentLeaves, lighting, forExport, capabilities);
    try {
        // A section with no block drawn keeps what it had; pruning drops
        // marks whose block is gone.
        if (!build.first) return;
        auto section = [](int v) { return v >= 0 ? v / 16 : -((-v + 15) / 16); };
        SectionKey key{build.dimension, section(build.first->x), section(build.first->y), section(build.first->z)};
        auto& found = build.marks;
        // Every mark must lie in the section that names it.
        auto dropped = std::erase_if(found, [&](Mark const& m) {
            return SectionKey{build.dimension, section(m.cell.x), section(m.cell.y), section(m.cell.z)} != key;
        });
        static std::atomic<int> droppedReports{0};
        if (dropped && droppedReports.fetch_add(1, std::memory_order_relaxed) < 3)
            Runtime::instance().self().getLogger().warn("Block highlight: a build spanned more than one section ({} marks dropped)", dropped);
        std::lock_guard lock(markLock);
        auto at = sections.find(key);
        if (found.empty()) {
            if (at == sections.end()) return;
            sections.erase(at);
        } else if (at == sections.end()) sections.emplace(key, std::move(found));
        else if (at->second != found) at->second = std::move(found);
        else return;
        ++markRevision;
    } catch (...) {}
}

// Marks are recorded as sections build, so each dimension's coordinator
// rebuilds its chunks when the wanted list differs from the one it last
// built with, and once when first seen while something is wanted.
// Client thread only.
std::vector<Entry> published;
// The settings snapshot the wanted list was last read from; every settings
// change publishes a new one, so an unchanged pointer means nothing to copy.
std::shared_ptr<Settings const> wantedFrom;
std::vector<Entry> wanted;
std::vector<Entry> const& wantedList() {
    auto& runtime = Runtime::instance();
    if (!capabilities::registry().status(capabilities::Id::BlockHighlight).available() || !runtime.enabled()) {
        wantedFrom.reset();
        wanted.clear();
        return wanted;
    }
    auto settings = runtime.snapshot();
    if (settings != wantedFrom) {
        wantedFrom = settings;
        if (settings && settings->visuals.blockHighlight) {
            if (settings->visuals.highlightBlocks != wanted) wanted = settings->visuals.highlightBlocks;
        } else wanted.clear();
    }
    return wanted;
}
std::unordered_map<RenderChunkCoordinator const*, unsigned> built;
void publish(std::vector<Entry> const& entries) {
    auto next = std::make_shared<Table>();
    for (auto const& e : entries) next->names.insert(e.name);
    {
        std::lock_guard lock(tableLock);
        table = next->names.empty() ? nullptr : std::shared_ptr<Table const>(std::move(next));
    }
    published = entries;
    generation.fetch_add(1, std::memory_order_release);
    // Marks of the old list go; the rebuild records the new ones.
    clearMarks();
}
LL_TYPE_INSTANCE_HOOK(HighlightRebuild, ll::memory::HookPriority::Normal, RenderChunkCoordinator,
    &RenderChunkCoordinator::tick, void) {
    origin();
    try {
        if (auto const& want = wantedList(); want != published) publish(want);
        unsigned now = generation.load(std::memory_order_relaxed);
        auto [state, first] = built.try_emplace(this, now);
        if (first ? !published.empty() : state->second != now) {
            state->second = now;
            _setAllDirty(false, false);
        }
    } catch (...) {}
}
struct Hook { int (*install)(bool); bool (*remove)(bool); bool installed = false; };
Hook hooks[] = {{HighlightBlock::hook, HighlightBlock::unhook}, {HighlightSection::hook, HighlightSection::unhook},
    {HighlightRebuild::hook, HighlightRebuild::unhook}, {HighlightShape::hook, HighlightShape::unhook}};
}
bool marks(int dimension, std::uint64_t& revision, std::vector<Mark>& out) {
    std::lock_guard lock(markLock);
    if (revision == markRevision) return false;
    revision = markRevision;
    out.clear();
    for (auto const& [key, list] : sections)
        if (std::get<0>(key) == dimension) out.insert(out.end(), list.begin(), list.end());
    return true;
}
// `keep` reads the world, so it runs outside the lock the build threads take.
// Each section's first mark is checked; only a section that fails is checked
// mark by mark, and the result applies only if no build replaced it meanwhile.
void pruneMarks(int dimension, std::function<bool(Mark const&)> const& keep) {
    std::vector<std::pair<SectionKey, Mark>> fronts;
    {
        std::lock_guard lock(markLock);
        if (std::erase_if(sections, [&](auto const& entry) {
                return entry.second.empty() || std::get<0>(entry.first) != dimension;
            }))
            ++markRevision;
        fronts.reserve(sections.size());
        for (auto const& [key, list] : sections) fronts.emplace_back(key, list.front());
    }
    std::vector<Mark> list;
    for (auto const& [key, front] : fronts) {
        if (keep(front)) continue;
        {
            std::lock_guard lock(markLock);
            auto at = sections.find(key);
            if (at == sections.end()) continue;
            list = at->second;
        }
        auto kept = list;
        std::erase_if(kept, [&](Mark const& m) { return !keep(m); });
        std::lock_guard lock(markLock);
        auto at = sections.find(key);
        if (at == sections.end() || at->second != list) continue;
        if (kept.empty()) sections.erase(at);
        else at->second = std::move(kept);
        ++markRevision;
    }
}
std::atomic<size_t> capped{0};
void reportCapped(size_t shown) { capped.store(shown, std::memory_order_relaxed); }
size_t takeCapped() { return capped.exchange(0, std::memory_order_relaxed); }
void clearMarks() {
    std::lock_guard lock(markLock);
    if (sections.empty()) return;
    sections.clear();
    ++markRevision;
}
bool start() {
    if (installed) return true;
    // Chunk mesh vertices are version-sensitive: on an unverified game, stay vanilla.
    capabilities::begin(capabilities::Id::BlockHighlight);
    if (capabilities::registry().status(capabilities::Id::BlockHighlight).compatibility != capabilities::Reason::None) {
        versionSensitiveAllowed(capabilities::Id::BlockHighlight); return true;
    }
    for (auto& hook : hooks) {
        if (hook.installed) continue;
        if (hook.install(true) != 0) {
            capabilities::hooks(capabilities::Id::BlockHighlight, false);
            stop();
            return false;
        }
        hook.installed = true;
    }
    installed = true;
    capabilities::ready(capabilities::Id::BlockHighlight);
    return true;
}
void stop() {
    capabilities::stop(capabilities::Id::BlockHighlight);
    bool removed = true;
    for (auto& hook : hooks) if (hook.installed) {
        if (hook.remove(true)) hook.installed = false;
        else removed = false;
    }
    if (!removed) capabilities::fail(capabilities::Id::BlockHighlight, capabilities::Reason::RemovalFailed);
    {
        std::lock_guard lock(tableLock);
        table = nullptr;
    }
    generation.fetch_add(1, std::memory_order_release);
    published.clear();
    built.clear();
    wantedFrom.reset();
    wanted.clear();
    clearMarks();
    installed = !removed;
}
}
