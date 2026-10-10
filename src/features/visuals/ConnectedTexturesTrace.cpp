#include "features/visuals/ConnectedTexturesTrace.h"
#ifdef LAMIUM_CTM_TRACE
#include "app/Runtime.h"
#include "ll/api/memory/Hook.h"
#include "mc/client/renderer/block/BlockTessellator.h"
#include "mc/client/renderer/texture/TextureUVCoordinateSet.h"
#include "mc/deps/core/math/Vec3.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/block/BlockType.h"
#include <atomic>
#include <format>
#include <string>

namespace lamium::visuals::connectedTexturesTrace {
namespace {
template <class... Args>
void log(std::format_string<Args...> format, Args&&... args) noexcept {
    try { Runtime::instance().self().getLogger().info(std::format(format, std::forward<Args>(args)...)); } catch (...) {}
}
// The block being tessellated on this (chunk-build) thread.
struct Current {
    Block const* block = nullptr;
    BlockPos pos{};
    BlockSource const* region = nullptr;
};
thread_local Current current;
std::atomic<int> faceLogs{0}, blockLogs{0}, glassBlocks{0}, glassFaces{0};

bool glass(Block const& block) {
    auto const& name = block.getTypeName();
    return name.ends_with("glass") && !name.ends_with("_pane");
}
bool same(Block const& block, BlockPos const& at) {
    try { return &current.region->getBlock(at).getBlockType() == &block.getBlockType(); } catch (...) { return false; }
}
enum Face { Down, Up, North, South, West, East };
constexpr char const* faceNames[]{"down", "up", "north", "south", "west", "east"};
// Neighbors in the face's plane, seen from outside: the block to the left
// (toward u0), right (u1), above (v0) and below (v1). Guesses for the spike;
// the log and the screen tell whether a side is mirrored.
struct Plane { BlockPos left, right, top, bottom; };
Plane plane(Face face, BlockPos p) {
    auto at = [&](int dx, int dy, int dz) { return BlockPos{p.x + dx, p.y + dy, p.z + dz}; };
    switch (face) {
    case North: return {at(1, 0, 0), at(-1, 0, 0), at(0, 1, 0), at(0, -1, 0)};
    case South: return {at(-1, 0, 0), at(1, 0, 0), at(0, 1, 0), at(0, -1, 0)};
    case East: return {at(0, 0, 1), at(0, 0, -1), at(0, 1, 0), at(0, -1, 0)};
    case West: return {at(0, 0, -1), at(0, 0, 1), at(0, 1, 0), at(0, -1, 0)};
    case Up: return {at(-1, 0, 0), at(1, 0, 0), at(0, 0, -1), at(0, 0, 1)};
    default: return {at(-1, 0, 0), at(1, 0, 0), at(0, 0, 1), at(0, 0, -1)};
    }
}
// A copy of the face's texture with the border texel cut on connected sides.
TextureUVCoordinateSet trimmed(Face face, Block const& block, Vec3 const& p, TextureUVCoordinateSet const& tex) {
    TextureUVCoordinateSet out = tex;
    if (!current.region || current.block != &block || !glass(block)) return out;
    auto sides = plane(face, current.pos);
    bool left = same(block, sides.left), right = same(block, sides.right), top = same(block, sides.top),
         bottom = same(block, sides.bottom);
    int width = tex._sourceImageWidth ? tex._sourceImageWidth : 16, height = tex._sourceImageHeight ? tex._sourceImageHeight : 16;
    float du = (tex._u1 - tex._u0) / width, dv = (tex._v1 - tex._v0) / height;
    if (left) out._u0 = tex._u0 + du;
    if (right) out._u1 = tex._u1 - du;
    if (top) out._v0 = tex._v0 + dv;
    if (bottom) out._v1 = tex._v1 - dv;
    ++glassFaces;
    if (faceLogs < 40) {
        ++faceLogs;
        log("L-96 {} face of {} at {} {} {} (p {:.2f} {:.2f} {:.2f}): uv {:.5f},{:.5f} - {:.5f},{:.5f} image {}x{}; "
            "connected left {} right {} top {} bottom {}",
            faceNames[face], block.getTypeName(), current.pos.x, current.pos.y, current.pos.z, p.x, p.y, p.z, tex._u0, tex._v0,
            tex._u1, tex._v1, width, height, left, right, top, bottom);
    }
    return out;
}

LL_TYPE_INSTANCE_HOOK(CtmBlock, ll::memory::HookPriority::Normal, BlockTessellator, &BlockTessellator::tessellateBlockInWorld,
    bool, Tessellator& tessellator, Block const& block, BlockPos const& pos, std::bitset<6> const faces,
    AirAndSimpleBlockBits const* simple) {
    auto saved = current;
    current = {&block, pos, mRegion};
    bool isGlass = false;
    try { isGlass = glass(block); } catch (...) {}
    if (isGlass) {
        ++glassBlocks;
        if (blockLogs < 10) {
            ++blockLogs;
            log("L-96 tessellateBlockInWorld {} at {} {} {} faces {}", block.getTypeName(), pos.x, pos.y, pos.z, faces.to_string());
        }
    }
    bool result = origin(tessellator, block, pos, faces, simple);
    current = saved;
    return result;
}
#define LAMIUM_CTM_FACE(Name, Function, FaceId)                                                                              \
    LL_TYPE_INSTANCE_HOOK(Name, ll::memory::HookPriority::Normal, BlockTessellator, &BlockTessellator::Function, void,     \
        Tessellator& tessellator, Block const& block, Vec3 const& p, TextureUVCoordinateSet const& tex) {                   \
        TextureUVCoordinateSet copy = tex;                                                                                  \
        try { copy = trimmed(FaceId, block, p, tex); } catch (...) {}                                                       \
        origin(tessellator, block, p, copy);                                                                                \
    }
LAMIUM_CTM_FACE(CtmDown, tessellateFaceDown, Down)
LAMIUM_CTM_FACE(CtmUp, tessellateFaceUp, Up)
LAMIUM_CTM_FACE(CtmNorth, tessellateNorth, North)
LAMIUM_CTM_FACE(CtmSouth, tessellateSouth, South)
LAMIUM_CTM_FACE(CtmWest, tessellateWest, West)
LAMIUM_CTM_FACE(CtmEast, tessellateEast, East)
}
void start() {
    CtmBlock::hook();
    CtmDown::hook();
    CtmUp::hook();
    CtmNorth::hook();
    CtmSouth::hook();
    CtmWest::hook();
    CtmEast::hook();
    Runtime::instance().self().getLogger().warn("Connected textures spike enabled (L-96)");
}
void stop() {
    CtmBlock::unhook(true);
    CtmDown::unhook(true);
    CtmUp::unhook(true);
    CtmNorth::unhook(true);
    CtmSouth::unhook(true);
    CtmWest::unhook(true);
    CtmEast::unhook(true);
    log("L-96 totals: glass blocks {}, glass faces {}", glassBlocks.load(), glassFaces.load());
}
}
#else
namespace lamium::visuals::connectedTexturesTrace { void start() {} void stop() {} }
#endif
