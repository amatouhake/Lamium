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
#include "ll/api/event/EventBus.h"
#include "ll/api/event/render/UIRenderEvent.h"
#include "mc/client/gui/controls/UIControl.h"
#include "mc/client/gui/controls/VisualTree.h"
#include "mc/client/gui/screens/ScreenView.h"
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
    return name.find("tier") != std::string::npos || name.find("trade") != std::string::npos
        || name.find("hover") != std::string::npos; // Round 3: hover text of locked trades.
}
void logBinding(std::string const& collection, int index, std::string const& name, std::string const& override,
                UIPropertyBag& bag) {
    std::string value = "?";
    try {
        auto const& json = *bag.mJsonValue;
        auto const& key = override.empty() ? name : override;
        value = json.isMember(key) ? json[key].toStyledString() : "(unset)";
        while (!value.empty() && (value.back() == '\n' || value.back() == ' ')) value.pop_back();
        for (auto& c : value) if (c == '\n') c = '|';
        if (value.size() > 120) value = value.substr(0, 120) + "...";
    } catch (...) {}
    auto line = std::format("bind [{}:{}] {} -> {} = {}", collection, index, name, override, value);
    std::lock_guard lock{seenMutex};
    if (seen.size() < 1000 && seen.insert(line).second)
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
// Round 4 (2026-10-10): the trade list's control tree once per screen, to
// find locked trade items and their tier/trade indexes.
ll::event::ListenerPtr treeListener;
UIControl const* dumpedRoot = nullptr;
void dumpControl(UIControl const& control, int depth, std::string& out, int& lines) {
    if (lines > 400 || depth > 14) return;
    ++lines;
    std::string bag;
    try {
        if (auto const& owned = control.mPropertyBag; owned) {
            auto const& json = *owned->mJsonValue;
            for (char const* key : {"#collection_index", "#collection_name", "$collection_name", "collection_index"})
                if (json.isMember(key)) {
                    auto value = json[key].toStyledString();
                    while (!value.empty() && (value.back() == '\n' || value.back() == ' ')) value.pop_back();
                    bag += std::format(" {}={}", key, value);
                }
        }
    } catch (...) { bag = " bag?"; }
    glm::vec2 position = *control.mCachedPosition, size = *control.mSize;
    out += std::format("\n{}{} en={} anc={} hover={} vis={} at {:.1f},{:.1f} size {:.1f}x{:.1f}{}", std::string(depth * 2, ' '),
        *control.mName, control.mEnabled, control.mAllAncestorsEnabled, control.mHover,
        static_cast<int>(control.mVisible), position.x, position.y, size.x, size.y, bag);
    for (auto const& child : *control.mChildren)
        if (child) dumpControl(*child, depth + 1, out, lines);
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
    treeListener = ll::event::EventBus::getInstance().emplaceListener<ll::event::AfterUIRenderEvent>([](auto& event) {
        try {
            auto* tree = event.screenView().mVisualTree.get();
            if (!tree) return;
            auto root = tree->getControlByName("trade_selector_stack_panel", true);
            if (!root || root.get() == dumpedRoot || root->mCachedPositionDirty) return;
            static int frames = 0;
            if (++frames < 30) return; // Let the list lay out first.
            frames = 0;
            dumpedRoot = root.get();
            std::string out;
            int lines = 0;
            dumpControl(*root, 0, out, lines);
            Runtime::instance().self().getLogger().info("L-129 trade list tree:{}", out);
        } catch (...) {}
    });
    Runtime::instance().self().getLogger().warn("Trade diagnostics enabled (L-129)");
}
void stop() {
    if (treeListener) ll::event::EventBus::getInstance().removeListener(treeListener);
    treeListener.reset();
    TradeHook::unhook(true); CollectionBindHook::unhook(true); GlobalBindHook::unhook(true); }
}
#else
namespace lamium::researchTrace::trade { void start() {} void stop() {} }
#endif
