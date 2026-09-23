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
    bool replenishing = false;
};
std::shared_ptr<Operation> pending;
ll::event::ListenerPtr tickListener, exitListener;
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
        if (pending || hand != HandSlot::Mainhand) return {};
        auto* player = eligible();
        auto controller = hud.lock();
        if (!player || player != &actor || !controller || !owned(*controller,*player)) return {};
        auto op = std::make_shared<Operation>();
        op->controller = controller;
        op->playerId = player->getRuntimeID().rawID;
        op->dimension = static_cast<int>(player->getDimensionId());
        op->before = snapshot(*op,*controller,*player);
        auto const& held = op->before.slots[op->before.selected];
        if (held.count != 1 || held.locked) return {};
        op->token = game::beginTransfer(*controller);
        if (!op->token) return {};
        pending = op;
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
        if (!success || !player || !controller || !current(*op,*player,*controller)) { cancel(); return; }
        game::endTransfer(*op->token);
        op->plan = planRestock(op->before,snapshot(*op,*controller,*player),true);
        if (!op->plan) cancel();
    } catch (...) { failure(); }
}
void tick() noexcept {
    if (!pending) return;
    try {
        auto op = pending;
        auto* player = eligible();
        auto controller = op->controller.lock();
        if (!player || !controller || !current(*op,*player,*controller)) { cancel(); return; }
        if (!op->plan) return; // A synchronous vanilla use is still on the stack.
        auto result = game::transferResult(*op->token);
        if (result == ResponseBarrier::Result::Waiting) return;
        if (result != ResponseBarrier::Result::Accepted) {
            Runtime::instance().self().getLogger().info("Hand Restock stopped: inventory response {}",static_cast<int>(result));
            cancel(); return;
        }
        auto now = snapshot(*op,*controller,*player);
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
    return result;
}
LL_TYPE_INSTANCE_HOOK(Use, ll::memory::HookPriority::Normal, GameMode,
    &GameMode::$useItem, bool, ItemStack& item, HandSlot hand) {
    auto op = beginUse(mPlayer,hand);
    try { bool result = origin(item,hand); finishUse(op,result); return result; }
    catch (...) { if (pending == op) cancel(); throw; }
}
LL_TYPE_INSTANCE_HOOK(UseOn, ll::memory::HookPriority::Normal, GameMode,
    &GameMode::$useItemOn, InteractionResult, ItemStack& item, BlockPos const& pos, uchar face,
    Vec3 const& hit, HandSlot hand, Block const* target, bool first) {
    auto op = beginUse(mPlayer,hand);
    try { auto result = origin(item,pos,face,hit,hand,target,first); finishUse(op,result.mSuccess); return result; }
    catch (...) { if (pending == op) cancel(); throw; }
}
LL_TYPE_INSTANCE_HOOK(CompleteUse, ll::memory::HookPriority::Normal, Player,
    &Player::completeUsingItem, void) {
    auto op = beginUse(*this,HandSlot::Mainhand);
    try { origin(); finishUse(op,true); }
    catch (...) { if (pending == op) cancel(); throw; }
}
LL_TYPE_INSTANCE_HOOK(FocusLost, ll::memory::HookPriority::Normal, MinecraftGame,
    &MinecraftGame::$onAppFocusLost, void) { cancel(); origin(); }
struct Hook { int (*install)(bool); bool (*remove)(bool); bool installed = false; };
Hook hooks[] = {{CaptureHud::hook,CaptureHud::unhook},{Use::hook,Use::unhook},
    {UseOn::hook,UseOn::unhook},{CompleteUse::hook,CompleteUse::unhook},{FocusLost::hook,FocusLost::unhook}};
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
