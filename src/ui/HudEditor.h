#pragma once
#include "ui/HudEditorLayout.h"
#include <optional>
class MinecraftUIRenderContext;

namespace lamium::ui::hud_editor {
// HUD layout editor (BACKLOG L-04c). Runs inside the settings screen, which
// owns the scene, input routing and locking; this module owns the editor's
// selection, drag and panel state and saves through Runtime.
enum class Result { Stay, Exit };
void reset();
void select(std::optional<HudElementId>);
void render(MinecraftUIRenderContext&, float width, float height, float pointerX, float pointerY);
Result press(float x, float y);
void release();
void wheel(int step, float x, float y);
Result key(int key, bool shift);
// The toolbar's scale typed as a number (screen::number(), L-321): the core
// routes text to it, applies it each frame and ends it like any other number.
bool editingNumber();
void applyNumber();
void endEditing();
void editKey(int key);
// Where the typed number is drawn, for the text keyboard.
std::optional<Point> caret();
}
