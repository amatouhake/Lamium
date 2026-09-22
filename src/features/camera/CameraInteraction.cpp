#include "features/camera/CameraInteraction.h"
#include "features/camera/Zoom.h"
#include "app/Runtime.h"
#include "ll/api/memory/Hook.h"
#include "mc/world/gamemode/GameMode.h"
#include "mc/world/gamemode/InteractionResult.h"
#include <stdexcept>

namespace lamium::camera {
namespace {
bool blocked(Player& player) { return Zoom::instance().blocksLookInteraction(player); }

LL_TYPE_INSTANCE_HOOK(LookStartBreak, ll::memory::HookPriority::Highest, GameMode,
    &GameMode::$startDestroyBlock, bool, BlockPos const& pos, uchar face, bool& destroyed) {
    if (blocked(mPlayer)) { destroyed = false; return false; }
    return origin(pos, face, destroyed);
}
LL_TYPE_INSTANCE_HOOK(LookContinueBreak, ll::memory::HookPriority::Highest, GameMode,
    &GameMode::$continueDestroyBlock, bool, BlockPos const& pos, uchar face, Vec3 const& playerPos, bool& destroyed) {
    if (blocked(mPlayer)) { destroyed = false; return false; }
    return origin(pos, face, playerPos, destroyed);
}
LL_TYPE_INSTANCE_HOOK(LookFinishBreak, ll::memory::HookPriority::Highest, GameMode,
    &GameMode::$destroyBlock, bool, BlockPos const& pos, uchar face) {
    if (blocked(mPlayer)) return false;
    return origin(pos, face);
}
LL_TYPE_INSTANCE_HOOK(LookStartBuild, ll::memory::HookPriority::Highest, GameMode,
    &GameMode::$startBuildBlock, void, BlockPos const& pos, uchar face, HandSlot hand) {
    if (!blocked(mPlayer)) origin(pos, face, hand);
}
LL_TYPE_INSTANCE_HOOK(LookContinueBuild, ll::memory::HookPriority::Highest, GameMode,
    &GameMode::$continueBuildBlock, void, BlockPos const& pos, uchar face, HandSlot hand) {
    if (!blocked(mPlayer)) origin(pos, face, hand);
}
LL_TYPE_INSTANCE_HOOK(LookBuild, ll::memory::HookPriority::Highest, GameMode,
    &GameMode::$buildBlock, bool, BlockPos const& pos, uchar face, HandSlot hand, bool simTick) {
    if (blocked(mPlayer)) return false;
    return origin(pos, face, hand, simTick);
}
LL_TYPE_INSTANCE_HOOK(LookUse, ll::memory::HookPriority::Highest, GameMode,
    &GameMode::$useItem, bool, ItemStack& item, HandSlot hand) {
    if (blocked(mPlayer)) return false;
    return origin(item, hand);
}
LL_TYPE_INSTANCE_HOOK(LookUseAttack, ll::memory::HookPriority::Highest, GameMode,
    &GameMode::$useItemAsAttack, bool, ItemStack& item, Vec3 const& direction, HandSlot hand) {
    if (blocked(mPlayer)) return false;
    return origin(item, direction, hand);
}
LL_TYPE_INSTANCE_HOOK(LookUseOn, ll::memory::HookPriority::Highest, GameMode,
    &GameMode::$useItemOn, InteractionResult, ItemStack& item, BlockPos const& pos, uchar face,
    Vec3 const& hit, HandSlot hand, Block const* target, bool first) {
    if (blocked(mPlayer)) return InteractionResult{false, false};
    return origin(item, pos, face, hit, hand, target, first);
}
LL_TYPE_INSTANCE_HOOK(LookInteract, ll::memory::HookPriority::Highest, GameMode,
    &GameMode::$interact, bool, Actor& entity, Vec3 const& location, HandSlot hand) {
    if (blocked(mPlayer)) return false;
    return origin(entity, location, hand);
}
LL_TYPE_INSTANCE_HOOK(LookAttack, ll::memory::HookPriority::Highest, GameMode,
    &GameMode::$attack, bool, Actor& entity, Vec3 const& hit) {
    if (blocked(mPlayer)) return false;
    return origin(entity, hit);
}
// Stop/release operations must still reach vanilla to clean up existing actions.
struct Hook { int (*install)(bool); bool (*remove)(bool); bool installed = false; };
Hook hooks[] = {
    {LookStartBreak::hook, LookStartBreak::unhook}, {LookContinueBreak::hook, LookContinueBreak::unhook},
    {LookFinishBreak::hook, LookFinishBreak::unhook}, {LookStartBuild::hook, LookStartBuild::unhook},
    {LookContinueBuild::hook, LookContinueBuild::unhook}, {LookBuild::hook, LookBuild::unhook},
    {LookUse::hook, LookUse::unhook}, {LookUseAttack::hook, LookUseAttack::unhook},
    {LookUseOn::hook, LookUseOn::unhook}, {LookInteract::hook, LookInteract::unhook},
    {LookAttack::hook, LookAttack::unhook}
};
}
void startInteractionGuard() {
    for (auto& hook : hooks) {
        if (hook.installed) continue;
        if (hook.install(true) != 0) throw std::runtime_error("Could not install detached-camera interaction guard");
        hook.installed = true;
    }
}
void stopInteractionGuard() {
    for (auto it = std::rbegin(hooks); it != std::rend(hooks); ++it) {
        if (!it->installed) continue;
        if (it->remove(true)) it->installed = false;
        else Runtime::instance().self().getLogger().error("Could not remove a camera interaction hook");
    }
}
}
