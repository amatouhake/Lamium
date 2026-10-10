#include "features/inspection/render/IconTrace.h"
#ifdef LAMIUM_ICON_TRACE
#include "app/Runtime.h"
#include "app/TraceLog.h"
#include "ll/api/memory/Hook.h"
#include "mc/client/gui/controls/RenderableComponent.h"
#include "mc/client/gui/controls/renderers/InventoryItemRenderer.h"
#include "mc/client/renderer/actor/ItemRenderer.h"
#include "mc/client/gui/geometry_atlas/ItemRenderContextImpl.h"
#include "mc/client/gui/geometry_atlas/RenderContextImpl.h"
#include "mc/client/gui/geometry_atlas/ItemData.h"
#include "mc/client/gui/screens/BatchKey.h"
#include "mc/client/gui/screens/ComponentRenderBatch.h"
#include "mc/client/gui/screens/UIItemRenderInfo.h"
#include "mc/client/gui/controls/renderers/InventoryItemRenderOwnerData.h"
#include "mc/client/renderer/BaseActorRenderContext.h"
#include "mc/client/renderer/screen/MinecraftUIRenderContext.h"
#include "mc/deps/core/resource/ResourceLocation.h"
#include "mc/world/item/Item.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/level/block/Block.h"
#include "mc/deps/minecraft_renderer/framebuilder/dragon/RenderMetadata.h"
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
    for (char const* part : {"fence", "door", "sign", "_bed", "stairs", "wall", "leather", "shield"})
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
TraceBudget passBudget, chunkBudget, newBudget, blitBudget, typeBudget, routeBudget;
// The caller of the current icon draw; nested draws happen inside these calls on the render thread.
thread_local std::string current;
std::string location(ResourceLocation const& value) {
    try { return const_cast<ResourceLocation&>(value).mPath->get(); } catch (...) { return "?"; }
}
void route(std::string const& what) {
    if (current.empty()) return;
    auto key = std::format("{} from=[{}]", what, current);
    if (firstTime(key)) traceLog(routeBudget, 200, "L-119 {}", key);
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
            materials += std::format(" p{}=m{}[{} | {}]", p, static_cast<int>(this->getUIMaterialType(p)),
                location(this->getResourceLocation(0, p)), location(this->getResourceLocation(1, p)));
        // getItemRenderInfo crashed when called from here (2026-10-10); the batch key carries the same flags.
        auto key = std::format("slot {} pass={} of {} itemMaterial={} renderType={} texture={} enchanted={} batch={} preRender={}{}",
            name, pass, passes, static_cast<int>(this->mUIMaterialType), static_cast<int>(this->mItemRenderType),
            *this->mTextureName, this->mIsEnchanted, static_cast<int>(this->getBatchType()),
            this->getRequiresPreRenderSetup(pass), materials);
        if (firstTime(key)) traceLog(passBudget, 200, "L-119 {}", key);
    } catch (...) {}
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
    auto key = std::format("chunk {} type={} scale={:.2f} light={:.2f} alpha={:.2f} z={} uv={} from=[{}]", name,
        static_cast<int>(type), scale, light, alpha, zOrder, uv.has_value(), current);
    if (firstTime(key)) traceLog(chunkBudget, 200, "L-119 {}", key);
    auto previous = std::exchange(current, std::format("chunk {} type={}", name, static_cast<int>(type)));
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
LL_TYPE_INSTANCE_HOOK(BlockTypeHook, ll::memory::HookPriority::Normal, ItemRenderer,
    &ItemRenderer::_renderGuiBlockTypeItem, void, BaseActorRenderContext& context, ItemStack const& item,
    BlockGraphics const* graphics, mce::TexturePtr const& texture, float x, float y, float light, float alpha,
    float scale, float const pop, int const zOrder) {
    route(std::format("blockType graphics={} scale={:.2f} light={:.2f} alpha={:.2f}", graphics != nullptr, scale, light, alpha));
    origin(context, item, graphics, texture, x, y, light, alpha, scale, pop, zOrder);
}
LL_TYPE_INSTANCE_HOOK(DataDrivenHook, ll::memory::HookPriority::Normal, ItemRenderer,
    &ItemRenderer::_renderGuiDataDrivenBlockItem, void, BaseActorRenderContext& context, Block const* block, float x,
    float y, float scale, float const squeeze, int const zOrder) {
    std::string name = "none";
    try { if (block) name = block->getTypeName(); } catch (...) { name = "?"; }
    route(std::format("dataDriven block={} scale={:.2f} squeeze={:.2f}", name, scale, squeeze));
    origin(context, block, x, y, scale, squeeze, zOrder);
}
LL_TYPE_INSTANCE_HOOK(EntityBlockHook, ll::memory::HookPriority::Normal, ItemRenderer,
    &ItemRenderer::_renderGuiEntityBlockItem, bool, BaseActorRenderContext& context, ItemRenderChunkType type,
    dragon::RenderMetadata const metadata, ItemStack const& item, float x, float y, float light, float scale) {
    bool drawn = origin(context, type, metadata, item, x, y, light, scale);
    route(std::format("entityBlock type={} drawn={} scale={:.2f}", static_cast<int>(type), drawn, scale));
    return drawn;
}
// Vanilla's shared-mesh item batch (round 4, 2026-10-10): whether slots go
// through it or the geometry atlas, and the batch key per pass.
std::string rendererItem(InventoryItemRenderer& renderer) {
    try { return renderer.mItemInstance->getTypeName(); } catch (...) { return "?"; }
}
LL_TYPE_INSTANCE_HOOK(BatchKeyHook, ll::memory::HookPriority::Normal, GeometryAtlas::ItemRenderContextImpl,
    &GeometryAtlas::ItemRenderContextImpl::$createBatchKey, BatchKey, int pass) {
    auto key = origin(pass);
    try {
        auto name = rendererItem(this->mRenderer);
        if (watchedName(name)) {
            auto line = std::format("batchKey {} pass={} type={} material={} info={} depth={} alpha={:.2f} tex=[{} | {}]",
                name, pass, static_cast<int>(key.mBatchType), static_cast<int>(key.mUIMaterialType),
                static_cast<int>(key.mUIItemRenderInfo->info), static_cast<int>(key.mDepth), static_cast<float>(key.mAlpha),
                location((*key.mResourceLocations)[0]), location((*key.mResourceLocations)[1]));
            if (firstTime(line)) traceLog(routeBudget, 200, "L-119 {}", line);
        }
    } catch (...) {}
    return key;
}
LL_TYPE_INSTANCE_HOOK(ContextRenderHook, ll::memory::HookPriority::Normal, GeometryAtlas::ItemRenderContextImpl,
    &GeometryAtlas::ItemRenderContextImpl::$render, void, InventoryItemRenderOwnerData const& data, int pass, float alpha) {
    std::string name = rendererItem(this->mRenderer);
    bool watch = watchedName(name);
    if (watch) {
        auto line = std::format("contextRender {} pass={} alpha={:.2f} scale={:.2f} z={}", name, pass, alpha, static_cast<float>(data.mScale), static_cast<int>(data.mZOrder));
        if (firstTime(line)) traceLog(routeBudget, 200, "L-119 {}", line);
    }
    auto previous = watch ? std::exchange(current, std::format("context {} pass={}", name, pass)) : current;
    origin(data, pass, alpha);
    current = std::move(previous);
}
LL_TYPE_INSTANCE_HOOK(SharedBatchHook, ll::memory::HookPriority::Normal, GeometryAtlas::ItemRenderContextImpl,
    &GeometryAtlas::ItemRenderContextImpl::$beginSharedMeshBatch, void, ComponentRenderBatch const& batch) {
    auto name = rendererItem(this->mRenderer);
    if (watchedName(name)) {
        auto line = std::format("sharedBatch {} pass={} preRender={} instances={}", name, static_cast<int>(batch.mRenderPass),
            static_cast<bool>(batch.mRequiresPreRenderSetup), batch.mCustomRenderInstances->size());
        if (firstTime(line)) traceLog(routeBudget, 200, "L-119 {}", line);
    }
    origin(batch);
}
LL_TYPE_INSTANCE_HOOK(TileHook, ll::memory::HookPriority::Normal, GeometryAtlas::RenderContextImpl,
    &GeometryAtlas::RenderContextImpl::$renderItemToTile, bool, dragon::atlas::AtlasTileHandle const& tile,
    GeometryAtlas::ItemData const& data) {
    bool drawn = origin(tile, data);
    auto line = std::format("atlasTile drawn={} scale={:.2f}", drawn, static_cast<float>(data.mScale));
    if (firstTime(line)) traceLog(routeBudget, 50, "L-119 {}", line);
    return drawn;
}
LL_TYPE_INSTANCE_HOOK(BlitHook, ll::memory::HookPriority::Normal, ItemRenderer, &ItemRenderer::iconBlit, void,
    BaseActorRenderContext& context, mce::TexturePtr const& texture, float x, float y, float z,
    TextureUVCoordinateSet const& uv, float w, float h, float light, float alpha, int color, int secondaryColor,
    float xscale, float yscale, IconBlitGlint const glint, bool const multiColor) {
    if (!current.empty()) {
        try {
            auto key = std::format("blit from=[{}] glint={} multiColor={} uv=({:.4f},{:.4f})-({:.4f},{:.4f}) size={:.1f}x{:.1f} scale={:.2f}",
                current, static_cast<int>(glint), multiColor, uv._u0, uv._v0, uv._u1, uv._v1, w, h, xscale);
            if (firstTime(key)) traceLog(blitBudget, 300, "L-119 {}", key);
        } catch (...) {}
    }
    origin(context, texture, x, y, z, uv, w, h, light, alpha, color, secondaryColor, xscale, yscale, glint, multiColor);
}
bool hooked = false;
}
void start() {
    if (SlotRenderHook::hook(true) != 0 || RenderTypeHook::hook(true) != 0 || ChunkHook::hook(true) != 0
        || NewHook::hook(true) != 0 || BlockTypeHook::hook(true) != 0 || DataDrivenHook::hook(true) != 0
        || EntityBlockHook::hook(true) != 0 || BlitHook::hook(true) != 0 || BatchKeyHook::hook(true) != 0
        || ContextRenderHook::hook(true) != 0 || SharedBatchHook::hook(true) != 0 || TileHook::hook(true) != 0) {
        stop();
        throw std::runtime_error("Could not install icon diagnostics");
    }
    hooked = true;
    Runtime::instance().self().getLogger().warn("Icon diagnostics enabled (L-119)");
}
void stop() {
    SlotRenderHook::unhook(true); RenderTypeHook::unhook(true); ChunkHook::unhook(true); NewHook::unhook(true);
    BlockTypeHook::unhook(true); DataDrivenHook::unhook(true); EntityBlockHook::unhook(true); BlitHook::unhook(true);
    BatchKeyHook::unhook(true); ContextRenderHook::unhook(true); SharedBatchHook::unhook(true); TileHook::unhook(true);
    hooked = false;
}
}
#else
namespace lamium::inspection::iconTrace { void start() {} void stop() {} }
#endif
