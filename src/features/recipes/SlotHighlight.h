#pragma once
#include <functional>
#include <string>
class ItemStack;
class MinecraftUIRenderContext;
class ScreenView;

// L-378: while the search highlight is on and the field holds text, the open screen's slots whose items do
// not match are darkened (an empty slot matches nothing), where items are kept: the inventory, the hotbar, a
// container's. The slots are found among the screen's controls once the window rests; what each holds is read
// every frame. Behind the SearchHighlight capability. Client thread only.
namespace lamium::recipes::slotHighlight {
// `settled`: the game's window rests where it stays (not sliding in or out), at `windowX`, `windowY`; the slots are
// found again when that changes. `query`: the search as typed; a slot is matched again when it or its item changes.
void draw(MinecraftUIRenderContext& context, ScreenView& screen, bool settled, float windowX, float windowY, std::string const& query,
          std::function<bool(ItemStack const&)> const& matches);
// Forgets the slots found (a world left, the screen gone).
void forget();
}
