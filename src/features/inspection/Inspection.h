#pragma once
class MinecraftUIRenderContext;
namespace lamium::inspection {
bool start();
void stop();
// Called right before the game draws an item tooltip, inside the screen's
// own drawing: what it paints is above the screen's controls and under the
// tooltip (L-170's list beside the anvil). One painter; null removes it.
// Client thread.
void setBeforeTooltip(void (*painter)(MinecraftUIRenderContext&));
}
