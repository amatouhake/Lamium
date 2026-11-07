#include "features/visuals/BlockHighlightHooks.h"
#include "features/map/Waypoints.h"
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
#include "mc/world/level/block/Block.h"
#include "mc/world/phys/AABB.h"
#include <algorithm>
#include <atomic>
#include <bit>
#include <cmath>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace lamium::visuals::highlight {
namespace {
// What chunk-build threads tint: type name to color. Replaced, never
// changed, on the client thread; builders take a copy when `generation` moves.
struct Paint { std::uint32_t packed; std::uint8_t index; };
struct Table {
    std::unordered_map<std::string, Paint> colors;
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

// Marks per chunk section, replaced whole each time the section is built.
// Written by build threads, read by the render thread.
using SectionKey = std::tuple<int, int, int>;
std::mutex markLock;
std::map<SectionKey, std::vector<Mark>> sections;
std::uint64_t markRevision = 1;
// The section being built on this thread, if any.
thread_local std::vector<Mark>* capture = nullptr;
std::atomic<int> faceSamples{0};

void tint(Tessellator& tessellator, size_t from, std::uint32_t color) {
    auto& data = *tessellator.mMeshData;
    size_t to = data.mPositions->size();
    if (to <= from) return;
    auto r = static_cast<std::uint8_t>(map::channel(color, 0)), g = static_cast<std::uint8_t>(map::channel(color, 1)),
         b = static_cast<std::uint8_t>(map::channel(color, 2));
    auto& colors = *data.mColors;
    if (colors.size() >= to)
        for (size_t i = from; i < to; ++i) colors[i] = tintVertex(colors[i], r, g, b);
}

LL_TYPE_INSTANCE_HOOK(HighlightBlock, ll::memory::HookPriority::Normal, BlockTessellator,
    &BlockTessellator::tessellateBlockInWorld, bool, Tessellator& tessellator, Block const& block, BlockPos const& pos,
    std::bitset<6> const faces, AirAndSimpleBlockBits const* simple) {
    Paint paint{};
    bool wanted = false;
    try {
        if (auto current = currentTable(); current && !current->colors.empty()) {
            auto found = current->colors.find(block.getTypeName());
            if (found != current->colors.end()) { paint = found->second; wanted = true; }
        }
    } catch (...) {}
    if (!wanted) return origin(tessellator, block, pos, faces, simple);
    size_t before = tessellator.mMeshData->mPositions->size();
    bool result = origin(tessellator, block, pos, faces, simple);
    try {
        tint(tessellator, before, paint.packed);
        auto bits = static_cast<std::uint8_t>(faces.to_ulong() & 0x3f);
        // Whether the bits are the faces drawn is checked against the quads
        // the block added (L-320 step 2).
        if (faceSamples.fetch_add(1, std::memory_order_relaxed) < 4)
            Runtime::instance().self().getLogger().info("Block highlight: faces {:06b} ({} set), {} vertices added",
                bits, std::popcount(bits), tessellator.mMeshData->mPositions->size() - before);
        if (capture && bits) capture->push_back({{pos.x, pos.y, pos.z}, bits, paint.index});
    } catch (...) {}
    return result;
}

LL_TYPE_INSTANCE_HOOK(HighlightSection, ll::memory::HookPriority::Normal, RenderChunkBuilder,
    &RenderChunkBuilder::build, void, RenderChunkGeometry& geometry, bool transparentLeaves, BakedBlockLightType lighting,
    bool forExport, mce::framebuilder::FrameLightingModelCapabilities const& capabilities) {
    if (forExport || capture) return origin(geometry, transparentLeaves, lighting, forExport, capabilities);
    std::vector<Mark> found;
    capture = &found;
    struct Reset { ~Reset() { capture = nullptr; } } reset;
    origin(geometry, transparentLeaves, lighting, forExport, capabilities);
    try {
        auto const& box = *mBuildBoundingBox;
        SectionKey key{static_cast<int>(std::floor(box.min.x)), static_cast<int>(std::floor(box.min.y)),
                       static_cast<int>(std::floor(box.min.z))};
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

// Chunks keep the colors they were built with, so each dimension's
// coordinator rebuilds its chunks when the wanted list differs from the one
// it last built with, and once when first seen while something is wanted.
// Client thread only.
std::vector<Entry> wantedList() {
    auto& runtime = Runtime::instance();
    if (!capabilities::registry().status(capabilities::Id::BlockHighlight).available() || !runtime.enabled()) return {};
    auto const& visuals = runtime.snapshot()->visuals;
    return visuals.blockHighlight ? visuals.highlightBlocks : std::vector<Entry>{};
}
std::vector<Entry> published;
std::unordered_map<RenderChunkCoordinator const*, unsigned> built;
void publish(std::vector<Entry> const& entries) {
    auto next = std::make_shared<Table>();
    for (auto const& e : entries) {
        auto index = map::clampColor(e.color);
        next->colors[e.name] = {map::waypointColors[static_cast<size_t>(index)], static_cast<std::uint8_t>(index)};
    }
    {
        std::lock_guard lock(tableLock);
        table = next->colors.empty() ? nullptr : std::shared_ptr<Table const>(std::move(next));
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
        auto want = wantedList();
        if (want != published) publish(want);
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
    {HighlightRebuild::hook, HighlightRebuild::unhook}};
}
bool marks(std::uint64_t& revision, std::vector<Mark>& out) {
    std::lock_guard lock(markLock);
    if (revision == markRevision) return false;
    revision = markRevision;
    out.clear();
    for (auto const& [key, list] : sections) out.insert(out.end(), list.begin(), list.end());
    return true;
}
void pruneMarks(std::function<bool(Mark const&)> const& keep) {
    std::lock_guard lock(markLock);
    if (std::erase_if(sections, [&](auto const& entry) { return entry.second.empty() || !keep(entry.second.front()); }))
        ++markRevision;
}
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
    clearMarks();
    installed = !removed;
}
}
