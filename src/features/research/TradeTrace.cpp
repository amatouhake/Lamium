#include "features/research/TradeTrace.h"
#ifdef LAMIUM_TRADE_TRACE
#include "app/Runtime.h"
#include "ll/api/memory/Hook.h"
#include "mc/client/network/ClientNetworkHandler.h"
#include "mc/network/packet/UpdateTradePacket.h"
#include <atomic>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace lamium::researchTrace::trade {
namespace {
std::atomic<int> count{0};
LL_TYPE_INSTANCE_HOOK(TradeHook, ll::memory::HookPriority::Normal, ClientNetworkHandler,
    &ClientNetworkHandler::$handle, void, NetworkIdentifier const& source, UpdateTradePacket const& packet) {
    try {
        int n = ++count;
        if (n <= 20) {
            auto const& p = static_cast<UpdateTradePacketPayload const&>(packet);
            auto data = p.mData->toString();
            auto path = Runtime::instance().self().getModDir() / "logs" / std::format("trade-{}.snbt", n);
            std::ofstream{path, std::ios::binary} << data;
            Runtime::instance().self().getLogger().info(
                "L-129 trade {}: name='{}' tier={} size={} newScreen={} economy={} entity={} bytes={} -> {}", n,
                *p.mDisplayName, static_cast<int>(p.mTraderTier), static_cast<int>(p.mSize), static_cast<bool>(p.mUseNewTradeScreen), static_cast<bool>(p.mUsingEconomyTrade),
                p.mEntityUniqueID->rawID, data.size(), path.filename().string());
        }
    } catch (...) {}
    origin(source, packet);
}
}
void start() {
    if (TradeHook::hook(true) != 0) throw std::runtime_error("Could not install trade diagnostics");
    Runtime::instance().self().getLogger().warn("Trade diagnostics enabled (L-129)");
}
void stop() { TradeHook::unhook(true); }
}
#else
namespace lamium::researchTrace::trade { void start() {} void stop() {} }
#endif
