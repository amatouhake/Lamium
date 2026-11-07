#include "features/visuals/BlockHighlightHooks.h"
#include "features/visuals/BlockHighlight.h"
#include "features/map/Waypoints.h"
#include "app/Capabilities.h"
#include "app/Runtime.h"
#include "app/Versions.h"
#include "ll/api/memory/Hook.h"
#include "mc/client/renderer/Tessellator.h"
#include "mc/client/renderer/block/BlockTessellator.h"
#include "mc/client/renderer/chunks/RenderChunkCoordinator.h"
#include "mc/deps/minecraft_renderer/renderer/MeshData.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/block/Block.h"
#include <algorithm>
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace lamium::visuals::highlight {
namespace {
// What chunk-build threads tint: type name to packed color. Replaced, never
// changed, on the client thread; builders take a copy when `generation` moves.
struct Table {
    std::unordered_map<std::string, std::uint32_t> colors;
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
// Light map coordinates are raised to at least this (both block and sky), so
// a highlighted block reads at full brightness in the dark. UV1 being the
// light map is a hypothesis checked in game (L-320 step 1).
constexpr float fullLight = .9375f;
std::atomic<int> uvSamples{0};

void tint(Tessellator& tessellator, size_t from, std::uint32_t color) {
    auto& data = *tessellator.mMeshData;
    size_t to = data.mPositions->size();
    if (to <= from) return;
    auto r = static_cast<std::uint8_t>(map::channel(color, 0)), g = static_cast<std::uint8_t>(map::channel(color, 1)),
         b = static_cast<std::uint8_t>(map::channel(color, 2));
    auto& colors = *data.mColors;
    if (colors.size() >= to)
        for (size_t i = from; i < to; ++i) colors[i] = tintVertex(colors[i], r, g, b);
    auto& light = *data.mTextureUVs[1];
    if (light.size() >= to) {
        if (uvSamples.fetch_add(1, std::memory_order_relaxed) < 3) {
            float low = 1e9f, high = -1e9f;
            for (size_t i = from; i < to; ++i) low = std::min({low, light[i].x, light[i].y}), high = std::max({high, light[i].x, light[i].y});
            try { Runtime::instance().self().getLogger().info("Block highlight: light map UV range {}..{} over {} vertices", low, high, to - from); } catch (...) {}
        }
        for (size_t i = from; i < to; ++i) {
            light[i].x = std::max(light[i].x, fullLight);
            light[i].y = std::max(light[i].y, fullLight);
        }
    }
}

LL_TYPE_INSTANCE_HOOK(HighlightBlock, ll::memory::HookPriority::Normal, BlockTessellator,
    &BlockTessellator::tessellateBlockInWorld, bool, Tessellator& tessellator, Block const& block, BlockPos const& pos,
    std::bitset<6> const faces, AirAndSimpleBlockBits const* simple) {
    std::uint32_t color = 0;
    bool wanted = false;
    try {
        if (auto current = currentTable(); current && !current->colors.empty()) {
            auto found = current->colors.find(block.getTypeName());
            if (found != current->colors.end()) { color = found->second; wanted = true; }
        }
    } catch (...) {}
    if (!wanted) return origin(tessellator, block, pos, faces, simple);
    size_t before = tessellator.mMeshData->mPositions->size();
    bool result = origin(tessellator, block, pos, faces, simple);
    try { tint(tessellator, before, color); } catch (...) {}
    return result;
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
    for (auto const& e : entries) next->colors[e.name] = map::waypointColors[static_cast<size_t>(map::clampColor(e.color))];
    {
        std::lock_guard lock(tableLock);
        table = next->colors.empty() ? nullptr : std::shared_ptr<Table const>(std::move(next));
    }
    published = entries;
    generation.fetch_add(1, std::memory_order_release);
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
Hook hooks[] = {{HighlightBlock::hook, HighlightBlock::unhook}, {HighlightRebuild::hook, HighlightRebuild::unhook}};
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
    installed = !removed;
}
}
