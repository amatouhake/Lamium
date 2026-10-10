#pragma once
// The Shapes view of the settings screen (BACKLOG L-134): the list of this
// world's shapes, the editor of the selected one or of an unsaved draft,
// docked over the world or inside the settings table. Its state lives in
// ShapesView.cpp; the screen's core forwards input and asks it to draw.
#include "overlay/ShapeCollection.h"
#include "ui/SettingsTable.h"
#include <glm/vec2.hpp>
#include <optional>
#include <string>
class MinecraftUIRenderContext;
namespace lamium::ui::shapes_view {
// The list as it is now; before each frame's input.
void refresh();
void click(float x, float y, bool right);
void key(int key);
// Shows a shape (from the world map); nothing if it is gone.
void select(overlay::ShapeId id);
// Scrolls the pane under the pointer; the selection stays.
void wheel(int step, glm::vec2 pointer);

// Typing: the shape's name, or a numeric field (screen::number()).
bool editingName();
bool editingNumber();
void type(std::string const& text);
void applyNumber();
void applyName();
void endEditing();
// Where the native keyboard's caret goes.
glm::vec2 caret();

bool docked();
void renderDocked(MinecraftUIRenderContext& context, glm::vec2 size, glm::vec2 pointer);
void renderContent(MinecraftUIRenderContext& context, glm::vec2 size, glm::vec2 pointer, SettingsTable const& table);
// Another view was chosen, or the screen closed: a draft is never kept.
void leave();
void reset();
// Where a range warning was raised (L-81).
int fieldSelected();
std::optional<overlay::ShapeId> selected();
}
