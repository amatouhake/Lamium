#pragma once
#include "ui/SettingsTable.h"
#include <glm/vec2.hpp>
class MinecraftUIRenderContext;
// General > Game language: replaces the settings rows while open.
namespace lamium::ui::language_view {
bool begin();
bool opened();
void reset();
// Focus left the window: end a drag, keep the page open.
void calm();
// False when the click is outside the list (sidebar, header, footer).
bool click(float x, float y, bool right);
void key(int key);
void wheel(int amount);
void release();
void renderContent(MinecraftUIRenderContext&, SettingsTable const&, glm::vec2 pointer);
}
