#pragma once
#include "features/map/SavedData.h"
#include <glm/vec2.hpp>
class MinecraftUIRenderContext;
// The notice on the tool screens (BACKLOG L-343, docs/demos/server-data.html):
// other saved data may be the joined server's and holds more of what this
// screen shows. It says whose, and offers to use it, to open General > Saved
// data, or not to be shown again for this server.
namespace lamium::ui::data_notice {
using Kind = map::waypoints::DataKind;
inline constexpr float bandHeight = 46;
// The room the screen leaves for it: bandHeight while there is something to say, else 0.
float height(Kind kind);
// `solid`: over the world map, where nothing else is behind it.
void draw(MinecraftUIRenderContext&, float x, float y, float width, Kind kind, glm::vec2 pointer, bool solid = false);
// A press at (x, y): Outside when not on the notice drawn last. "Use this data" and "Do not show again" are
// done here; opening the Saved data page is the caller's, which knows how to leave its own screen.
enum class Pressed { Outside, Inside, Open };
Pressed press(float x, float y);
}
