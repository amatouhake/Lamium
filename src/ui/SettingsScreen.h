#pragma once
#include "features/map/Waypoints.h"
class IClientInstance;
namespace lamium::ui {
void start();
void stop();
void open(IClientInstance& client);
void openShapes(IClientInstance& client);
void openHotkeys(IClientInstance& client);
void openHudLayout(IClientInstance& client);
// The waypoint add prompt over the world, for a waypoint already placed.
void openWaypointPrompt(IClientInstance& client, map::Waypoint draft);
void openWaypoints(IClientInstance& client);
// tab: -1 keeps the last one, else 0 Files, 1 Placed, 2 Check, 3 Materials.
void openSchematics(IClientInstance& client, int tab);
// The settings page with the schematic keys.
void openSchematicKeys(IClientInstance& client);
// The save prompt for the area chosen with the corner keys (L-93).
void openSchematicSave(IClientInstance& client);
// The schematic menu over the world (L-93).
void openSchematicMenu(IClientInstance& client);
void openWorldMap(IClientInstance& client);
// The calculator panel over the world (L-171).
void openCalculator(IClientInstance& client);
// The block highlight list in the settings (L-320).
void openBlockHighlight(IClientInstance& client);
// The enchantment order planner in the settings (L-170).
// `overAnvil`: asked from the anvil list; the screen may then open over the anvil screen.
void openEnchantOrder(IClientInstance& client, bool overAnvil = false);
// General > Saved data (L-343).
void openSavedData(IClientInstance& client);
bool ownsInput();
void cancelInputCapture();
}
