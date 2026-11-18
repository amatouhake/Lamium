#include "features/inspection/render/HeldItem.h"
#include "features/inspection/render/DurabilityBar.h"
#include "ui/GameFont.h"
#include "ui/Widgets.h"
#include "mc/client/game/IClientInstance.h"
#include "mc/client/game/IMinecraftGame.h"
#include "mc/client/gui/CaretMeasureData.h"
#include "mc/client/gui/Font.h"
#include "mc/client/gui/FontRepository.h"
#include "mc/client/gui/TextAlignment.h"
#include "mc/client/gui/TextMeasureData.h"
#include "mc/client/gui/controls/UIControl.h"
#include "mc/client/gui/controls/VisualTree.h"
#include "mc/client/gui/screens/ScreenController.h"
#include "mc/client/gui/screens/ScreenView.h"
#include "mc/client/gui/screens/controllers/ContainerScreenController.h"
#include "mc/client/renderer/screen/MinecraftUIRenderContext.h"
#include "mc/deps/core/math/Color.h"
#include "mc/deps/input/RectangleArea.h"
#include "mc/world/containers/managers/controllers/ContainerManagerController.h"
#include "mc/world/item/Item.h"
#include "mc/world/item/ItemStack.h"
#include <optional>
#include <string>

namespace lamium::inspection::render {
namespace {
// The game's count label of the held item, `selected_stack_size_text` (12x10 units; read from the log 2026-11-12).
UIControl const* countControl(UIControl const& control, int depth = 0) {
    if (depth > 8) return nullptr;
    for (auto const& child : *control.mChildren) {
        if (!child || (static_cast<int>(child->mVisible) & 1) == 0) continue;
        glm::vec2 const extent = *child->mSize;
        if (*child->mName == "selected_stack_size_text" && extent.x > 0 && extent.y > 0) return child.get();
        if (auto const* found = countControl(*child, depth + 1)) return found;
    }
    return nullptr;
}
}
ItemStack const* heldStack(ScreenView& view) {
    try {
        if (!view.mController || !view.mController->_isContainerScreen()) return nullptr;
        auto& screenController = static_cast<ContainerScreenController&>(*view.mController);
        auto manager = screenController.mContainerManagerController;
        if (!manager || manager->mContainersClosed) return nullptr;
        auto const& onPointer = manager->getItemStack("cursor_items", 0);
        return onPointer.isNull() ? nullptr : &onPointer;
    } catch (...) { return nullptr; }
}
HeldIcon heldOver(ScreenView& view, ItemStack const* held, std::function<bool(float, float, float, float)> const& covers) {
    HeldIcon out;
    auto* tree = view.mVisualTree.get();
    if (!held || !tree) return out;
    static std::string const heldName = "selected_item_icon", buttonName = "inventory_selected_icon_button";
    try {
        auto icon = tree->getControlByName(heldName, true);
        if (!icon || icon->mCachedPositionDirty) return out;
        glm::vec2 const where = *icon->mCachedPosition, extent = *icon->mSize;
        if (!(extent.x > 0) || !covers(where.x, where.y, extent.x, extent.y)) return out;
        out = {held, where.x, where.y, extent.x};
        if (auto button = tree->getControlByName(buttonName, true))
            if (auto const* count = countControl(*button)) {
                glm::vec2 const at = *count->mCachedPosition, size = *count->mSize;
                out.counted = true;
                out.countX = at.x; out.countY = at.y; out.countWidth = size.x; out.countHeight = size.y;
            }
    } catch (...) { out = {}; }
    return out;
}
void drawHeldMarks(MinecraftUIRenderContext& context, HeldIcon const& held, float opacity) {
    using namespace lamium::ui;
    if (!held.stack || !held.stack->mItem) return;
    // Its durability bar as the game draws it on a slot, at the held item's size.
    int const maxDamage = static_cast<int>(held.stack->mItem->getMaxDamage());
    if (shouldShowDurabilityBar(held.stack->isDamageableItem(), held.stack->getDamageValue(), maxDamage)) {
        float const unit = held.size / 16, ratio = durabilityRatio(held.stack->getDamageValue(), maxDamage);
        auto const back = durabilityBackground({0, 0, 16, 16});
        auto const front = durabilityForeground(back, ratio);
        auto const color = durabilityColor(ratio);
        fill(context, held.x + back.x0 * unit, held.y + back.y0 * unit, back.width() * unit, back.height() * unit, palette::shade);
        if (front.width() > 0)
            fill(context, held.x + front.x0 * unit, held.y + front.y0 * unit, front.width() * unit, front.height() * unit,
                Rgb{color.r, color.g, color.b});
    }
    // Its count, on top of it and where the game writes it (the game's own number is under the item
    // drawn again), drawn as the container previews draw theirs.
    if (held.stack->mCount > 1 && held.counted) {
        auto const& fontHandle = context.mClient.getMinecraftGame_DEPRECATED().getFontRepository()->getFontFromFontType("default");
        TextMeasureData const textData{1.0f, 0.0f, true, false, false, ::ui::TextAlignment::Right};
        CaretMeasureData const caret{-1, false};
        context.drawText(ui::fontOf(fontHandle),
            RectangleArea{held.countX, held.countX + held.countWidth, held.countY, held.countY + held.countHeight},
            std::to_string(static_cast<int>(held.stack->mCount)), mce::Color{1.f, 1.f, 1.f, opacity}, 1.0f, ::ui::TextAlignment::Right,
            textData, caret);
        context.flushText(0, std::nullopt);
    }
}
}
