#include "features/interaction/ToolGuard.h"
#include "features/inventory/EquipmentPlan.h"
#include "features/inventory/RestockUse.h"
#include "features/inventory/game/InventoryMove.h"
#include "app/Runtime.h"
#include "app/TraceLog.h"
#include "input/Actions.h"
#include "ui/Localization.h"
#include "ui/SettingsScreen.h"
#include "ui/Toast.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/service/TargetedBedrock.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/client/ClientExitLevelEvent.h"
#include "ll/api/event/world/ClientLevelTickEvent.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/world/actor/player/Inventory.h"
#include "mc/world/actor/player/PlayerInventory.h"
#include "mc/world/gamemode/GameMode.h"
#include "mc/world/item/Item.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/item/enchanting/ItemEnchants.h"
#include "mc/world/item/enchanting/EnchantmentInstance.h"
#include "mc/deps/shared_types/legacy/actor/ArmorSlot.h"
#include <atomic>
#include <chrono>
#include <stdexcept>

namespace lamium::interaction::toolGuard {
namespace {
using namespace inventory;
using Clock = std::chrono::steady_clock;
bool installed = false;
ll::event::ListenerPtr tickListener, exitListener;
// The held tool a stop applied to; a new press with it mines on (overridden).
struct Identity {
    int slot = -1, id = 0, damage = -1;
    bool operator==(Identity const&) const = default;
};
Identity stopped, overridden;
Identity heldSeen, chestSeen;
// Set when the same held item or worn elytra wore down to breaking, so merely
// selecting or wearing one kept at 1 (for Mending) never swaps it.
bool heldWornDown = false, chestWornDown = false;
Clock::time_point lastDestroy{}, lastHeldChange{}, lastChestChange{};
void trace(char const* stage, int value = 0) noexcept {
#ifdef LAMIUM_RESTOCK_TRACE
    static TraceBudget budget;
    traceLog(budget, 2048, "ToolGuard: {} value={}", stage, value);
#else
    (void)stage; (void)value;
#endif
}
LocalPlayer* localPlayer(Player const* actor = nullptr) {
    auto& runtime = Runtime::instance();
    if (!runtime.enabled() || !runtime.snapshot()->interaction.toolGuard) return nullptr;
    auto client = ll::service::getClientInstance();
    auto* player = client ? client->getLocalPlayer() : nullptr;
    if (!player || (actor && static_cast<Player const*>(player) != actor)) return nullptr;
    if (!player->isAlive() || player->isCreative() || player->isSpectator() || !player->mInventory) return nullptr;
    auto& inventory = *player->mInventory;
    if (inventory.mSelectedContainerId != ContainerID::Inventory || inventory.mSelected < 0 || inventory.mSelected >= 9)
        return nullptr;
    return player;
}
bool damageable(ItemStack const& stack) {
    return !stack.isNull() && stack.mCount > 0 && stack.mItem && stack.mItem->getMaxDamage() > 0;
}
bool breaking(ItemStack const& stack) {
    return damageable(stack) && aboutToBreak(stack.mItem->getMaxDamage(),stack.getDamageValue());
}
Identity identity(ItemStack const& stack, int slot) {
    if (!damageable(stack)) return {};
    return {slot,static_cast<int>(stack.getId()),stack.getDamageValue()};
}
std::vector<EnchantLevel> enchants(ItemStack const& stack) {
    std::vector<EnchantLevel> result;
    if (!stack.isEnchanted()) return result;
    for (auto const& e : stack.constructItemEnchantsFromUserData().getAllEnchants())
        result.push_back({static_cast<int>(static_cast<::Enchant::Type>(e.mEnchantType)),e.mLevel});
    return result;
}
// The same item, closest enchantments first: the main inventory, then other
// hotbar slots (maintainer, 2026-09-30). The selection never changes.
std::optional<int> replacement(LocalPlayer& player, ItemStack const& worn) {
    auto wanted = enchants(worn);
    auto search = [&](int first, int last) {
        std::vector<ReplacementCandidate> candidates;
        for (int slot = first; slot < last; ++slot) {
            auto const& stack = player.getInventory().getItem(slot);
            if (slot == player.mInventory->mSelected || !damageable(stack) || stack.getId() != worn.getId()) continue;
            candidates.push_back({slot,enchantDistance(wanted,enchants(stack)),
                stack.mItem->getMaxDamage() - stack.getDamageValue()});
        }
        return chooseReplacement(candidates);
    };
    if (auto slot = search(9,36)) return slot;
    return search(0,9);
}
int msSince(Clock::time_point then) {
    return static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - then).count());
}
// Durability is changed by the server's handling of the last break or hit;
// a move sent right after it could reach the server first (L-66 ordering).
bool quiet(Clock::time_point lastChange) {
    return msSince(lastDestroy) >= restockQuietMs && msSince(lastChange) >= restockQuietMs;
}
bool swapHeld(LocalPlayer& player) {
    int selected = player.mInventory->mSelected;
    auto const& held = player.getInventory().getItem(selected);
    auto source = replacement(player,held);
    if (!source) return false;
    ItemStack worn = held, fresh = player.getInventory().getItem(*source);
    bool moved = game::movePair(player,{game::Place::Inventory,selected},fresh,{game::Place::Inventory,*source},worn);
    trace(moved ? "held-swapped" : "held-swap-busy",*source);
    return moved;
}
bool swapElytra(LocalPlayer& player) {
    auto const& chest = player.getArmor(SharedTypes::Legacy::ArmorSlot::Torso);
    auto source = replacement(player,chest);
    if (!source) return false;
    ItemStack worn = chest, fresh = player.getInventory().getItem(*source);
    bool moved = game::movePair(player,{game::Place::Armor,1},fresh,{game::Place::Inventory,*source},worn);
    trace(moved ? "elytra-swapped" : "elytra-swap-busy",*source);
    return moved;
}
void tick() noexcept {
    try {
        auto* player = localPlayer();
        auto client = ll::service::getClientInstance();
        if (!player || !client || ui::ownsInput() || !client->isInGameInputEnabled()) return;
        int selected = player->mInventory->mSelected;
        auto const& held = player->getInventory().getItem(selected);
        auto heldNow = identity(held,selected);
        auto wore = [](Identity const& was, Identity const& now) {
            return was.slot == now.slot && was.id == now.id && now.id && now.damage > was.damage;
        };
        if (heldNow != heldSeen) {
            heldWornDown = wore(heldSeen,heldNow) ? breaking(held) : heldWornDown && heldNow.slot == heldSeen.slot && heldNow.id == heldSeen.id;
            heldSeen = heldNow; lastHeldChange = Clock::now();
        }
        if (overridden != heldNow) overridden = {};
        auto const& chest = player->getArmor(SharedTypes::Legacy::ArmorSlot::Torso);
        auto chestNow = identity(chest,-2);
        if (chestNow != chestSeen) {
            chestWornDown = wore(chestSeen,chestNow) ? breaking(chest) : chestWornDown && chestNow.id == chestSeen.id;
            chestSeen = chestNow; lastChestChange = Clock::now();
        }
        if (heldWornDown && breaking(held) && overridden != heldNow && quiet(lastHeldChange) && swapHeld(*player))
            heldWornDown = false;
        // The elytra stops working at 1 durability rather than breaking.
        if (chestWornDown && breaking(chest) && chest.getTypeName() == "minecraft:elytra" && quiet(lastChestChange)
            && swapElytra(*player)) chestWornDown = false;
    } catch (std::exception const& error) {
        static bool reported = false;
        if (!reported) { reported = true; Runtime::instance().self().getLogger().error("Tool Protection stopped: {}",error.what()); }
    } catch (...) {}
}
void stopToast() {
    try { ui::showMessageToast(ui::translated("toolGuard.stopped")); } catch (...) {}
}
// What mining with the held item should do now, applying a new press.
GuardMining decide(LocalPlayer& player, bool newPress) {
    int selected = player.mInventory->mSelected;
    auto const& held = player.getInventory().getItem(selected);
    auto id = identity(held,selected);
    bool about = breaking(held);
    if (!about) return GuardMining::Continue;
    if (newPress && stopped == id && !Runtime::instance().snapshot()->interaction.toolGuardStrict) overridden = id;
    auto action = guardMining(true,replacement(player,held).has_value(),overridden == id);
    if (action == GuardMining::Stop) stopped = id;
    return action;
}
void reset() { stopped = overridden = heldSeen = chestSeen = {}; heldWornDown = chestWornDown = false; }
}
mining::Gate startGate(Player& actor, bool newPress) {
    try {
        if (auto* player = localPlayer(&actor)) {
            switch (decide(*player,newPress)) {
            case GuardMining::Stop:
                trace("start-stop"); stopToast(); return mining::Gate::End;
            case GuardMining::Wait:
                // A fresh press after a pause can swap at once; right after a
                // break it waits for the tick (press again then).
                if (!quiet(lastHeldChange) || !swapHeld(*player)) { trace("start-wait"); return mining::Gate::End; }
                break;
            case GuardMining::Continue: break;
            }
            lastDestroy = Clock::now();
        }
    } catch (...) {}
    return mining::Gate::Proceed;
}
mining::Gate continueGate(Player& actor) {
    try {
        if (auto* player = localPlayer(&actor)) {
            switch (decide(*player,false)) {
            case GuardMining::Stop:
                // Ending the session: a held button never restarts it (L-36),
                // so only a new press mines on.
                trace("continue-stop"); stopToast(); return mining::Gate::End;
            case GuardMining::Wait:
                // Keep the session but make no progress until the swap;
                // mining with it is reason enough to swap it out.
                heldWornDown = true;
                trace("continue-wait");
                return mining::Gate::Pause;
            case GuardMining::Continue: break;
            }
            lastDestroy = Clock::now();
        }
    } catch (...) {}
    return mining::Gate::Proceed;
}
void start() {
    if (installed) return;
    try {
        auto& bus = ll::event::EventBus::getInstance();
        tickListener = bus.emplaceListener<ll::event::ClientLevelTickEvent>([](auto&) { tick(); });
        exitListener = bus.emplaceListener<ll::event::ClientExitLevelEvent>([](auto&) { reset(); });
        if (!tickListener || !exitListener) throw std::runtime_error("Could not subscribe Tool Protection lifecycle");
    } catch (...) { stop(); throw; }
    installed = true;
}
void stop() {
    for (auto* listener : {&tickListener,&exitListener}) if (*listener) {
        ll::event::EventBus::getInstance().removeListener(*listener); listener->reset();
    }
    reset();
    installed = false;
}
}
