#pragma once
#include <string>
class MinecraftUIRenderContext;

namespace lamium::ui {
// Coordinates use the game's GUI units. Widgets borrow the render context only
// for this call; screens retain navigation and input ownership themselves.
void label(MinecraftUIRenderContext&, float x, float y, float width, std::string text);
void panel(MinecraftUIRenderContext&, float left, float top, float width, float height, float opacity = .78f);
void rowBackground(MinecraftUIRenderContext&, float left, float top, float width, float height,
                   bool selected, bool hovered);
}
