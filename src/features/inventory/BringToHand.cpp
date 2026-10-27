#include "features/inventory/BringToHand.h"
#include "features/inventory/HandPick.h"
#include "features/inventory/game/InventoryMove.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/world/actor/player/Inventory.h"
#include "mc/world/actor/player/PlayerInventory.h"
#include "mc/world/item/Item.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/item/ItemInstance.h"
#include "mc/world/item/registry/CreativeItemEntry.h"
#include "mc/world/item/registry/CreativeItemRegistry.h"
#include "mc/world/item/registry/ItemRegistryRef.h"
#include "mc/world/level/Level.h"

namespace lamium::inventory {
namespace {
// The 36 slots with their data values: the pick decisions compare both,
// because variants share a name (a skull's kind, a monster egg's).
HandSlots handSlots(LocalPlayer& player) {
    HandSlots slots{};
    for (int slot = 0; slot < 36; ++slot) {
        auto const& stack = player.getInventory().getItem(slot);
        if (stack.isNull() || !stack.mItem) continue;
        auto& at = slots[static_cast<size_t>(slot)];
        at.name = stack.getTypeName();
        at.aux = stack.getAuxValue();
    }
    return slots;
}
}
int bringToHand(LocalPlayer& player, std::string_view item, short aux) {
    auto* supplies = player.mInventory.get();
    if (!supplies || supplies->mSelectedContainerId != ContainerID::Inventory) return notCarried;
    int selected = supplies->mSelected;
    auto pick = pickFor(handSlots(player), selected, item, aux);
    if (pick.kind == HandPick::Select) {
        if (pick.slot != selected) supplies->selectSlot(pick.slot, ContainerID::Inventory);
        return pick.slot;
    }
    if (pick.kind != HandPick::Swap) return notCarried;
    ItemStack held = player.getInventory().getItem(selected), wanted = player.getInventory().getItem(pick.slot);
    if (!game::movePair(player, {game::Place::Inventory, selected}, wanted, {game::Place::Inventory, pick.slot}, held))
        return notCarried;
    return selected;
}
namespace {
bool listedInCreative(CreativeItemRegistry const& registry, ItemStack const& item) {
    auto const& name = item.getTypeName();
    short aux = item.getAuxValue();
    for (auto const& entry : registry.mCreativeItems.get()) {
        auto const& listed = entry.mItemInstance.get();
        if (!listed.isNull() && listed.getTypeName() == name && listed.getAuxValue() == aux) return true;
    }
    return false;
}
}
int giveToHand(LocalPlayer& player, ItemStack const& item) {
    auto* supplies = player.mInventory.get();
    if (!supplies || supplies->mSelectedContainerId != ContainerID::Inventory) return notSent;
    // Only what the creative inventory lists: the server refused others
    // (water, lava, a structure block) and the slot went back.
    CreativeItemRegistry const* registry = player.getLevel().getItemRegistry().getCreativeItemRegistry().get();
    if (!registry || !listedInCreative(*registry, item)) return notListed;
    int slot = creativeSlotFor(handSlots(player), supplies->mSelected);
    if (slot < 0) return notSent;
    // Selected first, as the game's own pick does: a give into another
    // hotbar slot was refused twice before the slot was selected.
    if (slot != supplies->mSelected) supplies->selectSlot(slot, ContainerID::Inventory);
    return game::giveCreative(player, slot, item) ? slot : notSent;
}
}
