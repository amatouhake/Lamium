#include "features/visuals/ConnectedTexturesHooks.h"
#include "features/visuals/ConnectedTextures.h"
#include "app/Runtime.h"
#include "app/Versions.h"
#include "ll/api/memory/Hook.h"
#include "mc/client/renderer/Tessellator.h"
#include "mc/client/renderer/TessellatorQuadInfo.h"
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
#include <format>
#include <string>
#include <array>
#include <cstdint>
#include <atomic>
#include <cmath>
#include <unordered_map>
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
    if (current.block != &block || !current.region || current.rule.split) return out;
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
// The cells a split rule draws this face as; one cell means draw it as is.
std::vector<Cell> faceCells(Face face, Block const& block) {
    if (current.block != &block || !current.region || !current.rule.split) return {};
    if (current.rule.sidesOnly && (face == Face::Up || face == Face::Down)) return {};
    auto s = sides(face);
    return splitCells({sameAt(block, s.left), sameAt(block, s.right), sameAt(block, s.top), sameAt(block, s.bottom)},
                      current.rule);
}
// Gives the quad at `dst` (a copy of the quad at `src`) the cell's part of
// it: corners placed across the source quad (s, t from its u0/v0 corner) and
// texels from the texture's fractions (su, tv) of `tex`. Vanilla's corner
// shading (color and light) is interpolated to the cell's corners, so strips
// keep the face's gradient. Reads all of `src` before writing `dst`.
struct TexRect { float u0, v0, u1, v1; };
void reshape(Tessellator& tessellator, size_t src, size_t dst, Cell const& c, TexRect tex) {
    auto& positions = *tessellator.mMeshData->mPositions;
    auto& uvs = *tessellator.mMeshData->mTextureUVs[0];
    auto& colors = *tessellator.mMeshData->mColors;
    float du = tex.u1 - tex.u0, dv = tex.v1 - tex.v0;
    if (du == 0 || dv == 0 || positions.size() < dst + 4 || uvs.size() < dst + 4) return;
    // The source corners by where they sit on its part of the texture.
    std::array<float, 4> fu{}, fv{};
    for (size_t i = 0; i < 4; ++i) {
        fu[i] = (uvs[src + i].x - tex.u0) / du;
        fv[i] = (uvs[src + i].y - tex.v0) / dv;
    }
    float midU = (*std::min_element(fu.begin(), fu.end()) + *std::max_element(fu.begin(), fu.end())) / 2;
    float midV = (*std::min_element(fv.begin(), fv.end()) + *std::max_element(fv.begin(), fv.end())) / 2;
    std::array<bool, 4> high{}, low{};
    std::array<glm::vec3, 4> corner{};
    std::array<std::uint32_t, 4> cornerColor{};
    std::array<std::array<glm::vec2, 4>, 2> cornerLight{};
    bool hasColors = colors.size() >= dst + 4;
    std::array<bool, 2> hasLight{};
    for (size_t set = 0; set < 2; ++set) hasLight[set] = tessellator.mMeshData->mTextureUVs[set + 1]->size() >= dst + 4;
    // Positions by corner (the face is flat); shading by emitted vertex, as
    // the quad's two triangles blend it.
    std::array<float, 4> S{}, T{};
    for (size_t i = 0; i < 4; ++i) {
        high[i] = fu[i] > midU;
        low[i] = fv[i] > midV;
        S[i] = high[i] ? 1.f : 0.f;
        T[i] = low[i] ? 1.f : 0.f;
        corner[(high[i] ? 1 : 0) + (low[i] ? 2 : 0)] = positions[src + i];
        if (hasColors) cornerColor[i] = colors[src + i];
        for (size_t set = 0; set < 2; ++set)
            if (hasLight[set]) cornerLight[set][i] = (*tessellator.mMeshData->mTextureUVs[set + 1])[src + i];
    }
    for (size_t i = 0; i < 4; ++i) {
        size_t v = dst + i;
        float s = high[i] ? c.s1 : c.s0, t = low[i] ? c.t1 : c.t0;
        std::array<float, 4> p{(1 - s) * (1 - t), s * (1 - t), (1 - s) * t, s * t};
        positions[v] = corner[0] * p[0] + corner[1] * p[1] + corner[2] * p[2] + corner[3] * p[3];
        auto w = triangleWeights(S, T, s, t);
        if (hasColors) {
            std::uint32_t out = 0;
            for (int shift = 0; shift < 32; shift += 8) {
                float channel = 0;
                for (size_t k = 0; k < 4; ++k) channel += w[k] * static_cast<float>((cornerColor[k] >> shift) & 0xFFu);
                out |= static_cast<std::uint32_t>(std::clamp(std::lround(channel), 0L, 255L)) << shift;
            }
            colors[v] = out;
        }
        for (size_t set = 0; set < 2; ++set)
            if (hasLight[set]) {
                auto const& l = cornerLight[set];
                (*tessellator.mMeshData->mTextureUVs[set + 1])[v] = l[0] * w[0] + l[1] * w[1] + l[2] * w[2] + l[3] * w[3];
            }
        uvs[v].x = tex.u0 + du * (high[i] ? c.su1 : c.su0);
        uvs[v].y = tex.v0 + dv * (low[i] ? c.tv1 : c.tv0);
    }
    // Everything else per vertex comes from the first draw as well: a repeat
    // call need not write the same normal or material data (dark patches on
    // split faces at night, 2026-10-11).
    if (dst == src) return;
    auto& mesh = *tessellator.mMeshData;
    auto copyFrom = [&](auto& values) {
        if (values.size() >= dst + 4 && values.size() >= src + 4)
            for (size_t i = 0; i < 4; ++i) values[dst + i] = values[src + i];
    };
    copyFrom(*mesh.mNormals);
    copyFrom(*mesh.mTangents);
    copyFrom(*mesh.mBoneId0s);
    copyFrom(*mesh.mPBRTextureIndices);
    copyFrom(*mesh.mMERS);
    copyFrom(*mesh.mGeoType);
    // The quad info follows too, center included: translucent faces are drawn
    // sorted by their centers, and a pane's front and back are 2 texels apart,
    // so cells with their own centers swapped order with the view direction
    // and blended dark on one half (2026-10-11). Every cell sorts as its face.
    auto& quads = *tessellator.mQuadInfoList;
    if (src % 4 == 0 && dst % 4 == 0 && quads.size() > dst / 4 && quads.size() > src / 4) quads[dst / 4] = quads[src / 4];
}
// After a block face was drawn once per cell, give each copy its cell; the
// first copy is the source, so it goes last.
void shapeCells(Tessellator& tessellator, TextureUVCoordinateSet const& tex, size_t before, std::vector<Cell> const& cells) {
    TexRect rect{tex._u0, tex._v0, tex._u1, tex._v1};
    for (size_t k = cells.size(); k-- > 0;) reshape(tessellator, before, before + 4 * k, cells[k], rect);
}
#define LAMIUM_CONNECTED_FACE(Name, Function, FaceId)                                                                        \
    LL_TYPE_INSTANCE_HOOK(Name, ll::memory::HookPriority::Normal, BlockTessellator, &BlockTessellator::Function, void,     \
        Tessellator& tessellator, Block const& block, Vec3 const& p, TextureUVCoordinateSet const& tex) {                   \
        if (current.block != &block) return origin(tessellator, block, p, tex);                                             \
        std::vector<Cell> cells;                                                                                            \
        try { cells = faceCells(FaceId, block); } catch (...) {}                                                            \
        if (cells.size() > 1) {                                                                                             \
            size_t before = tessellator.mMeshData->mPositions->size();                                                     \
            origin(tessellator, block, p, tex);                                                                             \
            /* Only a plain four-corner face can be split. */                                                               \
            if (tessellator.mMeshData->mPositions->size() != before + 4) return;                                           \
            for (size_t k = 1; k < cells.size(); ++k) origin(tessellator, block, p, tex);                                  \
            try { shapeCells(tessellator, tex, before, cells); } catch (...) {}                                             \
            return;                                                                                                         \
        }                                                                                                                   \
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
// Panes keep vanilla's quads and move the glass texture inward on joined
// edges (a slight stretch). Drawing them split like glass blocks needed extra
// quads, and translucent panes then blended dark on the half of each block to
// the viewer's right (sorting of translucent faces; not found yet, 2026-10-11).
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
// Each dimension's coordinator ticks on the client thread. Chunks keep the
// look they were built with, so every coordinator rebuilds its chunks when
// its last state differs from the switch, and once when first seen while the
// switch is on: chunks built before that first tick (joining a world) may
// have missed it, and far ones were only rebuilt when approached (2026-10-11).
// Client thread only.
std::unordered_map<RenderChunkCoordinator const*, bool> built;
LL_TYPE_INSTANCE_HOOK(ConnectedRebuild, ll::memory::HookPriority::Normal, RenderChunkCoordinator,
    &RenderChunkCoordinator::tick, void) {
    origin();
    try {
        bool want = wanted();
        active = want;
        auto [state, first] = built.try_emplace(this, false);
        if (first ? want : state->second != want) {
            state->second = want;
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
    supported = versionSensitiveAllowed("Connected Textures (chunk mesh vertices)");
    if (!supported) return true;
    // Chunks built on joining a world already follow the switch.
    try { active = Runtime::instance().snapshot()->visuals.connectedTextures; } catch (...) {}
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
    built.clear();
    installed = false;
}
}
