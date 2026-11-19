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
// While `wants` answers true, the game's tooltip is not drawn inside the
// screen but after every after-render painter, on top of them (the recipe
// list's bands). `dropDeferredTooltip` throws this frame's away, the
// container preview too (a slot under Lamium's open window). One asker; null removes it. Client thread.
void setDeferTooltip(bool (*wants)());
void dropDeferredTooltip();
}
