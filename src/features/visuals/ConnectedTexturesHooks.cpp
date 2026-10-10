#include "features/visuals/ConnectedTexturesHooks.h"
#include "features/visuals/ConnectedTextures.h"
#include "app/Runtime.h"
#include "app/Versions.h"
#include "ll/api/memory/Hook.h"
#include "mc/client/renderer/Tessellator.h"
#include "mc/client/renderer/block/BlockGraphics.h"
#include "mc/client/renderer/block/BlockTessellator.h"
#include "mc/client/renderer/block/TextureItem.h"
#include "mc/deps/minecraft_renderer/renderer/MeshData.h"
#include "mc/world/phys/AABB.h"
#include "mc/client/renderer/chunks/RenderChunkCoordinator.h"
#include "mc/client/renderer/texture/TextureUVCoordinateSet.h"
#include "mc/deps/core/math/Vec3.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/block/BlockType.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <vector>

namespace lamium::visuals::connected {
namespace {
// Read by chunk-build threads; written once per coordinator tick.
std::atomic<bool> active{false};
bool supported = false, installed = false;
// The connecting block being tessellated on this (chunk-build) thread.
struct Current {
    Rule rule;
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
    if (current.rule.sidesOnly && (face == Face::Up || face == Face::Down)) return out;
    auto s = sides(face);
    auto uv = trim({tex._u0, tex._v0, tex._u1, tex._v1}, tex._sourceImageWidth, tex._sourceImageHeight,
                   {sameAt(block, s.left), sameAt(block, s.right), sameAt(block, s.top), sameAt(block, s.bottom)},
                   current.rule);
    out._u0 = uv.u0;
    out._v0 = uv.v0;
    out._u1 = uv.u1;
    out._v1 = uv.v1;
    return out;
}

LL_TYPE_INSTANCE_HOOK(ConnectedBlock, ll::memory::HookPriority::Normal, BlockTessellator,
    &BlockTessellator::tessellateBlockInWorld, bool, Tessellator& tessellator, Block const& block, BlockPos const& pos,
    std::bitset<6> const faces, AirAndSimpleBlockBits const* simple) {
    std::optional<Rule> rule;
    try { if (active.load(std::memory_order_relaxed) && mRegion) rule = ruleFor(block.getTypeName()); } catch (...) {}
    if (!rule) return origin(tessellator, block, pos, faces, simple);
    auto saved = current;
    current = {*rule, &block, pos, mRegion};
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
// ---- Glass panes ----
// The pane being tessellated on this thread and its glass rectangle (slot 0;
// slot 5 is the thin edge).
struct Glass { float u0 = 0, v0 = 0, u1 = 0, v1 = 0; int width = 16, height = 16; bool known = false; };
struct PaneBuild {
    Block const* block = nullptr;
    BlockPos pos{};
    BlockSource const* region = nullptr;
    Glass glass;
};
thread_local PaneBuild pane;

Parts partsAt(Block const& block, BlockPos at) {
    auto const& other = pane.region->getBlock(at);
    if (&other.getBlockType() != &block.getBlockType()) return {};
    std::vector<AABB> found;
    other.getBlockType().addAABBs(other, *pane.region, at, nullptr, found);
    std::vector<Box> boxes;
    for (auto const& box : found)
        boxes.push_back({box.min.x - at.x, box.max.x - at.x, box.min.z - at.z, box.max.z - at.z});
    return partsOf(boxes);
}
bool paneAt(Block const& block, int dx, int dz) {
    return &pane.region->getBlock({pane.pos.x + dx, pane.pos.y, pane.pos.z + dz}).getBlockType() == &block.getBlockType();
}
// Adjusts the quads vanilla just added for this pane (positions from `before`).
void adjustPane(Tessellator& tessellator, Block const& block, size_t before) {
    auto& positions = *tessellator.mMeshData->mPositions;
    auto& uvs = *tessellator.mMeshData->mTextureUVs[0];
    size_t after = positions.size();
    if (after <= before || before % 4 || (after - before) % 4 || uvs.size() < after || !pane.glass.known) return;
    // Block coordinates: the mesh is offset from the world by whole blocks.
    float bx = positions[before].x, by = positions[before].y, bz = positions[before].z;
    for (size_t i = before; i < after; ++i) {
        bx = std::min(bx, positions[i].x);
        by = std::min(by, positions[i].y);
        bz = std::min(bz, positions[i].z);
    }
    bx = std::floor(bx);
    by = std::floor(by);
    bz = std::floor(bz);
    auto above = partsAt(block, {pane.pos.x, pane.pos.y + 1, pane.pos.z});
    auto below = partsAt(block, {pane.pos.x, pane.pos.y - 1, pane.pos.z});
    bool east = paneAt(block, 1, 0), west = paneAt(block, -1, 0), south = paneAt(block, 0, 1), north = paneAt(block, 0, -1);
    auto const& g = pane.glass;
    float du = (g.u1 - g.u0) / g.width, dv = (g.v1 - g.v0) / g.height;
    constexpr float e = 1e-5f;
    for (size_t q = before; q < after; q += 4) {
        float x0 = 2, x1 = -1, y0 = 2, y1 = -1, z0 = 2, z1 = -1;
        for (size_t i = q; i < q + 4; ++i) {
            float x = positions[i].x - bx, y = positions[i].y - by, z = positions[i].z - bz;
            x0 = std::min(x0, x); x1 = std::max(x1, x);
            y0 = std::min(y0, y); y1 = std::max(y1, y);
            z0 = std::min(z0, z); z1 = std::max(z1, z);
        }
        auto part = partOf(x0, x1, z0, z1);
        if (y1 - y0 < 0.01f) {
            // A thin top or bottom face: fold it to a point under (over) the same part.
            bool top = y0 > 0.5f;
            if ((top && above.has(part)) || (!top && below.has(part)))
                for (size_t i = q + 1; i < q + 4; ++i) positions[i] = positions[q];
            continue;
        }
        bool glass = uvs[q].x >= std::min(g.u0, g.u1) - e && uvs[q].x <= std::max(g.u0, g.u1) + e
            && uvs[q].y >= std::min(g.v0, g.v1) - e && uvs[q].y <= std::max(g.v0, g.v1) + e;
        if (!glass) continue;
        float uMin = uvs[q].x, uMax = uMin, vMin = uvs[q].y, vMax = vMin;
        for (size_t i = q; i < q + 4; ++i) {
            uMin = std::min(uMin, uvs[i].x); uMax = std::max(uMax, uvs[i].x);
            vMin = std::min(vMin, uvs[i].y); vMax = std::max(vMax, uvs[i].y);
        }
        for (size_t i = q; i < q + 4; ++i) {
            float x = positions[i].x - bx, y = positions[i].y - by, z = positions[i].z - bz;
            if ((x > 0.99f && east) || (x < 0.01f && west) || (z > 0.99f && south) || (z < 0.01f && north))
                uvs[i].x = inward(uvs[i].x, uMin, uMax, du);
            if ((y > 0.9f && above.has(part)) || (y < 0.1f && below.has(part))) uvs[i].y = inward(uvs[i].y, vMin, vMax, dv);
        }
    }
}
LL_TYPE_INSTANCE_HOOK(ConnectedPane, ll::memory::HookPriority::Normal, BlockTessellator,
    &BlockTessellator::tessellateDoubleThinFenceInWorld, bool, Tessellator& tessellator, Block const& block,
    BlockPos const& p, bool singleSide) {
    bool connecting = false;
    try { connecting = active.load(std::memory_order_relaxed) && mRegion && connectsPane(block.getTypeName()); } catch (...) {}
    if (!connecting) return origin(tessellator, block, p, singleSide);
    auto saved = pane;
    pane = {&block, p, mRegion};
    size_t before = tessellator.mMeshData->mPositions->size();
    bool result = origin(tessellator, block, p, singleSide);
    try { adjustPane(tessellator, block, before); } catch (...) {}
    pane = saved;
    return result;
}
using PosTexture = TextureUVCoordinateSet const& (BlockGraphics::*)(BlockPos const&, uint64, int) const;
LL_TYPE_INSTANCE_HOOK(ConnectedPaneGlass, ll::memory::HookPriority::Normal, BlockGraphics,
    static_cast<PosTexture>(&BlockGraphics::getTexture), TextureUVCoordinateSet const&, BlockPos const& at, uint64 slot,
    int variant) {
    auto const& tex = origin(at, slot, variant);
    if (pane.block && slot == 0)
        pane.glass = {tex._u0, tex._v0, tex._u1, tex._v1, tex._sourceImageWidth ? tex._sourceImageWidth : 16,
                      tex._sourceImageHeight ? tex._sourceImageHeight : 16, true};
    return tex;
}
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
    {ConnectedEast::hook, ConnectedEast::unhook}, {ConnectedPane::hook, ConnectedPane::unhook},
    {ConnectedPaneGlass::hook, ConnectedPaneGlass::unhook}, {ConnectedRebuild::hook, ConnectedRebuild::unhook}};
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
