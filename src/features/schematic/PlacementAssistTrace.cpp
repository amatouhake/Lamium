#include "features/schematic/PlacementAssistTrace.h"
#ifdef LAMIUM_ASSIST_TRACE
#include "app/Runtime.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/service/TargetedBedrock.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/game/IClientInstance.h"
#include "mc/client/network/ClientNetworkHandler.h"
#include "mc/client/network/LegacyClientNetworkHandler.h"
#include "mc/network/LoopbackPacketSender.h"
#include "mc/network/packet/ItemStackResponseContainerInfo.h"
#include "mc/network/packet/ItemStackResponseInfo.h"
#include "mc/network/packet/ItemStackResponseSlotInfo.h"
#include "mc/network/packet/ItemStackResponsePacket.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/network/MinecraftPacketIds.h"
#include "mc/network/packet/InventoryContentPacket.h"
#include "mc/network/packet/InventorySlotPacket.h"
#include "mc/network/packet/InventoryTransactionPacket.h"
#include "mc/network/packet/UpdateBlockPacket.h"
#include "mc/world/actor/player/Inventory.h"
#include "mc/world/actor/player/PlayerInventory.h"
#include "mc/world/inventory/transaction/ComplexInventoryTransaction.h"
#include "mc/world/inventory/transaction/InventoryTransaction.h"
#include "mc/world/inventory/transaction/ItemUseInventoryTransaction.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/block/BlockType.h"
#include <atomic>
#include <cmath>
#include <format>
#include <string>

namespace lamium::schematic::assist::trace {
namespace {
std::atomic<int> lines{0};
void log(std::string const& text) noexcept {
    if (lines.fetch_add(1) >= 1500) return;
    try { Runtime::instance().self().getLogger().info("Assist net: {}", text); } catch (...) {}
}
LocalPlayer* localPlayer() {
    auto client = ll::service::getClientInstance();
    return client ? client->getLocalPlayer() : nullptr;
}
std::string slotText(LocalPlayer& player, int slot) {
    if (slot < 0 || slot >= 36) return "?";
    auto const& stack = player.getInventory().getItem(slot);
    return stack.isNull() ? std::string{"empty"} : std::format("{} x{}", stack.getTypeName(), static_cast<int>(stack.mCount));
}

// The server setting one slot (a revert puts the old item back here).
LL_TYPE_INSTANCE_HOOK(AssistTraceSlot, ll::memory::HookPriority::Low, LegacyClientNetworkHandler,
    &LegacyClientNetworkHandler::$handle, void, NetworkIdentifier const& source, InventorySlotPacket const& packet) {
    origin(source, packet);
    try {
        auto* player = localPlayer();
        if (!player) return;
        int const slot = static_cast<int>(packet.mSlot);
        log(std::format("slot from server: container {} slot {} -> {} (selected {})", static_cast<int>(packet.mInventoryId), slot,
                        slotText(*player, slot), player->mInventory->mSelected));
    } catch (...) {}
}
// The server setting a whole container (a resync).
LL_TYPE_INSTANCE_HOOK(AssistTraceContent, ll::memory::HookPriority::Low, LegacyClientNetworkHandler,
    &LegacyClientNetworkHandler::$handle, void, NetworkIdentifier const& source, InventoryContentPacket const& packet) {
    origin(source, packet);
    try {
        auto* player = localPlayer();
        if (!player) return;
        log(std::format("content from server: container {} (selected {} now {})", static_cast<int>(packet.mInventoryId),
                        player->mInventory->mSelected, slotText(*player, player->mInventory->mSelected)));
    } catch (...) {}
}
// The server setting a block near the player (a refused placement is undone here).
LL_TYPE_INSTANCE_HOOK(AssistTraceBlock, ll::memory::HookPriority::Low, LegacyClientNetworkHandler,
    &LegacyClientNetworkHandler::$handle, void, NetworkIdentifier const& source, std::shared_ptr<UpdateBlockPacket> packet) {
    BlockPos pos{};
    unsigned layer = 0, runtime = 0;
    if (packet) {
        pos = *packet->mPos;
        layer = packet->mLayer;
        runtime = packet->mRuntimeId;
    }
    origin(source, packet);
    try {
        auto* player = localPlayer();
        if (!player || !packet) return;
        Vec3 const eye = player->getEyePos();
        double const dx = pos.x + .5 - eye.x, dy = pos.y + .5 - eye.y, dz = pos.z + .5 - eye.z;
        if (dx * dx + dy * dy + dz * dz > 10 * 10) return;
        Block const& now = player->getDimensionBlockSource().getBlock(pos, layer);
        log(std::format("block from server: {},{},{} layer {} runtime {} -> {}", pos.x, pos.y, pos.z, layer, runtime,
                        now.getBlockType().mNameInfo->mFullName->getString()));
    } catch (...) {}
}
void sentAs(char const* path, Packet& packet) noexcept;
// Packets sent by sendToServer, not send: the hand swap may go this way.
LL_TYPE_INSTANCE_HOOK(AssistTraceToServer, ll::memory::HookPriority::Low, LoopbackPacketSender, &LoopbackPacketSender::$sendToServer,
    void, Packet& packet) {
    sentAs("toServer", packet);
    origin(packet);
}
// The server's answers to item stack requests (legacy ones included).
LL_TYPE_INSTANCE_HOOK(AssistTraceResponse, ll::memory::HookPriority::Low, ClientNetworkHandler, &ClientNetworkHandler::$handle,
    void, NetworkIdentifier const& source, ItemStackResponsePacket const& packet) {
    origin(source, packet);
    try {
        std::string results;
        for (auto const& r : *packet.mResponses) results += std::format("{} ", static_cast<int>(r.mResult));
        log(std::format("stack responses from server: {} (results {})", packet.mResponses->size(), results));
    } catch (...) {}
}
}
void sent(Packet& packet) noexcept { sentAs("send", packet); }
namespace {
void sentAs(char const* path, Packet& packet) noexcept {
    try {
        if (packet.getId() != MinecraftPacketIds::InventoryTransaction) return;
        auto& transaction = static_cast<InventoryTransactionPacket&>(packet);
        auto* complex = transaction.mTransaction.get();
        if (!complex) return;
        auto* player = localPlayer();
        int const selected = player ? player->mInventory->mSelected : -1;
        if (complex->mType == ComplexInventoryTransaction::Type::ItemUseTransaction) {
            auto& use = static_cast<ItemUseInventoryTransaction&>(*complex);
            BlockPos const pos = *use.mPos;
            log(std::format("{} item use: action {} at {},{},{} face {} slot {} (selected {} holds {})", path,
                            static_cast<int>(use.mActionType), pos.x, pos.y, pos.z, static_cast<int>(use.mFace), static_cast<int>(use.mSlot), selected,
                            player ? slotText(*player, selected) : "?"));
            return;
        }
        log(std::format("{} transaction type {} (selected {} holds {})", path, static_cast<int>(complex->mType), selected,
                        player ? slotText(*player, selected) : "?"));
    } catch (...) {}
}
}
void line(char const* text) noexcept { log(text); }
void start() {
    AssistTraceSlot::hook();
    AssistTraceContent::hook();
    AssistTraceBlock::hook();
    AssistTraceToServer::hook();
    AssistTraceResponse::hook();
}
void stop() {
    AssistTraceResponse::unhook();
    AssistTraceToServer::unhook();
    AssistTraceBlock::unhook();
    AssistTraceContent::unhook();
    AssistTraceSlot::unhook();
}
}
#endif
