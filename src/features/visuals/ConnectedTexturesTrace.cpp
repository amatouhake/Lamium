#include "features/visuals/ConnectedTexturesTrace.h"
#ifdef LAMIUM_CTM_TRACE
#include "app/Runtime.h"
#include "ll/api/memory/Hook.h"
#include "mc/client/renderer/block/BlockTessellator.h"
#include "mc/client/renderer/block/tessellation_pipeline/client_block_pipeline/BlockTessellatorPipeline.h"
#include "mc/client/renderer/block/tessellation_pipeline/client_block_pipeline/Inputs.h"
#include "mc/client/renderer/block/tessellation_pipeline/client_block_pipeline/Transforms.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/phys/AABB.h"
#include <atomic>
#include <format>
#include <mutex>
#include <set>
#include <tuple>

// Glass far away kept its borders (2026-10-11). Does the newer pipeline
// build those chunks instead of BlockTessellator?
namespace lamium::visuals::connectedTexturesTrace {
namespace {
template <class... Args>
void log(std::format_string<Args...> format, Args&&... args) noexcept {
    try { Runtime::instance().self().getLogger().info(std::format(format, std::forward<Args>(args)...)); } catch (...) {}
}
std::atomic<int> fullLogs{0}, newLogs{0}, oldLogs{0}, newGlass{0}, oldGlass{0};
std::mutex seenMutex;
std::set<std::tuple<int, int, int>> seenOld;

bool glass(Block const& block) { return block.getTypeName().ends_with("glass"); }

LL_STATIC_HOOK(TraceFullChunk, ll::memory::HookPriority::Normal,
    &ClientBlockPipeline::BlockTessellatorPipeline::setupFullChunkInputs, ClientBlockPipeline::Inputs, AABB&& aabb,
    ClientBlockPipeline::Transforms&& transforms, ChunkViewSource& source, bool inUI) {
    if (fullLogs < 60) {
        ++fullLogs;
        log("L-96 new pipeline full chunk {:.0f} {:.0f} {:.0f} .. {:.0f} {:.0f} {:.0f} (ui {})", aabb.min.x, aabb.min.y, aabb.min.z,
            aabb.max.x, aabb.max.y, aabb.max.z, inUI);
    }
    return origin(std::move(aabb), std::move(transforms), source, inUI);
}
LL_STATIC_HOOK(TraceUseNew, ll::memory::HookPriority::Normal,
    &ClientBlockPipeline::BlockTessellatorPipeline::useNewTessellation, bool, Block const& block, bool onlyNew) {
    bool result = origin(block, onlyNew);
    try {
        if (glass(block)) {
            (result ? newGlass : oldGlass)++;
            if (newLogs < 20) {
                ++newLogs;
                log("L-96 useNewTessellation {} onlyNew {} -> {}", block.getTypeName(), onlyNew, result);
            }
        }
    } catch (...) {}
    return result;
}
LL_TYPE_INSTANCE_HOOK(TraceOld, ll::memory::HookPriority::Highest, BlockTessellator, &BlockTessellator::tessellateBlockInWorld,
    bool, Tessellator& tessellator, Block const& block, BlockPos const& pos, std::bitset<6> const faces,
    AirAndSimpleBlockBits const* simple) {
    try {
        if (glass(block)) {
            std::lock_guard lock{seenMutex};
            auto chunk = std::make_tuple(pos.x >> 4, pos.y >> 4, pos.z >> 4);
            if (seenOld.insert(chunk).second && oldLogs < 200) {
                ++oldLogs;
                log("L-96 BlockTessellator glass in subchunk {} {} {} ({} at {} {} {})", std::get<0>(chunk), std::get<1>(chunk),
                    std::get<2>(chunk), block.getTypeName(), pos.x, pos.y, pos.z);
            }
        }
    } catch (...) {}
    return origin(tessellator, block, pos, faces, simple);
}
}
void start() {
    TraceFullChunk::hook();
    TraceUseNew::hook();
    TraceOld::hook();
    Runtime::instance().self().getLogger().warn("Connected textures pipeline diagnostics enabled (L-96)");
}
void stop() {
    TraceFullChunk::unhook(true);
    TraceUseNew::unhook(true);
    TraceOld::unhook(true);
    log("L-96 useNewTessellation for glass: new {}, old {}", newGlass.load(), oldGlass.load());
}
}
#else
namespace lamium::visuals::connectedTexturesTrace { void start() {} void stop() {} }
#endif
