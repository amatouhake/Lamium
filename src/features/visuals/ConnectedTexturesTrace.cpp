#include "features/visuals/ConnectedTexturesTrace.h"
#ifdef LAMIUM_CTM_TRACE
#include "features/visuals/ConnectedTextures.h"
#include "app/Runtime.h"
#include "ll/api/memory/Hook.h"
#include "mc/client/renderer/Tessellator.h"
#include "mc/deps/minecraft_renderer/renderer/MeshData.h"
#include <algorithm>
#include "mc/client/renderer/block/BlockTessellator.h"
#include "mc/client/renderer/block/BlockGraphics.h"
#include "mc/client/renderer/block/TextureItem.h"
#include "mc/client/renderer/texture/TextureUVCoordinateSet.h"
#include "mc/deps/core/math/Vec2.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/block/BlockType.h"
#include "mc/world/phys/AABB.h"
#include <cmath>
#include <vector>
#include <atomic>
#include <format>
#include <string>

// Round 2 (2026-10-11): glass panes. Glass blocks are the shipped feature;
// this spike only watches pane tessellation: which texture lookups and UV
// calls it makes, and whether a trimmed lookup joins neighboring panes.
namespace lamium::visuals::connectedTexturesTrace {
namespace {
using namespace connected;
template <class... Args>
void log(std::format_string<Args...> format, Args&&... args) noexcept {
    try { Runtime::instance().self().getLogger().info(std::format(format, std::forward<Args>(args)...)); } catch (...) {}
}
struct Pane {
    Block const* block = nullptr;
    BlockPos pos{};
    BlockSource const* region = nullptr;
    int lookups = 0, uvs = 0;
};
thread_local Pane pane;
thread_local TextureUVCoordinateSet trimmedSet;
std::atomic<int> paneLogs{0}, lookupLogs{0}, uvLogs{0}, graphicsLogs{0}, foldLogs{0};

bool isPane(Block const& block) { return block.getTypeName().ends_with("glass_pane"); }
bool sameAt(Block const& block, Offset o) {
    BlockPos at{pane.pos.x + o.x, pane.pos.y + o.y, pane.pos.z + o.z};
    return &pane.region->getBlock(at).getBlockType() == &block.getBlockType();
}

LL_TYPE_INSTANCE_HOOK(PaneFence, ll::memory::HookPriority::Normal, BlockTessellator,
    &BlockTessellator::tessellateDoubleThinFenceInWorld, bool, Tessellator& tessellator, Block const& block,
    BlockPos const& p, bool singleSide) {
    bool watch = false;
    // Round 4: follow the Connected Textures switch so panes can be compared
    // on and off (the switch already rebuilds the chunks).
    try {
        auto& runtime = Runtime::instance();
        watch = runtime.enabled() && runtime.snapshot()->visuals.connectedTextures && isPane(block) && mRegion;
    } catch (...) {}
    if (!watch) return origin(tessellator, block, p, singleSide);
    auto saved = pane;
    pane = {&block, p, mRegion};
    auto& positions = *tessellator.mMeshData->mPositions;
    size_t before = positions.size();
    bool result = origin(tessellator, block, p, singleSide);
    // Round 5: fold the pane's top faces when a pane sits on it, and its
    // bottom faces when it sits on a pane: quads whose four corners all lie
    // at the pane's highest (lowest) height.
    try {
        size_t after = positions.size();
        bool above = sameAt(block, {0, 1, 0}), below = sameAt(block, {0, -1, 0});
        if ((above || below) && before % 4 == 0 && after > before && (after - before) % 4 == 0) {
            float top = positions[before].y, bottom = top;
            for (size_t i = before; i < after; ++i) {
                top = std::max(top, positions[i].y);
                bottom = std::min(bottom, positions[i].y);
            }
            // Round 8: fold only between panes of the same shape (the boxes of
            // their center and arms match); mixed stacks keep the vanilla
            // faces and their line. Partial folds and shortening (rounds 6-7)
            // left odd gaps where the shapes differ.
            auto shapeOf = [&](BlockPos at) {
                std::vector<AABB> boxes;
                auto const& other = pane.region->getBlock(at);
                other.getBlockType().addAABBs(other, *pane.region, at, nullptr, boxes);
                for (auto& box : boxes) {
                    box.min.x -= at.x; box.max.x -= at.x;
                    box.min.z -= at.z; box.max.z -= at.z;
                }
                return boxes;
            };
            auto sameShape = [](std::vector<AABB> const& a, std::vector<AABB> const& b) {
                if (a.size() != b.size()) return false;
                constexpr float e = 0.001f;
                for (size_t i = 0; i < a.size(); ++i)
                    if (std::abs(a[i].min.x - b[i].min.x) > e || std::abs(a[i].max.x - b[i].max.x) > e
                        || std::abs(a[i].min.z - b[i].min.z) > e || std::abs(a[i].max.z - b[i].max.z) > e) return false;
                return true;
            };
            auto own = shapeOf(p);
            bool foldTop = above && sameShape(own, shapeOf({p.x, p.y + 1, p.z}));
            bool foldBottom = below && sameShape(own, shapeOf({p.x, p.y - 1, p.z}));
            int folded = 0, shortened = 0;
            for (size_t q = before; q < after; q += 4) {
                bool atTop = true, atBottom = true;
                for (size_t i = q; i < q + 4; ++i) {
                    atTop = atTop && positions[i].y == top;
                    atBottom = atBottom && positions[i].y == bottom;
                }
                if ((foldTop && atTop) || (foldBottom && atBottom)) {
                    for (size_t i = q + 1; i < q + 4; ++i) positions[i] = positions[q];
                    ++folded;
                }
            }
            if (foldLogs < 20) {
                ++foldLogs;
                log("L-96 pane at {} {} {}: {} vertices (from {}), y {:.3f}..{:.3f}, above {} below {}, folded {} shortened {}", p.x, p.y,
                    p.z, after - before, before, bottom, top, above, below, folded, shortened);
            }
        }
    } catch (...) {}
    if (paneLogs < 12) {
        ++paneLogs;
        log("L-96 pane {} at {} {} {} singleSide {}: {} texture lookups, {} uv calls", block.getTypeName(), p.x, p.y, p.z,
            singleSide, pane.lookups, pane.uvs);
    }
    pane = saved;
    return result;
}
LL_TYPE_INSTANCE_HOOK(PaneTexture, ll::memory::HookPriority::Normal, BlockTessellator, &BlockTessellator::_getTexture,
    TextureUVCoordinateSet const&, BlockPos const& pos, Block const& block, uchar face, int forcedVariant,
    BlockGraphics const* hint) {
    auto const& tex = origin(pos, block, face, forcedVariant, hint);
    if (!pane.block) return tex;
    ++pane.lookups;
    try {
        bool own = &block == pane.block && face < 6;
        Joined joined{};
        if (own) {
            auto s = sides(static_cast<Face>(face));
            joined = {sameAt(block, s.left), sameAt(block, s.right), sameAt(block, s.top), sameAt(block, s.bottom)};
        }
        if (lookupLogs < 60) {
            ++lookupLogs;
            log("L-96 pane lookup face {} of {} at {} {} {} (pane at {} {} {}): uv {:.5f},{:.5f} - {:.5f},{:.5f} "
                "image {}x{} joined L{} R{} T{} B{}",
                face, block.getTypeName(), pos.x, pos.y, pos.z, pane.pos.x, pane.pos.y, pane.pos.z, tex._u0, tex._v0,
                tex._u1, tex._v1, tex._sourceImageWidth, tex._sourceImageHeight, joined.left, joined.right, joined.top,
                joined.bottom);
        }
        if (!own) return tex;
        auto uv = trim({tex._u0, tex._v0, tex._u1, tex._v1}, tex._sourceImageWidth, tex._sourceImageHeight, joined);
        trimmedSet = tex;
        trimmedSet._u0 = uv.u0;
        trimmedSet._v0 = uv.v0;
        trimmedSet._u1 = uv.u1;
        trimmedSet._v1 = uv.v1;
        return trimmedSet;
    } catch (...) {}
    return tex;
}
// Round 2: panes made no _getTexture or _tex1 call; try the graphics and
// mapped-texture lookups, treating the slot as the face.
TextureUVCoordinateSet const& paneTexture(char const* via, TextureUVCoordinateSet const& tex, int slot, BlockPos const* at) {
    if (!pane.block) return tex;
    ++pane.lookups;
    try {
        // Round 3: the slot is a texture slot, not a face. Slot 0 is the glass
        // used on both sides of the pane, slot 5 the thin edge strip (left
        // alone). Left/right follow the pane's run: X neighbors as seen in
        // round 2, Z neighbors guessed the same way the east face reads;
        // top/bottom are the panes above and below.
        Joined joined{};
        auto* b = pane.block;
        if (slot == 0)
            joined = {sameAt(*b, {-1, 0, 0}) || sameAt(*b, {0, 0, 1}), sameAt(*b, {1, 0, 0}) || sameAt(*b, {0, 0, -1}),
                      sameAt(*b, {0, 1, 0}), sameAt(*b, {0, -1, 0})};
        if (graphicsLogs < 80) {
            ++graphicsLogs;
            log("L-96 pane {} slot {} at {} (pane {} {} {}): uv {:.5f},{:.5f} - {:.5f},{:.5f} image {}x{} joined L{} R{} T{} B{}",
                via, slot, at ? std::format("{} {} {}", at->x, at->y, at->z) : std::string("-"), pane.pos.x, pane.pos.y,
                pane.pos.z, tex._u0, tex._v0, tex._u1, tex._v1, tex._sourceImageWidth, tex._sourceImageHeight, joined.left,
                joined.right, joined.top, joined.bottom);
        }
        if (slot != 0) return tex;
        auto uv = trim({tex._u0, tex._v0, tex._u1, tex._v1}, tex._sourceImageWidth, tex._sourceImageHeight, joined);
        trimmedSet = tex;
        trimmedSet._u0 = uv.u0;
        trimmedSet._v0 = uv.v0;
        trimmedSet._u1 = uv.u1;
        trimmedSet._v1 = uv.v1;
        return trimmedSet;
    } catch (...) {}
    return tex;
}
using SlotTexture = TextureUVCoordinateSet const& (BlockGraphics::*)(uint64, int) const;
using PosTexture = TextureUVCoordinateSet const& (BlockGraphics::*)(BlockPos const&, uint64, int) const;
LL_TYPE_INSTANCE_HOOK(PaneGraphicsSlot, ll::memory::HookPriority::Normal, BlockGraphics,
    static_cast<SlotTexture>(&BlockGraphics::getTexture), TextureUVCoordinateSet const&, uint64 slot, int variant) {
    auto const& tex = origin(slot, variant);
    return paneTexture("graphics", tex, static_cast<int>(slot), nullptr);
}
LL_TYPE_INSTANCE_HOOK(PaneGraphicsPos, ll::memory::HookPriority::Normal, BlockGraphics,
    static_cast<PosTexture>(&BlockGraphics::getTexture), TextureUVCoordinateSet const&, BlockPos const& at, uint64 slot,
    int variant) {
    auto const& tex = origin(at, slot, variant);
    return paneTexture("graphics+pos", tex, static_cast<int>(slot), &at);
}
LL_STATIC_HOOK(PaneMapped, ll::memory::HookPriority::Normal, &BlockTessellator::_getMappedTexture,
    TextureUVCoordinateSet const&, Block const& block, uchar face) {
    auto const& tex = origin(block, face);
    return paneTexture("mapped", tex, face, nullptr);
}
LL_TYPE_INSTANCE_HOOK(PaneUv, ll::memory::HookPriority::Normal, BlockTessellator, &BlockTessellator::_tex1, void,
    Tessellator& tessellator, Vec2 const& uv) {
    if (pane.block) {
        ++pane.uvs;
        if (uvLogs < 40) {
            ++uvLogs;
            log("L-96 pane _tex1 {:.5f},{:.5f}", uv.x, uv.y);
        }
    }
    origin(tessellator, uv);
}
}
void start() {
    PaneFence::hook();
    PaneTexture::hook();
    PaneUv::hook();
    PaneGraphicsSlot::hook();
    PaneGraphicsPos::hook();
    PaneMapped::hook();
    Runtime::instance().self().getLogger().warn("Connected textures pane spike enabled (L-96)");
}
void stop() {
    PaneFence::unhook(true);
    PaneTexture::unhook(true);
    PaneUv::unhook(true);
    PaneGraphicsSlot::unhook(true);
    PaneGraphicsPos::unhook(true);
    PaneMapped::unhook(true);
}
}
#else
namespace lamium::visuals::connectedTexturesTrace { void start() {} void stop() {} }
#endif
