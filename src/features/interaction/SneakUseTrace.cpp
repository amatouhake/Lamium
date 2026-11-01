#include "features/interaction/SneakUseTrace.h"
#ifdef LAMIUM_SNEAK_TRACE
#include "app/TraceLog.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/service/TargetedBedrock.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/game/IClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/network/LoopbackPacketSender.h"
#include "mc/network/MinecraftPacketIds.h"
#include "mc/network/packet/InventoryTransactionPacket.h"
#include "mc/network/packet/PlayerAuthInputPacket.h"
#include "mc/world/gamemode/GameMode.h"
#include "mc/world/gamemode/InteractionResult.h"
#include "mc/world/gamemode/SurvivalMode.h"
#include "mc/world/inventory/transaction/ComplexInventoryTransaction.h"
#include "mc/world/inventory/transaction/ItemUseInventoryTransaction.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/block/BlockType.h"
#include <atomic>
#include <chrono>
#include <stdexcept>
#include <string>

// L-293 research: when the client and the server see sneaking around a use
// on a block. Read-only; nothing is changed.
namespace lamium::interaction::sneakUseTrace {
namespace {
using Clock = std::chrono::steady_clock;
auto const epoch = Clock::now();
TraceBudget authBudget, transactionBudget, useBudget;
std::atomic<uint64> lastTick{0};
std::atomic_uint lastBits{~0u};
// Auth inputs right after a use are logged even when the sneak bits stay.
std::atomic_int followUp{0};
long long ms() { return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - epoch).count(); }
LocalPlayer* localPlayer() noexcept {
    try {
        auto client = ll::service::getClientInstance();
        return client ? client->getLocalPlayer() : nullptr;
    } catch (...) { return nullptr; }
}
LL_TYPE_INSTANCE_HOOK(SneakTraceSend, ll::memory::HookPriority::Low, LoopbackPacketSender,
    &LoopbackPacketSender::$send, void, Packet& packet) {
    try {
        auto id = packet.getId();
        auto* player = localPlayer();
        bool flag = player && player->isSneaking();
        if (id == MinecraftPacketIds::PlayerAuthInputPacket) {
            auto& auth = static_cast<PlayerAuthInputPacket&>(packet);
            using A = PlayerAuthInputPacketPayload::InputData;
            unsigned bits = 0, bit = 1;
            for (auto input : {A::Sneaking, A::SneakDown, A::StartSneaking, A::StopSneaking,
                               A::SneakCurrentRaw, A::PerformItemInteraction}) {
                if (auth.mInputData->contains(input)) bits |= bit;
                bit <<= 1;
            }
            uint64 tick = auth.mClientTick->mValue;
            lastTick.store(tick);
            bool changed = lastBits.exchange(bits) != bits;
            bool follow = followUp.load() > 0;
            if (follow) followUp.fetch_sub(1);
            if (changed || follow || (bits & 32u))
                traceLog(authBudget, 600,
                    "L-293 auth t={}ms tick={} sneaking={} sneakDown={} start={} stop={} raw={} itemUse={} itemUseTx={} localFlag={}",
                    ms(), tick, (bits & 1u) != 0, (bits & 2u) != 0, (bits & 4u) != 0, (bits & 8u) != 0,
                    (bits & 16u) != 0, (bits & 32u) != 0, auth.mItemUseTransaction.get() != nullptr, flag);
        } else if (id == MinecraftPacketIds::InventoryTransaction) {
            auto& tx = static_cast<InventoryTransactionPacket&>(packet);
            auto* complex = tx.mTransaction.get();
            int type = complex ? static_cast<int>(complex->mType) : -1;
            int action = -1, face = -1;
            BlockPos pos{};
            if (complex && complex->mType == ComplexInventoryTransaction::Type::ItemUseTransaction) {
                auto& use = static_cast<ItemUseInventoryTransaction&>(*complex);
                action = static_cast<int>(use.mActionType);
                face = use.mFace;
                pos = use.mPos;
                followUp.store(3);
            }
            traceLog(transactionBudget, 300,
                "L-293 transaction t={}ms afterTick={} type={} action={} pos={},{},{} face={} localFlag={}",
                ms(), lastTick.load(), type, action, pos.x, pos.y, pos.z, face, flag);
        }
    } catch (...) {}
    origin(packet);
}
#define ON_HOOK(Name, Mode) \
LL_TYPE_INSTANCE_HOOK(Name, ll::memory::HookPriority::Low, Mode, \
    &Mode::$useItemOn, InteractionResult, ItemStack& item, BlockPos const& pos, uchar face, \
    Vec3 const& hit, HandSlot hand, Block const* target, bool first) { \
    auto result = origin(item, pos, face, hit, hand, target, first); \
    try { \
        auto* local = localPlayer(); \
        bool interactive = target && target->getBlockType().isInteractiveBlock(); \
        traceLog(useBudget, 300, \
            "L-293 " #Mode "-useItemOn t={}ms afterTick={} who={} sneaking={} first={} item={} interactive={} pos={},{},{} face={} success={}", \
            ms(), lastTick.load(), local == &mPlayer ? "local" : "other", mPlayer.isSneaking(), first, \
            item.isNull() ? std::string("empty") : item.getTypeName(), interactive, pos.x, pos.y, pos.z, \
            static_cast<int>(face), static_cast<bool>(result.mSuccess)); \
    } catch (...) {} \
    return result; \
}
ON_HOOK(SneakTraceUseOn, GameMode)
ON_HOOK(SneakTraceSurvivalUseOn, SurvivalMode)
#undef ON_HOOK

struct Hook { int (*install)(bool); bool (*remove)(bool); bool installed = false; };
Hook hooks[] = {
    {SneakTraceSend::hook, SneakTraceSend::unhook},
    {SneakTraceUseOn::hook, SneakTraceUseOn::unhook},
    {SneakTraceSurvivalUseOn::hook, SneakTraceSurvivalUseOn::unhook},
};
}
void start() {
    try {
        for (auto& hook : hooks) if (!hook.installed) {
            if (hook.install(true) != 0) throw std::runtime_error("Could not install sneak use diagnostics");
            hook.installed = true;
        }
        Runtime::instance().self().getLogger().warn("L-293 diagnostics enabled: sneak state around block uses; no changes");
    } catch (...) { stop(); throw; }
}
void stop() {
    for (auto it = std::rbegin(hooks); it != std::rend(hooks); ++it)
        if (it->installed && it->remove(true)) it->installed = false;
}
}
#else
namespace lamium::interaction::sneakUseTrace {
void start() {}
void stop() {}
}
#endif
