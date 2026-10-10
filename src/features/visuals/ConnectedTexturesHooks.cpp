#include "features/visuals/ConnectedTexturesHooks.h"
#include "features/visuals/ConnectedTextures.h"
#include "app/Runtime.h"
#include "app/Versions.h"
#include "ll/api/memory/Hook.h"
#include "mc/client/renderer/block/BlockTessellator.h"
#include "mc/client/renderer/chunks/RenderChunkCoordinator.h"
#include "mc/client/renderer/texture/TextureUVCoordinateSet.h"
#include "mc/deps/core/math/Vec3.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/block/BlockType.h"
#include <atomic>

namespace lamium::visuals::connected {
namespace {
// Read by chunk-build threads; written once per coordinator tick.
std::atomic<bool> active{false};
bool supported = false, installed = false;
// The connecting block being tessellated on this (chunk-build) thread.
struct Current {
    Block const* block = nullptr;
    BlockPos pos{};
    BlockSource const* region = nullptr;
};
thread_local Current current;

bool wanted() {
    auto& runtime = Runtime::instance();
    return supported && runtime.enabled() && runtime.snapshot()->visuals.connectedTextures;
}
bool sameAt(Block const& block, Offset offset) {
    BlockPos at{current.pos.x + offset.x, current.pos.y + offset.y, current.pos.z + offset.z};
    return &current.region->getBlock(at).getBlockType() == &block.getBlockType();
}
TextureUVCoordinateSet faceTexture(Face face, Block const& block, TextureUVCoordinateSet const& tex) {
    TextureUVCoordinateSet out = tex;
    if (current.block != &block || !current.region) return out;
    auto s = sides(face);
    auto uv = trim({tex._u0, tex._v0, tex._u1, tex._v1}, tex._sourceImageWidth, tex._sourceImageHeight,
                   {sameAt(block, s.left), sameAt(block, s.right), sameAt(block, s.top), sameAt(block, s.bottom)});
    out._u0 = uv.u0;
    out._v0 = uv.v0;
    out._u1 = uv.u1;
    out._v1 = uv.v1;
    return out;
}

LL_TYPE_INSTANCE_HOOK(ConnectedBlock, ll::memory::HookPriority::Normal, BlockTessellator,
    &BlockTessellator::tessellateBlockInWorld, bool, Tessellator& tessellator, Block const& block, BlockPos const& pos,
    std::bitset<6> const faces, AirAndSimpleBlockBits const* simple) {
    bool connecting = false;
    try { connecting = active.load(std::memory_order_relaxed) && mRegion && connects(block.getTypeName()); } catch (...) {}
    if (!connecting) return origin(tessellator, block, pos, faces, simple);
    auto saved = current;
    current = {&block, pos, mRegion};
    bool result = origin(tessellator, block, pos, faces, simple);
    current = saved;
    return result;
}
#define LAMIUM_CONNECTED_FACE(Name, Function, FaceId)                                                                        \
    LL_TYPE_INSTANCE_HOOK(Name, ll::memory::HookPriority::Normal, BlockTessellator, &BlockTessellator::Function, void,     \
        Tessellator& tessellator, Block const& block, Vec3 const& p, TextureUVCoordinateSet const& tex) {                   \
        if (current.block != &block) return origin(tessellator, block, p, tex);                                             \
        TextureUVCoordinateSet copy = tex;                                                                                  \
        try { copy = faceTexture(FaceId, block, tex); } catch (...) {}                                                      \
        origin(tessellator, block, p, copy);                                                                                \
    }
LAMIUM_CONNECTED_FACE(ConnectedDown, tessellateFaceDown, Face::Down)
LAMIUM_CONNECTED_FACE(ConnectedUp, tessellateFaceUp, Face::Up)
LAMIUM_CONNECTED_FACE(ConnectedNorth, tessellateNorth, Face::North)
LAMIUM_CONNECTED_FACE(ConnectedSouth, tessellateSouth, Face::South)
LAMIUM_CONNECTED_FACE(ConnectedWest, tessellateWest, Face::West)
LAMIUM_CONNECTED_FACE(ConnectedEast, tessellateEast, Face::East)
// Each dimension's coordinator ticks on the client thread: when the switch
// changed, rebuild its chunks so glass redraws either way.
LL_TYPE_INSTANCE_HOOK(ConnectedRebuild, ll::memory::HookPriority::Normal, RenderChunkCoordinator,
    &RenderChunkCoordinator::tick, void) {
    origin();
    try {
        bool want = wanted();
        if (want != active.load()) {
            active = want;
            _setAllDirty(false, false);
        }
    } catch (...) {}
}
struct Hook { int (*install)(bool); bool (*remove)(bool); };
Hook hooks[] = {{ConnectedBlock::hook, ConnectedBlock::unhook}, {ConnectedDown::hook, ConnectedDown::unhook},
    {ConnectedUp::hook, ConnectedUp::unhook}, {ConnectedNorth::hook, ConnectedNorth::unhook},
    {ConnectedSouth::hook, ConnectedSouth::unhook}, {ConnectedWest::hook, ConnectedWest::unhook},
    {ConnectedEast::hook, ConnectedEast::unhook}, {ConnectedRebuild::hook, ConnectedRebuild::unhook}};
}
bool start() {
    if (installed) return true;
    // Chunk meshes are version-sensitive: on an unverified game, stay vanilla.
    supported = verifiedGameExecutable();
    if (!supported) {
        Runtime::instance().self().getLogger().warn("Connected Textures: vanilla glass retained (unverified game version)");
        return true;
    }
    for (auto& hook : hooks)
        if (hook.install(true) != 0) {
            for (auto& undo : hooks) undo.remove(true);
            supported = false;
            return false;
        }
    installed = true;
    return true;
}
void stop() {
    if (!installed) return;
    active = false;
    for (auto& hook : hooks) hook.remove(true);
    installed = false;
}
}
