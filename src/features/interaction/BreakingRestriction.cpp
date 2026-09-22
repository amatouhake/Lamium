#include "features/interaction/BreakingRestriction.h"
#include "app/Runtime.h"
#include "ui/SettingsScreen.h"
#include "input/Actions.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/service/TargetedBedrock.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/client/ClientExitLevelEvent.h"
#include "mc/client/game/IClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/client/renderer/game/LevelRendererPlayer.h"
#include "mc/world/gamemode/GameMode.h"
#include "mc/world/phys/HitResult.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/block/Block.h"
#include <mutex>

namespace lamium::interaction::breaking {
namespace {
std::mutex mutex;
std::optional<RestrictionRegion> anchor;
ll::event::ListenerPtr exitListener;
LL_TYPE_INSTANCE_HOOK(StartBreak, ll::memory::HookPriority::Highest, GameMode,
    &GameMode::$startDestroyBlock, bool, BlockPos const& pos, uchar face, bool& destroyed) {
    if (!allows(mPlayer,pos)) { destroyed = false; return false; }
    return origin(pos,face,destroyed);
}
LL_TYPE_INSTANCE_HOOK(ContinueBreak, ll::memory::HookPriority::Highest, GameMode,
    &GameMode::$continueDestroyBlock, bool, BlockPos const& pos, uchar face, Vec3 const& playerPos, bool& destroyed) {
    if (!allows(mPlayer,pos)) { destroyed = false; return false; }
    return origin(pos,face,playerPos,destroyed);
}
LL_TYPE_INSTANCE_HOOK(FinishBreak, ll::memory::HookPriority::Highest, GameMode,
    &GameMode::$destroyBlock, bool, BlockPos const& pos, uchar face) {
    if (!allows(mPlayer,pos)) return false;
    return origin(pos,face);
}
LL_TYPE_INSTANCE_HOOK(ChangeDimension, ll::memory::HookPriority::Normal, LevelRendererPlayer,
    &LevelRendererPlayer::$onWillChangeDimension, void, Player& player) {
    reset();
    origin(player);
}
struct Hook { int (*install)(bool); bool (*remove)(bool); bool installed = false; };
Hook hooks[]{{StartBreak::hook,StartBreak::unhook},{ContinueBreak::hook,ContinueBreak::unhook},
             {FinishBreak::hook,FinishBreak::unhook},{ChangeDimension::hook,ChangeDimension::unhook}};
}
void reset() { std::lock_guard lock(mutex); anchor.reset(); }
std::optional<RestrictionRegion> region() { std::lock_guard lock(mutex); return anchor; }
void capture(IClientInstance& client) {
    auto* player = client.getLocalPlayer();
    auto const& hit = client.getLatestHitResult();
    if (!player || hit.mType != HitResultType::Tile || hit.mFacing > 5) return;
    auto& source = player->getDimensionBlockSource();
    if (!source.getChunkAt(hit.mBlock) || source.getBlock(hit.mBlock).isAir()) return;
    // Bedrock face IDs: down/up, north/south, west/east.
    Axis axis = hit.mFacing < 2 ? Axis::Y : hit.mFacing < 4 ? Axis::Z : Axis::X;
    auto mode = Runtime::instance().preferences().interaction.breakingMode;
    std::lock_guard lock(mutex);
    anchor = RestrictionRegion{mode,{hit.mBlock.x,hit.mBlock.y,hit.mBlock.z},axis};
}
bool allows(Player& player, BlockPos const& pos) {
    auto& runtime = Runtime::instance();
    if (!runtime.enabled() || !runtime.preferences().interaction.breaking) return true;
    auto client = ll::service::getClientInstance();
    if (!client || client->getLocalPlayer() != &player) return true;
    if (ui::ownsInput() || !gameplayScreen(client->getScreenName())) return false;
    auto current = region();
    return current && current->contains({pos.x,pos.y,pos.z});
}
void start() {
    try {
        for (auto& hook : hooks) if (!hook.installed) {
            if (hook.install(true) != 0) throw std::runtime_error("Could not install breaking restriction hook");
            hook.installed = true;
        }
        if (!exitListener) exitListener = ll::event::EventBus::getInstance().emplaceListener<ll::event::ClientExitLevelEvent>([](auto&) { reset(); });
    } catch (...) { stop(); throw; }
}
void stop() {
    reset();
    if (exitListener) { ll::event::EventBus::getInstance().removeListener(exitListener); exitListener.reset(); }
    for (auto& hook : hooks) if (hook.installed && hook.remove(true)) hook.installed = false;
}
}
