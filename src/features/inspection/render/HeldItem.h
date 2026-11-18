#pragma once
#include <functional>
class ItemStack;
class MinecraftUIRenderContext;
class ScreenView;
namespace lamium::inspection::render {
// The item held on the pointer of a container screen, for a panel painted
// after the screen: such a panel covers it, so it is drawn once more in the
// box the game draws it in. Valid for one frame.
struct HeldIcon {
    ItemStack const* stack = nullptr;
    float x = 0, y = 0, size = 16;
    // Where the game writes the stack's count, when it shows one.
    bool counted = false;
    float countX = 0, countY = 0, countWidth = 0, countHeight = 0;
};
// The stack on the pointer (the container manager's `cursor_items`), null when there is none or the
// screen has no open containers.
ItemStack const* heldStack(ScreenView& view);
// `held` in the game's box for it (`selected_item_icon`, 21 units, following the pointer), when
// `covers(x, y, width, height)` says a panel overlaps that box; an empty icon otherwise, and on a
// screen that lacks the control.
HeldIcon heldOver(ScreenView& view, ItemStack const* held, std::function<bool(float, float, float, float)> const& covers);
// What goes over the icon: its durability bar and its count, where the game writes them. The icon
// itself is the caller's, last among its icons: {stack, x, y, size / 16, 0, opacity, true, true}.
void drawHeldMarks(MinecraftUIRenderContext& context, HeldIcon const& held, float opacity = 1);
}
