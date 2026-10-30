#pragma once
#include "features/map/MapCave.h"
#include "settings/Settings.h"
#include "ui/HudEditorLayout.h"
#include <optional>
class IClientInstance;
class MinecraftUIRenderContext;
namespace lamium::map {
// Minimap HUD element (BACKLOG L-60). Called once per frame from the HUD
// draw, on the client thread; scans, composes and uploads within a budget.
std::optional<ui::hud_editor::Box> drawMinimap(MinecraftUIRenderContext&, float width, float height,
                                               ui::HudElement const&, Settings::Map const&, bool preview,
                                               float cardOpacity = .72f);
// The world map's recording (BACKLOG L-60 world map): scans around the player
// into the saved regions whether or not the minimap is shown. Called once per
// frame on the client thread from the HUD draw and the world map screen.
// `scan` false only keeps the saved map attached (the world map screen with
// the feature off): nothing new is recorded.
void record(IClientInstance&, Settings::Map const&, bool scan = true);
// The automatic cave/surface view at the player, independent of a forced
// HUD view; the world map uses it for its opening choice (L-228).
ViewMode automaticView();
// The height of the cave band currently used for recording. The world map
// opens at this height so a boundary step does not show an empty new band;
// empty until the first map frame.
std::optional<int> caveLayerY();
// Whether the body (not the FreeCamera eye) is in a cave. The world map uses
// this when it follows the body, so a camera in a cave does not decide it.
bool bodyCave(IClientInstance&);
// The hold-to-enlarge key: twice the size and twice the area at the same scale.
void setEnlarged(bool held);
// The radar's hold key: mob faces become dots (or dots faces) while held.
void setFacesHeld(bool held);
// Whether the server's command list for this player includes /tp (teleport);
// empty until the list arrives in this world.
std::optional<bool> teleportListed();
void start();
void stop();
}
