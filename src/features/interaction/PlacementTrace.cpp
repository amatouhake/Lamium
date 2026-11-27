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
#include "ui/Toast.h"
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
    if (++useLines > 4000) return;
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
            // What the block's own placement rule says for this use, read
            // before the call: the cell is the target when it can be built
            // over, else the one beside it.
            if (Block const* held = item.mBlock) {
                Block const& there = region.getBlock(at);
                BlockPos const cell = there.getBlockType().canBeBuiltOver(there, region, at) ? at : beside(at, face);
                Vec3 const relative{hit.x - static_cast<float>(at.x), hit.y - static_cast<float>(at.y), hit.z - static_cast<float>(at.z)};
                Block const& byRelative = held->getBlockType().getPlacementBlock(actor, cell, face, relative, item.getAuxValue());
                Block const& byAbsolute = held->getBlockType().getPlacementBlock(actor, cell, face, hit, item.getAuxValue());
                note(std::format("predict: cell {},{},{} by relative click {}; by absolute click {}", cell.x, cell.y, cell.z,
                                 blockText(byRelative), blockText(byAbsolute)));
            }
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
LL_TYPE_INSTANCE_HOOK(PlacementTraceUseItem, ll::memory::HookPriority::Normal, GameMode, &GameMode::$useItem, bool,
                      ItemStack& item, HandSlot hand) {
    if (isLocal(mPlayer)) try {
        auto client = ll::service::getClientInstance();
        auto const& hit = client->getLatestHitResult();
        note(std::format("client useItem item {} hand {} hit type {} block {},{},{} indirect {}", item.isNull() ? "none" : item.getTypeName(),
                         static_cast<int>(hand), static_cast<int>(hit.mType), hit.mBlock.x, hit.mBlock.y, hit.mBlock.z, hit.mIndirectHit));
    } catch (...) {}
    return origin(item, hand);
}
#ifdef LAMIUM_PLACEMENT_FAKE
// L-265 experiment, fourth build: while use is held with stairs, faked
// placements follow one another at an interval that changes with each new
// press (1, 0.5, 0.25 s, then as fast as vanilla repeats), shown in a toast.
// Oak stairs turn the move input with the look ("turned"); other stairs
// leave it ("untouched"). Inside the interval nothing is placed. Each fake:
// the auth inputs until its Place transaction say yaw + 180, and the stair
// goes into the cell vanilla would fill, clicked from another solid
// neighbor at a chosen click (side faces alternate upper and lower half),
// with the level's hit result set for the buildBlock call.
struct Held { BlockPos aimed; uchar aimedFace; BlockPos pos; uchar face; Vec3 click; bool turnMove; };
std::optional<Held> held;
bool replayQueued = false, replaying = false, pressing = false;
int authsFaked = 0;
unsigned upperCount = 0, pressCount = 0;
double interval = 1.0;
std::chrono::steady_clock::time_point lastFake{};
float flip(float yaw) { return yaw >= 0 ? yaw - 180.0f : yaw + 180.0f; }
void dropHeld(char const* why) {
    if (held) note(std::string("fake: ") + why);
    held.reset();
    replayQueued = false;
    authsFaked = 0;
}
std::string stairIn(Player& actor, HandSlot hand) noexcept {
    try {
        if (replaying || !isLocal(actor)) return {};
        auto const& item = hand == HandSlot::Offhand ? actor.getOffhandSlot() : actor.getSelectedItem();
        if (item.isNull()) return {};
        std::string name = item.getTypeName();
        return name.ends_with("_stairs") ? name : std::string{};
    } catch (...) { return {}; }
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
Held plan(Player& actor, BlockPos const& aimed, uchar aimedFace, bool turnMove) {
    BlockPos const cell = beside(aimed, aimedFace);
    bool const upper = (upperCount++ % 2) == 0;
    auto& region = actor.getDimensionBlockSource();
    for (uchar dir : {uchar{2}, uchar{3}, uchar{4}, uchar{5}, uchar{0}, uchar{1}}) {
        BlockPos const from = beside(cell, dir);
        if (from == aimed) continue;
        Block const& block = region.getBlock(from);
        if (block.getBlockType().canBeBuiltOver(block, region, from)) continue;
        uchar const face = opposite(dir);
        return {aimed, aimedFace, from, face, clickOn(face, upper), turnMove};
    }
    return {aimed, aimedFace, aimed, aimedFace, clickOn(aimedFace, upper), turnMove};
}
Held plan(Player& actor, BlockPos const& aimed, uchar aimedFace, bool turnMove);
// A new press picks the next interval.
void startPress() {
    static constexpr double intervals[] = {1.0, 0.5, 0.25, 0.0};
    interval = intervals[pressCount++ % 4];
    pressing = true;
    lastFake = {};
    std::string const text = interval > 0 ? std::format("L-265: {} s", interval) : std::string("L-265: vanilla repeat");
    note("fake: new press, interval " + text);
    try { ui::showMessageToast(text, 1.5); } catch (...) {}
}
bool holdBuild(Player& actor, BlockPos const& pos, uchar face, HandSlot hand) noexcept {
    try {
        std::string const stair = stairIn(actor, hand);
        if (stair.empty()) return false;
        if (held) return true;
        auto const now = std::chrono::steady_clock::now();
        if (now - lastFake < std::chrono::duration<double>(interval)) return true;
        lastFake = now;
        bool const turnMove = stair == "minecraft:oak_stairs";
        held = plan(actor, pos, face, turnMove);
        replayQueued = false;
        authsFaked = 0;
        note(std::format("fake: held a stair use aimed at {},{},{} face {}; placing from {},{},{} face {} click {:.2f},{:.2f},{:.2f}; "
                         "interval {} s, move {}",
                         pos.x, pos.y, pos.z, face, held->pos.x, held->pos.y, held->pos.z, held->face, held->click.x, held->click.y,
                         held->click.z, interval, turnMove ? "turned" : "untouched"));
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
void swapFlags(PlayerAuthInputPacket& auth, AuthInput a, AuthInput b) {
    auto& bits = auth.mInputData->mContainer;
    bool const hasA = bits.test(static_cast<size_t>(a)), hasB = bits.test(static_cast<size_t>(b));
    bits.set(static_cast<size_t>(a), hasB);
    bits.set(static_cast<size_t>(b), hasA);
}
void fakeAuth(PlayerAuthInputPacket& auth) {
    auth.mRot->y = flip(auth.mRot->y);
    auth.mYHeadRot = flip(static_cast<float>(auth.mYHeadRot));
    auth.mInteractRotation->y = flip(auth.mInteractRotation->y);
    auth.mCameraOrientation->x = -auth.mCameraOrientation->x;
    auth.mCameraOrientation->z = -auth.mCameraOrientation->z;
    if (held->turnMove) {
        *auth.mMove = Vec2{-auth.mMove->x, -auth.mMove->z};
        *auth.mRawMoveVector = Vec2{-auth.mRawMoveVector->x, -auth.mRawMoveVector->z};
        *auth.mAnalogMoveVector = Vec2{-auth.mAnalogMoveVector->x, -auth.mAnalogMoveVector->z};
        swapFlags(auth, AuthInput::Up, AuthInput::Down);
        swapFlags(auth, AuthInput::Left, AuthInput::Right);
        swapFlags(auth, AuthInput::UpLeft, AuthInput::DownRight);
        swapFlags(auth, AuthInput::UpRight, AuthInput::DownLeft);
    }
    ++authsFaked;
    followAuth = 20;
}
#define HOLD_HOOKS(Mode) \
LL_TYPE_INSTANCE_HOOK(PlacementFake##Mode##Build, ll::memory::HookPriority::High, Mode, \
    &Mode::$buildBlock, bool, BlockPos const& pos, uchar face, HandSlot hand, bool simTick) { \
    if (holdBuild(mPlayer, pos, face, hand)) return false; \
    return origin(pos, face, hand, simTick); \
} \
LL_TYPE_INSTANCE_HOOK(PlacementFake##Mode##StartBuild, ll::memory::HookPriority::High, Mode, \
    &Mode::$startBuildBlock, void, BlockPos const& pos, uchar face, HandSlot hand) { \
    if (!stairIn(mPlayer, hand).empty()) startPress(); \
    if (!holdBuild(mPlayer, pos, face, hand)) origin(pos, face, hand); \
}
HOLD_HOOKS(GameMode)
HOLD_HOOKS(SurvivalMode)
#undef HOLD_HOOKS
LL_TYPE_INSTANCE_HOOK(PlacementFakeStopBuild, ll::memory::HookPriority::High, GameMode, &GameMode::$stopBuildBlock, void) {
    if (pressing && isLocal(mPlayer)) {
        pressing = false;
        note("fake: press ended");
    }
    origin();
}
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
                    note(std::format("client auth after use: tick {} pos {:.3f},{:.3f} yaw {:.1f} move {:.2f},{:.2f}",
                                     static_cast<unsigned long long>(auth.mClientTick->mValue), auth.mPos->x, auth.mPos->z, auth.mRot->y,
                                     auth.mMove->x, auth.mMove->z));
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
bool calculated = false, used = false, usedSurvival = false, sent = false, usedItem = false;
#ifdef LAMIUM_PLACEMENT_FAKE
bool holdHooks = false;
bool installHolds() {
    return PlacementFakeGameModeBuild::hook(true) == 0 && PlacementFakeGameModeStartBuild::hook(true) == 0
        && PlacementFakeSurvivalModeBuild::hook(true) == 0 && PlacementFakeSurvivalModeStartBuild::hook(true) == 0
        && PlacementFakeStopBuild::hook(true) == 0;
}
void removeHolds() {
    PlacementFakeStopBuild::unhook(true);
    PlacementFakeSurvivalModeStartBuild::unhook(true);
    PlacementFakeSurvivalModeBuild::unhook(true);
    PlacementFakeGameModeStartBuild::unhook(true);
    PlacementFakeGameModeBuild::unhook(true);
    held.reset();
    replayQueued = replaying = pressing = false;
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
    usedItem = PlacementTraceUseItem::hook(true) == 0;
#ifdef LAMIUM_PLACEMENT_FAKE
    holdHooks = installHolds();
    if (!holdHooks) { stop(); throw std::runtime_error("Could not install the placement fake experiment"); }
    Runtime::instance().self().getLogger().warn("L-265 experiment: stair placements are sent looking the opposite way");
#endif
    if (!calculated || !used || !usedSurvival || !sent || !usedItem) { stop(); throw std::runtime_error("Could not install placement diagnostics"); }
    Runtime::instance().self().getLogger().warn("Placement diagnostics enabled: first 200 local calls and 800 L-265 lines only");
}
void stop() {
#ifdef LAMIUM_PLACEMENT_FAKE
    if (holdHooks) { removeHolds(); holdHooks = false; }
#endif
    if (usedItem && PlacementTraceUseItem::unhook(true)) usedItem = false;
    if (sent && PlacementTraceSend::unhook(true)) sent = false;
    if (usedSurvival && PlacementTraceSurvivalModeUseOn::unhook(true)) usedSurvival = false;
    if (used && PlacementTraceGameModeUseOn::unhook(true)) used = false;
    if (calculated && Calculate::unhook(true)) calculated = false;
}
}
#else
namespace lamium::interaction::placementTrace { void start() {} void stop() {} }
#endif
