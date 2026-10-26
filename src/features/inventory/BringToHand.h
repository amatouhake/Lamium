#pragma once
#include <string_view>
class LocalPlayer;
namespace lamium::inventory {
// Puts the item named `item` in the hand from the player's own inventory
// (HandPick.h decides the slot). Client thread only. False when it is not
// carried or another inventory move is in progress.
bool bringToHand(LocalPlayer& player, std::string_view item);
}
