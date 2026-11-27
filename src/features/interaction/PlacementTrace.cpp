#include "features/interaction/PlacementTrace.h"
#ifdef LAMIUM_PLACEMENT_TRACE
#include "app/Runtime.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/service/TargetedBedrock.h"
#include "mc/client/game/IClientInstance.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/deps/nbt/CompoundTag.h"
#include "mc/network/LoopbackPacketSender.h"
#include "mc/network/MinecraftPacketIds.h"
#include "mc/network/packet/InventoryTransactionPacket.h"
#include "mc/network/packet/PlayerAuthInputPacket.h"
#include "mc/world/gamemode/GameMode.h"
#include "mc/world/gamemode/InteractionResult.h"
#include "mc/world/gamemode/SurvivalMode.h"
#include "mc/world/inventory/transaction/ItemUseInventoryTransaction.h"
#include "mc/world/item/HandSlot.h"
#include "mc/world/item/Item.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/block/BlockType.h"
#ifdef LAMIUM_PLACEMENT_FAKE
#include "features/interaction/PeriodicInput.h"
#include "ll/api/thread/ClientThreadExecutor.h"
#include "mc/world/phys/HitResult.h"
#include <optional>
#endif
#include <atomic>
#include <format>
#include <string>

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
    &Item::$calculatePlacePos, bool, ItemStackBase const& stack, Actor const& actor, uchar& face, BlockPos& pos) {
    auto id = ticket(actor);
    log(id,"calculate.enter",pos,face,-1);
    auto result = origin(stack,actor,face,pos);
    log(id,"calculate.exit",pos,face,result);
    return result;
}
// L-265: what decides a placed block's state. Both sides of useItemOn (the
// local server's too, for the rotation it used) and the packets around a use.
std::atomic<int> useLines{};
void note(std::string const& text) noexcept {
    if (++useLines > 800) return;
    try { Runtime::instance().self().getLogger().info("L-265 {}", text); } catch (...) {}
}
std::string blockText(Block const& block) {
    std::string out = block.getBlockType().mNameInfo->mFullName->getString();
    auto const& tags = block.mSerializationId->mTags;
    if (auto found = tags.find("states"); found != tags.end()) {
        std::string states = found->second.toSnbt(SnbtFormat::Minimize);
        if (states != "{}") out += states;
    }
    return out;
}
BlockPos beside(BlockPos pos, uchar face) {
    switch (face) {
    case 0: --pos.y; break;
    case 1: ++pos.y; break;
    case 2: --pos.z; break;
    case 3: ++pos.z; break;
    case 4: --pos.x; break;
    case 5: ++pos.x; break;
    default: break;
    }
    return pos;
}
bool isLocal(Player const& actor) {
    auto client = ll::service::getClientInstance();
    return client && client->getLocalPlayer() == &actor;
}
// Client thread only.
int followAuth = 0;
std::string lastAuth;
template <class Call>
InteractionResult useTraced(char const* mode, Player& actor, ItemStack const& item, BlockPos const& at, uchar face,
                            Vec3 const& hit, Call&& call) {
    std::string before;
    bool const local = isLocal(actor);
    try {
        auto& region = actor.getDimensionBlockSource();
        Vec2 const rot = actor.getRotation();
        before = std::format("{} {} useItemOn item {} at {},{},{} face {} hit {:.3f},{:.3f},{:.3f} rot pitch {:.1f} yaw {:.1f}; "
                             "target {}; beside {}",
            local ? "client" : "server", mode, item.isNull() ? "none" : item.getTypeName(), at.x, at.y, at.z, face, hit.x, hit.y,
            hit.z, rot.x, rot.y, blockText(region.getBlock(at)), blockText(region.getBlock(beside(at, face))));
        if (local) {
            if (!lastAuth.empty()) note("client auth before use: " + lastAuth);
            followAuth = 2;
        }
    } catch (...) {}
    InteractionResult const result = call();
    try {
        auto& region = actor.getDimensionBlockSource();
        note(std::format("{} -> success {} swing {}; target now {}; beside now {}", before, static_cast<bool>(result.mSuccess),
            static_cast<bool>(result.mSwing), blockText(region.getBlock(at)), blockText(region.getBlock(beside(at, face)))));
    } catch (...) {}
    return result;
}
#define USE_HOOK(Mode) \
LL_TYPE_INSTANCE_HOOK(PlacementTrace##Mode##UseOn, ll::memory::HookPriority::Normal, Mode, \
    &Mode::$useItemOn, InteractionResult, ItemStack& item, BlockPos const& at, uchar face, Vec3 const& hit, \
    HandSlot hand, Block const* target, bool first) { \
    return useTraced(#Mode, mPlayer, item, at, face, hit, [&] { return origin(item, at, face, hit, hand, target, first); }); \
}
USE_HOOK(GameMode)
USE_HOOK(SurvivalMode)
#undef USE_HOOK
#ifdef LAMIUM_PLACEMENT_FAKE
// L-265 experiment: a stair placement is held back, the next auth input says
// the player looks the opposite way (yaw + 180, move vectors turned with it
// so the motion stays the same), and the use is replayed right after it with
// the local player's yaw turned the same for the client's own call.
struct Held { BlockPos pos; uchar face; };
// Every auth input is faked from the hold until the Place transaction leaves
// (the replay's timing against the next auth input is unknown).
std::optional<Held> held;
bool replayQueued = false, replaying = false;
int authsFaked = 0;
float flip(float yaw) { return yaw >= 0 ? yaw - 180.0f : yaw + 180.0f; }
void dropHeld(char const* why) {
    if (held) note(std::string("fake: ") + why);
    held.reset();
    replayQueued = false;
    authsFaked = 0;
}
bool holdsStair(Player& actor, HandSlot hand) noexcept {
    try {
        if (replaying || !isLocal(actor)) return false;
        auto const& item = hand == HandSlot::Offhand ? actor.getOffhandSlot() : actor.getSelectedItem();
        return !item.isNull() && item.getTypeName().ends_with("_stairs");
    } catch (...) { return false; }
}
bool holdBuild(Player& actor, BlockPos const& pos, uchar face, HandSlot hand) noexcept {
    if (!holdsStair(actor, hand)) return false;
    if (!held) {
        held = Held{pos, face};
        replayQueued = false;
        authsFaked = 0;
        note(std::format("fake: held a stair use at {},{},{} face {}", pos.x, pos.y, pos.z, face));
    }
    return true;
}
// Puts the local yaw and the replay flag back whatever happens in between.
struct FlippedYaw {
    Vec2& rot;
    Vec2 was;
    explicit FlippedYaw(Vec2& r) : rot(r), was(r) {
        rot.y = flip(was.y);
        replaying = true;
    }
    ~FlippedYaw() {
        rot = was;
        replaying = false;
    }
};
void replay() {
    ll::thread::ClientThreadExecutor::getDefault().execute([] {
        try {
            auto client = ll::service::getClientInstance();
            auto* player = client ? client->getLocalPlayer() : nullptr;
            if (!player || !held) return dropHeld("no player or nothing held at the replay");
            auto const& hit = client->getLatestHitResult();
            if (hit.mType != HitResultType::Tile || hit.mBlock != held->pos || hit.mFacing != held->face)
                return dropHeld("aim moved before the replay; dropped");
            bool down = false;
            float yaw = 0;
            {
                FlippedYaw const flipped(*player->mBuiltInComponents->mActorRotationComponent->mRot);
                yaw = flipped.rot.y;
                down = periodic::sendUseEdge(*client, true);
                if (down) periodic::sendUseEdge(*client, false);
            }
            note(std::format("fake: replayed (edge sent {}) with client yaw {:.1f}; held still {}", down, yaw, held.has_value()));
            if (!down) dropHeld("the use edge was not sent");
        } catch (...) { dropHeld("replay failed"); }
    });
}
void fakeAuth(PlayerAuthInputPacket& auth) {
    auth.mRot->y = flip(auth.mRot->y);
    auth.mYHeadRot = flip(static_cast<float>(auth.mYHeadRot));
    auth.mInteractRotation->y = flip(auth.mInteractRotation->y);
    auth.mCameraOrientation->x = -auth.mCameraOrientation->x;
    auth.mCameraOrientation->z = -auth.mCameraOrientation->z;
    *auth.mMove = Vec2{-auth.mMove->x, -auth.mMove->z};
    *auth.mRawMoveVector = Vec2{-auth.mRawMoveVector->x, -auth.mRawMoveVector->z};
    *auth.mAnalogMoveVector = Vec2{-auth.mAnalogMoveVector->x, -auth.mAnalogMoveVector->z};
    ++authsFaked;
    followAuth = 3;
}
#define HOLD_HOOKS(Mode) \
LL_TYPE_INSTANCE_HOOK(PlacementFake##Mode##Build, ll::memory::HookPriority::High, Mode, \
    &Mode::$buildBlock, bool, BlockPos const& pos, uchar face, HandSlot hand, bool simTick) { \
    if (holdBuild(mPlayer, pos, face, hand)) return false; \
    return origin(pos, face, hand, simTick); \
} \
LL_TYPE_INSTANCE_HOOK(PlacementFake##Mode##StartBuild, ll::memory::HookPriority::High, Mode, \
    &Mode::$startBuildBlock, void, BlockPos const& pos, uchar face, HandSlot hand) { \
    if (!holdBuild(mPlayer, pos, face, hand)) origin(pos, face, hand); \
}
HOLD_HOOKS(GameMode)
HOLD_HOOKS(SurvivalMode)
#undef HOLD_HOOKS
#endif
using AuthFlag = PlayerAuthInputPacketPayload::InputData;
LL_TYPE_INSTANCE_HOOK(PlacementTraceSend, ll::memory::HookPriority::Normal, LoopbackPacketSender,
    &LoopbackPacketSender::$send, void, Packet& packet) {
    try {
        auto client = ll::service::getClientInstance();
        if (client && packet.mSenderSubId == client->getClientSubId()) {
            if (packet.getId() == MinecraftPacketIds::PlayerAuthInputPacket) {
                auto& auth = static_cast<PlayerAuthInputPacket&>(packet);
#ifdef LAMIUM_PLACEMENT_FAKE
                if (held && authsFaked >= 5) dropHeld("no Place transaction after 5 faked auth inputs; gave up");
                bool const faking = held.has_value();
                if (faking) fakeAuth(auth);
#endif
                lastAuth = std::format("tick {} rot {:.1f},{:.1f} headYaw {:.1f} interact {:.1f},{:.1f} camera {:.3f},{:.3f},{:.3f} "
                                       "pos {:.2f},{:.2f},{:.2f} itemInteraction {} embedded {}",
                    static_cast<unsigned long long>(auth.mClientTick->mValue), auth.mRot->x, auth.mRot->y, static_cast<float>(auth.mYHeadRot),
                    auth.mInteractRotation->x, auth.mInteractRotation->y, auth.mCameraOrientation->x, auth.mCameraOrientation->y,
                    auth.mCameraOrientation->z, auth.mPos->x, auth.mPos->y, auth.mPos->z,
                    auth.mInputData->contains(AuthFlag::PerformItemInteraction), static_cast<bool>(auth.mItemUseTransaction.get()));
#ifdef LAMIUM_PLACEMENT_FAKE
                if (faking) {
                    note(std::format("fake: auth {} sent as {}; move {:.2f},{:.2f}", authsFaked, lastAuth, auth.mMove->x, auth.mMove->z));
                    origin(packet);
                    if (!replayQueued) {
                        replayQueued = true;
                        replay();
                    }
                    return;
                }
#endif
                if (followAuth > 0) {
                    --followAuth;
                    note("client auth after use: " + lastAuth);
                }
            } else if (packet.getId() == MinecraftPacketIds::InventoryTransaction) {
                auto& sent = static_cast<InventoryTransactionPacket&>(packet);
                auto const* complex = sent.mTransaction.get();
                if (complex && complex->mType == ComplexInventoryTransaction::Type::ItemUseTransaction) {
                    auto const& use = static_cast<ItemUseInventoryTransaction const&>(*complex);
                    note(std::format("client sends item use: action {} trigger {} pos {},{},{} face {} click {:.3f},{:.3f},{:.3f} "
                                     "from {:.2f},{:.2f},{:.2f} slot {} targetBlockId {} predicted {}",
                        static_cast<int>(use.mActionType), static_cast<int>(use.mTriggerType), use.mPos->x, use.mPos->y,
                        use.mPos->z, static_cast<int>(use.mFace), use.mClickPos->x, use.mClickPos->y, use.mClickPos->z,
                        use.mFromPos->x, use.mFromPos->y, use.mFromPos->z, static_cast<int>(use.mSlot), static_cast<unsigned>(use.mTargetBlockId),
                        static_cast<int>(use.mClientPredictedResult)));
#ifdef LAMIUM_PLACEMENT_FAKE
                    if (held && use.mActionType == ItemUseInventoryTransaction::ActionType::Place) {
                        note(std::format("fake: Place left after {} faked auth input(s)", authsFaked));
                        held.reset();
                        replayQueued = false;
                        authsFaked = 0;
                    }
#endif
                }
            }
        }
    } catch (...) {}
    origin(packet);
}
bool calculated = false, used = false, usedSurvival = false, sent = false;
#ifdef LAMIUM_PLACEMENT_FAKE
bool holdHooks = false;
bool installHolds() {
    return PlacementFakeGameModeBuild::hook(true) == 0 && PlacementFakeGameModeStartBuild::hook(true) == 0
        && PlacementFakeSurvivalModeBuild::hook(true) == 0 && PlacementFakeSurvivalModeStartBuild::hook(true) == 0;
}
void removeHolds() {
    PlacementFakeSurvivalModeStartBuild::unhook(true);
    PlacementFakeSurvivalModeBuild::unhook(true);
    PlacementFakeGameModeStartBuild::unhook(true);
    PlacementFakeGameModeBuild::unhook(true);
    held.reset();
    replayQueued = replaying = false;
    authsFaked = 0;
}
#endif
}
void start() {
    count = 0;
    useLines = 0;
    followAuth = 0;
    lastAuth.clear();
    calculated = Calculate::hook(true) == 0;
    used = PlacementTraceGameModeUseOn::hook(true) == 0;
    usedSurvival = PlacementTraceSurvivalModeUseOn::hook(true) == 0;
    sent = PlacementTraceSend::hook(true) == 0;
#ifdef LAMIUM_PLACEMENT_FAKE
    holdHooks = installHolds();
    if (!holdHooks) { stop(); throw std::runtime_error("Could not install the placement fake experiment"); }
    Runtime::instance().self().getLogger().warn("L-265 experiment: stair placements are sent looking the opposite way");
#endif
    if (!calculated || !used || !usedSurvival || !sent) { stop(); throw std::runtime_error("Could not install placement diagnostics"); }
    Runtime::instance().self().getLogger().warn("Placement diagnostics enabled: first 200 local calls and 800 L-265 lines only");
}
void stop() {
#ifdef LAMIUM_PLACEMENT_FAKE
    if (holdHooks) { removeHolds(); holdHooks = false; }
#endif
    if (sent && PlacementTraceSend::unhook(true)) sent = false;
    if (usedSurvival && PlacementTraceSurvivalModeUseOn::unhook(true)) usedSurvival = false;
    if (used && PlacementTraceGameModeUseOn::unhook(true)) used = false;
    if (calculated && Calculate::unhook(true)) calculated = false;
}
}
#else
namespace lamium::interaction::placementTrace { void start() {} void stop() {} }
#endif
