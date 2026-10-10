#include "features/inspection/LockedTrades.h"
#include "app/Runtime.h"
#include "ll/api/memory/Hook.h"
#include "mc/client/gui/controls/UIPropertyBag.h"
#include "mc/client/gui/screens/ScreenController.h"
#include "mc/deps/json/Value.h"
#include <string>

namespace lamium::inspection::lockedTrades {
namespace {
using CollectionBind = bool (ScreenController::*)(std::string const&, uint, int, std::string const&, uint,
    std::string const&, UIPropertyBag&);
// Vanilla binds "#tier_visible" per entry of the "trade_tiers" collection:
// true up to the next locked level, false above it. The locked look comes
// from "#is_tier_unlocked" and "#trade_toggle_enabled", left as vanilla sets them.
LL_TYPE_INSTANCE_HOOK(TierVisibleHook, ll::memory::HookPriority::Normal, ScreenController,
    static_cast<CollectionBind>(&ScreenController::$bind), bool, std::string const& collection, uint collectionHash,
    int index, std::string const& name, uint nameHash, std::string const& override, UIPropertyBag& bag) {
    bool result = origin(collection, collectionHash, index, name, nameHash, override, bag);
    try {
        if (name == "#tier_visible" && collection == "trade_tiers" && Runtime::instance().snapshot()->inspection.lockedTrades) {
            auto& value = (*bag.mJsonValue)[override.empty() ? name : override];
            if (value.isBool()) value.value_.bool_ = true;
        }
    } catch (...) {}
    return result;
}
bool installed = false;
}
bool start() {
    if (!installed) installed = TierVisibleHook::hook(true) == 0;
    return installed;
}
void stop() {
    if (installed && TierVisibleHook::unhook(true)) installed = false;
}
}
