#include "features/inspection/render/IconTrace.h"
#ifdef LAMIUM_ICON_TRACE
#include "app/Runtime.h"
#include "app/TraceLog.h"
#include "ll/api/memory/Hook.h"
#include "mc/client/gui/controls/renderers/InventoryItemRenderer.h"
#include "mc/client/renderer/actor/ItemRenderer.h"
#include "mc/client/renderer/ActorShaderManager.h"
#include "mc/deps/core/math/Color.h"
#include "mc/deps/core/math/Vec2.h"
#include "mc/deps/core/math/Vec4.h"
#include "mc/client/renderer/BaseActorRenderContext.h"
#include "mc/client/renderer/screen/MinecraftUIRenderContext.h"
#include "mc/deps/core/resource/ResourceLocation.h"
#include "mc/world/item/Item.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/level/block/Block.h"
#include <mutex>
#include <set>
#include <stdexcept>
#include <string>

// Icon route diagnostics. L-91 (2026-10-06) traced leather and shield; L-119
// (2026-10-10) compares fence gates, which Lamium's renderGuiItemNew calls draw
// without an icon, with other special block items and with vanilla slots.
namespace lamium::inspection::iconTrace {
namespace {
bool watchedName(std::string const& name) {
    for (char const* part : {"fence", "door", "sign", "_bed", "stairs", "wall", "leather", "shield", "compass", "clock", "sword"})
        if (name.find(part) != std::string::npos) return true;
    return false;
}
bool watched(ItemStack const& item) {
    if (item.isNull() || !item.mItem) return false;
    return watchedName(item.getTypeName());
}
std::mutex seenMutex;
std::set<std::string> seen;
// Logs each distinct line once: the trace is about which routes exist, not how often.
bool firstTime(std::string const& key) {
    std::lock_guard lock{seenMutex};
    return seen.size() < 600 && seen.insert(key).second;
}
TraceBudget passBudget, chunkBudget, newBudget, blitBudget, typeBudget;
// The caller of the current icon draw; nested draws happen inside these calls on the render thread.
thread_local std::string current;
std::string lastSlot; // The last watched slot pass, to place calls made outside one (pre-render setup).
std::string location(ResourceLocation const& value) {
    try { return const_cast<ResourceLocation&>(value).mPath->get(); } catch (...) { return "?"; }
}

LL_TYPE_INSTANCE_HOOK(SlotRenderHook, ll::memory::HookPriority::Normal, InventoryItemRenderer,
    &InventoryItemRenderer::$render, void, MinecraftUIRenderContext& context, IClientInstance& client, UIControl& owner,
    int pass) {
    ItemStack const& item = this->mItemInstance;
    bool watch = false;
    try { watch = watched(item); } catch (...) {}
    if (!watch) { origin(context, client, owner, pass); return; }
    std::string name = item.getTypeName();
    try {
        int passes = this->getNumRenderPasses();
        std::string materials;
        for (int p = 0; p < passes && p < 8; ++p)
            materials += std::format(" p{}=m{}[{} | {}] pre={}", p, static_cast<int>(this->getUIMaterialType(p)),
                location(this->getResourceLocation(0, p)), location(this->getResourceLocation(1, p)),
                this->getRequiresPreRenderSetup(p));
        // getItemRenderInfo crashed when called from here (2026-10-10); the batch key carries the same flags.
        auto key = std::format("slot {} pass={} of {} itemMaterial={} renderType={} texture={} enchanted={} batch={} preRender={}{}",
            name, pass, passes, static_cast<int>(this->mUIMaterialType), static_cast<int>(this->mItemRenderType),
            *this->mTextureName, this->mIsEnchanted, static_cast<int>(this->getBatchType()),
            this->getRequiresPreRenderSetup(pass), materials);
        if (firstTime(key)) traceLog(passBudget, 200, "L-119 {}", key);
    } catch (...) {}
    lastSlot = std::format("{} pass={}", name, pass);
    auto previous = std::exchange(current, std::format("slot {} pass={}", name, pass));
    origin(context, client, owner, pass);
    current = std::move(previous);
}
LL_STATIC_HOOK(RenderTypeHook, ll::memory::HookPriority::Normal, &InventoryItemRenderer::getRenderTypeFromItem,
    ItemRenderChunkType, ItemStack const& item) {
    auto type = origin(item);
    try {
        if (watched(item)) {
            auto key = std::format("renderType {} = {}", item.getTypeName(), static_cast<int>(type));
            if (firstTime(key)) traceLog(typeBudget, 80, "L-119 {}", key);
        }
    } catch (...) {}
    return type;
}
LL_TYPE_INSTANCE_HOOK(ChunkHook, ll::memory::HookPriority::Normal, ItemRenderer, &ItemRenderer::renderGuiItemInChunk,
    void, BaseActorRenderContext& context, ItemRenderChunkType type, ItemStack const& item, float x, float y, float light,
    float alpha, float scale, int frame, bool animate, int zOrder, std::optional<TextureUVCoordinateSet> const& uv) {
    bool watch = false;
    try { watch = watched(item); } catch (...) {}
    if (!watch) { origin(context, type, item, x, y, light, alpha, scale, frame, animate, zOrder, uv); return; }
    std::string name = item.getTypeName();
    // L-339: compasses and clocks lose their frame in Lamium's batch; the frame and custom UVs say what vanilla passes.
    std::string uvText = uv ? std::format("({:.4f},{:.4f})-({:.4f},{:.4f}) tex={}x{}", uv->_u0, uv->_v0, uv->_u1, uv->_v1,
                                          static_cast<int>(uv->_texSizeW), static_cast<int>(uv->_texSizeH))
                            : std::string{"none"};
    auto key = std::format("chunk {} type={} frame={} animate={} scale={:.2f} light={:.2f} alpha={:.2f} z={} uv={} from=[{}]",
        name, static_cast<int>(type), frame, animate, scale, light, alpha, zOrder, uvText, current.empty() ? "lamium" : current);
    if (firstTime(key)) traceLog(chunkBudget, 200, "L-119 {}", key);
    auto previous = std::exchange(current, std::format("chunk {} type={} by={}", name, static_cast<int>(type),
                                                       current.empty() ? "lamium" : "vanilla"));
    origin(context, type, item, x, y, light, alpha, scale, frame, animate, zOrder, uv);
    current = std::move(previous);
}
LL_TYPE_INSTANCE_HOOK(NewHook, ll::memory::HookPriority::Normal, ItemRenderer, &ItemRenderer::renderGuiItemNew, void,
    BaseActorRenderContext& context, ItemStack const& item, int frame, float x, float y, bool foil, float transparency,
    float light, float scale, int zOrder) {
    bool watch = false;
    try { watch = watched(item); } catch (...) {}
    if (!watch) { origin(context, item, frame, x, y, foil, transparency, light, scale, zOrder); return; }
    std::string name = item.getTypeName();
    std::string block = "none";
    try {
        if (auto const* b = item.getBlockForRendering()) block = b->getTypeName();
    } catch (...) { block = "?"; }
    auto key = std::format("new {} block={} frame={} foil={} transparency={:.2f} light={:.2f} scale={:.2f} z={}", name,
        block, frame, foil, transparency, light, scale, zOrder);
    if (firstTime(key)) traceLog(newBudget, 150, "L-119 {}", key);
    auto previous = std::exchange(current, std::format("new {} foil={}", name, foil));
    origin(context, item, frame, x, y, foil, transparency, light, scale, zOrder);
    current = std::move(previous);
}
// Hooks on the block-type, data-driven and entity-block routes and on the geometry atlas's batch keys, renders
// and tiles were here (L-119); 1.26.60's SDK has no symbols for them, so those routes are no longer traced.
LL_TYPE_INSTANCE_HOOK(BlitHook, ll::memory::HookPriority::Normal, ItemRenderer, &ItemRenderer::iconBlit, void,
    BaseActorRenderContext& context, mce::TexturePtr const& texture, float x, float y, float z,
    TextureUVCoordinateSet const& uv, float w, float h, float light, float alpha, int color, int secondaryColor,
    float xscale, float yscale, IconBlitGlint const glint, bool const multiColor) {
    if (!current.empty()) {
        try {
            auto key = std::format("blit from=[{}] glint={} multiColor={} uv=({:.4f},{:.4f})-({:.4f},{:.4f}) tex={}x{} size={:.1f}x{:.1f} scale={:.2f}",
                current, static_cast<int>(glint), multiColor, uv._u0, uv._v0, uv._u1, uv._v1,
                static_cast<int>(uv._texSizeW), static_cast<int>(uv._texSizeH), w, h, xscale);
            if (firstTime(key)) traceLog(blitBudget, 300, "L-119 {}", key);
        } catch (...) {}
    }
    origin(context, texture, x, y, z, uv, w, h, light, alpha, color, secondaryColor, xscale, yscale, glint, multiColor);
}
// L-91 shield round 3 (2026-11-17): which glint constants are set, and from where. The glint pass's chunk call
// sets them itself, for Lamium's calls too, so missing constants are not why a shield shows no glint.
std::string colorText(mce::Color const& c) { return std::format("({:.2f},{:.2f},{:.2f},{:.2f})", c.r, c.g, c.b, c.a); }
TraceBudget constantsBudget, screenBudget;
LL_STATIC_HOOK(EntityConstantsHook, ll::memory::HookPriority::Normal, &ActorShaderManager::setEntityConstants, void,
    mce::ActorConstants& constants, mce::RenderContext& renderContext, mce::Color const& tileLight, Vec2 const& tileLightUV,
    glm::vec4 const& blockLight, mce::Color const& overlay, mce::Color const& change, mce::Color const& change2,
    mce::Color const& glint, Vec2 const& glintScale, Vec4 const& uvAnim, float offset1, float offset2, float rot1, float rot2) {
    try {
        if (glint.a > 0 || glint.r > 0 || glint.g > 0 || glint.b > 0) {
            auto key = std::format("constants glint={} scale=({:.2f},{:.2f}) rot=({:.2f},{:.2f}) overlay={} change={} in=[{}] last=[{}]",
                colorText(glint), glintScale.x, glintScale.y, rot1, rot2, colorText(overlay), colorText(change), current, lastSlot);
            if (firstTime(key)) traceLog(constantsBudget, 80, "L-91 {} offsets=({:.3f},{:.3f}) uvAnim=({:.2f},{:.2f},{:.2f},{:.2f})",
                key, offset1, offset2, uvAnim.x, uvAnim.y, uvAnim.z, uvAnim.w);
        }
    } catch (...) {}
    origin(constants, renderContext, tileLight, tileLightUV, blockLight, overlay, change, change2, glint, glintScale, uvAnim,
        offset1, offset2, rot1, rot2);
}
using ScreenGlintSetup = void (*)(ScreenContext&, mce::Color const&, mce::Color const&, mce::Color const&, mce::Color const&,
    float, float, float, float, Vec2 const&, Vec4 const&, float);
LL_STATIC_HOOK(ScreenGlintHook, ll::memory::HookPriority::Normal,
    static_cast<ScreenGlintSetup>(&ActorShaderManager::setupShaderParameters), void, ScreenContext& screen,
    mce::Color const& overlay, mce::Color const& change, mce::Color const& change2, mce::Color const& glint, float offset1,
    float offset2, float rot1, float rot2, Vec2 const& scale, Vec4 const& uvAnim, float last) {
    try {
        auto key = std::format("screenSetup glint={} scale=({:.2f},{:.2f}) rot=({:.2f},{:.2f}) last={:.2f} in=[{}] lastSlot=[{}]",
            colorText(glint), scale.x, scale.y, rot1, rot2, last, current, lastSlot);
        if (firstTime(key)) traceLog(screenBudget, 60, "L-91 {} offsets=({:.3f},{:.3f})", key, offset1, offset2);
    } catch (...) {}
    origin(screen, overlay, change, change2, glint, offset1, offset2, rot1, rot2, scale, uvAnim, last);
}
bool hooked = false;
}

void start() {
    if (SlotRenderHook::hook(true) != 0 || RenderTypeHook::hook(true) != 0 || ChunkHook::hook(true) != 0
        || NewHook::hook(true) != 0 || BlitHook::hook(true) != 0 || EntityConstantsHook::hook(true) != 0
        || ScreenGlintHook::hook(true) != 0) {
        stop();
        throw std::runtime_error("Could not install icon diagnostics");
    }
    hooked = true;
    Runtime::instance().self().getLogger().warn("Icon diagnostics enabled (L-119)");
}
void stop() {
    SlotRenderHook::unhook(true); RenderTypeHook::unhook(true); ChunkHook::unhook(true); NewHook::unhook(true); BlitHook::unhook(true);
    EntityConstantsHook::unhook(true); ScreenGlintHook::unhook(true);
    hooked = false;
}
}
#else
namespace lamium::inspection::iconTrace { void start() {} void stop() {} }
#endif
