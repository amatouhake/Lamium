#include "features/inventory/game/TransferRoutes.h"
#include "features/inventory/game/CursorState.h"
#include "features/inventory/game/ScreenTracker.h"
#include "features/inventory/game/TransferSession.h"
#include "mc/client/gui/screens/ScreenView.h"
#include "app/Runtime.h"
#include "app/Versions.h"
#include "ll/api/memory/Hook.h"
#include "mc/client/gui/screens/controllers/ContainerScreenController.h"
#include "mc/world/containers/SlotData.h"
#include "mc/world/containers/controllers/AutoPlaceItem.h"
#include "mc/world/containers/managers/controllers/ContainerManagerController.h"
#include "mc/world/item/ItemStack.h"
#include <algorithm>
#include <utility>

namespace lamium::inventory::game::transferRoutes {
namespace {
bool amountInstalled = false, stackInstalled = false, outputsInstalled = false, dropInstalled = false;
constexpr auto capability = capabilities::Id::TransferRoutes;
struct RouteScope;
thread_local RouteScope* active = nullptr;
// Borrowed only during one synchronous vanilla call, never across a frame.
struct RouteScope {
    ContainerManagerController* manager;
    std::vector<AutoPlaceItem> const& order;
    RouteScope* previous;
    bool applied = false;
    RouteScope(ContainerManagerController& value, std::vector<AutoPlaceItem> const& targets)
    : manager(&value), order(targets), previous(std::exchange(active, this)) {}
    ~RouteScope() { active = previous; }
};
// Keep the screen's request/feedback path, and the game's slot restrictions.
// Only Lamium's synchronous call gets a filtered destination order.
LL_TYPE_INSTANCE_HOOK(TransferAutoPlaceOrder, ll::memory::HookPriority::Normal, ContainerManagerController,
    &ContainerManagerController::$handleAutoPlace, int, SlotData const& source, int amount,
    std::vector<AutoPlaceItem> const& order, std::vector<AutoPlaceResult>& destinations) {
    if (active && active->manager == this) {
        active->applied = true;
        return origin(source, amount, active->order, destinations);
    }
    return origin(source, amount, order, destinations);
}
LL_TYPE_INSTANCE_HOOK(TransferAutoPlaceStackOrder, ll::memory::HookPriority::Normal, ContainerManagerController,
    &ContainerManagerController::$handleAutoPlaceStack, int, SlotData const& source, ItemTakeType type,
    std::vector<AutoPlaceItem> const& order, std::vector<AutoPlaceResult>& destinations) {
    if (active && active->manager == this) {
        active->applied = true;
        return origin(source, type, active->order, destinations);
    }
    return origin(source, type, order, destinations);
}
// Specialized managers can override the two public entry points; their
// request output builder must use the same restricted targets too.
LL_TYPE_INSTANCE_HOOK(TransferAutoPlaceOutputsOrder, ll::memory::HookPriority::Normal, ContainerManagerController,
    &ContainerManagerController::_appendAutoPlaceOutputs, int, ItemTransferRequest& request,
    ItemStackBase const& item, int amount, std::vector<AutoPlaceItem> const& order, int group) {
    if (active && active->manager == this) {
        active->applied = true;
        return origin(request, item, amount, active->order, group);
    }
    return origin(request, item, amount, order, group);
}
// The drop calls take ItemTransferAmount by value; a 12-byte argument is
// passed by address, so they are called and hooked with its traced bytes
// (TransferGesture.h dropAmountBytes).
using DropItem = void (*)(ContainerScreenController*, std::string const&, int, void const*);
using DropSelected = void (*)(ContainerScreenController*, void const*);
uintptr_t dropSelectedTarget() {
    return reinterpret_cast<uintptr_t>(ll::memory::toFuncPtr(&ContainerScreenController::_handleDropSelectedItem));
}
// Lamium's own cursor drop (all but one) is not a player's click outside.
thread_local bool ownDrop = false;
// A click outside the window drops the held stack here; with Shift, the
// matching items follow (L-169).
LL_TYPE_INSTANCE_HOOK(TransferDropSelected, ll::memory::HookPriority::Normal, ContainerScreenController,
    dropSelectedTarget(), void, void const* amount) {
    std::optional<ItemStack> held;
    try {
        auto manager = mContainerManagerController;
        if (!ownDrop && manager && !manager->mContainersClosed && manager->hasContainerController("cursor_items")) {
            auto const& item = manager->getItemStack("cursor_items", 0);
            if (!item.isNull() && item.mCount > 0) held = item;
        }
    } catch (...) { held.reset(); }
    origin(amount);
    if (held) TransferSession::cursorDropped(*this, *held);
}
}
bool allowed() { return versionSensitiveAllowed(capability); }
bool supported(ContainerScreenController& controller) {
    if (!allowed()) return false;
    auto manager = controller.mContainerManagerController;
    if (!manager || manager->mContainersClosed || manager->mContainers->size() > 32
        || controller.mAutoPlaceOrderMap->size() > 32) return false;
    for (auto const& [source, order] : controller.mAutoPlaceOrderMap.get())
        if (order.size() > 32) return false;
    return true;
}
void start() {
    capabilities::begin(capability);
    if (capabilities::registry().status(capability).compatibility != capabilities::Reason::None) {
        versionSensitiveAllowed(capability); return;
    }
    if (!amountInstalled) amountInstalled = TransferAutoPlaceOrder::hook(true) == 0;
    if (!stackInstalled) stackInstalled = TransferAutoPlaceStackOrder::hook(true) == 0;
    if (!outputsInstalled) outputsInstalled = TransferAutoPlaceOutputsOrder::hook(true) == 0;
    if (!dropInstalled) dropInstalled = TransferDropSelected::hook(true) == 0;
    if (!amountInstalled || !stackInstalled || !outputsInstalled || !dropInstalled) {
        stop();
        capabilities::hooks(capability,false);
        Runtime::instance().self().getLogger().warn("Item Transfer extensions unavailable: native route hook failed");
    } else capabilities::ready(capability);
}
void stop() {
    capabilities::stop(capability);
    if (amountInstalled && TransferAutoPlaceOrder::unhook(true)) amountInstalled = false;
    if (stackInstalled && TransferAutoPlaceStackOrder::unhook(true)) stackInstalled = false;
    if (outputsInstalled && TransferAutoPlaceOutputsOrder::unhook(true)) outputsInstalled = false;
    if (dropInstalled && TransferDropSelected::unhook(true)) dropInstalled = false;
    if (amountInstalled || stackInstalled || outputsInstalled || dropInstalled) {
        capabilities::fail(capability,capabilities::Reason::RemovalFailed);
        Runtime::instance().self().getLogger().error("Could not remove Item Transfer route hooks");
    }
}
std::vector<std::string> collections(ContainerScreenController& controller) {
    std::vector<std::string> result;
    if (!supported(controller)) return result;
    auto manager = controller.mContainerManagerController;
    if (!manager || manager->mContainersClosed) return result;
    auto append = [&](std::string const& name) {
        if (result.size() < 32 && !transfer::playerCollection(name) && !transfer::excludedCollection(name)
            && manager->hasContainerController(name) && std::ranges::find(result, name) == result.end())
            result.push_back(name);
    };
    size_t visited = 0;
    for (auto const& [source, order] : controller.mAutoPlaceOrderMap.get()) {
        if (++visited > 32) break;
        size_t targets = 0;
        for (auto const& target : order) {
            if (++targets > 32) break;
            if (transfer::playerCollection(source)) append(target.collection.get());
            else if (transfer::playerCollection(target.collection.get())) append(source);
        }
    }
    std::ranges::sort(result);
    return result;
}
bool autoPlace(ContainerScreenController& controller, std::string const& collection,
               int index, int amount, transfer::Side destination) {
    if (!supported(controller) || amount <= 0) return false;
    auto manager = controller.mContainerManagerController;
    auto const* order = controller.tryGetAutoPlaceOrder(collection);
    if (!manager || manager->mContainersClosed || !order) return false;
    auto storage = collections(controller);
    std::vector<AutoPlaceItem> filtered;
    size_t visited = 0;
    for (auto const& target : *order) {
        if (++visited > 32) break;
        auto const& name = target.collection.get();
        bool const accepted = transfer::routeTarget(name, destination, std::ranges::find(storage, name) != storage.end());
        if (accepted && manager->hasContainerController(name))
            filtered.emplace_back(name, bool(target.stopWhenPlaced));
    }
    if (filtered.empty()) return false;
    RouteScope scope(*manager, filtered);
    controller._handleAutoPlace(amount, collection, index);
    if (!scope.applied) Runtime::instance().self().getLogger().warn("Item Transfer stopped: native route filtering was not reached");
    return scope.applied;
}
bool drop(ContainerScreenController& controller, std::string const& collection, int index, bool one) {
    if (!supported(controller)) return false;
    auto bytes = transfer::dropAmountBytes(one);
    reinterpret_cast<DropItem>(ll::memory::toFuncPtr(&ContainerScreenController::_handleDropItem))(
        &controller, collection, index, bytes.data());
    return true;
}
bool pressAsPlainClick(ContainerScreenController& controller) {
    if (!supported(controller)) return false;
    auto& tracker = ScreenTracker::getInstance();
    auto const active = tracker.current();
    auto const* view = tracker.currentView();
    if (!view || active.get() != &controller) return false;
    // The global button and focus of a plain left press, traced on 1.26.60.05 (L-305).
    constexpr uint plainLeftPress = 3024615655u;
    auto& target = const_cast<ScreenView&>(*view);
    target.handleButtonEvent(plainLeftPress, ButtonState::Down, FocusImpact::DeactivateFocus);
    target.handleButtonEvent(plainLeftPress, ButtonState::Up, FocusImpact::DeactivateFocus);
    return true;
}
bool dropAmount(ContainerScreenController& controller, std::string const& collection, int index, int amount) {
    if (!supported(controller) || amount <= 0 || game::cursorHasItems(controller)) return false;
    controller._handleTakeAmount(amount, collection, index);
    // Not taken onto the cursor: nothing to drop.
    if (!game::cursorHasItems(controller)) return false;
    auto bytes = transfer::dropAmountBytes(false);
    ownDrop = true;
    try {
        reinterpret_cast<DropSelected>(dropSelectedTarget())(&controller, bytes.data());
    } catch (...) { ownDrop = false; throw; }
    ownDrop = false;
    return true;
}
}
