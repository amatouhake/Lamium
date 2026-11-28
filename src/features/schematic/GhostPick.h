#pragma once
#include "features/schematic/GhostRenderer.h"
#include <optional>

class Block;
class HitResult;
class ItemStack;
class LocalPlayer;
namespace lamium::schematic::ghosts {
// The first ghost on the game's pick ray in front of what the game hit
// (L-263, L-285); the placement assist (L-265) aims with the same ray.
// With `accept`, the first ghost it accepts (GhostRenderer.h ghostAlong).
std::optional<GhostAt> aimedGhost(LocalPlayer& player, HitResult const& hit,
                                  std::function<bool(GhostAt const&)> const& accept = {});
// The ghost's block as one item, as the game's own pick would give it.
ItemStack ghostItem(Block const& block);
// The game's pick block on a ghost (L-263): the ghost's block comes from the
// inventory into the hand instead of the real block behind it.
void startPick();
void stopPick();
// Reads back the pick from an earlier frame against the slot the server left
// behind (L-263); once per frame from the ghost pass, cheap when none waits.
void stepPick(LocalPlayer& player);
}
