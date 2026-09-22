#pragma once
#include "settings/Settings.h"
class MinecraftUIRenderContext;
namespace lamium::information {
void drawHud(MinecraftUIRenderContext&, float width, float height, Settings::Information const&);
}
