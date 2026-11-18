#include "features/recipes/RecipePlace.h"
#include "app/Runtime.h"
#include "features/inspection/hover/HoverTracker.h"
#include "features/inventory/game/TransferRoutes.h"
#include "features/recipes/MassCraftPlan.h"
#include "features/recipes/RecipeState.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/client/ClientExitLevelEvent.h"
#include "ll/api/event/render/UIRenderEvent.h"
#include "mc/client/gui/screens/ScreenController.h"
#include "mc/client/gui/screens/ScreenView.h"
#include "mc/client/gui/screens/controllers/ContainerScreenController.h"
#include "mc/client/gui/screens/controllers/CraftingScreenController.h"
#include "mc/deps/shared_types/legacy/ContainerType.h"
#include "mc/world/containers/SlotData.h"
#include "mc/world/containers/managers/controllers/ContainerManagerController.h"
#include "mc/world/containers/managers/controllers/CraftingContainerManagerController.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/item/crafting/Recipe.h"
#include <atomic>
#include <chrono>
#include <climits>
#include <string>

namespace lamium::recipes {
namespace {
std::string const grid = "crafting_input_items", output = "crafting_output_items", pointer = "cursor_items", hotbar = "hotbar_items",
    inventory = "inventory_items";
// A recipe waiting to be laid in: what lay in the grid is on its way back to the inventory.
struct Waiting {
    std::shared_ptr<service::Table const> table;
    int recipe = -1;
    std::map<int, Item> picks;
    std::string screen;
    bool many = false;
    int tries = 0; // frames waited for the grid to read empty
} going;
constexpr int mostTries = 20;
// The crafting screens: the inventory's 2x2 and the crafting table's 3x3 (seen with craft_trace, 2026-11-18).
CraftingScreenController* crafting(ScreenView& screen) {
    if (!screen.mController || !screen.mController->_isContainerScreen()) return nullptr;
    std::string const name = screen.getScreenName();
    if (name != "inventory_screen" && name != "crafting_screen") return nullptr;
    auto& controller = static_cast<CraftingScreenController&>(*screen.mController);
    if (!inventory::game::transferRoutes::supported(controller)) return nullptr;
    auto manager = controller.mContainerManagerController;
    if (!manager->hasContainerController(grid) || !manager->hasContainerController(pointer)) return nullptr;
    return &controller;
}
int sideOf(ContainerManagerController& manager) {
    int const size = manager.getContainerSize(grid);
    return size == 4 ? 2 : size == 9 ? 3 : 0;
}
int held(ContainerManagerController& manager) {
    auto const& stack = manager.getItemStack(pointer, 0);
    return stack.isNull() ? 0 : static_cast<int>(stack.mCount);
}
bool gridEmpty(ContainerManagerController& manager) {
    for (int i = 0, size = manager.getContainerSize(grid); i < size; ++i)
        if (!manager.getItemStack(grid, i).isNull()) return false;
    return true;
}
std::vector<PlaceSource> carriedOf(ContainerManagerController& manager, Index const& index) {
    std::vector<PlaceSource> out;
    auto add = [&](std::string const& collection, int size, int first) {
        if (!manager.hasContainerController(collection) || manager.getContainerSize(collection) < size) return;
        for (int i = 0; i < size; ++i) {
            auto const& stack = manager.getItemStack(collection, i);
            if (stack.isNull() || stack.mCount <= 0) continue;
            Item const item = index.find(stack.getTypeName(), stack.getAuxValue());
            if (item >= 0) out.push_back({first + i, item, static_cast<int>(stack.mCount), std::max(1, static_cast<int>(stack.getMaxStackSize()))});
        }
    };
    add(hotbar, 9, 0);
    add(inventory, 27, 9);
    return out;
}
// The recipe laid into an empty grid.
bool lay(CraftingScreenController& controller, Index const& index, Recipe const& recipe, std::map<int, Item> const& picks, bool many) {
    auto& manager = *controller.mContainerManagerController;
    if (held(manager) || !gridEmpty(manager)) return false;
    auto const plan = placePlan(index, recipe, sideOf(manager), carriedOf(manager, index), picks, many);
    if (!plan.ready()) return false;
    for (auto const& move : plan.moves) {
        bool const top = move.slot < 9;
        controller._handleTakeAmount(move.amount, top ? hotbar : inventory, top ? move.slot : move.slot - 9);
        // Not as planned: what the pointer holds, if anything, is left to the player.
        if (held(manager) != move.amount) return false;
        controller._handlePlaceAll(grid, move.cell);
        if (held(manager)) return false;
    }
    return true;
}
}
namespace place {
int gridSide(ScreenView& screen) {
    try {
        auto* controller = crafting(screen);
        return controller ? sideOf(*controller->mContainerManagerController) : 0;
    } catch (...) { return 0; }
}
std::vector<PlaceSource> carried(ScreenView& screen, Index const& index) {
    try {
        auto* controller = crafting(screen);
        if (!controller) return {};
        auto& manager = *controller->mContainerManagerController;
        auto out = carriedOf(manager, index);
        // What lies in the grid goes back to the inventory before a recipe is laid in, so it counts.
        for (int i = 0, size = manager.getContainerSize(grid); i < size; ++i) {
            auto const& stack = manager.getItemStack(grid, i);
            if (stack.isNull() || stack.mCount <= 0) continue;
            if (Item const item = index.find(stack.getTypeName(), stack.getAuxValue()); item >= 0)
                out.push_back({100 + i, item, static_cast<int>(stack.mCount), std::max(1, static_cast<int>(stack.getMaxStackSize()))});
        }
        return out;
    } catch (...) {}
    return {};
}
bool run(ScreenView& screen, std::shared_ptr<service::Table const> const& table, int recipe, std::map<int, Item> const& picks, bool many) {
    going = {};
    try {
        auto* controller = crafting(screen);
        if (!controller || !table || recipe < 0 || recipe >= static_cast<int>(table->index.recipes.size())) return false;
        auto& manager = *controller->mContainerManagerController;
        if (held(manager)) return false;
        if (!gridEmpty(manager)) {
            controller->_evacuateCraftingGrid();
            // Not read as empty at once: the recipe is laid in when it is, a few frames on.
            if (!gridEmpty(manager)) {
                going = {table, recipe, picks, screen.getScreenName(), many, 0};
                return true;
            }
        }
        return lay(*controller, table->index, table->index.recipes[static_cast<size_t>(recipe)], picks, many);
    } catch (...) {
        try { Runtime::instance().self().getLogger().warn("Recipes: placing a recipe failed"); } catch (...) {}
        return false;
    }
}
void tick(ScreenView& screen) {
    if (going.recipe < 0) return;
    try {
        auto* controller = crafting(screen);
        if (!controller || screen.getScreenName() != going.screen) { going = {}; return; }
        // Waiting for what lay in the grid to be back in the inventory (no room for it: given up).
        if (!gridEmpty(*controller->mContainerManagerController)) { if (++going.tries > mostTries) going = {}; return; }
        lay(*controller, going.table->index, going.table->index.recipes[static_cast<size_t>(going.recipe)], going.picks, going.many);
    } catch (...) {}
    going = {};
}
void stop() { going = {}; }
}
namespace massCraft {
namespace {
using Clock = std::chrono::steady_clock;
// Between two steps of a run, so a fast frame rate does not send requests faster; and how long a
// round that cannot be laid in waits for more to arrive, or for what it threw out to be gone.
// `patience`: how long a take's result or a product may take to show before the run gives up.
constexpr auto step = std::chrono::milliseconds(50), idle = std::chrono::milliseconds(250), afterSurplus = std::chrono::milliseconds(1000),
               patience = std::chrono::milliseconds(500);
constexpr int slots = 36;
enum class Phase { Lay, Take, Throw };
struct Run {
    bool on = false, over = false; // over: this press of the key does nothing more
    std::shared_ptr<service::Table const> table;
    int recipe = -1;
    std::map<int, Item> picks;
    Phase phase = Phase::Take;
    std::vector<int> before; // per slot, what it held of the product before the take
    Clock::time_point next, began, since; // since: when the current step began
    int rounds = 0, takes = 0, thrown = 0, surplus = 0;
} run;
std::atomic<bool> keyHeld{false};
ll::event::ListenerPtr renderListener, exitListener;
void finish(char const* why) {
    if (run.on) {
        auto const ms = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - run.began).count();
        try {
            Runtime::instance().self().getLogger().info("Mass Craft: {} rounds laid, {} takes, {} stacks thrown, {} surplus stacks in {} ms ({})",
                                                       run.rounds, run.takes, run.thrown, run.surplus, ms, why);
        } catch (...) {}
    }
    run = {};
    run.over = keyHeld;
}
// As Item transfer reads the slot under the pointer: the inventory screen's own record of it is not kept
// up, so the hover tracker tells there.
bool overOutput(CraftingScreenController& controller) {
    auto const& native = controller.mHoveredSlotData.get();
    if (controller.mContainerManagerController->getContainerType() != SharedTypes::Legacy::ContainerType::Inventory && !native.mCollectionName->empty())
        return native.mCollectionName.get() == output;
    auto const& current = inspection::hover::HoverTracker::getInstance().current();
    return current && current->controller == &controller && current->collectionName == output;
}
std::string const& collectionOf(int slot) { return slot < 9 ? hotbar : inventory; }
int indexOf(int slot) { return slot < 9 ? slot : slot - 9; }
// Hotbar and inventory, slot by slot: the item (-1: empty or not in the table) and how many.
std::vector<std::pair<Item, int>> slotsOf(ContainerManagerController& manager, Index const& index, bool& anyFree) {
    std::vector<std::pair<Item, int>> out(slots, {-1, 0});
    anyFree = false;
    if (manager.getContainerSize(hotbar) < 9 || manager.getContainerSize(inventory) < 27) return out;
    for (int slot = 0; slot < slots; ++slot) {
        auto const& stack = manager.getItemStack(collectionOf(slot), indexOf(slot));
        if (stack.isNull() || stack.mCount <= 0) { anyFree = true; continue; }
        out[static_cast<size_t>(slot)] = {index.find(stack.getTypeName(), stack.getAuxValue()), static_cast<int>(stack.mCount)};
    }
    return out;
}
bool makes(ContainerManagerController& manager) { return static_cast<CraftingContainerManagerController&>(manager).mCraftingRecipe != nullptr; }
void begin(CraftingScreenController& controller) {
    auto& manager = *controller.mContainerManagerController;
    // The recipe is the one lying in the grid: nothing in it, nothing to do yet.
    if (held(manager) || !makes(manager) || service::want() != service::State::Ready) return;
    auto table = service::table();
    if (!table) return;
    int const side = sideOf(manager);
    std::vector<Item> cells;
    for (int i = 0; i < side * side; ++i) {
        auto const& stack = manager.getItemStack(grid, i);
        cells.push_back(stack.isNull() || stack.mCount <= 0 ? -1 : table->index.find(stack.getTypeName(), stack.getAuxValue()));
    }
    std::string named;
    if (::Recipe const* made = static_cast<CraftingContainerManagerController&>(manager).mCraftingRecipe) named = *made->mRecipeId;
    int const recipe = recipeInGrid(table->index, side, cells, named);
    if (recipe < 0) {
        try { Runtime::instance().self().getLogger().info("Mass Craft: the grid lays out no recipe of the table"); } catch (...) {}
        run.over = true;
        return;
    }
    Request request;
    apply(table->index, state::notes(), request);
    run = {};
    run.on = true;
    run.table = std::move(table);
    run.recipe = recipe;
    run.picks = std::move(request.picks);
    run.next = run.began = run.since = Clock::now();
    try {
        Runtime::instance().self().getLogger().info("Mass Craft: started with {} (the game names {})", run.table->index.recipes[static_cast<size_t>(recipe)].id, named);
    } catch (...) {}
}
void advance(CraftingScreenController& controller) {
    auto& manager = *controller.mContainerManagerController;
    auto const& index = run.table->index;
    auto const& recipe = index.recipes[static_cast<size_t>(run.recipe)];
    auto const now = Clock::now();
    if (now < run.next) return;
    if (held(manager)) { finish("the pointer holds an item"); return; }
    bool anyFree = false;
    switch (run.phase) {
    case Phase::Take: {
        if (!makes(manager)) {
            if (gridEmpty(manager)) { run.phase = Phase::Lay; run.since = now; return; }
            // What a recipe leaves behind (a bucket), or a product not shown yet.
            if (now - run.since > patience) finish("the grid holds something that is not the recipe");
            return;
        }
        auto const now36 = slotsOf(manager, index, anyFree);
        run.before.assign(slots, 0);
        for (int slot = 0; slot < slots; ++slot)
            if (now36[static_cast<size_t>(slot)].first == recipe.makes) run.before[static_cast<size_t>(slot)] = now36[static_cast<size_t>(slot)].second;
        // What a Shift click on the output does: all the grid makes, as far as the inventory has room.
        controller._handleAutoPlace(INT_MAX, output, 0);
        ++run.takes;
        run.phase = Phase::Throw;
        run.since = now;
        run.next = now + step;
        return;
    }
    case Phase::Throw: {
        auto const found = gains(run.before, slotsOf(manager, index, anyFree), recipe.makes);
        if (found.empty()) {
            if (now - run.since > patience) finish("the product found no room");
            return;
        }
        for (auto const& gain : found) {
            bool const sent = gain.whole ? inventory::game::transferRoutes::drop(controller, collectionOf(gain.slot), indexOf(gain.slot), false)
                                         : inventory::game::transferRoutes::dropAmount(controller, collectionOf(gain.slot), indexOf(gain.slot), gain.amount);
            if (!sent) { finish("throwing the product out failed"); return; }
            ++run.thrown;
        }
        run.phase = Phase::Take;
        run.since = now;
        run.next = now + step;
        return;
    }
    case Phase::Lay: {
        if (!gridEmpty(manager)) { run.phase = Phase::Take; run.since = now; return; }
        auto const carried = carriedOf(manager, index);
        auto const plan = placePlan(index, recipe, sideOf(manager), carried, run.picks, true);
        if (!plan.fits) { finish("the recipe does not fit the grid"); return; }
        if (plan.ready()) {
            if (!lay(controller, index, recipe, run.picks, true)) { finish("laying the recipe in failed"); return; }
            ++run.rounds;
            run.phase = Phase::Take;
            run.since = now;
            run.next = now + step;
            return;
        }
        // Not enough of an ingredient: more may arrive while the key is held. With no slot free for it,
        // what the others fill beyond their share goes out.
        run.next = now + idle;
        slotsOf(manager, index, anyFree);
        if (anyFree) return;
        for (int slot : surplusSlots(index, recipe, carried)) {
            if (!inventory::game::transferRoutes::drop(controller, collectionOf(slot), indexOf(slot), false)) { finish("throwing a surplus out failed"); return; }
            ++run.surplus;
            run.next = now + afterSurplus;
        }
        return;
    }
    }
}
void tick(ScreenView& screen) {
    if (!keyHeld) { if (run.on || run.over) finish("the key was released"); return; }
    if (run.over) return;
    auto& runtime = Runtime::instance();
    if (!runtime.enabled() || !runtime.snapshot()->inventory.massCraft) { finish("switched off"); return; }
    auto* controller = crafting(screen);
    if (!controller) { finish("not a crafting screen"); return; }
    // Only while the pointer is over the output slot; holding the key before it gets there waits.
    if (!overOutput(*controller)) { if (run.on) finish("the pointer left the output slot"); return; }
    if (!run.on) begin(*controller);
    if (run.on) advance(*controller);
}
}
void setHeld(bool held) {
    keyHeld = held;
    if (!held) finish("the key was released");
}
void start() {
    auto& bus = ll::event::EventBus::getInstance();
    renderListener = bus.emplaceListener<ll::event::AfterUIRenderEvent>([](auto& event) {
        try {
            auto& screen = event.screenView();
            // Several screens are drawn a frame (the HUD under a container): only the focused item screen is this run's.
            if (!screen.mHasFocus || !screen.mController || !screen.mController->_isContainerScreen()) return;
            tick(screen);
        } catch (...) { try { finish("an error"); } catch (...) {} }
    });
    exitListener = bus.emplaceListener<ll::event::ClientExitLevelEvent>([](auto&) { run = {}; });
}
void stop() {
    for (auto* listener : {&renderListener, &exitListener})
        if (*listener) { ll::event::EventBus::getInstance().removeListener(*listener); listener->reset(); }
    run = {};
}
}
}
