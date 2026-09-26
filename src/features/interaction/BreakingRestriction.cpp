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
#include <format>
#include <mutex>
#include <string>

namespace lamium::interaction::breaking {
namespace {
std::mutex mutex;
std::optional<RestrictionRegion> anchor;
ll::event::ListenerPtr exitListener;
#ifdef LAMIUM_RESEARCH_TRACE
// L-36: record how vanilla drives a held attack across a rejected block.
// Repeated identical calls are collapsed; the first 400 changes are logged.
std::mutex traceMutex;
std::string lastTrace;
unsigned traceLines = 0, traceRepeats = 0;
void traceBreak(char const* phase, Player& player, BlockPos const& pos, int allowed, int result) noexcept {
    try {
        auto client = ll::service::getClientInstance();
        if (!client || client->getLocalPlayer() != &player) return;
        auto line = std::format("{} pos={},{},{} allowed={} result={}", phase, pos.x, pos.y, pos.z, allowed, result);
        std::lock_guard lock(traceMutex);
        if (line == lastTrace) { ++traceRepeats; return; }
        if (traceLines >= 400) return;
        ++traceLines;
        Runtime::instance().self().getLogger().info("research L-36 {} (previous repeated {}x)", line, traceRepeats);
        lastTrace = std::move(line);
        traceRepeats = 0;
    } catch (...) {} // Diagnostics must not replace the vanilla result.
}
#else
void traceBreak(char const*, Player&, BlockPos const&, int, int) noexcept {}
#endif
// Set when a forbidden target aborted the session; the next allowed target
// starts afresh (like a new click) so the server gets a start action again.
bool restartPending = false;
bool gameplayInput() {
    auto client = ll::service::getClientInstance();
    return client && !ui::ownsInput() && gameplayScreen(client->getScreenName());
}
LL_TYPE_INSTANCE_HOOK(StartBreak, ll::memory::HookPriority::Highest, GameMode,
    &GameMode::$startDestroyBlock, bool, BlockPos const& pos, uchar face, bool& destroyed) {
    if (!allows(mPlayer,pos)) { destroyed = false; traceBreak("start", mPlayer, pos, 0, 0); return false; }
    restartPending = false;
    bool result = origin(pos,face,destroyed);
    traceBreak("start", mPlayer, pos, 1, result);
    return result;
}
LL_TYPE_INSTANCE_HOOK(ContinueBreak, ll::memory::HookPriority::Highest, GameMode,
    &GameMode::$continueDestroyBlock, bool, BlockPos const& pos, uchar face, Vec3 const& playerPos, bool& destroyed) {
    // Returning false here makes vanilla stop the breaking session, and a held
    // button never restarts it (L-36). Skip a block outside the region but keep
    // the session alive, so an allowed block reached later continues breaking.
    // A menu or settings screen still ends the session.
    if (!allows(mPlayer,pos)) {
        destroyed = false;
        bool keep = gameplayInput();
        traceBreak("continue", mPlayer, pos, 0, keep);
        // Abort the allowed block's progress through vanilla's own stop, or it
        // keeps cracking while the crosshair rests on the forbidden block.
        if (keep && static_cast<float const&>(mDestroyProgress) > 0.f) {
            stopDestroyBlock(static_cast<BlockPos const&>(mDestroyBlockPos));
            restartPending = true;
        }
        return keep;
    }
    if (restartPending) {
        traceBreak("restart", mPlayer, pos, 1, -1);
        return startDestroyBlock(pos, face, destroyed);
    }
    bool result = origin(pos,face,playerPos,destroyed);
    traceBreak("continue", mPlayer, pos, 1, result);
    return result;
}
LL_TYPE_INSTANCE_HOOK(FinishBreak, ll::memory::HookPriority::Highest, GameMode,
    &GameMode::$destroyBlock, bool, BlockPos const& pos, uchar face) {
    if (!allows(mPlayer,pos)) { traceBreak("destroy", mPlayer, pos, 0, 0); return false; }
    bool result = origin(pos,face);
    traceBreak("destroy", mPlayer, pos, 1, result);
    return result;
}
LL_TYPE_INSTANCE_HOOK(StopBreak, ll::memory::HookPriority::Normal, GameMode,
    &GameMode::$stopDestroyBlock, void, BlockPos const& pos) {
    traceBreak("stop", mPlayer, pos, -1, -1);
    origin(pos);
}
LL_TYPE_INSTANCE_HOOK(ChangeDimension, ll::memory::HookPriority::Normal, LevelRendererPlayer,
    &LevelRendererPlayer::$onWillChangeDimension, void, Player& player) {
    reset();
    origin(player);
}
struct Hook { int (*install)(bool); bool (*remove)(bool); bool installed = false; };
Hook hooks[]{{StartBreak::hook,StartBreak::unhook},{ContinueBreak::hook,ContinueBreak::unhook},
             {FinishBreak::hook,FinishBreak::unhook},{ChangeDimension::hook,ChangeDimension::unhook},
#ifdef LAMIUM_RESEARCH_TRACE
             {StopBreak::hook,StopBreak::unhook},
#endif
};
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
