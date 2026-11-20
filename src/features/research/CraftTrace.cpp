#include "features/research/CraftTrace.h"
#ifdef LAMIUM_CRAFT_TRACE
#include "app/Runtime.h"
#include "app/TraceLog.h"
#include "features/inventory/game/TransferRoutes.h"
#include "features/recipes/ItemKey.h"
#include "features/recipes/ItemList.h"
#include "features/recipes/RecipeService.h"
#include "features/recipes/RecipeState.h"
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
#include "mc/client/player/LocalPlayer.h"
#include "mc/deps/shared_types/legacy/ContainerType.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/block/Block.h"
#include <Windows.h>
#include <algorithm>
#include <atomic>
#include <climits>
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
TraceBudget callBudget, stateBudget, requestBudget, responseBudget, refusedBudget, screenBudget, pressBudget, experimentBudget;
std::atomic<int64_t> observedAt{0};
std::atomic<unsigned> sequence{0};
std::mutex seenMutex;
std::string lastState, lastHover, hoverCollection;
int hoverIndex = -1;
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
            hoverCollection = collection, hoverIndex = index;
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
// The experiment (not read-only): Lamium lays a recipe into the grid and takes the output, with the slot
// calls Item transfer uses. F7 over an item of Lamium's lists lays its recipe in, one of each ingredient
// (with Shift: as many as divide evenly); F8 takes the output as a Shift click does; F9 keeps taking it
// while there is one; F6 and F10 drop the output from its slot. One step a tick, each logged with what the pointer holds after it.
struct Step { int amount; std::string from; int index; int cell; };
std::vector<Step> steps;
size_t stepAt = 0;
int wait = 0, takes = 0, idle = 0;
bool f7Down = false, f8Down = false, f9Down = false, f6Down = false, f10Down = false;
bool edge(int key, bool& was) {
    bool const down = (GetAsyncKeyState(key) & 0x8000) != 0, pressed = down && !was;
    was = down;
    return pressed;
}
int cursorCount(ContainerManagerController& manager) {
    auto const& held = manager.getItemStack("cursor_items", 0);
    return held.isNull() ? 0 : int(held.mCount);
}
void layOut(CraftingScreenController& screen, ContainerManagerController& manager, bool many) {
    using namespace lamium::recipes;
    steps.clear();
    stepAt = 0;
    std::string const key = itemList::hoveredItem();
    auto const table = service::table();
    if (key.empty() || !table) { log(callBudget, 400, "lay: nothing under the pointer in Lamium's lists, or no table"); return; }
    auto const& index = table->index;
    auto const parts = splitItemKey(key);
    Item const id = index.find(parts.name, parts.aux);
    auto const choice = state::choice(table);
    int const number = id >= 0 && choice ? choice->recipe[static_cast<size_t>(id)] : -1;
    if (number < 0) { log(callBudget, 400, "lay: {} has no recipe in use", key); return; }
    auto const& recipe = index.recipes[static_cast<size_t>(number)];
    int const grid = manager.getContainerSize("crafting_input_items"), side = grid == 4 ? 2 : grid == 9 ? 3 : 0;
    int const across = recipe.width ? recipe.width : side;
    int const count = static_cast<int>(recipe.cells.size());
    if (recipe.station != Station::Crafting || !side || across < 1 || across > side || (count + across - 1) / across > side) {
        log(callBudget, 400, "lay: {} ({}) does not fit this grid of {}", key, recipe.id, grid);
        return;
    }
    if (cursorCount(manager)) { log(callBudget, 400, "lay: the pointer holds something"); return; }
    for (int i = 0; i < grid; ++i)
        if (!manager.getItemStack("crafting_input_items", i).isNull()) {
            log(callBudget, 400, "lay: the grid is not empty, emptied first; press again");
            screen._evacuateCraftingGrid();
            return;
        }
    // What is carried, slot by slot, and which slot serves each cell: the first that still has one.
    struct Source { std::string from; int index; Item item; int left, uses; };
    std::vector<Source> sources;
    for (auto const& [from, size] : {std::pair<std::string, int>{"hotbar_items", 9}, {"inventory_items", 27}})
        for (int i = 0; i < size; ++i) {
            auto const& stack = manager.getItemStack(from, i);
            if (stack.isNull()) continue;
            Item const item = index.find(stack.getTypeName(), stack.getAuxValue());
            if (item >= 0) sources.push_back({from, i, item, int(stack.mCount), 0});
        }
    std::vector<std::pair<int, size_t>> served; // grid cell, source
    for (int i = 0; i < count; ++i) {
        auto const& part = recipe.cells[static_cast<size_t>(i)];
        if (part.id < 0) continue;
        auto fits = [&](Item item) {
            if (!part.group) return item == part.id;
            auto const& members = index.groups[static_cast<size_t>(part.id)].members;
            return std::find(members.begin(), members.end(), item) != members.end();
        };
        size_t found = sources.size();
        for (size_t s = 0; s < sources.size(); ++s) if (fits(sources[s].item) && sources[s].left - sources[s].uses > 0) { found = s; break; }
        if (found == sources.size()) { log(callBudget, 400, "lay: {} lacks an ingredient for cell {}", recipe.id, i); return; }
        ++sources[found].uses;
        served.emplace_back(i / across * side + i % across, found);
    }
    int each = 1;
    if (many) {
        each = 64;
        for (auto const& source : sources) if (source.uses) each = std::min(each, source.left / source.uses);
    }
    for (auto const& [cell, source] : served) steps.push_back({each, sources[source].from, sources[source].index, cell});
    log(callBudget, 400, "lay: {} ({}) into {} cells, {} each", key, recipe.id, steps.size(), each);
}
void experiment(CraftingScreenController& screen, ContainerManagerController& manager) {
    bool const shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
    if (edge(VK_F7, f7Down)) layOut(screen, manager, shift);
    bool const once = edge(VK_F8, f8Down);
    // Crafting without laying in (L-165): the game's own request that crafts many straight from the
    // inventory comes from a Shift click on a recipe of its book, through calls the SDK does not link.
    // F6: the screen's virtual "slot pressed" on the recipe-book cell hovered last. F10: the slot call
    // that takes all, on that cell. Both with and without Shift held, which the game may read itself.
    bool const press = edge(VK_F6, f6Down), take = edge(VK_F10, f10Down);
    if (press || take) {
        std::string collection;
        int index = -1;
        {
            std::lock_guard lock{seenMutex};
            collection = hoverCollection, index = hoverIndex;
        }
        if (!collection.starts_with("recipe_") || index < 0)
            log(experimentBudget, 400, "recipe cell: none hovered last ({}:{})", collection, index);
        else {
            log(experimentBudget, 400, "recipe cell {}:{} {} shift={} before:{}", collection, index, press ? "slot-pressed" : "take-all", shift, slotsText(manager));
            if (press) screen._onContainerSlotPressed(collection, index);
            else screen._handleAutoPlace(INT_MAX, collection, index);
            log(experimentBudget, 400, "recipe cell return after:{}", slotsText(manager));
        }
    }
    if (edge(VK_F9, f9Down)) takes = takes ? 0 : 40, idle = 0;
    if (wait > 0) { --wait; return; }
    if (stepAt < steps.size()) {
        auto const& step = steps[stepAt++];
        screen._handleTakeAmount(step.amount, step.from, step.index);
        int const taken = cursorCount(manager);
        if (taken) screen._handlePlaceAll("crafting_input_items", step.cell);
        log(callBudget, 400, "lay step {}/{}: {} from {}:{} to cell {}: took {}, the pointer holds {} after", stepAt, steps.size(), step.amount,
            step.from, step.index, step.cell, taken, cursorCount(manager));
        if (!taken || cursorCount(manager)) steps.clear(), stepAt = 0; // not as planned: stop, leave the rest to the player
        wait = 1;
        return;
    }
    // The output slot reads empty even while the game shows a product (seen 2026-11-18): the recipe the
    // grid makes says whether there is one.
    bool const output = static_cast<CraftingContainerManagerController&>(manager).mCraftingRecipe != nullptr;
    if (once || (takes > 0 && output)) {
        if (!output) { log(callBudget, 400, "take: no output"); return; }
        if (takes > 0) --takes;
        idle = 0;
        log(callBudget, 400, "take: Shift take of the output{}", once ? "" : std::format(", {} left of the run", takes));
        screen._handleAutoPlace(INT_MAX, "crafting_output_items", 0);
        wait = 4;
    } else if (takes > 0 && ++idle > 40) {
        log(callBudget, 400, "take: the run ended, no output for 40 ticks");
        takes = 0;
    }
}
LL_TYPE_INSTANCE_HOOK(CraftTick, ll::memory::HookPriority::Normal, CraftingScreenController,
    &CraftingScreenController::$tick, ::ui::DirtyFlag) {
    auto result = origin();
    try {
        if (auto manager = mContainerManagerController.get(); manager && !manager->mContainersClosed) experiment(*this, *manager);
    } catch (...) { log(callBudget, 400, "experiment threw"); }
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
            // Refusals have a budget of their own: a long run must not use it up with what went well.
            log(response.mResult == ItemStackNetResult::Success ? responseBudget : refusedBudget, 400, "response id={} result={} containers={} slots={}",
                response.mClientRequestId->mRawId, int(response.mResult), response.mContainers->size(), slots);
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
// Counted places (L-359 step 8): how an open container can be told apart and read. One line when a
// container screen opens (its position and entity as the screen holds them, the block there, the
// container's type and size), one when its items first show, one when it is gone.
void const* probed = nullptr;
int probeFrames = 0, probeFilled = -1;
TraceBudget placeBudget;
void seePlace(ScreenView& view) {
    auto& controller = static_cast<ContainerScreenController&>(*view.mController);
    auto manager = controller.mContainerManagerController;
    if (!manager || manager->mContainersClosed) return;
    std::string const items = "container_items";
    bool const has = manager->hasContainerController(items);
    int const size = has ? manager->getContainerSize(items) : -1;
    int filled = 0;
    for (int i = 0; i < size && i < 200; ++i)
        if (!manager->getItemStack(items, i).isNull()) ++filled;
    if (probed != &controller) {
        probed = &controller;
        probeFrames = 0;
        probeFilled = filled;
        BlockPos const pos = controller.mBlockPos;
        std::string block = "?";
        int dimension = -99;
        if (auto client = ll::service::getClientInstance())
            if (auto* player = client->getLocalPlayer()) {
                dimension = static_cast<int>(player->getDimensionId());
                try { block = player->getDimensionBlockSource().getBlock(pos).getTypeName(); } catch (...) { block = "unreadable"; }
            }
        log(placeBudget, 200, "place opened: screen '{}' type={} dimension={} pos={},{},{} block={} entity={} container_items={} filled={}",
            view.getScreenName(), static_cast<int>(manager->getContainerType()), dimension, pos.x, pos.y, pos.z, block,
            controller.mEntityUniqueID->rawID, size, filled);
        return;
    }
    ++probeFrames;
    if (filled != probeFilled) {
        log(placeBudget, 200, "place items: {} filled after {} frames (was {})", filled, probeFrames, probeFilled);
        probeFilled = filled;
    }
}
void seeStonecutter(ContainerScreenController& screen);
void seeScreen(ll::event::UIRenderEvent& event) {
    auto& view = event.screenView();
    if (!view.mHasFocus || !view.mController || !view.mController->_isContainerScreen()) return;
    try { seePlace(view); } catch (...) {}
    try { seeStonecutter(static_cast<ContainerScreenController&>(*view.mController)); } catch (...) {}
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
// L-380: how the game chooses the stonecutter's result. On a stonecutter screen only: each hover the screen handles
// and each take it makes ( take-place and auto-place log as `recent`), with
// the collection and index and what lies there; and, every frame, the stonecutter's collections whenever they change
// (the result shows once one is chosen). Read-only.
TraceBudget stoneBudget;
bool stonecutter(ContainerScreenController& screen) {
    auto manager = screen.mContainerManagerController.get();
    return manager && manager->getContainerType() == SharedTypes::Legacy::ContainerType::Stonecutter;
}
std::string slotItem(ContainerScreenController& screen, std::string const& collection, int index) {
    try {
        auto manager = screen.mContainerManagerController.get();
        if (!manager || !manager->hasContainerController(collection)) return "-";
        auto const& stack = manager->getItemStack(collection, index);
        return stack.isNull() ? std::string("-") : std::format("{}x{}", stack.getTypeName(), static_cast<int>(stack.mCount));
    } catch (...) { return "?"; }
}
std::string lastStoneHover;
LL_TYPE_INSTANCE_HOOK(StoneHovered, ll::memory::HookPriority::Normal, ContainerScreenController,
    &ContainerScreenController::$_onContainerSlotHovered, ::ui::ViewRequest, std::string const& collection, int index) {
    if (stonecutter(*this)) {
        observedAt = nowMs();
        std::string cell = std::format("{}:{}", collection, index);
        if (cell != lastStoneHover) log(stoneBudget, 600, "stone hovered {} item={}", cell, slotItem(*this, collection, index)), lastStoneHover = std::move(cell);
    }
    return origin(collection, index);
}
LL_TYPE_INSTANCE_HOOK(StoneTakeAmount, ll::memory::HookPriority::Normal, ContainerScreenController,
    &ContainerScreenController::_handleTakeAmount, void, int amount, std::string const& collection, int index) {
    if (stonecutter(*this)) log(stoneBudget, 600, "stone take-amount {} {}:{} item={}", amount, collection, index, slotItem(*this, collection, index));
    origin(amount, collection, index);
}
// Every frame on a stonecutter screen: its collections and what lies in them, logged when that changes.
std::string lastStoneState;
void seeStonecutter(ContainerScreenController& screen) {
    if (!stonecutter(screen)) return;
    auto manager = screen.mContainerManagerController.get();
    if (!manager || manager->mContainersClosed) return;
    std::string state;
    unsigned seen = 0;
    for (auto const& [collection, container] : manager->mContainers.get()) {
        if (++seen > 40) break;
        if (collection.find("inventory") != std::string::npos || collection.find("hotbar") != std::string::npos) continue;
        state += " " + collection + "[";
        for (int i = 0, size = manager->getContainerSize(collection); i < size && i < 64; ++i)
            if (auto const& stack = manager->getItemStack(collection, i); !stack.isNull())
                state += std::format("{}:{}x{} ", i, stack.getTypeName(), static_cast<int>(stack.mCount));
        state += "]";
    }
    if (state != lastStoneState) log(stoneBudget, 600, "stone state{}", state), lastStoneState = std::move(state);
}
struct Hook { int (*install)(bool); bool (*remove)(bool); bool installed = false; };
Hook hooks[] = {
    {CraftSlotHovered::hook, CraftSlotHovered::unhook},
    {CraftAutoPlace::hook, CraftAutoPlace::unhook}, {CraftTakePlace::hook, CraftTakePlace::unhook},
    {CraftPlaceOne::hook, CraftPlaceOne::unhook}, {CraftEvacuate::hook, CraftEvacuate::unhook},
    {CraftServerAction::hook, CraftServerAction::unhook}, {CraftTick::hook, CraftTick::unhook},
    {CraftResponse::hook, CraftResponse::unhook},
    {StoneHovered::hook, StoneHovered::unhook}, {StoneTakeAmount::hook, StoneTakeAmount::unhook}
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
