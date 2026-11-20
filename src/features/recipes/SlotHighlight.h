#pragma once
#include <functional>
class ItemStack;
class MinecraftUIRenderContext;
class ScreenView;

// L-378: while the search highlight is on and the field holds text, the open screen's slots whose items do
// not match are darkened (an empty slot matches nothing). The slots are found among the screen's controls, a
// collection cell each, looked at again when the screen changes or every half second; what each holds is read
// every frame. Behind the SearchHighlight capability. Client thread only.
namespace lamium::recipes::slotHighlight {
void draw(MinecraftUIRenderContext& context, ScreenView& screen, std::function<bool(ItemStack const&)> const& matches);
// Forgets the slots found (a world left, the screen gone).
void forget();
}
