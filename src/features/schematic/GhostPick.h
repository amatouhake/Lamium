#pragma once

class LocalPlayer;
namespace lamium::schematic::ghosts {
// The game's pick block on a ghost (L-263): the ghost's block comes from the
// inventory into the hand instead of the real block behind it.
void startPick();
void stopPick();
// Reads back the pick from an earlier frame against the slot the server left
// behind (L-263); once per frame from the ghost pass, cheap when none waits.
void stepPick(LocalPlayer& player);
}
