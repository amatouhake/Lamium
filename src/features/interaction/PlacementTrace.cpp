#include "features/interaction/PlacementTrace.h"
#ifdef LAMIUM_PLACEMENT_TRACE
#include "app/Runtime.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/service/TargetedBedrock.h"
#include "mc/client/game/IClientInstance.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/world/item/Item.h"
#include "mc/world/level/BlockPos.h"
#include "mc/gameplayhandlers/CoordinatorResult.h"
#include <atomic>

namespace lamium::interaction::placementTrace {
namespace {
std::atomic<unsigned> count{};
unsigned ticket(Actor const& actor) {
    auto client = ll::service::getClientInstance();
    if (!client || client->getLocalPlayer() != &actor) return 0;
    auto current = count.load();
    while (current < 200) if (count.compare_exchange_weak(current,current+1)) return current+1;
    return 0;
}
void log(unsigned id, char const* phase, BlockPos const& pos, unsigned face, int result) noexcept {
    if (!id) return;
    try { Runtime::instance().self().getLogger().info("placement trace {} {} pos={},{},{} face={} result={}",id,phase,pos.x,pos.y,pos.z,face,result); }
    catch (...) {} // Diagnostics must not replace the vanilla result.
}
LL_TYPE_INSTANCE_HOOK(Calculate, ll::memory::HookPriority::Normal, Item,
    &Item::$calculatePlacePos, bool, ItemStackBase& stack, Actor& actor, uchar& face, BlockPos& pos) {
    auto id = ticket(actor);
    log(id,"calculate.enter",pos,face,-1);
    auto result = origin(stack,actor,face,pos);
    log(id,"calculate.exit",pos,face,result);
    return result;
}
LL_TYPE_INSTANCE_HOOK(TryPlace, ll::memory::HookPriority::Normal, Item,
    &Item::_sendTryPlaceBlockEvent, CoordinatorResult, Block const& block, BlockSource const& source,
    Actor const& actor, BlockPos const& pos, uchar face, Vec3 const& click) {
    auto id = ticket(actor);
    log(id,"tryplace.enter",pos,face,-1);
    auto result = origin(block,source,actor,pos,face,click);
    log(id,"tryplace.exit",pos,face,static_cast<int>(result));
    return result;
}
bool calculated = false, tried = false;
}
void start() {
    count = 0;
    calculated = Calculate::hook(true) == 0;
    tried = TryPlace::hook(true) == 0;
    if (!calculated || !tried) { stop(); throw std::runtime_error("Could not install placement diagnostics"); }
    Runtime::instance().self().getLogger().warn("Placement diagnostics enabled: first 200 local calls only");
}
void stop() {
    if (tried && TryPlace::unhook(true)) tried = false;
    if (calculated && Calculate::unhook(true)) calculated = false;
}
}
#else
namespace lamium::interaction::placementTrace { void start() {} void stop() {} }
#endif
