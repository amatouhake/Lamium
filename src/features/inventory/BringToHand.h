#pragma once
#include <string_view>
class ItemStack;
class LocalPlayer;
namespace lamium::inventory {
// Puts the item named `item` in the hand from the player's own inventory
// (HandPick.h decides the slot). Client thread only. False when it is not
// carried or another inventory move is in progress.
bool bringToHand(LocalPlayer& player, std::string_view item);
#ifdef LAMIUM_SCHEMATIC_CREATIVE_PICK
// Creative mode, `item` not carried (L-263 experiment): gives it from the
// creative inventory into the hotbar slot creativeSlotFor chooses and
// selects that slot. The slot it went to, or notSent / notListed.
inline constexpr int notSent = -1;   // no hotbar slot, or another move in progress
inline constexpr int notListed = -2; // the creative inventory does not list it
int giveToHand(LocalPlayer& player, ItemStack const& item);
#endif
}
