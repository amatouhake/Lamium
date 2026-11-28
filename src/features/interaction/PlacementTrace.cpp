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
#include "mc/world/phys/HitResult.h"
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
using AuthFlag = PlayerAuthInputPacketPayload::InputData;
LL_TYPE_INSTANCE_HOOK(PlacementTraceSend, ll::memory::HookPriority::Normal, LoopbackPacketSender,
    &LoopbackPacketSender::$send, void, Packet& packet) {
    try {
        auto client = ll::service::getClientInstance();
        if (client && packet.mSenderSubId == client->getClientSubId()) {
            if (packet.getId() == MinecraftPacketIds::PlayerAuthInputPacket) {
                auto& auth = static_cast<PlayerAuthInputPacket&>(packet);
                lastAuth = std::format("tick {} rot {:.1f},{:.1f} headYaw {:.1f} interact {:.1f},{:.1f} camera {:.3f},{:.3f},{:.3f} "
                                       "pos {:.2f},{:.2f},{:.2f} itemInteraction {} embedded {}",
                    static_cast<unsigned long long>(auth.mClientTick->mValue), auth.mRot->x, auth.mRot->y, static_cast<float>(auth.mYHeadRot),
                    auth.mInteractRotation->x, auth.mInteractRotation->y, auth.mCameraOrientation->x, auth.mCameraOrientation->y,
                    auth.mCameraOrientation->z, auth.mPos->x, auth.mPos->y, auth.mPos->z,
                    auth.mInputData->contains(AuthFlag::PerformItemInteraction), static_cast<bool>(auth.mItemUseTransaction.get()));
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
                }
            }
        }
    } catch (...) {}
    origin(packet);
}
bool calculated = false, used = false, usedSurvival = false, sent = false, usedItem = false;
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
    if (!calculated || !used || !usedSurvival || !sent || !usedItem) { stop(); throw std::runtime_error("Could not install placement diagnostics"); }
    Runtime::instance().self().getLogger().warn("Placement diagnostics enabled: first 200 local calls and 800 L-265 lines only");
}
void stop() {
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
