#pragma once
#include "settings/Settings.h"
#include "ui/HudEditorLayout.h"
class MinecraftUIRenderContext;
namespace lamium::information {
// Layout editor preview: draw with this layout instead of the saved one, show
// every element whether or not its feature is on, and fill empty elements
// with sample content so they can be placed.
struct HudPreview { Settings::Hud layout; };
// Returns where each element was drawn this frame (for editor hit tests).
ui::hud_editor::Boxes drawHud(MinecraftUIRenderContext&, float width, float height, Settings::Information const&,
                              HudPreview const* preview = nullptr);
}
