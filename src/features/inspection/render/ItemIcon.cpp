#include "features/inspection/render/ItemIcon.h"
#include "mc/client/game/IClientInstance.h"
#include "mc/client/game/IMinecraftGame.h"
#include "mc/client/gui/controls/UIMaterialType.h"
#include "mc/client/gui/controls/renderers/InventoryItemRenderer.h"
#include "mc/client/gui/screens/BatchClippingState.h"
#include "mc/client/gui/screens/BatchKey.h"
#include "mc/client/gui/screens/ComponentRenderBatch.h"
#include "mc/client/gui/screens/ScreenContext.h"
#include "mc/client/gui/screens/UIBatchType.h"
#include "mc/client/renderer/BaseActorRenderContext.h"
#include "mc/client/renderer/actor/ItemRenderer.h"
#include "mc/client/renderer/screen/MinecraftUIRenderContext.h"
#include "mc/deps/core/file/PathView.h"
#include "mc/deps/core/math/Color.h"
#include "mc/deps/core/resource/ResourceLocation.h"
#include "mc/deps/renderer/ShaderColor.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/item/Item.h"
#include "mc/deps/core/string/HashedString.h"
#include "mc/deps/input/RectangleArea.h"
#ifdef LAMIUM_GLINT_TRIAL
#include "app/TraceLog.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/world/actor/player/PlayerInventory.h"
#endif
#include <algorithm>
#include <cstddef>
#include <cstring>
#include <memory>
#include <new>
#include <optional>
#include <vector>

namespace lamium::inspection::render {
namespace {
// Chunk types vanilla slots batch (InventoryItemRenderer::getRenderTypeFromItem):
// 0 blocks on the terrain atlas (slot alpha argument 0), 2 flat items on the
// item atlas (alpha 1). Both use UI material 13 ("Item").
constexpr int blockChunk = 0, flatChunk = 2, glinting = -2;
constexpr UIMaterialType itemMaterial = static_cast<UIMaterialType>(13);

// ComponentRenderBatch and BatchKey have no usable default constructors, so
// the batch lives in raw storage, built like a slot's.
class SharedItemBatch {
    alignas(ComponentRenderBatch) std::byte bytes[sizeof(ComponentRenderBatch)];
public:
    SharedItemBatch(int depth, char const* atlas, float alpha = 1.0f) {
        std::memset(bytes, 0, sizeof bytes);
        auto& batch = get();
        alignas(BatchClippingState) std::byte clipBytes[sizeof(BatchClippingState)]{};
        auto& key = *::new (&*batch.mBatchKey) BatchKey(depth, alpha, *reinterpret_cast<BatchClippingState*>(clipBytes));
        key.mBatchType = UIBatchType::SharedMesh;
        key.mUIMaterialType = itemMaterial;
        auto& textures = *key.mResourceLocations;
        std::destroy_at(&textures[0]);
        ::new (&textures[0]) ResourceLocation(Core::PathView(atlas));
        std::destroy_at(&textures[1]);
        ::new (&textures[1]) ResourceLocation(Core::PathView("textures/entity/banner/banner"));
        batch.mIsDirty = true; // Rebuilt every frame: Lamium's icons change freely.
        batch.mRequiresPreRenderSetup = false;
        batch.mRenderPass = 0;
        ::new (&*batch.mCustomRenderInstances) std::vector<CustomRenderComponent*>();
        ::new (&*batch.mSpriteInstances) std::vector<SpriteComponent*>();
        ::new (&*batch.mTextInstances) std::vector<TextComponent*>();
    }
    ~SharedItemBatch() { std::destroy_at(&get()); }
    SharedItemBatch(SharedItemBatch const&) = delete;
    SharedItemBatch& operator=(SharedItemBatch const&) = delete;
    ComponentRenderBatch& get() { return *std::launder(reinterpret_cast<ComponentRenderBatch*>(bytes)); }
};
bool drawable(IconAt const& icon) { return icon.stack && !icon.stack->isNull() && icon.stack->mItem; }
// Glinting items keep the direct path, with the foil call right after their
// icon as in 0.1.8; an enchanted leather piece loses its undyeable layer
// that way, as before 0.1.9.
// renderGuiItemNew(foil = true) draws the additive glint overlay on its own,
// outside the UI's InventoryItemGlint material pass, and comes out weaker than
// vanilla's slot glint. Its `transparency` argument scales the overlay
// linearly, so one pass at this value reproduces vanilla: frame-averaged
// blue-minus-green over the item pixels of an enchanted book, measured in the
// same frames as a vanilla inventory slot, was +4 at 1.0, +23 at 1.3, +38 at
// 1.5 and +27.6 at 1.35 against vanilla's +27.7. Drawing the overlay several
// times instead over-tints: +30 for two passes, +58 for three.
constexpr float glintOverlayAlpha = 1.35f;
// The overlay is also scaled by the colour of whatever was filled last (seen 2026-11-12, L-170: on a white fill
// it shows, on the settings panel's near-black one it does not, with nothing before it it is weak and not
// pink). The value above was measured in the container preview, whose last fill before the icons is its
// border, so that colour is set for every overlay (confirmed in game the same day: the anvil list, the planner
// screen and the preview all glint alike).
constexpr mce::Color glintTint{0.55f, 0.35f, 0.70f, 1.0f};
int chunkOf(ItemStack const& stack) {
    if (stack.mItem->isGlint(stack)) return glinting;
    return static_cast<int>(InventoryItemRenderer::getRenderTypeFromItem(stack));
}
#ifdef LAMIUM_GLINT_TRIAL
// The trial variants and the flush in drawItemIcons follow BarelyEnoughItems, copyright LeviMC,
// LGPL-3.0-or-later (docs/PROVENANCE.md).
// L-91 trial: the draw context BarelyEnoughItems (2d4facf, src/ui/gfx/Painter.cpp) uses around the same two
// renderGuiItemNew calls differs in three ways. One variant per frame, picked by the selected hotbar slot, because
// glint state leaks between icons drawn in one frame (L-142).
// 1 (slot 1): as before. 2: an off-screen white quad flushed with "ui_flush" before the glinting icons.
// 3: zOrder 0 for icon and foil. 4: foil at the icon's own alpha, shader colour untouched. 5: 2+3+4.
struct GlintTrial { bool flush = false, zeroZ = false, plainFoil = false; };
GlintTrial glintTrial(MinecraftUIRenderContext& context, std::span<IconAt const> icons) {
    static int logged = -1;
    static TraceBudget budget;
    auto* player = context.mClient.getLocalPlayer();
    int slot = player && player->mInventory ? player->mInventory->mSelected : 0;
    int variant = slot >= 1 && slot <= 4 ? slot + 1 : 1;
    if (variant != logged) {
        logged = variant;
        for (auto const& icon : icons)
            if (drawable(icon) && icon.glint && icon.stack->mItem->isGlint(*icon.stack))
                traceLog(budget, 200, "[glint_trial] variant {} item {} renderType {}", variant,
                    icon.stack->getTypeName(), static_cast<int>(InventoryItemRenderer::getRenderTypeFromItem(*icon.stack)));
        traceLog(budget, 200, "[glint_trial] variant {} active", variant);
    }
    return {variant == 2 || variant == 5, variant == 3 || variant == 5, variant == 4 || variant == 5};
}
#endif
}
void drawItemIcons(MinecraftUIRenderContext& context, std::span<IconAt const> icons, int zOrder) {
    auto& client = context.mClient;
    auto* renderer = client.getItemRenderer();
    if (!renderer) return;
    BaseActorRenderContext renderContext(context.mScreenContext, client, client.getMinecraftGame_DEPRECATED());
    std::vector<int> chunks;
    chunks.reserve(icons.size());
    for (auto const& icon : icons) chunks.push_back(!drawable(icon) ? -1 : icon.direct ? glinting : chunkOf(*icon.stack));
#ifdef LAMIUM_GLINT_TRIAL
    auto const trial = glintTrial(context, icons);
#else
    struct { bool flush = false, zeroZ = false, plainFoil = false; } const trial;
#endif
    auto direct = [&](size_t i) {
        auto const& icon = icons[i];
        bool const glint = icon.glint && icon.stack->mItem->isGlint(*icon.stack);
        int const z = glint && trial.zeroZ ? 0 : zOrder;
        renderer->renderGuiItemNew(renderContext, *icon.stack, icon.frame, icon.x, icon.y, false, icon.alpha, 1.0f, icon.scale, z);
        if (glint && trial.plainFoil) {
            renderer->renderGuiItemNew(renderContext, *icon.stack, icon.frame, icon.x, icon.y, true, icon.alpha, 1.0f, icon.scale, z);
        } else if (glint) {
            auto& shaderColor = static_cast<ShaderColor&>(context.mScreenContext.currentShaderColor);
            mce::Color const was = shaderColor.color;
            shaderColor.color = glintTint;
            shaderColor.dirty = true;
            renderer->renderGuiItemNew(renderContext, *icon.stack, icon.frame, icon.x, icon.y, true, glintOverlayAlpha * icon.alpha,
                1.0f, icon.scale, z);
            shaderColor.color = was;
            shaderColor.dirty = true;
        }
    };
    if (trial.flush && std::find(chunks.begin(), chunks.end(), glinting) != chunks.end()) {
        mce::Color const white{1.f, 1.f, 1.f, 1.f};
        context.fillRectangle(RectangleArea{-10000.f, -9999.f, -10000.f, -9999.f}, white, 1.f);
        context.flushImages(white, 1.f, HashedString{"ui_flush"});
    }
    // Glinting items before any shared-mesh batch: after one, the foil call
    // drew nothing even right after its icon (L-142 round 2); 0.1.8 drew no
    // batches at all.
    for (size_t i = 0; i < icons.size(); ++i) if (chunks[i] == glinting) direct(i);
    // One batch per chunk type and opacity: the batch key carries the alpha.
    std::vector<float> alphas;
    for (auto const& icon : icons) if (std::find(alphas.begin(), alphas.end(), icon.alpha) == alphas.end()) alphas.push_back(icon.alpha);
    for (float alpha : alphas)
    for (int chunk : {blockChunk, flatChunk}) {
        bool any = false;
        for (size_t i = 0; i < icons.size(); ++i) any = any || (chunks[i] == chunk && icons[i].alpha == alpha);
        if (!any) continue;
        SharedItemBatch batch{zOrder, chunk == blockChunk ? "atlas.terrain" : "atlas.items", alpha};
        context.beginSharedMeshBatch(batch.get());
        for (size_t i = 0; i < icons.size(); ++i) {
            if (chunks[i] != chunk || icons[i].alpha != alpha) continue;
            auto const& icon = icons[i];
            renderer->renderGuiItemInChunk(renderContext, static_cast<ItemRenderChunkType>(chunk), *icon.stack, icon.x,
                icon.y, 1.0f, chunk == blockChunk ? 0.0f : 1.0f, icon.scale, icon.frame, false, zOrder, std::nullopt);
        }
        context.endSharedMeshBatch(batch.get());
    }
    for (size_t i = 0; i < icons.size(); ++i)
        if (chunks[i] != -1 && chunks[i] != glinting && chunks[i] != blockChunk && chunks[i] != flatChunk) direct(i);
}
}
