#include "features/inventory/game/InventoryMove.h"
#include "app/Runtime.h"
#include "app/TraceLog.h"
#include "ll/api/memory/Hook.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/world/actor/player/Inventory.h"
#include "mc/world/actor/player/PlayerInventory.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/inventory/network/ItemStackNetManagerClient.h"
#include "mc/world/inventory/transaction/InventoryAction.h"
#include "mc/world/inventory/transaction/InventorySource.h"
#include "mc/world/inventory/transaction/InventoryTransaction.h"
#include "mc/world/inventory/transaction/InventoryTransactionManager.h"
#include "mc/deps/shared_types/legacy/actor/ArmorSlot.h"
#include <atomic>
#include <stdexcept>

namespace lamium::inventory::game {
namespace {
bool active = false;
// Containers whose setters recorded an action during the current move. The
// transaction's own action map is not read: walking it after an armor setter
// crashed (04b594d follow-up), so the manager's addAction is observed instead.
bool recordedInventory = false, recordedOffhand = false, recordedArmor = false;
bool hookInstalled = false;
void trace(char const* stage, int value) noexcept {
#ifdef LAMIUM_RESTOCK_TRACE
    static TraceBudget budget;
    traceLog(budget, 2048, "Move: {} value={}", stage, value);
#else
    (void)stage; (void)value;
#endif
}
ContainerID container(Place place) {
    return place == Place::Offhand ? ContainerID::Offhand : place == Place::Armor ? ContainerID::Armor : ContainerID::Inventory;
}
void set(LocalPlayer& player, Location at, ItemStack const& item) {
    switch (at.place) {
    case Place::Inventory: player.getInventory().$setItem(at.slot,item); break;
    case Place::Offhand: player.setOffhandSlot(item); break;
    case Place::Armor: player.setArmor(static_cast<SharedTypes::Legacy::ArmorSlot>(at.slot),item); break;
    }
}
bool recorded(Place place) {
    return place == Place::Offhand ? recordedOffhand : place == Place::Armor ? recordedArmor : recordedInventory;
}
LL_TYPE_INSTANCE_HOOK(MoveAddAction, ll::memory::HookPriority::Normal, InventoryTransactionManager,
    &InventoryTransactionManager::addAction, void, InventoryAction const& action, bool forceBalanced) {
    if (active) {
        auto const& source = action.mSource.get();
        if (source.mType == InventorySourceType::ContainerInventory) {
            if (source.mContainerId == ContainerID::Inventory) recordedInventory = true;
            if (source.mContainerId == ContainerID::Offhand) recordedOffhand = true;
            if (source.mContainerId == ContainerID::Armor) recordedArmor = true;
        }
    }
    origin(action,forceBalanced);
}
}
void startMoves() {
    if (hookInstalled) return;
    if (MoveAddAction::hook(true) != 0) throw std::runtime_error("Could not observe inventory move actions");
    hookInstalled = true;
}
void stopMoves() {
    if (hookInstalled && MoveAddAction::unhook(true)) hookInstalled = false;
}
ItemStack const& itemAt(LocalPlayer& player, Location at) {
    switch (at.place) {
    case Place::Offhand: return player.getOffhandSlot();
    case Place::Armor: return player.getArmor(static_cast<SharedTypes::Legacy::ArmorSlot>(at.slot));
    default: return player.getInventory().getItem(at.slot);
    }
}
bool moving() { return active; }
bool giveCreative(LocalPlayer& player, int slot, ItemStack const& item) {
    if (active || !hookInstalled || !player.isCreative() || slot < 0 || slot >= 9 || item.isNull()) return false;
    auto& manager = player.mTransactionManager.get();
    auto* base = player.mItemStackNetManager.get();
    if (manager.mCurrentTransaction.get()) { trace("transaction-busy",0); return false; }
    if (!base || !base->mIsEnabled || !base->mIsClientSide) { trace("manager-unavailable",0); return false; }
    if (static_cast<ItemStackNetManagerClient*>(base)->mRequest) { trace("request-busy",0); return false; }
    ItemStack old = player.getInventory().getItem(slot);
    struct Active { Active() { active = true; } ~Active() { active = false; } } guard;
    recordedInventory = false;
    auto scope = ItemStackNetManagerBase::_tryBeginClientLegacyTransactionRequest(&player);
    if (!base->mLegacyTransactionRequestId->mRawId) { trace("no-legacy-scope",0); return false; }
    player.getInventory().$setItem(slot,item);
    if (manager.mCurrentTransaction.get()) {
        InventorySource inventory;
        inventory.mType = InventorySourceType::ContainerInventory;
        inventory.mContainerId = ContainerID::Inventory;
        inventory.mFlags = InventorySource::InventorySourceFlags::NoFlag;
        if (!recordedInventory) manager.addAction(InventoryAction{inventory,static_cast<uint>(slot),old,item},false);
        // The creative inventory gives the item (slot 1: create) and takes
        // the replaced one (slot 0: delete), so the transaction balances.
        InventorySource creative;
        creative.mType = InventorySourceType::CreativeInventory;
        creative.mContainerId = ContainerID::None;
        creative.mFlags = InventorySource::InventorySourceFlags::NoFlag;
        manager.addAction(InventoryAction{creative,1,item,ItemStack()},false);
        if (!old.isNull()) manager.addAction(InventoryAction{creative,0,ItemStack(),old},false);
    } else trace("recorded-nothing-pending",0);
    if (manager.mCurrentTransaction.get()) player.updateInventoryTransactions();
    if (manager.mCurrentTransaction.get()) throw std::runtime_error("Creative pick transaction remained unbalanced");
    trace("creative-sent",slot);
    return true;
}
bool movePair(LocalPlayer& player, Location a, ItemStack const& newA, Location b, ItemStack const& newB) {
    if (active || !hookInstalled) return false;
    auto& manager = player.mTransactionManager.get();
    auto* base = player.mItemStackNetManager.get();
    if (manager.mCurrentTransaction.get()) { trace("transaction-busy",0); return false; }
    if (!base || !base->mIsEnabled || !base->mIsClientSide) { trace("manager-unavailable",0); return false; }
    if (static_cast<ItemStackNetManagerClient*>(base)->mRequest) { trace("request-busy",0); return false; }
    ItemStack oldA = itemAt(player,a), oldB = itemAt(player,b);
    struct Active { Active() { active = true; } ~Active() { active = false; } } guard;
    recordedInventory = recordedOffhand = recordedArmor = false;
    auto scope = ItemStackNetManagerBase::_tryBeginClientLegacyTransactionRequest(&player);
    if (!base->mLegacyTransactionRequestId->mRawId) { trace("no-legacy-scope",0); return false; }
    trace("set-first",static_cast<int>(a.place));
    set(player,a,newA);
    trace("set-second",static_cast<int>(b.place));
    set(player,b,newB);
    // The inventory setters record their own actions (L-66). Offhand and armor
    // setters are not established to; record a missing side explicitly so the
    // transaction balances instead of leaving a half-recorded move.
    if (manager.mCurrentTransaction.get()) {
        for (auto [at, from, to] : {std::tuple{a,&oldA,&newA}, std::tuple{b,&oldB,&newB}}) {
            bool own = recorded(at.place);
            trace(at.place == Place::Inventory ? "recorded-inventory" : at.place == Place::Offhand ? "recorded-offhand" : "recorded-armor",own);
            if (own || at.place == Place::Inventory) continue;
            InventorySource source;
            source.mType = InventorySourceType::ContainerInventory;
            source.mContainerId = container(at.place);
            source.mFlags = InventorySource::InventorySourceFlags::NoFlag;
            manager.addAction(InventoryAction{source,static_cast<uint>(at.place == Place::Offhand ? 0 : at.slot),*from,*to},false);
        }
    } else trace("recorded-nothing-pending",0);
    if (manager.mCurrentTransaction.get()) player.updateInventoryTransactions();
    if (manager.mCurrentTransaction.get()) throw std::runtime_error("Inventory move transaction remained unbalanced");
    trace("sent",static_cast<int>(a.place) * 10 + static_cast<int>(b.place));
    return true;
}
}
