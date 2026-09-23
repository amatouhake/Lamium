#include "features/inventory/HandRestock.h"
#include "features/inventory/RestockPlan.h"
#include "features/inventory/game/RequestTracker.h"
#include "app/Runtime.h"
#include "ui/SettingsScreen.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/service/TargetedBedrock.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/client/ClientExitLevelEvent.h"
#include "ll/api/event/world/ClientLevelTickEvent.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/game/MinecraftGame.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/client/gui/screens/models/ClientInstanceScreenModel.h"
#include "mc/world/containers/managers/controllers/HudContainerManagerController.h"
#include "mc/world/containers/managers/models/ContainerManagerModel.h"
#include "mc/world/containers/SlotData.h"
#include "mc/world/actor/player/PlayerInventory.h"
#include "mc/world/actor/player/Inventory.h"
#include "mc/world/gamemode/GameMode.h"
#include "mc/world/gamemode/InteractionResult.h"
#include "mc/world/item/ItemLockHelper.h"
#include "mc/world/item/ItemLockMode.h"
#include "mc/world/item/HandSlot.h"
#include "mc/legacy/ActorRuntimeID.h"
#include <atomic>
#include <chrono>
#include "mc/world/inventory/transaction/ItemUseInventoryTransaction.h"
#ifdef LAMIUM_RESTOCK_TRACE
#include "mc/client/network/LegacyClientNetworkHandler.h"
#include "mc/network/packet/InventorySlotPacket.h"
#include "mc/network/packet/InventoryContentPacket.h"
#include "mc/world/inventory/transaction/ComplexInventoryTransaction.h"
#endif

namespace lamium::inventory::restock {
namespace {
using game::ResponseBarrier;
constexpr char collection[] = "hotbar_items";
std::weak_ptr<HudContainerManagerController> hud;
struct Operation {
    std::weak_ptr<HudContainerManagerController> controller;
    std::uint64_t playerId;
    int dimension;
    RestockSnapshot before;
    std::vector<ItemStack> kinds;
    std::optional<RestockPlan> plan;
    std::optional<game::TransferToken> token;
    bool useFinished = false;
    bool replenishing = false;
    bool useSent = false;
    std::chrono::steady_clock::time_point useDeadline;
};
std::shared_ptr<Operation> pending;
ll::event::ListenerPtr tickListener, exitListener;
// Opt-in diagnostics explain silent early exits without changing use/transfer
// behavior. Only fixed stage labels and numeric state are emitted.
void trace(char const* stage, int value = 0) noexcept {
#ifdef LAMIUM_RESTOCK_TRACE
    try {
        if (!Runtime::instance().preferences().inventory.handRestock) return;
        static std::atomic<unsigned> samples{};
        if (samples.fetch_add(1) >= 128) return;
        Runtime::instance().self().getLogger().info("Restock use trace: {} value={}",stage,value);
    } catch (...) {}
#else
    (void)stage; (void)value;
#endif
}
void cancel() {
    auto old = std::move(pending);
    if (old && old->token) game::cancelTransfer(*old->token);
}
void failure() noexcept {
    cancel();
    static bool reported = false;
    if (!reported) {
        reported = true;
        try { Runtime::instance().self().getLogger().error("Hand Restock stopped after an inventory error"); }
        catch (...) {}
    }
}
LocalPlayer* eligible() {
    auto& runtime = Runtime::instance();
    auto client = ll::service::getClientInstance();
    if (!runtime.enabled() || !runtime.preferences().inventory.handRestock || ui::ownsInput()
        || !client || !client->isInGameInputEnabled()) return nullptr;
    auto* player = client->getLocalPlayer();
    if (!player || !player->isAlive() || player->isCreative() || player->isSpectator()
        || player->isSleeping() || !player->hasRuntimeID() || !player->mInventory) return nullptr;
    auto& inventory = *player->mInventory;
    if (inventory.mSelectedContainerId != ContainerID::Inventory || inventory.mSelected < 0
        || inventory.mSelected >= 9) return nullptr;
    return player;
}
bool owned(HudContainerManagerController& controller, Player& player) {
    auto model = controller.mContainerManagerModel.lock();
    return model && &model->mPlayer == &player && !controller.mContainersClosed
        && controller.hasContainerController(collection) && controller.getContainerSize(collection) == 36;
}
RestockSnapshot snapshot(Operation& op, HudContainerManagerController& controller, LocalPlayer& player) {
    RestockSnapshot result;
    result.context = 1; // Operation identity is separately checked by runtime ID, dimension and weak controller.
    result.selected = player.mInventory->mSelected;
    for (int slot = 0; slot < 36; ++slot) {
        auto const& stack = controller.getItemStack(collection,slot);
        auto const& inventoryStack = player.getInventory().getItem(slot);
        bool empty = stack.isNull() || stack.mCount <= 0;
        bool inventoryEmpty = inventoryStack.isNull() || inventoryStack.mCount <= 0;
        if (empty != inventoryEmpty || (!empty && (stack.mCount != inventoryStack.mCount
            || !stack.matchesItem(inventoryStack)))) throw std::runtime_error("HUD inventory mismatch");
        if (empty) continue;
        int kind = 0;
        for (; kind < static_cast<int>(op.kinds.size()); ++kind)
            if (stack.matchesItem(op.kinds[kind])) break;
        if (kind == static_cast<int>(op.kinds.size())) op.kinds.emplace_back(stack);
        result.slots[slot] = {kind,stack.mCount,ItemLockHelper::getItemLockMode(stack) == ItemLockMode::LockInSlot};
    }
    return result;
}
std::shared_ptr<Operation> beginUse(Player& actor, HandSlot hand) noexcept {
    try {
        if (pending || hand != HandSlot::Mainhand) { trace("nested-or-offhand"); return {}; }
        auto* player = eligible();
        auto controller = hud.lock();
        if (!player) { trace("ineligible"); return {}; }
        if (player != &actor) { trace("non-local-actor"); return {}; }
        if (!controller) { trace("hud-expired"); return {}; }
        if (!owned(*controller,*player)) { trace("hud-not-owned-or-closed"); return {}; }
        auto op = std::make_shared<Operation>();
        op->controller = controller;
        op->playerId = player->getRuntimeID().rawID;
        op->dimension = static_cast<int>(player->getDimensionId());
        op->before = snapshot(*op,*controller,*player);
        auto const& held = op->before.slots[op->before.selected];
        if (held.count != 1 || held.locked) { trace("held-count-or-lock",held.count); return {}; }
        op->token = game::beginTransfer(*controller);
        if (!op->token) { trace("capture-unavailable"); return {}; }
        pending = op;
        trace("capture-started");
        return op;
    } catch (...) { failure(); return {}; }
}
bool current(Operation const& op, LocalPlayer& player, HudContainerManagerController& controller) {
    return op.playerId == player.getRuntimeID().rawID
        && op.dimension == static_cast<int>(player.getDimensionId())
        && op.before.selected == player.mInventory->mSelected
        && hud.lock() == op.controller.lock() && owned(controller,player);
}
void finishUse(std::shared_ptr<Operation> const& op, bool success) noexcept {
    if (!op || pending != op) return;
    try {
        auto* player = eligible();
        auto controller = op->controller.lock();
        if (!success || !player || !controller || !current(*op,*player,*controller)) {
            trace("finish-invalid",success); cancel(); return;
        }
        game::endTransfer(*op->token);
        // The local use callback can return before inventory depletion arrives.
        // Legacy consumption can instead send a complex transaction after this
        // callback. Keep the ownership token while observing that separate path.
        op->useFinished = true;
        op->useDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
        trace("use-finished");
    } catch (...) { failure(); }
}
void tick() noexcept {
    if (!pending) return;
    try {
        auto op = pending;
        auto* player = eligible();
        auto controller = op->controller.lock();
        if (!player || !controller || !current(*op,*player,*controller)) {
            trace("tick-context-changed"); cancel(); return;
        }
        if (!op->useFinished) return; // A synchronous vanilla use is still on the stack.
        auto result = game::transferResult(*op->token);
        if (result == ResponseBarrier::Result::Waiting) return;
        bool legacyUse = !op->replenishing && result == ResponseBarrier::Result::Untracked && op->useSent;
        if (result != ResponseBarrier::Result::Accepted && !legacyUse) {
            Runtime::instance().self().getLogger().info("Hand Restock stopped: inventory response {}",static_cast<int>(result));
            cancel(); return;
        }
        auto now = snapshot(*op,*controller,*player);
        if (legacyUse && std::chrono::steady_clock::now() >= op->useDeadline) { cancel(); return; }
        if (legacyUse && !now.slots[now.selected].empty()) {
            // Only an unchanged inventory may wait for delayed depletion. The
            // deadline cancels observation; elapsed time never proves success.
            if (now.slots != op->before.slots || std::chrono::steady_clock::now() >= op->useDeadline) cancel();
            return;
        }
        if (!op->plan) {
            trace(legacyUse ? "observed-use-count" : "accepted-use-count",now.slots[now.selected].count);
            op->plan = planRestock(op->before,now,true);
            trace(op->plan ? "plan-ready" : "no-depletion-plan");
            if (!op->plan) { cancel(); return; }
        }
        if (op->replenishing) {
            auto const& destination = now.slots[op->plan->destination];
            bool complete = destination == op->plan->expectedSource && now.slots[op->plan->source].empty();
            Runtime::instance().self().getLogger().info("Hand Restock {}",complete ? "acknowledged" : "stopped: inventory changed");
            cancel(); return;
        }
        if (!op->plan->stillValid(now)) { cancel(); return; }
        game::cancelTransfer(*op->token);
        op->token = game::beginTransfer(*controller);
        if (!op->token) { cancel(); return; }
        op->replenishing = true;
        bool success = controller->handleSwap(SlotData{collection,op->plan->source},SlotData{collection,op->plan->destination});
        if (pending != op) return; // Vanilla may synchronously leave the screen/world.
        game::endTransfer(*op->token);
        if (!success) cancel();
    } catch (...) { failure(); }
}
LL_TYPE_INSTANCE_HOOK(CaptureHud, ll::memory::HookPriority::Normal, ClientInstanceScreenModel,
    &ClientInstanceScreenModel::createHudContainerManagerController, std::shared_ptr<HudContainerManagerController>) {
    auto result = origin();
    cancel();
    hud = result;
    trace("hud-captured",bool(result));
    return result;
}
LL_TYPE_INSTANCE_HOOK(Use, ll::memory::HookPriority::Normal, GameMode,
    &GameMode::$useItem, bool, ItemStack& item, HandSlot hand) {
    trace("use-item");
    auto op = beginUse(mPlayer,hand);
    try { bool result = origin(item,hand); finishUse(op,result); return result; }
    catch (...) { if (pending == op) cancel(); throw; }
}
LL_TYPE_INSTANCE_HOOK(UseOn, ll::memory::HookPriority::Normal, GameMode,
    &GameMode::$useItemOn, InteractionResult, ItemStack& item, BlockPos const& pos, uchar face,
    Vec3 const& hit, HandSlot hand, Block const* target, bool first) {
    trace("use-item-on");
    auto op = beginUse(mPlayer,hand);
    try { auto result = origin(item,pos,face,hit,hand,target,first); finishUse(op,result.mSuccess); return result; }
    catch (...) { if (pending == op) cancel(); throw; }
}
LL_TYPE_INSTANCE_HOOK(CompleteUse, ll::memory::HookPriority::Normal, Player,
    &Player::completeUsingItem, void) {
    trace("complete-use");
    auto op = beginUse(*this,HandSlot::Mainhand);
    try { origin(); finishUse(op,true); }
    catch (...) { if (pending == op) cancel(); throw; }
}
LL_TYPE_INSTANCE_HOOK(FocusLost, ll::memory::HookPriority::Normal, MinecraftGame,
    &MinecraftGame::$onAppFocusLost, void) { cancel(); origin(); }
// A legacy use transaction need not appear in the item-stack request batch.
// Observe this boundary without interpreting a send as server acceptance.
LL_TYPE_INSTANCE_HOOK(ComplexSend, ll::memory::HookPriority::Normal, LocalPlayer,
    &LocalPlayer::$sendComplexInventoryTransaction, void,
    std::unique_ptr<ComplexInventoryTransaction> transaction) {
    std::shared_ptr<Operation> observed;
    try { if (eligible() == this) {
        trace("complex-transaction-send-type",transaction ? static_cast<int>(transaction->mType) : -1);
        trace("complex-transaction-during-use",bool(pending && !pending->useFinished));
        auto op = pending;
        if (op && !op->replenishing) {
            bool matches = false;
            if (transaction && transaction->mType == ComplexInventoryTransaction::Type::ItemUseTransaction) {
                auto const& use = static_cast<ItemUseInventoryTransaction const&>(*transaction);
                matches = use.mHand == HandSlot::Mainhand && use.mSlot == op->before.selected
                    && (use.mActionType == ItemUseInventoryTransaction::ActionType::Use
                        || use.mActionType == ItemUseInventoryTransaction::ActionType::Place);
            }
            if (matches && !op->useSent) observed = op;
            else cancel();
        }
    }} catch (...) { failure(); }
    try { origin(std::move(transaction)); }
    catch (...) { if (observed && pending == observed) cancel(); throw; }
    if (observed && pending == observed) observed->useSent = true;
}
LL_TYPE_INSTANCE_HOOK(Drop, ll::memory::HookPriority::Normal, Player,
    &Player::$drop, bool, ItemStack const& item, bool const randomly) {
    try { if (static_cast<Player*>(eligible()) == static_cast<Player*>(this)) cancel(); } catch (...) { failure(); }
    return origin(item,randomly);
}
#ifdef LAMIUM_RESTOCK_TRACE
// Observe the legacy inventory path without treating an arbitrary server update
// as acknowledgement of a use. Do not retain packet data or alter pending work.
void traceInventoryUpdate(char const* stage) noexcept {
    try {
        auto* player = eligible();
        if (!player) return;
        auto const& held = player->getInventory().getItem(player->mInventory->mSelected);
        trace(stage,held.isNull() ? 0 : static_cast<int>(held.mCount));
    } catch (...) {}
}
LL_TYPE_INSTANCE_HOOK(SlotUpdateTrace, ll::memory::HookPriority::Normal, LegacyClientNetworkHandler,
    &LegacyClientNetworkHandler::$handle, void,
    NetworkIdentifier const& source, InventorySlotPacket const& packet) {
    origin(source,packet);
    traceInventoryUpdate("legacy-slot-applied-held-count");
}
LL_TYPE_INSTANCE_HOOK(ContentUpdateTrace, ll::memory::HookPriority::Normal, LegacyClientNetworkHandler,
    &LegacyClientNetworkHandler::$handle, void,
    NetworkIdentifier const& source, InventoryContentPacket const& packet) {
    origin(source,packet);
    traceInventoryUpdate("legacy-content-applied-held-count");
}
#endif
struct Hook { int (*install)(bool); bool (*remove)(bool); bool installed = false; };
Hook hooks[] = {{CaptureHud::hook,CaptureHud::unhook},{Use::hook,Use::unhook},
    {UseOn::hook,UseOn::unhook},{CompleteUse::hook,CompleteUse::unhook},{FocusLost::hook,FocusLost::unhook},
    {ComplexSend::hook,ComplexSend::unhook},{Drop::hook,Drop::unhook},
#ifdef LAMIUM_RESTOCK_TRACE
    {SlotUpdateTrace::hook,SlotUpdateTrace::unhook},{ContentUpdateTrace::hook,ContentUpdateTrace::unhook},
#endif
};
}
void start() {
    try {
        for (auto& hook : hooks) if (!hook.installed) {
            if (hook.install(true) != 0) throw std::runtime_error("Could not install Hand Restock hook");
            hook.installed = true;
        }
        auto& bus = ll::event::EventBus::getInstance();
        if (!tickListener) tickListener = bus.emplaceListener<ll::event::ClientLevelTickEvent>([](auto&) { tick(); });
        if (!exitListener) exitListener = bus.emplaceListener<ll::event::ClientExitLevelEvent>([](auto&) { cancel(); hud.reset(); });
        if (!tickListener || !exitListener) throw std::runtime_error("Could not subscribe Hand Restock lifecycle");
    } catch (...) { stop(); throw; }
}
void stop() {
    cancel(); hud.reset();
    for (auto* listener : {&tickListener,&exitListener}) if (*listener) {
        ll::event::EventBus::getInstance().removeListener(*listener); listener->reset();
    }
    for (auto it = std::rbegin(hooks); it != std::rend(hooks); ++it)
        if (it->installed && it->remove(true)) it->installed = false;
}
}
