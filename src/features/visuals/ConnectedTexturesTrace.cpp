#include "features/visuals/ConnectedTexturesTrace.h"
#ifdef LAMIUM_CTM_TRACE
#include "app/Runtime.h"
#include "ll/api/memory/Hook.h"
#include "mc/client/renderer/Tessellator.h"
#include "mc/client/renderer/block/BlockGraphics.h"
#include "mc/client/renderer/block/BlockTessellator.h"
#include "mc/client/renderer/block/TextureItem.h"
#include "mc/client/renderer/texture/TextureUVCoordinateSet.h"
#include "mc/deps/minecraft_renderer/renderer/MeshData.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/block/BlockType.h"
#include "mc/world/phys/AABB.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <format>
#include <string>
#include <vector>

// Round 10 (2026-10-11): glass panes, adjusted per part after vanilla builds
// the pane. The glass texture is shared by the center post and every arm, so
// trimming it as a whole (rounds 1-9) also cut borders where only one part
// touched a neighbor. Now, on the built mesh:
// - a glass face's side border goes where its block edge touches the same pane;
// - its top (bottom) border and the thin top (bottom) face go where the pane
//   above (below) has the same part: the center, or an arm in that direction.
// Parts come from the drawn geometry; a pane's arms from its shape boxes
// (each arm box reaches the block edge; the center post is always drawn).
namespace lamium::visuals::connectedTexturesTrace {
namespace {
template <class... Args>
void log(std::format_string<Args...> format, Args&&... args) noexcept {
    try { Runtime::instance().self().getLogger().info(std::format(format, std::forward<Args>(args)...)); } catch (...) {}
}
struct Glass { float u0 = 0, v0 = 0, u1 = 0, v1 = 0; int width = 16, height = 16; bool known = false; };
struct Pane {
    Block const* block = nullptr;
    BlockPos pos{};
    BlockSource const* region = nullptr;
    Glass glass;
};
thread_local Pane pane;
std::atomic<int> paneLogs{0};

bool isPane(Block const& block) { return block.getTypeName().ends_with("glass_pane"); }
enum Part { Center, East, West, South, North };
// Which arms the pane at `at` has, from its shape boxes; not present when it
// is not the same pane.
struct Arms { bool present = false; std::array<bool, 5> part{}; };
Arms armsAt(Block const& block, BlockPos at) {
    Arms arms;
    auto const& other = pane.region->getBlock(at);
    if (&other.getBlockType() != &block.getBlockType()) return arms;
    arms.present = true;
    arms.part[Center] = true;
    std::vector<AABB> boxes;
    other.getBlockType().addAABBs(other, *pane.region, at, nullptr, boxes);
    for (auto const& box : boxes) {
        if (box.max.x - at.x > 0.9f) arms.part[East] = true;
        if (box.min.x - at.x < 0.1f) arms.part[West] = true;
        if (box.max.z - at.z > 0.9f) arms.part[South] = true;
        if (box.min.z - at.z < 0.1f) arms.part[North] = true;
    }
    return arms;
}
bool sameAt(Block const& block, int dx, int dy, int dz) {
    return &pane.region->getBlock({pane.pos.x + dx, pane.pos.y + dy, pane.pos.z + dz}).getBlockType() == &block.getBlockType();
}
struct Rect { float x0 = 2, x1 = -1, y0 = 2, y1 = -1, z0 = 2, z1 = -1; };
Part partOf(Rect const& r) {
    if (r.x1 > 0.6f) return East;
    if (r.x0 < 0.4f) return West;
    if (r.z1 > 0.6f) return South;
    if (r.z0 < 0.4f) return North;
    return Center;
}
void adjust(Tessellator& tessellator, Block const& block, size_t before) {
    auto& positions = *tessellator.mMeshData->mPositions;
    auto& uvs = *tessellator.mMeshData->mTextureUVs[0];
    size_t after = positions.size();
    if (after <= before || before % 4 || (after - before) % 4 || uvs.size() < after || !pane.glass.known) return;
    float bx = positions[before].x, by = positions[before].y, bz = positions[before].z;
    for (size_t i = before; i < after; ++i) {
        bx = std::min(bx, positions[i].x);
        by = std::min(by, positions[i].y);
        bz = std::min(bz, positions[i].z);
    }
    bx = std::floor(bx);
    by = std::floor(by);
    bz = std::floor(bz);
    auto above = armsAt(block, {pane.pos.x, pane.pos.y + 1, pane.pos.z});
    auto below = armsAt(block, {pane.pos.x, pane.pos.y - 1, pane.pos.z});
    bool east = sameAt(block, 1, 0, 0), west = sameAt(block, -1, 0, 0), south = sameAt(block, 0, 0, 1),
         north = sameAt(block, 0, 0, -1);
    auto const& g = pane.glass;
    float du = (g.u1 - g.u0) / g.width, dv = (g.v1 - g.v0) / g.height;
    auto glassUv = [&](float u, float v) {
        constexpr float e = 1e-5f;
        return u >= std::min(g.u0, g.u1) - e && u <= std::max(g.u0, g.u1) + e && v >= std::min(g.v0, g.v1) - e
            && v <= std::max(g.v0, g.v1) + e;
    };
    auto inward = [](float value, float lo, float hi, float step) {
        if (std::abs(value - lo) < 1e-6f) return value + std::abs(step);
        if (std::abs(value - hi) < 1e-6f) return value - std::abs(step);
        return value;
    };
    int folded = 0, trimmed = 0;
    for (size_t q = before; q < after; q += 4) {
        Rect r;
        for (size_t i = q; i < q + 4; ++i) {
            float x = positions[i].x - bx, y = positions[i].y - by, z = positions[i].z - bz;
            r.x0 = std::min(r.x0, x); r.x1 = std::max(r.x1, x);
            r.y0 = std::min(r.y0, y); r.y1 = std::max(r.y1, y);
            r.z0 = std::min(r.z0, z); r.z1 = std::max(r.z1, z);
        }
        auto part = partOf(r);
        if (r.y1 - r.y0 < 0.01f) {
            // The thin top or bottom face: fold it where the neighbor has the part.
            bool top = r.y0 > 0.5f;
            if ((top && above.part[part]) || (!top && below.part[part])) {
                for (size_t i = q + 1; i < q + 4; ++i) positions[i] = positions[q];
                ++folded;
            }
            continue;
        }
        if (!glassUv(uvs[q].x, uvs[q].y)) continue;
        // A glass face: move the border texel inward on joined edges.
        float uMin = uvs[q].x, uMax = uMin, vMin = uvs[q].y, vMax = vMin;
        for (size_t i = q; i < q + 4; ++i) {
            uMin = std::min(uMin, uvs[i].x); uMax = std::max(uMax, uvs[i].x);
            vMin = std::min(vMin, uvs[i].y); vMax = std::max(vMax, uvs[i].y);
        }
        bool any = false;
        for (size_t i = q; i < q + 4; ++i) {
            float x = positions[i].x - bx, y = positions[i].y - by, z = positions[i].z - bz;
            bool side = (x > 0.99f && east) || (x < 0.01f && west) || (z > 0.99f && south) || (z < 0.01f && north);
            bool vertical = (y > 0.9f && above.part[part]) || (y < 0.1f && below.part[part]);
            if (side) {
                uvs[i].x = inward(uvs[i].x, uMin, uMax, du);
                any = true;
            }
            if (vertical) {
                uvs[i].y = inward(uvs[i].y, vMin, vMax, dv);
                any = true;
            }
        }
        trimmed += any;
    }
    if (paneLogs < 20) {
        ++paneLogs;
        log("L-96 pane {} {} {}: {} quads, folded {}, trimmed {}; above {} below {}; side E{} W{} S{} N{}", pane.pos.x,
            pane.pos.y, pane.pos.z, (after - before) / 4, folded, trimmed, above.present, below.present, east, west, south, north);
    }
}

LL_TYPE_INSTANCE_HOOK(PaneFence, ll::memory::HookPriority::Normal, BlockTessellator,
    &BlockTessellator::tessellateDoubleThinFenceInWorld, bool, Tessellator& tessellator, Block const& block,
    BlockPos const& p, bool singleSide) {
    bool watch = false;
    // Follows the Connected Textures switch, which rebuilds chunks on change.
    try {
        auto& runtime = Runtime::instance();
        watch = runtime.enabled() && runtime.snapshot()->visuals.connectedTextures && isPane(block) && mRegion;
    } catch (...) {}
    if (!watch) return origin(tessellator, block, p, singleSide);
    auto saved = pane;
    pane = {&block, p, mRegion};
    size_t before = tessellator.mMeshData->mPositions->size();
    bool result = origin(tessellator, block, p, singleSide);
    try { adjust(tessellator, block, before); } catch (...) {}
    pane = saved;
    return result;
}
// Panes read their glass from slot 0 (both sides) and the thin edge from
// slot 5 (round 2); remember the glass rectangle, change nothing.
using PosTexture = TextureUVCoordinateSet const& (BlockGraphics::*)(BlockPos const&, uint64, int) const;
LL_TYPE_INSTANCE_HOOK(PaneGlass, ll::memory::HookPriority::Normal, BlockGraphics,
    static_cast<PosTexture>(&BlockGraphics::getTexture), TextureUVCoordinateSet const&, BlockPos const& at, uint64 slot,
    int variant) {
    auto const& tex = origin(at, slot, variant);
    if (pane.block && slot == 0)
        pane.glass = {tex._u0, tex._v0, tex._u1, tex._v1, tex._sourceImageWidth ? tex._sourceImageWidth : 16,
                      tex._sourceImageHeight ? tex._sourceImageHeight : 16, true};
    return tex;
}
}
void start() {
    PaneFence::hook();
    PaneGlass::hook();
    Runtime::instance().self().getLogger().warn("Connected textures pane spike enabled (L-96)");
}
void stop() {
    PaneFence::unhook(true);
    PaneGlass::unhook(true);
}
}
#else
namespace lamium::visuals::connectedTexturesTrace { void start() {} void stop() {} }
#endif
