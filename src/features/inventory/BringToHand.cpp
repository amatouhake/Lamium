#include "features/inventory/BringToHand.h"
#include "features/inventory/HandPick.h"
#include "features/inventory/game/InventoryMove.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/world/actor/player/Inventory.h"
#include "mc/world/actor/player/PlayerInventory.h"
#include "mc/world/item/Item.h"

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
}
