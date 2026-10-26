#include "features/inventory/BringToHand.h"
#include "features/inventory/HandPick.h"
#include "features/inventory/game/InventoryMove.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/world/actor/player/Inventory.h"
#include "mc/world/actor/player/PlayerInventory.h"
#include "mc/world/item/Item.h"
#include "mc/world/item/ItemStack.h"
#ifdef LAMIUM_SCHEMATIC_CREATIVE_PICK
#include "mc/world/item/ItemInstance.h"
#include "mc/world/item/registry/CreativeItemEntry.h"
#include "mc/world/item/registry/CreativeItemRegistry.h"
#include "mc/world/item/registry/ItemRegistryRef.h"
#include "mc/world/level/Level.h"
#endif

namespace lamium::inventory {
bool bringToHand(LocalPlayer& player, std::string_view item) {
    auto* supplies = player.mInventory.get();
    if (!supplies || supplies->mSelectedContainerId != ContainerID::Inventory) return false;
    int selected = supplies->mSelected;
    std::array<std::string, 36> names;
    for (int slot = 0; slot < 36; ++slot) {
        auto const& stack = player.getInventory().getItem(slot);
        if (!stack.isNull() && stack.mItem) names[static_cast<size_t>(slot)] = stack.getTypeName();
    }
    auto pick = pickFor(names, selected, item);
    if (pick.kind == HandPick::Select) {
        if (pick.slot != selected) supplies->selectSlot(pick.slot, ContainerID::Inventory);
        return true;
    }
    if (pick.kind != HandPick::Swap) return false;
    ItemStack held = player.getInventory().getItem(selected), wanted = player.getInventory().getItem(pick.slot);
    return game::movePair(player, {game::Place::Inventory, selected}, wanted, {game::Place::Inventory, pick.slot}, held);
}
#ifdef LAMIUM_SCHEMATIC_CREATIVE_PICK
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
    std::array<std::string, 36> names;
    for (int slot = 0; slot < 36; ++slot) {
        auto const& stack = player.getInventory().getItem(slot);
        if (!stack.isNull() && stack.mItem) names[static_cast<size_t>(slot)] = stack.getTypeName();
    }
    // Only what the creative inventory lists: the server refused others
    // (water, lava, a structure block) and the slot went back.
    CreativeItemRegistry const* registry = player.getLevel().getItemRegistry().getCreativeItemRegistry().get();
    if (!registry || !listedInCreative(*registry, item)) return notListed;
    int slot = creativeSlotFor(names, supplies->mSelected);
    if (slot < 0) return notSent;
    // Selected first, as the game's own pick does: a give into another
    // hotbar slot was refused twice before the slot was selected.
    if (slot != supplies->mSelected) supplies->selectSlot(slot, ContainerID::Inventory);
    return game::giveCreative(player, slot, item) ? slot : notSent;
}
#endif
}
