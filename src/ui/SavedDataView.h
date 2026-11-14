#pragma once
#include "ui/SettingsTable.h"
#include <glm/vec2.hpp>
#include <string>
class MinecraftUIRenderContext;
// General > Saved data (BACKLOG L-343): replaces the settings rows while open.
namespace lamium::ui::saved_data_view {
bool begin();
bool opened();
void reset();
// False when the click is outside the view (sidebar, header, footer).
bool click(float x, float y, bool right);
void key(int key);
void wheel(int amount);
void release();
void renderContent(MinecraftUIRenderContext&, SettingsTable const&, glm::vec2 pointer);
// The joined server's data as its row shows it; empty when there is nothing to choose.
std::string currentName();
}
