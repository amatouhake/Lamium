#include "features/inventory/OffhandUseTrace.h"
#ifdef LAMIUM_OFFHAND_TRACE
#include "app/TraceLog.h"
#include "features/inventory/FakeOffhand.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/service/TargetedBedrock.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/game/ClientInputCallbacks.h"
#include "mc/client/input/BuildActionIntention.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/world/actor/player/Inventory.h"
#include "mc/world/actor/player/PlayerInventory.h"
#include "mc/world/gamemode/GameMode.h"
#include "mc/world/gamemode/SurvivalMode.h"
#include "mc/world/gamemode/InteractionResult.h"
#include "mc/world/item/HandSlot.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/item/Item.h"
#include "mc/world/phys/HitResult.h"
#include <chrono>
#include <optional>
#include <stdexcept>
#include <string>

namespace lamium::inventory::offhandUseTrace {
namespace {
using Clock = std::chrono::steady_clock;
auto const epoch = Clock::now();
TraceBudget buildBudget, handleBudget, useBudget, onBudget, interactBudget, timedBudget;

struct State {
    int selected = -1, sent = -1, useSlot = -1, useContainer = -1;
    int heldCount = 0, useCount = 0, targetSlot = -1, targetCount = 0;
    int heldDuration = -1, targetDuration = -1, heldAnimation = -1, targetAnimation = -1;
    bool chord = false, enabled = false;
    std::string held, usingItem, target;
    bool operator==(State const&) const = default;
};
LocalPlayer* localPlayer() {
    auto client = ll::service::getClientInstance();
    return client ? client->getLocalPlayer() : nullptr;
}
bool local(Player const* player) noexcept {
    try { return player && localPlayer() == player; } catch (...) { return false; }
}
std::string itemName(ItemStack const& item) { return item.isNull() ? "empty" : item.getTypeName(); }
std::optional<State> snapshot() noexcept {
    try {
        if (!Runtime::instance().enabled()) return {};
        auto* player = localPlayer();
        if (!player || !player->mInventory) return {};
        auto const value = Runtime::instance().preferences();
        State state;
        state.selected = player->mInventory->mSelected;
        state.sent = player->mSentSelectedSlot;
        if (state.selected < 0 || state.selected >= 9) return {};
        auto const& held = player->getInventory().getItem(state.selected);
        auto const& inUse = player->mItemInUse.get();
        state.held = itemName(held);
        state.heldCount = held.isNull() ? 0 : static_cast<int>(held.mCount);
        if (!held.isNull() && held.mItem) {
            state.heldDuration = held.mItem->getMaxUseDuration(&held);
            state.heldAnimation = static_cast<int>(held.mItem->mUseAnim);
        }
        state.usingItem = itemName(inUse.mItem);
        state.useCount = inUse.mItem->isNull() ? 0 : static_cast<int>(inUse.mItem->mCount);
        // The slot record is meaningful only while the owned use item exists.
        if (state.useCount) {
            state.useSlot = inUse.mSlot->mSlot;
            state.useContainer = static_cast<int>(inUse.mSlot->mContainerId);
        }
        state.targetSlot = value.inventory.fakeOffhandSlot - 1;
        if (state.targetSlot >= 0 && state.targetSlot < 9) {
            auto const& target = player->getInventory().getItem(state.targetSlot);
            state.target = itemName(target);
            state.targetCount = target.isNull() ? 0 : static_cast<int>(target.mCount);
            if (!target.isNull() && target.mItem) {
                state.targetDuration = target.mItem->getMaxUseDuration(&target);
                state.targetAnimation = static_cast<int>(target.mItem->mUseAnim);
            }
        }
        state.enabled = value.inventory.fakeOffhand;
        state.chord = fakeOffhand::rightChordActive();
        return state;
    } catch (...) { return {}; }
}
void log(TraceBudget& budget, char const* stage, int argument = -1) noexcept {
    auto state = snapshot();
    if (!state) return;
    traceLog(budget, 256,
        "L-95 t={} stage={} arg={} fake={} chord={} selected={} sent={} held={}:{} use={}:{} useSlot={}/{} target={}:{}@{} duration={}/{} animation={}/{}",
        std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - epoch).count(), stage, argument,
        state->enabled, state->chord, state->selected, state->sent, state->held, state->heldCount,
        state->usingItem, state->useCount, state->useContainer, state->useSlot,
        state->target, state->targetCount, state->targetSlot,
        state->heldDuration, state->targetDuration, state->heldAnimation, state->targetAnimation);
}
// Changes only, independently budgeted from the callbacks below. Owned values
// survive between ticks; no player or inventory pointer does.
void logBuild(bool entering, HitResult const& solid) noexcept {
    try {
        static thread_local std::optional<State> before, after;
        auto& previous = entering ? before : after;
        auto state = snapshot();
        if (!state) { previous.reset(); return; }
        if (previous == state) return;
        previous = std::move(state);
        log(buildBudget, entering ? "build-enter" : "build-exit", static_cast<int>(solid.mType));
    } catch (...) {}
}
LL_TYPE_INSTANCE_HOOK(Build, ll::memory::HookPriority::Low, ClientInstance,
    &ClientInstance::_tickBuildAction, void, HitResult const& solid, HitResult const& liquid, bool advance) {
    bool const observe = local(getLocalPlayer());
    if (observe) logBuild(true, solid);
    origin(solid, liquid, advance);
    if (observe) logBuild(false, solid);
}
LL_STATIC_HOOK(HandleBuild, ll::memory::HookPriority::Low, &ClientInputCallbacks::handleBuildAction, bool,
    IClientInstance& client, BuildActionIntention& bai, HitResult const& solid, HitResult const& liquid) {
    static thread_local int previous = -1;
    int action = bai.mAction;
    bool const observe = local(client.getLocalPlayer()) && action != previous;
    if (observe) previous = action;
    if (observe) log(handleBudget, "handle-enter", bai.mAction);
    bool result = origin(client, bai, solid, liquid);
    if (observe) log(handleBudget, "handle-exit", bai.mAction);
    return result;
}

#define USE_HOOK(Name, Mode) \
LL_TYPE_INSTANCE_HOOK(Name, ll::memory::HookPriority::Low, Mode, \
    &Mode::$useItem, bool, ItemStack& item, HandSlot hand) { \
    bool const observe = local(&mPlayer); \
    if (observe) log(useBudget, #Mode "-use-enter", static_cast<int>(hand)); \
    bool result = origin(item, hand); \
    if (observe) log(useBudget, #Mode "-use-exit", result); \
    return result; \
}
USE_HOOK(Use, GameMode)
USE_HOOK(SurvivalUse, SurvivalMode)
#undef USE_HOOK

#define ON_HOOK(Name, Mode) \
LL_TYPE_INSTANCE_HOOK(Name, ll::memory::HookPriority::Low, Mode, \
    &Mode::$useItemOn, InteractionResult, ItemStack& item, BlockPos const& pos, uchar face, \
    Vec3 const& hit, HandSlot hand, Block const* target, bool first) { \
    bool const observe = local(&mPlayer); \
    if (observe) log(onBudget, #Mode "-on-enter", static_cast<int>(hand)); \
    auto result = origin(item, pos, face, hit, hand, target, first); \
    if (observe) log(onBudget, #Mode "-on-exit", static_cast<bool>(result.mSuccess)); \
    return result; \
}
ON_HOOK(UseOn, GameMode)
ON_HOOK(SurvivalUseOn, SurvivalMode)
#undef ON_HOOK

#define INTERACT_HOOK(Name, Mode) \
LL_TYPE_INSTANCE_HOOK(Name, ll::memory::HookPriority::Low, Mode, \
    &Mode::$interact, bool, Actor& actor, Vec3 const& location, HandSlot hand) { \
    bool const observe = local(&mPlayer); \
    if (observe) log(interactBudget, #Mode "-interact-enter", static_cast<int>(hand)); \
    bool result = origin(actor, location, hand); \
    if (observe) log(interactBudget, #Mode "-interact-exit", result); \
    return result; \
}
INTERACT_HOOK(Interact, GameMode)
INTERACT_HOOK(SurvivalInteract, SurvivalMode)
#undef INTERACT_HOOK

LL_TYPE_INSTANCE_HOOK(StartUse, ll::memory::HookPriority::Low, Player,
    &Player::startUsingItem, void, ItemStack const& item, int duration, HandSlot hand) {
    bool const observe = local(this);
    if (observe) log(timedBudget, "start-enter", duration);
    origin(item, duration, hand);
    if (observe) log(timedBudget, "start-exit", duration);
}
#define TIMED_HOOK(Name, Method) \
LL_TYPE_INSTANCE_HOOK(Name, ll::memory::HookPriority::Low, Player, &Player::Method, void) { \
    bool const observe = local(this); \
    if (observe) log(timedBudget, #Method "-enter"); \
    origin(); \
    if (observe) log(timedBudget, #Method "-exit"); \
}
TIMED_HOOK(CompleteUse, completeUsingItem)
TIMED_HOOK(ReleaseUse, releaseUsingItem)
TIMED_HOOK(StopUse, stopUsingItem)
#undef TIMED_HOOK

struct Hook { int (*install)(bool); bool (*remove)(bool); bool installed = false; };
Hook hooks[] = {
    {Build::hook, Build::unhook}, {HandleBuild::hook, HandleBuild::unhook},
    {Use::hook, Use::unhook}, {SurvivalUse::hook, SurvivalUse::unhook},
    {UseOn::hook, UseOn::unhook}, {SurvivalUseOn::hook, SurvivalUseOn::unhook},
    {Interact::hook, Interact::unhook}, {SurvivalInteract::hook, SurvivalInteract::unhook},
    {StartUse::hook, StartUse::unhook}, {CompleteUse::hook, CompleteUse::unhook},
    {ReleaseUse::hook, ReleaseUse::unhook}, {StopUse::hook, StopUse::unhook},
};
}
void start() {
    try {
        for (auto& hook : hooks) if (!hook.installed) {
            if (hook.install(true) != 0) throw std::runtime_error("Could not install offhand use diagnostics");
            hook.installed = true;
        }
        Runtime::instance().self().getLogger().warn("L-95 diagnostics enabled: item-use callbacks and slot state; no use changes");
    } catch (...) { stop(); throw; }
}
void stop() {
    for (auto it = std::rbegin(hooks); it != std::rend(hooks); ++it)
        if (it->installed && it->remove(true)) it->installed = false;
}
}
#else
namespace lamium::inventory::offhandUseTrace {
void start() {}
void stop() {}
}
#endif
