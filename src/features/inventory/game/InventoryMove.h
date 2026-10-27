#pragma once
class ItemStack;
class LocalPlayer;
namespace lamium::inventory::game {
// A player-owned slot a screenless move can change: the 36 inventory slots,
// the offhand, or an armor slot (0 head, 1 chest, 2 legs, 3 feet).
enum class Place { Inventory, Offhand, Armor };
struct Location { Place place; int slot; };
ItemStack const& itemAt(LocalPlayer& player, Location at);
// Predicts two slots through the client's own setters under a legacy
// request scope and flushes the resulting balanced transaction (L-66). A
// side the setter did not record (offhand/armor) is added as its own action.
// Returns false, changing nothing, when another transaction or request is
// in progress. Throws if the transaction stays unbalanced after mutation.
bool movePair(LocalPlayer& player, Location a, ItemStack const& newA, Location b, ItemStack const& newB);
// L-263: puts `item` into hotbar slot `slot` from the creative inventory,
// like movePair: the slot's setter under a legacy request scope, plus
// creative-inventory actions giving `item` and taking back what the slot
// held. Accepted in a local world (2026-10-26); servers not yet checked.
// False, changing nothing, when not in creative mode or another
// transaction or request is in progress.
bool giveCreative(LocalPlayer& player, int slot, ItemStack const& item);
// True while movePair is changing slots, so observers ignore its own effects.
bool moving();
// Installs the action observer movePair relies on; without it moves refuse.
void startMoves();
void stopMoves();
}
