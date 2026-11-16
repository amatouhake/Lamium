#include "features/inspection/render/ItemIcon.h"
#include "ui/Widgets.h"
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
#include "mc/world/item/ResolvedItemIconInfo.h"
#include <algorithm>
#include <cstddef>
#include <cstring>
#include <memory>
#include <new>
#include <optional>
#include <string>
#include <unordered_map>
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
// renderGuiItemNew(foil = true) draws the glint overlay scaled by the current
// shader colour, i.e. whatever was filled last (L-170: on a near-black fill it
// does not show). With the colour set to white and the icon's own alpha it
// matches a vanilla hotbar slot exactly: frame-averaged colour over an
// enchanted book's pixels, same frames, no difference (2026-11-14). The earlier
// purple x 1.35 had been fitted against the container preview's border fill.
constexpr mce::Color glintTint{1.0f, 1.0f, 1.0f, 1.0f};
// Compasses and clocks draw each frame from a strip of frames of their own, not from the item atlas: a slot
// batches them with that texture and lets the renderer animate. Through the item atlas they showed their still
// icon, and a recovery compass, which has none there, showed nothing (L-339, traced 2026-11-16). Such an item
// answers the out-of-pane icon question with raw UVs into its strip; the strip's path is not on the item (its
// atlas file is the item atlas), so the paths are the ones the vanilla slots were traced using. Anything else
// keeps the item atlas. Asked once per item kind.
std::string const& ownTexture(ItemStack const& stack) {
    static std::unordered_map<int, std::string> known;
    int const id = stack.getId();
    if (auto found = known.find(id); found != known.end()) return found->second;
    static std::pair<char const*, char const*> const strips[]{
        {"minecraft:compass", "textures/items/compass_atlas"},
        {"minecraft:recovery_compass", "textures/items/recovery_compass_atlas"},
        {"minecraft:clock", "textures/items/watch_atlas"},
    };
    std::string name;
    try {
        auto const type = stack.getTypeName();
        for (auto [item, strip] : strips) {
            if (type != item) continue;
            auto info = stack.mItem->getIconInfo(stack, 0, false);
            if (static_cast<ItemIconInfoType>(info.mIconType) == ItemIconInfoType::RawUVs) name = strip;
        }
    } catch (...) { name.clear(); }
    return known.emplace(id, std::move(name)).first->second;
}
int chunkOf(ItemStack const& stack) {
    if (stack.mItem->isGlint(stack)) return glinting;
    return static_cast<int>(InventoryItemRenderer::getRenderTypeFromItem(stack));
}
}
void drawItemIcons(MinecraftUIRenderContext& context, std::span<IconAt const> icons, int zOrder) {
    // Measured but not drawn (HUD layout's hidden elements, L-334); a faint
    // element's icons are as faint as its text.
    float const opacity = ui::drawOpacity();
    if (opacity <= 0) return;
    std::vector<IconAt> faded;
    if (opacity < 1) {
        faded.assign(icons.begin(), icons.end());
        for (auto& icon : faded) icon.alpha *= opacity;
        icons = faded;
    }
    auto& client = context.mClient;
    auto* renderer = client.getItemRenderer();
    if (!renderer) return;
    BaseActorRenderContext renderContext(context.mScreenContext, client, client.getMinecraftGame_DEPRECATED());
    std::vector<int> chunks;
    chunks.reserve(icons.size());
    for (auto const& icon : icons) chunks.push_back(!drawable(icon) ? -1 : icon.direct ? glinting : chunkOf(*icon.stack));
    auto direct = [&](size_t i) {
        auto const& icon = icons[i];
        renderer->renderGuiItemNew(renderContext, *icon.stack, icon.frame, icon.x, icon.y, false, icon.alpha, 1.0f, icon.scale,
            zOrder);
        if (icon.glint && icon.stack->mItem->isGlint(*icon.stack)) {
            auto& shaderColor = static_cast<ShaderColor&>(context.mScreenContext.currentShaderColor);
            mce::Color const was = shaderColor.color;
            shaderColor.color = glintTint;
            shaderColor.dirty = true;
            renderer->renderGuiItemNew(renderContext, *icon.stack, icon.frame, icon.x, icon.y, true, icon.alpha,
                1.0f, icon.scale, zOrder);
            shaderColor.color = was;
            shaderColor.dirty = true;
        }
    };
    // Glinting items before any shared-mesh batch: after one, the foil call
    // drew nothing even right after its icon (L-142 round 2); 0.1.8 drew no
    // batches at all.
    for (size_t i = 0; i < icons.size(); ++i) if (chunks[i] == glinting) direct(i);
    // One batch per chunk type and opacity: the batch key carries the alpha.
    std::vector<float> alphas;
    for (auto const& icon : icons) if (std::find(alphas.begin(), alphas.end(), icon.alpha) == alphas.end()) alphas.push_back(icon.alpha);
    // Flat items with a texture of their own get a batch per texture, animated as in a slot.
    std::vector<std::string const*> textures(icons.size(), nullptr);
    std::vector<std::string const*> ownTextures;
    for (size_t i = 0; i < icons.size(); ++i) {
        if (chunks[i] != flatChunk) continue;
        auto const& name = ownTexture(*icons[i].stack);
        if (name.empty()) continue;
        textures[i] = &name;
        if (std::find(ownTextures.begin(), ownTextures.end(), &name) == ownTextures.end()) ownTextures.push_back(&name);
    }
    auto drawBatch = [&](int chunk, std::string const* texture, float alpha) {
        bool any = false;
        for (size_t i = 0; i < icons.size(); ++i)
            any = any || (chunks[i] == chunk && textures[i] == texture && icons[i].alpha == alpha);
        if (!any) return;
        SharedItemBatch batch{zOrder, texture ? texture->c_str() : chunk == blockChunk ? "atlas.terrain" : "atlas.items", alpha};
        context.beginSharedMeshBatch(batch.get());
        for (size_t i = 0; i < icons.size(); ++i) {
            if (chunks[i] != chunk || textures[i] != texture || icons[i].alpha != alpha) continue;
            auto const& icon = icons[i];
            renderer->renderGuiItemInChunk(renderContext, static_cast<ItemRenderChunkType>(chunk), *icon.stack, icon.x,
                icon.y, 1.0f, chunk == blockChunk ? 0.0f : 1.0f, icon.scale, icon.frame, texture != nullptr, zOrder,
                std::nullopt);
        }
        context.endSharedMeshBatch(batch.get());
    };
    for (float alpha : alphas) {
        drawBatch(blockChunk, nullptr, alpha);
        drawBatch(flatChunk, nullptr, alpha);
        for (auto const* texture : ownTextures) drawBatch(flatChunk, texture, alpha);
    }
    for (size_t i = 0; i < icons.size(); ++i)
        if (chunks[i] != -1 && chunks[i] != glinting && chunks[i] != blockChunk && chunks[i] != flatChunk) direct(i);
}
}
