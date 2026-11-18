#include "features/research/CraftTrace.h"
#ifdef LAMIUM_CRAFT_TRACE
#include "app/Runtime.h"
#include "app/TraceLog.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/render/UIRenderEvent.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/service/TargetedBedrock.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/gui/controls/UIControl.h"
#include "mc/client/gui/controls/VisualTree.h"
#include "mc/client/gui/screens/ScreenController.h"
#include "mc/client/gui/screens/ScreenView.h"
#include "mc/client/gui/screens/controllers/ContainerScreenController.h"
#include "mc/client/gui/screens/controllers/CraftingScreenController.h"
#include "mc/client/network/ClientNetworkHandler.h"
#include "mc/network/packet/ItemStackResponseContainerInfo.h"
#include "mc/network/packet/ItemStackResponseInfo.h"
#include "mc/network/packet/ItemStackResponsePacket.h"
#include "mc/world/containers/SlotData.h"
#include "mc/world/containers/controllers/AutoPlaceItem.h"
#include "mc/world/containers/controllers/ItemCraftType.h"
#include "mc/world/containers/managers/controllers/ContainerManagerController.h"
#include "mc/world/containers/managers/controllers/CraftingContainerManagerController.h"
#include "mc/world/inventory/network/ItemStackNetResult.h"
#include "mc/world/inventory/network/ItemStackRequestAction.h"
#include "mc/world/inventory/simulation/AllowedAnywhereConsume.h"
#include "mc/world/inventory/simulation/ExpectedAnywhereConsume.h"
#include "mc/world/inventory/simulation/ExpectedSlotConsume.h"
#include "mc/world/inventory/network/crafting/CraftHandlerCrafting.h"
#include "mc/world/inventory/network/crafting/ItemStackRequestActionCraftBase.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/item/crafting/Recipe.h"
#include <atomic>
#include <chrono>
#include <format>
#include <iterator>
#include <map>
#include <mutex>
#include <set>
#include <stdexcept>
#include <string>

namespace lamium::researchTrace::craft {
namespace {
using Clock = std::chrono::steady_clock;
TraceBudget callBudget, stateBudget, requestBudget, responseBudget, screenBudget, pressBudget;
std::atomic<int64_t> observedAt{0};
std::atomic<unsigned> sequence{0};
std::mutex seenMutex;
std::string lastState, lastHover;
std::map<std::string, int> screenFrames; // per item screen: frames seen; reported once it has stood a moment
std::set<std::string> reported;
ll::event::ListenerPtr renderListener;
int64_t nowMs() { return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now().time_since_epoch()).count(); }
// Requests and answers are logged only around a crafting screen, so other screens' traffic stays out.
bool recent() {
    auto const at = observedAt.load();
    return at != 0 && nowMs() - at < 10000;
}
template <class... Args>
void log(TraceBudget& budget, unsigned limit, std::format_string<Args...> format, Args&&... args) noexcept {
    if (!budget.take(limit)) return;
    try {
        Runtime::instance().self().getLogger().info("L-359 craft seq={} ms={} {}", ++sequence, nowMs(),
            std::format(format, std::forward<Args>(args)...));
    } catch (...) {}
}
std::string recipeText(Recipe const* recipe) {
    if (!recipe) return "none";
    try { return std::format("{}#{}", *recipe->mRecipeId, recipe->mRecipeNetId->mRawId); } catch (...) { return "unreadable"; }
}
std::string placesText(std::vector<AutoPlaceItem> const& places) {
    std::string out;
    int count = 0;
    for (auto const& place : places) {
        if (++count > 12) break;
        out += std::format(" {}{}", *place.collection, bool(place.stopWhenPlaced) ? "!" : "");
    }
    return out.empty() ? " (none)" : out;
}
// The small collections (the grid, the output, the pointer) slot by slot; the large ones by name and size.
std::string slotsText(ContainerManagerController& manager) {
    std::string out;
    if (manager.mContainersClosed) return " closed";
    unsigned collections = 0;
    for (auto const& [name, container] : manager.mContainers.get()) {
        if (++collections > 24) break;
        int const count = manager.getContainerSize(name);
        if (count > 9) { out += std::format(" [{}:{}]", name, count); continue; }
        out += std::format(" [{}", name);
        for (int i = 0; i < count; ++i) {
            auto const& stack = manager.getItemStack(name, i);
            out += stack.isNull() ? std::string(" -") : std::format(" {}x{}", stack.getTypeName(), int(stack.mCount));
        }
        out += "]";
    }
    return out;
}
std::string managerText(CraftingContainerManagerController& manager) {
    return std::format("grid={} selected={} crafting={} invalid={} busy={}{}", int(manager.mGridSize), recipeText(manager.mSelectedRecipe),
        recipeText(manager.mCraftingRecipe), bool(manager.mDisplayingInvalidRecipe), bool(manager.mIsCrafting), slotsText(manager));
}
// What the screen does with a slot: the recipe book's cells, the grid, the output. The screen's and the
// manager's own "recipe selected" and "craft all" calls are not in the SDK, so what they did is read from
// these, from the state after them and, in a local world, from what the server is asked.
std::string managerOf(ContainerScreenController& screen) {
    auto manager = screen.mContainerManagerController.get();
    return manager ? slotsText(*manager) : std::string(" no manager");
}
LL_TYPE_INSTANCE_HOOK(CraftSlotHovered, ll::memory::HookPriority::Normal, CraftingScreenController,
    &CraftingScreenController::$_onContainerSlotHovered, ::ui::ViewRequest, std::string const& collection, int index) {
    observedAt = nowMs();
    try {
        std::string cell = std::format("{}:{}", collection, index);
        std::lock_guard lock{seenMutex};
        if (cell != lastHover) {
            auto const& stack = _getVisualItemStack(collection, index);
            log(pressBudget, 400, "slot-hovered {} item={}", cell, stack.isNull() ? std::string("-") : stack.getTypeName());
            lastHover = std::move(cell);
        }
    } catch (...) {}
    return origin(collection, index);
}
LL_TYPE_INSTANCE_HOOK(CraftAutoPlace, ll::memory::HookPriority::Normal, ContainerScreenController,
    &ContainerScreenController::_handleAutoPlace, void, int amount, std::string const& collection, int index) {
    if (recent()) log(callBudget, 400, "auto-place amount={} {}:{} before:{}", amount, collection, index, managerOf(*this));
    origin(amount, collection, index);
    if (recent()) log(callBudget, 400, "auto-place-return after:{}", managerOf(*this));
}
LL_TYPE_INSTANCE_HOOK(CraftTakePlace, ll::memory::HookPriority::Normal, ContainerScreenController,
    &ContainerScreenController::$_handleTakePlace, int, std::string const& collection, int index, bool all) {
    if (recent()) log(callBudget, 400, "take-place {}:{} all={} before:{}", collection, index, all, managerOf(*this));
    int const result = origin(collection, index, all);
    if (recent()) log(callBudget, 400, "take-place-return result={} after:{}", result, managerOf(*this));
    return result;
}
LL_TYPE_INSTANCE_HOOK(CraftPlaceOne, ll::memory::HookPriority::Normal, CraftingScreenController,
    &CraftingScreenController::$_handlePlaceOne, void, std::string const& collection, int index) {
    log(callBudget, 400, "place-one {}:{}", collection, index);
    origin(collection, index);
}
LL_TYPE_INSTANCE_HOOK(CraftEvacuate, ll::memory::HookPriority::Normal, CraftingScreenController,
    &CraftingScreenController::_evacuateCraftingGrid, void) {
    log(callBudget, 400, "evacuate-grid");
    origin();
}
// A local world's own server, in this process: the craft it is asked for, by recipe and how many times.
LL_TYPE_INSTANCE_HOOK(CraftServerAction, ll::memory::HookPriority::Normal, CraftHandlerCrafting,
    &CraftHandlerCrafting::$_handleCraftAction, ::ItemStackNetResult, ItemStackRequestActionCraftBase const& action) {
    auto const result = origin(action);
    try {
        log(requestBudget, 400, "server-craft type={} crafts={} recipeNetId={} workbench={} deferred={} result={}", int(action.mActionType),
            int(action.mNumCrafts), mRecipeNetId->mRawId, bool(mWorkbench), int(mDeferredCraftCount), int(result));
    } catch (...) {}
    return result;
}
// The screen's state whenever it changes: the chosen recipe, the grid, the output.
LL_TYPE_INSTANCE_HOOK(CraftTick, ll::memory::HookPriority::Normal, CraftingScreenController,
    &CraftingScreenController::$tick, ::ui::DirtyFlag) {
    auto result = origin();
    try {
        observedAt = nowMs();
        if (auto manager = mContainerManagerController.get()) {
            std::string state = managerText(static_cast<CraftingContainerManagerController&>(*manager));
            std::lock_guard lock{seenMutex};
            if (state != lastState) {
                log(stateBudget, 600, "state {}", state);
                lastState = std::move(state);
            }
        }
    } catch (...) {}
    return result;
}
LL_TYPE_INSTANCE_HOOK(CraftResponse, ll::memory::HookPriority::Normal, ClientNetworkHandler,
    &ClientNetworkHandler::$handle, void, NetworkIdentifier const& source, ItemStackResponsePacket const& packet) {
    origin(source, packet);
    try {
        if (!recent()) return;
        unsigned count = 0;
        for (auto const& response : packet.mResponses.get()) {
            if (++count > 32) break;
            size_t slots = 0;
            for (auto const& container : response.mContainers.get()) slots += container.mSlots->size();
            log(responseBudget, 400, "response id={} result={} containers={} slots={}", response.mClientRequestId->mRawId, int(response.mResult),
                response.mContainers->size(), slots);
        }
    } catch (...) {}
}
// Shown controls with a size, a few levels down: name and box.
void describe(UIControl const& control, int depth, std::string& out, int& count) {
    if (count >= 120 || depth > 6) return;
    auto const& children = *control.mChildren;
    if (children.size() > 600) return; // not a child list
    for (auto const& child : children) {
        if (!child || count >= 120) continue;
        std::string const& name = *child->mName;
        if (name.size() > 120 || !(static_cast<int>(child->mVisible) & 1)) continue;
        glm::vec2 const at = *child->mCachedPosition, size = *child->mSize;
        if (size.x > 0 && size.y > 0) {
            ++count;
            out += std::format(" {}:{}[{:.0f},{:.0f} {:.0f}x{:.0f}]", depth, name, at.x, at.y, size.x, size.y);
        }
        describe(*child, depth + 1, out, count);
    }
}
// Once per item screen, after it has stood for a second: its size, its controls and its collections.
void seeScreen(ll::event::UIRenderEvent& event) {
    auto& view = event.screenView();
    if (!view.mHasFocus || !view.mController || !view.mController->_isContainerScreen()) return;
    auto* tree = view.mVisualTree.get();
    UIControl* const root = tree ? tree->mRootControl.get() : nullptr;
    if (!root) return;
    std::string const name = view.getScreenName();
    if (name.size() > 80 || reported.contains(name) || ++screenFrames[name] < 60) return;
    reported.insert(name);
    glm::vec2 const screen = *view.mSize;
    std::string controls, collections;
    int count = 0;
    describe(*root, 0, controls, count);
    auto& controller = static_cast<ContainerScreenController&>(*view.mController);
    if (auto manager = controller.mContainerManagerController) {
        unsigned seen = 0;
        if (!manager->mContainersClosed)
            for (auto const& [collection, container] : manager->mContainers.get()) {
                if (++seen > 40) break;
                collections += std::format(" {}:{}", collection, manager->getContainerSize(collection));
            }
    }
    log(screenBudget, 60, "screen '{}' {:.0f}x{:.0f} collections:{}", name, screen.x, screen.y, collections);
    // The line can be long: in parts, so the log keeps all of it.
    for (size_t at = 0; at < controls.size(); at += 1500) log(screenBudget, 60, "screen '{}' controls:{}", name, controls.substr(at, 1500));
}
struct Hook { int (*install)(bool); bool (*remove)(bool); bool installed = false; };
Hook hooks[] = {
    {CraftSlotHovered::hook, CraftSlotHovered::unhook},
    {CraftAutoPlace::hook, CraftAutoPlace::unhook}, {CraftTakePlace::hook, CraftTakePlace::unhook},
    {CraftPlaceOne::hook, CraftPlaceOne::unhook}, {CraftEvacuate::hook, CraftEvacuate::unhook},
    {CraftServerAction::hook, CraftServerAction::unhook}, {CraftTick::hook, CraftTick::unhook},
    {CraftResponse::hook, CraftResponse::unhook}
};
}
void start() {
    try {
        for (auto& hook : hooks) if (!hook.installed) {
            if (hook.install(true) == 0) hook.installed = true;
            else throw std::runtime_error("Could not install craft diagnostics");
        }
        if (!renderListener)
            renderListener = ll::event::EventBus::getInstance().emplaceListener<ll::event::AfterUIRenderEvent>([](auto& event) {
                try { seeScreen(event); } catch (...) {}
            });
        Runtime::instance().self().getLogger().warn("L-359 read-only craft diagnostics enabled");
    } catch (...) { stop(); throw; }
}
void stop() {
    if (renderListener) { ll::event::EventBus::getInstance().removeListener(renderListener); renderListener.reset(); }
    for (auto it = std::rbegin(hooks); it != std::rend(hooks); ++it)
        if (it->installed && it->remove(true)) it->installed = false;
    for (auto const& hook : hooks) if (hook.installed) {
        Runtime::instance().self().getLogger().error("Could not remove craft diagnostics");
        break;
    }
    observedAt = 0;
}
}
#else
namespace lamium::researchTrace::craft { void start() {} void stop() {} }
#endif
