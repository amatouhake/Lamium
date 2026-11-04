#include "features/inventory/game/ConsumptionTrace.h"
#ifdef LAMIUM_CONSUMPTION_TRACE
#include "app/Runtime.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/service/TargetedBedrock.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/world/actor/Actor.h"
#include "mc/world/actor/player/Inventory.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/actor/player/PlayerInventory.h"
#include "mc/world/gamemode/GameMode.h"
#include "mc/world/gamemode/InteractionResult.h"
#include "mc/world/item/HandSlot.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/item/ItemStackBase.h"
#include "mc/world/item/ItemUseMethod.h"
#include <atomic>
#include <format>
#include <stdexcept>
#include <utility>

namespace lamium::inventory::game::consumptionTrace {
namespace {
// Bounded: short manual category tests need a few dozen lines each.
constexpr unsigned maxLines = 512;
std::atomic<unsigned> lines{0};

LocalPlayer* localPlayer() {
    auto client = ll::service::getClientInstance();
    return client ? client->getLocalPlayer() : nullptr;
}
template <class... Args>
void log(std::format_string<Args...> format, Args&&... args) noexcept {
    try {
        if (!Runtime::instance().enabled()) return;
        if (lines.fetch_add(1) >= maxLines) return;
        Runtime::instance().self().getLogger().info(
            "research L-66 consume {}", std::format(format, std::forward<Args>(args)...)
        );
    } catch (...) {}
}
bool isLocal(Player* player) {
    auto* local = localPlayer();
    return local && player == local;
}
int heldSlot(LocalPlayer& player) { return player.mInventory->mSelected; }
int heldCount(LocalPlayer& player) {
    auto const& held = player.getInventory().getItem(heldSlot(player));
    return held.isNull() ? 0 : static_cast<int>(held.mCount);
}
int method(ItemUseMethod value) { return static_cast<int>(value); }
// Counts only; item identity is established by the controlled test setup.
bool sameItem(ItemStack const& before, ItemStack const& after) {
    return !before.isNull() && !after.isNull() && before.matchesItem(after);
}

LL_TYPE_INSTANCE_HOOK(UseItem, ll::memory::HookPriority::Low, GameMode,
    &GameMode::$useItem, bool, ItemStack& item, HandSlot hand) {
    ItemStack before = item;
    int heldBefore = -1;
    auto* player = localPlayer();
    bool local = isLocal(&mPlayer);
    if (local) heldBefore = heldCount(*player);
    bool result = origin(item, hand);
    if (local) log("cb=use-item hand={} item={}->{} itemChanged={} held={}->{} result={}",
        static_cast<int>(hand), static_cast<int>(before.mCount), static_cast<int>(item.mCount),
        !sameItem(before, item), heldBefore, heldCount(*player), result);
    return result;
}
LL_TYPE_INSTANCE_HOOK(UseItemOn, ll::memory::HookPriority::Low, GameMode,
    &GameMode::$useItemOn, InteractionResult, ItemStack& item, BlockPos const& pos, uchar face,
    Vec3 const& hit, HandSlot hand, Block const* target, bool first) {
    ItemStack before = item;
    int heldBefore = -1;
    auto* player = localPlayer();
    bool local = isLocal(&mPlayer);
    if (local) heldBefore = heldCount(*player);
    auto result = origin(item, pos, face, hit, hand, target, first);
    if (local) log("cb=use-item-on hand={} item={}->{} itemChanged={} held={}->{} success={}",
        static_cast<int>(hand), static_cast<int>(before.mCount), static_cast<int>(item.mCount),
        !sameItem(before, item), heldBefore, heldCount(*player), static_cast<bool>(result.mSuccess));
    return result;
}
LL_TYPE_INSTANCE_HOOK(PlayerUseItem, ll::memory::HookPriority::Low, Player,
    &Player::$useItem, void, ItemStackBase& instance, ItemUseMethod itemUseMethod, bool consumeItem) {
    int countBefore = static_cast<int>(instance.mCount);
    int heldBefore = -1;
    auto* player = localPlayer();
    bool local = isLocal(static_cast<Player*>(this));
    if (local) heldBefore = heldCount(*player);
    origin(instance, itemUseMethod, consumeItem);
    if (local) log("cb=player-use-item method={} consumeArg={} item={}->{} held={}->{}",
        method(itemUseMethod), consumeItem, countBefore, static_cast<int>(instance.mCount),
        heldBefore, heldCount(*player));
}
LL_TYPE_INSTANCE_HOOK(CompleteUse, ll::memory::HookPriority::Low, Player,
    &Player::completeUsingItem, void) {
    int heldBefore = -1;
    auto* player = localPlayer();
    bool local = isLocal(static_cast<Player*>(this));
    if (local) heldBefore = heldCount(*player);
    origin();
    if (local) log("cb=complete-using-item held={}->{}", heldBefore, heldCount(*player));
}
LL_TYPE_INSTANCE_HOOK(StartUse, ll::memory::HookPriority::Low, Player,
    &Player::startUsingItem, void, ItemStack const& instance, int duration, HandSlot hand) {
    auto* player = localPlayer();
    bool local = isLocal(static_cast<Player*>(this));
    if (local) log("cb=start-using-item count={} duration={} held={}",
        static_cast<int>(instance.mCount), duration, heldCount(*player));
    origin(instance, duration, hand);
}
LL_TYPE_INSTANCE_HOOK(StopUse, ll::memory::HookPriority::Low, Player,
    &Player::stopUsingItem, void) {
    int heldBefore = -1;
    auto* player = localPlayer();
    bool local = isLocal(static_cast<Player*>(this));
    if (local) heldBefore = heldCount(*player);
    origin();
    if (local) log("cb=stop-using-item held={}->{}", heldBefore, heldCount(*player));
}
LL_TYPE_INSTANCE_HOOK(UseSelected, ll::memory::HookPriority::Low, Player,
    &Player::useSelectedItem, void, ItemUseMethod itemUseMethod, bool consumeItem) {
    int heldBefore = -1;
    auto* player = localPlayer();
    bool local = isLocal(static_cast<Player*>(this));
    if (local) heldBefore = heldCount(*player);
    origin(itemUseMethod, consumeItem);
    if (local) log("cb=use-selected-item method={} consumeArg={} held={}->{}",
        method(itemUseMethod), consumeItem, heldBefore, heldCount(*player));
}
LL_TYPE_INSTANCE_HOOK(HurtAndBreak, ll::memory::HookPriority::Low, ItemStackBase,
    &ItemStackBase::hurtAndBreak, bool, int deltaDamage, Actor* owner) {
    int before = static_cast<int>(mCount);
    bool broke = origin(deltaDamage, owner);
    try {
        auto* player = localPlayer();
        if (player && owner == player)
            log("cb=hurt-and-break delta={} count={}->{} broke={}", deltaDamage, before,
                static_cast<int>(mCount), broke);
    } catch (...) {}
    return broke;
}
struct Hook {
    int (*install)(bool);
    bool (*remove)(bool);
    bool installed = false;
};
Hook hooks[] = {
    {UseItem::hook,      UseItem::unhook     },
    {UseItemOn::hook,    UseItemOn::unhook   },
    {PlayerUseItem::hook, PlayerUseItem::unhook},
    {CompleteUse::hook,  CompleteUse::unhook },
    {StartUse::hook,     StartUse::unhook    },
    {StopUse::hook,      StopUse::unhook     },
    {UseSelected::hook,  UseSelected::unhook },
    {HurtAndBreak::hook, HurtAndBreak::unhook},
};
} // namespace
void start() {
    try {
        for (auto& hook : hooks)
            if (!hook.installed) {
                if (hook.install(true) != 0) throw std::runtime_error("Could not install consumption trace hook");
                hook.installed = true;
            }
        Runtime::instance().self().getLogger().warn(
            "L-66 consumption diagnostics enabled (callbacks, slots, counts only)"
        );
    } catch (...) { stop(); throw; }
}
void stop() {
    for (auto it = std::rbegin(hooks); it != std::rend(hooks); ++it)
        if (it->installed && it->remove(true)) it->installed = false;
}
}
#else
namespace lamium::inventory::game::consumptionTrace {
void start() {}
void stop() {}
}
#endif
