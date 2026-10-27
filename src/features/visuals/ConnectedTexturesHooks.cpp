#include "features/visuals/ConnectedTexturesHooks.h"
#include "features/visuals/ConnectedSprites.h"
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
#include <array>
#include <atomic>
#include <cmath>
#include <unordered_map>
#include <utility>
#include <vector>

namespace lamium::visuals::connected {
namespace {
// Read by chunk-build threads; written once per coordinator tick.
std::atomic<bool> active{false};
// The L-208/L-270 corner options, same lifetime as `active`: edges where glass
// blocks turn inward, and - with `paneCorners` - the inside edge of an
// L-shaped pane, whose outside edge needs `paneCorners` alone.
std::atomic<bool> insideCorners{false}, paneCorners{false};
bool installed = false;
bool drawingEnabled() {
    return active.load(std::memory_order_relaxed)
        && capabilities::registry().status(capabilities::Id::ConnectedTextures).available();
}
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
    return capabilities::registry().status(capabilities::Id::ConnectedTextures).available()
        && runtime.enabled() && runtime.snapshot()->visuals.connectedTextures;
}
bool sameAt(Block const& block, Offset offset) {
    BlockPos at{current.pos.x + offset.x, current.pos.y + offset.y, current.pos.z + offset.z};
    return &current.region->getBlock(at).getBlockType() == &block.getBlockType();
}
// The face's texture moved onto the sprite for its joined sides; vanilla's
// when nothing joins or the atlas has no sprite for it (fail open).
TextureUVCoordinateSet faceTexture(Face face, Block const& block, TextureUVCoordinateSet const& tex) {
    if (current.block != &block || !current.region) return tex;
    if (current.rule.sidesOnly && (face == Face::Up || face == Face::Down)) return tex;
    bool reversedU = tex._u0 > tex._u1, reversedV = tex._v0 > tex._v1;
    auto mask = faceMask(face, current.rule, reversedU, reversedV, insideCorners.load(std::memory_order_relaxed),
                         [&](Offset offset) { return sameAt(block, offset); });
    if (!mask) return tex;
    // A missing corner sprite falls back to the plain joined one, not vanilla.
    auto moved = sprites::shift(std::min(tex._u0, tex._u1), std::min(tex._v0, tex._v1), mask);
    if (!moved && mask > 15) moved = sprites::shift(std::min(tex._u0, tex._u1), std::min(tex._v0, tex._v1), mask & 15);
    if (!moved) return tex;
    TextureUVCoordinateSet out = tex;
    out._u0 += moved->u;
    out._u1 += moved->u;
    out._v0 += moved->v;
    out._v1 += moved->v;
    return out;
}

LL_TYPE_INSTANCE_HOOK(ConnectedBlock, ll::memory::HookPriority::Normal, BlockTessellator,
    &BlockTessellator::tessellateBlockInWorld, bool, Tessellator& tessellator, Block const& block, BlockPos const& pos,
    std::bitset<6> const faces, AirAndSimpleBlockBits const* simple) {
    std::optional<Rule> rule;
    try { if (drawingEnabled() && mRegion) rule = ruleFor(block.getTypeName()); } catch (...) {}
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
// L-271: cut a face whose two ends sit at concave corners into a corner slice,
// the unchanged middle and a corner slice. Each piece keeps its own share of
// the face's u window, so a slice that slides onto the tile border keeps its
// width and only the texels it replaces change. The extra quads are asked of
// the tessellator itself, so its vertex count, quad info and index phase stay
// its own; every per-vertex stream is then copied from the face and only the
// axis coordinate and u change. Returns the first quad it added, 0 when the
// mesh cannot take them (the face is then left as it is).
size_t splitPaneFace(Tessellator& tessellator, size_t q, PaneSlidePlan const& plan, float bx, float by, float bz,
                     float x0, float x1, float z0, float z1, float uLow, float uHigh) {
    auto& data = *tessellator.mMeshData;
    size_t count = data.mPositions->size();
    if (!data.mIndices->empty() || count % 4 || q + 4 > count || data.mTextureUVs[0]->size() < count) return 0;
    std::array<float, 4> vx{}, vy{}, vz{}, vu{};
    for (size_t k = 0; k < 4; ++k) {
        vx[k] = (*data.mPositions)[q + k].x - bx;
        vy[k] = (*data.mPositions)[q + k].y - by;
        vz[k] = (*data.mPositions)[q + k].z - bz;
        vu[k] = (*data.mTextureUVs[0])[q + k].x;
    }
    bool alongX = x1 - x0 > z1 - z0;
    float axisLow = alongX ? x0 : z0, axisHigh = alongX ? x1 : z1;
    if (axisHigh - axisLow < 0.02f) return 0;
    // The u each end of the face shows, from the vertices sitting there.
    float endU[2]{0.f, 0.f};
    int ends[2]{0, 0};
    for (size_t k = 0; k < 4; ++k) {
        float along = alongX ? vx[k] : vz[k];
        if (std::abs(along - axisLow) < 0.02f) { endU[0] += vu[k]; ++ends[0]; }
        else if (std::abs(along - axisHigh) < 0.02f) { endU[1] += vu[k]; ++ends[1]; }
        else return 0;
    }
    if (!ends[0] || !ends[1]) return 0;
    float uAtLow = endU[0] / static_cast<float>(ends[0]), uAtHigh = endU[1] / static_cast<float>(ends[1]);
    auto pieces = panePieces(plan);
    if (pieces.size() < 2) return 0;
    size_t extra = pieces.size() - 1;
    for (size_t i = 0; i < extra * 4; ++i) tessellator.vertex(vx[0] + bx, vy[0] + by, vz[0] + bz);
    if (data.mPositions->size() != count + extra * 4) return 0;
    auto carry = [&](auto& values) {
        if (values.size() < count + extra * 4) return;
        for (size_t i = 0; i < extra * 4; ++i) values[count + i] = values[q + (i % 4)];
    };
    carry(*data.mPositions);
    carry(*data.mNormals);
    carry(*data.mTangents);
    carry(*data.mColors);
    carry(*data.mBoneId0s);
    for (int i = 0; i < 3; ++i) carry(*data.mTextureUVs[i]);
    carry(*data.mPBRTextureIndices);
    carry(*data.mMERS);
    carry(*data.mGeoType);
    for (size_t i = 0; i <= extra; ++i) {
        // The face's own vertex order, so the winding stays the one vanilla made.
        size_t slot = i == 0 ? q : count + (i - 1) * 4;
        std::array<float, 4> px{}, pz{}, pu{}, turned{};
        for (size_t k = 0; k < 4; ++k) {
            float t = std::abs((alongX ? vx[k] : vz[k]) - axisLow) < 0.02f ? pieces[i].from : pieces[i].to;
            float moved = axisLow + (axisHigh - axisLow) * t;
            auto& position = (*data.mPositions)[slot + k];
            if (alongX) position.x = moved + bx;
            else position.z = moved + bz;
            px[k] = position.x - bx;
            pz[k] = position.z - bz;
            pu[k] = uAtLow + (uAtHigh - uAtLow) * t;
        }
        turned = pu;
        if (pieces[i].lowCorner) paneCornerU(px, pz, pu, plan.atLow.first, plan.atLow.second, uLow, uHigh, turned);
        else if (pieces[i].highCorner) paneCornerU(px, pz, pu, plan.atHigh.first, plan.atHigh.second, uLow, uHigh, turned);
        for (size_t k = 0; k < 4; ++k) (*data.mTextureUVs[0])[slot + k].x = turned[k];
    }
    return count;
}
// Panes keep vanilla's quads: each glass quad moves onto the glass sprite for
// the tile edges it has on joined block edges (an arm shows part of the tile;
// its joined edge is a tile edge). Adjusts the quads vanilla just added for
// this pane (positions from `before`).
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
    // L-208/L-255/L-270/L-271: an L-shaped pane (arms on two adjacent sides)
    // keeps vanilla's fully merged corner post; with the pane option on, the
    // post's outside faces reach the tile border at the corners of the turn
    // (L-208). The faces beside a concave corner - one for an L, two for a T,
    // four for a cross - gain an inside edge only with the inside-corner option
    // too, so the pane option alone leaves the turn looking as L-208 left it
    // (maintainer 2026-10-27). With the pane option off no pane geometry is
    // computed at all.
    auto edges = paneEdgeKinds(paneCorners.load(), insideCorners.load());
    Parts parts;
    std::optional<std::pair<float, float>> outside;
    if (edges.outside) {
        parts = partsAt(block, pane.pos);
        outside = paneCornerPoint(parts);
    }
    auto const& g = pane.glass;
    float uLow = std::min(g.u0, g.u1), uHigh = std::max(g.u0, g.u1), vLow = std::min(g.v0, g.v1), vHigh = std::max(g.v0, g.v1);
    float du = (uHigh - uLow) / g.width, dv = (vHigh - vLow) / g.height;
    constexpr float e = 1e-5f;
    // A corner end slides its whole u window onto the tile border there (L-255).
    auto slide = [&](size_t q, std::pair<float, float> const& corner) {
        std::array<float, 4> x{}, z{}, u{}, turned{};
        for (size_t i = 0; i < 4; ++i) {
            x[i] = positions[q + i].x - bx;
            z[i] = positions[q + i].z - bz;
            u[i] = uvs[q + i].x;
        }
        if (!paneCornerU(x, z, u, corner.first, corner.second, uLow, uHigh, turned)) return;
        for (size_t i = 0; i < 4; ++i) uvs[q + i].x = turned[i];
    };
    // Vanilla's sprite for the tile edges the quad has on joined block edges.
    auto shade = [&](size_t q, Part part) {
        Mask mask = 0;
        for (size_t i = q; i < q + 4; ++i) {
            float x = positions[i].x - bx, y = positions[i].y - by, z = positions[i].z - bz;
            if ((x > 0.99f && east) || (x < 0.01f && west) || (z > 0.99f && south) || (z < 0.01f && north))
                mask |= edgeSide(uvs[i].x, uLow, uHigh, du, Left, Right);
            if ((y > 0.9f && above.has(part)) || (y < 0.1f && below.has(part))) mask |= edgeSide(uvs[i].y, vLow, vHigh, dv, Top, Bottom);
        }
        auto moved = sprites::shift(uLow, vLow, mask & ruleSides(glassRule()));
        if (!moved) return;
        for (size_t i = q; i < q + 4; ++i) {
            uvs[i].x += moved->u;
            uvs[i].y += moved->v;
        }
    };
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
        bool glass = uvs[q].x >= uLow - e && uvs[q].x <= uHigh + e && uvs[q].y >= vLow - e && uvs[q].y <= vHigh + e;
        if (!glass) continue;
        // L-208: the post's own faces take the outside corner of an L. L-255/
        // L-271: a face that ends at a concave corner takes the inside edge -
        // one end slides, two ends split so neither end compresses the texture.
        if (edges.outside) {
            auto corner = paneCornerFor(part, x0, x1, z0, z1, parts);
            if (corner == PaneCorner::Outside && outside) {
                slide(q, *outside);
            } else if (corner == PaneCorner::Inside && edges.inside) {
                auto plan = paneSlidePlan(part, x0, x1, z0, z1, parts);
                auto pieces = panePieces(plan);
                if (pieces.size() > 1) {
                    size_t first = splitPaneFace(tessellator, q, plan, bx, by, bz, x0, x1, z0, z1, uLow, uHigh);
                    for (size_t s = first; s && s + 4 <= positions.size(); s += 4) shade(s, part);
                } else if (pieces.size() == 1) {
                    slide(q, pieces[0].lowCorner ? plan.atLow : plan.atHigh);
                }
            }
        }
        shade(q, part);
    }
}
LL_TYPE_INSTANCE_HOOK(ConnectedPane, ll::memory::HookPriority::Normal, BlockTessellator,
    &BlockTessellator::tessellateDoubleThinFenceInWorld, bool, Tessellator& tessellator, Block const& block,
    BlockPos const& p, bool singleSide) {
    bool connecting = false;
    try { connecting = drawingEnabled() && mRegion && connectsPane(block.getTypeName()); } catch (...) {}
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
// Each dimension's coordinator ticks on the client thread. Chunks keep the
// look they were built with, so every coordinator rebuilds its chunks when
// its last state differs from the switch, and once when first seen while the
// switch is on: chunks built before that first tick (joining a world) may
// have missed it, and far ones were only rebuilt when approached (2026-10-11).
// A new sprite lookup (after the terrain atlas was rebuilt) rebuilds them too.
// Client thread only.
struct Built { bool on = false; bool inside = false, pane = false; int sprites = 0; };
std::unordered_map<RenderChunkCoordinator const*, Built> built;
int spriteVersion = 0;
LL_TYPE_INSTANCE_HOOK(ConnectedRebuild, ll::memory::HookPriority::Normal, RenderChunkCoordinator,
    &RenderChunkCoordinator::tick, void) {
    origin();
    try {
        if (capabilities::registry().status(capabilities::Id::ConnectedTextures).available() && sprites::refresh()) ++spriteVersion;
        bool want = wanted();
        active = want;
        auto const& visuals = Runtime::instance().snapshot()->visuals;
        bool wantInside = want && visuals.connectedCorners, wantPane = want && visuals.connectedPaneCorners;
        insideCorners = wantInside;
        paneCorners = wantPane;
        auto [state, first] = built.try_emplace(this, Built{false, false, false, spriteVersion});
        if (first ? want : (state->second.on != want || state->second.inside != wantInside || state->second.pane != wantPane
                || (want && state->second.sprites != spriteVersion))) {
            state->second = {want, wantInside, wantPane, spriteVersion};
            _setAllDirty(false, false);
        }
    } catch (...) {}
}
struct Hook { int (*install)(bool); bool (*remove)(bool); bool installed = false; };
Hook hooks[] = {{ConnectedBlock::hook, ConnectedBlock::unhook}, {ConnectedDown::hook, ConnectedDown::unhook},
    {ConnectedUp::hook, ConnectedUp::unhook}, {ConnectedNorth::hook, ConnectedNorth::unhook},
    {ConnectedSouth::hook, ConnectedSouth::unhook}, {ConnectedWest::hook, ConnectedWest::unhook},
    {ConnectedEast::hook, ConnectedEast::unhook}, {ConnectedPane::hook, ConnectedPane::unhook},
    {ConnectedPaneGlass::hook, ConnectedPaneGlass::unhook}, {ConnectedRebuild::hook, ConnectedRebuild::unhook}};
}
bool start() {
    if (installed) return true;
    // Chunk meshes and the terrain atlas are version-sensitive: on an
    // unverified game, stay vanilla.
    capabilities::begin(capabilities::Id::ConnectedTextures);
    if (capabilities::registry().status(capabilities::Id::ConnectedTextures).compatibility != capabilities::Reason::None) {
        versionSensitiveAllowed(capabilities::Id::ConnectedTextures); return true;
    }
    // Chunks built on joining a world already follow the switch.
    try { active = Runtime::instance().snapshot()->visuals.connectedTextures; } catch (...) {}
    // Sprites are added whenever the game is verified, so the switch only
    // rebuilds chunks; they must be in place before the terrain atlas is built.
    if (!sprites::start()) {
        capabilities::fail(capabilities::Id::ConnectedTextures,capabilities::Reason::OperationFailed,"terrain atlas initialization");
        return false;
    }
    for (auto& hook : hooks) {
        if (hook.installed) continue;
        if (hook.install(true) != 0) {
            capabilities::hooks(capabilities::Id::ConnectedTextures,false);
            stop();
            return false;
        }
        hook.installed = true;
    }
    installed = true;
    capabilities::ready(capabilities::Id::ConnectedTextures);
    return true;
}
void stop() {
    capabilities::stop(capabilities::Id::ConnectedTextures);
    active = false;
    bool removed = true;
    for (auto& hook : hooks) if (hook.installed) {
        if (hook.remove(true)) hook.installed = false;
        else removed = false;
    }
    if (!removed) capabilities::fail(capabilities::Id::ConnectedTextures,capabilities::Reason::RemovalFailed);
    sprites::stop();
    built.clear();
    installed = !removed;
}
}
