#pragma once
#include <string_view>
class ItemStack;
class LocalPlayer;
namespace lamium::inventory {
// Puts the item named `item` with the data value `aux` into the hand from
// the player's own inventory (HandPick.h decides the slot, comparing both).
// Client thread only. The slot that holds it now (selected, or the selected
// slot after a swap), or notCarried when it is not carried or another
// inventory move is in progress.
inline constexpr int notCarried = -1;
int bringToHand(LocalPlayer& player, std::string_view item, short aux);
// Creative mode, `item` not carried (L-263): gives it from the
// creative inventory into the hotbar slot creativeSlotFor chooses and
// selects that slot. The slot it went to, or notSent / notListed.
inline constexpr int notSent = -1;   // no hotbar slot, or another move in progress
inline constexpr int notListed = -2; // the creative inventory does not list it
int giveToHand(LocalPlayer& player, ItemStack const& item);
}
