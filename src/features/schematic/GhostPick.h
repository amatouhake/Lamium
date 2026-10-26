#pragma once

namespace lamium::schematic::ghosts {
// The game's pick block on a ghost (L-263): the ghost's block comes from the
// inventory into the hand instead of the real block behind it.
void startPick();
void stopPick();
}
