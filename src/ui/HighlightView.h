#pragma once
#include "ui/SettingsTable.h"
#include "ui/TextEdit.h"
#include <glm/vec2.hpp>
class MinecraftUIRenderContext;
// Camera > Block highlight > the list (BACKLOG L-320 step 2): the listed
// types and a search over every block type. Replaces the settings rows while
// open, like the game-language list. Client thread, under the screen's lock.
namespace lamium::ui::highlight_view {
// False (with a footer message) outside a world.
bool begin();
bool opened();
void reset();
// Focus left the window: end a drag, disarm, keep the page open.
void calm();
// The search field, always focused while open.
TextEdit& field();
void edited();
void type(std::string const& text);
// False when the click is outside the view (sidebar, header, footer).
bool click(float x, float y, bool right);
void key(int key);
void wheel(int amount);
void release();
void renderContent(MinecraftUIRenderContext&, SettingsTable const&, glm::vec2 pointer);
}
