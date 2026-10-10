#pragma once
// The Schematics view of the settings screen (BACKLOG L-93, split out by
// L-134): this world's placements, the files, the check of the selected
// placement and its materials in four tabs, with the 3D preview. Its state
// lives in SchematicsView.cpp; the screen's core forwards input and asks it
// to draw.
#include "ui/SettingsTable.h"
#include <glm/vec2.hpp>
#include <cstdint>
class MinecraftUIRenderContext;
namespace lamium::ui::schematics_view {
enum class Tab { Files, Placements, Verify, Materials }; // mockup order
// The placements as they are now, and with `files` the folder scanned again.
void refresh(bool files);
void show(Tab tab);
// The Placed tab's row of a placement (from the world map); nothing if gone.
void showPlacement(std::uint64_t id);
void click(float x, float y, bool right);
void key(int key);
// Over the preview the wheel zooms it (with Shift: peels layers); elsewhere
// it scrolls the pane under the pointer.
void wheel(int step, glm::vec2 pointer, bool shift);
// The left button let go: a preview press that did not turn picks a block.
void release();
// While the preview is pressed: turns it once the pointer moved a little.
void drag(glm::vec2 pointer);

bool editingNumber();
void applyNumber();
void endEditing();
glm::vec2 caret();

bool docked();
void renderDocked(MinecraftUIRenderContext& context, glm::vec2 size, glm::vec2 pointer);
void renderContent(MinecraftUIRenderContext& context, glm::vec2 size, glm::vec2 pointer, SettingsTable const& table);
void reset();
}
