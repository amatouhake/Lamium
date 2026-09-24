#pragma once
class MinecraftUIRenderContext;

namespace lamium::ui::hud_editor {
// HUD layout editor (BACKLOG L-04c). Runs inside the settings screen, which
// owns the scene, input routing and locking; this module owns the editor's
// selection, drag and panel state and saves through Runtime.
enum class Result { Stay, Exit };
void reset();
void render(MinecraftUIRenderContext&, float width, float height, float pointerX, float pointerY);
Result press(float x, float y);
void release();
void wheel(int step, float x, float y);
Result key(int key, bool shift);
}
