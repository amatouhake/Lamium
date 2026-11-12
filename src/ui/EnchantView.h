#pragma once
#include "ui/SettingsTable.h"
#include <glm/vec2.hpp>
class MinecraftUIRenderContext;
// Inventory > Enchantment order (BACKLOG L-170): the item, the enchantments
// wanted and the cheapest order of anvil uses. Replaces the settings rows
// while open, like the block highlight list. Client thread, under the
// screen's lock.
namespace lamium::ui::enchant_view {
// False (with a footer message) outside a world or when the game's
// enchantments cannot be read.
bool begin();
bool opened();
void reset();
// False when the click is outside the view (sidebar, header, footer).
bool click(float x, float y, bool right);
void key(int key);
void wheel(int amount);
void release();
void renderContent(MinecraftUIRenderContext&, SettingsTable const&, glm::vec2 pointer);
}
