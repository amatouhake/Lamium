#include "features/inspection/render/PreviewRenderer.h"

#include "app/Runtime.h"
#include "features/inspection/preview/ContainerPreview.h"
#include "features/inspection/render/DurabilityBar.h"
#include "features/inspection/render/PreviewLayout.h"

#include "mc/client/game/IClientInstance.h"
#include "mc/client/game/IMinecraftGame.h"
#include "mc/client/gui/CaretMeasureData.h"
#include "mc/client/gui/Font.h"
#include "mc/client/gui/FontHandle.h"
#include "mc/client/gui/FontRepository.h"
#include "mc/client/gui/TextAlignment.h"
#include "mc/client/gui/TextMeasureData.h"
#include "mc/client/gui/controls/MeasureResult.h"
#include "mc/client/gui/controls/UIMeasureStrategy.h"
#include "mc/client/gui/screens/ScreenView.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/client/renderer/BaseActorRenderContext.h"
#include "mc/client/renderer/actor/ItemRenderer.h"
#include "mc/client/renderer/screen/MinecraftUIRenderContext.h"
#include "mc/deps/core/math/Color.h"
#include "mc/deps/core/string/HashedString.h"
#include "mc/deps/core/utility/NonOwnerPointer.h"
#include "mc/deps/input/RectangleArea.h"
#include "mc/world/item/Item.h"

#include <optional>
#include <string>

namespace lamium::inspection::render {

namespace {

// Material the UI uses for plain coloured quads (fillRectangle/drawRectangle).
HashedString const kFillMaterial{"ui_fillColor"};

constexpr mce::Color kFrameBackground{0.10f, 0.10f, 0.10f, 1.0f};
constexpr mce::Color kFrameBorder{0.55f, 0.35f, 0.70f, 1.0f};
constexpr mce::Color kSlotBackground{0.23f, 0.23f, 0.23f, 1.0f};
constexpr mce::Color kCountText{1.0f, 1.0f, 1.0f, 1.0f};
constexpr mce::Color kWhite{1.0f, 1.0f, 1.0f, 1.0f};
constexpr mce::Color kDurabilityBackground{0.0f, 0.0f, 0.0f, 1.0f};

constexpr float kFrameAlpha = 0.92f;
constexpr float kSlotAlpha  = 1.0f;
constexpr int   kItemZOrder = 17;

// renderGuiItemNew(foil = true) draws the additive glint overlay on its own,
// outside the UI's InventoryItemGlint material pass, and comes out weaker than
// vanilla's slot glint. Its `transparency` argument scales the overlay
// linearly, so one pass at this value reproduces vanilla: frame-averaged
// blue-minus-green over the item pixels of an enchanted book, measured in the
// same frames as a vanilla inventory slot, was +4 at 1.0, +23 at 1.3, +38 at
// 1.5 and +27.6 at 1.35 against vanilla's +27.7. Drawing the overlay several
// times instead (the previous approach) over-tints: +30 for two passes, +58
// for three, at 1.5-2x the CPU cost.
constexpr float kGlintOverlayAlpha = 1.35f;

// Vanilla's stack count is a plain UI label ("font_size": "normal", shadow,
// anchored bottom-right of the 18x18 slot panel with offset [0, 1]); see
// common.stack_count_label in the vanilla ui_common.json. Mirror those values.
constexpr float kCountFontSize     = 1.0f;
constexpr float kCountOffsetY      = 1.0f;
constexpr int   kCountMeasureLimit = 1000; // no wrapping/clipping for a few digits

RectangleArea toArea(Rect const& r) { return RectangleArea{r.x0, r.x1, r.y0, r.y1}; }

} // namespace

void PreviewRenderer::render(
    ScreenView&                      view,
    MinecraftUIRenderContext&        context,
    preview::ContainerPreview const& preview
) {
    if (preview.columns <= 0 || preview.rows <= 0) {
        // Defensive only: providers report a real frame even for empty
        // containers (Bundle: minimal 3x1, Shulker: fixed 9x3), so a 0x0 grid
        // only arises from a malformed preview and has no frame to anchor.
        return;
    }
    if (preview.slots.size() < static_cast<size_t>(preview.slotCount())) {
        return;
    }

    glm::vec2 const pointer = view.mPointerLocationPrevious;
    glm::vec2 const screen  = view.mSize;
    auto const      layout =
        PreviewLayout::anchored(preview.columns, preview.rows, pointer.x, pointer.y, screen.x, screen.y);

    static bool sLoggedFirstRender = false;
    if (!sLoggedFirstRender) {
        sLoggedFirstRender = true;
        Runtime::instance().self().getLogger().debug(
            "First preview render: pointer=({}, {}) screen=({}, {}) frame=({}, {})-({}, {})",
            pointer.x,
            pointer.y,
            screen.x,
            screen.y,
            layout.frame.x0,
            layout.frame.y0,
            layout.frame.x1,
            layout.frame.y1
        );
    }

    // 1. Frame and slot backgrounds. These are batched by the context, so
    //    flush them before drawing anything that must appear on top. Each
    //    slot only fills its 16x16 icon area so the 2-unit gaps between cells
    //    keep the grid visible even when the box is empty.
    context.fillRectangle(toArea(layout.frame), kFrameBackground, kFrameAlpha);
    for (int slot = 0; slot < preview.slotCount(); ++slot) {
        context.fillRectangle(toArea(layout.icon(slot)), kSlotBackground, kSlotAlpha);
    }
    context.drawRectangle(toArea(layout.frame), kFrameBorder, 1.0f, 1);
    context.flushImages(kWhite, 1.0f, kFillMaterial);

    // 2. Item icons, drawn immediately by the game's item renderer.
    IClientInstance& client       = context.mClient;
    ItemRenderer*    itemRenderer = client.getItemRenderer();
    if (itemRenderer) {
        BaseActorRenderContext renderContext(context.mScreenContext, client, client.getMinecraftGame_DEPRECATED());
        Mob* const             holder = client.getLocalPlayer();
        for (int slot = 0; slot < preview.slotCount(); ++slot) {
            ItemStack const& stack = preview.slots[static_cast<size_t>(slot)];
            if (stack.isNull() || !stack.mItem) {
                continue;
            }
            Item const& item = *stack.mItem;
            Rect const  icon = layout.icon(slot);
            // Same frame source as a vanilla inventory slot: the item decides
            // (clock, compass, crossbow, ...); static items return 0.
            int const frame = item.getAnimationFrameFor(holder, false, &stack, true);

            // renderEnchantmentFoil selects the pass: false draws the item
            // icon itself, true draws only the additive glint overlay.
            itemRenderer
                ->renderGuiItemNew(renderContext, stack, frame, icon.x0, icon.y0, false, 1.0f, 1.0f, 1.0f, kItemZOrder);
            // Vanilla's glint predicate: Item::isGlint, which items override
            // (enchanted books, enchanted golden apples, ...), not raw
            // enchantment NBT.
            if (item.isGlint(stack)) {
                itemRenderer->renderGuiItemNew(
                    renderContext,
                    stack,
                    frame,
                    icon.x0,
                    icon.y0,
                    true,
                    kGlintOverlayAlpha,
                    1.0f,
                    1.0f,
                    kItemZOrder
                );
            }
        }
    }

    // 3. Durability bars, drawn after the icons and glint (as in vanilla, the
    //    overlay sits on top of the sprite) but before the stack counts, so
    //    counts stay legible. Uses the stack's own damageable/damaged/max-damage
    //    state directly: no NBT decode or ItemStack rebuild in the render loop,
    //    just a few tiny rectangles for <=27 slots.
    for (int slot = 0; slot < preview.slotCount(); ++slot) {
        ItemStack const& stack = preview.slots[static_cast<size_t>(slot)];
        if (stack.isNull() || !stack.mItem) {
            continue;
        }
        int const maxDamage = static_cast<int>(stack.mItem->getMaxDamage());
        if (!shouldShowDurabilityBar(stack.isDamageableItem(), stack.getDamageValue(), maxDamage)) {
            continue;
        }
        float const         ratio      = durabilityRatio(stack.getDamageValue(), maxDamage);
        Rect const          background = durabilityBackground(layout.icon(slot));
        Rect const          foreground = durabilityForeground(background, ratio);
        DurabilityRgb const rgb        = durabilityColor(ratio);
        context.fillRectangle(toArea(background), kDurabilityBackground, kSlotAlpha);
        // The background stays unconditional so a nearly-broken item still
        // shows the black strip; the fill rounds to zero width below 1/24
        // remaining, as in the vanilla slot.
        if (foreground.width() > 0.0f) {
            context.fillRectangle(toArea(foreground), mce::Color{rgb.r, rgb.g, rgb.b, 1.0f}, kSlotAlpha);
        }
    }
    context.flushImages(kWhite, 1.0f, kFillMaterial);
    // 4. Stack counts, laid out like vanilla's stack_count_label: the font a
    //    "default" UI label resolves to (locale and font overrides included),
    //    measured by the UI's own strategy, anchored bottom-right of the cell.
    auto const& fontHandle = client.getMinecraftGame_DEPRECATED().getFontRepository()->getFontFromFontType("default");
    Font&       font       = fontHandle.getFont();
    Bedrock::NotNullNonOwnerPtr<FontHandle const> const fontRef{Bedrock::NonOwnerPointer<FontHandle const>{fontHandle}};
    TextMeasureData const  textData{kCountFontSize, 0.0f, true, false, false, ui::TextAlignment::Right};
    CaretMeasureData const caretData{-1, false};
    auto&                  measure = context.getMeasureStrategy();
    bool                   anyText = false;
    for (int slot = 0; slot < preview.slotCount(); ++slot) {
        ItemStack const& stack = preview.slots[static_cast<size_t>(slot)];
        if (stack.isNull() || stack.mCount <= 1) {
            continue;
        }
        std::string     text = std::to_string(static_cast<int>(stack.mCount));
        glm::vec2 const size =
            measure.measureText(fontRef, text, kCountMeasureLimit, kCountMeasureLimit, textData, caretData).mSize;
        Rect const cell = layout.cell(slot);
        Rect const textRect{cell.x1 - size.x, cell.y1 + kCountOffsetY - size.y, cell.x1, cell.y1 + kCountOffsetY};
        context.drawText(
            font,
            toArea(textRect),
            std::move(text),
            kCountText,
            1.0f,
            ui::TextAlignment::Right,
            textData,
            caretData
        );
        anyText = true;
    }
    if (anyText) {
        context.flushText(0.0f, std::nullopt);
    }
}

} // namespace lamium::inspection::render

