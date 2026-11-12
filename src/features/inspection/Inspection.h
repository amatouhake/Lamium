#pragma once
namespace lamium::inspection {
bool start();
void stop();
// The box of the game's item tooltip drawn since the last call, in GUI
// units, if one was drawn (L-170 keeps its list out from under it). Client
// thread.
struct TooltipBox { float x = 0, y = 0, w = 0, h = 0; };
bool takeTooltip(TooltipBox& box);
}
