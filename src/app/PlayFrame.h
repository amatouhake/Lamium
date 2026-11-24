#pragma once
class IClientInstance;
namespace lamium {
// What runs once every frame of play, apart from anything drawn (L-401): the
// map's recording, the death point and waypoints reached, the notices
// features leave for a toast, and the schematic adjust key's wheel. Called
// before the HUD draws, also while the HUD is hidden (L-107); the layout
// editor's preview runs none of it.
void playFrame(IClientInstance& client);
}
