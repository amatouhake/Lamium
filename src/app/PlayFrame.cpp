#include "app/PlayFrame.h"
#include "app/Runtime.h"
#include "features/map/Minimap.h"
#include "features/map/SavedData.h"
#include "features/map/WaypointSession.h"
#include "features/schematic/GhostRenderer.h"
#include "features/schematic/SchematicActions.h"
#include "features/visuals/BlockHighlightHooks.h"
#include "features/visuals/ServerPacks.h"
#include "ui/Localization.h"
#include "ui/SavedDataView.h"
#include "ui/Toast.h"
#include "mc/client/game/IClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include <cmath>

namespace lamium {
void playFrame(IClientInstance& client) {
    auto const snapshot = Runtime::instance().snapshot();
    auto const& runtime = *snapshot;
    map::record(client, runtime.map);
    // Waypoints off is all of it off: no death point, nothing reached (L-319).
    map::waypoints::frame(runtime.map.waypoints && runtime.map.waypointsDeath, runtime.map.waypointsDeathTemporary,
                          ui::translated("waypoint.deathName"));
    // A map merge that ended while playing (L-343): the saved data page would have said it.
    if (auto merged = map::waypoints::takeFinishedMerge()) ui::showMessageToast(ui::saved_data_view::mergeReport(*merged));
    // The world differs from its saved data (L-365): the tool screens ask, this says where.
    if (map::waypoints::takeWorldToast()) ui::showMessageToast(ui::translated("savedData.worldToast"), 6);
    // The body arrives, not FreeCamera's view.
    if (auto* player = client.getLocalPlayer(); player && player->isAlive()) {
        auto feet = player->getFeetPos();
        if (runtime.map.waypoints && std::isfinite(feet.x) && std::isfinite(feet.y) && std::isfinite(feet.z))
            for (auto& name : map::waypoints::arrive(static_cast<int>(player->getDimensionId()), feet.x, feet.y, feet.z,
                                                     runtime.map.waypointArrival))
                ui::showMessageToast(ui::translated("waypoint.reached", name));
    }
    if (auto message = schematic::ghosts::takeSaveMessage()) ui::showMessageToast(std::move(*message));
    // Once the world shows: the server's look is missing on purpose.
    if (int packs = visuals::serverPacks::takeSkipped()) ui::showMessageToast(ui::translated("skipServerPacks.notice", packs), 6);
    if (auto shown = visuals::highlight::takeCapped()) ui::showMessageToast(ui::translated("highlight.capped", shown), 6);
    if (runtime.schematic.enabled) schematic::actions::adjustFrame(client);
}
}
