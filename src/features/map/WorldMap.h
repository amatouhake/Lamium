#pragma once
#include "features/map/MapMarks.h"
#include "features/map/Waypoints.h"
#include "settings/Settings.h"
#include <glm/vec2.hpp>
#include <optional>
#include <string>
class IClientInstance;
class MinecraftUIRenderContext;
namespace lamium::map::world {
// The world map screen (BACKLOG L-60 world map, docs/demos/worldmap.html and
// worldmap-review.html). The settings screen's scene owns focus and the
// cursor and forwards its queued input here; every call runs on the client
// thread from its render.
struct Request {
    enum class Kind { None, Close, AddWaypoint, OpenWaypoints, OpenSchematic, OpenShape } kind = Kind::None;
    Waypoint draft; // AddWaypoint
    std::optional<MarkKey> mark; // Open...: the waypoint, death point, placement or shape to show, if any.
};
// `resume` keeps the view and selection (back from the Waypoints screen).
void open(IClientInstance&, bool resume = false);
// Releases the screen's textures; also on world exit.
void close();
Request press(float x, float y, bool right);
void release();
void wheel(int direction);
Request key(int key, bool openKey);
// The waypoint name typed in the side panel; text arrives from the native
// keyboard, editing keys from key().
bool editingName();
void typeText(std::string const& text);
void backspace();
void selectAllName();
glm::vec2 namePosition();
void render(MinecraftUIRenderContext&, glm::vec2 size, glm::vec2 pointer, Settings::Map const&);
}
