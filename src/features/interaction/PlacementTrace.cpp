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
#include "mc/world/level/Level.h"
#include <algorithm>
#include <chrono>
#include <cmath>
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
            followAuth = std::max(followAuth, 2);
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
// L-265 experiment, third build: a stair placement is held, the auth inputs
// until its Place transaction leaves say yaw + 180 (move input untouched),
// and the stair is placed into the cell vanilla would fill, but clicked from
// another solid neighbor when there is one, at a click position Lamium
// chooses (side faces alternate upper and lower half). The level's hit result
// says that click for the buildBlock call; the sent click is compared with it.
// At most one fake a second; a stair used sooner is placed by vanilla.
struct Held { BlockPos aimed; uchar aimedFace; BlockPos pos; uchar face; Vec3 click; };
std::optional<Held> held;
bool replayQueued = false, replaying = false;
int authsFaked = 0;
unsigned upperCount = 0;
std::chrono::steady_clock::time_point lastFake{};
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
uchar opposite(uchar face) { return static_cast<uchar>(face ^ 1); }
// The point on `face` of a block, relative to the block; side faces at the
// upper or lower half.
Vec3 clickOn(uchar face, bool upper) {
    float const y = upper ? 0.8f : 0.2f;
    switch (face) {
    case 0: return {0.5f, 0.0f, 0.5f};
    case 1: return {0.5f, 1.0f, 0.5f};
    case 2: return {0.5f, y, 0.0f};
    case 3: return {0.5f, y, 1.0f};
    case 4: return {0.0f, y, 0.5f};
    default: return {1.0f, y, 0.5f};
    }
}
Held plan(Player& actor, BlockPos const& aimed, uchar aimedFace) {
    BlockPos const cell = beside(aimed, aimedFace);
    bool const upper = (upperCount++ % 2) == 0;
    auto& region = actor.getDimensionBlockSource();
    for (uchar dir : {uchar{2}, uchar{3}, uchar{4}, uchar{5}, uchar{0}, uchar{1}}) {
        BlockPos const from = beside(cell, dir);
        if (from == aimed) continue;
        Block const& block = region.getBlock(from);
        if (block.getBlockType().canBeBuiltOver(block, region, from)) continue;
        uchar const face = opposite(dir);
        return {aimed, aimedFace, from, face, clickOn(face, upper)};
    }
    return {aimed, aimedFace, aimed, aimedFace, clickOn(aimedFace, upper)};
}
bool holdBuild(Player& actor, BlockPos const& pos, uchar face, HandSlot hand) noexcept {
    try {
        if (!holdsStair(actor, hand)) return false;
        if (held) return true;
        auto const now = std::chrono::steady_clock::now();
        if (now - lastFake < std::chrono::seconds(1)) return false;
        lastFake = now;
        held = plan(actor, pos, face);
        replayQueued = false;
        authsFaked = 0;
        note(std::format("fake: held a stair use aimed at {},{},{} face {}; placing from {},{},{} face {} click {:.2f},{:.2f},{:.2f}{}",
                         pos.x, pos.y, pos.z, face, held->pos.x, held->pos.y, held->pos.z, held->face, held->click.x, held->click.y,
                         held->click.z, held->pos == pos ? " (no other neighbor)" : ""));
        return true;
    } catch (...) { return false; }
}
// Puts the local yaw, the level's hit result and the replay flag back whatever happens in between.
struct AsPlanned {
    Vec2& rot;
    Vec2 was;
    HitResult& hit;
    HitResult hitWas;
    AsPlanned(Vec2& r, HitResult& h, Held const& at) : rot(r), was(r), hit(h), hitWas(h) {
        rot.y = flip(was.y);
        hit.mType = HitResultType::Tile;
        hit.mBlock = at.pos;
        hit.mFacing = at.face;
        hit.mPos = Vec3{static_cast<float>(at.pos.x) + at.click.x, static_cast<float>(at.pos.y) + at.click.y,
                        static_cast<float>(at.pos.z) + at.click.z};
        replaying = true;
    }
    ~AsPlanned() {
        rot = was;
        hit = hitWas;
        replaying = false;
    }
};
void replay() {
    ll::thread::ClientThreadExecutor::getDefault().execute([] {
        try {
            auto client = ll::service::getClientInstance();
            auto* player = client ? client->getLocalPlayer() : nullptr;
            if (!player || !held) return dropHeld("no player or nothing held at the placement");
            GameMode* mode = player->mGameMode.get();
            if (!mode) return dropHeld("no game mode");
            Held const at = *held;
            bool placed = false;
            {
                AsPlanned const planned(*player->mBuiltInComponents->mActorRotationComponent->mRot, player->getLevel().getHitResult(), at);
                placed = mode->buildBlock(at.pos, at.face, HandSlot::Mainhand, false);
            }
            note(std::format("fake: buildBlock({},{},{} face {}) returned {}; held still {}", at.pos.x, at.pos.y, at.pos.z, at.face,
                             placed, held.has_value()));
            if (held) dropHeld("buildBlock sent no Place transaction");
        } catch (...) { dropHeld("placement failed"); }
    });
}
using AuthInput = PlayerAuthInputPacketPayload::InputData;
void fakeAuth(PlayerAuthInputPacket& auth) {
    auth.mRot->y = flip(auth.mRot->y);
    auth.mYHeadRot = flip(static_cast<float>(auth.mYHeadRot));
    auth.mInteractRotation->y = flip(auth.mInteractRotation->y);
    auth.mCameraOrientation->x = -auth.mCameraOrientation->x;
    auth.mCameraOrientation->z = -auth.mCameraOrientation->z;
    ++authsFaked;
    followAuth = 6;
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
                    note(std::format("fake: auth {} sent as {}", authsFaked, lastAuth));
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
                        Vec3 const sentClick = *use.mClickPos;
                        bool const off = std::abs(sentClick.x - held->click.x) > 0.01f || std::abs(sentClick.y - held->click.y) > 0.01f
                                      || std::abs(sentClick.z - held->click.z) > 0.01f;
                        if (off) *const_cast<ItemUseInventoryTransaction&>(use).mClickPos = held->click;
                        note(std::format("fake: Place left after {} faked auth input(s); click sent {:.3f},{:.3f},{:.3f}, wanted {:.2f},{:.2f},{:.2f}{}",
                                         authsFaked, sentClick.x, sentClick.y, sentClick.z, held->click.x, held->click.y, held->click.z,
                                         off ? "; patched to the wanted click" : ""));
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
