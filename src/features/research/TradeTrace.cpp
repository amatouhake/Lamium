#include "features/research/TradeTrace.h"
#ifdef LAMIUM_TRADE_TRACE
#include "app/Runtime.h"
#include "ll/api/memory/Hook.h"
#include "mc/client/network/ClientNetworkHandler.h"
#include "mc/network/packet/UpdateTradePacket.h"
#include "mc/client/gui/screens/ScreenController.h"
#include "mc/client/gui/screens/controllers/Trade2ScreenController.h"
#include "mc/client/gui/controls/UIPropertyBag.h"
#include "mc/deps/json/Value.h"
#include <mutex>
#include <set>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace lamium::researchTrace::trade {
namespace {
std::atomic<int> count{0};
// Round 2 (2026-10-10): the trade screen's tier and trade bindings, to find
// what hides locked tiers. Each distinct value is logged once.
std::mutex seenMutex;
std::set<std::string> seen;
bool tradeBinding(std::string const& name) {
    return name.find("tier") != std::string::npos || name.find("trade") != std::string::npos;
}
void logBinding(std::string const& collection, int index, std::string const& name, std::string const& override,
                UIPropertyBag& bag) {
    std::string value = "?";
    try {
        auto const& json = *bag.mJsonValue;
        auto const& key = override.empty() ? name : override;
        value = json.isMember(key) ? json[key].toStyledString() : "(unset)";
        while (!value.empty() && (value.back() == '\n' || value.back() == ' ')) value.pop_back();
    } catch (...) {}
    auto line = std::format("bind [{}:{}] {} -> {} = {}", collection, index, name, override, value);
    std::lock_guard lock{seenMutex};
    if (seen.size() < 400 && seen.insert(line).second)
        Runtime::instance().self().getLogger().info("L-129 {}", line);
}
using CollectionBind = bool (ScreenController::*)(std::string const&, uint, int, std::string const&, uint,
    std::string const&, UIPropertyBag&);
using GlobalBind = bool (ScreenController::*)(std::string const&, uint, std::string const&, UIPropertyBag&);
LL_TYPE_INSTANCE_HOOK(CollectionBindHook, ll::memory::HookPriority::Normal, ScreenController,
    static_cast<CollectionBind>(&ScreenController::$bind), bool, std::string const& collection, uint collectionHash, int index,
    std::string const& name, uint nameHash, std::string const& override, UIPropertyBag& bag) {
    bool result = origin(collection, collectionHash, index, name, nameHash, override, bag);
    try { if (tradeBinding(name)) logBinding(collection, index, name, override, bag); } catch (...) {}
    return result;
}
LL_TYPE_INSTANCE_HOOK(GlobalBindHook, ll::memory::HookPriority::Normal, ScreenController,
    static_cast<GlobalBind>(&ScreenController::$bind), bool, std::string const& name, uint nameHash, std::string const& override,
    UIPropertyBag& bag) {
    bool result = origin(name, nameHash, override, bag);
    try {
        if (tradeBinding(name)) logBinding("", -1, name, override, bag);
        if (name == "#trade_selector_total") {
            auto const& tiers = *reinterpret_cast<Trade2ScreenController*>(this)->mNumberOfTradesByTier;
            std::string list;
            if (tiers.size() <= 10) // Sanity check: the cast assumes ScreenController is the first base.
                for (int n : tiers) list += std::format("{} ", n);
            logBinding("numberOfTradesByTier", static_cast<int>(tiers.size()), list, "", bag);
        }
    } catch (...) {}
    return result;
}
LL_TYPE_INSTANCE_HOOK(TradeHook, ll::memory::HookPriority::Normal, ClientNetworkHandler,
    &ClientNetworkHandler::$handle, void, NetworkIdentifier const& source, UpdateTradePacket const& packet) {
    try {
        int n = ++count;
        if (n <= 20) {
            auto const& p = static_cast<UpdateTradePacketPayload const&>(packet);
            auto data = p.mData->toSnbt();
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
    if (TradeHook::hook(true) != 0 || CollectionBindHook::hook(true) != 0 || GlobalBindHook::hook(true) != 0)
        throw std::runtime_error("Could not install trade diagnostics");
    Runtime::instance().self().getLogger().warn("Trade diagnostics enabled (L-129)");
}
void stop() { TradeHook::unhook(true); CollectionBindHook::unhook(true); GlobalBindHook::unhook(true); }
}
#else
namespace lamium::researchTrace::trade { void start() {} void stop() {} }
#endif
