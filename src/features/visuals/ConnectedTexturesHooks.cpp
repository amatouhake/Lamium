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
    // Bounded: whether repeat draws differed from the first (dark patches).
    static std::atomic<int> differences{0};
    if (differences < 12) try {
        auto& normals = *mesh.mNormals;
        auto& quadsSeen = *tessellator.mQuadInfoList;
        bool normal = normals.size() >= dst + 4 && normals[dst] != normals[src];
        bool facing = quadsSeen.size() > dst / 4 && quadsSeen[dst / 4].facing != quadsSeen[src / 4].facing;
        if (normal || facing) {
            ++differences;
            Runtime::instance().self().getLogger().info(
                "Connected Textures: a repeat draw differed: normal {} ({:.2f} {:.2f} {:.2f} vs {:.2f} {:.2f} {:.2f}), facing {} ({} vs {})",
                normal, normals.size() > dst ? normals[dst].x : 0.f, normals.size() > dst ? normals[dst].y : 0.f,
                normals.size() > dst ? normals[dst].z : 0.f, normals.size() > src ? normals[src].x : 0.f,
                normals.size() > src ? normals[src].y : 0.f, normals.size() > src ? normals[src].z : 0.f, facing,
                quadsSeen.size() > dst / 4 ? static_cast<int>(quadsSeen[dst / 4].facing) : -1,
                quadsSeen.size() > src / 4 ? static_cast<int>(quadsSeen[src / 4].facing) : -1);
        }
    } catch (...) {}
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
// Bounded dump of a split face's vertex data before and after (dark patches
// at night, 2026-10-11).
std::atomic<int> dumps{0}, paneDumps{0}, outlierLogs{0};
std::string vertexText(Tessellator& tessellator, size_t from, size_t count) {
    auto& mesh = *tessellator.mMeshData;
    std::string out;
    for (size_t v = from; v < from + count; ++v) {
        auto const& p = (*mesh.mPositions)[v];
        out += std::format("\n    v{} pos {:.3f} {:.3f} {:.3f} uv {:.4f} {:.4f}", v - from, p.x, p.y, p.z, (*mesh.mTextureUVs[0])[v].x,
                           (*mesh.mTextureUVs[0])[v].y);
        if (mesh.mColors->size() > v) out += std::format(" color {:08x}", (*mesh.mColors)[v]);
        for (size_t set = 1; set < 3; ++set)
            if (mesh.mTextureUVs[set]->size() > v)
                out += std::format(" uv{} {:.4f} {:.4f}", set, (*mesh.mTextureUVs[set])[v].x, (*mesh.mTextureUVs[set])[v].y);
        if (mesh.mNormals->size() > v) out += std::format(" n {:.2f} {:.2f} {:.2f} {:.2f}", (*mesh.mNormals)[v].x, (*mesh.mNormals)[v].y,
                                                         (*mesh.mNormals)[v].z, (*mesh.mNormals)[v].w);
    }
    return out;
}
void shapeCells(Tessellator& tessellator, TextureUVCoordinateSet const& tex, size_t before, std::vector<Cell> const& cells) {
    TexRect rect{tex._u0, tex._v0, tex._u1, tex._v1};
    bool dump = dumps < 4;
    std::string text;
    if (dump) text = std::format("Connected Textures dump: {} cells, drawn{}", cells.size(), vertexText(tessellator, before, 4 * cells.size()));
    for (size_t k = cells.size(); k-- > 0;) reshape(tessellator, before, before + 4 * k, cells[k], rect);
    if (dump) {
        ++dumps;
        text += "\n  shaped" + vertexText(tessellator, before, 4 * cells.size());
        try { Runtime::instance().self().getLogger().info("{}", text); } catch (...) {}
    }
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
// What to do with each quad of a pane, planned from vanilla's first draw:
// thin top/bottom faces fold where the neighbor above/below has the part;
// glass faces split into cells along their joined edges (as glass blocks:
// two texels filled from the middle), every other quad stays.
struct PaneQuad {
    bool thin = false, fold = false, glass = false;
    float fu0 = 0, fu1 = 1, fv0 = 0, fv1 = 1; // the part of the glass texture it shows
    std::vector<Cell> cells;
};
std::vector<PaneQuad> planPane(Tessellator& tessellator, Block const& block, size_t before, size_t after) {
    auto& positions = *tessellator.mMeshData->mPositions;
    auto& uvs = *tessellator.mMeshData->mTextureUVs[0];
    std::vector<PaneQuad> plan;
    if (after <= before || before % 4 || (after - before) % 4 || uvs.size() < after || !pane.glass.known) return plan;
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
    float du = g.u1 - g.u0, dv = g.v1 - g.v0;
    if (du == 0 || dv == 0) return plan;
    Rule rule = *ruleFor("minecraft:glass");
    constexpr float e = 1e-4f;
    for (size_t q = before; q < after; q += 4) {
        PaneQuad quad;
        float x0 = 2, x1 = -1, y0 = 2, y1 = -1, z0 = 2, z1 = -1;
        std::array<float, 4> fu{}, fv{};
        for (size_t i = 0; i < 4; ++i) {
            float x = positions[q + i].x - bx, y = positions[q + i].y - by, z = positions[q + i].z - bz;
            x0 = std::min(x0, x); x1 = std::max(x1, x);
            y0 = std::min(y0, y); y1 = std::max(y1, y);
            z0 = std::min(z0, z); z1 = std::max(z1, z);
            fu[i] = (uvs[q + i].x - g.u0) / du;
            fv[i] = (uvs[q + i].y - g.v0) / dv;
        }
        auto part = partOf(x0, x1, z0, z1);
        if (y1 - y0 < 0.01f) {
            quad.thin = true;
            bool top = y0 > 0.5f;
            quad.fold = (top && above.has(part)) || (!top && below.has(part));
            plan.push_back(std::move(quad));
            continue;
        }
        quad.fu0 = *std::min_element(fu.begin(), fu.end());
        quad.fu1 = *std::max_element(fu.begin(), fu.end());
        quad.fv0 = *std::min_element(fv.begin(), fv.end());
        quad.fv1 = *std::max_element(fv.begin(), fv.end());
        quad.glass = quad.fu0 > -e && quad.fu1 < 1 + e && quad.fv0 > -e && quad.fv1 < 1 + e;
        if (!quad.glass) {
            plan.push_back(std::move(quad));
            continue;
        }
        // Which ends of the texture this face joins: a corner on a block
        // edge next to the same pane, or at the top/bottom under (over) the
        // same part.
        bool uLow = false, uHigh = false, vLow = false, vHigh = false;
        for (size_t i = 0; i < 4; ++i) {
            float x = positions[q + i].x - bx, y = positions[q + i].y - by, z = positions[q + i].z - bz;
            bool side = (x > 0.99f && east) || (x < 0.01f && west) || (z > 0.99f && south) || (z < 0.01f && north);
            bool vertical = (y > 0.9f && above.has(part)) || (y < 0.1f && below.has(part));
            if (side) (fu[i] < 0.5f ? uLow : uHigh) = true;
            if (vertical) (fv[i] < 0.5f ? vLow : vHigh) = true;
        }
        auto across = clip(spans(uLow, rule.left, uHigh, rule.right, rule.leftFrom, rule.rightFrom), quad.fu0, quad.fu1);
        auto down = clip(spans(vLow, rule.top, vHigh, rule.bottom, rule.topFrom, rule.bottomFrom), quad.fv0, quad.fv1);
        for (auto const& a : across)
            for (auto const& d : down) quad.cells.push_back({a.at0, a.at1, d.at0, d.at1, a.from0, a.from1, d.from0, d.from1});
        plan.push_back(std::move(quad));
    }
    return plan;
}
// Copies of the pane follow the first draw (`count` vertices each). Each
// quad keeps its first copy unless planned otherwise; its further copies
// become its extra cells, the rest fold to a point.
void applyPane(Tessellator& tessellator, size_t before, size_t count, size_t copies, std::vector<PaneQuad> const& plan) {
    auto& positions = *tessellator.mMeshData->mPositions;
    if (positions.size() < before + count * copies) return;
    auto fold = [&](size_t q) { for (size_t i = q + 1; i < q + 4; ++i) positions[i] = positions[q]; };
    TexRect rect{pane.glass.u0, pane.glass.v0, pane.glass.u1, pane.glass.v1};
    for (size_t j = 0; j < plan.size(); ++j) {
        auto const& quad = plan[j];
        size_t src = before + 4 * j;
        for (size_t k = copies; k-- > 0;) {
            size_t dst = before + k * count + 4 * j;
            if (quad.thin) {
                if (k > 0 || quad.fold) fold(dst);
            } else if (quad.glass && k < quad.cells.size() && quad.cells.size() > 1 && copies >= quad.cells.size()) {
                reshape(tessellator, src, dst, quad.cells[k], rect);
            } else if (k > 0) {
                fold(dst);
            }
        }
    }
}
// Copies of quads appended straight to the tessellator's arrays. Drawing a
// pane again for its extra cells also drew again a part the pane writes
// elsewhere, unfolded, which stacked translucent glass dark (2026-10-11).
// Every per-vertex array in use, the per-quad info, the index list and the
// vertex count must agree before anything is appended.
template <class Visit>
void eachVertexArray(mce::MeshData& mesh, Visit&& visit) {
    visit(*mesh.mPositions);
    visit(*mesh.mNormals);
    visit(*mesh.mTangents);
    visit(*mesh.mColors);
    visit(*mesh.mBoneId0s);
    for (size_t set = 0; set < 3; ++set) visit(*mesh.mTextureUVs[set]);
    visit(*mesh.mPBRTextureIndices);
    visit(*mesh.mMERS);
    visit(*mesh.mGeoType);
}
std::atomic<int> appendLogs{0};
bool canAppend(Tessellator& tessellator) {
    auto& mesh = *tessellator.mMeshData;
    size_t n = mesh.mPositions->size();
    bool ok = n % 4 == 0;
    eachVertexArray(mesh, [&](auto const& values) { ok = ok && (values.empty() || values.size() == n); });
    size_t quads = tessellator.mQuadInfoList->size(), indices = mesh.mIndices->size();
    ok = ok && (quads == 0 || quads * 4 == n) && (indices == 0 || indices == n / 4 * 6);
    if (appendLogs < 2) {
        ++appendLogs;
        try {
            Runtime::instance().self().getLogger().info(
                "Connected Textures: pane copies by appending {}: {} vertices, {} quad infos, {} indices, count {}", ok, n, quads,
                indices, static_cast<unsigned>(tessellator.mCount));
        } catch (...) {}
    }
    return ok;
}
// Appends a copy of the quad starting at vertex `src` (checked by canAppend).
void appendQuad(Tessellator& tessellator, size_t src) {
    auto& mesh = *tessellator.mMeshData;
    size_t n = mesh.mPositions->size();
    eachVertexArray(mesh, [&](auto& values) {
        if (values.size() != n) return;
        for (size_t i = 0; i < 4; ++i) {
            auto value = values[src + i];
            values.push_back(value);
        }
    });
    auto& quads = *tessellator.mQuadInfoList;
    if (!quads.empty()) {
        auto info = quads[src / 4];
        quads.push_back(info);
    }
    auto& indices = *mesh.mIndices;
    if (!indices.empty()) {
        size_t from = src / 4 * 6;
        for (size_t m = 0; m < 6; ++m) {
            unsigned index = static_cast<unsigned>(indices[from + m] - src + n);
            indices.push_back(index);
        }
    }
    if (tessellator.mCount == n) tessellator.mCount = static_cast<unsigned>(n + 4);
}
// Swaps two whole quads in every vertex array and the quad info; the index
// list repeats the same pattern per quad, so it stays valid.
void swapQuads(Tessellator& tessellator, size_t a, size_t b) {
    auto& mesh = *tessellator.mMeshData;
    size_t n = mesh.mPositions->size();
    eachVertexArray(mesh, [&](auto& values) {
        if (values.size() != n) return;
        for (size_t i = 0; i < 4; ++i) std::swap(values[a + i], values[b + i]);
    });
    auto& quads = *tessellator.mQuadInfoList;
    if (quads.size() > a / 4 && quads.size() > b / 4) std::swap(quads[a / 4], quads[b / 4]);
}
void reverseCopies(Tessellator& tessellator, size_t before, size_t count, size_t copies) {
    size_t quads = count / 4;
    for (size_t k = 1; k < copies; ++k)
        for (size_t j = 0; j < quads / 2; ++j)
            swapQuads(tessellator, before + k * count + 4 * j, before + k * count + 4 * (quads - 1 - j));
}
LL_TYPE_INSTANCE_HOOK(ConnectedPane,ll::memory::HookPriority::Normal, BlockTessellator,
    &BlockTessellator::tessellateDoubleThinFenceInWorld, bool, Tessellator& tessellator, Block const& block,
    BlockPos const& p, bool singleSide) {
    bool connecting = false;
    try { connecting = active.load(std::memory_order_relaxed) && mRegion && connectsPane(block.getTypeName()); } catch (...) {}
    if (!connecting) return origin(tessellator, block, p, singleSide);
    auto saved = pane;
    pane = {&block, p, mRegion};
    size_t before = tessellator.mMeshData->mPositions->size();
    bool result = origin(tessellator, block, p, singleSide);
    try {
        size_t count = tessellator.mMeshData->mPositions->size() - before;
        auto plan = planPane(tessellator, block, before, before + count);
        size_t copies = 1;
        for (auto const& quad : plan) copies = std::max(copies, quad.cells.size());
        // Copy the pane's quads once per extra cell, in draw order, so copy k of
        // quad j sits at before + k * count + 4 * j; without consistent arrays,
        // only fold.
        bool same = tessellator.mMeshData->mPositions->size() == before + count;
        if (copies > 1 && same && canAppend(tessellator)) {
            for (size_t k = 1; k < copies; ++k)
                for (size_t j = 0; j < count / 4; ++j) appendQuad(tessellator, before + 4 * j);
        } else {
            copies = 1;
        }
        // Only panes with an arm toward the south or west (dark there, 2026-10-11).
        bool southOrWest = false;
        try {
            auto parts = partsAt(block, p);
            southOrWest = parts.has(Part::South) || parts.has(Part::West);
        } catch (...) {}
        bool dump = paneDumps < 3 && copies > 1 && southOrWest;
        std::string text;
        if (dump) {
            text = std::format("Connected Textures pane dump at {} {} {}: {} quads x {} copies, same {}", p.x, p.y, p.z, count / 4, copies, same);
            for (size_t j = 0; j < plan.size(); ++j)
                text += std::format("\n  quad {} thin {} fold {} glass {} f {:.3f}..{:.3f} x {:.3f}..{:.3f} cells {}", j, plan[j].thin,
                                    plan[j].fold, plan[j].glass, plan[j].fu0, plan[j].fu1, plan[j].fv0, plan[j].fv1, plan[j].cells.size());
        }
        if (!plan.empty() && same) applyPane(tessellator, before, count, copies, plan);
        // L-133 trial: reverse the quad order inside each appended copy, so a
        // face pair there is emitted back to front. If the dark half moves to
        // the opposite view direction, copies are drawn in emission order.
        if (copies > 1 && same) reverseCopies(tessellator, before, count, copies);
        // Bounded: vertices whose light differs from the pane's first vertex
        // (edges darkening in daylight, 2026-10-11).
        if (outlierLogs < 40) try {
            auto& mesh = *tessellator.mMeshData;
            auto& light = *mesh.mTextureUVs[1];
            auto& colors = *mesh.mColors;
            size_t end = mesh.mPositions->size();
            if (light.size() >= end && end > before) {
                auto base = light[before];
                for (size_t v = before; v < end && outlierLogs < 40; ++v) {
                    if (std::abs(light[v].x - base.x) < 1e-5f && std::abs(light[v].y - base.y) < 1e-5f) continue;
                    ++outlierLogs;
                    size_t local = v - before;
                    auto const& pos = (*mesh.mPositions)[v];
                    Runtime::instance().self().getLogger().info(
                        "Connected Textures light outlier at pane {} {} {}: vertex {} (copy {}, quad {}) pos {:.3f} {:.3f} {:.3f} light {:.4f} {:.4f} vs {:.4f} {:.4f} color {:08x} copies {} count {}",
                        p.x, p.y, p.z, local, local / std::max<size_t>(count, 1), local % std::max<size_t>(count, 1) / 4, pos.x, pos.y,
                        pos.z, light[v].x, light[v].y, base.x, base.y, colors.size() > v ? colors[v] : 0u, copies, count);
                }
            }
        } catch (...) {}
        if (dump) {
            ++paneDumps;
            text += "\n  after" + vertexText(tessellator, before, count * copies);
            try { Runtime::instance().self().getLogger().info("{}", text); } catch (...) {}
        }
    } catch (...) {}
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
    supported = verifiedGameExecutable();
    if (!supported) {
        Runtime::instance().self().getLogger().warn("Connected Textures: vanilla glass retained (unverified game version)");
        return true;
    }
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
