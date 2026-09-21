#pragma once
class ScreenView;
class MinecraftUIRenderContext;
class ItemStackBase;
namespace lamium::inspection::render {
void renderDurability(ScreenView&, MinecraftUIRenderContext&, ItemStackBase const&);
}
