#include "features/inventory/game/CursorState.h"
#include "features/inventory/game/TransferTrace.h"
#ifdef LAMIUM_TRANSFER_TRACE
#include "app/TraceLog.h"
#include "app/Versions.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/input/KeyInputEvent.h"
#include "ll/api/event/input/MouseInputEvent.h"
#include "ll/api/memory/Hook.h"
#include "mc/client/gui/screens/controllers/ContainerScreenController.h"
#include "mc/client/gui/screens/ScreenView.h"
#include "mc/client/gui/screens/ScreenController.h"
#include "mc/client/gui/screens/ScreenEvent.h"
#include "mc/client/gui/screens/ScreenEventType.h"
#include "mc/client/network/ClientNetworkHandler.h"
#include "mc/network/packet/ItemStackResponseSlotInfo.h"
#include "mc/network/packet/ItemStackResponseContainerInfo.h"
#include "mc/network/packet/ItemStackResponseInfo.h"
#include "mc/network/packet/ItemStackResponsePacket.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/containers/managers/models/ContainerManagerModel.h"
#include "mc/world/inventory/network/ItemStackNetManagerClient.h"
#include "mc/world/inventory/network/ItemStackRequestBatch.h"
#include "mc/world/inventory/network/ItemStackRequestData.h"
#include "mc/deps/input/MouseAction.h"
#include "mc/world/containers/controllers/AutoPlaceItem.h"
#include "mc/world/containers/managers/controllers/ContainerManagerController.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/containers/SlotData.h"
#include <Windows.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <format>
#include <iterator>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace lamium::inventory::game::transferTrace {
namespace {
TraceBudget viewBudget, screenBudget, routeBudget, inputBudget, callBudget, buttonBudget, responseBudget;
std::atomic<int64_t> screenAt{0};
std::atomic<bool> altEvent{false};
ll::event::ListenerPtr mouseListener, keyListener;
std::set<std::string> seenViews;
int64_t now() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
bool allowed() { return versionSensitiveAllowed(capabilities::Id::InventoryDiagnostics); }
bool recent() { auto at = screenAt.load(); return at != 0 && now() - at < 1000; }
std::string modifiers() {
    constexpr int down = 0x8000;
    return std::format("shift={} ctrl={} alt={} alt-event={}", bool(GetAsyncKeyState(VK_SHIFT) & down),
        bool(GetAsyncKeyState(VK_CONTROL) & down), bool(GetAsyncKeyState(VK_MENU) & down), altEvent.load());
}
std::vector<std::string> collections(ContainerManagerController& manager) {
    std::vector<std::string> names;
    size_t visited = 0;
    for (auto const& [name, container] : manager.mContainers.get()) {
        if (++visited > 32) break;
        if (container) names.push_back(name);
    }
    std::ranges::sort(names);
    return names;
}
std::string snapshot(ContainerScreenController& controller) {
    auto manager = controller.mContainerManagerController;
    if (!manager || manager->mContainersClosed) return "closed";
    std::string result = std::format("type={} cursor={}", int(manager->getContainerType()),
        game::cursorHasItems(controller));
    int budget = 256;
    for (auto const& name : collections(*manager)) {
        int size = manager->getContainerSize(name);
        result += std::format(" {}[{}]={{", name, size);
        for (int i = 0; i < size && budget > 0; ++i, --budget) {
            auto const& item = manager->getItemStack(name, i);
            if (!item.isNull() && item.mCount > 0)
                result += std::format("{}:{}x{},", i, item.getTypeName(), int(item.mCount));
        }
        result += "}";
        if (budget == 0) { result += " truncated"; break; }
    }
    return result;
}
// Whether vanilla waits for answers: the open request and the batch not yet sent.
std::string requests(ContainerScreenController& controller) {
    auto manager = controller.mContainerManagerController;
    if (!manager) return "requests=?";
    auto model = manager->mContainerManagerModel.lock();
    if (!model) return "requests=?";
    auto* base = model->mPlayer.mItemStackNetManager.get();
    if (!base || !base->mIsClientSide) return "requests=?";
    auto* client = static_cast<ItemStackNetManagerClient*>(base);
    return std::format("open-request={} batched={}", bool(client->mRequest),
        client->mRequestBatch ? client->mRequestBatch->mRequests->size() : 0);
}
void observe(ContainerScreenController& controller, char const* operation, char const* stage,
             std::string const& collection, int index, int amount) noexcept {
    try {
        if (!recent() || !allowed() || !callBudget.take(600)) return;
        Runtime::instance().self().getLogger().info("L-169 vanilla t={} {} {} {}:{} amount={} {} {} {}",
            now(), operation, stage, collection, index, amount, requests(controller), modifiers(), snapshot(controller));
    } catch (...) {}
}
// Observe ordinary player operations only; no diagnostic invokes a handler.
LL_TYPE_INSTANCE_HOOK(TransferTakeAllTrace, ll::memory::HookPriority::Normal, ContainerScreenController,
    &ContainerScreenController::_handleTakeAll, void, std::string const& collection, int index) {
    observe(*this, "take-all", "before", collection, index, -1);
    origin(collection, index);
    observe(*this, "take-all", "predicted-after", collection, index, -1);
}
LL_TYPE_INSTANCE_HOOK(TransferPlaceAllTrace, ll::memory::HookPriority::Normal, ContainerScreenController,
    &ContainerScreenController::$_handlePlaceAll, void, std::string const& collection, int index) {
    observe(*this, "place-all", "before", collection, index, -1);
    origin(collection, index);
    observe(*this, "place-all", "predicted-after", collection, index, -1);
}
LL_TYPE_INSTANCE_HOOK(TransferAutoPlaceTrace, ll::memory::HookPriority::Normal, ContainerScreenController,
    &ContainerScreenController::_handleAutoPlace, void, int amount, std::string const& collection, int index) {
    observe(*this, "auto-place", "before", collection, index, amount);
    origin(amount, collection, index);
    observe(*this, "auto-place", "predicted-after", collection, index, amount);
}
LL_TYPE_INSTANCE_HOOK(TransferAddAllTrace, ll::memory::HookPriority::Normal, ContainerScreenController,
    &ContainerScreenController::_handleAddAllToCursor, void, std::string const& collection, int index) {
    observe(*this, "add-all-to-cursor", "before", collection, index, -1);
    origin(collection, index);
    observe(*this, "add-all-to-cursor", "predicted-after", collection, index, -1);
}
LL_TYPE_INSTANCE_HOOK(TransferTakeAmountTrace, ll::memory::HookPriority::Normal, ContainerScreenController,
    &ContainerScreenController::_handleTakeAmount, void, int amount, std::string const& collection, int index) {
    observe(*this, "take-amount", "before", collection, index, amount);
    origin(amount, collection, index);
    observe(*this, "take-amount", "predicted-after", collection, index, amount);
}
LL_TYPE_INSTANCE_HOOK(TransferTakePlaceTrace, ll::memory::HookPriority::Normal, ContainerScreenController,
    &ContainerScreenController::$_handleTakePlace, int, std::string const& collection, int index, bool all) {
    observe(*this, "take-place", "before", collection, index, all ? -1 : 1);
    int result = origin(collection, index, all);
    observe(*this, "take-place", "predicted-after", collection, index, result);
    return result;
}
LL_TYPE_INSTANCE_HOOK(TransferPlaceOneTrace, ll::memory::HookPriority::Normal, ContainerScreenController,
    &ContainerScreenController::$_handlePlaceOne, void, std::string const& collection, int index) {
    observe(*this, "place-one", "before", collection, index, 1);
    origin(collection, index);
    observe(*this, "place-one", "predicted-after", collection, index, 1);
}
LL_TYPE_INSTANCE_HOOK(TransferTakeHalfTrace, ll::memory::HookPriority::Normal, ContainerScreenController,
    &ContainerScreenController::$_handleTakeHalf, void, std::string const& collection, int index) {
    observe(*this, "take-half", "before", collection, index, -2);
    origin(collection, index);
    observe(*this, "take-half", "predicted-after", collection, index, -2);
}
// The drop key and clicks outside the window in screens (L-169 drop gestures).
// ItemTransferAmount's fields are not named in the SDK; a 12-byte argument is
// passed by address, so the hooks read its raw bytes to learn the layout.
std::string amountBytes(void const* amount) {
    if (!amount) return "null";
    auto const* bytes = static_cast<unsigned char const*>(amount);
    std::string text;
    for (int i = 0; i < 12; ++i) text += std::format("{:02x}{}", bytes[i], i % 4 == 3 && i < 11 ? " " : "");
    return text;
}
std::string labelled(char const* operation, void const* amount) noexcept {
    try { return std::string(operation) + " " + amountBytes(amount); } catch (...) { return {}; }
}
uintptr_t dropItemTarget() { return reinterpret_cast<uintptr_t>(ll::memory::toFuncPtr(&ContainerScreenController::_handleDropItem)); }
uintptr_t dropSelectedTarget() {
    return reinterpret_cast<uintptr_t>(ll::memory::toFuncPtr(&ContainerScreenController::_handleDropSelectedItem));
}
uintptr_t managerDropTarget() { return reinterpret_cast<uintptr_t>(ll::memory::toFuncPtr(&ContainerManagerController::$handleDrop)); }
LL_TYPE_INSTANCE_HOOK(TransferDropItemTrace, ll::memory::HookPriority::Normal, ContainerScreenController,
    dropItemTarget(), void, std::string const& collection, int index, void const* amount) {
    observe(*this, labelled("drop-item", amount).c_str(), "before", collection, index, -1);
    origin(collection, index, amount);
    observe(*this, "drop-item", "predicted-after", collection, index, -1);
}
LL_TYPE_INSTANCE_HOOK(TransferDropSelectedTrace, ll::memory::HookPriority::Normal, ContainerScreenController,
    dropSelectedTarget(), void, void const* amount) {
    observe(*this, labelled("drop-selected", amount).c_str(), "before", "-", -1, -1);
    origin(amount);
    observe(*this, "drop-selected", "predicted-after", "-", -1, -1);
}
LL_TYPE_INSTANCE_HOOK(TransferManagerDropTrace, ll::memory::HookPriority::Normal, ContainerManagerController,
    managerDropTarget(), bool, SlotData const& slot, void const* amount) {
    bool result = origin(slot, amount);
    try {
        if (recent() && allowed())
            traceLog(callBudget, 600, "L-169 vanilla t={} manager-drop {}:{} amount={} result={} {}", now(),
                slot.mCollectionName.get(), int(slot.mCollectionIndex), amountBytes(amount), result, modifiers());
    } catch (...) {}
    return result;
}
// Which button a pack's control sends. Ids are matched to names by pressing
// known vanilla controls (Shift + click is container_auto_place) in the same run.
LL_TYPE_INSTANCE_HOOK(TransferButtonTrace, ll::memory::HookPriority::Normal, ScreenController,
    &ScreenController::_handleEvent, ::ui::ViewRequest, ScreenEvent& event) {
    try {
        if (event.type == ScreenEventType::ButtonEvent && recent() && allowed()) {
            auto const& button = event.data->button.get();
            traceLog(buttonBudget, 600, "L-169 button t={} id={} from={} state={}->{} interacted={} {}", now(),
                button.id, button.fromButtonId, int(button.previousState), int(button.state),
                bool(button.isInteracted), modifiers());
        }
    } catch (...) {}
    return origin(event);
}
LL_TYPE_INSTANCE_HOOK(TransferResponseTrace, ll::memory::HookPriority::Normal, ClientNetworkHandler,
    &ClientNetworkHandler::$handle, void, NetworkIdentifier const& source, ItemStackResponsePacket const& packet) {
    try {
        if (recent() && allowed()) {
            std::string ids;
            size_t shown = 0;
            for (auto const& response : packet.mResponses.get()) {
                if (++shown > 32) { ids += " truncated"; break; }
                ids += std::format(" {}:{}", response.mClientRequestId->mRawId, int(response.mResult));
            }
            traceLog(responseBudget, 400, "L-169 responses t={} count={}{}", now(), packet.mResponses->size(), ids);
        }
    } catch (...) {}
    origin(source, packet);
}
struct Hook { int (*install)(bool); bool (*remove)(bool); bool installed = false; };
Hook hooks[]{
    {TransferTakeAllTrace::hook, TransferTakeAllTrace::unhook},
    {TransferPlaceAllTrace::hook, TransferPlaceAllTrace::unhook},
    {TransferAutoPlaceTrace::hook, TransferAutoPlaceTrace::unhook},
    {TransferAddAllTrace::hook, TransferAddAllTrace::unhook},
    {TransferTakeAmountTrace::hook, TransferTakeAmountTrace::unhook},
    {TransferTakePlaceTrace::hook, TransferTakePlaceTrace::unhook},
    {TransferPlaceOneTrace::hook, TransferPlaceOneTrace::unhook},
    {TransferTakeHalfTrace::hook, TransferTakeHalfTrace::unhook},
    {TransferButtonTrace::hook, TransferButtonTrace::unhook},
    {TransferDropItemTrace::hook, TransferDropItemTrace::unhook},
    {TransferDropSelectedTrace::hook, TransferDropSelectedTrace::unhook},
    {TransferManagerDropTrace::hook, TransferManagerDropTrace::unhook},
    {TransferResponseTrace::hook, TransferResponseTrace::unhook}
};
}
void inspectView(ScreenView& view) noexcept {
    try {
        if (!view.mHasFocus || !view.mController || !allowed() || seenViews.size() >= 24) return;
        auto name = view.getScreenName();
        if (!seenViews.insert(name).second) return;
        traceLog(viewBudget, 24, "L-169 focused view={} is-container={}", name,
            view.mController->_isContainerScreen());
    } catch (...) {}
}
void inspect(ContainerScreenController& controller, bool changed) noexcept {
    try {
        if (!allowed()) return;
        screenAt = now();
        // Input callbacks only see the owned time, never the controller.
        if (!changed) return;
        if (!screenBudget.take(24)) return;
        auto manager = controller.mContainerManagerController;
        if (!manager) return;
        Runtime::instance().self().getLogger().info("L-169 layout {}", snapshot(controller));
        for (auto const& name : collections(*manager)) {
            std::string order;
            if (auto* targets = controller.tryGetAutoPlaceOrder(name)) {
                size_t count = 0;
                for (auto const& target : *targets) {
                    if (++count > 32) { order += " truncated"; break; }
                    order += std::format(" {}(stop={})", target.collection.get(), bool(target.stopWhenPlaced));
                }
            }
            traceLog(routeBudget, 200, "L-169 route {} ->{}", name, order.empty() ? " none" : order);
        }
    } catch (...) {}
}
void start() {
    if (!allowed()) return;
    if (mouseListener && keyListener) return;
    try {
        for (auto& hook : hooks) if (!hook.installed) {
            if (hook.install(true) != 0) throw std::runtime_error("Could not install transfer diagnostics");
            hook.installed = true;
        }
        auto& bus = ll::event::EventBus::getInstance();
        mouseListener = bus.emplaceListener<ll::event::input::MouseInputEvent>([](auto& event) {
            if (!recent() || event.actionButtonId() == MouseAction::ActionMove
                || event.actionButtonId() == MouseAction::ActionMoveRelative) return;
            try { traceLog(inputBudget, 240, "L-169 mouse button={} data={} cancelled={} {}",
                int(event.actionButtonId()), int(event.buttonData()), event.isCancelled(), modifiers()); } catch (...) {}
        });
        keyListener = bus.emplaceListener<ll::event::input::KeyInputEvent>([](auto& event) {
            bool alt = event.keyCode() == VK_MENU || event.keyCode() == VK_LMENU || event.keyCode() == VK_RMENU;
            if (alt) altEvent = event.isDown();
            if (recent()) traceLog(inputBudget, 240, "L-169 key code={} down={} cancelled={} {}",
                event.keyCode(), event.isDown(), event.isCancelled(), modifiers());
        });
        if (!mouseListener || !keyListener) throw std::runtime_error("Could not subscribe transfer diagnostics");
        Runtime::instance().self().getLogger().warn("L-169 read-only transfer diagnostics enabled; disable Item Transfer to observe vanilla");
    } catch (...) {
        stop();
        Runtime::instance().self().getLogger().error("L-169 transfer diagnostics unavailable; inventory features remain enabled");
    }
}
void stop() {
    auto& bus = ll::event::EventBus::getInstance();
    for (auto* listener : {&mouseListener, &keyListener}) {
        if (*listener) bus.removeListener(*listener);
        listener->reset();
    }
    for (auto it = std::rbegin(hooks); it != std::rend(hooks); ++it)
        if (it->installed && !it->remove(true))
            Runtime::instance().self().getLogger().error("Could not remove transfer diagnostic hook");
        else it->installed = false;
    screenAt = 0;
    altEvent = false;
    seenViews.clear();
}
}
#else
namespace lamium::inventory::game::transferTrace {
void start() {}
void stop() {}
void inspectView(ScreenView&) noexcept {}
void inspect(ContainerScreenController&, bool) noexcept {}
}
#endif
