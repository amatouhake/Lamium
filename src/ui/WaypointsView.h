#pragma once
// The Waypoints view of the settings screen (BACKLOG L-134): this world's
// waypoints and death point, nearest first, and the editor of the selected
// one, docked over the world or inside the settings table. Its state lives
// in WaypointsView.cpp; the screen's core forwards input and asks it to draw.
#include "features/map/MapMarks.h"
#include "ui/SettingsTable.h"
#include <glm/vec2.hpp>
#include <optional>
#include <string>
class MinecraftUIRenderContext;
namespace lamium::ui::waypoints_view {
// The waypoints as they are now; before each frame's input.
void refresh();
// Shows the death point or a waypoint (from the world map).
void select(std::optional<map::MarkKey> mark);
void click(float x, float y, bool right);
void key(int key);
void wheel(int step, glm::vec2 pointer);

bool editingName();
bool editingNumber();
void type(std::string const& text);
void applyNumber();
void applyName();
void endEditing();
glm::vec2 caret();

bool docked();
void renderDocked(MinecraftUIRenderContext& context, glm::vec2 size, glm::vec2 pointer);
void renderContent(MinecraftUIRenderContext& context, glm::vec2 size, glm::vec2 pointer, SettingsTable const& table);
void reset();
// Opened from the world map: Close and Esc go back to it.
void setFromMap(bool fromMap);
bool fromMap();
}
