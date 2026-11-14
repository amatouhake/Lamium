#pragma once
#include "features/inspection/render/ItemIcon.h"
#include "features/inventory/EnchantOrderGame.h"
#include "ui/EnchantLayout.h"
#include <string>
#include <vector>
class MinecraftUIRenderContext;
// The enchantment order's choosing part (BACKLOG L-170): the item's line, the
// kind cells with the item cells last, the set buttons and the enchantment
// rows. One drawing and one set of presses for Lamium's screen and for the
// panel left of the anvil screen, so the two look and behave alike.
namespace lamium::ui::enchant_chooser {
// An item icon in the widgets' units; icons are drawn by the caller in one call after its text.
void addIcon(std::vector<inspection::render::IconAt>& icons, ItemStack const* stack, float x, float y, float size = 10);
void draw(MinecraftUIRenderContext&, EnchantLayout const&, inventory::enchantOrder::game::View const&, EnchantLayout::Hit hover,
          std::vector<inspection::render::IconAt>& icons, int& itemsDrawn, bool scrollHot = false);
// What the status line says about the zone under the pointer; empty when it is not one of the chooser's.
std::string status(inventory::enchantOrder::game::View const&, EnchantLayout::Hit hover);
enum class Pressed { No, Yes, NewItem };
// Applies a press on one of the chooser's zones. `message` is set when the press is answered with words.
Pressed press(EnchantLayout::Hit hit, std::string& message);
}
